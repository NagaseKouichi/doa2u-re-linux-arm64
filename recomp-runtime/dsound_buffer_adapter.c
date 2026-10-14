/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dsound_buffer_adapter.h"
#include "dsound_api_adapter.h"
#include "dsound_buffer_model.h"
#include "audio_output.h"
#include "xbox_adpcm.h"
#include "xapi_time_adapter.h"
#include "kernel_abi.h"
#include "stop_report.h"

#include <limits.h>
#include <string.h>

enum { BUFFER_COUNT = 256, ROOT_SIZE = 0x24, SETTINGS_SIZE = 0xe0, PUBLIC_OFFSET = 0x1c };
typedef struct Buffer {
    uint32_t root, settings, references, flags;
    uint32_t data, size, owned, owned_size, format, channels, bits, align, rate, frequency;
    uint32_t headroom;
    int32_t volume;
    const uint8_t *pcm;
    RecompDsoundBufferModel model;
} Buffer;
static Buffer buffers[BUFFER_COUNT];

static uint32_t canonical(uint32_t address)
{
    uint32_t window = address & ~(RECOMP_XBOX_RAM_SIZE - 1u);
    return window == 0u || window == RECOMP_XBOX_CACHED_ALIAS ||
        window == RECOMP_XBOX_PHYSICAL_ALIAS ? address & (RECOMP_XBOX_RAM_SIZE - 1u) : UINT32_MAX;
}

static uint8_t *span(uint32_t address, uint32_t size)
{
    uint32_t offset = canonical(address);
    return address && offset < RECOMP_XBOX_RAM_SIZE && size <= RECOMP_XBOX_RAM_SIZE - offset
        ? recomp_memory(address, size) : NULL;
}

static uint32_t read32(uint32_t address)
{
    uint32_t value;
    memcpy(&value, recomp_memory(address, sizeof value), sizeof value);
    return value;
}

static void write32(uint32_t address, uint32_t value)
{
    memcpy(recomp_memory(address, sizeof value), &value, sizeof value);
}

static uint64_t now_ms(void)
{
#ifdef RECOMP_DSOUND_TEST_CLOCK
    uint64_t recomp_test_dsound_now_ms(void);
    return recomp_test_dsound_now_ms();
#else
    return recomp_xapi_performance_counter() / 1000000u;
#endif
}

static Buffer *buffer_at(uint32_t handle)
{
    uint32_t address = canonical(handle);
    for (uint32_t i = 0; i < BUFFER_COUNT; ++i) {
        Buffer *buffer = &buffers[i];
        if (buffer->root && address == buffer->root + PUBLIC_OFFSET &&
            read32(buffer->root + 4u) == buffer->references &&
            read32(address) == buffer->settings) return buffer;
    }
    recomp_stop(2, "dsound:buffer-handle-invalid:%08x", handle);
    return NULL;
}

static uint32_t decoded_bytes(const Buffer *buffer, uint32_t bytes)
{
    return buffer->format == 0x69u
        ? bytes / buffer->align * XBOX_ADPCM_BLOCK_SAMPLES * buffer->channels * 2u : bytes;
}

static uint32_t source_cursor(const Buffer *buffer)
{
    uint32_t bytes = buffer->model.cursor_bytes;
    if (buffer->format == 0x69u) bytes = bytes / (64u * buffer->channels * 2u) * buffer->align;
    return bytes / buffer->align * buffer->align;
}

static void pump(Buffer *buffer, uint64_t now)
{
    if (!recomp_audio_output_ready()) recomp_stop(2, "dsound:audio-output-failed");
    if (!buffer->model.playing || !buffer->pcm) return;
    uint32_t offset = 0u;
    uint32_t bytes = recomp_dsound_buffer_consume(&buffer->model, now, &offset);
    if (!bytes) return;
    int16_t samples[40000];
    uint8_t *pcm = (uint8_t *)samples;
    if (bytes > sizeof samples) recomp_stop(2, "dsound:buffer-output-size");
    uint32_t end = buffer->model.play_flags & RECOMP_DSOUND_PLAY_LOOPING
        ? buffer->model.loop_end_bytes : buffer->model.size_bytes;
    for (uint32_t written = 0u; written < bytes;) {
        uint32_t count = end - offset;
        const uint8_t *source;
        int16_t decoded[XBOX_ADPCM_BLOCK_SAMPLES * 2u];
        if (buffer->format == 0x69u) {
            uint32_t block_bytes = 64u * buffer->channels * 2u;
            uint32_t within = offset % block_bytes;
            if (count > block_bytes - within) count = block_bytes - within;
            if (!xbox_adpcm_decode_block(buffer->pcm + offset / block_bytes * buffer->align,
                    buffer->align, buffer->channels, decoded, 128u)) {
                recomp_stop(2, "dsound:buffer-adpcm-invalid");
            }
            source = (const uint8_t *)decoded + within;
        } else source = buffer->pcm + offset;
        if (count > bytes - written) count = bytes - written;
        memcpy(pcm + written, source, count);
        written += count;
        offset += count;
        if (offset == end) offset = buffer->model.loop_start_bytes;
    }
    if (!recomp_audio_output_ready()) recomp_stop(2, "dsound:audio-output-failed");
    recomp_audio_output_submit((uint32_t)(buffer - buffers), pcm, bytes,
        buffer->model.sample_rate, buffer->channels, buffer->format == 0x69u ? 16u : buffer->bits,
        buffer->volume);
    if (!recomp_audio_output_ready()) recomp_stop(2, "dsound:audio-output-failed");
}

