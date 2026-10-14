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
