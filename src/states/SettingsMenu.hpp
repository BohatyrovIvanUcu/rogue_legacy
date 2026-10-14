// === FILE: src/states/SettingsMenu.hpp ===
#pragma once
#include "core/InputConfig.hpp"
#include <raylib.h>
#include <string>

class GameContext;

class SettingsMenu {
public:
    SettingsMenu() = default;

    void Open(bool inGamePause = false);
    void Close(GameContext* ctx = nullptr);
    bool IsOpen() const { return isOpen_; }
    bool IsInGamePause() const { return inGamePause_; }

    void Update(float dt, GameContext& ctx);
    void Draw(GameContext& ctx);

private:
    bool isOpen_ = false;
    bool inGamePause_ = false;
    int currentTab_ = 0; // 0 = Controls, 1 = Audio & Display

    // Controls tab state
    int selectedActionIdx_ = 0; // 0 .. InputAction::COUNT - 1
    int selectedSlot_ = 0;      // 0 = Primary, 1 = Secondary
    bool isRebinding_ = false;
    float rebindPulseTimer_ = 0.0f;

    // Audio & Display tab state
    int audioSelectedRow_ = 0; // 0 = Music Mute, 1 = Music Vol, 2 = SFX Mute, 3 = SFX Vol, 4 = Fullscreen

    float animTimer_ = 0.0f;
    std::string statusMessage_ = "";
    float statusMessageTimer_ = 0.0f;
};
