add_executable(WaffleJackpotControl WIN32
    ControlMain.cpp
    Registration.h
    Registration.cpp
    WaffleJackpotControl.manifest
)

target_link_libraries(WaffleJackpotControl
    PRIVATE
        advapi32
        shell32
        shlwapi
)

target_compile_features(WaffleJackpotControl PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(WaffleJackpotControl PRIVATE /W4)
endif()
