add_executable(WaffleJackpotDemo WIN32
    DemoMain.cpp
    DemoWindow.h
    DemoWindow.cpp
    WaffleJackpotDemo.manifest
)

target_link_libraries(WaffleJackpotDemo
    PRIVATE
        WaffleCore
        WaffleRender
        comctl32
)

target_compile_features(WaffleJackpotDemo PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(WaffleJackpotDemo PRIVATE /W4)
endif()

# config.default.json is resolved next to the .exe at runtime
# (DemoWindow::ResolveConfigPath), so keep a copy there after each build.
add_custom_command(TARGET WaffleJackpotDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:WaffleJackpotDemo>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config/config.default.json"
        "$<TARGET_FILE_DIR:WaffleJackpotDemo>/config/config.default.json"
)
