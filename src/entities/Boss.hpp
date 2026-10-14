// === FILE: src/entities/Boss.hpp ===
#pragma once

#include "entities/Entity.hpp"
#include "entities/Enemy.hpp"
#include "entities/Projectile.hpp"
#include <vector>
#include <string>

class GameContext;
class Player;

struct SlimeUnit {
    uint32_t id = 0;
    Vector2 pos{ 0.0f, 0.0f };
    Vector2 vel{ 0.0f, 0.0f };
    int tier = 4; // 4 (2.4x) -> 3 (1.7x) -> 2 (1.2x) -> 1 (0.75x)
    float scale = 2.4f;
    float radius = 58.0f;
    int hp = 150;
    int maxHp = 150;
    float jumpTimer = 1.0f;
    float hurtFlashTimer = 0.0f;
    bool onGround = false;
};

class Boss : public Entity {
public:
    BossType type = BossType::Khidr;
    int level = 10;
    int phase = 1; // Для JohannesTraitor: 1 = Johannes the Traitor, 2 = The Fountain
    int contactDamage = 24;

    float stateTimer = 0.0f;
    float subAttackTimer = 0.0f;
    int currentPattern = 0;
    float spiralAngle = 0.0f;
    float dashCooldown = 5.0f;
    bool isDashing = false;
    Vector2 dashVel{ 0.0f, 0.0f };
    float hurtFlashTimer = 0.0f;
    float animTimer = 0.0f;

    // Під-частини для Боса 4 (Herodotus — Нескінченний Слайм)
    std::vector<SlimeUnit> slimeUnits{};

public:
    Boss() = default;

    void Init(BossType bType, int bossLevel, Vector2 arenaCenter);

    void Update(
        float dt,
        GameContext& ctx,
        const ITileMap* tileMap,
        const Player& player,
        std::vector<Projectile>& outProjectiles,
        std::vector<Enemy>& outMinions,
        bool timeStopActive
    );

    void Draw() const;
    void DrawHudHealthBar() const;

    // Перевірка влучання мечем або снарядом гравця (для Herodotus перевіряє кожен живий SlimeUnit)
    bool CheckAndApplyPlayerAttack(
        const Rectangle& attackBox,
        int damage,
        bool isCrit,
        uint32_t swingOrProjId,
        std::vector<uint32_t>& hitRegistry,
        GameContext& ctx,
        std::vector<Projectile>& outProjectiles,
        Vector2& outHitCenter
    );

    // Перевірка контактної шкоди боса по гравцю
    bool CheckPlayerContact(const Rectangle& playerBounds, Vector2& outContactCenter) const;

    std::string GetCurrentName() const;
    std::string GetCurrentTitle() const;

private:
    void UpdateKhidr(float dt, const Player& player, std::vector<Projectile>& outProjectiles);
    void UpdateAlexander(float dt, GameContext& ctx, const Player& player, std::vector<Projectile>& outProjectiles, std::vector<Enemy>& outMinions);
    void UpdatePonceDeLeon(float dt, GameContext& ctx, const Player& player, std::vector<Projectile>& outProjectiles);
    void UpdateHerodotus(float dt, const ITileMap* tileMap, const Player& player, std::vector<Projectile>& outProjectiles);
    void UpdateJohannes(float dt, GameContext& ctx, const ITileMap* tileMap, const Player& player, std::vector<Projectile>& outProjectiles);

    static SlimeUnit MakeSlimeUnit(int tier, int baseUnitHp, Vector2 center, Vector2 initialVel);
};