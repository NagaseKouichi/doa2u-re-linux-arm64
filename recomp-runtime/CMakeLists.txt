cmake_minimum_required(VERSION 3.20)
project(doa2u_recomp_runtime C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

# These targets use synthetic fixtures, with no guest address bindings.
add_executable(recomp_model_test
    model_test.c
    apu_model.c apu_model_test.c
    ohci_model.c ohci_model_test.c
    fiber_model.c fiber_model_test.c
    directory_model.c directory_model_test.c
    symbolic_link_model.c symbolic_link_model_test.c
    cri_service_model.c cri_service_model_test.c
    d3d_frame_model.c d3d_frame_model_test.c)
add_test(NAME recomp-runtime-models COMMAND recomp_model_test)

add_executable(recomp_manual_test program_manual.c program_manual_test.c)
add_test(NAME recomp-runtime-unbound-lookups COMMAND recomp_manual_test)

add_executable(recomp_xbox_adpcm_test xbox_adpcm.c xbox_adpcm_test.c)
add_test(NAME recomp-runtime-xbox-adpcm COMMAND recomp_xbox_adpcm_test)

add_executable(recomp_movie_color_test movie_color_model.c movie_color_model_test.c)
add_test(NAME recomp-runtime-movie-color COMMAND recomp_movie_color_test)

add_executable(recomp_dsound_hrtf_test dsound_hrtf.c dsound_hrtf_test.c)
if(NOT MSVC)
    target_link_libraries(recomp_dsound_hrtf_test PRIVATE m)
endif()
add_test(NAME recomp-runtime-dsound-hrtf COMMAND recomp_dsound_hrtf_test)

add_executable(recomp_save_transaction_test save_transaction.cpp save_transaction_test.cpp)
add_test(NAME recomp-runtime-save-transaction COMMAND recomp_save_transaction_test)

add_executable(recomp_controller_settings_test controller_settings.cpp controller_settings_test.cpp)
add_test(NAME recomp-runtime-controller-settings COMMAND recomp_controller_settings_test)

add_executable(recomp_input_pulse_source_test input_pulse_source.c input_pulse_source_test.c)
target_compile_definitions(recomp_input_pulse_source_test PRIVATE RECOMP_INPUT_PULSE_SOURCE_STANDALONE)
add_test(NAME recomp-runtime-input-pulses COMMAND recomp_input_pulse_source_test)

if(WIN32)
    add_executable(recomp_d3d_vblank_test d3d_vblank.cpp xapi_clock.cpp d3d_vblank_test.cpp)
    add_test(NAME recomp-runtime-d3d-vblank COMMAND recomp_d3d_vblank_test)
    add_test(NAME recomp-runtime-unpaced COMMAND "${CMAKE_COMMAND}" -E env
        RECOMP_UNPACED=1 "$<TARGET_FILE:recomp_d3d_vblank_test>")
endif()

add_executable(recomp_program_abi_call_test program_abi_call_test.c)
add_test(NAME recomp-runtime-program-abi-call COMMAND recomp_program_abi_call_test)

add_executable(recomp_guest_heap_test guest_heap.c guest_heap_test.c)
add_test(NAME recomp-runtime-guest-heap COMMAND recomp_guest_heap_test)

if(WIN32)
add_library(recomp_xbe STATIC ../xbe/sha.cpp ../xbe/xbe_parser.cpp runner_load.cpp)
target_include_directories(recomp_xbe PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../xbe" "${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(recomp_runner_load_test runner_load_test.cpp)
target_link_libraries(recomp_runner_load_test PRIVATE recomp_xbe)
add_test(NAME recomp-runtime-runner-load COMMAND recomp_runner_load_test)

    target_link_libraries(recomp_xbe PUBLIC bcrypt)
    add_library(recomp_kernel STATIC
        runtime.c stop_report.c guest_heap.c xapi_clock.cpp native_fiber.c
        kernel_config.c kernel_crypto.c kernel_debug.c kernel_device.c
        kernel_dispatch.c kernel_file.c kernel_interrupt.c kernel_memory.c
        kernel_ordinals.c kernel_rtl.c kernel_startup.c kernel_thread.c kernel_video.c
        device_model.c apu_model.c ohci_model.c directory_model.c symbolic_link_model.c
        save_transaction.cpp)
    target_include_directories(recomp_kernel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(recomp_kernel PUBLIC bcrypt)
    add_executable(recomp_kernel_test kernel_test.c kernel_device_test.c kernel_crypto_test.c
        kernel_rtl_test.c kernel_thread_test.c kernel_video_test.c
        flag_macro_test.c sse_semantics_test.c)
    target_link_libraries(recomp_kernel_test PRIVATE recomp_kernel)
    add_test(NAME recomp-runtime-kernel COMMAND recomp_kernel_test)
    add_executable(recomp_kernel_scheduler_test kernel_scheduler_test.c)
    target_link_libraries(recomp_kernel_scheduler_test PRIVATE recomp_kernel)
    add_test(NAME recomp-runtime-kernel-scheduler COMMAND recomp_kernel_scheduler_test)
    set_tests_properties(recomp-runtime-kernel-scheduler PROPERTIES TIMEOUT 10)
endif()

set(RECOMP_PROGRAM_DIR "" CACHE PATH "Authenticated complete generated-source snapshot")
set(RECOMP_PROGRAM_MANIFEST_SHA256 "" CACHE STRING "SHA-256 of the generated-source manifest")
set(RECOMP_XBE_SHA256 "" CACHE STRING "SHA-256 of the executable used to generate this program")
set(RECOMP_PROGRAM_EBP_EXPECTED 0 CACHE STRING "Expected uninitialized generated EBP prologue count")
if((RECOMP_PROGRAM_DIR AND NOT RECOMP_PROGRAM_MANIFEST_SHA256) OR
   (RECOMP_PROGRAM_MANIFEST_SHA256 AND NOT RECOMP_PROGRAM_DIR))
    message(FATAL_ERROR "RECOMP_PROGRAM_DIR and RECOMP_PROGRAM_MANIFEST_SHA256 must be supplied together")
endif()

if(RECOMP_PROGRAM_DIR)
    if(NOT WIN32)
        message(FATAL_ERROR "The initial kernel-only runner requires Windows")
    endif()
    get_filename_component(_program_dir "${RECOMP_PROGRAM_DIR}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
    get_filename_component(_program_dir "${_program_dir}" REALPATH)
    get_filename_component(_private_dir "${CMAKE_CURRENT_SOURCE_DIR}/../private" REALPATH)
    file(RELATIVE_PATH _private_relative "${_private_dir}" "${_program_dir}")
    if(_private_relative MATCHES [=[^\.\.]=] OR IS_ABSOLUTE "${_private_relative}")
        message(FATAL_ERROR "Generated program source must stay under private/")
    endif()
    file(GLOB _program_names CONFIGURE_DEPENDS LIST_DIRECTORIES false RELATIVE "${_program_dir}"
        "${_program_dir}/recomp_[0-9][0-9][0-9][0-9].c")
    list(SORT _program_names)
    if(NOT _program_names)
        message(FATAL_ERROR "Authenticated program has no source chunks")
    endif()
    set(_chunk_index 0)
    foreach(_name IN LISTS _program_names)
        string(REGEX REPLACE [=[^recomp_0*([0-9]+)\.c$]=] "\\1" _number "${_name}")
        if(NOT _number EQUAL _chunk_index)
            message(FATAL_ERROR "Authenticated program chunk sequence has a gap: ${_name}")
        endif()
        math(EXPR _chunk_index "${_chunk_index} + 1")
    endforeach()
    list(APPEND _program_names recomp_dispatch.c recomp_funcs.h)
    set(_program_manifest "")
    foreach(_name IN LISTS _program_names)
        set(_path "${_program_dir}/${_name}")
        if(NOT EXISTS "${_path}")
            message(FATAL_ERROR "Authenticated program member is missing: ${_name}")
        endif()
        file(SIZE "${_path}" _size)
        file(SHA256 "${_path}" _sha256)
        string(APPEND _program_manifest "${_name}\t${_size}\t${_sha256}\n")
    endforeach()
    string(SHA256 _manifest_sha256 "${_program_manifest}")
    string(TOLOWER "${RECOMP_PROGRAM_MANIFEST_SHA256}" _expected_manifest_sha256)
    if(NOT _manifest_sha256 STREQUAL _expected_manifest_sha256)
        message(FATAL_ERROR "RECOMP_PROGRAM_DIR does not match its authenticated manifest")
    endif()
    if(NOT RECOMP_PROGRAM_EBP_EXPECTED STREQUAL "0")
        message(FATAL_ERROR "Uninitialized generated EBP must be fixed in the lifter, then regenerated")
    endif()
    string(LENGTH "${RECOMP_XBE_SHA256}" _xbe_sha256_length)
    if(NOT _xbe_sha256_length EQUAL 64 OR NOT RECOMP_XBE_SHA256 MATCHES "^[0-9A-Fa-f]+$")
        message(FATAL_ERROR "RECOMP_XBE_SHA256 must authenticate the generated program's executable")
    endif()
    string(TOLOWER "${RECOMP_XBE_SHA256}" _xbe_sha256)
    # Isolate the lifter's recomp_types.h while keeping all game-derived files private.
    get_filename_component(_program_parent "${_program_dir}" DIRECTORY)
    set(_derived_dir "${_program_parent}/runtime-sources")
    file(MAKE_DIRECTORY "${_derived_dir}")
    set(_program_sources)
    set(_definitions)
    set(_references)
    foreach(_name IN LISTS _program_names)
        set(_path "${_program_dir}/${_name}")
        configure_file("${_path}" "${_derived_dir}/${_name}" COPYONLY)
        if(_name MATCHES [=[\.c$]=])
            list(APPEND _program_sources "${_derived_dir}/${_name}")
            file(READ "${_path}" _source_text)
            if(_source_text MATCHES "\n[ \t]+uint32_t ebp;[ \r]*\n")
                message(FATAL_ERROR "Authenticated program has an uninitialized EBP prologue")
            endif()
            string(REGEX MATCHALL "void[ \t\r\n]+sub_[0-9A-Fa-f]+[ \t\r\n]*\\([ \t\r\n]*void[ \t\r\n]*\\)[ \t\r\n]*\\{" _matches "${_source_text}")
            foreach(_match IN LISTS _matches)
                string(REGEX MATCH "sub_[0-9A-Fa-f]+" _symbol "${_match}")
                list(APPEND _definitions "${_symbol}")
            endforeach()
            string(REGEX MATCHALL "sub_[0-9A-Fa-f]+" _matches "${_source_text}")
            list(APPEND _references ${_matches})
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _definitions)
    list(REMOVE_DUPLICATES _references)
    set(_missing_source "${_derived_dir}/recomp_program_missing.c")
    file(WRITE "${_missing_source}" "#include <stdint.h>\nvoid recomp_program_missing(uint32_t);\n")
    set(_missing_count 0)
    foreach(_symbol IN LISTS _references)
        list(FIND _definitions "${_symbol}" _defined_index)
        if(_defined_index EQUAL -1)
            string(SUBSTRING "${_symbol}" 4 -1 _address)
            file(APPEND "${_missing_source}" "void ${_symbol}(void) { recomp_program_missing(0x${_address}u); }\n")
            math(EXPR _missing_count "${_missing_count} + 1")
        endif()
    endforeach()
    message(STATUS "Generated ${_missing_count} fail-loud stubs for absent snapshot bodies")
    add_executable(recomp_program_runner runner.cpp program_adapters.c program_manual.c
        host_diagnostics.cpp "${_missing_source}" ${_program_sources})
    target_compile_definitions(recomp_program_runner PRIVATE RECOMP_XBE_SHA256="${_xbe_sha256}")
    if(MSVC)
        target_compile_options(recomp_program_runner PRIVATE /we4013)
    endif()
    target_link_libraries(recomp_program_runner PRIVATE recomp_kernel recomp_xbe dbghelp)
    target_include_directories(recomp_program_runner PRIVATE "${_derived_dir}")
endif()
