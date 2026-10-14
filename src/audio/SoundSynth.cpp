#include "audio/SoundSynth.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace {
    constexpr int SAMPLE_RATE = 44100;
    constexpr float TWO_PI = 6.28318530718f;

    inline float RandFloatSym() {
        return (static_cast<float>(std::rand() % 20000) / 10000.0f) - 1.0f;
    }

    inline short FloatToPcm(float s) {
        float clamped = std::clamp(s, -1.0f, 1.0f);
        return static_cast<short>(clamped * 32000.0f);
    }
}

SoundSynth::~SoundSynth() {
    Shutdown();
}

void SoundSynth::Init() {
    if (initialized_) return;

    InitAudioDevice();
    SetMasterVolume(0.75f);

    for (size_t i = 0; i < static_cast<size_t>(SfxId::COUNT); ++i) {
        sfxBank_[i] = GenerateSfxWave(static_cast<SfxId>(i));
    }

    // Генерація характерного звуку для риси IBS
    {
        float duration = 0.32f;
        int sampleCount = static_cast<int>(SAMPLE_RATE * duration);
        Wave wave{};
        wave.frameCount = static_cast<unsigned int>(sampleCount);
        wave.sampleRate = SAMPLE_RATE;
        wave.sampleSize = 16;
        wave.channels = 1;
        short* pcm = static_cast<short*>(RL_MALLOC(sizeof(short) * sampleCount));

        float phase = 0.0f;
        for (int i = 0; i < sampleCount; ++i) {
            float t = static_cast<float>(i) / SAMPLE_RATE;
            float env = std::exp(-t * 8.5f) * std::sin(t * 3.14159f / duration);
            float flutter = 68.0f + t * 25.0f + 18.0f * std::sin(TWO_PI * 28.0f * t);
            phase += TWO_PI * flutter / SAMPLE_RATE;
            float saw = 2.0f * (phase / TWO_PI - std::floor(0.5f + phase / TWO_PI));
            float noise = RandFloatSym() * 0.35f;
            pcm[i] = FloatToPcm((saw * 0.65f + noise) * env * 0.65f);
        }
        wave.data = pcm;
        ibsFartSound_ = LoadSoundFromWave(wave);
        UnloadWave(wave);
    }

    // Частоти Ре гармонічного мінору (D Harmonic Minor)
    const std::array<float, 8> bassFreqs = {
        73.42f, 82.41f, 87.31f, 98.00f, 110.00f, 116.54f, 138.59f, 146.83f
    };
    for (size_t i = 0; i < bassFreqs.size(); ++i) {
        bassNotes_[i] = GenerateNoteWave(bassFreqs[i], 0.24f, 1, 0.26f);
    }

    const std::array<float, 16> leadFreqs = {
        293.66f, 329.63f, 349.23f, 392.00f, 440.00f, 466.16f, 554.37f, 587.33f,
        659.25f, 698.46f, 783.99f, 880.00f, 932.33f, 1108.73f, 1174.66f, 1396.91f
    };
    for (size_t i = 0; i < leadFreqs.size(); ++i) {
        leadNotes_[i] = GenerateNoteWave(leadFreqs[i], 0.16f, 0, 0.18f);
    }

    percKick_ = GenerateNoteWave(95.0f, 0.10f, 2, 0.30f);
    percHat_  = GenerateNoteWave(4000.0f, 0.05f, 3, 0.12f);

    initialized_ = true;
}

void SoundSynth::Shutdown() {
    if (!initialized_) return;

    for (Sound& s : sfxBank_) {
        UnloadSound(s);
    }
    UnloadSound(ibsFartSound_);
    for (Sound& s : leadNotes_) {
        UnloadSound(s);
    }
    for (Sound& s : bassNotes_) {
        UnloadSound(s);
    }
    UnloadSound(percKick_);
    UnloadSound(percHat_);

    CloseAudioDevice();
    initialized_ = false;
}

