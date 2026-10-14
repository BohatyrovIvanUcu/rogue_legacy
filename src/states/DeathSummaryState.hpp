// === FILE: src/states/DeathSummaryState.hpp ===
#pragma once

#include "states/IGameState.hpp"

class DeathSummaryState : public IGameState {
public:
    DeathSummaryState() = default;
    ~DeathSummaryState() override = default;

    void OnEnter(GameContext& ctx) override;
    void OnExit(GameContext& ctx) override;
    void Update(float dt, GameContext& ctx) override;
    void Draw(GameContext& ctx) override;

private:
    float animTimer_ = 0.0f;
    float fireworkTimer_ = 0.0f;
    int galleryIndex_ = -1; // -1 = current run summary, >= 0 = ancestral painting index

    void DrawCastleCorridorBackground(float time) const;
    void DrawRunOverviewModal(GameContext& ctx);
    void DrawAncestorGalleryModal(GameContext& ctx);
};