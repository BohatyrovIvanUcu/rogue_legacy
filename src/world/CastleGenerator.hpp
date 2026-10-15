// === FILE: src/world/CastleGenerator.hpp ===
#pragma once

#include "core/Config.hpp"
#include "core/Types.hpp"
#include "world/Room.hpp"
#include <array>
#include <vector>

class GameContext;

enum class CellState : uint8_t {
    Empty,
    NeedsDoor,
    Filled
};

struct GridCell {
    CellState state = CellState::Empty;
    int roomId = -1;
    int relX = 0;
    int relY = 0;
    BiomeType biome = BiomeType::CastleHamson;
    uint8_t requiredDoors = 0;
    uint8_t forbiddenDoors = 0;
};

class CastleGenerator {
public:
    CastleGenerator();

    // Генерує новий замок ~100 кімнат (4 біоми по 23-27 кімнат) або відновлює збережений замок при активованому Архітекторі
    void GenerateOrRestoreCastle(GameContext& ctx);

    // Синхронізує стан поточної кімнати (відвідано/зачищено/скрині відкрито) назад у GameContext::savedRooms
    void SyncRoomStateToContext(const Room& room, GameContext& ctx);

    Room& GetRoom(int gx, int gy);
    const Room& GetRoom(int gx, int gy) const;
    Room* GetRoomById(int roomId);
    const Room* GetRoomById(int roomId) const;
    bool IsRoomActive(int gx, int gy) const;

    const GridCell& GetGridCell(int gx, int gy) const;
    const std::vector<Room>& GetAllRooms() const { return rooms_; }
    std::vector<Room>& GetAllRooms() { return rooms_; }

    static BiomeType DetermineBiomeAt(int gx, int gy);
    static BossType GetBiomeBoss(BiomeType biome);
    static int CalculateRoomLevel(int gx, int gy, BiomeType biome, const GameContext& ctx);

private:
    void GenerateFreshGraph(GameContext& ctx);
    void BuildRoomsFromSavedGrid(GameContext& ctx);

private:
    std::vector<Room> rooms_{};
    GridCell grid_[Config::CASTLE_GRID_H][Config::CASTLE_GRID_W]{};
    mutable Room dummyRoom_{};
};