// === FILE: src/world/CastleGenerator.cpp ===
#include "world/CastleGenerator.hpp"
#include "core/GameContext.hpp"
#include "data/RoomTemplates.hpp"
#include <random>
#include <queue>
#include <cmath>
#include <algorithm>

BiomeType CastleGenerator::DetermineBiomeAt(int gx, int gy) {
    // 1. CastleHamson (Центральний замок 1.0x): gx в [3..5], gy в [3..5]
    if (gx >= 3 && gx <= 5 && gy >= 3 && gy <= 5) {
        return BiomeType::CastleHamson;
    }
    // 2. ForestAbkhazia (Лісове крило праворуч 1.45x): gx >= 6
    if (gx >= 6) {
        return BiomeType::ForestAbkhazia;
    }
    // 3. TheMaya (Небесна вежа зверху 1.85x): gy <= 2 (і gx > 2, щоб лівий бік був LandOfDarkness)
    if (gy <= 2 && gx >= 3) {
        return BiomeType::TheMaya;
    }
    // 4. LandOfDarkness (Підземелля знизу gy >= 6 або ліворуч gx <= 2, складність 2.30x)
    return BiomeType::LandOfDarkness;
}

BossType CastleGenerator::GetBiomeBoss(BiomeType biome) {
    switch (biome) {
        case BiomeType::CastleHamson:   return BossType::Khidr;
        case BiomeType::ForestAbkhazia: return BossType::Alexander;
        case BiomeType::TheMaya:        return BossType::PonceDeLeon;
        case BiomeType::LandOfDarkness: return BossType::Herodotus;
        default:                        return BossType::Khidr;
    }
}

int CastleGenerator::CalculateRoomLevel(int gx, int gy, BiomeType biome, const GameContext& ctx) {
    int manhattan = std::abs(gx - Config::ENTRANCE_GRID_X) + std::abs(gy - Config::ENTRANCE_GRID_Y);
    float biomeMult = 1.0f;
    switch (biome) {
        case BiomeType::CastleHamson:   biomeMult = 1.00f; break;
        case BiomeType::ForestAbkhazia: biomeMult = 1.45f; break;
        case BiomeType::TheMaya:        biomeMult = 1.85f; break;
        case BiomeType::LandOfDarkness: biomeMult = 2.30f; break;
        default: break;
    }

    int graceRunes = ctx.CountEquippedRune(RuneType::Grace);
    float graceReduction = std::max(0.55f, 1.0f - graceRunes * 0.12f);

    int baseLvl = 1 + static_cast<int>(std::round((manhattan * 2.5f) * biomeMult * graceReduction));
    return baseLvl + (ctx.ngPlusLevel * 15);
}

void CastleGenerator::GenerateOrRestoreCastle(GameContext& ctx) {
    if (ctx.isCastleLocked && ctx.hasSavedCastle) {
        BuildRoomsFromSavedGrid(ctx);
    } else {
        GenerateFreshGraph(ctx);
        BuildRoomsFromSavedGrid(ctx);
        ctx.hasSavedCastle = true;
    }
}

void CastleGenerator::SyncRoomStateToContext(const Room& room, GameContext& ctx) {
    if (room.gridX < 0 || room.gridX >= Config::CASTLE_GRID_SIZE ||
        room.gridY < 0 || room.gridY >= Config::CASTLE_GRID_SIZE) {
        return;
    }
    SavedRoomNode& saved = ctx.savedCastle[static_cast<size_t>(room.gridY)][static_cast<size_t>(room.gridX)];
    saved.visited = room.visited;
    saved.cleared = room.cleared;
    saved.chestOpened = room.chestOpened;
}

Room& CastleGenerator::GetRoom(int gx, int gy) {
    int cx = std::clamp(gx, 0, Config::CASTLE_GRID_SIZE - 1);
    int cy = std::clamp(gy, 0, Config::CASTLE_GRID_SIZE - 1);
    return rooms_[static_cast<size_t>(cy)][static_cast<size_t>(cx)];
}

const Room& CastleGenerator::GetRoom(int gx, int gy) const {
    int cx = std::clamp(gx, 0, Config::CASTLE_GRID_SIZE - 1);
    int cy = std::clamp(gy, 0, Config::CASTLE_GRID_SIZE - 1);
    return rooms_[static_cast<size_t>(cy)][static_cast<size_t>(cx)];
}

bool CastleGenerator::IsRoomActive(int gx, int gy) const {
    if (gx < 0 || gx >= Config::CASTLE_GRID_SIZE || gy < 0 || gy >= Config::CASTLE_GRID_SIZE) {
        return false;
    }
    return rooms_[static_cast<size_t>(gy)][static_cast<size_t>(gx)].doorMask != 0;
}

