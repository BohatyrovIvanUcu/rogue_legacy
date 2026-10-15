// === FILE: src/entities/Entity.hpp ===
#pragma once

#include "core/Config.hpp"
#include "core/Types.hpp"
#include <raylib.h>
#include <cstdint>
#include <cmath>
#include <algorithm>

// Абстрактний інтерфейс тайлової карти кімнати для фізичних колізій сутностей
class ITileMap {
public:
    virtual ~ITileMap() = default;

    virtual char GetTileAt(int col, int row) const = 0;
    virtual float GetTrapTimer() const { return 0.0f; }

    virtual int GetWidthCells() const { return 1; }
    virtual int GetHeightCells() const { return 1; }
    virtual int GetTotalCols() const { return GetWidthCells() * Config::CELL_COLS; }
    virtual int GetTotalRows() const { return GetHeightCells() * Config::CELL_ROWS; }
    virtual int GetPixelWidth() const { return GetTotalCols() * Config::TILE_SIZE; }
    virtual int GetPixelHeight() const { return GetTotalRows() * Config::TILE_SIZE; }

    int WorldToCol(float worldX) const {
        return static_cast<int>(std::floor((worldX - static_cast<float>(Config::ROOM_OFFSET_X)) / static_cast<float>(Config::TILE_SIZE)));
    }

    int WorldToRow(float worldY) const {
        return static_cast<int>(std::floor((worldY - static_cast<float>(Config::ROOM_OFFSET_Y)) / static_cast<float>(Config::TILE_SIZE)));
    }

    Rectangle GetTileRect(int col, int row) const {
        return Rectangle{
            static_cast<float>(Config::ROOM_OFFSET_X + col * Config::TILE_SIZE),
            static_cast<float>(Config::ROOM_OFFSET_Y + row * Config::TILE_SIZE),
            static_cast<float>(Config::TILE_SIZE),
            static_cast<float>(Config::TILE_SIZE)
        };
    }

    bool IsSolidAt(int col, int row, bool isDwarf = false) const {
        char c = GetTileAt(col, row);
        if (c == '#' || c == 'X') return true;
        // Тайл 'D' (Dwarf Tunnel) пропускає лише спадкоємця з рисою Dwarfism
        if (c == 'D' && !isDwarf) return true;
        // Тайл 'I' (Illusion Wall) пропускає гравця безперешкодно!
        return false;
    }

    bool IsOneWayAt(int col, int row) const {
        return GetTileAt(col, row) == '=';
    }

    bool IsSlopeAt(int col, int row) const {
        char c = GetTileAt(col, row);
        return c == '/' || c == '\\';
    }

    bool GetSlopeSurfaceY(float worldX, int col, int row, float& outSurfaceY) const {
        char c = GetTileAt(col, row);
        if (c != '/' && c != '\\') return false;
        float tileLeft = static_cast<float>(Config::ROOM_OFFSET_X + col * Config::TILE_SIZE);
        float tileTop  = static_cast<float>(Config::ROOM_OFFSET_Y + row * Config::TILE_SIZE);
        float tSize    = static_cast<float>(Config::TILE_SIZE);
        float localX   = std::clamp((worldX - tileLeft) / tSize, 0.0f, 1.0f);
        if (c == '/') {
            outSurfaceY = tileTop + tSize * (1.0f - localX);
        } else {
            outSurfaceY = tileTop + tSize * localX;
        }
        return true;
    }

    bool IsSpikeHazardAt(int col, int row, bool hasPadTrait) const {
        char c = GetTileAt(col, row);
        if (c == '^' || c == 'v') return true;
        if (c == '~' && !hasPadTrait) {
            float ext = std::clamp(std::sin(GetTrapTimer() * 3.5f) * 1.4f, 0.0f, 1.0f);
            return ext > 0.35f;
        }
        return false;
    }

    Rectangle GetSpikeHitbox(int col, int row) const {
        char c = GetTileAt(col, row);
        Rectangle tileRec = GetTileRect(col, row);
        if (c == '^' || c == '~') {
            return Rectangle{ tileRec.x + 4.0f, tileRec.y + 22.0f, tileRec.width - 8.0f, 26.0f };
        }
        if (c == 'v') {
            return Rectangle{ tileRec.x + 4.0f, tileRec.y, tileRec.width - 8.0f, 26.0f };
        }
        return tileRec;
    }
};

// Базовий клас для всіх фізичних сутностей у світі
class Entity {
public:
    uint32_t id = 0;
    Vector2 pos{ 0.0f, 0.0f }; // Верхній лівий кут AABB хітбоксу
    Vector2 vel{ 0.0f, 0.0f };
    float width = 32.0f;
    float height = 32.0f;

    int hp = 10;
    int maxHp = 10;
    bool active = true;
    bool onGround = false;
    float facingDir = 1.0f; // 1.0f = праворуч, -1.0f = ліворуч

public:
    Entity() {
        static uint32_t nextEntityId = 1;
        id = nextEntityId++;
    }

    virtual ~Entity() = default;

    Rectangle GetBounds() const {
        return Rectangle{ pos.x, pos.y, width, height };
    }

    Vector2 GetCenter() const {
        return Vector2{ pos.x + width * 0.5f, pos.y + height * 0.5f };
    }

    void SetCenter(Vector2 center) {
        pos.x = center.x - width * 0.5f;
        pos.y = center.y - height * 0.5f;
    }

    float GetBottom() const {
        return pos.y + height;
    }

    float GetRight() const {
        return pos.x + width;
    }

    bool Intersects(const Rectangle& other) const {
        return CheckCollisionRecs(GetBounds(), other);
    }

    bool IsAlive() const {
        return active && hp > 0;
    }
};