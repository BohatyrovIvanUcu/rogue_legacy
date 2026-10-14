// === FILE: src/states/TitleState.hpp ===
#pragma once

#include "states/IGameState.hpp"
#include "states/SettingsMenu.hpp"

class TitleState : public IGameState {
public:
    TitleState() = default;
    ~TitleState() override = default;

    void OnEnter(GameContext& ctx) override;
    void OnExit(GameContext& ctx) override;
    void Update(float dt, GameContext& ctx) override;
    void Draw(GameContext& ctx) override;

private:
    int selectedIndex_ = 0;
    bool hasSaveFile_ = false;
    float animTimer_ = 0.0f;
    SettingsMenu settingsMenu_{};
};