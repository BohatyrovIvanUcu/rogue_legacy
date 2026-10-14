// === FILE: src/entities/Enemy.hpp ===
#pragma once

#include "entities/Entity.hpp"
#include "entities/Projectile.hpp"
#include <vector>
#include <string>

class GameContext;
class Player;

class Enemy : public Entity {
public:
    EnemyType type = EnemyType::Skeleton;
    int tier = 1; // 1 = Base, 2 = Advanced, 3 = Elite
    int level = 1;
    int contactDamage = 15;
    int goldReward = 20;
    bool isDementiaPhantom = false; // Якщо true — напівпрозорий фіолетовий фантом (гине з 1 удару, 0 золота)

    float stateTimer = 0.0f;
    float attackCooldown = 1.5f;
    float stunTimer = 0.0f;
    float hurtFlashTimer = 0.0f;
    float animTimer = 0.0f;

    // Додаткові параметри для специфічних ШІ (FloatingEye, FireMage, Warg)
    Vector2 anchorPos{ 0.0f, 0.0f };
    float sinePhase = 0.0f;
    bool isTelegraphing = false;
    Vector2 lockedAttackTarget{ 0.0f, 0.0f };

public:
    Enemy() = default;

    void Init(
        EnemyType eType,
        int eTier,
        int roomLevel,
        Vector2 spawnCenter,
        bool dementiaPhantom = false
    );

    void Update(
        float dt,
        GameContext& ctx,
        const ITileMap* tileMap,
        const Player& player,
        std::vector<Projectile>& outProjectiles,
        bool timeStopActive
    );

    void Draw() const;

    // Повертає фронтальний хітбокс щита для ShieldKnight (якщо ворог іншого типу — повертає порожній прямокутник)
    Rectangle GetShieldHitbox() const;
    bool IsBlockedByShield(Vector2 attackOrigin, bool isDownStrike) const;

    // Отримання шкоди. Повертає true, якщо ворога вбито цим ударом.
    bool TakeDamage(
        int amount,
        Vector2 knockbackVel,
        bool isCrit,
        GameContext& ctx
    );

    void ApplyStunAndKnockback(Vector2 velocity, float stunDuration);
    std::string GetDisplayName() const;

private:
    void ResolvePhysicsAndTiles(float dt, const ITileMap* tileMap, bool stopAtPlatformEdges);
};