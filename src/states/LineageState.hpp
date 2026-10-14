// === FILE: src/states/LineageState.hpp ===
#pragma once

#include "states/IGameState.hpp"

class LineageState : public IGameState {
public:
    LineageState() = default;
    ~LineageState() override = default;

    void OnEnter(GameContext& ctx) override;
    void OnExit(GameContext& ctx) override;
    void Update(float dt, GameContext& ctx) override;
    void Draw(GameContext& ctx) override;

private:
    int selectedHeirIndex_ = 0;
    float animTimer_ = 0.0f;
};