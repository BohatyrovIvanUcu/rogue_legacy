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

    static Vector2 GetVirtualMousePos();

private:
    enum class FocusArea : int {
        Tabs = 0,
        List = 1,
        Bottom = 2
    };

    bool isOpen_ = false;
    bool inGamePause_ = false;
    int currentTab_ = 0; // 0 = Controls, 1 = Audio & Display
    FocusArea focusArea_ = FocusArea::List;
    int bottomButtonIdx_ = 1; // 0 = Reset Defaults, 1 = Back / Apply, 2 = Quit (if pause)

    // Controls tab state
    int selectedActionIdx_ = 0; // 0 .. InputAction::COUNT - 1
    int selectedSlot_ = 0;      // 0 = Primary, 1 = Secondary
    bool isRebinding_ = false;
    float rebindPulseTimer_ = 0.0f;

    // Audio & Display tab state
    int audioSelectedRow_ = 0; // 0..6

    float animTimer_ = 0.0f;
    std::string statusMessage_ = "";
    float statusMessageTimer_ = 0.0f;
};
