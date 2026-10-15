// === FILE: src/world/CastleGenerator.cpp ===
#include "world/CastleGenerator.hpp"
#include "core/GameContext.hpp"
#include "data/RoomTemplates.hpp"
#include <random>
#include <queue>
#include <cmath>
#include <algorithm>
#include <chrono>

namespace {

    inline uint8_t OppositeDoor(uint8_t d) {
        switch (d) {
            case DoorMask::NORTH: return DoorMask::SOUTH;
            case DoorMask::SOUTH: return DoorMask::NORTH;
            case DoorMask::EAST:  return DoorMask::WEST;
            case DoorMask::WEST:  return DoorMask::EAST;
            default: return 0;
        }
    }

    struct DirInfo {
        int dx;
        int dy;
        uint8_t bit;
    };

    const DirInfo DIRS[4] = {
        {  0, -1, DoorMask::NORTH },
        {  1,  0, DoorMask::EAST  },
        {  0,  1, DoorMask::SOUTH },
        { -1,  0, DoorMask::WEST  }
    };

    struct FrontierDoor {
        int targetGx = 0;
        int targetGy = 0;
        uint8_t requiredDoor = 0; // Door that the new room must have on its side facing (targetGx, targetGy)
        BiomeType biome = BiomeType::CastleHamson;
    };

} // anonymous namespace

CastleGenerator::CastleGenerator() {
    for (int y = 0; y < Config::CASTLE_GRID_H; ++y) {
        for (int x = 0; x < Config::CASTLE_GRID_W; ++x) {
            grid_[y][x] = GridCell{};
        }
    }
}

BiomeType CastleGenerator::DetermineBiomeAt(int gx, int gy) {
    if (gy <= 25) return BiomeType::TheMaya;
    if (gy >= 39) return BiomeType::LandOfDarkness;
    if (gx >= 31) return BiomeType::ForestAbkhazia;
    return BiomeType::CastleHamson;
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
    int manhattan = std::abs(gx - Config::ENTRANCE_GX) + std::abs(gy - Config::ENTRANCE_GY);
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

    int baseLvl = 1 + static_cast<int>(std::round((manhattan * 1.5f) * biomeMult * graceReduction));
    return baseLvl + (ctx.ngPlusLevel * 15);
}

void CastleGenerator::GenerateOrRestoreCastle(GameContext& ctx) {
    if (ctx.isCastleLocked && ctx.hasSavedCastle && !ctx.savedRooms.empty()) {
        BuildRoomsFromSavedGrid(ctx);
    } else {
        GenerateFreshGraph(ctx);
        BuildRoomsFromSavedGrid(ctx);
        ctx.hasSavedCastle = true;
    }
}

void CastleGenerator::SyncRoomStateToContext(const Room& room, GameContext& ctx) {
    if (room.roomId >= 0 && room.roomId < static_cast<int>(ctx.savedRooms.size())) {
        SavedRoomNode& saved = ctx.savedRooms[static_cast<size_t>(room.roomId)];
        saved.visited = room.visited;
        saved.cleared = room.cleared;
        saved.chestOpened = room.chestOpened;
    }
}

Room& CastleGenerator::GetRoom(int gx, int gy) {
    if (gx < 0 || gx >= Config::CASTLE_GRID_W || gy < 0 || gy >= Config::CASTLE_GRID_H) {
        return dummyRoom_;
    }
    int rid = grid_[gy][gx].roomId;
    if (rid >= 0 && rid < static_cast<int>(rooms_.size())) {
        return rooms_[static_cast<size_t>(rid)];
    }
    return dummyRoom_;
}

const Room& CastleGenerator::GetRoom(int gx, int gy) const {
    if (gx < 0 || gx >= Config::CASTLE_GRID_W || gy < 0 || gy >= Config::CASTLE_GRID_H) {
        return dummyRoom_;
    }
    int rid = grid_[gy][gx].roomId;
    if (rid >= 0 && rid < static_cast<int>(rooms_.size())) {
        return rooms_[static_cast<size_t>(rid)];
    }
    return dummyRoom_;
}

Room* CastleGenerator::GetRoomById(int roomId) {
    if (roomId >= 0 && roomId < static_cast<int>(rooms_.size())) {
        return &rooms_[static_cast<size_t>(roomId)];
    }
    return nullptr;
}

