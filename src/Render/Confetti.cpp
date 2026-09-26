#include "Confetti.h"

#include <algorithm>

namespace waffle::render {
namespace {

constexpr D2D1_COLOR_F kConfettiColors[] = {
    {0.95f, 0.75f, 0.10f, 1.0f},  // gold
    {0.85f, 0.15f, 0.20f, 1.0f},  // red
    {0.20f, 0.55f, 0.85f, 1.0f},  // blue
    {0.30f, 0.75f, 0.35f, 1.0f},  // green
    {0.95f, 0.95f, 0.95f, 1.0f},  // white
};

}  // namespace

ConfettiSystem::ConfettiSystem(ID2D1RenderTarget* renderTarget) { OnRenderTargetChanged(renderTarget); }

void ConfettiSystem::OnRenderTargetChanged(ID2D1RenderTarget* renderTarget) {
    renderTarget_ = renderTarget;
    brush_.Reset();
    if (renderTarget_) {
        renderTarget_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_);
    }
}

void ConfettiSystem::Burst(float areaWidth, float areaHeight) {
    std::uniform_real_distribution<float> xDist(0.0f, areaWidth);
    std::uniform_real_distribution<float> vxDist(-60.0f, 60.0f);
    std::uniform_real_distribution<float> vyDist(-260.0f, -120.0f);
    std::uniform_real_distribution<float> spinDist(-260.0f, 260.0f);
    std::uniform_real_distribution<float> sizeDist(4.0f, 9.0f);
    std::uniform_real_distribution<float> lifeDist(2200.0f, 3400.0f);
    std::uniform_int_distribution<std::size_t> colorDist(0, std::size(kConfettiColors) - 1);

    const std::size_t spawnCount =
        std::min<std::size_t>(90, kMaxParticles - std::min(particles_.size(), kMaxParticles));

    for (std::size_t i = 0; i < spawnCount; ++i) {
        ConfettiParticle p;
        p.x = xDist(rng_);
        p.y = areaHeight * 0.05f;
        p.vx = vxDist(rng_);
        p.vy = vyDist(rng_);
        p.rotation = 0.0f;
        p.angularVelocityDegPerSec = spinDist(rng_);
        p.size = sizeDist(rng_);
        p.color = kConfettiColors[colorDist(rng_)];
        p.lifeMs = lifeDist(rng_);
        p.ageMs = 0.0f;
        particles_.push_back(p);
    }
}

void ConfettiSystem::Update(std::uint32_t dtMs) {
    const float dtSec = static_cast<float>(dtMs) / 1000.0f;

    for (auto& p : particles_) {
        p.vy += kGravityDipsPerSec2 * dtSec;
        p.x += p.vx * dtSec;
        p.y += p.vy * dtSec;
        p.rotation += p.angularVelocityDegPerSec * dtSec;
        p.ageMs += static_cast<float>(dtMs);
    }

    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                     [](const ConfettiParticle& p) { return p.ageMs >= p.lifeMs; }),
                      particles_.end());
}

void ConfettiSystem::Draw() {
    if (!renderTarget_ || !brush_ || particles_.empty()) {
        return;
    }

    D2D1_MATRIX_3X2_F savedTransform;
    renderTarget_->GetTransform(&savedTransform);

    for (const auto& p : particles_) {
        const float lifeFraction = p.ageMs / p.lifeMs;
        const float alpha = 1.0f - std::clamp(lifeFraction, 0.0f, 1.0f);
        D2D1_COLOR_F color = p.color;
        color.a = alpha;

        const auto rotation = D2D1::Matrix3x2F::Rotation(p.rotation, D2D1::Point2F(p.x, p.y));
        renderTarget_->SetTransform(rotation * savedTransform);

        const D2D1_RECT_F rect = D2D1::RectF(p.x - p.size * 0.5f, p.y - p.size * 0.25f,
                                              p.x + p.size * 0.5f, p.y + p.size * 0.25f);
        brush_->SetColor(color);
        renderTarget_->FillRectangle(rect, brush_.Get());
    }

    renderTarget_->SetTransform(savedTransform);
}

void ConfettiSystem::Clear() {
    particles_.clear();
}

}  // namespace waffle::render
