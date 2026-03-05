function(generate_build_info target template_dir)
  if(NOT TARGET ${target})
    message(
      FATAL_ERROR "generate_build_info: target '${target}' does not exist")
  endif()

  set(TEMPLATE_FILE "${template_dir}/build_info.hpp.in")
  set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/build_info.hpp")
  set(GENERATE_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/generate_build_info.cmake")

  if(NOT EXISTS "${TEMPLATE_FILE}")
    message(FATAL_ERROR "build_info.hpp.in not found in ${template_dir}")
  endif()

  file(
    WRITE "${GENERATE_SCRIPT}"
    "
        execute_process(
            COMMAND git rev-parse --short HEAD
            WORKING_DIRECTORY \"${CMAKE_SOURCE_DIR}\"
            OUTPUT_VARIABLE GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT GIT_COMMIT_HASH)
            set(GIT_COMMIT_HASH \"unknown\")
        endif()

        string(TIMESTAMP BUILD_TIME \"%Y-%m-%d %H:%M:%S\")

        configure_file(\"${TEMPLATE_FILE}\" \"${OUTPUT_FILE}\" @ONLY)
    ")

  # Create a custom target that always runs to update build info
  add_custom_target(
    ${target}_build_info ALL
    COMMAND ${CMAKE_COMMAND} -P "${GENERATE_SCRIPT}"
    BYPRODUCTS "${OUTPUT_FILE}"
    COMMENT "Updating build information (Git hash & Timestamp)..."
    VERBATIM)

  # Make the main target depend on the build info target
  add_dependencies(${target} ${target}_build_info)

  target_sources(${target} PRIVATE "${OUTPUT_FILE}")
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

endfunction()
