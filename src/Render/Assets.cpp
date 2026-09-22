#include "Assets.h"

#include <algorithm>
#include <cmath>

using Microsoft::WRL::ComPtr;

namespace waffle::render {
namespace {

D2D1_RECT_F Inset(const D2D1_RECT_F& r, float amount) {
    return D2D1::RectF(r.left + amount, r.top + amount, r.right - amount, r.bottom - amount);
}

// Largest centered square that fits inside r, with margin taken as a
// fraction of the smaller side.
D2D1_RECT_F CenteredSquare(const D2D1_RECT_F& r, float marginFraction) {
    const float w = r.right - r.left;
    const float h = r.bottom - r.top;
    const float side = std::min(w, h) * (1.0f - marginFraction);
    const float cx = (r.left + r.right) * 0.5f;
    const float cy = (r.top + r.bottom) * 0.5f;
    return D2D1::RectF(cx - side * 0.5f, cy - side * 0.5f, cx + side * 0.5f, cy + side * 0.5f);
}

D2D1_COLOR_F Lerp(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b, float t) {
    return D2D1::ColorF(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                         a.a + (b.a - a.a) * t);
}

}  // namespace

SymbolPainter::SymbolPainter(ID2D1RenderTarget* renderTarget) { OnRenderTargetChanged(renderTarget); }

void SymbolPainter::OnRenderTargetChanged(ID2D1RenderTarget* renderTarget) {
    renderTarget_ = renderTarget;
    brush_.Reset();
    if (renderTarget_) {
        renderTarget_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_);
    }
}

ID2D1SolidColorBrush* SymbolPainter::Brush(const D2D1_COLOR_F& color) {
    brush_->SetColor(color);
    return brush_.Get();
}

void SymbolPainter::DrawSymbol(waffle::Symbol symbol, const D2D1_RECT_F& cellRect, float glow) {
    if (!renderTarget_ || !brush_) {
        return;
    }
    const D2D1_RECT_F square = CenteredSquare(cellRect, 0.18f);
    switch (symbol) {
        case waffle::Symbol::Waffle:  DrawWaffle(square, glow); return;
        case waffle::Symbol::Cherry:  DrawCherry(square); return;
        case waffle::Symbol::Lemon:   DrawLemon(square); return;
        case waffle::Symbol::Bell:    DrawBell(square); return;
        case waffle::Symbol::Diamond: DrawDiamond(square); return;
        case waffle::Symbol::Star:    DrawStar(square); return;
        case waffle::Symbol::Seven:   DrawSeven(square); return;
        case waffle::Symbol::Lucky:   DrawLucky(square); return;
        case waffle::Symbol::Count:   return;
    }
}

void SymbolPainter::DrawWaffle(const D2D1_RECT_F& r, float glow) {
    glow = std::clamp(glow, 0.0f, 1.0f);

    // Glow: a couple of oversized, increasingly transparent rounded rects
    // behind the waffle. Cheap stand-in for a real Gaussian blur effect —
    // good enough for a plate-sized UI element at 60 FPS, and avoids
    // pulling in ID2D1Effect / D2D1_FACTORY_TYPE_MULTI_THREADED plumbing
    // for a joke project's glow.
    if (glow > 0.0f) {
        for (int i = 3; i >= 1; --i) {
            const float pad = i * 6.0f * glow;
            const float alpha = 0.10f * glow / static_cast<float>(i);
            const D2D1_RECT_F glowRect = Inset(r, -pad);
            const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(glowRect, 10.0f + pad, 10.0f + pad);
            renderTarget_->FillRoundedRectangle(rr, Brush(D2D1::ColorF(palette::kGoldBright.r,
                                                                        palette::kGoldBright.g,
                                                                        palette::kGoldBright.b, alpha)));
        }
    }

    // Body: golden square, browned edge, waffle grid.
    const D2D1_COLOR_F body = Lerp(palette::kGold, palette::kGoldBright, 0.4f + 0.6f * glow);
    const D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(r, 6.0f, 6.0f);
    renderTarget_->FillRoundedRectangle(rr, Brush(body));

    const D2D1_COLOR_F browned = D2D1::ColorF(0.45f, 0.28f, 0.08f, 1.0f);
    renderTarget_->DrawRoundedRectangle(rr, Brush(browned), 3.0f);

    // Grid: 3x3 pockets.
    const float w = r.right - r.left;
    const float h = r.bottom - r.top;
    for (int i = 1; i < 4; ++i) {
        const float x = r.left + w * i / 4.0f;
        renderTarget_->DrawLine(D2D1::Point2F(x, r.top + 4), D2D1::Point2F(x, r.bottom - 4),
                                 Brush(browned), 2.0f);
        const float y = r.top + h * i / 4.0f;
        renderTarget_->DrawLine(D2D1::Point2F(r.left + 4, y), D2D1::Point2F(r.right - 4, y),
                                 Brush(browned), 2.0f);
    }

    // Highlight glint, top-left.
    const D2D1_RECT_F glint = D2D1::RectF(r.left + w * 0.12f, r.top + h * 0.12f,
                                           r.left + w * 0.35f, r.top + h * 0.22f);
    renderTarget_->FillRoundedRectangle(D2D1::RoundedRect(glint, 4.0f, 4.0f),
                                         Brush(D2D1::ColorF(1.0f, 1.0f, 0.9f, 0.5f)));

    // Syrup drip, bottom-right corner.
    const float dripX = r.right - w * 0.18f;
    const float dripY = r.bottom - h * 0.08f;
    const D2D1_ELLIPSE drip = D2D1::Ellipse(D2D1::Point2F(dripX, dripY), w * 0.05f, h * 0.07f);
    renderTarget_->FillEllipse(drip, Brush(D2D1::ColorF(0.55f, 0.30f, 0.05f, 0.85f)));
}

