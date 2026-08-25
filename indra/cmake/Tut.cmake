# -*- cmake -*-

# The tut unit testing framework is a small set of header-only files that
# almost never change, so it is vendored in indra/externals rather than
# fetched as a prebuilt package.
#
# TUT_INCLUDE_DIR is referenced explicitly by indra/test/CMakeLists.txt and by
# LL_ADD_INTEGRATION_TEST; adding it here as well covers the libraries that
# used to pick tut up implicitly from the prebuilt include directory.

set(TUT_INCLUDE_DIR ${LIBS_OPEN_DIR}/externals/tut)

include_directories(SYSTEM ${TUT_INCLUDE_DIR})
