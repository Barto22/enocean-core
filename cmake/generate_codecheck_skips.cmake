function(generate_codecheck_skips OUTPUT_FILE)
  set(SKIP_PATHS ${ARGN})
  file(WRITE ${OUTPUT_FILE} "")

  foreach(PATH ${SKIP_PATHS})
    get_filename_component(ABSOLUTE_PATH "${PATH}" ABSOLUTE BASE_DIR
                           "${CMAKE_CURRENT_SOURCE_DIR}")

    get_filename_component(FILE_EXT "${PATH}" EXT)

    set(FORMATTED_PATH "-${ABSOLUTE_PATH}")

    if(FILE_EXT STREQUAL "")
      set(FORMATTED_PATH "${FORMATTED_PATH}/*")
    endif()

    file(APPEND ${OUTPUT_FILE} "${FORMATTED_PATH}\n")
  endforeach()
endfunction()
