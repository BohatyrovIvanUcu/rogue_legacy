// === FILE: src/world/Room.hpp ===
#pragma once

#include "entities/Entity.hpp"
#include "entities/Enemy.hpp"
#include "entities/Boss.hpp"
#include "entities/Interactable.hpp"
#include "entities/Projectile.hpp"
#include <array>
#include <vector>
#include <string>

class GameContext;
class Player;

class Room : public ITileMap {
public:
    int roomId = -1;
    int originGx = Config::ENTRANCE_GX;
    int originGy = Config::ENTRANCE_GY;
    int gridX = Config::ENTRANCE_GX;
    int gridY = Config::ENTRANCE_GY;

    int widthCells = 1;
    int heightCells = 1;
    int totalCols = Config::CELL_COLS;
    int totalRows = Config::CELL_ROWS;
    uint8_t subCellDoors[2][2]{};

    RoomType roomType = RoomType::Entrance;
    BiomeType biome = BiomeType::CastleHamson;
    uint8_t doorMask = 0;
    int templateIdx = 0;
    bool isReversed = false;
    int roomLevel = 1;

    FairyCondition fairyCond = FairyCondition::NoDamage;
    bool fairyTrialActive = false;
    bool fairyTrialFailed = false;
    bool fairyTrialCompleted = false;
    float fairyTimer = 7.5f;

    BossType bossType = BossType::Khidr;
    bool isBossArenaActive = false;
    bool bossDefeatedInArena = false;

    bool visited = false;
    bool cleared = false;
    bool chestOpened = false;

    std::vector<std::string> tiles{};
    float trapTimer = 0.0f;

    std::vector<Enemy> enemies{};
    std::vector<Interactable> interactables{};
    std::vector<Projectile> projectiles{};
    std::vector<Rectangle> bookcaseRects{};
    Boss activeBoss{};
    bool hasActiveBoss = false;

    // Події для CastlePlayState
    bool requestOpenTeleporterMap = false;
    bool requestEnterBossArena = false;
    BossType requestedBossArenaType = BossType::Khidr;
    bool requestExitBossArena = false;
    bool requestFinalVictory = false;

public:
    Room() = default;

    // Реалізація інтерфейсу ITileMap
    char GetTileAt(int col, int row) const override;
    float GetTrapTimer() const override { return trapTimer; }

    int GetWidthCells() const override { return widthCells; }
    int GetHeightCells() const override { return heightCells; }
    int GetTotalCols() const override { return totalCols; }
    int GetTotalRows() const override { return totalRows; }
    int GetPixelWidth() const override { return totalCols * Config::TILE_SIZE; }
    int GetPixelHeight() const override { return totalRows * Config::TILE_SIZE; }

    void BuildFromNode(
        int gx,
        int gy,
        const SavedRoomNode& node,
        int calculatedRoomLevel,
        const GameContext& ctx
    );

    void EnterBossArena(BossType targetBoss, int bossLvl, GameContext& ctx, Player& player);
    void OnPlayerEnterRoom(GameContext& ctx, Player& player);

    void Update(float dt, GameContext& ctx, Player& player);
    void Draw(const GameContext& ctx, const Player& player) const;

    // Окремі проходи графічного пайплайну
    void DrawBackgroundAndTiles(const GameContext& ctx, const Player& player) const;
    void DrawProps(const GameContext& ctx, const Player& player) const;
    void DrawEntities(const GameContext& ctx, const Player& player) const;
    void DrawLights(const GameContext& ctx, const Player& player) const;
    void DrawEmissive(const GameContext& ctx, const Player& player) const;
    void DrawWorldUi(const GameContext& ctx, const Player& player, const Camera2D& cam) const;

    std::vector<Vector2> GetActiveTorchScreenPositions() const;

private:
    void ApplyDoorMasksToTiles();
    void SanitizeRoomGeometry();
    void SpawnEntitiesFromTemplate(const GameContext& ctx);
    void EvaluateFairyTrial(float dt, GameContext& ctx, Player& player);
    void HandleCombatCollisions(GameContext& ctx, Player& player);
};