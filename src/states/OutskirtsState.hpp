// === FILE: src/states/OutskirtsState.hpp ===
#pragma once

#include "states/IGameState.hpp"
#include "core/Types.hpp"
#include <raylib.h>
#include <vector>

struct TollCoinVisual {
    Vector2 pos{};
    Vector2 vel{};
    bool active = true;
};

class OutskirtsState : public IGameState {
public:
    OutskirtsState() = default;
    ~OutskirtsState() override = default;

    void OnEnter(GameContext& ctx) override;
    void OnExit(GameContext& ctx) override;
    void Update(float dt, GameContext& ctx) override;
    void Draw(GameContext& ctx) override;

private:
    enum class ModalMenu : int {
        None = 0,
        Blacksmith = 1,
        Enchantress = 2,
        Architect = 3,
        CharonToll = 4
    };

    void UpdateModalMenu(GameContext& ctx);
    void DrawBlacksmithModal(const GameContext& ctx) const;
    void DrawEnchantressModal(const GameContext& ctx) const;
    void DrawArchitectModal(const GameContext& ctx) const;
    void DrawCharonModal(const GameContext& ctx) const;

private:
    float heroX_ = 160.0f;
    float heroY_ = 580.0f;
    float heroVelX_ = 0.0f;
    float heroVelY_ = 0.0f;
    bool onGround_ = true;
    float facingDir_ = 1.0f;
    float animTimer_ = 0.0f;
    float attackTimer_ = 0.0f;

    ModalMenu activeMenu_ = ModalMenu::None;
    int menuSlotTab_ = 0; // 0..4 (EquipSlot)
    int menuItemRow_ = 0; // 0..4 (EquipTier) для Коваля або 0..7 (RuneType) для Чарівниці

    // Анімація сплати мита Харону та опускання решітки воріт замку
    bool charonSequenceActive_ = false;
    float charonTimer_ = 0.0f;
    float gatePortcullisY_ = 0.0f;
    std::vector<TollCoinVisual> flyingCoins_;
};