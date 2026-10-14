#include "kernel_abi.h"

#include <stdint.h>
#include <string.h>

/* The Xbox kernel exports XcSHAInit/XcSHAUpdate/XcSHAFinal (ordinals 335-337)
   as plain FIPS 180-1 SHA-1. The title uses them for save-data and disc
   integrity checks, so the digest has to be the real one: a stub that returns
   a constant makes the guest reject its own data further along, which is a
   worse failure than stopping here.

   The guest owns the context buffer and only ever passes it back to these
   three entry points, so the layout below is this runtime's own. It is laid
   out to match the shape the XDK header describes - five state words, a
   64-bit bit-count, then the 64-byte block buffer - because the guest
   allocates the context from that header's size, and writing past it would
   corrupt whatever the title put next to it.

   Nothing here knows it is reached from a guest call: rule 8 keeps the model
   reachable as plain C so the vectors below can test it directly. */

enum {
    SHA1_BLOCK_BYTES = 64u,
    SHA1_STATE_WORDS = 5u,
};

/* Offsets into the guest's context buffer, in bytes.

   The XDK's A_SHA_CTX begins with 24 bytes this implementation does not use,
   and the live state follows it. The prefix is preserved rather than written:
   the guest allocated the whole structure, and the retail kernel leaves those
   bytes alone too, so anything the title stored there must survive a call.
   Corroborated against the Cxbx-Reloaded kernel crypto implementation, which
   skips the same 24 bytes before its state. */
enum {
    SHA_CONTEXT_PREFIX = 24u,                /* untouched by this model */
    SHA_CONTEXT_STATE = SHA_CONTEXT_PREFIX,  /* 5 x u32 */
    SHA_CONTEXT_COUNT = SHA_CONTEXT_STATE + 20u, /* u64, length in BITS */
    SHA_CONTEXT_BUFFER = SHA_CONTEXT_COUNT + 8u, /* 64 raw bytes */
    SHA_CONTEXT_BYTES = SHA_CONTEXT_BUFFER + 64u, /* 116 total */
};

static uint32_t rotate_left(uint32_t value, unsigned bits)
{
    return (value << bits) | (value >> (32u - bits));
}
/* One SHA-1 compression over a 64-byte big-endian block. */
static void sha1_compress(uint32_t *state, const uint8_t *block)
{
    uint32_t w[80];
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (unsigned i = 0u; i < 16u; ++i) {
        w[i] = ((uint32_t)block[4u * i] << 24u) |
               ((uint32_t)block[4u * i + 1u] << 16u) |
               ((uint32_t)block[4u * i + 2u] << 8u) |
               (uint32_t)block[4u * i + 3u];
    }
    for (unsigned i = 16u; i < 80u; ++i) {
        w[i] = rotate_left(w[i - 3u] ^ w[i - 8u] ^ w[i - 14u] ^ w[i - 16u], 1u);
    }

    for (unsigned i = 0u; i < 80u; ++i) {
        uint32_t f;
        uint32_t k;

        if (i < 20u) {
            f = (b & c) | (~b & d);
            k = 0x5a827999u;
        } else if (i < 40u) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1u;
        } else if (i < 60u) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdcu;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6u;
        }

        uint32_t temp = rotate_left(a, 5u) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotate_left(b, 30u);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void recomp_kernel_sha_init(RecompShaContext *context)
{
    if (context == NULL) {
        return;
    }
    context->state[0] = 0x67452301u;
    context->state[1] = 0xefcdab89u;
    context->state[2] = 0x98badcfeu;
    context->state[3] = 0x10325476u;
    context->state[4] = 0xc3d2e1f0u;
    context->bit_count = 0u;
    memset(context->buffer, 0, sizeof context->buffer);
}

