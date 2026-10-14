#pragma once
#include <raylib.h>
#include <vector>
#include <string>

struct Particle {
    Vector2 pos{};
    Vector2 vel{};
    Color color = WHITE;
    float size = 4.0f;
    float life = 0.5f;
    float maxLife = 0.5f;
    float gravityScale = 1.0f;
    float rotation = 0.0f;
    float rotSpeed = 0.0f;
    bool isCircle = false;
    bool fadeOut = true;
};

struct FloatingText {
    Vector2 pos{};
    Vector2 vel{};
    std::string text;
    Color color = WHITE;
    int fontSize = 20;
    float life = 0.85f;
    float maxLife = 0.85f;
    bool isCrit = false;
};

struct DecoyLog {
    Vector2 pos{};
    Vector2 vel{};
    float rotation = 0.0f;
    float life = 1.25f;
};

class ParticleSystem {
public:
    ParticleSystem() = default;

    void Clear();
    void Update(float dt);
    void Draw() const;

    void SpawnBlood(Vector2 pos, int count = 14);
    void SpawnSparks(Vector2 pos, Color tint = GOLD, int count = 12);
    void SpawnDust(Vector2 pos, int count = 6);
    void SpawnSmoke(Vector2 pos, Color tint = GRAY, int count = 16);
    void SpawnDebris(Vector2 pos, Color tint, int count = 12);
    void SpawnGasCloud(Vector2 pos);
    void SpawnExplosion(Vector2 pos, float radius, Color tint);
    void SpawnHealSparkles(Vector2 pos, Color tint, int count = 12);
    void SpawnDecoyLog(Vector2 pos);

    void SpawnFloatingText(
        Vector2 pos,
        const std::string& text,
        Color color,
        int fontSize = 20,
        bool isCrit = false
    );

private:
    std::vector<Particle> particles_;
    std::vector<FloatingText> texts_;
    std::vector<DecoyLog> decoyLogs_;
};
