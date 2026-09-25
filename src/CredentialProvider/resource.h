#pragma once

// No bitmap/RCDATA/WAVE resources are embedded here: the tile image is
// drawn procedurally in GDI (JackpotCredential::CreateTileBitmap, spec
// §3's "рисуются процедурно" alternative to shipping asset files), and
// Phase 4 has no other assets yet. This header exists for the version
// resource in resources.rc, and as a place for future IDR_*/IDB_* IDs
// once WaffleRender-shared assets get embedded here (spec §3: "в
// Credential Provider ассеты встраиваются в DLL как ресурсы").

#define VER_FILEVERSION 0, 1, 0, 0
#define VER_FILEVERSION_STR "0.1.0.0"
#define VER_PRODUCTVERSION 0, 1, 0, 0
#define VER_PRODUCTVERSION_STR "0.1.0.0"
