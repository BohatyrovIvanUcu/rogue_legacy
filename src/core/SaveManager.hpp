#pragma once
#include <string>

class GameContext;

class SaveManager {
public:
    static bool SaveGame(const GameContext& ctx, const std::string& filePath = "rogue_legacy_save.json");
    static bool LoadGame(GameContext& ctx, const std::string& filePath = "rogue_legacy_save.json");
    static bool SaveFileExists(const std::string& filePath = "rogue_legacy_save.json");
};
