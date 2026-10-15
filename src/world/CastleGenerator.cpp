// === FILE: src/world/CastleGenerator.cpp ===
#include "world/CastleGenerator.hpp"
#include "core/GameContext.hpp"
#include "data/RoomTemplates.hpp"
#include <random>
#include <queue>
#include <deque>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iostream>

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

        // Запобігання утворенню 1D-колон/стовпчиків у межах біому (Column Prevention)
        for (int rx = 0; rx < tmpl.widthCells; ++rx) {
            int gx = originGx + rx;
            int upCount = 0;
            for (int y = originGy - 1; y >= 0; --y) {
                if (grid_[y][gx].state == CellState::Filled && grid_[y][gx].biome == targetBiome) upCount++;
                else break;
            }
            int downCount = 0;
            for (int y = originGy + tmpl.heightCells; y < Config::CASTLE_GRID_H; ++y) {
                if (grid_[y][gx].state == CellState::Filled && grid_[y][gx].biome == targetBiome) downCount++;
                else break;
            }
            int totalVertSpan = upCount + tmpl.heightCells + downCount;
            if (totalVertSpan >= 4) {
                bool hasLateral = false;
                for (int y = originGy - upCount; y < originGy + tmpl.heightCells + downCount; ++y) {
                    if (gx > 0 && grid_[y][gx - 1].state == CellState::Filled && grid_[y][gx - 1].biome == targetBiome) hasLateral = true;
                    if (gx + 1 < Config::CASTLE_GRID_W && grid_[y][gx + 1].state == CellState::Filled && grid_[y][gx + 1].biome == targetBiome) hasLateral = true;
                    if (hasLateral) break;
                }
                if (tmpl.widthCells > 1) hasLateral = true;

                if (!hasLateral || totalVertSpan >= 5) {
                    return false;
                }
            }
        }

        // Запобігання утворенню 1D-горизонтальних тунелів у межах біому >= 5
        for (int ry = 0; ry < tmpl.heightCells; ++ry) {
            int gy = originGy + ry;
            int leftCount = 0;
            for (int x = originGx - 1; x >= 0; --x) {
                if (grid_[gy][x].state == CellState::Filled && grid_[gy][x].biome == targetBiome) leftCount++;
                else break;
            }
            int rightCount = 0;
            for (int x = originGx + tmpl.widthCells; x < Config::CASTLE_GRID_W; ++x) {
                if (grid_[gy][x].state == CellState::Filled && grid_[gy][x].biome == targetBiome) rightCount++;
                else break;
            }
            int totalHorizSpan = leftCount + tmpl.widthCells + rightCount;
            if (totalHorizSpan >= 5) {
                return false;
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
        std::deque<int> recentTemplates;

        auto GetNeighborTemplates = [&](int gx, int gy) -> std::vector<int> {
            std::vector<int> nTmpls;
            for (const auto& d : DIRS) {
                int nx = gx + d.dx;
                int ny = gy + d.dy;
                if (nx >= 0 && nx < Config::CASTLE_GRID_W && ny >= 0 && ny < Config::CASTLE_GRID_H) {
                    if (grid_[ny][nx].state == CellState::Filled && grid_[ny][nx].roomId >= 0) {
                        nTmpls.push_back(rooms_[static_cast<size_t>(grid_[ny][nx].roomId)].templateIdx);
                    }
                }
            }
            return nTmpls;
        };

        auto PickDiverseTemplate = [&](const std::vector<int>& pool, int gx, int gy) -> int {
            std::vector<int> nTmpls = GetNeighborTemplates(gx, gy);
            std::vector<int> candidates;
            for (int t : pool) {
                bool inRecent = std::find(recentTemplates.begin(), recentTemplates.end(), t) != recentTemplates.end();
                bool inNeighbor = std::find(nTmpls.begin(), nTmpls.end(), t) != nTmpls.end();
                if (!inRecent && !inNeighbor) {
                    candidates.push_back(t);
                }
            }
            if (candidates.empty()) {
                for (int t : pool) {
                    bool inRecent = std::find(recentTemplates.begin(), recentTemplates.end(), t) != recentTemplates.end();
                    if (!inRecent) candidates.push_back(t);
                }
            }
            if (candidates.empty()) candidates = pool;
            return candidates[rng() % candidates.size()];
        };

        // 2D-гілкування: чергування напрямків та запобігання утворенню 1D-колон/ліній
        auto GetBiomeFrontierIdx = [&]() -> int {
            std::vector<int> matching;
            std::vector<int> allValid;
            bool preferVertical = (roomsInBiome % 2 == 1);

            int colCount[Config::CASTLE_GRID_W]{};
            int rowCount[Config::CASTLE_GRID_H]{};
            for (const auto& rm : rooms_) {
                if (rm.biome == bType) {
                    for (int rx = 0; rx < rm.widthCells; ++rx) {
                        int gx = rm.originGx + rx;
                        if (gx >= 0 && gx < Config::CASTLE_GRID_W) colCount[gx]++;
                    }
                    for (int ry = 0; ry < rm.heightCells; ++ry) {
                        int gy = rm.originGy + ry;
                        if (gy >= 0 && gy < Config::CASTLE_GRID_H) rowCount[gy]++;
                    }
                }
            }

            for (size_t i = 0; i < frontier.size(); ++i) {
                if (frontier[i].biome == bType) {
                    allValid.push_back(static_cast<int>(i));
                    int tgx = frontier[i].targetGx;
                    int tgy = frontier[i].targetGy;
                    bool isVert = (frontier[i].requiredDoor & (DoorMask::NORTH | DoorMask::SOUTH)) != 0;

                    // Запобігаємо витягуванню в лінію: якщо в цій колонці вже 3 кімнати, не ростимо вертикально
                    if (isVert && tgx >= 0 && tgx < Config::CASTLE_GRID_W && colCount[tgx] >= 3) continue;
                    // Якщо в цьому рядку вже 4 кімнати, не ростимо горизонтально
                    if (!isVert && tgy >= 0 && tgy < Config::CASTLE_GRID_H && rowCount[tgy] >= 4) continue;

                    if (isVert == preferVertical) {
                        matching.push_back(static_cast<int>(i));
                    }
                }
            }
            if (!matching.empty()) return matching[rng() % matching.size()];
            if (!allValid.empty()) return allValid[rng() % allValid.size()];
            return -1;
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
                                        if ((grid_[ny][nx].state == CellState::Empty || grid_[ny][nx].state == CellState::NeedsDoor) && DetermineBiomeAt(nx, ny) == bType) {
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
                        chosenTmpl = PickDiverseTemplate(pool2x1, fd.targetGx, fd.targetGy);
                    } else if (multiType == 1) {
                        chosenTmpl = PickDiverseTemplate(pool1x2, fd.targetGx, fd.targetGy);
                    } else {
                        chosenTmpl = PickDiverseTemplate(pool2x2, fd.targetGx, fd.targetGy);
                    }
                } else {
                    chosenTmpl = PickDiverseTemplate(pool1x1, fd.targetGx, fd.targetGy);
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
                // Fallback to 1x1 combat з перемішуванням пулу та пріоритетом неповторюваних
                std::vector<int> shuffledPool = pool1x1;
                std::shuffle(shuffledPool.begin(), shuffledPool.end(), rng);
                std::sort(shuffledPool.begin(), shuffledPool.end(), [&](int a, int b) {
                    bool aRecent = std::find(recentTemplates.begin(), recentTemplates.end(), a) != recentTemplates.end();
                    bool bRecent = std::find(recentTemplates.begin(), recentTemplates.end(), b) != recentTemplates.end();
                    return !aRecent && bRecent;
                });
                for (int tIdx : shuffledPool) {
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
                grid_[fd.targetGy][fd.targetGx].state = CellState::Empty;
                grid_[fd.targetGy][fd.targetGx].requiredDoors = 0;
                continue;
            }

            auto [arx, ary] = validAnchors[rng() % validAnchors.size()];
            int origGx = fd.targetGx - arx;
            int origGy = fd.targetGy - ary;

            PlaceRoom(chosenTmpl, origGx, origGy, bType, rType, bBoss, fCond, frontier);
            roomsInBiome++;
            recentTemplates.push_back(chosenTmpl);
            if (recentTemplates.size() > 4) {
                recentTemplates.pop_front();
            }

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

    // Helper to guarantee a gateway door from Castle Hamson into another biome
    auto EnsureGatewaySeed = [&](BiomeType bType) {
        for (const auto& fd : globalFrontier) {
            if (fd.biome == bType) return; // вже є точка входу у біом
        }
        int bestDist = 9999;
        int targetGx = -1, targetGy = -1;
        uint8_t targetReqDoor = 0;

        for (const auto& rm : rooms_) {
            const auto& tmpl = RoomTemplates::TEMPLATES[static_cast<size_t>(rm.templateIdx)];
            for (int ry = 0; ry < rm.heightCells; ++ry) {
                for (int rx = 0; rx < rm.widthCells; ++rx) {
                    int gx = rm.originGx + rx;
                    int gy = rm.originGy + ry;
                    for (const auto& d : DIRS) {
                        if (!(tmpl.possibleDoors[ry][rx] & d.bit)) continue;
                        int nx = gx + d.dx;
                        int ny = gy + d.dy;
                        if (nx >= 2 && nx < Config::CASTLE_GRID_W - 2 &&
                            ny >= 2 && ny < Config::CASTLE_GRID_H - 2) {
                            if (grid_[ny][nx].state == CellState::Empty && DetermineBiomeAt(nx, ny) == bType) {
                                int dist = std::abs(nx - Config::ENTRANCE_GX) + std::abs(ny - Config::ENTRANCE_GY);
                                if (dist < bestDist) {
                                    bestDist = dist;
                                    targetGx = nx;
                                    targetGy = ny;
                                    targetReqDoor = OppositeDoor(d.bit);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (targetGx >= 0) {
            grid_[targetGy][targetGx].state = CellState::NeedsDoor;
            grid_[targetGy][targetGx].requiredDoors |= targetReqDoor;
            globalFrontier.push_back({ targetGx, targetGy, targetReqDoor, bType });
        } else {
            // Прокладаємо місток з найближчої кімнати
            int closestDist = 9999;
            int bridgeGx = Config::ENTRANCE_GX;
            int bridgeGy = Config::ENTRANCE_GY;
            for (const auto& rm : rooms_) {
                for (int ry = 0; ry < rm.heightCells; ++ry) {
                    for (int rx = 0; rx < rm.widthCells; ++rx) {
                        int gx = rm.originGx + rx;
                        int gy = rm.originGy + ry;
                        int dist = (bType == BiomeType::TheMaya) ? (gy - 25) :
                                   ((bType == BiomeType::LandOfDarkness) ? (39 - gy) :
                                   ((bType == BiomeType::ForestAbkhazia) ? (31 - gx) : 9999));
                        if (dist >= 0 && dist < closestDist) {
                            closestDist = dist;
                            bridgeGx = gx;
                            bridgeGy = gy;
                        }
                    }
                }
            }

            int curX = bridgeGx;
            int curY = bridgeGy;
            while (DetermineBiomeAt(curX, curY) != bType) {
                if (bType == BiomeType::TheMaya) curY--;
                else if (bType == BiomeType::LandOfDarkness) curY++;
                else if (bType == BiomeType::ForestAbkhazia) curX++;
                else break;

                if (curX < 2 || curX >= Config::CASTLE_GRID_W - 2 || curY < 2 || curY >= Config::CASTLE_GRID_H - 2) break;

                if (DetermineBiomeAt(curX, curY) == bType) {
                    uint8_t reqDoor = (bType == BiomeType::TheMaya) ? DoorMask::SOUTH :
                                      ((bType == BiomeType::LandOfDarkness) ? DoorMask::NORTH : DoorMask::WEST);
                    grid_[curY][curX].state = CellState::NeedsDoor;
                    grid_[curY][curX].requiredDoors |= reqDoor;
                    globalFrontier.push_back({ curX, curY, reqDoor, bType });
                    break;
                } else if (grid_[curY][curX].state == CellState::Empty) {
                    int tmpl = pool1x1[rng() % pool1x1.size()];
                    PlaceRoom(tmpl, curX, curY, DetermineBiomeAt(curX, curY), RoomType::Combat, BossType::Khidr, FairyCondition::KillAll, globalFrontier);
                }
            }
        }
    };

    // 2. Grow Castle Hamson organically (22-26 rooms)
    GrowBiome(BiomeType::CastleHamson, 22 + static_cast<int>(rng() % 5), globalFrontier);

    // 3. Grow Forest (22-26 rooms)
    EnsureGatewaySeed(BiomeType::ForestAbkhazia);
    GrowBiome(BiomeType::ForestAbkhazia, 22 + static_cast<int>(rng() % 5), globalFrontier);

    // 4. Grow Maya (22-26 rooms)
    EnsureGatewaySeed(BiomeType::TheMaya);
    GrowBiome(BiomeType::TheMaya, 22 + static_cast<int>(rng() % 5), globalFrontier);

    // 5. Grow Darkness (22-26 rooms)
    EnsureGatewaySeed(BiomeType::LandOfDarkness);
    GrowBiome(BiomeType::LandOfDarkness, 22 + static_cast<int>(rng() % 5), globalFrontier);

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