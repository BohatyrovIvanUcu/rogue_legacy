// === FILE: src/world/CastleGenerator.cpp ===
#include "world/CastleGenerator.hpp"
#include "core/GameContext.hpp"
#include "data/RoomTemplates.hpp"
#include <random>
#include <queue>
#include <cmath>
#include <algorithm>
#include <chrono>

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
    uint64_t timeSeed = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::random_device rd;
    std::mt19937 rng(static_cast<uint32_t>(rd() ^ timeSeed));

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

    auto IsActive = [&](int x, int y) -> bool {
        if (x < 0 || x >= Config::CASTLE_GRID_SIZE || y < 0 || y >= Config::CASTLE_GRID_SIZE) return false;
        return ctx.savedCastle[static_cast<size_t>(y)][static_cast<size_t>(x)].active;
    };

    const int dx[4] = { 0, 1, 0, -1 };
    const int dy[4] = { -1, 0, 1, 0 };

    // 2. ПРОЦЕДУРНЕ ВИРОЩУВАННЯ ЗАМКУ (гарантована зв'язність, унікальна топологія кожен забіг)
    ActivateRoom(Config::ENTRANCE_GRID_X, Config::ENTRANCE_GRID_Y);

    // --- БІОМ 1: CastleHamson (Центральний хаб 3x3: x in [3..5], y in [3..5]) ---
    std::vector<std::pair<int, int>> hamsonFrontier = { { Config::ENTRANCE_GRID_X, Config::ENTRANCE_GRID_Y } };
    int targetHamson = 6 + static_cast<int>(rng() % 3); // 6..8 кімнат
    int hamsonCount = 1;

    for (int step = 0; step < 60 && hamsonCount < targetHamson && !hamsonFrontier.empty(); ++step) {
        int idx = static_cast<int>(rng() % hamsonFrontier.size());
        auto [cx, cy] = hamsonFrontier[static_cast<size_t>(idx)];

        std::vector<std::pair<int, int>> validNeighbors;
        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (nx >= 3 && nx <= 5 && ny >= 3 && ny <= 5 && !IsActive(nx, ny)) {
                validNeighbors.push_back({ nx, ny });
            }
        }

        if (validNeighbors.empty()) {
            hamsonFrontier.erase(hamsonFrontier.begin() + idx);
        } else {
            auto [chosenX, chosenY] = validNeighbors[rng() % validNeighbors.size()];
            ActivateRoom(chosenX, chosenY);
            hamsonFrontier.push_back({ chosenX, chosenY });
            hamsonCount++;
        }
    }

    // Гарантуємо проходи з Hamson до сусідніх біомів:
    // Східний вихід до ForestAbkhazia (x=5)
    bool hasEastExit = IsActive(5, 3) || IsActive(5, 4) || IsActive(5, 5);
    if (!hasEastExit) {
        ActivateRoom(5, 4);
    }
    // Північний вихід до TheMaya (y=3)
    bool hasNorthExit = IsActive(3, 3) || IsActive(4, 3) || IsActive(5, 3);
    if (!hasNorthExit) {
        ActivateRoom(4, 3);
    }
    // Вихід до LandOfDarkness (x=3 або y=5)
    bool hasDarkExit = IsActive(3, 4) || IsActive(3, 5) || IsActive(4, 5) || IsActive(5, 5);
    if (!hasDarkExit) {
        ActivateRoom(4, 5);
    }

    // Допоміжна функція для органічного росту біому методом випадкового фронтиру
    auto GrowBiomeWing = [&](BiomeType bType, const std::vector<std::pair<int, int>>& seedPoints, int targetTotal) {
        std::vector<std::pair<int, int>> frontier = seedPoints;
        for (const auto& pt : seedPoints) {
            ActivateRoom(pt.first, pt.second);
        }
        int curCount = static_cast<int>(seedPoints.size());

        for (int step = 0; step < 80 && curCount < targetTotal && !frontier.empty(); ++step) {
            int idx = static_cast<int>(rng() % frontier.size());
            auto [cx, cy] = frontier[static_cast<size_t>(idx)];

            std::vector<std::pair<int, int>> candidates;
            for (int d = 0; d < 4; ++d) {
                int nx = cx + dx[d];
                int ny = cy + dy[d];
                if (nx >= 0 && nx < Config::CASTLE_GRID_SIZE && ny >= 0 && ny < Config::CASTLE_GRID_SIZE) {
                    if (!IsActive(nx, ny) && DetermineBiomeAt(nx, ny) == bType) {
                        candidates.push_back({ nx, ny });
                    }
                }
            }

            if (candidates.empty()) {
                frontier.erase(frontier.begin() + idx);
            } else {
                auto [chosenX, chosenY] = candidates[rng() % candidates.size()];
                ActivateRoom(chosenX, chosenY);
                frontier.push_back({ chosenX, chosenY });
                curCount++;
            }
        }
    };

    // --- БІОМ 2: ForestAbkhazia (Східне крило) ---
    std::vector<std::pair<int, int>> eastBridges;
    for (int y = 3; y <= 5; ++y) {
        if (IsActive(5, y)) eastBridges.push_back({ 6, y });
    }
    if (eastBridges.empty()) eastBridges.push_back({ 6, 4 });
    std::shuffle(eastBridges.begin(), eastBridges.end(), rng);
    std::vector<std::pair<int, int>> forestSeeds = { eastBridges.front() };
    int targetForest = 6 + static_cast<int>(rng() % 2); // 6..7 кімнат
    GrowBiomeWing(BiomeType::ForestAbkhazia, forestSeeds, targetForest);

    // --- БІОМ 3: TheMaya (Північна вежа) ---
    std::vector<std::pair<int, int>> northBridges;
    for (int x = 3; x <= 5; ++x) {
        if (IsActive(x, 3)) northBridges.push_back({ x, 2 });
    }
    if (northBridges.empty()) northBridges.push_back({ 4, 2 });
    std::shuffle(northBridges.begin(), northBridges.end(), rng);
    std::vector<std::pair<int, int>> mayaSeeds = { northBridges.front() };
    int targetMaya = 6 + static_cast<int>(rng() % 2); // 6..7 кімнат
    GrowBiomeWing(BiomeType::TheMaya, mayaSeeds, targetMaya);

    // --- БІОМ 4: LandOfDarkness (Підземелля: Південь / Захід) ---
    std::vector<std::pair<int, int>> darkBridges;
    for (int x = 3; x <= 5; ++x) {
        if (IsActive(x, 5)) darkBridges.push_back({ x, 6 });
    }
    for (int y = 3; y <= 5; ++y) {
        if (IsActive(3, y)) darkBridges.push_back({ 2, y });
    }
    if (darkBridges.empty()) darkBridges.push_back({ 4, 6 });
    std::shuffle(darkBridges.begin(), darkBridges.end(), rng);
    std::vector<std::pair<int, int>> darkSeeds = { darkBridges.front() };
    if (darkBridges.size() > 1 && (rng() % 2 == 0)) {
        darkSeeds.push_back(darkBridges[1]);
    }
    int targetDarkness = 7 + static_cast<int>(rng() % 2); // 7..8 кімнат
    GrowBiomeWing(BiomeType::LandOfDarkness, darkSeeds, targetDarkness);

    // 3. ПЕРЕВІРКА ТА ГАРАНТІЯ ЗВ'ЯЗНОСТІ ЧЕРЕЗ BFS (від входу 4, 4)
    std::vector<std::vector<bool>> reachable(Config::CASTLE_GRID_SIZE, std::vector<bool>(Config::CASTLE_GRID_SIZE, false));
    std::queue<std::pair<int, int>> q;
    q.push({ Config::ENTRANCE_GRID_X, Config::ENTRANCE_GRID_Y });
    reachable[Config::ENTRANCE_GRID_Y][Config::ENTRANCE_GRID_X] = true;

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (nx >= 0 && nx < Config::CASTLE_GRID_SIZE && ny >= 0 && ny < Config::CASTLE_GRID_SIZE) {
                if (IsActive(nx, ny) && !reachable[static_cast<size_t>(ny)][static_cast<size_t>(nx)]) {
                    reachable[static_cast<size_t>(ny)][static_cast<size_t>(nx)] = true;
                    q.push({ nx, ny });
                }
            }
        }
    }

    // Деактивуємо будь-які ізольовані кімнати
    for (int y = 0; y < Config::CASTLE_GRID_SIZE; ++y) {
        for (int x = 0; x < Config::CASTLE_GRID_SIZE; ++x) {
            if (IsActive(x, y) && !reachable[static_cast<size_t>(y)][static_cast<size_t>(x)]) {
                ctx.savedCastle[static_cast<size_t>(y)][static_cast<size_t>(x)].active = false;
            }
        }
    }

    // 4. ОБЧИСЛЕННЯ БІТОВИХ МАСОК ДВЕРЕЙ МІЖ СУСІДНІМИ АКТИВНИМИ КІМНАТАМИ
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
                    if (IsActive(nx, ny)) {
                        mask |= dirBit[d];
                    }
                }
            }
            node.doorMask = mask;
        }
    }

    // 5. ПРОЦЕДУРНИЙ РОЗПОДІЛ ТИПІВ КІМНАТ ТА ШАБЛОНІВ У КОЖНОМУ З 4 БІОМІВ
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

        if (candidates.empty()) continue;

        // а) Кімната боса (BossEntrance): обираємо випадково серед найбільш віддалених кімнат біому (тупиків)
        int maxDist = 0;
        for (const auto& [cx, cy] : candidates) {
            int d = std::abs(cx - Config::ENTRANCE_GRID_X) + std::abs(cy - Config::ENTRANCE_GRID_Y);
            if (d > maxDist) maxDist = d;
        }
        std::vector<size_t> bossEligibleIndices;
        for (size_t i = 0; i < candidates.size(); ++i) {
            int d = std::abs(candidates[i].first - Config::ENTRANCE_GRID_X) + std::abs(candidates[i].second - Config::ENTRANCE_GRID_Y);
            if (d >= std::max(2, maxDist - 1)) {
                bossEligibleIndices.push_back(i);
            }
        }
        size_t chosenBossIdx = bossEligibleIndices[rng() % bossEligibleIndices.size()];
        auto [bossX, bossY] = candidates[chosenBossIdx];
        SavedRoomNode& bossNode = ctx.savedCastle[static_cast<size_t>(bossY)][static_cast<size_t>(bossX)];
        bossNode.roomType = RoomType::BossEntrance;
        bossNode.templateIdx = 14;
        bossNode.bossType = GetBiomeBoss(curBiome);
        candidates.erase(candidates.begin() + static_cast<ptrdiff_t>(chosenBossIdx));

        // Перемішуємо решту кандидатів, щоб Скарбниця, Телепортер, Скриня Феї та Бойові кімнати не сиділи в однакових координатах!
        std::shuffle(candidates.begin(), candidates.end(), rng);

        // б) Скриня Феї (FairyChest) з випадковою умовою випробування
        if (!candidates.empty()) {
            auto [fx, fy] = candidates.back();
            candidates.pop_back();
            SavedRoomNode& fairyNode = ctx.savedCastle[static_cast<size_t>(fy)][static_cast<size_t>(fx)];
            fairyNode.roomType = RoomType::FairyChest;
            fairyNode.fairyCond = static_cast<FairyCondition>(rng() % 4);
            fairyNode.bossType = GetBiomeBoss(curBiome);
            if (fairyNode.fairyCond == FairyCondition::NoJumping) {
                fairyNode.templateIdx = 11;
            } else if (fairyNode.fairyCond == FairyCondition::KillAll) {
                fairyNode.templateIdx = 10;
            } else {
                fairyNode.templateIdx = 9 + static_cast<int>(rng() % 2);
            }
        }

        // в) Телепортер (Teleporter) біому
        if (!candidates.empty()) {
            auto [tx, ty] = candidates.back();
            candidates.pop_back();
            SavedRoomNode& telNode = ctx.savedCastle[static_cast<size_t>(ty)][static_cast<size_t>(tx)];
            telNode.roomType = RoomType::Teleporter;
            telNode.templateIdx = 13;
            telNode.bossType = GetBiomeBoss(curBiome);
        }

        // г) Королівська скарбниця зі скринею (Treasure)
        if (!candidates.empty()) {
            auto [rx, ry] = candidates.back();
            candidates.pop_back();
            SavedRoomNode& trNode = ctx.savedCastle[static_cast<size_t>(ry)][static_cast<size_t>(rx)];
            trNode.roomType = RoomType::Treasure;
            trNode.templateIdx = 12;
            trNode.bossType = GetBiomeBoss(curBiome);
        }

        // ґ) Усі інші кімнати — різноманітні бойові випробування (Combat)
        // Використовуємо перемішаний пул шаблонів 1..8, щоб сусідні кімнати не повторювали одну й ту ж саму геометрію
        std::vector<int> combatPool = { 1, 2, 3, 4, 5, 6, 7, 8 };
        std::shuffle(combatPool.begin(), combatPool.end(), rng);
        size_t poolIdx = 0;

        for (const auto& [cx, cy] : candidates) {
            SavedRoomNode& cNode = ctx.savedCastle[static_cast<size_t>(cy)][static_cast<size_t>(cx)];
            cNode.roomType = RoomType::Combat;
            cNode.bossType = GetBiomeBoss(curBiome);
            cNode.templateIdx = combatPool[poolIdx % combatPool.size()];
            poolIdx++;
            if (poolIdx % combatPool.size() == 0) {
                std::shuffle(combatPool.begin(), combatPool.end(), rng);
            }
        }
    }

    // 6. НАЛАШТУВАННЯ КІМНАТИ ВХОДУ (4, 4)
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