void recomp_kernel_sha_update(
    RecompShaContext *context, const uint8_t *input, uint32_t length)
{
    if (context == NULL || (input == NULL && length != 0u)) {
        return;
    }

    uint32_t buffered = (uint32_t)((context->bit_count / 8u) %
                                   SHA1_BLOCK_BYTES);
    context->bit_count += (uint64_t)length * 8u;

    uint32_t consumed = 0u;
    if (buffered != 0u) {
        uint32_t space = SHA1_BLOCK_BYTES - buffered;
        uint32_t take = length < space ? length : space;

        memcpy(context->buffer + buffered, input, take);
        consumed = take;
        if (take < space) {
            return;
        }
        sha1_compress(context->state, context->buffer);
    }

    while (length - consumed >= SHA1_BLOCK_BYTES) {
        sha1_compress(context->state, input + consumed);
        consumed += SHA1_BLOCK_BYTES;
    }
    if (consumed < length) {
        memcpy(context->buffer, input + consumed, length - consumed);
    }
}

void recomp_kernel_sha_final(RecompShaContext *context, uint8_t *digest)
{
    if (context == NULL || digest == NULL) {
        return;
    }

    uint64_t bit_count = context->bit_count;
    uint32_t buffered = (uint32_t)((bit_count / 8u) % SHA1_BLOCK_BYTES);

    /* Append 0x80, pad with zeros, then the 64-bit big-endian bit length. */
    context->buffer[buffered] = 0x80u;
    ++buffered;
    if (buffered > SHA1_BLOCK_BYTES - 8u) {
        memset(context->buffer + buffered, 0, SHA1_BLOCK_BYTES - buffered);
        sha1_compress(context->state, context->buffer);
        buffered = 0u;
    }
    memset(context->buffer + buffered, 0, SHA1_BLOCK_BYTES - 8u - buffered);
    for (unsigned i = 0u; i < 8u; ++i) {
        context->buffer[SHA1_BLOCK_BYTES - 1u - i] =
            (uint8_t)(bit_count >> (8u * i));
    }
    sha1_compress(context->state, context->buffer);

    for (unsigned i = 0u; i < SHA1_STATE_WORDS; ++i) {
        digest[4u * i] = (uint8_t)(context->state[i] >> 24u);
        digest[4u * i + 1u] = (uint8_t)(context->state[i] >> 16u);
        digest[4u * i + 2u] = (uint8_t)(context->state[i] >> 8u);
        digest[4u * i + 3u] = (uint8_t)context->state[i];
    }
}

void recomp_kernel_hmac(
    const uint8_t *key, uint32_t key_length,
    const uint8_t *first, uint32_t first_length,
    const uint8_t *second, uint32_t second_length, uint8_t *digest)
{
    RecompShaContext context;
    uint8_t pad[SHA1_BLOCK_BYTES] = {0};
    uint8_t inner[RECOMP_SHA_DIGEST_BYTES];

    if (digest == NULL || (key == NULL && key_length != 0u) ||
        (first == NULL && first_length != 0u) ||
        (second == NULL && second_length != 0u)) {
        return;
    }
    /* Xbox truncates oversized keys instead of hashing them first. */
    if (key_length > SHA1_BLOCK_BYTES) key_length = SHA1_BLOCK_BYTES;
    if (key_length != 0u) memcpy(pad, key, key_length);
    for (unsigned i = 0u; i < sizeof pad; ++i) pad[i] ^= 0x36u;
    recomp_kernel_sha_init(&context);
    recomp_kernel_sha_update(&context, pad, sizeof pad);
    if (first_length != 0u) recomp_kernel_sha_update(&context, first, first_length);
    if (second_length != 0u) recomp_kernel_sha_update(&context, second, second_length);
    recomp_kernel_sha_final(&context, inner);

    for (unsigned i = 0u; i < sizeof pad; ++i) pad[i] ^= 0x36u ^ 0x5cu;
    recomp_kernel_sha_init(&context);
    recomp_kernel_sha_update(&context, pad, sizeof pad);
    recomp_kernel_sha_update(&context, inner, sizeof inner);
    recomp_kernel_sha_final(&context, digest);
}

