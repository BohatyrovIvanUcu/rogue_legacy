#pragma once

namespace Config {

    inline constexpr int LOGICAL_WIDTH = 1280;
    inline constexpr int LOGICAL_HEIGHT = 720;
    inline constexpr int VIRTUAL_WIDTH = 1280;
    inline constexpr int VIRTUAL_HEIGHT = 720;

    inline constexpr int RENDER_WIDTH = 1920;
    inline constexpr int RENDER_HEIGHT = 1080;
    inline constexpr float SCALE_FACTOR = 1.5f;
    inline constexpr int TARGET_FPS = 60;

    inline constexpr int TILE_SIZE = 48;
    inline constexpr int CELL_COLS = 25;
    inline constexpr int CELL_ROWS = 15;
    inline constexpr int CELL_PIXEL_WIDTH = CELL_COLS * TILE_SIZE; // 1200 px
    inline constexpr int CELL_PIXEL_HEIGHT = CELL_ROWS * TILE_SIZE; // 720 px

    inline constexpr int ROOM_COLS = CELL_COLS;
    inline constexpr int ROOM_ROWS = CELL_ROWS;
    inline constexpr int ROOM_PIXEL_WIDTH = CELL_PIXEL_WIDTH;
    inline constexpr int ROOM_PIXEL_HEIGHT = CELL_PIXEL_HEIGHT;
    inline constexpr int ROOM_OFFSET_X = 0;
    inline constexpr int ROOM_OFFSET_Y = 0;

    inline constexpr int CASTLE_GRID_W = 64;
    inline constexpr int CASTLE_GRID_H = 64;
    inline constexpr int CASTLE_GRID_SIZE = 64;
    inline constexpr int ENTRANCE_GX = 24;
    inline constexpr int ENTRANCE_GY = 32;
    inline constexpr int ENTRANCE_GRID_X = ENTRANCE_GX;
    inline constexpr int ENTRANCE_GRID_Y = ENTRANCE_GY;

    inline constexpr float GRAVITY = 1750.0f;
    inline constexpr float MAX_FALL_SPEED = 950.0f;
    inline constexpr float GRACE_RUNE_MAX_FALL_SPEED = 420.0f;

    inline constexpr float PLAYER_BASE_SPEED = 290.0f;
    inline constexpr float PLAYER_JUMP_VELOCITY = -860.0f;
    inline constexpr float PLAYER_JUMP_CUT_MULTIPLIER = 0.56f;
    inline constexpr float PLAYER_POGO_BOUNCE_VELOCITY = -610.0f;
    inline constexpr float COYOTE_TIME_MAX = 0.10f;
    inline constexpr float JUMP_BUFFER_MAX = 0.10f;

    inline constexpr float DASH_SPEED = 820.0f;
    inline constexpr float DASH_DURATION = 0.16f;
    inline constexpr float DASH_COOLDOWN = 0.45f;

    inline constexpr float PLAYER_BASE_WIDTH = 32.0f;
    inline constexpr float PLAYER_BASE_HEIGHT = 54.0f;

    inline constexpr float SWORD_ATTACK_DURATION = 0.22f;
    inline constexpr float SWORD_COOLDOWN = 0.28f;
    inline constexpr float HITSTOP_DURATION = 0.035f;
    inline constexpr float BASE_IFRAME_DURATION = 0.85f;

    inline constexpr int MANOR_INFLATION_PER_LEVEL = 10;
    inline constexpr float ARCHITECT_GOLD_MULTIPLIER = 0.70f;

    inline constexpr int BASE_PLAYER_HP = 100;
    inline constexpr int BASE_PLAYER_MP = 50;
    inline constexpr int BASE_PLAYER_STR = 18;
    inline constexpr int BASE_PLAYER_INT = 20;
    inline constexpr int BASE_PLAYER_ARMOR = 0;
    inline constexpr int BASE_EQUIP_WEIGHT = 25;
    inline constexpr float BASE_CRIT_CHANCE = 0.05f;
    inline constexpr float BASE_CRIT_DMG = 1.50f;

    inline constexpr const char* SAVE_FILE_PATH = "rogue_legacy_save.json";

} // namespace Config