void SoundSynth::PlaySfx(SfxId id, float pitch, float volume) {
    if (!initialized_ || sfxMuted_) return;
    int idx = std::clamp(static_cast<int>(id), 0, static_cast<int>(SfxId::COUNT) - 1);
    Sound& snd = sfxBank_[static_cast<size_t>(idx)];
    SetSoundPitch(snd, pitch);
    SetSoundVolume(snd, std::clamp(volume * sfxVol_, 0.0f, 1.0f));
    PlaySound(snd);
}

void SoundSynth::PlayIbsFart() {
    if (!initialized_ || sfxMuted_) return;
    float pitch = 0.85f + static_cast<float>(std::rand() % 35) / 100.0f;
    SetSoundPitch(ibsFartSound_, pitch);
    SetSoundVolume(ibsFartSound_, std::clamp(1.0f * sfxVol_, 0.0f, 1.0f));
    PlaySound(ibsFartSound_);
}

void SoundSynth::SetBossMusic(bool bossMode) {
    bossMusicMode_ = bossMode;
}

void SoundSynth::UpdateMusic(float dt, bool muted) {
    if (!initialized_ || muted || musicVol_ <= 0.001f) return;

    float stepDuration = bossMusicMode_ ? 0.115f : 0.155f;
    stepTimer_ += dt;
    if (stepTimer_ < stepDuration) return;
    stepTimer_ -= stepDuration;

    static const std::array<int, 32> arpeggioPattern = {
        0, 2, 4, 7,  0, 3, 5, 8,  0, 2, 4, 6,  7, 6, 4, 2,
        1, 3, 5, 9,  1, 4, 6, 10, 0, 4, 7, 11, 13, 11, 7, 4
    };

    static const std::array<int, 32> bossArpeggioPattern = {
        7, 6, 4, 0,  8, 5, 4, 0,  11, 10, 7, 6,  4, 6, 7, 11,
        14, 13, 11, 7, 12, 11, 9, 6, 7, 4, 2, 0, 6, 9, 13, 14
    };

    static const std::array<int, 32> bassPattern = {
        0, -1, 0, -1,  3, -1, 3, -1,  5, -1, 5, -1,  4, -1, 6, -1,
        0, -1, 7, -1,  5, -1, 3, -1,  4, -1, 4, -1,  6, -1, 7, -1
    };

    int step = currentStep_ % 32;

    int leadIdx = bossMusicMode_ ? bossArpeggioPattern[static_cast<size_t>(step)]
                                 : arpeggioPattern[static_cast<size_t>(step)];
    if (leadIdx >= 0 && leadIdx < static_cast<int>(leadNotes_.size())) {
        PlaySound(leadNotes_[static_cast<size_t>(leadIdx)]);
    }

    int bassIdx = bassPattern[static_cast<size_t>(step)];
    if (bassIdx >= 0 && bassIdx < static_cast<int>(bassNotes_.size())) {
        PlaySound(bassNotes_[static_cast<size_t>(bassIdx)]);
    } else if (bossMusicMode_ && (step % 2 == 1)) {
        PlaySound(bassNotes_[0]);
    }

    if (step % 4 == 0 || (bossMusicMode_ && step % 4 == 2)) {
        PlaySound(percKick_);
    } else if (step % 2 == 1) {
        PlaySound(percHat_);
    }

    currentStep_ = (currentStep_ + 1) % 32;
}

