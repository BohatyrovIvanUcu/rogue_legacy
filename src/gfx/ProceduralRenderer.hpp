#pragma once
#include "core/Types.hpp"
#include <raylib.h>
#include <array>

class GameContext;

struct HeroRenderParams {
    Vector2 centerPos{};
    Vector2 velocity{};
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float facingDir = 1.0f;
    ClassType classType = ClassType::Knights_Base;
    EquipTier swordTier = EquipTier::Squire;
    EquipTier helmTier = EquipTier::None;
    EquipTier chestTier = EquipTier::None;
    EquipTier limbsTier = EquipTier::None;
    EquipTier capeTier = EquipTier::None;

    bool isAttacking = false;
    bool isDownStriking = false;
    float attackProgress = 0.0f;
    bool isBlocking = false;
    bool isMistForm = false;
    bool headlampOn = false;
    bool isInvulnerable = false;
    float animTime = 0.0f;
};

class ProceduralRenderer {
public:
    ProceduralRenderer() = default;

    void DrawHero(const HeroRenderParams& p) const;
    void DrawHeroPreview(
        Vector2 centerPos,
        float uniformScale,
        const HeirCandidate& heir,
        const GameContext& ctx,
        float animTime
    ) const;

    void DrawBiomeBackground(BiomeType biome, int roomGx, int roomGy) const;
    void DrawTile(char tileCode, int col, int row, BiomeType biome, float trapTimer = 0.0f, float illusionAlpha = 1.0f) const;
    void DrawGoldenBossDoor(Vector2 bottomCenter, const std::array<bool, 5>& bossDefeated, float time) const;
    void DrawManorCastle(const GameContext& ctx, float time) const;

    // UI & Icon procedural rendering
    void DrawTraitIcon(Vector2 center, TraitType trait, float size, Color col) const;
    void DrawTraitBadge(Vector2 pos, float width, float height, TraitType trait, bool isSelected, float animTime) const;
    void DrawStatBadge(Vector2 pos, float width, float height, const char* label, int multPercent, Color accentCol) const;
    void DrawAbilitySlot(Vector2 pos, float width, float height, const char* keyText, const char* name, int manaCost, Color accentCol) const;
    void DrawManorNodeIcon(Vector2 center, ManorNodeId id, bool unlocked, bool maxed, float animTime) const;
    void DrawDetailedCharon(Vector2 pos, float animTime, bool highlight) const;

    void DrawRuneIcon(Vector2 center, RuneType rune, bool unlocked, float time) const;
    void DrawEquipSlotIcon(Vector2 center, EquipSlot slot, bool active) const;
    void DrawEquipTierBadge(Vector2 center, EquipTier tier) const;
    void DrawEnemyIcon(Vector2 center, EnemyType type, float size) const;

    static Color GetTierPrimaryColor(EquipTier tier, Color defaultCol);
    static Color GetTierSecondaryColor(EquipTier tier, Color defaultCol);
};
