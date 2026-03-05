if(CMAKE_BUILD_TYPE STREQUAL AddressSanitizer)
  message(STATUS "AddressSanitizer enabled")

  # environment variables
  # ASAN_OPTIONS=check_initialization_order=1,detect_stack_use_after_return=1
  add_compile_options(
    -O1
    -g3
    -fsanitize=address
    -fno-omit-frame-pointer # Leave frame pointers. Allows the fast unwinder to
                            # function properly.
    -fno-common # Do not treat global variable in C as common variables (allows
                # ASan to instrument them)
    -fno-optimize-sibling-calls # disable inlining and and tail call elimination
                                # for perfect stack traces
  )

  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address"
      CACHE INTERNAL "" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address"
      CACHE INTERNAL "" FORCE)
  set(CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=address"
      CACHE INTERNAL "" FORCE)

  function(sanitizer_fail_test_on_error test_name)
    set_tests_properties(${test_name} PROPERTIES FAIL_REGULAR_EXPRESSION
                                                 "ERROR: AddressSanitizer")
    set_tests_properties(${test_name} PROPERTIES FAIL_REGULAR_EXPRESSION
                                                 "ERROR: LeakSanitizer")
  endfunction(sanitizer_fail_test_on_error)

elseif(CMAKE_BUILD_TYPE STREQUAL MemorySanitizer)
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message(WARNING "MemorySanitizer might not be available with gcc")
  else()
    message(STATUS "MemorySanitizer enabled")
  endif()

  add_compile_options(
    -O1
    -g3
    -fsanitize=memory
    -fsanitize-memory-track-origins
    -fno-omit-frame-pointer # Leave frame pointers. Allows the fast unwinder to
                            # function properly.
    -fno-common # Do not treat global variable in C as common variables (allows
                # ASan to instrument them)
    -fno-optimize-sibling-calls # disable inlining and and tail call elimination
                                # for perfect stack traces
  )

  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory"
      CACHE INTERNAL "" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=memory"
      CACHE INTERNAL "" FORCE)
  set(CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=memory"
      CACHE INTERNAL "" FORCE)

  function(sanitizer_fail_test_on_error test_name)
    set_tests_properties(${test_name} PROPERTIES FAIL_REGULAR_EXPRESSION
                                                 "WARNING: MemorySanitizer")
  endfunction(sanitizer_fail_test_on_error)

elseif(CMAKE_BUILD_TYPE STREQUAL ThreadSanitizer)
  message(STATUS "ThreadSanitizer enabled")

  add_compile_options(-g3 -fsanitize=thread)

  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread"
      CACHE INTERNAL "" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread"
      CACHE INTERNAL "" FORCE)
  set(CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=thread"
      CACHE INTERNAL "" FORCE)

  function(sanitizer_fail_test_on_error test_name)
    set_tests_properties(${test_name} PROPERTIES FAIL_REGULAR_EXPRESSION
                                                 "WARNING: ThreadSanitizer")
  endfunction(sanitizer_fail_test_on_error)

elseif(CMAKE_BUILD_TYPE STREQUAL UndefinedBehaviorSanitizer)
  message(STATUS "UndefinedBehaviorSanitizer enabled")

  add_compile_options(
    -g3
    -fsanitize=alignment
    -fsanitize=bool
    -fsanitize=bounds
    -fsanitize=enum
    -fsanitize=float-cast-overflow
    -fsanitize=float-divide-by-zero
    -fsanitize=integer-divide-by-zero
    -fsanitize=nonnull-attribute
    -fsanitize=null
    -fsanitize=object-size
    -fsanitize=return
    -fsanitize=returns-nonnull-attribute
    -fsanitize=shift
    -fsanitize=signed-integer-overflow
    -fsanitize=unreachable
    -fsanitize=vla-bound
    -fno-sanitize-recover=bounds,null)

  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined"
      CACHE INTERNAL "" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined"
      CACHE INTERNAL "" FORCE)
  set(CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=undefined"
      CACHE INTERNAL "" FORCE)

  function(sanitizer_fail_test_on_error test_name)
    set_tests_properties(${test_name} PROPERTIES FAIL_REGULAR_EXPRESSION
                                                 "runtime error:")
  endfunction(sanitizer_fail_test_on_error)

elseif(CMAKE_BUILD_TYPE STREQUAL FuzzTesting)
  message(STATUS "FuzzTesting enabled")

  add_compile_options(-g3 -fsanitize=fuzzer -DFUZZTESTING)

  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=fuzzer $ENV{LIB_FUZZING_ENGINE}"
      CACHE INTERNAL "" FORCE)
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=fuzzer $ENV{LIB_FUZZING_ENGINE}"
      CACHE INTERNAL "" FORCE)
  set(CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS} -fsanitize=fuzzer $ENV{LIB_FUZZING_ENGINE}"
      CACHE INTERNAL "" FORCE)

  function(sanitizer_fail_test_on_error test_name)
    # Not sure what to do here
  endfunction(sanitizer_fail_test_on_error)
else()

  function(sanitizer_fail_test_on_error test_name)
    # default: don't do anything
  endfunction(sanitizer_fail_test_on_error)

endif()
