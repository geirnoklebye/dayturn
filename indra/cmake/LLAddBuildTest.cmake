# -*- cmake -*-
include(00-Common)
include(LLTestCommand)
include(bugsplat)
include(Tut)

#*****************************************************************************
#   LL_ADD_PROJECT_UNIT_TESTS
#*****************************************************************************
MACRO(LL_ADD_PROJECT_UNIT_TESTS project sources)
  # Given a project name and a list of sourcefiles (with optional properties on each),
  # add targets to build and run the tests specified.
  # ASSUMPTIONS:
  # * this macro is being executed in the project file that is passed in
  # * current working SOURCE dir is that project dir
  # * there is a subfolder tests/ with test code corresponding to the filenames passed in
  # * properties for each sourcefile passed in indicate what libs to link that file with (MAKE NO ASSUMPTIONS ASIDE FROM TUT)
  #
  # More info and examples at: https://wiki.secondlife.com/wiki/How_to_add_unit_tests_to_indra_code
  #
  # WARNING: do NOT modify this code without working with poppy -
  # there is another branch that will conflict heavily with any changes here.

  IF(LL_TEST_VERBOSE)
    MESSAGE("LL_ADD_PROJECT_UNIT_TESTS UNITTEST_PROJECT_${project} sources: ${sources}")
  ENDIF(LL_TEST_VERBOSE)

  # Start with the header and project-wide setup before making targets
  #project(UNITTEST_PROJECT_${project})
  # Setup includes, paths, etc
  SET(alltest_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/test/test.cpp
    ${CMAKE_SOURCE_DIR}/test/lltut.cpp
    )
  SET(alltest_DEP_TARGETS
    # needed by the test harness itself
    ${APRUTIL_LIBRARIES}
    ${APR_LIBRARIES}
    llcommon
    )
  IF(NOT "${project}" STREQUAL "llmath")
    # add llmath as a dep unless the tested module *is* llmath!
    LIST(APPEND alltest_DEP_TARGETS
      llmath
      )
  ENDIF(NOT "${project}" STREQUAL "llmath")
  SET(alltest_INCLUDE_DIRS
    ${LLMATH_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${LIBS_OPEN_DIR}/test
    )
  SET(alltest_LIBRARIES
    ${BOOST_FIBER_LIBRARY}
    ${BOOST_CONTEXT_LIBRARY}
    ${BOOST_SYSTEM_LIBRARY}
    ${PTHREAD_LIBRARY}
    ${WINDOWS_LIBRARIES}
    )
  # Headers, for convenience in targets.
  SET(alltest_HEADER_FILES
    ${CMAKE_SOURCE_DIR}/test/test.h
    )

  # start the source test executable definitions
  SET(${project}_TEST_OUTPUT "")
  FOREACH (source ${sources})
    STRING( REGEX REPLACE "(.*)\\.[^.]+$" "\\1" name ${source} )
    STRING( REGEX REPLACE ".*\\.([^.]+)$" "\\1" extension ${source} )
    IF(LL_TEST_VERBOSE)
      MESSAGE("LL_ADD_PROJECT_UNIT_TESTS UNITTEST_PROJECT_${project} individual source: ${source} (${name}.${extension})")
    ENDIF(LL_TEST_VERBOSE)

    #
    # Per-codefile additional / external source, header, and include dir property extraction
    #
    # Source
    GET_OPT_SOURCE_FILE_PROPERTY(${name}_test_additional_SOURCE_FILES ${source} LL_TEST_ADDITIONAL_SOURCE_FILES)
    SET(${name}_test_SOURCE_FILES
      ${source}
      tests/${name}_test.${extension}
      ${alltest_SOURCE_FILES}
      ${${name}_test_additional_SOURCE_FILES} )
    IF(LL_TEST_VERBOSE)
      MESSAGE("LL_ADD_PROJECT_UNIT_TESTS ${name}_test_SOURCE_FILES ${${name}_test_SOURCE_FILES}")
    ENDIF(LL_TEST_VERBOSE)

    if (USE_BUGSPLAT)
      SET_PROPERTY(SOURCE ${${name}_test_SOURCE_FILES}
          APPEND PROPERTY COMPILE_DEFINITIONS "${BUGSPLAT_DEFINE}")
    endif (USE_BUGSPLAT)

    # Headers
    GET_OPT_SOURCE_FILE_PROPERTY(${name}_test_additional_HEADER_FILES ${source} LL_TEST_ADDITIONAL_HEADER_FILES)
    SET(${name}_test_HEADER_FILES ${name}.h ${${name}_test_additional_HEADER_FILES})
    set_source_files_properties(${${name}_test_HEADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
    LIST(APPEND ${name}_test_SOURCE_FILES ${${name}_test_HEADER_FILES})
    IF(LL_TEST_VERBOSE)
      MESSAGE("LL_ADD_PROJECT_UNIT_TESTS ${name}_test_HEADER_FILES ${${name}_test_HEADER_FILES}")
    ENDIF(LL_TEST_VERBOSE)
    # Include dirs
    GET_OPT_SOURCE_FILE_PROPERTY(${name}_test_additional_INCLUDE_DIRS ${source} LL_TEST_ADDITIONAL_INCLUDE_DIRS)
    INCLUDE_DIRECTORIES(${alltest_INCLUDE_DIRS} ${${name}_test_additional_INCLUDE_DIRS} )
    IF(LL_TEST_VERBOSE)
      MESSAGE("LL_ADD_PROJECT_UNIT_TESTS ${name}_test_additional_INCLUDE_DIRS ${${name}_test_additional_INCLUDE_DIRS}")
    ENDIF(LL_TEST_VERBOSE)


    # Setup target
    ADD_EXECUTABLE(PROJECT_${project}_TEST_${name} ${${name}_test_SOURCE_FILES})
    SET_TARGET_PROPERTIES(PROJECT_${project}_TEST_${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${EXE_STAGING_DIR}")

    #
    # Per-codefile additional / external project dep and lib dep property extraction
    #
    # WARNING: it's REALLY IMPORTANT to not mix these. I guarantee it will not work in the future. + poppy 2009-04-19
    # Projects
    GET_OPT_SOURCE_FILE_PROPERTY(${name}_test_additional_PROJECTS ${source} LL_TEST_ADDITIONAL_PROJECTS)
    # Libraries
    GET_OPT_SOURCE_FILE_PROPERTY(${name}_test_additional_LIBRARIES ${source} LL_TEST_ADDITIONAL_LIBRARIES)
    IF(LL_TEST_VERBOSE)
      MESSAGE("LL_ADD_PROJECT_UNIT_TESTS ${name}_test_additional_PROJECTS ${${name}_test_additional_PROJECTS}")
      MESSAGE("LL_ADD_PROJECT_UNIT_TESTS ${name}_test_additional_LIBRARIES ${${name}_test_additional_LIBRARIES}")
    ENDIF(LL_TEST_VERBOSE)
    # Add to project
    TARGET_LINK_LIBRARIES(PROJECT_${project}_TEST_${name} ${alltest_LIBRARIES} ${alltest_DEP_TARGETS} ${${name}_test_additional_PROJECTS} ${${name}_test_additional_LIBRARIES} )
    # Compile-time Definitions
    GET_OPT_SOURCE_FILE_PROPERTY(${name}_test_additional_CFLAGS ${source} LL_TEST_ADDITIONAL_CFLAGS)
    SET_TARGET_PROPERTIES(PROJECT_${project}_TEST_${name}
      PROPERTIES
      COMPILE_FLAGS "${${name}_test_additional_CFLAGS}"
      COMPILE_DEFINITIONS "LL_TEST=${name};LL_TEST_${name}")
    IF(LL_TEST_VERBOSE)
      MESSAGE("LL_ADD_PROJECT_UNIT_TESTS ${name}_test_additional_CFLAGS ${${name}_test_additional_CFLAGS}")
    ENDIF(LL_TEST_VERBOSE)
     
    #
    # Setup test targets
    #
    SET(TEST_EXE $<TARGET_FILE:PROJECT_${project}_TEST_${name}>)
    SET(TEST_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/PROJECT_${project}_TEST_${name}_ok.txt)
    SET(TEST_CMD ${TEST_EXE} --touch=${TEST_OUTPUT} --sourcedir=${CMAKE_CURRENT_SOURCE_DIR})

    # daveh - what configuration does this use? Debug? it's cmake-time, not build time. + poppy 2009-04-19
    IF(LL_TEST_VERBOSE)
      MESSAGE(STATUS "LL_ADD_PROJECT_UNIT_TESTS ${name} test_cmd  = ${TEST_CMD}")
    ENDIF(LL_TEST_VERBOSE)

    SET_TEST_PATH(LD_LIBRARY_PATH)
    LL_TEST_COMMAND(TEST_SCRIPT_CMD "${LD_LIBRARY_PATH}" ${TEST_CMD})
    IF(LL_TEST_VERBOSE)
      MESSAGE(STATUS "LL_ADD_PROJECT_UNIT_TESTS ${name} test_script  = ${TEST_SCRIPT_CMD}")
    ENDIF(LL_TEST_VERBOSE)
    # Add test 
    ADD_CUSTOM_COMMAND(
        OUTPUT ${TEST_OUTPUT}
        COMMAND ${TEST_SCRIPT_CMD}
        DEPENDS PROJECT_${project}_TEST_${name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    # Why not add custom target and add POST_BUILD command?
    # Slightly less uncertain behavior
    # (OUTPUT commands run non-deterministically AFAIK) + poppy 2009-04-19
    # > I did not use a post build step as I could not make it notify of a 
    # > failure after the first time you build and fail a test. - daveh 2009-04-20
    LIST(APPEND ${project}_TEST_OUTPUT ${TEST_OUTPUT})
  ENDFOREACH (source)

  # Add the test runner target per-project
  # (replaces old _test_ok targets all over the place)
  ADD_CUSTOM_TARGET(${project}_tests ALL DEPENDS ${${project}_TEST_OUTPUT})
  ADD_DEPENDENCIES(${project} ${project}_tests)
ENDMACRO(LL_ADD_PROJECT_UNIT_TESTS)

#*****************************************************************************
#   GET_OPT_SOURCE_FILE_PROPERTY
#*****************************************************************************
MACRO(GET_OPT_SOURCE_FILE_PROPERTY var filename property)
  GET_SOURCE_FILE_PROPERTY(${var} "${filename}" "${property}")
  IF("${${var}}" MATCHES NOTFOUND)
    SET(${var} "")
  ENDIF("${${var}}" MATCHES NOTFOUND)
ENDMACRO(GET_OPT_SOURCE_FILE_PROPERTY)

#*****************************************************************************
#   LL_ADD_INTEGRATION_TEST
#*****************************************************************************
FUNCTION(LL_ADD_INTEGRATION_TEST 
    testname
    additional_source_files
    library_dependencies
# variable args
    )
  if(TEST_DEBUG)
    message(STATUS "Adding INTEGRATION_TEST_${testname} - debug output is on")
  endif(TEST_DEBUG)

  SET(source_files
    tests/${testname}_test.cpp
    ${CMAKE_SOURCE_DIR}/test/test.cpp
    ${CMAKE_SOURCE_DIR}/test/lltut.cpp
    ${additional_source_files}
    )

  SET(libraries
    ${LEGACY_STDIO_LIBS}
    ${library_dependencies}
    ${BOOST_FIBER_LIBRARY}
    ${BOOST_CONTEXT_LIBRARY}
    ${BOOST_SYSTEM_LIBRARY}
    ${PTHREAD_LIBRARY}
    )

  # Add test executable build target
  if(TEST_DEBUG)
    message(STATUS "ADD_EXECUTABLE(INTEGRATION_TEST_${testname} ${source_files})")
  endif(TEST_DEBUG)
  ADD_EXECUTABLE(INTEGRATION_TEST_${testname} ${source_files})
  SET_TARGET_PROPERTIES(INTEGRATION_TEST_${testname}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EXE_STAGING_DIR}"
    COMPILE_DEFINITIONS "LL_TEST=${testname};LL_TEST_${testname}"
    )

  if(USESYSTEMLIBS)
    SET_TARGET_PROPERTIES(INTEGRATION_TEST_${testname} PROPERTIES COMPILE_FLAGS -I"${TUT_INCLUDE_DIR}")
  endif(USESYSTEMLIBS)

  if (USE_BUGSPLAT)
      SET_PROPERTY(SOURCE ${source_files}
          APPEND PROPERTY COMPILE_DEFINITIONS "${BUGSPLAT_DEFINE}")
  endif (USE_BUGSPLAT)

  # The following was copied to llcorehttp/CMakeLists.txt's texture_load target. 
  # Any changes made here should be replicated there.
  if (WINDOWS)
    SET_TARGET_PROPERTIES(INTEGRATION_TEST_${testname}
        PROPERTIES
        LINK_FLAGS "/debug /NODEFAULTLIB:LIBCMT /SUBSYSTEM:CONSOLE"
        LINK_FLAGS_DEBUG "/NODEFAULTLIB:\"LIBCMT;LIBCMTD;MSVCRT\" /INCREMENTAL:NO"
        LINK_FLAGS_RELEASE ""
        )
  endif (WINDOWS)

  # Add link deps to the executable
  if(TEST_DEBUG)
    message(STATUS "TARGET_LINK_LIBRARIES(INTEGRATION_TEST_${testname} ${libraries})")
  endif(TEST_DEBUG)
  TARGET_LINK_LIBRARIES(INTEGRATION_TEST_${testname} ${libraries})

  # Create the test running command
  SET(test_command ${ARGN})
  SET(TEST_EXE $<TARGET_FILE:INTEGRATION_TEST_${testname}>)
  LIST(FIND test_command "{}" test_exe_pos)
  IF(test_exe_pos LESS 0)
    # The {} marker means "the full pathname of the test executable."
    # test_exe_pos -1 means we didn't find it -- so append the test executable
    # name to $ARGN, the variable part of the arg list. This is convenient
    # shorthand for both straightforward execution of the test program (empty
    # $ARGN) and for running a "wrapper" program of some kind accepting the
    # pathname of the test program as the last of its args. You need specify
    # {} only if the test program's pathname isn't the last argument in the
    # desired command line.
    LIST(APPEND test_command "${TEST_EXE}")
  ELSE (test_exe_pos LESS 0)
    # Found {} marker at test_exe_pos. Remove the {}...
    LIST(REMOVE_AT test_command test_exe_pos)
    # ...and replace it with the actual name of the test executable.
    LIST(INSERT test_command test_exe_pos "${TEST_EXE}")
  ENDIF (test_exe_pos LESS 0)

  SET_TEST_PATH(LD_LIBRARY_PATH)
  LL_TEST_COMMAND(TEST_SCRIPT_CMD "${LD_LIBRARY_PATH}" ${test_command})

  if(TEST_DEBUG)
    message(STATUS "TEST_SCRIPT_CMD: ${TEST_SCRIPT_CMD}")
  endif(TEST_DEBUG)

  ADD_CUSTOM_COMMAND(
    TARGET INTEGRATION_TEST_${testname}
    POST_BUILD
    COMMAND ${TEST_SCRIPT_CMD}
    )

  # Use CTEST? Not sure how to yet...
  # ADD_TEST(INTEGRATION_TEST_RUNNER_${testname} ${TEST_SCRIPT_CMD})

ENDFUNCTION(LL_ADD_INTEGRATION_TEST)

#*****************************************************************************
#   SET_TEST_PATH
#*****************************************************************************
MACRO(SET_TEST_PATH LISTVAR)
  IF(WINDOWS)
    # We typically build/package only Release variants of third-party
    # libraries, so append the Release staging dir in case the library being
    # sought doesn't have a debug variant.
    set(${LISTVAR} ${SHARED_LIB_STAGING_DIR}/${CMAKE_CFG_INTDIR} ${SHARED_LIB_STAGING_DIR}/Release)
  ELSEIF(DARWIN)
    # We typically build/package only Release variants of third-party
    # libraries, so append the Release staging dir in case the library being
    # sought doesn't have a debug variant.
    set(${LISTVAR} ${SHARED_LIB_STAGING_DIR}/${CMAKE_CFG_INTDIR}/Resources ${SHARED_LIB_STAGING_DIR}/Release/Resources /usr/lib)
  ELSE(WINDOWS)
    # Linux uses a single staging directory anyway.
    IF (USESYSTEMLIBS)
      set(${LISTVAR} ${CMAKE_BINARY_DIR}/llcommon /usr/lib /usr/local/lib)
    ELSE (USESYSTEMLIBS)
      set(${LISTVAR} ${SHARED_LIB_STAGING_DIR} /usr/lib)
    ENDIF (USESYSTEMLIBS)
  ENDIF(WINDOWS)
ENDMACRO(SET_TEST_PATH)