void recomp_dsound_buffers_pump(void)
{
    /* shortcut: API/DoWork pumping drops spans after >100 ms stalls; add an
       independent producer when continuous playback across stalls is required. */
    uint64_t now = now_ms();
    for (uint32_t i = 0; i < BUFFER_COUNT; ++i) if (buffers[i].root) pump(&buffers[i], now);
}

static void dispose(Buffer *buffer)
{
    recomp_audio_output_reset_voice((uint32_t)(buffer - buffers));
    if (buffer->owned) recomp_kernel_free_pool(buffer->owned);
    if (buffer->settings) recomp_kernel_free_pool(buffer->settings);
    if (buffer->root) recomp_kernel_free_pool(buffer->root);
    *buffer = (Buffer){0};
}

void recomp_dsound_buffers_reset(void)
{
    for (uint32_t i = 0; i < BUFFER_COUNT; ++i) if (buffers[i].root) {
        dispose(&buffers[i]);
        recomp_dsound_device_release_owner();
    }
}

int recomp_dsound_buffer_addref_root(uint32_t root, uint32_t *references)
{
    uint32_t address = canonical(root);
    for (uint32_t i = 0; i < BUFFER_COUNT; ++i) if (buffers[i].root && buffers[i].root == address) {
        Buffer *buffer = buffer_at(address + PUBLIC_OFFSET);
        if (buffer->references == UINT32_MAX) recomp_stop(2, "dsound:buffer-refcount-overflow");
        *references = ++buffer->references;
        write32(address + 4u, *references);
        return 1;
    }
    return 0;
}

static uint32_t set_data(Buffer *buffer, uint32_t data, uint32_t size)
{
    if (size % buffer->align || (buffer->format == 0x69u &&
        (uint64_t)(size / buffer->align) * 64u * buffer->channels * 2u > UINT32_MAX)) {
        return RECOMP_DSOUND_INVALID_PARAM;
    }
    if (data && size && !span(data, size)) return RECOMP_DSOUND_POINTER_ERROR;
    if (canonical(data) == canonical(buffer->data) && size == buffer->size) return RECOMP_DSOUND_OK;
    if (buffer->owned && data && size) {
        uint32_t start = canonical(data);
        if (start < buffer->owned + buffer->owned_size && buffer->owned < (uint64_t)start + size)
            return RECOMP_DSOUND_INVALID_PARAM;
    }
    uint32_t owned = 0u;
    if (!data && size) {
        if (size > RECOMP_XBOX_RAM_SIZE - 31u) return RECOMP_DSOUND_OUT_OF_MEMORY;
        owned = recomp_kernel_allocate_pool(size + 31u);
        if (!owned) return RECOMP_DSOUND_OUT_OF_MEMORY;
        data = (owned + 31u) & ~31u;
        uint8_t *storage = span(data, size);
        if (!storage) { recomp_kernel_free_pool(owned); return RECOMP_DSOUND_OUT_OF_MEMORY; }
        memset(storage, buffer->format == 1u && buffer->bits == 8u ? 0x80 : 0, size);
    }
    pump(buffer, now_ms());
    recomp_audio_output_reset_voice((uint32_t)(buffer - buffers));
    if (buffer->owned) recomp_kernel_free_pool(buffer->owned);
    buffer->owned = owned;
    buffer->owned_size = owned ? size + 31u : 0u;
    buffer->data = size ? data : 0u;
    buffer->size = size;
    buffer->pcm = size ? span(data, size) : NULL;
    buffer->model = (RecompDsoundBufferModel){0};
    if (size) recomp_dsound_buffer_configure(&buffer->model, decoded_bytes(buffer, size),
        buffer->frequency, buffer->channels * (buffer->format == 0x69u ? 2u : buffer->bits / 8u), now_ms());
    write32(buffer->settings + 8u, buffer->flags | (owned ? 0x80000000u : 0u));
    write32(buffer->settings + 0xbcu, buffer->data);
    write32(buffer->settings + 0xc0u, size);
    return RECOMP_DSOUND_OK;
}

