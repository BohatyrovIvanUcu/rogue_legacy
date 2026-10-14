// === FILE: src/entities/Projectile.hpp ===
#pragma once

#include "entities/Entity.hpp"
#include <vector>
#include <cstdint>

class GameContext;

enum class ProjectileKind : int {
    PlayerDagger = 0,
    PlayerAxe = 1,
    PlayerChakram = 2,
    PlayerScythe = 3,
    PlayerFlameOrb = 4,
    PlayerConflux = 5,
    PlayerCrow = 6,
    PlayerDragonFireball = 7,
    PlayerAncestorBlade = 8,
    PlayerPaladinLightning = 9,

    EnemyBone = 10,
    EnemyEyeBolt = 11,
    EnemyFirePillar = 12,
    EnemyArrow = 13,
    BossFlameTrail = 14,
    BossAcidDrop = 15,
    BossSwordWave = 16
};

class Projectile {
public:
    uint32_t id = 0;
    ProjectileKind kind = ProjectileKind::PlayerDagger;
    bool fromPlayer = true;
    bool active = true;

    Vector2 pos{ 0.0f, 0.0f }; // Центр снаряда
    Vector2 vel{ 0.0f, 0.0f };
    float radius = 10.0f;
    float scale = 1.0f;
    int damage = 15;

    float life = 2.0f;
    float maxLife = 2.0f;
    float rotation = 0.0f;
    float rotSpeed = 0.0f;

    bool piercing = false;
    bool ignoreWalls = false;
    bool destroyableBySword = false;
    int bouncesLeft = 0;

    // Спеціальні параметри для Chakram, FlameBarrier та CrowStorm
    float orbitAngle = 0.0f;
    float orbitRadius = 68.0f;
    bool returningToPlayer = false;
    Vector2 targetHint{ 0.0f, 0.0f };
    bool hasValidTargetHint = false;

    Color primaryColor = WHITE;
    Color secondaryColor = GOLD;

    // Список ID сутностей, які вже отримали шкоду від цього проникного снаряда
    std::vector<uint32_t> hitIds{};
    float rehitResetTimer = 0.0f;

public:
    Projectile();

    Rectangle GetHitbox() const;
    bool HasAlreadyHit(uint32_t entityId) const;
    void RegisterHit(uint32_t entityId);

    void Update(
        float dt,
        GameContext& ctx,
        const ITileMap* tileMap,
        Vector2 playerCenter,
        bool timeStopActive
    );

    void Draw() const;

    // Фабричні методи для створення снарядів заклять, талантів та атак ворогів/босів
    static void SpawnPlayerSpell(
        std::vector<Projectile>& outList,
        SpellType spell,
        Vector2 origin,
        float facingDir,
        int baseDamage,
        float empowerScale,
        float empowerDmgMult
    );

    static Projectile MakeDragonFireball(Vector2 origin, Vector2 dir, int damage);
    static Projectile MakeAncestorBlade(float xPos, int damage);
    static Projectile MakePaladinLightning(Vector2 targetPos, int damage);

    static Projectile MakeEnemyBone(Vector2 origin, Vector2 velocity, int damage);
    static Projectile MakeEnemyEyeBolt(Vector2 origin, Vector2 velocity, int damage, Color tint = ORANGE);
    static Projectile MakeEnemyFirePillar(Vector2 floorPos, int damage);
    static Projectile MakeEnemyArrow(Vector2 origin, float dirX, int damage);
    static Projectile MakeBossFlameTrail(Vector2 pos, int damage, float duration = 4.5f);
    static Projectile MakeBossAcidDrop(Vector2 origin, Vector2 velocity, int damage);
    static Projectile MakeBossSwordWave(Vector2 origin, Vector2 velocity, int damage);
};