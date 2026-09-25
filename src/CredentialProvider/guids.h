#pragma once

#include <guiddef.h>

// {81BD70D2-21D9-40AC-8CE2-51E7FEED8EAF}
//
// Included normally (without <initguid.h> first), this expands to a plain
// `extern const GUID CLSID_JackpotProvider;` declaration -- safe from any
// number of translation units. guids.cpp includes <initguid.h> before this
// header exactly once, which is what actually allocates storage for it.
// Same DEFINE_GUID idiom the Microsoft sample's guid.h/guid.cpp pair uses,
// just as a single reused header instead of a header/source split.
DEFINE_GUID(CLSID_JackpotProvider, 0x81bd70d2, 0x21d9, 0x40ac, 0x8c, 0xe2, 0x51, 0xe7, 0xfe, 0xed, 0x8e, 0xaf);
