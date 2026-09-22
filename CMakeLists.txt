cmake_minimum_required(VERSION 3.20)
project(WaffleJackpotLogin CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src/Core)

# Render/Demo/CredentialProvider/Control all depend on Win32 (Direct2D,
# COM, LogonUI) and are only buildable on Windows with the Windows SDK.
# WaffleCore has no such dependency by design (project spec §4.1) and is
# what this repository's CI builds and tests on Linux.
if(WIN32)
    add_subdirectory(src/Render)
    add_subdirectory(src/Demo)
    # add_subdirectory(src/CredentialProvider)
    # add_subdirectory(src/Control)
endif()

# Unit tests (GoogleTest) land in Phase 3; tests/CMakeLists.txt doesn't
# exist yet, so it isn't wired in here until then.