void CastleGenerator::GenerateFreshGraph(GameContext& ctx) {
    static std::mt19937 rng(std::random_device{}());

    // 1. Очищуємо сітку 9x9
    for (int y = 0; y < Config::CASTLE_GRID_SIZE; ++y) {
        for (int x = 0; x < Config::CASTLE_GRID_SIZE; ++x) {
            SavedRoomNode& n = ctx.savedCastle[static_cast<size_t>(y)][static_cast<size_t>(x)];
            n = SavedRoomNode{};
            n.biome = DetermineBiomeAt(x, y);
        }
    }

    auto ActivateRoom = [&](int x, int y) {
        if (x >= 0 && x < Config::CASTLE_GRID_SIZE && y >= 0 && y < Config::CASTLE_GRID_SIZE) {
            ctx.savedCastle[static_cast<size_t>(y)][static_cast<size_t>(x)].active = true;
            ctx.savedCastle[static_cast<size_t>(y)][static_cast<size_t>(x)].biome = DetermineBiomeAt(x, y);
        }
    };

    // 2. Будуємо гарантований хребетний каркас замку (~22 клітинки), що глибоко проникає у всі 4 біоми
    // Центральний хаб CastleHamson (gx 3..5, gy 3..5)
    ActivateRoom(4, 4); // Entrance
    ActivateRoom(4, 3);
    ActivateRoom(3, 4);
    ActivateRoom(5, 4);
    ActivateRoom(4, 5);
    ActivateRoom(3, 3);
    ActivateRoom(5, 3);
    ActivateRoom(3, 5);

    // Крило ForestAbkhazia праворуч (gx >= 6)
    ActivateRoom(6, 4);
    ActivateRoom(7, 4);
    ActivateRoom(8, 4);
    ActivateRoom(7, 3);
    ActivateRoom(7, 5);
    ActivateRoom(8, 3);

    // Вежа TheMaya зверху (gy <= 2, gx >= 3)
    ActivateRoom(4, 2);
    ActivateRoom(4, 1);
    ActivateRoom(4, 0);
    ActivateRoom(3, 1);
    ActivateRoom(5, 1);
    ActivateRoom(5, 0);

    // Підземелля LandOfDarkness знизу та ліворуч (gy >= 6 або gx <= 2)
    ActivateRoom(4, 6);
    ActivateRoom(4, 7);
    ActivateRoom(4, 8);
    ActivateRoom(3, 7);
    ActivateRoom(2, 4);
    ActivateRoom(1, 4);
    ActivateRoom(2, 5);

    // 3. Додаємо 3-4 випадкові додаткові кімнати-відгалуження (загалом 28-30 активних кімнат)
    const int dx[4] = { 0, 1, 0, -1 };
    const int dy[4] = { -1, 0, 1, 0 };

    int extraAdded = 0;
    for (int attempt = 0; attempt < 60 && extraAdded < 3; ++attempt) {
        int rx = static_cast<int>(rng() % Config::CASTLE_GRID_SIZE);
        int ry = static_cast<int>(rng() % Config::CASTLE_GRID_SIZE);
        if (ctx.savedCastle[static_cast<size_t>(ry)][static_cast<size_t>(rx)].active) continue;

        bool touchesActive = false;
        for (int dir = 0; dir < 4; ++dir) {
            int nx = rx + dx[dir];
            int ny = ry + dy[dir];
            if (nx >= 0 && nx < Config::CASTLE_GRID_SIZE && ny >= 0 && ny < Config::CASTLE_GRID_SIZE) {
                if (ctx.savedCastle[static_cast<size_t>(ny)][static_cast<size_t>(nx)].active) {
                    touchesActive = true;
                    break;
                }
            }
        }
        if (touchesActive) {
            ActivateRoom(rx, ry);
            extraAdded++;
        }
    }

    // 4. Обчислюємо бітові маски дверей (NORTH=1, EAST=2, SOUTH=4, WEST=8) між усіма сусідніми активними кімнатами
    const uint8_t dirBit[4] = { DoorMask::NORTH, DoorMask::EAST, DoorMask::SOUTH, DoorMask::WEST };
    for (int y = 0; y < Config::CASTLE_GRID_SIZE; ++y) {
        for (int x = 0; x < Config::CASTLE_GRID_SIZE; ++x) {
            SavedRoomNode& node = ctx.savedCastle[static_cast<size_t>(y)][static_cast<size_t>(x)];
            if (!node.active) continue;
            uint8_t mask = 0;
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx[d];
                int ny = y + dy[d];
                if (nx >= 0 && nx < Config::CASTLE_GRID_SIZE && ny >= 0 && ny < Config::CASTLE_GRID_SIZE) {
                    if (ctx.savedCastle[static_cast<size_t>(ny)][static_cast<size_t>(nx)].active) {
                        mask |= dirBit[d];
                    }
                }
            }
            node.doorMask = mask;
        }
    }

    // 5. Розподіляємо типи кімнат у кожному з 4 біомів:
    // Гарантуємо у кожному біомі: 1 BossEntrance (на відстані >= 2), 1 FairyChest, 1 Teleporter, решта Combat та Treasure
    for (int b = 0; b < static_cast<int>(BiomeType::COUNT); ++b) {
        BiomeType curBiome = static_cast<BiomeType>(b);
        std::vector<std::pair<int, int>> candidates;

        for (int y = 0; y < Config::CASTLE_GRID_SIZE; ++y) {
            for (int x = 0; x < Config::CASTLE_GRID_SIZE; ++x) {
                if (x == Config::ENTRANCE_GRID_X && y == Config::ENTRANCE_GRID_Y) continue;
                const auto& n = ctx.savedCastle[static_cast<size_t>(y)][static_cast<size_t>(x)];
                if (n.active && n.biome == curBiome) {
                    candidates.push_back({ x, y });
                }
            }
        }

        // Сортуємо за відстанню від входу замку (найвіддаленіші — ідеальні для BossEntrance та FairyChest)
        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            int da = std::abs(a.first - Config::ENTRANCE_GRID_X) + std::abs(a.second - Config::ENTRANCE_GRID_Y);
            int db = std::abs(b.first - Config::ENTRANCE_GRID_X) + std::abs(b.second - Config::ENTRANCE_GRID_Y);
            return da > db;
        });

        for (size_t i = 0; i < candidates.size(); ++i) {
            int cx = candidates[i].first;
            int cy = candidates[i].second;
            SavedRoomNode& node = ctx.savedCastle[static_cast<size_t>(cy)][static_cast<size_t>(cx)];
            node.bossType = GetBiomeBoss(curBiome);

            if (i == 0) {
                // 1. BossEntrance (на максимальній відстані >= 2 у біомі)
                node.roomType = RoomType::BossEntrance;
                node.templateIdx = 14;
            } else if (i == 1) {
                // 2. FairyChest (з випадковою умовою FairyCondition)
                node.roomType = RoomType::FairyChest;
                node.fairyCond = static_cast<FairyCondition>(rng() % 4);
                if (node.fairyCond == FairyCondition::NoJumping) {
                    node.templateIdx = 11; // Шаблон зі сходинками без потреби стрибати
                } else if (node.fairyCond == FairyCondition::KillAll) {
                    node.templateIdx = 10; // Шаблон із ворогами
                } else {
                    node.templateIdx = 9 + static_cast<int>(rng() % 2);
                }
            } else if (i == 2) {
                // 3. Teleporter
                node.roomType = RoomType::Teleporter;
                node.templateIdx = 13;
            } else if (i == 3) {
                // 4. Treasure
                node.roomType = RoomType::Treasure;
                node.templateIdx = 12;
            } else {
                // Решта — різноманітні бойові кімнати (шаблони 1..8)
                node.roomType = RoomType::Combat;
                node.templateIdx = 1 + static_cast<int>(rng() % 8);
            }
        }
    }

    // 6. Налаштовуємо кімнату входу (4, 4)
    SavedRoomNode& entrance = ctx.savedCastle[Config::ENTRANCE_GRID_Y][Config::ENTRANCE_GRID_X];
    entrance.active = true;
    entrance.roomType = RoomType::Entrance;
    entrance.biome = BiomeType::CastleHamson;
    entrance.templateIdx = 0;
    entrance.visited = true;
    entrance.cleared = true;
}

void CastleGenerator::BuildRoomsFromSavedGrid(GameContext& ctx) {
    for (int y = 0; y < Config::CASTLE_GRID_SIZE; ++y) {
        for (int x = 0; x < Config::CASTLE_GRID_SIZE; ++x) {
            const SavedRoomNode& saved = ctx.savedCastle[static_cast<size_t>(y)][static_cast<size_t>(x)];
            Room& rm = rooms_[static_cast<size_t>(y)][static_cast<size_t>(x)];
            if (!saved.active) {
                rm = Room{};
                rm.gridX = x;
                rm.gridY = y;
                rm.doorMask = 0;
                continue;
            }
            int lvl = CalculateRoomLevel(x, y, saved.biome, ctx);
            rm.BuildFromNode(x, y, saved, lvl, ctx);
        }
    }
}