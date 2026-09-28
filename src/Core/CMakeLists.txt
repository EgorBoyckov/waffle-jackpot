add_library(WaffleCore STATIC
    Symbol.h
    Symbol.cpp
    Rng.h
    Rng.cpp
    Config.h
    Config.cpp
    Reel.h
    Reel.cpp
    SlotMachine.h
    SlotMachine.cpp
    JackpotState.h
    JackpotState.cpp
)

target_include_directories(WaffleCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party
)

target_compile_features(WaffleCore PUBLIC cxx_std_20)

if(WIN32)
    target_link_libraries(WaffleCore PRIVATE bcrypt)
endif()

if(MSVC)
    target_compile_options(WaffleCore PRIVATE /W4)
else()
    target_compile_options(WaffleCore PRIVATE -Wall -Wextra)
endif()
