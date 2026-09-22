#pragma once

#include <d2d1.h>
#include <wrl/client.h>

#include "Core/Symbol.h"

namespace waffle::render {

// Procedurally-drawn symbol art. Project spec §3: no bitmaps, no fonts
// (Segoe UI Emoji renders inconsistently across Windows versions), nothing
// fetched from the network at build or run time — every symbol is a
// handful of Direct2D primitives composed at draw time.
//
// Not thread-safe; only ever called from the render thread that owns the
// D2D render target.
class SymbolPainter {
public:
    explicit SymbolPainter(ID2D1RenderTarget* renderTarget);

    // Re-binds to a new render target after a device-loss recovery
    // (D2DERR_RECREATE_TARGET). The old target's brushes are gone with it.
    void OnRenderTargetChanged(ID2D1RenderTarget* renderTarget);

    // Draws `symbol` centered in `cellRect`, square-fitted with margin.
    // `glow` in [0,1] drives the jackpot shine used during
    // JackpotSequence; 0 is the normal look.
    void DrawSymbol(waffle::Symbol symbol, const D2D1_RECT_F& cellRect, float glow = 0.0f);

private:
    ID2D1SolidColorBrush* Brush(const D2D1_COLOR_F& color);

    void DrawWaffle(const D2D1_RECT_F& r, float glow);
    void DrawCherry(const D2D1_RECT_F& r);
    void DrawLemon(const D2D1_RECT_F& r);
    void DrawBell(const D2D1_RECT_F& r);
    void DrawDiamond(const D2D1_RECT_F& r);
    void DrawStar(const D2D1_RECT_F& r);
    void DrawSeven(const D2D1_RECT_F& r);
    void DrawLucky(const D2D1_RECT_F& r);

    ID2D1RenderTarget* renderTarget_ = nullptr;  // not owned
    // Single reusable solid-color brush, recolored per draw call. Cheaper
    // than a brush-per-color cache for a scene this small, and there's
    // never more than one draw in flight on this thread.
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
};

// Cabinet chrome colors, shared by SlotRenderer for the background, lamp
// ring and button so the palette lives in one place.
namespace palette {
constexpr D2D1_COLOR_F kCabinetDark = {0.06f, 0.05f, 0.08f, 1.0f};
constexpr D2D1_COLOR_F kCabinetPanel = {0.13f, 0.10f, 0.16f, 1.0f};
constexpr D2D1_COLOR_F kGold = {0.85f, 0.65f, 0.13f, 1.0f};
constexpr D2D1_COLOR_F kGoldBright = {1.0f, 0.84f, 0.30f, 1.0f};
constexpr D2D1_COLOR_F kLampRed = {0.90f, 0.15f, 0.15f, 1.0f};
constexpr D2D1_COLOR_F kLampYellow = {1.0f, 0.85f, 0.20f, 1.0f};
constexpr D2D1_COLOR_F kLampOff = {0.25f, 0.20f, 0.10f, 1.0f};
constexpr D2D1_COLOR_F kTextLight = {0.96f, 0.94f, 0.90f, 1.0f};
constexpr D2D1_COLOR_F kTextDim = {0.60f, 0.56f, 0.55f, 1.0f};
}  // namespace palette

}  // namespace waffle::render
