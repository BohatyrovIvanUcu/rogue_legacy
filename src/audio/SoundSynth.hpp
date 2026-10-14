#pragma once
#include "core/Types.hpp"
#include <raylib.h>
#include <array>
#include <vector>
#include <algorithm>

class SoundSynth {
public:
    SoundSynth() = default;
    ~SoundSynth();

    void Init();
    void Shutdown();

    void PlaySfx(SfxId id, float pitch = 1.0f, float volume = 1.0f);
    void PlayIbsFart();
    void UpdateMusic(float dt, bool muted);
    void SetBossMusic(bool bossMode);

    void SetSfxMuted(bool muted) { sfxMuted_ = muted; }
    bool IsSfxMuted() const { return sfxMuted_; }
    void SetMusicVolume(float vol) { musicVol_ = std::clamp(vol, 0.0f, 1.0f); }
    float GetMusicVolume() const { return musicVol_; }
    void SetSfxVolume(float vol) { sfxVol_ = std::clamp(vol, 0.0f, 1.0f); }
    float GetSfxVolume() const { return sfxVol_; }

private:
    Sound GenerateSfxWave(SfxId id);
    Sound GenerateNoteWave(float freq, float duration, int waveType, float volume);

private:
    bool initialized_ = false;
    std::array<Sound, static_cast<size_t>(SfxId::COUNT)> sfxBank_{};
    Sound ibsFartSound_{};

    std::array<Sound, 16> leadNotes_{};
    std::array<Sound, 8> bassNotes_{};
    Sound percKick_{};
    Sound percHat_{};

    float stepTimer_ = 0.0f;
    int currentStep_ = 0;
    bool bossMusicMode_ = false;
    bool sfxMuted_ = false;
    float musicVol_ = 1.0f;
    float sfxVol_ = 1.0f;
};