Sound SoundSynth::GenerateNoteWave(float freq, float duration, int waveType, float volume) {
    int sampleCount = static_cast<int>(SAMPLE_RATE * duration);
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(sampleCount);
    wave.sampleRate = SAMPLE_RATE;
    wave.sampleSize = 16;
    wave.channels = 1;

    short* pcm = static_cast<short*>(RL_MALLOC(sizeof(short) * sampleCount));
    for (int i = 0; i < sampleCount; ++i) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float env = std::exp(-t * 10.5f);
        if (t < 0.008f) env *= (t / 0.008f);

        float sample = 0.0f;
        if (waveType == 0) {
            float vibrato = 1.0f + 0.004f * std::sin(TWO_PI * 6.0f * t);
            float phase = std::fmod(t * freq * vibrato, 1.0f);
            sample = (phase < 0.25f) ? 0.8f : -0.4f;
        } else if (waveType == 1) {
            float phase = std::fmod(t * freq, 1.0f);
            float saw = 2.0f * phase - 1.0f;
            float sub = (std::fmod(t * freq * 0.5f, 1.0f) < 0.5f) ? 0.6f : -0.6f;
            sample = 0.6f * saw + 0.4f * sub;
        } else if (waveType == 2) {
            float dropFreq = freq * std::exp(-t * 26.0f) + 32.0f;
            sample = std::sin(TWO_PI * dropFreq * t);
            env = std::exp(-t * 18.0f);
        } else {
            sample = RandFloatSym();
            env = std::exp(-t * 48.0f);
        }

        pcm[i] = FloatToPcm(sample * env * volume);
    }

    wave.data = pcm;
    Sound snd = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return snd;
}