static void create_buffer(void)
{
    recomp_dsound_require_device(kernel_arg(1u));
    uint32_t desc_address = kernel_arg(2u), output = kernel_arg(3u);
    uint32_t result = RECOMP_DSOUND_POINTER_ERROR, desc[6];
    uint8_t *desc_span = span(desc_address, sizeof desc);
    if (!span(output, 4u)) { kernel_return(4u, result); return; }
    write32(output, 0u);
    if (!desc_span) { kernel_return(4u, result); return; }
    memcpy(desc, desc_span, sizeof desc);
    if (desc[0] != sizeof desc || kernel_arg(4u)) { kernel_return(4u, RECOMP_DSOUND_INVALID_PARAM); return; }
    if (desc[1] & 0x10u) recomp_stop(2, "dsound:buffer-3d-unbound");
    if (desc[4] || desc[5]) recomp_stop(2, "dsound:buffer-mix-bins-unbound");
    if (desc[1] & ~0x400a0u) recomp_stop(2, "dsound:buffer-create-flags-unbound:%08x", desc[1]);
    uint8_t *format = span(desc[3], 18u);
    if (!format) { kernel_return(4u, result); return; }
    uint16_t tag, channels, bits, align, extra;
    uint32_t rate, average;
    memcpy(&tag, format, 2); memcpy(&channels, format + 2, 2);
    memcpy(&rate, format + 4, 4); memcpy(&average, format + 8, 4);
    memcpy(&align, format + 12, 2); memcpy(&bits, format + 14, 2); memcpy(&extra, format + 16, 2);
    uint16_t samples = 0u;
    if (tag == 0x69u && extra == 2u && span(desc[3], 20u)) memcpy(&samples, format + 18, 2);
    result = RECOMP_DSOUND_INVALID_PARAM;
    if (channels < 1 || channels > 2 || rate < 1000 || rate > 200000 ||
        (tag != 1u && tag != 0x69u) ||
        (tag == 1u && ((bits != 8 && bits != 16) || align != channels * bits / 8 ||
            average != rate * align || extra != 0u)) ||
        (tag == 0x69u && (bits != 4 || align != 36u * channels || extra != 2u ||
            !span(desc[3], 20u) || samples != 64u))) {
        kernel_return(4u, result); return;
    }
    uint32_t slot = 0u;
    while (slot < BUFFER_COUNT && buffers[slot].root) ++slot;
    if (slot == BUFFER_COUNT) { kernel_return(4u, RECOMP_DSOUND_OUT_OF_MEMORY); return; }
    Buffer *buffer = &buffers[slot];
    *buffer = (Buffer){.root = recomp_kernel_allocate_pool(ROOT_SIZE),
        .settings = recomp_kernel_allocate_pool(SETTINGS_SIZE), .references = 1u,
        .flags = desc[1], .format = tag, .channels = channels, .bits = bits, .align = align,
        .rate = rate, .frequency = rate, .headroom = (desc[1] & 0x382010u) ? 0u : 600u};
    if (!buffer->root || !buffer->settings) { dispose(buffer); kernel_return(4u, RECOMP_DSOUND_OUT_OF_MEMORY); return; }
    buffer->volume = -(int32_t)buffer->headroom;
    memset(recomp_memory(buffer->root, ROOT_SIZE), 0, ROOT_SIZE);
    memset(recomp_memory(buffer->settings, SETTINGS_SIZE), 0, SETTINGS_SIZE);
    write32(buffer->root, 0x46c1d8u);
    write32(buffer->root + 4u, 1u);
    write32(buffer->root + 8u, recomp_dsound_current_device() - 8u);
    write32(buffer->root + 0x10u, read32(recomp_dsound_current_device()));
    write32(buffer->root + 0x14u, buffer->root + 0x14u);
    write32(buffer->root + 0x18u, buffer->root + 0x14u);
    write32(buffer->root + PUBLIC_OFFSET, buffer->settings);
    write32(buffer->settings + 8u, buffer->flags);
    write32(buffer->settings + 0x1cu, (uint32_t)buffer->volume);
    write32(buffer->settings + 0x20u, buffer->headroom);
    result = set_data(buffer, 0u, desc[2]);
    if (result != RECOMP_DSOUND_OK) dispose(buffer);
    else { recomp_dsound_device_add_owner(); write32(output, buffer->root + PUBLIC_OFFSET); }
    kernel_return(4u, result);
}

