add_library(WaffleRender STATIC
    Assets.h
    Assets.cpp
    Animation.h
    Animation.cpp
    Confetti.h
    Confetti.cpp
    AudioManager.h
    AudioManager.cpp
    SlotRenderer.h
    SlotRenderer.cpp
)

target_include_directories(WaffleRender PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(WaffleRender
    PUBLIC
        WaffleCore
    PRIVATE
        d2d1
        dwrite
        xaudio2
)

target_compile_features(WaffleRender PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(WaffleRender PRIVATE /W4)
endif()