Sound SoundSynth::GenerateSfxWave(SfxId id) {
    float duration = 0.20f;
    switch (id) {
        case SfxId::SwordSlash:  duration = 0.14f; break;
        case SfxId::HitEnemy:    duration = 0.12f; break;
        case SfxId::HitShield:   duration = 0.20f; break;
        case SfxId::PogoBounce:  duration = 0.16f; break;
        case SfxId::Jump:        duration = 0.14f; break;
        case SfxId::Dash:        duration = 0.16f; break;
        case SfxId::CoinPickup:  duration = 0.18f; break;
        case SfxId::ChestOpen:   duration = 0.55f; break;
        case SfxId::FairyFail:   duration = 0.35f; break;
        case SfxId::SpellCast:   duration = 0.24f; break;
        case SfxId::PlayerHurt:  duration = 0.22f; break;
        case SfxId::PlayerDeath: duration = 0.65f; break;
        case SfxId::ManorBuild:  duration = 0.32f; break;
        case SfxId::BossRoar:    duration = 0.60f; break;
        default:                 duration = 0.15f; break;
    }

    int sampleCount = static_cast<int>(SAMPLE_RATE * duration);
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(sampleCount);
    wave.sampleRate = SAMPLE_RATE;
    wave.sampleSize = 16;
    wave.channels = 1;

    short* pcm = static_cast<short*>(RL_MALLOC(sizeof(short) * sampleCount));
    float lpState = 0.0f;

    for (int i = 0; i < sampleCount; ++i) {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float out = 0.0f;

        switch (id) {
            case SfxId::SwordSlash: {
                float noise = RandFloatSym();
                float mod = std::sin(TWO_PI * (340.0f + t * 1400.0f) * t);
                lpState = lpState * 0.72f + (noise * 0.75f + mod * 0.25f) * 0.28f;
                out = lpState * std::exp(-t * 22.0f) * 0.95f;
                break;
            }
            case SfxId::HitEnemy: {
                float freq = std::max(30.0f, 160.0f - t * 900.0f);
                float thud = std::sin(TWO_PI * freq * t);
                float splatter = RandFloatSym() * std::exp(-t * 32.0f) * 0.45f;
                out = (thud * 0.75f + splatter) * std::exp(-t * 16.0f) * 0.85f;
                break;
            }
            case SfxId::HitShield: {
                out = std::sin(TWO_PI * 880.0f * t + 4.0f * std::sin(TWO_PI * 1320.0f * t)) * std::exp(-t * 14.0f) * 0.75f;
                break;
            }
            case SfxId::PogoBounce: {
                float normT = t / duration;
                float freq = 240.0f + (680.0f - 240.0f) * normT;
                float s = std::sin(TWO_PI * freq * t);
                float sq = (s >= 0.0f) ? 0.65f : -0.65f;
                out = sq * std::exp(-t * 10.0f) * 0.75f;
                break;
            }
            case SfxId::Jump: {
                float normT = t / duration;
                float freq = 180.0f + 340.0f * (normT * normT);
                out = std::sin(TWO_PI * freq * t) * std::exp(-t * 12.0f) * 0.60f;
                break;
            }
            case SfxId::Dash: {
                float whoosh = RandFloatSym() * 0.7f + std::sin(TWO_PI * (420.0f + t * 1800.0f) * t) * 0.3f;
                lpState = lpState * 0.65f + whoosh * 0.35f;
                out = lpState * std::exp(-t * 16.0f) * 0.80f;
                break;
            }
            case SfxId::CoinPickup: {
                float freq = (t < 0.06f) ? 987.77f : 1318.51f;
                float localT = (t < 0.06f) ? t : (t - 0.06f);
                float bell = std::sin(TWO_PI * freq * t) + 0.25f * std::sin(TWO_PI * freq * 2.0f * t);
                out = bell * std::exp(-localT * 18.0f) * 0.65f;
                break;
            }
            case SfxId::ChestOpen: {
                float freq = 523.25f;
                if (t >= 0.36f)      freq = 1046.50f;
                else if (t >= 0.24f) freq = 783.99f;
                else if (t >= 0.12f) freq = 659.25f;
                float tone = std::sin(TWO_PI * freq * t) + 0.35f * std::sin(TWO_PI * freq * 2.0f * t);
                out = tone * std::exp(-std::fmod(t, 0.12f) * 8.0f) * (1.0f - t / duration * 0.4f) * 0.65f;
                break;
            }
            case SfxId::FairyFail: {
                float p1 = std::fmod(t * 130.0f, 1.0f) * 2.0f - 1.0f;
                float p2 = std::fmod(t * 138.0f, 1.0f) * 2.0f - 1.0f;
                out = (p1 + p2) * 0.45f * std::exp(-t * 5.0f);
                break;
            }
            case SfxId::SpellCast: {
                float freq = 320.0f + 560.0f * std::sin(TWO_PI * 9.0f * t);
                float fm = std::sin(TWO_PI * freq * t + 2.5f * std::sin(TWO_PI * 110.0f * t));
                out = fm * std::exp(-t * 10.0f) * 0.70f;
                break;
            }
            case SfxId::PlayerHurt: {
                float freq = 290.0f - t * 850.0f;
                float sq = (std::sin(TWO_PI * std::max(55.0f, freq) * t) >= 0.0f) ? 0.7f : -0.7f;
                out = (sq + RandFloatSym() * 0.3f) * std::exp(-t * 11.0f) * 0.80f;
                break;
            }
            case SfxId::PlayerDeath: {
                float freq = 260.0f * std::exp(-t * 2.6f);
                float saw = 2.0f * std::fmod(t * freq, 1.0f) - 1.0f;
                out = (saw * 0.7f + RandFloatSym() * 0.25f) * std::exp(-t * 3.5f) * 0.85f;
                break;
            }
            case SfxId::ManorBuild: {
                float hammer = (t < 0.08f || (t >= 0.14f && t < 0.22f)) ? RandFloatSym() : 0.0f;
                float chime = std::sin(TWO_PI * 440.0f * t) + std::sin(TWO_PI * 659.25f * t);
                out = (hammer * 0.5f + chime * 0.35f) * std::exp(-t * 7.5f) * 0.75f;
                break;
            }
            case SfxId::BossRoar: {
                float baseFreq = 92.0f - t * 40.0f + 14.0f * std::sin(TWO_PI * 19.0f * t);
                float growl = std::sin(TWO_PI * baseFreq * t + 3.5f * std::sin(TWO_PI * 45.0f * t));
                out = (growl * 0.7f + RandFloatSym() * 0.35f) * std::exp(-t * 3.2f) * 0.90f;
                break;
            }
            default:
                break;
        }

        pcm[i] = FloatToPcm(out);
    }

    wave.data = pcm;
    Sound snd = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return snd;
}
