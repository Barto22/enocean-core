function(add_codechecker CODECHECKER_SCRIPT_DIR CONFIG_DIR)
  set(CODECHECKER_SCRIPT "${CODECHECKER_SCRIPT_DIR}/codechecker_runner.py")

  # Use optional third argument for build directory, otherwise use current
  # binary dir
  if(ARGC GREATER 2)
    set(BUILD_DIR "${ARGV2}")
  else()
    set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")
  endif()

  find_program(PYTHON_EXECUTABLE NAMES python3 python REQUIRED)
  add_custom_target(
    codechecker_analyse
    COMMAND ${PYTHON_EXECUTABLE} ${CODECHECKER_SCRIPT} "${CONFIG_DIR}"
            "${BUILD_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Running CodeChecker static analysis and generating HTML report...")
endfunction()
