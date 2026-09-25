include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# Windows-specific GoogleTest knob; harmless (unused) on other platforms.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(WaffleCoreTests
    JackpotTests.cpp
    ReelTests.cpp
    RngTests.cpp
    SlotMachineTests.cpp
    StateMachineTests.cpp
    ConfigTests.cpp
)

target_include_directories(WaffleCoreTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(WaffleCoreTests PRIVATE WaffleCore gtest_main)
target_compile_features(WaffleCoreTests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(WaffleCoreTests PRIVATE /W4)
else()
    target_compile_options(WaffleCoreTests PRIVATE -Wall -Wextra)
endif()

include(GoogleTest)
gtest_discover_tests(WaffleCoreTests)
