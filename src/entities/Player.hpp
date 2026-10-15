// === FILE: src/entities/Player.hpp ===
#pragma once

#include "entities/Entity.hpp"
#include "entities/Projectile.hpp"
#include <vector>
#include <array>
#include <cstdint>

class GameContext;

struct ShoutPulseEvent {
    bool triggered = false;
    Vector2 origin{ 0.0f, 0.0f };
    float radius = 1400.0f;
    float knockbackSpeed = 450.0f;
    float stunDuration = 1.5f;
    int damage = 0;
};

class Player : public Entity {
public:
    CalculatedStats stats{};
    int mp = 50;
    float manaRegenAccum = 0.0f;

    // Стан стрибка, Coyote Time, Jump Buffer та Dash
    float coyoteTimer = 0.0f;
    float jumpBufferTimer = 0.0f;
    int airJumpsLeft = 0;
    int airDashesLeft = 1;
    bool isDashing = false;
    float dashTimer = 0.0f;
    float dashCooldownTimer = 0.0f;
    float dashDirX = 1.0f;
    float dropThroughPlatformTimer = 0.0f;

    // Стан ближнього бою та Pogo Down-Strike
    bool isAttacking = false;
    bool isDownStriking = false;
    float attackTimer = 0.0f;
    float attackCooldownTimer = 0.0f;
    std::vector<uint32_t> currentSwingHitIds{};

    // Стан заклять та талантів класу
    SpellType activeSpell = SpellType::Dagger;
    std::array<SpellType, 3> archmageSpells{ SpellType::Dagger, SpellType::Axe, SpellType::CrowStorm };
    int archmageSpellIndex = 0;
    float spellCooldownTimer = 0.0f;
    float talentCooldownTimer = 0.0f;
    float timeStopTimer = 0.0f;

    bool isBlocking = false;
    bool isMistForm = false;
    float mistManaAccum = 0.0f;
    bool headlampOn = true;

    // Події ударної хвилі Варвара та візуалізація кільця крику
    ShoutPulseEvent pendingShout_{};
    float shoutVisualTimer = 0.0f;

    // Таймери невразливості, Hitstop та IBS
    float iframeTimer = 0.0f;
    float hitstopTimer = 0.0f;
    float hurtFlashTimer = 0.0f;
    float ibsTimer = 0.0f;
    float animTime = 0.0f;

    // Phase 7: Squash & Stretch, Footsteps
    float landSquashTimer = 0.0f;
    float jumpStretchTimer = 0.0f;
    bool wasOnGround = true;
    float footstepDustTimer = 0.0f;

    // Риси Clonus, StereoBlind та Ambilevous
    float clonusIntervalTimer = 18.0f;
    float clonusSpasmTimer = 0.0f;
    float turnFlipTimer = 0.0f;
    float prevFacingDir = 1.0f;
    float ambilevousTimer = 0.0f;

    // Прапорці для випробувань Скринь Феї (Fairy Chest) у поточній кімнаті
    bool jumpedInCurrentRoom = false;
    bool damagedInCurrentRoom = false;

public:
    Player() = default;

    void InitFromContext(GameContext& ctx, Vector2 spawnCenter);
    void RecalculateStats(GameContext& ctx, bool preserveRatios = true);
    void ResetRoomTrialFlags();

    void Update(
        float dt,
        GameContext& ctx,
        const ITileMap* tileMap,
        std::vector<Projectile>& projectiles
    );

    void Draw(const GameContext& ctx) const;

    // Бойова взаємодія
    Rectangle GetSwordHitbox() const;
    int RollMeleeDamage(bool isDownStrike, bool& outIsCrit) const;
    void TriggerPogoBounce(GameContext& ctx);
    void OnMeleeDamageDealt(int damageDealt, GameContext& ctx);
    void OnEnemyKilled(GameContext& ctx, EnemyType enemyType = EnemyType::Skeleton);

    // Отримання шкоди (повертає true, якщо шкоду було реально завдано)
    bool TakeDamage(
        int rawDamage,
        Vector2 sourceCenter,
        GameContext& ctx,
        const std::string& attackerName,
        int& outRetaliationDamage,
        std::vector<Projectile>* outProjectiles = nullptr
    );

    void HealHp(int amount, GameContext& ctx);
    void RestoreMp(int amount, GameContext& ctx);

    bool ConsumeShoutPulse(ShoutPulseEvent& outEvent);
    bool IsTimeStopActive() const { return timeStopTimer > 0.0f; }
    bool IsDwarf() const { return stats.scaleY < 0.65f; }

private:
    void HandleMovementAndPhysics(
        float dt,
        GameContext& ctx,
        const ITileMap* tileMap,
        std::vector<Projectile>& projectiles
    );

    void HandleCombatAndAbilities(
        float dt,
        GameContext& ctx,
        const ITileMap* tileMap,
        std::vector<Projectile>& projectiles
    );

    void ResolveTileCollisionsX(const ITileMap* tileMap, bool isDwarf);
    void ResolveTileCollisionsY(const ITileMap* tileMap, bool isDwarf, float prevBottom, bool hasEhs = false);
};