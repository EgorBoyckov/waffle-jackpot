// Including <initguid.h> before guids.h is what turns DEFINE_GUID's
// declaration into a definition -- this must be the only translation unit
// that does so, or every other .cpp including guids.h normally would fail
// to link against duplicate storage.
#include <initguid.h>

#include "guids.h"
