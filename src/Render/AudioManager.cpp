#include "AudioManager.h"

#include <algorithm>
#include <cmath>

namespace waffle::render {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr std::uint32_t kSampleRate = 44100;

std::vector<float> MakeSilence(double seconds) {
    return std::vector<float>(static_cast<std::size_t>(seconds * kSampleRate), 0.0f);
}

void EnsureLength(std::vector<float>& buf, std::size_t end) {
    if (buf.size() < end) {
        buf.resize(end, 0.0f);
    }
}

void AddSine(std::vector<float>& buf, double startSec, double freqHz, double durationSec, double amplitude) {
    const auto start = static_cast<std::size_t>(startSec * kSampleRate);
    const auto count = static_cast<std::size_t>(durationSec * kSampleRate);
    EnsureLength(buf, start + count);
    for (std::size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        buf[start + i] += static_cast<float>(amplitude * std::sin(2.0 * kPi * freqHz * t));
    }
}

// Linear frequency sweep, e.g. the descending "deflating trombone" loss
// sound. Integrates frequency into phase so it stays continuous.
void AddSweep(std::vector<float>& buf, double startSec, double freqStart, double freqEnd,
              double durationSec, double amplitude) {
    const auto start = static_cast<std::size_t>(startSec * kSampleRate);
    const auto count = static_cast<std::size_t>(durationSec * kSampleRate);
    EnsureLength(buf, start + count);
    double phase = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        const double frac = durationSec > 0.0 ? t / durationSec : 0.0;
        const double freq = freqStart + (freqEnd - freqStart) * frac;
        phase += 2.0 * kPi * freq / kSampleRate;
        buf[start + i] += static_cast<float>(amplitude * std::sin(phase));
    }
}

// Low-pass-filtered white noise, used for lever/clack thumps and the
// jackpot's coin-waterfall texture.
void AddNoiseBurst(std::vector<float>& buf, std::mt19937& rng, double startSec, double durationSec,
                    double amplitude) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    const auto start = static_cast<std::size_t>(startSec * kSampleRate);
    const auto count = static_cast<std::size_t>(durationSec * kSampleRate);
    EnsureLength(buf, start + count);
    float lowPassState = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        lowPassState += 0.25f * (dist(rng) - lowPassState);
        buf[start + i] += static_cast<float>(amplitude) * lowPassState;
    }
}

void ApplyEnvelope(std::vector<float>& buf, double startSec, double durationSec, double attackSec,
                    double releaseSec) {
    const auto start = static_cast<std::size_t>(startSec * kSampleRate);
    const auto count = static_cast<std::size_t>(durationSec * kSampleRate);
    const auto attackSamples = static_cast<std::size_t>(attackSec * kSampleRate);
    const auto releaseSamples = static_cast<std::size_t>(releaseSec * kSampleRate);
    for (std::size_t i = 0; i < count && start + i < buf.size(); ++i) {
        float env = 1.0f;
        if (attackSamples > 0 && i < attackSamples) {
            env = static_cast<float>(i) / static_cast<float>(attackSamples);
        } else if (releaseSamples > 0 && count > i && count - i <= releaseSamples) {
            env = static_cast<float>(count - i) / static_cast<float>(releaseSamples);
        }
        buf[start + i] *= env;
    }
}

// Scales to targetPeak and never clips — spec §5.4: "громко и нелепо, но с
// нормализацией (без клиппинга)".
std::vector<std::int16_t> NormalizeToInt16(const std::vector<float>& buf, float targetPeak = 0.9f) {
    float peak = 0.0f;
    for (float sample : buf) {
        peak = std::max(peak, std::fabs(sample));
    }
    const float scale = peak > 1e-6f ? (targetPeak / peak) : 1.0f;

    std::vector<std::int16_t> out(buf.size());
    for (std::size_t i = 0; i < buf.size(); ++i) {
        const float scaled = std::clamp(buf[i] * scale, -1.0f, 1.0f);
        out[i] = static_cast<std::int16_t>(scaled * 32767.0f);
    }
    return out;
}

}  // namespace

