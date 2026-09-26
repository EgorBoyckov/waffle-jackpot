add_library(WaffleJackpotProvider SHARED
    guids.h
    guids.cpp
    helpers.h
    helpers.cpp
    ClassFactory.h
    ClassFactory.cpp
    dllmain.h
    dllmain.cpp
    FieldDescriptors.h
    JackpotCredential.h
    JackpotCredential.cpp
    JackpotProvider.h
    JackpotProvider.cpp
    KillSwitch.h
    KillSwitch.cpp
    SlotDialog.h
    SlotDialog.cpp
    resource.h
    resources.rc
    CredentialProvider.def
)

target_include_directories(WaffleJackpotProvider PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(WaffleJackpotProvider
    PRIVATE
        WaffleCore
        WaffleRender
        advapi32
        credui
        shlwapi
        propsys
        secur32
)

target_compile_features(WaffleJackpotProvider PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(WaffleJackpotProvider PRIVATE /W4)
endif()
