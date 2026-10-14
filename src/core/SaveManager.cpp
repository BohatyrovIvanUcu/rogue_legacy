#include "core/SaveManager.hpp"
#include "core/GameContext.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

namespace {

    json SerializeHeir(const HeirCandidate& h) {
        json j;
        j["name"] = h.name;
        j["classType"] = static_cast<int>(h.classType);
        j["spell"] = static_cast<int>(h.spell);
        j["isFemale"] = h.isFemale;
        j["generation"] = h.generation;
        j["visualSeed"] = h.visualSeed;

        std::vector<int> traitIds;
        traitIds.reserve(h.traits.size());
        for (TraitType t : h.traits) {
            traitIds.push_back(static_cast<int>(t));
        }
        j["traits"] = traitIds;
        return j;
    }

    HeirCandidate DeserializeHeir(const json& j) {
        HeirCandidate h;
        h.name = j.value("name", "Sir Johannes I");
        h.classType = static_cast<ClassType>(j.value("classType", 0));
        h.spell = static_cast<SpellType>(j.value("spell", 0));
        h.isFemale = j.value("isFemale", false);
        h.generation = j.value("generation", 1);
        h.visualSeed = j.value("visualSeed", 101u);

        if (j.contains("traits") && j["traits"].is_array()) {
            for (const auto& item : j["traits"]) {
                int tVal = item.get<int>();
                if (tVal >= 0 && tVal < static_cast<int>(TraitType::COUNT)) {
                    h.traits.push_back(static_cast<TraitType>(tVal));
                }
            }
        }
        return h;
    }

} // anonymous namespace

bool SaveManager::SaveFileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath);
}

