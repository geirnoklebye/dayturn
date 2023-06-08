# -*- cmake -*-
#
# Compilation options shared by all Dayturn components.

#*****************************************************************************
#   It's important to realize that CMake implicitly concatenates
#   CMAKE_CXX_FLAGS with (e.g.) CMAKE_CXX_FLAGS_RELEASE for Release builds. So
#   set switches in CMAKE_CXX_FLAGS that should affect all builds, but in
#   CMAKE_CXX_FLAGS_RELEASE or CMAKE_CXX_FLAGS_RELWITHDEBINFO for switches
#   that should affect only that build variant.
#
#   Also realize that CMAKE_CXX_FLAGS may already be partially populated on
#   entry to this file.
#*****************************************************************************


if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

include(Variables)

# Portable compilation flags.
set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG -DLL_DEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE
    "-DLL_RELEASE=1 -DLL_RELEASE_FOR_DOWNLOAD=1 -DNDEBUG")

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
    "-DLL_RELEASE=1 -DNDEBUG -DLL_RELEASE_WITH_DEBUG_INFO=1")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DADDRESS_SIZE=64")

# Configure crash reporting
set(RELEASE_CRASH_REPORTING OFF CACHE BOOL "Enable use of crash reporting in release builds")
set(NON_RELEASE_CRASH_REPORTING OFF CACHE BOOL "Enable use of crash reporting in developer builds")

if(RELEASE_CRASH_REPORTING)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DLL_SEND_CRASH_REPORTS=0")
endif()

if(NON_RELEASE_CRASH_REPORTING)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DLL_SEND_CRASH_REPORTS=0")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DLL_SEND_CRASH_REPORTS=0")
endif()  

# Don't bother with MinSizeRel or Debug builds.
set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Release" CACHE STRING
    "Supported build types." FORCE)


# Platform-specific compilation flags. All non-Darwin removed


if (DARWIN)
  add_definitions(-DLL_DARWIN=1)
  set(CMAKE_CXX_LINK_FLAGS "-Wl,-headerpad_max_install_names,-search_paths_first,-rpath,@loader_path/../Frameworks")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
  set(DARWIN_extra_cstar_flags "-Wbool-conversion -Wno-unused-local-typedef -Wno-deprecated-declarations -Wobjc-signed-char-bool-implicit-int-conversion")
  # Ensure that CMAKE_CXX_FLAGS has the correct -g debug information format --
  # see Variables.cmake.
  string(REPLACE "-gdwarf-2" "-g${CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT}"
    CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  # The viewer code base can now be successfully compiled with -std=c++14. But
  # turning that on in the generic viewer-build-variables/variables file would
  # potentially require tweaking each of our ~50 third-party library builds.
  # Until we decide to set -std=c++14 in viewer-build-variables/variables, set
  # it locally here: we want to at least prevent inadvertently reintroducing
  # viewer code that would fail with C++14.        
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DARWIN_extra_cstar_flags} -std=gnu++14")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${DARWIN_extra_cstar_flags}")
  # NOTE: it's critical that the optimization flag is put in front.
  # NOTE: it's critical to have both CXX_FLAGS and C_FLAGS covered.
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 ${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_C_FLAGS_RELEASE "-O3 ${CMAKE_C_FLAGS_RELEASE}")  
  set(ENABLE_SIGNING TRUE)
  set(SIGNING_IDENTITY "Developer ID Application: Geir Noklebye")

  # required for clang-15/xcode-15 since our boost package still uses deprecated std::unary_function/binary_function
  # see https://developer.apple.com/documentation/xcode-release-notes/xcode-15-release-notes#C++-Standard-Library
  # add_compile_definitions(_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION)

  if (CMAKE_CXX_COMPILER MATCHES ".*clang")
    set(CMAKE_COMPILER_IS_CLANGXX 1)
  endif (CMAKE_CXX_COMPILER MATCHES ".*clang")

  if (NOT GCC_DISABLE_FATAL_WARNINGS)
     set(GCC_WARNINGS "${GCC_WARNINGS} -Werror")
  endif (NOT GCC_DISABLE_FATAL_WARNINGS)

    set(GCC_CXX_WARNINGS "$[GCC_WARNINGS] -Wno-reorder -Wno-non-virtual-dtor")

  set(GCC_CXX_WARNINGS "$[GCC_WARNINGS] -Wno-reorder -Wno-non-virtual-dtor")


  set(CMAKE_C_FLAGS "${GCC_WARNINGS} ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${GCC_CXX_WARNINGS} ${CMAKE_CXX_FLAGS}")

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m${ADDRESS_SIZE}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m${ADDRESS_SIZE}")

endif (DARWIN)


if (WINDOWS)
  # Don't build DLLs.
  set(BUILD_SHARED_LIBS OFF)

  # for "backwards compatibility", cmake sneaks in the Zm1000 option which royally
  # screws incredibuild. this hack disables it.
  # for details see: http://connect.microsoft.com/VisualStudio/feedback/details/368107/clxx-fatal-error-c1027-inconsistent-values-for-ym-between-creation-and-use-of-precompiled-headers
  # http://www.ogre3d.org/forums/viewtopic.php?f=2&t=60015
  # http://www.cmake.org/pipermail/cmake/2009-September/032143.html
  string(REPLACE "/Zm1000" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})


  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Zc:wchar_t- /Zi /MD /MP /GR /O2 -D_SECURE_STL=0 -D_HAS_ITERATOR_DEBUGGING=0"
      CACHE STRING "C++ compiler release-with-debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${LL_CXX_FLAGS} /Zc:wchar_t- /Zi /GR /MD /MP /O2 /Ob2"
      CACHE STRING "C++ compiler release options" FORCE)
  
  # zlib has assembly-language object files incompatible with SAFESEH
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE /SAFESEH:NO /NODEFAULTLIB:LIBCMT /IGNORE:4099")

  set(CMAKE_CXX_STANDARD_LIBRARIES "")
  set(CMAKE_C_STANDARD_LIBRARIES "")

  add_definitions(
      /D_SECURE_STL=0
      /D_HAS_ITERATOR_DEBUGGING=0
      /D_WINDOWS
      /DLL_WINDOWS=1
      /DNOMINMAX
#      /DDOM_DYNAMIC            # For shared library colladadom
      /DUNICODE
      /D_UNICODE
      /DWINVER=0x0600
      /D_WIN32_WINNT=0x0600
      /DLL_OS_DRAGDROP_ENABLED=1
      /DCARES_STATICLIB
      /DLIB_NDOF=1
      )
  add_compile_options(
      /GS
      /TP
      /W3
      /c
      /Zc:forScope
      /nologo
      /Oy-
      /fp:fast
      )

  # Nicky: x64 implies SSE2
  if( ADDRESS_SIZE EQUAL 32 )
    add_definitions( /arch:SSE2 )
  endif()

  # Are we using the crummy Visual Studio KDU build workaround?
  if (NOT VS_DISABLE_FATAL_WARNINGS)
    add_definitions(/WX)
  endif (NOT VS_DISABLE_FATAL_WARNINGS)
endif (WINDOWS)


if (USESYSTEMLIBS)
  add_definitions(-DLL_USESYSTEMLIBS=1)
endif (USESYSTEMLIBS)

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
