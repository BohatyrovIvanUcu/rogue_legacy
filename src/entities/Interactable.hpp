// === FILE: src/entities/Interactable.hpp ===
#pragma once

#include "entities/Entity.hpp"
#include <vector>

class GameContext;
class Player;

enum class InteractableKind : int {
    Torch = 0,         // 'C' — підсвічник / факел (руйнується ударом, дає світло в LandOfDarkness)
    BreakableProp = 1, // 'B' — бочка / лицарський обладунок / стіл
    Chest = 2,         // '$' — звичайна або срібна скриня (золото або креслення Коваля)
    FairyChest = 3,    // 'F' — Скриня Феї (відкривається при виконанні FairyCondition, видає Руну)
    Teleporter = 4,    // 'P' — портал телепорту
    BossDoor = 5,      // Двері до арени боса біому або Фінального Боса
    DroppedCoin = 6,   // Монетка (10g) або мішечок золота (50g) з фізикою відскоку
    MeatDrop = 7,      // Смажена курка (+20% MaxHP * PotionEfficacy)
    ManaDrop = 8,      // Синій флакон мани (+30% MaxMP * PotionEfficacy)
    Shrine = 9,        // 'H' — Вівтар молитви / Скринька пожертви (Statue / Shrine / Donation box)
    Statue = 10        // 'K' — кам'яна статуя лицаря на постаменті
};

class Interactable : public Entity {
public:
    InteractableKind kind = InteractableKind::Torch;
    int subVariant = 0; // Для BreakableProp: 0=Barrel, 1=ArmorStand, 2=Table; для DroppedCoin: 0=Coin(10g), 1=Bag(50g)
    int goldValue = 10;

    bool isOpened = false;
    bool isLocked = false;
    bool isSilverChest = false;
    FairyCondition fairyCond = FairyCondition::NoDamage;
    BossType bossDoorTarget = BossType::Khidr;

    float animTimer = 0.0f;
    float pickupDelay = 0.20f;
    float lifetime = 30.0f;

public:
    Interactable() = default;

    void Update(
        float dt,
        GameContext& ctx,
        const ITileMap* tileMap,
        Player& player,
        std::vector<Interactable>& spawnedDrops
    );

    void Draw(const GameContext& ctx) const;

    // Викликається при влучанні мечем або снарядом гравця по об'єкту
    bool OnHitByPlayer(
        GameContext& ctx,
        Player& player,
        std::vector<Interactable>& outDrops
    );

    // Фабричні методи спавну
    static Interactable MakeTorch(Vector2 centerPos);
    static Interactable MakeBreakableProp(Vector2 bottomCenter, int variant);
    static Interactable MakeChest(Vector2 bottomCenter, bool silver, bool alreadyOpened = false);
    static Interactable MakeFairyChest(Vector2 bottomCenter, FairyCondition cond, bool alreadyOpened = false);
    static Interactable MakeTeleporter(Vector2 bottomCenter);
    static Interactable MakeBossDoor(Vector2 bottomCenter, BossType targetBoss);
    static Interactable MakeShrine(Vector2 bottomCenter);
    static Interactable MakeStatue(Vector2 bottomCenter);

    static void SpawnLootBurst(
        std::vector<Interactable>& outDrops,
        Vector2 origin,
        int baseGoldAmount,
        bool canDropConsumables = true
    );

private:
    void GrantBlueprintOrGold(GameContext& ctx, BiomeType currentBiome);
    void GrantRuneOrGold(GameContext& ctx);
};