void SymbolPainter::DrawCherry(const D2D1_RECT_F& r) {
    const float w = r.right - r.left;
    const float h = r.bottom - r.top;
    const D2D1_COLOR_F red = D2D1::ColorF(0.75f, 0.05f, 0.12f, 1.0f);
    const D2D1_COLOR_F stem = D2D1::ColorF(0.25f, 0.45f, 0.15f, 1.0f);

    const D2D1_POINT_2F stemTop = D2D1::Point2F((r.left + r.right) * 0.5f, r.top);
    const D2D1_POINT_2F left = D2D1::Point2F(r.left + w * 0.35f, r.top + h * 0.55f);
    const D2D1_POINT_2F right = D2D1::Point2F(r.right - w * 0.35f, r.top + h * 0.55f);
    renderTarget_->DrawLine(stemTop, left, Brush(stem), 2.5f);
    renderTarget_->DrawLine(stemTop, right, Brush(stem), 2.5f);

    const float rad = std::min(w, h) * 0.24f;
    renderTarget_->FillEllipse(D2D1::Ellipse(left, rad, rad), Brush(red));
    renderTarget_->FillEllipse(D2D1::Ellipse(right, rad, rad), Brush(red));
}

void SymbolPainter::DrawLemon(const D2D1_RECT_F& r) {
    const D2D1_COLOR_F yellow = D2D1::ColorF(0.95f, 0.85f, 0.15f, 1.0f);
    const D2D1_POINT_2F center = D2D1::Point2F((r.left + r.right) * 0.5f, (r.top + r.bottom) * 0.5f);
    const float rx = (r.right - r.left) * 0.42f;
    const float ry = (r.bottom - r.top) * 0.32f;
    renderTarget_->FillEllipse(D2D1::Ellipse(center, rx, ry), Brush(yellow));
    renderTarget_->DrawEllipse(D2D1::Ellipse(center, rx, ry), Brush(D2D1::ColorF(0.6f, 0.5f, 0.05f, 1.0f)), 2.0f);
}

void SymbolPainter::DrawBell(const D2D1_RECT_F& r) {
    const float w = r.right - r.left;
    const float h = r.bottom - r.top;
    const D2D1_COLOR_F gold = D2D1::ColorF(0.85f, 0.68f, 0.15f, 1.0f);

    ComPtr<ID2D1Factory> factory;
    renderTarget_->GetFactory(&factory);
    ComPtr<ID2D1PathGeometry> geometry;
    factory->CreatePathGeometry(&geometry);
    ComPtr<ID2D1GeometrySink> sink;
    geometry->Open(&sink);

    sink->BeginFigure(D2D1::Point2F(r.left + w * 0.5f, r.top + h * 0.05f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(r.left + w * 0.05f, r.top + h * 0.15f),
        D2D1::Point2F(r.left + w * 0.10f, r.top + h * 0.75f),
        D2D1::Point2F(r.left + w * 0.02f, r.top + h * 0.80f)));
    sink->AddLine(D2D1::Point2F(r.right - w * 0.02f, r.top + h * 0.80f));
    sink->AddBezier(D2D1::BezierSegment(
        D2D1::Point2F(r.right - w * 0.10f, r.top + h * 0.75f),
        D2D1::Point2F(r.right - w * 0.05f, r.top + h * 0.15f),
        D2D1::Point2F(r.left + w * 0.5f, r.top + h * 0.05f)));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    renderTarget_->FillGeometry(geometry.Get(), Brush(gold));

    const D2D1_ELLIPSE clapper = D2D1::Ellipse(D2D1::Point2F(r.left + w * 0.5f, r.top + h * 0.88f), w * 0.06f, w * 0.06f);
    renderTarget_->FillEllipse(clapper, Brush(D2D1::ColorF(0.4f, 0.3f, 0.05f, 1.0f)));
}