static uint32_t release_buffer(Buffer *buffer)
{
    uint32_t references = --buffer->references;
    write32(buffer->root + 4u, references);
    if (!references) { dispose(buffer); recomp_dsound_device_release_owner(); }
    return references;
}

enum Operation { RELEASE, DATA, LOCK, UNLOCK, PLAY, STOP, STOP_EX, STATUS, POSITION, SEEK, LOOP, FREQUENCY, VOLUME, HEADROOM };
static void buffer_call(enum Operation operation, uint32_t arguments)
{
    Buffer *buffer = buffer_at(kernel_arg(1u));
    if (operation != RELEASE && !recomp_audio_output_ready()) recomp_stop(2, "dsound:audio-output-failed");
    uint32_t value = arguments >= 2u ? kernel_arg(2u) : 0u;
    uint32_t result = RECOMP_DSOUND_OK;
    uint64_t now = now_ms();
    switch (operation) {
    case RELEASE: result = release_buffer(buffer); break;
    case DATA: result = set_data(buffer, value, kernel_arg(3u)); break;
    case LOCK: {
        uint32_t bytes = kernel_arg(3u), flags = kernel_arg(8u), size = buffer->size;
        uint32_t p1 = kernel_arg(4u), n1 = kernel_arg(5u), p2 = kernel_arg(6u), n2 = kernel_arg(7u);
        if (!span(p1, 4u) || !span(n1, 4u) || (p2 && !span(p2, 4u)) ||
            (n2 && !span(n2, 4u)) || (!p2 != !n2)) { result = RECOMP_DSOUND_POINTER_ERROR; break; }
        pump(buffer, now);
        if (flags & 1u) value = source_cursor(buffer);
        if (flags & 2u) bytes = size;
        if (!size || value >= size || !bytes || bytes > size || (flags & ~3u) ||
            value % buffer->align || bytes % buffer->align) { result = RECOMP_DSOUND_INVALID_PARAM; break; }
        uint32_t first = bytes < size - value ? bytes : size - value;
        if (bytes > first && !p2) { result = RECOMP_DSOUND_INVALID_PARAM; break; }
        write32(p1, buffer->data + value); write32(n1, first);
        if (p2) { write32(p2, bytes > first ? buffer->data : 0u); write32(n2, bytes - first); }
        break;
    }
    case UNLOCK: {
        uint32_t size = kernel_arg(3u), second = kernel_arg(4u), second_size = kernel_arg(5u);
        uint32_t offset = canonical(value), data = canonical(buffer->data);
        if (!buffer->size || offset < data || offset >= data + buffer->size ||
            size > data + buffer->size - offset || second_size > buffer->size - size ||
            (second_size && (offset + size != data + buffer->size || canonical(second) != data))) result = RECOMP_DSOUND_INVALID_PARAM;
        break;
    }
    case PLAY: {
        uint32_t flags = kernel_arg(4u);
        if (flags & ~3u) recomp_stop(2, "dsound:buffer-play-flags-unbound:%08x", flags);
        pump(buffer, now);
        uint32_t continuing = buffer->model.playing && !(flags & RECOMP_DSOUND_PLAY_FROMSTART);
        result = recomp_dsound_buffer_play(&buffer->model, flags, now);
        if (!result && !continuing) recomp_audio_output_reset_voice((uint32_t)(buffer - buffers));
        break;
    }
    case STOP_EX:
        if (value || kernel_arg(3u) || kernel_arg(4u)) recomp_stop(2, "dsound:buffer-stop-ex-unbound");
        /* Zero time/flags have immediate Stop semantics. */
    case STOP:
        pump(buffer, now); recomp_dsound_buffer_stop(&buffer->model, now);
        recomp_audio_output_reset_voice((uint32_t)(buffer - buffers)); break;
    case STATUS:
        if (!span(value, 4u)) { result = RECOMP_DSOUND_POINTER_ERROR; break; }
        pump(buffer, now);
        write32(value, buffer->model.playing ? 1u | ((buffer->model.play_flags & 1u) ? 4u : 0u) : 0u); break;
    case POSITION: {
        uint32_t write = kernel_arg(3u);
        if ((!value && !write) || (value && !span(value, 4u)) || (write && !span(write, 4u))) {
            result = RECOMP_DSOUND_POINTER_ERROR; break;
        }
        pump(buffer, now);
        if (value) write32(value, source_cursor(buffer));
        if (write) write32(write, source_cursor(buffer));
        break;
    }
    case SEEK:
        if (value >= buffer->size || value % buffer->align) { result = RECOMP_DSOUND_INVALID_PARAM; break; }
        pump(buffer, now);
        result = recomp_dsound_buffer_set_position(&buffer->model, decoded_bytes(buffer, value), now);
        if (!result) recomp_audio_output_reset_voice((uint32_t)(buffer - buffers)); break;
    case LOOP: {
        uint32_t length = kernel_arg(3u);
        if (value >= buffer->size || !length || length > buffer->size - value ||
            value % buffer->align || length % buffer->align) { result = RECOMP_DSOUND_INVALID_PARAM; break; }
        pump(buffer, now);
        buffer->model.loop_start_bytes = decoded_bytes(buffer, value);
        buffer->model.loop_end_bytes = decoded_bytes(buffer, value + length);
        recomp_audio_output_reset_voice((uint32_t)(buffer - buffers)); break;
    }
    case FREQUENCY:
        if (!value) value = buffer->rate;
        if (value < 1000u || value > 200000u) { result = RECOMP_DSOUND_INVALID_PARAM; break; }
        pump(buffer, now); buffer->frequency = value;
        if (buffer->size) result = recomp_dsound_buffer_set_frequency(&buffer->model, value, now); break;
    case VOLUME: case HEADROOM: {
        int32_t input = (int32_t)value;
        int64_t volume = operation == HEADROOM
            ? (int64_t)buffer->volume + buffer->headroom - value : (int64_t)input - buffer->headroom;
        /* Native arithmetic rejects overflow; retail has no headroom bound. */
        if ((operation == VOLUME && (input < -10000 || input > 0)) || volume < INT32_MIN || volume > 0) {
            result = RECOMP_DSOUND_INVALID_PARAM; break;
        }
        pump(buffer, now); buffer->volume = (int32_t)volume;
        if (operation == HEADROOM) { buffer->headroom = value; write32(buffer->settings + 0x20u, value); }
        write32(buffer->settings + 0x1cu, (uint32_t)buffer->volume); break;
    }
    }
    kernel_return(arguments, result);
}