void recomp_kernel_rc4_key(
    RecompRc4Context *context, const uint8_t *key, uint32_t key_length)
{
    if (context == NULL || key == NULL || key_length == 0u) return;
    for (unsigned i = 0u; i < 256u; ++i) context->state[i] = (uint8_t)i;
    uint8_t j = 0u;
    for (unsigned i = 0u; i < 256u; ++i) {
        j = (uint8_t)(j + context->state[i] + key[i % key_length]);
        uint8_t value = context->state[i];
        context->state[i] = context->state[j];
        context->state[j] = value;
    }
    context->i = context->j = 0u;
}

void recomp_kernel_rc4_crypt(RecompRc4Context *context, uint8_t *input, uint32_t length)
{
    if (context == NULL || (input == NULL && length != 0u)) return;
    for (uint32_t n = 0u; n < length; ++n) {
        context->i = (uint8_t)(context->i + 1u);
        context->j = (uint8_t)(context->j + context->state[context->i]);
        uint8_t value = context->state[context->i];
        context->state[context->i] = context->state[context->j];
        context->state[context->j] = value;
        uint8_t index = (uint8_t)(context->state[context->i] + context->state[context->j]);
        input[n] ^= context->state[index];
    }
}

/* Move the context between the guest's flat memory and a host struct. The
   guest may hand the same context to a long chain of updates, so the state
   has to survive in guest memory between calls rather than in a host cache
   keyed on a pointer the guest is free to move. */
static void sha_context_load(uint32_t address, RecompShaContext *context)
{
    for (unsigned i = 0u; i < SHA1_STATE_WORDS; ++i) {
        context->state[i] =
            *recomp_memory_u32(address + SHA_CONTEXT_STATE + 4u * i);
    }
    context->bit_count =
        (uint64_t)*recomp_memory_u32(address + SHA_CONTEXT_COUNT) |
        ((uint64_t)*recomp_memory_u32(address + SHA_CONTEXT_COUNT + 4u) << 32u);
    for (unsigned i = 0u; i < SHA1_BLOCK_BYTES; ++i) {
        context->buffer[i] =
            (uint8_t)*recomp_memory_i8(address + SHA_CONTEXT_BUFFER + i);
    }
}

static void sha_context_store(uint32_t address, const RecompShaContext *context)
{
    for (unsigned i = 0u; i < SHA1_STATE_WORDS; ++i) {
        *recomp_memory_u32(address + SHA_CONTEXT_STATE + 4u * i) =
            context->state[i];
    }
    *recomp_memory_u32(address + SHA_CONTEXT_COUNT) =
        (uint32_t)context->bit_count;
    *recomp_memory_u32(address + SHA_CONTEXT_COUNT + 4u) =
        (uint32_t)(context->bit_count >> 32u);
    for (unsigned i = 0u; i < SHA1_BLOCK_BYTES; ++i) {
        *recomp_memory_i8(address + SHA_CONTEXT_BUFFER + i) =
            (int8_t)context->buffer[i];
    }
}

/* XcSHAInit(PUCHAR pbSHAContext) is stdcall and returns void. */
static void bridge_xc_sha_init(void)
{
    uint32_t context_address = kernel_arg(1u);

    if (context_address != 0u) {
        RecompShaContext context;

        recomp_kernel_sha_init(&context);
        sha_context_store(context_address, &context);
    }
    kernel_return(1u, 0u);
}

/* XcSHAUpdate(PUCHAR pbSHAContext, PUCHAR pbInput, ULONG cbInput). */
static void bridge_xc_sha_update(void)
{
    uint32_t context_address = kernel_arg(1u);
    uint32_t input = kernel_arg(2u);
    uint32_t length = kernel_arg(3u);

    if (context_address != 0u && (input != 0u || length == 0u)) {
        RecompShaContext context;

        sha_context_load(context_address, &context);
        if (length != 0u) {
            recomp_kernel_sha_update(
                &context, recomp_memory(input, length), length);
        }
        sha_context_store(context_address, &context);
    }
    kernel_return(3u, 0u);
}

