// === FILE: src/world/CastleGenerator.hpp ===
#pragma once

#include "core/Config.hpp"
#include "core/Types.hpp"
#include "world/Room.hpp"
#include <array>
#include <vector>

class GameContext;

class CastleGenerator {
public:
    CastleGenerator() = default;

    // Генерує новий граф замку 9x9 або відновлює збережений замок при активованому Архітекторі
    void GenerateOrRestoreCastle(GameContext& ctx);

    // Синхронізує стан поточної кімнати (відвідано/зачищено/скрині відкрито) назад у GameContext::savedCastle
    void SyncRoomStateToContext(const Room& room, GameContext& ctx);

    Room& GetRoom(int gx, int gy);
    const Room& GetRoom(int gx, int gy) const;
    bool IsRoomActive(int gx, int gy) const;

    static BiomeType DetermineBiomeAt(int gx, int gy);
    static BossType GetBiomeBoss(BiomeType biome);
    static int CalculateRoomLevel(int gx, int gy, BiomeType biome, const GameContext& ctx);

private:
    void GenerateFreshGraph(GameContext& ctx);
    void BuildRoomsFromSavedGrid(GameContext& ctx);

private:
    std::array<std::array<Room, Config::CASTLE_GRID_SIZE>, Config::CASTLE_GRID_SIZE> rooms_{};
};