const Room* CastleGenerator::GetRoomById(int roomId) const {
    if (roomId >= 0 && roomId < static_cast<int>(rooms_.size())) {
        return &rooms_[static_cast<size_t>(roomId)];
    }
    return nullptr;
}

bool CastleGenerator::IsRoomActive(int gx, int gy) const {
    if (gx < 0 || gx >= Config::CASTLE_GRID_W || gy < 0 || gy >= Config::CASTLE_GRID_H) {
        return false;
    }
    return grid_[gy][gx].state == CellState::Filled && grid_[gy][gx].roomId >= 0;
}

const GridCell& CastleGenerator::GetGridCell(int gx, int gy) const {
    static const GridCell emptyCell{};
    if (gx < 0 || gx >= Config::CASTLE_GRID_W || gy < 0 || gy >= Config::CASTLE_GRID_H) {
        return emptyCell;
    }
    return grid_[gy][gx];
}

void CastleGenerator::GenerateFreshGraph(GameContext& ctx) {
    uint64_t timeSeed = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::random_device rd;
    std::mt19937 rng(static_cast<uint32_t>(rd() ^ timeSeed));

    rooms_.clear();
    for (int y = 0; y < Config::CASTLE_GRID_H; ++y) {
        for (int x = 0; x < Config::CASTLE_GRID_W; ++x) {
            grid_[y][x] = GridCell{};
            grid_[y][x].biome = DetermineBiomeAt(x, y);
        }
    }

    // Template categories from RoomTemplates.hpp
    // 0: Entrance
    // 1..8: 1x1 Combat
    // 9..11: FairyChest
    // 12..13: Treasure / Shrine
    // 14: Teleporter
    // 15: BossEntrance
    // 16: Secret
    // 17..20: 2x1 Combat
    // 21..24: 1x2 Combat
    // 25..27: 2x2 Combat
    // 28..32: Boss Arenas (Khidr, Alexander, Ponce, Herodotus, Johannes)

    const std::vector<int> pool1x1 = { 1, 2, 3, 4, 5, 6, 7, 8 };
    const std::vector<int> pool2x1 = { 17, 18, 19, 20 };
    const std::vector<int> pool1x2 = { 21, 22, 23, 24 };
    const std::vector<int> pool2x2 = { 25, 26, 27 };

    auto CanPlaceTemplateAt = [&](int tmplIdx, int originGx, int originGy, BiomeType targetBiome) -> bool {
        if (tmplIdx < 0 || tmplIdx >= static_cast<int>(RoomTemplates::TEMPLATES.size())) return false;
        const auto& tmpl = RoomTemplates::TEMPLATES[static_cast<size_t>(tmplIdx)];

        if (originGx < 2 || originGx + tmpl.widthCells > Config::CASTLE_GRID_W - 2) return false;
        if (originGy < 2 || originGy + tmpl.heightCells > Config::CASTLE_GRID_H - 2) return false;

        // Check each subcell
        for (int ry = 0; ry < tmpl.heightCells; ++ry) {
            for (int rx = 0; rx < tmpl.widthCells; ++rx) {
                int gx = originGx + rx;
                int gy = originGy + ry;
                if (DetermineBiomeAt(gx, gy) != targetBiome) return false;
                const GridCell& cell = grid_[gy][gx];
                if (cell.state == CellState::Filled) return false;
                // If cell requires doors, this template MUST provide them
                if (cell.state == CellState::NeedsDoor) {
                    if ((tmpl.possibleDoors[ry][rx] & cell.requiredDoors) != cell.requiredDoors) {
                        return false;
                    }
                }
            }
        }

        // Check neighbor filled cells: cannot connect door into a filled cell that has NO matching door
        for (int ry = 0; ry < tmpl.heightCells; ++ry) {
            for (int rx = 0; rx < tmpl.widthCells; ++rx) {
                int gx = originGx + rx;
                int gy = originGy + ry;
                for (const auto& d : DIRS) {
                    int nx = gx + d.dx;
                    int ny = gy + d.dy;
                    // Skip if within the candidate template itself
                    if (nx >= originGx && nx < originGx + tmpl.widthCells &&
                        ny >= originGy && ny < originGy + tmpl.heightCells) {
                        continue;
                    }
                    if (nx < 0 || nx >= Config::CASTLE_GRID_W || ny < 0 || ny >= Config::CASTLE_GRID_H) continue;
                }
            }
        }
        return true;
    };

    auto PlaceRoom = [&](int tmplIdx, int originGx, int originGy, BiomeType biome, RoomType roomType,
                         BossType bossType, FairyCondition fairyCond, std::vector<FrontierDoor>& frontier) -> int {
        const auto& tmpl = RoomTemplates::TEMPLATES[static_cast<size_t>(tmplIdx)];
        int newId = static_cast<int>(rooms_.size());

        Room rm;
        rm.roomId = newId;
        rm.originGx = originGx;
        rm.originGy = originGy;
        rm.widthCells = tmpl.widthCells;
        rm.heightCells = tmpl.heightCells;
        rm.totalCols = tmpl.widthCells * Config::CELL_COLS;
        rm.totalRows = tmpl.heightCells * Config::CELL_ROWS;
        rm.roomType = roomType;
        rm.biome = biome;
        rm.templateIdx = tmplIdx;
        rm.bossType = bossType;
        rm.fairyCond = fairyCond;
        rm.isReversed = (roomType == RoomType::Combat && (rng() % 2 == 1));

        // Mark grid cells
        for (int ry = 0; ry < tmpl.heightCells; ++ry) {
            for (int rx = 0; rx < tmpl.widthCells; ++rx) {
                int gx = originGx + rx;
                int gy = originGy + ry;
                GridCell& cell = grid_[gy][gx];
                cell.state = CellState::Filled;
                cell.roomId = newId;
                cell.relX = rx;
                cell.relY = ry;
                cell.biome = biome;
            }
        }

        // Add outward doors to frontier
        for (int ry = 0; ry < tmpl.heightCells; ++ry) {
            for (int rx = 0; rx < tmpl.widthCells; ++rx) {
                int gx = originGx + rx;
                int gy = originGy + ry;
                for (const auto& d : DIRS) {
                    if (tmpl.possibleDoors[ry][rx] & d.bit) {
                        int nx = gx + d.dx;
                        int ny = gy + d.dy;
                        if (nx >= originGx && nx < originGx + tmpl.widthCells &&
                            ny >= originGy && ny < originGy + tmpl.heightCells) {
                            continue; // Internal door
                        }
                        if (nx < 2 || nx >= Config::CASTLE_GRID_W - 2 || ny < 2 || ny >= Config::CASTLE_GRID_H - 2) {
                            continue;
                        }
                        GridCell& nCell = grid_[ny][nx];
                        if (nCell.state == CellState::Empty) {
                            nCell.state = CellState::NeedsDoor;
                            nCell.requiredDoors |= OppositeDoor(d.bit);
                            frontier.push_back({ nx, ny, OppositeDoor(d.bit), DetermineBiomeAt(nx, ny) });
                        } else if (nCell.state == CellState::NeedsDoor) {
                            nCell.requiredDoors |= OppositeDoor(d.bit);
                        }
                    }
                }
            }
        }

        rooms_.push_back(std::move(rm));
        return newId;
    };

    // Helper to grow a biome's rooms
    auto GrowBiome = [&](BiomeType bType, int targetCount, std::vector<FrontierDoor>& frontier) {
        BossType bBoss = GetBiomeBoss(bType);
        int roomsInBiome = 0;
        for (const auto& rm : rooms_) {
            if (rm.biome == bType) roomsInBiome++;
        }

        bool placedTeleporter = false;
        bool placedFairy = false;
        bool placedTreasure = false;
        bool placedSecret = false;
        int multiCellCount = 0;

        // Filter or prioritize frontier doors belonging to this biome
        auto GetBiomeFrontierIdx = [&]() -> int {
            std::vector<int> valid;
            for (size_t i = 0; i < frontier.size(); ++i) {
                if (frontier[i].biome == bType) {
                    valid.push_back(static_cast<int>(i));
                }
            }
            if (valid.empty()) return -1;
            return valid[rng() % valid.size()];
        };

        int maxAttempts = 500;
        while (roomsInBiome < targetCount && maxAttempts-- > 0) {
            int fIdx = GetBiomeFrontierIdx();
            if (fIdx < 0) {
                // If frontier is exhausted, look for any filled room in this biome with an empty neighbor
                bool foundNewDoor = false;
                for (const auto& rm : rooms_) {
                    if (rm.biome != bType) continue;
                    const auto& tmpl = RoomTemplates::TEMPLATES[static_cast<size_t>(rm.templateIdx)];
                    for (int ry = 0; ry < tmpl.heightCells && !foundNewDoor; ++ry) {
                        for (int rx = 0; rx < tmpl.widthCells && !foundNewDoor; ++rx) {
                            int gx = rm.originGx + rx;
                            int gy = rm.originGy + ry;
                            for (const auto& d : DIRS) {
                                if (tmpl.possibleDoors[ry][rx] & d.bit) {
                                    int nx = gx + d.dx;
                                    int ny = gy + d.dy;
                                    if (nx >= 2 && nx < Config::CASTLE_GRID_W - 2 &&
                                        ny >= 2 && ny < Config::CASTLE_GRID_H - 2) {
                                        if (grid_[ny][nx].state == CellState::Empty && DetermineBiomeAt(nx, ny) == bType) {
                                            grid_[ny][nx].state = CellState::NeedsDoor;
                                            grid_[ny][nx].requiredDoors |= OppositeDoor(d.bit);
                                            frontier.push_back({ nx, ny, OppositeDoor(d.bit), bType });
                                            foundNewDoor = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (foundNewDoor) break;
                }
                if (!foundNewDoor) break; // Cannot grow further
                continue;
            }

            FrontierDoor fd = frontier[static_cast<size_t>(fIdx)];
            frontier.erase(frontier.begin() + fIdx);

            if (grid_[fd.targetGy][fd.targetGx].state == CellState::Filled) {
                continue;
            }

            // Decide room role
            RoomType rType = RoomType::Combat;
            int chosenTmpl = -1;
            FairyCondition fCond = FairyCondition::KillAll;

            if (!placedTeleporter && roomsInBiome >= 8) {
                rType = RoomType::Teleporter;
                chosenTmpl = 14;
            } else if (!placedFairy && roomsInBiome >= 5) {
                rType = RoomType::FairyChest;
                int fSub = static_cast<int>(rng() % 3);
                chosenTmpl = 9 + fSub;
                fCond = static_cast<FairyCondition>(fSub);
            } else if (!placedTreasure && roomsInBiome >= 12) {
                rType = RoomType::Treasure;
                chosenTmpl = (rng() % 2 == 0) ? 12 : 13;
            } else if (!placedSecret && roomsInBiome >= 16) {
                rType = RoomType::Secret;
                chosenTmpl = 16;
            } else {
                // Combat room: targeting 35-40% multi-cell after fallbacks
                bool wantMulti = (rng() % 100 < 65);
                if (wantMulti) {
                    int multiType = static_cast<int>(rng() % 3);
                    if (multiType == 0) {
                        chosenTmpl = pool2x1[rng() % pool2x1.size()];
                    } else if (multiType == 1) {
                        chosenTmpl = pool1x2[rng() % pool1x2.size()];
                    } else {
                        chosenTmpl = pool2x2[rng() % pool2x2.size()];
                    }
                } else {
                    chosenTmpl = pool1x1[rng() % pool1x1.size()];
                }
            }

            // Try to place chosen template
            const auto& tmpl = RoomTemplates::TEMPLATES[static_cast<size_t>(chosenTmpl)];
            std::vector<std::pair<int, int>> validAnchors;
            for (int ry = 0; ry < tmpl.heightCells; ++ry) {
                for (int rx = 0; rx < tmpl.widthCells; ++rx) {
                    if (tmpl.possibleDoors[ry][rx] & fd.requiredDoor) {
                        int origGx = fd.targetGx - rx;
                        int origGy = fd.targetGy - ry;
                        if (CanPlaceTemplateAt(chosenTmpl, origGx, origGy, bType)) {
                            validAnchors.push_back({ rx, ry });
                        }
                    }
                }
            }

            // If template didn't fit and it was a special room or multi-cell, try fallback
            if (validAnchors.empty()) {
                if (rType != RoomType::Combat) {
                    // Try another 1x1 template or re-add frontier door
                    frontier.push_back(fd);
                    continue;
                }
                // Fallback to 1x1 combat
                for (int tIdx : pool1x1) {
                    int origGx = fd.targetGx;
                    int origGy = fd.targetGy;
                    if (CanPlaceTemplateAt(tIdx, origGx, origGy, bType)) {
                        chosenTmpl = tIdx;
                        validAnchors.push_back({ 0, 0 });
                        break;
                    }
                }
            }

            if (validAnchors.empty()) {
                continue;
            }

            auto [arx, ary] = validAnchors[rng() % validAnchors.size()];
            int origGx = fd.targetGx - arx;
            int origGy = fd.targetGy - ary;

            PlaceRoom(chosenTmpl, origGx, origGy, bType, rType, bBoss, fCond, frontier);
            roomsInBiome++;

            if (rType == RoomType::Teleporter) placedTeleporter = true;
            else if (rType == RoomType::FairyChest) placedFairy = true;
            else if (rType == RoomType::Treasure) placedTreasure = true;
            else if (rType == RoomType::Secret) placedSecret = true;
            else if (tmpl.widthCells > 1 || tmpl.heightCells > 1) multiCellCount++;
        }

        // Guarantee 1 BossEntrance per biome: pick the room furthest from Entrance/Gateway in this biome
        int maxDist = -1;
        int furthestRoomId = -1;
        for (const auto& rm : rooms_) {
            if (rm.biome != bType || rm.roomType == RoomType::Entrance || rm.roomType == RoomType::Teleporter) continue;
            int d = std::abs(rm.originGx - Config::ENTRANCE_GX) + std::abs(rm.originGy - Config::ENTRANCE_GY);
            if (d > maxDist) {
                maxDist = d;
                furthestRoomId = rm.roomId;
            }
        }
        if (furthestRoomId >= 0) {
            Room& bRoom = rooms_[static_cast<size_t>(furthestRoomId)];
            bRoom.roomType = RoomType::BossEntrance;
            bRoom.bossType = bBoss;
            // Use 1x1 boss entrance template (15) if 1x1, or keep template and mark bossType
            if (bRoom.widthCells == 1 && bRoom.heightCells == 1) {
                bRoom.templateIdx = 15;
            }
        }
    };

    // 1. Place CastleHamson Entrance at (24, 32)
    std::vector<FrontierDoor> globalFrontier;
    PlaceRoom(0, Config::ENTRANCE_GX, Config::ENTRANCE_GY, BiomeType::CastleHamson,
              RoomType::Entrance, BossType::Khidr, FairyCondition::KillAll, globalFrontier);
    rooms_[0].visited = true;
    rooms_[0].cleared = true;

    // Helper to connect gateway corridors from Entrance (24, 32) to borders
    auto ConnectCorridor = [&](int startX, int startY, int endX, int endY) {
        int cx = startX;
        int cy = startY;
        while (true) {
            if (grid_[cy][cx].state != CellState::Filled) {
                PlaceRoom(1, cx, cy, BiomeType::CastleHamson, RoomType::Combat,
                          BossType::Khidr, FairyCondition::KillAll, globalFrontier);
            }
            if (cx == endX && cy == endY) break;
            if (cx < endX) cx++;
            else if (cx > endX) cx--;
            else if (cy < endY) cy++;
            else if (cy > endY) cy--;
        }
    };

    ConnectCorridor(26, 32, 30, 32); // East to Forest boundary
    ConnectCorridor(26, 32, 26, 26); // North to Maya boundary
    ConnectCorridor(26, 32, 26, 38); // South to Darkness boundary

    // Explicitly seed gateway doors for Forest, Maya, Darkness
    grid_[32][31].state = CellState::NeedsDoor;
    grid_[32][31].requiredDoors |= DoorMask::WEST;
    globalFrontier.push_back({ 31, 32, DoorMask::WEST, BiomeType::ForestAbkhazia });

    grid_[25][26].state = CellState::NeedsDoor;
    grid_[25][26].requiredDoors |= DoorMask::SOUTH;
    globalFrontier.push_back({ 26, 25, DoorMask::SOUTH, BiomeType::TheMaya });

    grid_[39][26].state = CellState::NeedsDoor;
    grid_[39][26].requiredDoors |= DoorMask::NORTH;
    globalFrontier.push_back({ 26, 39, DoorMask::NORTH, BiomeType::LandOfDarkness });

    // 2. Grow Castle Hamson (24-26 rooms)
    GrowBiome(BiomeType::CastleHamson, 24 + static_cast<int>(rng() % 3), globalFrontier);

    // 3. Grow Forest (24-26 rooms)
    GrowBiome(BiomeType::ForestAbkhazia, 24 + static_cast<int>(rng() % 3), globalFrontier);

    // 4. Grow Maya (24-26 rooms)
    GrowBiome(BiomeType::TheMaya, 24 + static_cast<int>(rng() % 3), globalFrontier);

    // 5. Grow Darkness (24-26 rooms)
    GrowBiome(BiomeType::LandOfDarkness, 24 + static_cast<int>(rng() % 3), globalFrontier);

    // 7. Connect doors and cap all hanging doors
    for (auto& rm : rooms_) {
        const auto& tmpl = RoomTemplates::TEMPLATES[static_cast<size_t>(rm.templateIdx)];
        uint8_t totalDoorMask = 0;

        for (int ry = 0; ry < rm.heightCells; ++ry) {
            for (int rx = 0; rx < rm.widthCells; ++rx) {
                rm.subCellDoors[ry][rx] = 0;
                int gx = rm.originGx + rx;
                int gy = rm.originGy + ry;

                for (const auto& d : DIRS) {
                    if (tmpl.possibleDoors[ry][rx] & d.bit) {
                        int nx = gx + d.dx;
                        int ny = gy + d.dy;
                        if (nx >= rm.originGx && nx < rm.originGx + rm.widthCells &&
                            ny >= rm.originGy && ny < rm.originGy + rm.heightCells) {
                            continue; // Internal door
                        }
                        if (nx >= 0 && nx < Config::CASTLE_GRID_W && ny >= 0 && ny < Config::CASTLE_GRID_H) {
                            const GridCell& nCell = grid_[ny][nx];
                            if (nCell.state == CellState::Filled && nCell.roomId >= 0) {
                                const Room& nbr = rooms_[static_cast<size_t>(nCell.roomId)];
                                const auto& nbrTmpl = RoomTemplates::TEMPLATES[static_cast<size_t>(nbr.templateIdx)];
                                if (nbrTmpl.possibleDoors[nCell.relY][nCell.relX] & OppositeDoor(d.bit)) {
                                    rm.subCellDoors[ry][rx] |= d.bit;
                                    totalDoorMask |= d.bit;
                                }
                            }
                        }
                    }
                }
            }
        }
        rm.doorMask = totalDoorMask;
    }

    // 8. Populate ctx.savedRooms
    ctx.savedRooms.clear();
    ctx.savedRooms.reserve(rooms_.size());
    for (const auto& rm : rooms_) {
        SavedRoomNode node;
        node.active = true;
        node.roomId = rm.roomId;
        node.originGx = rm.originGx;
        node.originGy = rm.originGy;
        node.widthCells = rm.widthCells;
        node.heightCells = rm.heightCells;
        node.doorMask = rm.doorMask;
        for (int sy = 0; sy < 2; ++sy) {
            for (int sx = 0; sx < 2; ++sx) {
                node.subCellDoors[sy][sx] = rm.subCellDoors[sy][sx];
            }
        }
        node.roomType = rm.roomType;
        node.biome = rm.biome;
        node.templateIdx = rm.templateIdx;
        node.isReversed = rm.isReversed;
        node.fairyCond = rm.fairyCond;
        node.bossType = rm.bossType;
        node.visited = rm.visited;
        node.cleared = rm.cleared;
        node.chestOpened = rm.chestOpened;
        ctx.savedRooms.push_back(node);
    }
}

void CastleGenerator::BuildRoomsFromSavedGrid(GameContext& ctx) {
    rooms_.clear();
    for (int y = 0; y < Config::CASTLE_GRID_H; ++y) {
        for (int x = 0; x < Config::CASTLE_GRID_W; ++x) {
            grid_[y][x] = GridCell{};
            grid_[y][x].biome = DetermineBiomeAt(x, y);
        }
    }

    rooms_.reserve(ctx.savedRooms.size());
    for (const SavedRoomNode& node : ctx.savedRooms) {
        if (!node.active) continue;
        Room rm;
        int lvl = CalculateRoomLevel(node.originGx, node.originGy, node.biome, ctx);
        rm.BuildFromNode(node.originGx, node.originGy, node, lvl, ctx);

        for (int ry = 0; ry < node.heightCells; ++ry) {
            for (int rx = 0; rx < node.widthCells; ++rx) {
                int gx = node.originGx + rx;
                int gy = node.originGy + ry;
                if (gx >= 0 && gx < Config::CASTLE_GRID_W && gy >= 0 && gy < Config::CASTLE_GRID_H) {
                    GridCell& cell = grid_[gy][gx];
                    cell.state = CellState::Filled;
                    cell.roomId = node.roomId;
                    cell.relX = rx;
                    cell.relY = ry;
                    cell.biome = node.biome;
                }
            }
        }
        rooms_.push_back(std::move(rm));
    }
}