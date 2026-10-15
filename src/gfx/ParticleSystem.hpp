#pragma once
#include <raylib.h>
#include <array>
#include <string>

struct Particle {
    Vector2 pos{};
    Vector2 vel{};
    Color color = WHITE;
    float size = 4.0f;
    float life = 0.0f;
    float maxLife = 0.5f;
    float gravityScale = 1.0f;
    float rotation = 0.0f;
    float rotSpeed = 0.0f;
    bool isCircle = false;
    bool fadeOut = true;
    bool active = false;
};

struct FloatingText {
    Vector2 pos{};
    Vector2 vel{};
    std::string text;
    Color color = WHITE;
    int fontSize = 20;
    float life = 0.0f;
    float maxLife = 0.85f;
    bool isCrit = false;
    bool active = false;
};

struct DecoyLog {
    Vector2 pos{};
    Vector2 vel{};
    float rotation = 0.0f;
    float life = 0.0f;
    bool active = false;
};

struct SpeechBubble {
    Vector2 pos{};
    std::string text;
    float life = 0.0f;
    float maxLife = 1.0f;
    bool active = false;
};

class ParticleSystem {
public:
    static constexpr size_t POOL_SIZE = 512;
    static constexpr size_t TEXT_POOL_SIZE = 64;
    static constexpr size_t LOG_POOL_SIZE = 16;
    static constexpr size_t BUBBLE_POOL_SIZE = 8;

    ParticleSystem();

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

    // Phase 7 Game Feel particles
    void SpawnBones(Vector2 pos, int count = 12);
    void SpawnGoldSparkles(Vector2 pos, int count = 8);

    void SpawnFloatingText(
        Vector2 pos,
        const std::string& text,
        Color color,
        int fontSize = 20,
        bool isCrit = false
    );

    void SpawnSpeechBubble(
        Vector2 pos,
        const std::string& text,
        float duration = 1.0f
    );

private:
    void EmitParticle(const Particle& p);

    std::array<Particle, POOL_SIZE> pool_{};
    size_t poolHead_ = 0;

    std::array<FloatingText, TEXT_POOL_SIZE> texts_{};
    size_t textHead_ = 0;

    std::array<DecoyLog, LOG_POOL_SIZE> decoyLogs_{};
    size_t logHead_ = 0;

    std::array<SpeechBubble, BUBBLE_POOL_SIZE> speechBubbles_{};
    size_t bubbleHead_ = 0;
};