bool SaveManager::SaveGame(const GameContext& ctx, const std::string& filePath) {
    try {
        json root;
        root["version"] = 1;
        root["gold"] = ctx.gold;
        root["ngPlusLevel"] = ctx.ngPlusLevel;
        root["generationCount"] = ctx.generationCount;
        root["musicMuted"] = ctx.musicMuted;
        root["isCastleLocked"] = ctx.isCastleLocked;
        root["hasSavedCastle"] = ctx.hasSavedCastle;

        root["manorLevels"] = ctx.manorLevels;

        root["unlockedBlueprints"] = ctx.unlockedBlueprints;
        root["ownedEquipment"] = ctx.ownedEquipment;
        root["equippedTier"] = ctx.equippedTier;

        root["unlockedRunes"] = ctx.unlockedRunes;
        root["ownedRunes"] = ctx.ownedRunes;
        std::array<int, 5> eqRunesInt{};
        for (size_t i = 0; i < 5; ++i) {
            eqRunesInt[i] = static_cast<int>(ctx.equippedRunes[i]);
        }
        root["equippedRunes"] = eqRunesInt;

        root["bossDefeated"] = ctx.bossDefeated;

        root["activeHeir"] = SerializeHeir(ctx.activeHeir);
        json candidatesArr = json::array();
        for (size_t i = 0; i < 3; ++i) {
            candidatesArr.push_back(SerializeHeir(ctx.heirCandidates[i]));
        }
        root["heirCandidates"] = candidatesArr;

        json castleGrid = json::array();
        for (int y = 0; y < Config::CASTLE_GRID_SIZE; ++y) {
            json row = json::array();
            for (int x = 0; x < Config::CASTLE_GRID_SIZE; ++x) {
                const SavedRoomNode& node = ctx.savedCastle[static_cast<size_t>(y)][static_cast<size_t>(x)];
                json rj;
                rj["active"] = node.active;
                rj["roomType"] = static_cast<int>(node.roomType);
                rj["biome"] = static_cast<int>(node.biome);
                rj["doorMask"] = node.doorMask;
                rj["templateIdx"] = node.templateIdx;
                rj["fairyCond"] = static_cast<int>(node.fairyCond);
                rj["bossType"] = static_cast<int>(node.bossType);
                rj["visited"] = node.visited;
                rj["cleared"] = node.cleared;
                rj["chestOpened"] = node.chestOpened;
                row.push_back(rj);
            }
            castleGrid.push_back(row);
        }
        root["savedCastle"] = castleGrid;

        json historyArr = json::array();
        for (const SlainRecord& rec : ctx.lineageHistory) {
            json r;
            r["heroName"] = rec.heroName;
            r["heroClass"] = static_cast<int>(rec.heroClass);
            r["killerName"] = rec.killerName;
            r["goldCollected"] = rec.goldCollected;
            r["enemiesSlain"] = rec.enemiesSlain;
            r["wasVictory"] = rec.wasVictory;
            historyArr.push_back(r);
        }
        root["lineageHistory"] = historyArr;

        std::ofstream out(filePath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out << root.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool SaveManager::LoadGame(GameContext& ctx, const std::string& filePath) {
    if (!SaveFileExists(filePath)) {
        return false;
    }

    try {
        std::ifstream in(filePath);
        if (!in.is_open()) {
            return false;
        }

        json root;
        in >> root;

        ctx.gold = root.value("gold", 150);
        ctx.ngPlusLevel = root.value("ngPlusLevel", 0);
        ctx.generationCount = root.value("generationCount", 1);
        ctx.musicMuted = root.value("musicMuted", false);
        ctx.isCastleLocked = root.value("isCastleLocked", false);
        ctx.hasSavedCastle = root.value("hasSavedCastle", false);
        ctx.lichBonusMaxHp = 0;

        if (root.contains("manorLevels") && root["manorLevels"].is_array()) {
            const auto& arr = root["manorLevels"];
            for (size_t i = 0; i < std::min(arr.size(), ctx.manorLevels.size()); ++i) {
                ctx.manorLevels[i] = arr[i].get<int>();
            }
        }

        if (root.contains("unlockedBlueprints") && root["unlockedBlueprints"].is_array()) {
            const auto& arr = root["unlockedBlueprints"];
            for (size_t s = 0; s < std::min<size_t>(5, arr.size()); ++s) {
                for (size_t t = 0; t < std::min<size_t>(5, arr[s].size()); ++t) {
                    ctx.unlockedBlueprints[s][t] = arr[s][t].get<bool>();
                }
            }
        }

        if (root.contains("ownedEquipment") && root["ownedEquipment"].is_array()) {
            const auto& arr = root["ownedEquipment"];
            for (size_t s = 0; s < std::min<size_t>(5, arr.size()); ++s) {
                for (size_t t = 0; t < std::min<size_t>(5, arr[s].size()); ++t) {
                    ctx.ownedEquipment[s][t] = arr[s][t].get<bool>();
                }
            }
        }

        if (root.contains("equippedTier") && root["equippedTier"].is_array()) {
            const auto& arr = root["equippedTier"];
            for (size_t s = 0; s < std::min<size_t>(5, arr.size()); ++s) {
                ctx.equippedTier[s] = arr[s].get<int>();
            }
        }

        if (root.contains("unlockedRunes") && root["unlockedRunes"].is_array()) {
            const auto& arr = root["unlockedRunes"];
            for (size_t s = 0; s < std::min<size_t>(5, arr.size()); ++s) {
                for (size_t r = 0; r < std::min<size_t>(8, arr[s].size()); ++r) {
                    ctx.unlockedRunes[s][r] = arr[s][r].get<bool>();
                }
            }
        }

        if (root.contains("ownedRunes") && root["ownedRunes"].is_array()) {
            const auto& arr = root["ownedRunes"];
            for (size_t s = 0; s < std::min<size_t>(5, arr.size()); ++s) {
                for (size_t r = 0; r < std::min<size_t>(8, arr[s].size()); ++r) {
                    ctx.ownedRunes[s][r] = arr[s][r].get<bool>();
                }
            }
        }

        if (root.contains("equippedRunes") && root["equippedRunes"].is_array()) {
            const auto& arr = root["equippedRunes"];
            for (size_t s = 0; s < std::min<size_t>(5, arr.size()); ++s) {
                ctx.equippedRunes[s] = static_cast<RuneType>(arr[s].get<int>());
            }
        }

        if (root.contains("bossDefeated") && root["bossDefeated"].is_array()) {
            const auto& arr = root["bossDefeated"];
            for (size_t b = 0; b < std::min<size_t>(5, arr.size()); ++b) {
                ctx.bossDefeated[b] = arr[b].get<bool>();
            }
        }

        if (root.contains("activeHeir")) {
            ctx.activeHeir = DeserializeHeir(root["activeHeir"]);
        }

        if (root.contains("heirCandidates") && root["heirCandidates"].is_array()) {
            const auto& arr = root["heirCandidates"];
            for (size_t i = 0; i < std::min<size_t>(3, arr.size()); ++i) {
                ctx.heirCandidates[i] = DeserializeHeir(arr[i]);
            }
        }

        if (root.contains("savedCastle") && root["savedCastle"].is_array()) {
            const auto& rows = root["savedCastle"];
            for (size_t y = 0; y < std::min<size_t>(Config::CASTLE_GRID_SIZE, rows.size()); ++y) {
                for (size_t x = 0; x < std::min<size_t>(Config::CASTLE_GRID_SIZE, rows[y].size()); ++x) {
                    const auto& rj = rows[y][x];
                    SavedRoomNode& node = ctx.savedCastle[y][x];
                    node.active = rj.value("active", false);
                    node.roomType = static_cast<RoomType>(rj.value("roomType", 1));
                    node.biome = static_cast<BiomeType>(rj.value("biome", 0));
                    node.doorMask = rj.value("doorMask", static_cast<uint8_t>(0));
                    node.templateIdx = rj.value("templateIdx", 0);
                    node.fairyCond = static_cast<FairyCondition>(rj.value("fairyCond", 0));
                    node.bossType = static_cast<BossType>(rj.value("bossType", 0));
                    node.visited = rj.value("visited", false);
                    node.cleared = rj.value("cleared", false);
                    node.chestOpened = rj.value("chestOpened", false);
                }
            }
        }

        ctx.lineageHistory.clear();
        if (root.contains("lineageHistory") && root["lineageHistory"].is_array()) {
            for (const auto& item : root["lineageHistory"]) {
                SlainRecord rec;
                rec.heroName = item.value("heroName", "Sir Johannes");
                rec.heroClass = static_cast<ClassType>(item.value("heroClass", 0));
                rec.killerName = item.value("killerName", "Skeleton");
                rec.goldCollected = item.value("goldCollected", 0);
                rec.enemiesSlain = item.value("enemiesSlain", 0);
                rec.wasVictory = item.value("wasVictory", false);
                ctx.lineageHistory.push_back(rec);
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}
