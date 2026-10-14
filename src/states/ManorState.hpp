// === FILE: src/states/ManorState.hpp ===
#pragma once

#include "states/IGameState.hpp"
#include "core/Types.hpp"

class ManorState : public IGameState {
public:
    ManorState() = default;
    ~ManorState() override = default;

    void OnEnter(GameContext& ctx) override;
    void OnExit(GameContext& ctx) override;
    void Update(float dt, GameContext& ctx) override;
    void Draw(GameContext& ctx) override;

private:
    bool IsNodeUnlocked(ManorNodeId id, const GameContext& ctx) const;
    ManorNodeId FindNodeAtGrid(int gx, int gy) const;

private:
    int selectedNodeIdx_ = 3; // За замовчуванням Stat_Health
    float animTimer_ = 0.0f;
};