/* XcSHAFinal(PUCHAR pbSHAContext, PUCHAR pbDigest) writes 20 bytes. */
static void bridge_xc_sha_final(void)
{
    uint32_t context_address = kernel_arg(1u);
    uint32_t digest_address = kernel_arg(2u);

    if (context_address != 0u && digest_address != 0u) {
        RecompShaContext context;
        uint8_t digest[RECOMP_SHA_DIGEST_BYTES];

        sha_context_load(context_address, &context);
        recomp_kernel_sha_final(&context, digest);
        for (unsigned i = 0u; i < RECOMP_SHA_DIGEST_BYTES; ++i) {
            *recomp_memory_i8(digest_address + i) = (int8_t)digest[i];
        }
        sha_context_store(context_address, &context);
    }
    kernel_return(2u, 0u);
}

/* XcHMAC(key, keyBytes, data1, bytes1, data2, bytes2, digest), void/ret 0x1C. */
static void bridge_xc_hmac(void)
{
    uint32_t key = kernel_arg(1u), key_length = kernel_arg(2u);
    uint32_t first = kernel_arg(3u), first_length = kernel_arg(4u);
    uint32_t second = kernel_arg(5u), second_length = kernel_arg(6u);
    uint32_t digest = kernel_arg(7u);

    if (key_length > SHA1_BLOCK_BYTES) key_length = SHA1_BLOCK_BYTES;
    if (digest != 0u && (key != 0u || key_length == 0u) &&
        (first != 0u || first_length == 0u) &&
        (second != 0u || second_length == 0u)) {
        recomp_kernel_hmac(
            key_length != 0u ? recomp_memory(key, key_length) : NULL, key_length,
            first_length != 0u ? recomp_memory(first, first_length) : NULL, first_length,
            second_length != 0u ? recomp_memory(second, second_length) : NULL, second_length,
            recomp_memory(digest, RECOMP_SHA_DIGEST_BYTES));
    }
    kernel_return(7u, 0u);
}

/* Xbox's context is S[256], byte i, byte j; widening indices overruns it. */
static void bridge_xc_rc4_key(void)
{
    uint32_t context = kernel_arg(1u), length = kernel_arg(2u), key = kernel_arg(3u);
    if (context != 0u && key != 0u && length != 0u) {
        /* The 256-step key schedule reads at most 256 bytes. */
        uint32_t key_bytes = length < 256u ? length : 256u;
        recomp_kernel_rc4_key((RecompRc4Context *)recomp_memory(context, RECOMP_RC4_CONTEXT_BYTES),
            recomp_memory(key, key_bytes), length);
    }
    kernel_return(3u, 0u);
}

static void bridge_xc_rc4_crypt(void)
{
    uint32_t context = kernel_arg(1u), length = kernel_arg(2u), input = kernel_arg(3u);
    if (context != 0u && (input != 0u || length == 0u)) {
        recomp_kernel_rc4_crypt((RecompRc4Context *)recomp_memory(context, RECOMP_RC4_CONTEXT_BYTES),
            length != 0u ? recomp_memory(input, length) : NULL, length);
    }
    kernel_return(3u, 0u);
}

RecompFunction recomp_kernel_crypto(uint32_t ordinal)
{
    switch (ordinal) {
    case 335u: return bridge_xc_sha_init;
    case 336u: return bridge_xc_sha_update;
    case 337u: return bridge_xc_sha_final;
    case 338u: return bridge_xc_rc4_key;
    case 339u: return bridge_xc_rc4_crypt;
    case 340u: return bridge_xc_hmac;
    default: return NULL;
    }
}