AudioManager::AudioManager() {
    // Constructor assumes COM is already initialized on this thread by the
    // caller (Demo's WinMain, or the Credential Provider's DLL host) — an
    // audio helper has no business calling CoInitializeEx/CoUninitialize
    // itself and risking a mismatched apartment with whatever else the
    // host does on this thread.
    if (FAILED(XAudio2Create(&engine_, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
        return;
    }
    if (FAILED(engine_->CreateMasteringVoice(&masteringVoice_))) {
        engine_.Reset();
        return;
    }

    SynthesizeClips();

    const WAVEFORMATEX& format = clips_[0].format;
    bool poolOk = true;
    for (auto& voice : voicePool_) {
        if (FAILED(engine_->CreateSourceVoice(&voice, &format))) {
            poolOk = false;
            break;
        }
    }
    if (!poolOk) {
        for (auto& voice : voicePool_) {
            if (voice) {
                voice->DestroyVoice();
                voice = nullptr;
            }
        }
        masteringVoice_->DestroyVoice();
        masteringVoice_ = nullptr;
        engine_.Reset();
        return;
    }

    available_ = true;
}

AudioManager::~AudioManager() {
    for (auto& voice : voicePool_) {
        if (voice) {
            voice->Stop(0);
            voice->DestroyVoice();
        }
    }
    if (masteringVoice_) {
        masteringVoice_->DestroyVoice();
    }
    // engine_ (a ComPtr) releases on scope exit, stopping the engine.
}

void AudioManager::SetVolume(float volume01) {
    volume_ = std::clamp(volume01, 0.0f, 1.0f);
    if (masteringVoice_) {
        masteringVoice_->SetVolume(muted_ ? 0.0f : volume_);
    }
}

void AudioManager::SetMuted(bool muted) {
    muted_ = muted;
    if (masteringVoice_) {
        masteringVoice_->SetVolume(muted_ ? 0.0f : volume_);
    }
}

void AudioManager::Play(Clip clip) {
    if (!available_) {
        return;
    }
    const auto index = static_cast<std::size_t>(clip);
    if (index >= clips_.size()) {
        return;
    }

    IXAudio2SourceVoice* voice = voicePool_[nextVoice_];
    nextVoice_ = (nextVoice_ + 1) % kVoicePoolSize;
    if (!voice) {
        return;
    }

    const ClipData& data = clips_[index];
    voice->Stop(0);
    voice->FlushSourceBuffers();

    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes = static_cast<UINT32>(data.samples.size() * sizeof(std::int16_t));
    buffer.pAudioData = reinterpret_cast<const BYTE*>(data.samples.data());
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    voice->SubmitSourceBuffer(&buffer);
    voice->Start(0);
}

AudioManager::ClipData AudioManager::PackClip(const std::vector<float>& floatSamples) const {
    ClipData clip;
    clip.samples = NormalizeToInt16(floatSamples);
    clip.format.wFormatTag = WAVE_FORMAT_PCM;
    clip.format.nChannels = 1;
    clip.format.nSamplesPerSec = kSampleRate;
    clip.format.wBitsPerSample = 16;
    clip.format.nBlockAlign =
        static_cast<WORD>(clip.format.nChannels * clip.format.wBitsPerSample / 8);
    clip.format.nAvgBytesPerSec = clip.format.nSamplesPerSec * clip.format.nBlockAlign;
    clip.format.cbSize = 0;
    return clip;
}

void AudioManager::SynthesizeClips() {
    clips_[static_cast<std::size_t>(Clip::Lever)] = SynthesizeLever();
    clips_[static_cast<std::size_t>(Clip::Click)] = SynthesizeClick();
    clips_[static_cast<std::size_t>(Clip::Clack)] = SynthesizeClack();
    // ReelTick reuses Click's data; it's the same "tick" sound the reel
    // makes when it stops (spec §5.4 lists them together).
    clips_[static_cast<std::size_t>(Clip::ReelTick)] = clips_[static_cast<std::size_t>(Clip::Click)];
    clips_[static_cast<std::size_t>(Clip::Loss)] = SynthesizeLoss();
    clips_[static_cast<std::size_t>(Clip::NearMiss)] = SynthesizeNearMiss();
    clips_[static_cast<std::size_t>(Clip::Jackpot)] = SynthesizeJackpot();
}

AudioManager::ClipData AudioManager::SynthesizeLever() {
    std::vector<float> buf = MakeSilence(0.15);
    AddSine(buf, 0.0, 90.0, 0.15, 0.6);
    AddNoiseBurst(buf, rng_, 0.0, 0.05, 0.3);
    ApplyEnvelope(buf, 0.0, 0.15, 0.005, 0.10);
    return PackClip(buf);
}

AudioManager::ClipData AudioManager::SynthesizeClick() {
    std::vector<float> buf = MakeSilence(0.03);
    AddSine(buf, 0.0, 1800.0, 0.03, 0.5);
    ApplyEnvelope(buf, 0.0, 0.03, 0.002, 0.02);
    return PackClip(buf);
}

AudioManager::ClipData AudioManager::SynthesizeClack() {
    std::vector<float> buf = MakeSilence(0.05);
    AddSine(buf, 0.0, 900.0, 0.05, 0.55);
    AddNoiseBurst(buf, rng_, 0.0, 0.02, 0.2);
    ApplyEnvelope(buf, 0.0, 0.05, 0.002, 0.035);
    return PackClip(buf);
}

AudioManager::ClipData AudioManager::SynthesizeLoss() {
    // A short comedic descending sweep -- the "deflating trombone" from
    // spec §5.4, kept under 1 second.
    constexpr double kDuration = 0.9;
    std::vector<float> buf = MakeSilence(kDuration);
    AddSweep(buf, 0.0, 300.0, 70.0, kDuration, 0.5);
    ApplyEnvelope(buf, 0.0, kDuration, 0.02, 0.4);
    return PackClip(buf);
}

AudioManager::ClipData AudioManager::SynthesizeNearMiss() {
    constexpr double kDuration = 0.35;
    std::vector<float> buf = MakeSilence(kDuration);
    AddSweep(buf, 0.0, 500.0, 260.0, kDuration, 0.55);
    ApplyEnvelope(buf, 0.0, kDuration, 0.01, 0.2);
    return PackClip(buf);
}

AudioManager::ClipData AudioManager::SynthesizeJackpot() {
    constexpr double kDuration = 4.0;
    std::vector<float> buf = MakeSilence(kDuration);

    // Ascending fanfare arpeggio (C5 E5 G5 C6), twice, each note with a
    // faint octave shimmer.
    constexpr double kNotes[] = {523.25, 659.25, 783.99, 1046.50};
    double t = 0.0;
    for (int rep = 0; rep < 2; ++rep) {
        for (double freq : kNotes) {
            AddSine(buf, t, freq, 0.18, 0.35);
            AddSine(buf, t, freq * 2.0, 0.18, 0.12);
            ApplyEnvelope(buf, t, 0.18, 0.005, 0.08);
            t += 0.15;
        }
    }

    // Sustained triumphant chord under the tail.
    const double tailDuration = kDuration - t;
    AddSine(buf, t, 523.25, tailDuration, 0.25);
    AddSine(buf, t, 659.25, tailDuration, 0.20);
    AddSine(buf, t, 783.99, tailDuration, 0.20);
    ApplyEnvelope(buf, t, tailDuration, 0.02, 0.6);

    // Coin waterfall: scattered short high clicks over the tail.
    std::uniform_real_distribution<double> clickTimeDist(t, kDuration - 0.05);
    std::uniform_real_distribution<double> clickFreqDist(1800.0, 3200.0);
    for (int i = 0; i < 40; ++i) {
        const double clickStart = clickTimeDist(rng_);
        AddSine(buf, clickStart, clickFreqDist(rng_), 0.03, 0.15);
        ApplyEnvelope(buf, clickStart, 0.03, 0.001, 0.02);
    }

    return PackClip(buf);
}

}  // namespace waffle::render
