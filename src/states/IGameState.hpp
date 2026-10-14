// === FILE: src/states/IGameState.hpp ===
#pragma once

class GameContext;

class IGameState {
public:
    virtual ~IGameState() = default;

    virtual void OnEnter(GameContext& ctx) = 0;
    virtual void OnExit(GameContext& ctx) = 0;
    virtual void Update(float dt, GameContext& ctx) = 0;
    virtual void Draw(GameContext& ctx) = 0;
};