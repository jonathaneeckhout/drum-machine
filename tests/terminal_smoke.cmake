# Run the actual executable with no display and feed it real terminal commands.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/terminal-input.txt"
    "clear\nstep 1 1\ntempo 137\nvolume 1 0.5\nplay\nstatus\nstop\nquit\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY
        SDL_AUDIODRIVER=dummy "${DRUM_MACHINE}" --terminal
    INPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/terminal-input.txt"
    OUTPUT_VARIABLE output ERROR_VARIABLE error RESULT_VARIABLE result
    TIMEOUT 10)
if(NOT result EQUAL 0 OR NOT output MATCHES "playing / 137 BPM")
    message(FATAL_ERROR "Terminal integration failed (${result}): ${output} ${error}")
endif()