#define METHOD(name, op, count) static void name(void) { buffer_call(op, count); }
METHOD(release, RELEASE, 1u) METHOD(data, DATA, 3u) METHOD(lock_buffer, LOCK, 8u)
METHOD(unlock, UNLOCK, 5u) METHOD(play, PLAY, 4u) METHOD(stop, STOP, 1u)
METHOD(stop_ex, STOP_EX, 4u) METHOD(status, STATUS, 2u) METHOD(position, POSITION, 3u)
METHOD(seek, SEEK, 2u) METHOD(loop, LOOP, 3u) METHOD(frequency, FREQUENCY, 2u)
METHOD(volume, VOLUME, 2u) METHOD(headroom, HEADROOM, 2u)
static void private_release(void)
{
    kernel_return(1u, release_buffer(buffer_at(kernel_arg(1u) + PUBLIC_OFFSET)));
}
static void unbound_3d(void) { recomp_stop(2, "dsound:buffer-3d-unbound"); }
static void unbound_mix_bins(void) { recomp_stop(2, "dsound:buffer-mix-bins-unbound"); }
static void unbound_mix_volumes(void) { recomp_stop(2, "dsound:buffer-mix-volumes-unbound"); }
static void unbound_stream(void) { recomp_stop(2, "dsound:stream-unbound"); }

RecompFunction recomp_dsound_buffer_lookup_manual(uint32_t address)
{
    switch (address) {
    case 0x365fbcu: return create_buffer;
    case 0x363641u: return release; case 0x363b91u: return private_release;
    case 0x365793u: return data; case 0x3648dau: return lock_buffer; case 0x36363cu: return unlock;
    case 0x364802u: return play; case 0x364826u: return stop; case 0x36483eu: return stop_ex;
    case 0x364882u: return status; case 0x36489eu: return position; case 0x3648beu: return seek;
    case 0x364862u: return loop; case 0x365121u: return frequency;
    case 0x3647aeu: return volume; case 0x3647cau: return headroom;
    case 0x3647e6u: return unbound_mix_volumes; case 0x36513du: return unbound_mix_bins;
    case 0x365159u: case 0x36517du: case 0x3651a1u: case 0x3651d6u:
    case 0x36520bu: case 0x36522fu: return unbound_3d;
    case 0x363f70u: case 0x363fb7u: case 0x364005u: case 0x364104u:
    case 0x364155u: case 0x36406cu: case 0x3640b9u: case 0x36490au:
    case 0x36490fu: case 0x364914u: case 0x365253u: case 0x366257u: return unbound_stream;
    default: return NULL;
    }
}