void SymbolPainter::DrawDiamond(const D2D1_RECT_F& r) {
    const float w = r.right - r.left;
    const float h = r.bottom - r.top;
    const D2D1_COLOR_F cyan = D2D1::ColorF(0.55f, 0.90f, 0.95f, 1.0f);

    ComPtr<ID2D1Factory> factory;
    renderTarget_->GetFactory(&factory);
    ComPtr<ID2D1PathGeometry> geometry;
    factory->CreatePathGeometry(&geometry);
    ComPtr<ID2D1GeometrySink> sink;
    geometry->Open(&sink);

    sink->BeginFigure(D2D1::Point2F(r.left + w * 0.5f, r.top), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(r.right, r.top + h * 0.5f));
    sink->AddLine(D2D1::Point2F(r.left + w * 0.5f, r.bottom));
    sink->AddLine(D2D1::Point2F(r.left, r.top + h * 0.5f));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    renderTarget_->FillGeometry(geometry.Get(), Brush(cyan));
    renderTarget_->DrawGeometry(geometry.Get(), Brush(D2D1::ColorF(0.2f, 0.4f, 0.5f, 1.0f)), 2.0f);
}

void SymbolPainter::DrawStar(const D2D1_RECT_F& r) {
    const float w = r.right - r.left;
    const float h = r.bottom - r.top;
    const D2D1_POINT_2F center = D2D1::Point2F((r.left + r.right) * 0.5f, (r.top + r.bottom) * 0.5f);
    const float outerR = std::min(w, h) * 0.5f;
    const float innerR = outerR * 0.42f;

    ComPtr<ID2D1Factory> factory;
    renderTarget_->GetFactory(&factory);
    ComPtr<ID2D1PathGeometry> geometry;
    factory->CreatePathGeometry(&geometry);
    ComPtr<ID2D1GeometrySink> sink;
    geometry->Open(&sink);

    constexpr int kPoints = 5;
    constexpr double kPi = 3.14159265358979323846;
    auto pointAt = [&](int i, float radius) {
        const double angle = -kPi / 2.0 + i * kPi / kPoints;
        return D2D1::Point2F(center.x + radius * static_cast<float>(std::cos(angle)),
                              center.y + radius * static_cast<float>(std::sin(angle)));
    };
    sink->BeginFigure(pointAt(0, outerR), D2D1_FIGURE_BEGIN_FILLED);
    for (int i = 1; i < kPoints * 2; ++i) {
        sink->AddLine(pointAt(i, (i % 2 == 0) ? outerR : innerR));
    }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();

    renderTarget_->FillGeometry(geometry.Get(), Brush(D2D1::ColorF(1.0f, 0.85f, 0.2f, 1.0f)));
}

void SymbolPainter::DrawSeven(const D2D1_RECT_F& r) {
    const float w = r.right - r.left;
    const float h = r.bottom - r.top;
    const D2D1_COLOR_F red = D2D1::ColorF(0.80f, 0.10f, 0.10f, 1.0f);
    renderTarget_->DrawLine(D2D1::Point2F(r.left + w * 0.15f, r.top + h * 0.1f),
                             D2D1::Point2F(r.right - w * 0.1f, r.top + h * 0.1f), Brush(red), h * 0.14f);
    renderTarget_->DrawLine(D2D1::Point2F(r.right - w * 0.1f, r.top + h * 0.1f),
                             D2D1::Point2F(r.left + w * 0.3f, r.bottom - h * 0.05f), Brush(red), h * 0.14f);
}

void SymbolPainter::DrawLucky(const D2D1_RECT_F& r) {
    // A simple four-leaf clover approximation: four overlapping circles
    // plus a stem.
    const float w = r.right - r.left;
    const float h = r.bottom - r.top;
    const D2D1_COLOR_F green = D2D1::ColorF(0.15f, 0.55f, 0.20f, 1.0f);
    const D2D1_POINT_2F center = D2D1::Point2F((r.left + r.right) * 0.5f, r.top + h * 0.42f);
    const float leafR = std::min(w, h) * 0.20f;
    const float offset = leafR * 0.9f;

    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - offset, center.y - offset), leafR, leafR), Brush(green));
    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + offset, center.y - offset), leafR, leafR), Brush(green));
    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x - offset, center.y + offset), leafR, leafR), Brush(green));
    renderTarget_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x + offset, center.y + offset), leafR, leafR), Brush(green));
    renderTarget_->DrawLine(center, D2D1::Point2F(center.x, r.bottom - h * 0.05f),
                             Brush(D2D1::ColorF(0.10f, 0.35f, 0.10f, 1.0f)), 3.0f);
}

}  // namespace waffle::render
