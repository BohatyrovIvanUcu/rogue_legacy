// === FILE: src/states/CastlePlayState.hpp ===
#pragma once

#include "states/IGameState.hpp"
#include "states/SettingsMenu.hpp"
#include "world/CastleGenerator.hpp"
#include "entities/Player.hpp"
#include <string>

class CastlePlayState : public IGameState {
public:
    CastlePlayState() = default;
    ~CastlePlayState() override = default;

    void OnEnter(GameContext& ctx) override;
    void OnExit(GameContext& ctx) override;
    void Update(float dt, GameContext& ctx) override;
    void Draw(GameContext& ctx) override;

private:
    void CheckRoomDoorTransitions(GameContext& ctx);
    void DrawHud(const GameContext& ctx, const Room& curRoom) const;
    void DrawMinimap(const GameContext& ctx, const Room& curRoom) const;
    void DrawTeleporterOverlay(const GameContext& ctx) const;
    void UpdateTeleporterOverlay(GameContext& ctx);

private:
    CastleGenerator castle_{};
    Player player_{};
    int curRoomId_ = 0;
    int curGx_ = Config::ENTRANCE_GX;
    int curGy_ = Config::ENTRANCE_GY;

    bool teleporterMapOpen_ = false;
    int tpIndex_ = 0;
    int tpCursorGx_ = Config::ENTRANCE_GX;
    int tpCursorGy_ = Config::ENTRANCE_GY;

    Camera2D worldCam_{};
    Vector2 camTarget_{ 600.0f, 360.0f };
    SettingsMenu settingsMenu_{};

    float bannerTimer_ = 0.0f;
    std::string bannerText_{};
};