#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include <d2d1.h>
#include <wrl/client.h>

namespace waffle::render {

struct ConfettiParticle {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float rotation = 0.0f;
    float angularVelocityDegPerSec = 0.0f;
    float size = 6.0f;
    D2D1_COLOR_F color = {1, 1, 1, 1};
    float lifeMs = 2500.0f;
    float ageMs = 0.0f;
};

// Bounded confetti burst with gravity and rotation (spec §5.3 step 6).
// Particle count is capped so an unlucky run of jackpots, or a slow
// machine, can't make this unbounded.
class ConfettiSystem {
public:
    explicit ConfettiSystem(ID2D1RenderTarget* renderTarget);
    void OnRenderTargetChanged(ID2D1RenderTarget* renderTarget);

    // Spawns a burst across the top of a cabinet-sized area
    // (areaWidth x areaHeight, both in DIPs).
    void Burst(float areaWidth, float areaHeight);

    void Update(std::uint32_t dtMs);
    void Draw();

    void Clear();
    bool IsEmpty() const { return particles_.empty(); }

private:
    static constexpr std::size_t kMaxParticles = 220;
    static constexpr float kGravityDipsPerSec2 = 650.0f;

    std::vector<ConfettiParticle> particles_;
    std::mt19937 rng_{std::random_device{}()};

    ID2D1RenderTarget* renderTarget_ = nullptr;  // not owned
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
};

}  // namespace waffle::render
