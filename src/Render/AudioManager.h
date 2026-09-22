#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <vector>

#include <wrl/client.h>
#include <xaudio2.h>

namespace waffle::render {

// Every sound effect this project uses, all synthesized procedurally at
// startup (see AudioManager::SynthesizeClips) rather than loaded from WAV
// files or fetched from anywhere — project spec §3: "никаких изображений и
// звуков из интернета, ни при сборке «на лету», ни в рантайме". This is
// the audio equivalent of Assets.h's procedural Direct2D symbols: it
// trades the spec's suggested "numpy/wave at build time" route for
// "synthesize once, in C++, at process start" — same end result (no
// external asset, testable, license-free), one fewer moving part.
enum class Clip {
    Lever,
    Click,
    Clack,
    ReelTick,
    Loss,
    NearMiss,
    Jackpot,
    Count,
};

// Thin XAudio2 wrapper. Every public method is safe to call even if
// construction failed to initialize audio (IsAvailable() == false) — per
// spec §9.1, "без звука — работаем молча", never a crash or a blocking
// wait on the LogonUI thread. Voice control calls in XAudio2 are posted to
// the engine's own thread and return immediately, so Play() never blocks.
class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool IsAvailable() const { return available_; }

    void SetVolume(float volume01);
    void SetMuted(bool muted);

    // Non-blocking. Grabs the next voice from a small round-robin pool,
    // stops and refills it, and starts playback. If audio isn't
    // available, or `clip` somehow doesn't resolve, this is a no-op.
    void Play(Clip clip);

private:
    struct ClipData {
        std::vector<std::int16_t> samples;
        WAVEFORMATEX format{};
    };

    void SynthesizeClips();
    ClipData PackClip(const std::vector<float>& floatSamples) const;
    ClipData SynthesizeLever();
    ClipData SynthesizeClick();
    ClipData SynthesizeClack();
    ClipData SynthesizeLoss();
    ClipData SynthesizeNearMiss();
    ClipData SynthesizeJackpot();

    Microsoft::WRL::ComPtr<IXAudio2> engine_;
    IXAudio2MasteringVoice* masteringVoice_ = nullptr;

    static constexpr std::size_t kVoicePoolSize = 6;
    std::array<IXAudio2SourceVoice*, kVoicePoolSize> voicePool_{};
    std::size_t nextVoice_ = 0;

    std::array<ClipData, static_cast<std::size_t>(Clip::Count)> clips_;
    std::mt19937 rng_{std::random_device{}()};

    float volume_ = 1.0f;
    bool muted_ = false;
    bool available_ = false;
};

}  // namespace waffle::render
