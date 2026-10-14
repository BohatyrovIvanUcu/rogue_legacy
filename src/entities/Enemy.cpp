// === FILE: src/entities/Enemy.cpp ===
#include "entities/Enemy.hpp"
#include "entities/Player.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/Palette.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

void Enemy::Init(
    EnemyType eType,
    int eTier,
    int roomLevel,
    Vector2 spawnCenter,
    bool dementiaPhantom
) {
    type = eType;
    tier = std::clamp(eTier, 1, 3);
    level = std::max(1, roomLevel);
    isDementiaPhantom = dementiaPhantom;

    float lvlScale = 1.0f + (level - 1) * 0.14f;
    float tierMult = (tier == 1) ? 1.0f : ((tier == 2) ? 1.45f : 2.05f);

    switch (type) {
        case EnemyType::Skeleton:
            width = 30.0f;
            height = 46.0f;
            maxHp = static_cast<int>(std::round(32.0f * lvlScale * tierMult));
            contactDamage = static_cast<int>(std::round(14.0f * lvlScale * (0.85f + 0.15f * tier)));
            goldReward = 20 * tier;
            attackCooldown = 1.8f;
            break;

        case EnemyType::FloatingEye:
            width = 34.0f;
            height = 34.0f;
            maxHp = static_cast<int>(std::round(26.0f * lvlScale * tierMult));
            contactDamage = static_cast<int>(std::round(13.0f * lvlScale * (0.85f + 0.15f * tier)));
            goldReward = 20 * tier;
            attackCooldown = 2.4f;
            break;

        case EnemyType::ShieldKnight:
            width = 36.0f;
            height = 52.0f;
            maxHp = static_cast<int>(std::round(52.0f * lvlScale * tierMult));
            contactDamage = static_cast<int>(std::round(18.0f * lvlScale * (0.85f + 0.15f * tier)));
            goldReward = 30 * tier;
            attackCooldown = 1.2f;
            break;

        case EnemyType::FireMage:
            width = 30.0f;
            height = 48.0f;
            maxHp = static_cast<int>(std::round(30.0f * lvlScale * tierMult));
            contactDamage = static_cast<int>(std::round(15.0f * lvlScale * (0.85f + 0.15f * tier)));
            goldReward = 30 * tier;
            attackCooldown = 3.0f;
            break;

        case EnemyType::Warg:
            width = 48.0f;
            height = 32.0f;
            maxHp = static_cast<int>(std::round(38.0f * lvlScale * tierMult));
            contactDamage = static_cast<int>(std::round(17.0f * lvlScale * (0.85f + 0.15f * tier)));
            goldReward = 25 * tier;
            attackCooldown = 1.6f;
            break;

        case EnemyType::WallTurret:
        default:
            width = 44.0f;
            height = 44.0f;
            maxHp = static_cast<int>(std::round(40.0f * lvlScale * tierMult));
            contactDamage = static_cast<int>(std::round(15.0f * lvlScale * (0.85f + 0.15f * tier)));
            goldReward = 15 * tier;
            attackCooldown = 2.2f;
            break;
    }

    if (isDementiaPhantom) {
        maxHp = 1;
        goldReward = 0;
    }

    hp = maxHp;
    active = true;
    SetCenter(spawnCenter);
    anchorPos = spawnCenter;
    vel = { 0.0f, 0.0f };
    facingDir = (spawnCenter.x > Config::VIRTUAL_WIDTH * 0.5f) ? -1.0f : 1.0f;
    sinePhase = static_cast<float>(std::rand() % 628) / 100.0f;
    stateTimer = static_cast<float>(std::rand() % 100) / 100.0f;
}

std::string Enemy::GetDisplayName() const {
    if (isDementiaPhantom) return "Dementia Phantom";
    std::string prefix = (tier == 1) ? "" : ((tier == 2) ? "Greater " : "Elite ");
    switch (type) {
        case EnemyType::Skeleton:     return prefix + "Skeleton";
        case EnemyType::FloatingEye:  return prefix + "Floating Eye";
        case EnemyType::ShieldKnight: return prefix + "Shield Knight";
        case EnemyType::FireMage:     return prefix + "Fire Mage";
        case EnemyType::Warg:         return prefix + "Dire Warg";
        case EnemyType::WallTurret:   return prefix + "Gargoyle Turret";
        default:                      return "Castle Monster";
    }
}

Rectangle Enemy::GetShieldHitbox() const {
    if (type != EnemyType::ShieldKnight) return Rectangle{ 0, 0, 0, 0 };
    Vector2 c = GetCenter();
    float sw = 12.0f;
    float sh = height * 0.92f;
    float sx = (facingDir >= 0.0f) ? (GetRight() - 4.0f) : (pos.x - sw + 4.0f);
    return Rectangle{ sx, c.y - sh * 0.5f, sw, sh };
}

bool Enemy::IsBlockedByShield(Vector2 attackOrigin, bool isDownStrike) const {
    if (type != EnemyType::ShieldKnight || isDownStrike || stunTimer > 0.0f) {
        return false;
    }
    float dirToAttacker = attackOrigin.x - GetCenter().x;
    return (dirToAttacker * facingDir > 0.0f);
}

void Enemy::ApplyStunAndKnockback(Vector2 velocity, float stunDuration) {
    if (type == EnemyType::WallTurret) return;
    vel = velocity;
    stunTimer = std::max(stunTimer, stunDuration);
    isTelegraphing = false;
}

bool Enemy::TakeDamage(
    int amount,
    Vector2 knockbackVel,
    bool isCrit,
    GameContext& ctx
) {
    if (!active || hp <= 0) return false;

    hp -= std::max(1, amount);
    hurtFlashTimer = 0.14f;

    if (type != EnemyType::WallTurret) {
        vel.x = knockbackVel.x;
        vel.y = knockbackVel.y;
    }

    if (ctx.particles) {
        if (isDementiaPhantom) {
            ctx.particles->SpawnSmoke(GetCenter(), PURPLE, 12);
        } else {
            ctx.particles->SpawnBlood(GetCenter(), 8);
        }
        ctx.particles->SpawnFloatingText(
            { GetCenter().x, pos.y - 8.0f },
            std::to_string(amount) + (isCrit ? "!" : ""),
            isCrit ? GOLD : WHITE,
            isCrit ? 24 : 19,
            isCrit
        );
    }

    if (hp <= 0) {
        active = false;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitEnemy, 0.9f);
        if (ctx.particles) {
            ctx.particles->SpawnExplosion(GetCenter(), 42.0f, isDementiaPhantom ? PURPLE : RED);
        }
        return true;
    } else {
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitEnemy, 1.05f);
        return false;
    }
}

void Enemy::Update(
    float dt,
    GameContext& ctx,
    const ITileMap* tileMap,
    const Player& player,
    std::vector<Projectile>& outProjectiles,
    bool timeStopActive
) {
    if (!active) return;
    playerPosCached = player.GetCenter();

    if (hurtFlashTimer > 0.0f) hurtFlashTimer -= dt;
    if (timeStopActive) return;

    animTimer += dt;
    if (stunTimer > 0.0f) {
        stunTimer -= dt;
        if (type != EnemyType::FloatingEye && type != EnemyType::WallTurret) {
            ResolvePhysicsAndTiles(dt, tileMap, false);
        }
        return;
    }

    Vector2 pCenter = player.GetCenter();
    Vector2 myCenter = GetCenter();

    switch (type) {
        case EnemyType::Skeleton: {
            // Патрулює платформу, розвертається біля країв/стін, кидає кістки по параболі
            float walkSpeed = 75.0f + tier * 20.0f;
            vel.x = facingDir * walkSpeed;
            ResolvePhysicsAndTiles(dt, tileMap, true);

            stateTimer += dt;
            if (stateTimer >= attackCooldown) {
                stateTimer = 0.0f;
                float faceToPlayer = (pCenter.x >= myCenter.x) ? 1.0f : -1.0f;
                facingDir = faceToPlayer;

                int boneCount = (tier == 3) ? 2 : 1;
                for (int i = 0; i < boneCount; ++i) {
                    float vx = faceToPlayer * (210.0f + i * 75.0f + std::abs(pCenter.x - myCenter.x) * 0.35f);
                    float vy = -490.0f - i * 60.0f;
                    outProjectiles.push_back(Projectile::MakeEnemyBone(myCenter, { vx, vy }, contactDamage));
                }
            }
            break;
        }

        case EnemyType::FloatingEye: {
            // Літає синусоїдою, за 0.6s до атаки червоніє і випускає 4 (або 8) снарядів
            sinePhase += dt * 2.8f;
            Vector2 toPlayer = { pCenter.x - myCenter.x, pCenter.y - myCenter.y };
            float d = std::hypot(toPlayer.x, toPlayer.y);
            if (d > 90.0f) {
                vel.x = (toPlayer.x / d) * (65.0f + tier * 15.0f);
                vel.y = (toPlayer.y / d) * (65.0f + tier * 15.0f) + std::cos(sinePhase) * 55.0f;
            } else {
                vel.x = std::sin(sinePhase * 0.7f) * 45.0f;
                vel.y = std::cos(sinePhase) * 65.0f;
            }
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;

            stateTimer += dt;
            isTelegraphing = (stateTimer >= attackCooldown - 0.60f);
            if (stateTimer >= attackCooldown) {
                stateTimer = 0.0f;
                isTelegraphing = false;
                float boltSpeed = 235.0f + tier * 30.0f;

                if (tier == 1) {
                    // 4 снаряди по осях
                    const std::array<Vector2, 4> dirs = {{
                        { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f }
                    }};
                    for (const auto& dir : dirs) {
                        outProjectiles.push_back(Projectile::MakeEnemyEyeBolt(myCenter, { dir.x * boltSpeed, dir.y * boltSpeed }, contactDamage, ORANGE));
                    }
                } else if (tier == 2) {
                    // 4 снаряди по діагоналях
                    const float inv = 0.70710678f;
                    const std::array<Vector2, 4> dirs = {{
                        { inv, inv }, { -inv, inv }, { inv, -inv }, { -inv, -inv }
                    }};
                    for (const auto& dir : dirs) {
                        outProjectiles.push_back(Projectile::MakeEnemyEyeBolt(myCenter, { dir.x * boltSpeed, dir.y * boltSpeed }, contactDamage, MAGENTA));
                    }
                } else {
                    // Tier 3: 8 снарядів зіркою
                    for (int i = 0; i < 8; ++i) {
                        float ang = i * (6.2831853f / 8.0f);
                        outProjectiles.push_back(Projectile::MakeEnemyEyeBolt(
                            myCenter,
                            { std::cos(ang) * boltSpeed, std::sin(ang) * boltSpeed },
                            contactDamage, RED
                        ));
                    }
                }
            }
            break;
        }

        case EnemyType::ShieldKnight: {
            // Повільно крокує зі щитом поперед себе. Розвертається до гравця із затримкою.
            stateTimer += dt;
            if (stateTimer >= 0.75f) {
                stateTimer = 0.0f;
                if (std::abs(pCenter.x - myCenter.x) > 28.0f) {
                    facingDir = (pCenter.x >= myCenter.x) ? 1.0f : -1.0f;
                }
            }
            float marchSpeed = 58.0f + tier * 14.0f;
            vel.x = facingDir * marchSpeed;
            ResolvePhysicsAndTiles(dt, tileMap, true);
            break;
        }

        case EnemyType::FireMage: {
            // Кожні 3.0s телепортується у вільну точку, фокусується 0.7s і піднімає 3 вогняні стовпи
            stateTimer += dt;
            vel.x = 0.0f;
            ResolvePhysicsAndTiles(dt, tileMap, true);
            facingDir = (pCenter.x >= myCenter.x) ? 1.0f : -1.0f;

            if (!isTelegraphing && stateTimer >= 2.30f) {
                isTelegraphing = true;
                lockedAttackTarget = { pCenter.x, player.GetBottom() };
            }

            if (stateTimer >= 3.00f) {
                stateTimer = 0.0f;
                isTelegraphing = false;

                // Спавн 3 вогняних стовпів з підлоги під позицією гравця
                int pillarCount = (tier >= 2) ? 3 : 2;
                for (int i = 0; i < pillarCount; ++i) {
                    float offset = (i - (pillarCount - 1) * 0.5f) * 52.0f;
                    outProjectiles.push_back(Projectile::MakeEnemyFirePillar(
                        { lockedAttackTarget.x + offset, lockedAttackTarget.y },
                        contactDamage
                    ));
                }
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 0.85f, 0.7f);

                // Телепортація у нову вільну точку кімнати
                if (tileMap) {
                    for (int attempt = 0; attempt < 15; ++attempt) {
                        int c = 2 + (std::rand() % (Config::ROOM_COLS - 4));
                        int r = 2 + (std::rand() % (Config::ROOM_ROWS - 4));
                        if (!tileMap->IsSolidAt(c, r, false) &&
                            (tileMap->IsSolidAt(c, r + 1, false) || tileMap->IsOneWayAt(c, r + 1))) {
                            Rectangle tRec = tileMap->GetTileRect(c, r);
                            if (ctx.particles) ctx.particles->SpawnSmoke(myCenter, ORANGE, 12);
                            pos.x = tRec.x + (tRec.width - width) * 0.5f;
                            pos.y = tRec.y + tRec.height - height;
                            if (ctx.particles) ctx.particles->SpawnSmoke(GetCenter(), ORANGE, 12);
                            break;
                        }
                    }
                }
            }
            break;
        }

        case EnemyType::Warg: {
            // Швидко бігає по підлозі; коли бачить гравця на одній лінії — робить стрибок-укус
            stateTimer += dt;
            float baseRun = 135.0f + tier * 25.0f;
            bool sameHeight = std::abs(pCenter.y - myCenter.y) < 68.0f;
            float distX = std::abs(pCenter.x - myCenter.x);

            if (onGround && sameHeight && distX < 320.0f && stateTimer >= attackCooldown) {
                stateTimer = 0.0f;
                facingDir = (pCenter.x >= myCenter.x) ? 1.0f : -1.0f;
                vel.x = facingDir * (390.0f + tier * 35.0f);
                vel.y = -340.0f;
                onGround = false;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::BossRoar, 1.65f, 0.55f);
            } else if (onGround) {
                vel.x = facingDir * baseRun;
            }
            ResolvePhysicsAndTiles(dt, tileMap, onGround);
            break;
        }

        case EnemyType::WallTurret: {
            // Вмонтована у стіну кам'яна голова горгульї, кожні 2.2s випускає вогняну стрілу
            stateTimer += dt;
            isTelegraphing = (stateTimer >= attackCooldown - 0.45f);
            if (stateTimer >= attackCooldown) {
                stateTimer = 0.0f;
                isTelegraphing = false;
                Vector2 muzzle = { myCenter.x + facingDir * 24.0f, myCenter.y };
                outProjectiles.push_back(Projectile::MakeEnemyArrow(muzzle, facingDir, contactDamage));
            }
            break;
        }

        default:
            break;
    }
}

void Enemy::ResolvePhysicsAndTiles(float dt, const ITileMap* tileMap, bool stopAtPlatformEdges) {
    if (!tileMap) return;

    vel.y += Config::GRAVITY * dt;
    if (vel.y > Config::MAX_FALL_SPEED) vel.y = Config::MAX_FALL_SPEED;

    // Перевірка краю платформи під час патрулювання
    if (onGround && stopAtPlatformEdges && vel.x != 0.0f) {
        float probeX = (vel.x > 0.0f) ? (GetRight() + 6.0f) : (pos.x - 6.0f);
        int probeCol = tileMap->WorldToCol(probeX);
        int belowRow = tileMap->WorldToRow(GetBottom() + 6.0f);
        bool hasFloorAhead = tileMap->IsSolidAt(probeCol, belowRow, false) ||
                             tileMap->IsOneWayAt(probeCol, belowRow);
        bool hasSpikesAhead = tileMap->IsSpikeHazardAt(probeCol, tileMap->WorldToRow(GetBottom() - 4.0f), false);
        if (!hasFloorAhead || hasSpikesAhead) {
            facingDir = -facingDir;
            vel.x = -vel.x;
        }
    }

    // Горизонтальний рух
    pos.x += vel.x * dt;
    Rectangle b = GetBounds();
    int rMin = tileMap->WorldToRow(b.y + 4.0f);
    int rMax = tileMap->WorldToRow(b.y + b.height - 4.0f);

    if (vel.x > 0.0f) {
        int cRight = tileMap->WorldToCol(b.x + b.width);
        for (int r = rMin; r <= rMax; ++r) {
            if (tileMap->IsSolidAt(cRight, r, false)) {
                pos.x = tileMap->GetTileRect(cRight, r).x - width - 0.01f;
                facingDir = -1.0f;
                vel.x = -vel.x;
                break;
            }
        }
    } else if (vel.x < 0.0f) {
        int cLeft = tileMap->WorldToCol(b.x);
        for (int r = rMin; r <= rMax; ++r) {
            if (tileMap->IsSolidAt(cLeft, r, false)) {
                Rectangle tRec = tileMap->GetTileRect(cLeft, r);
                pos.x = tRec.x + tRec.width + 0.01f;
                facingDir = 1.0f;
                vel.x = -vel.x;
                break;
            }
        }
    }

    // Вертикальний рух
    float prevBottom = GetBottom();
    onGround = false;
    pos.y += vel.y * dt;
    b = GetBounds();
    int cMin = tileMap->WorldToCol(b.x + 4.0f);
    int cMax = tileMap->WorldToCol(b.x + b.width - 4.0f);

    if (vel.y >= 0.0f) {
        int rBot = tileMap->WorldToRow(b.y + b.height);
        for (int c = cMin; c <= cMax; ++c) {
            if (tileMap->IsSolidAt(c, rBot, false) ||
                (tileMap->IsOneWayAt(c, rBot) && prevBottom <= tileMap->GetTileRect(c, rBot).y + 10.0f)) {
                pos.y = tileMap->GetTileRect(c, rBot).y - height;
                vel.y = 0.0f;
                onGround = true;
                break;
            }
        }
    } else if (vel.y < 0.0f) {
        int rTop = tileMap->WorldToRow(b.y);
        for (int c = cMin; c <= cMax; ++c) {
            if (tileMap->IsSolidAt(c, rTop, false)) {
                Rectangle tRec = tileMap->GetTileRect(c, rTop);
                pos.y = tRec.y + tRec.height + 0.01f;
                vel.y = 0.0f;
                break;
            }
        }
    }
}

void Enemy::Draw() const {
    if (!active) return;

    const auto& pal = GetPalette();
    Vector2 c = GetCenter();
    float alpha = isDementiaPhantom ? 0.55f : 1.0f;

    Color tierTint = pal.bone[4];
    if (isDementiaPhantom) {
        tierTint = pal.moonMagic[3];
    } else if (tier == 2) {
        tierTint = pal.moonMagic[3];
    } else if (tier == 3) {
        tierTint = pal.fireGold[3];
    }
    if (hurtFlashTimer > 0.0f) {
        tierTint = pal.bloodCloth[3];
    }

    switch (type) {
        case EnemyType::Skeleton: {
            Color boneCol   = isDementiaPhantom ? tierTint : ColorTint(pal.bone[3], tierTint);
            Color shadowBone = ColorTint(boneCol, pal.bone[1]);
            Color hiBone     = ColorTint(boneCol, pal.bone[4]);

            // 1. Череп з вилицями, щелепою та очницями
            float skullY = pos.y + 10.0f;
            // Купол черепа з 3-тональним cel-shading
            DrawCircleV({ c.x, skullY }, 9.5f, Fade(shadowBone, alpha));
            DrawCircleSector({ c.x, skullY }, 9.5f, 110.0f, 290.0f, 12, Fade(boneCol, alpha));
            DrawCircle(static_cast<int>(c.x - 2.5f), static_cast<int>(skullY - 2.5f), 4.2f, Fade(hiBone, alpha));

            // Загострені вилиці (Zygomatic arches)
            DrawTriangle({ c.x - 6.0f, skullY + 1.0f }, { c.x - 9.5f, skullY + 3.0f }, { c.x - 5.0f, skullY + 5.0f }, Fade(boneCol, alpha));
            DrawTriangle({ c.x + 6.0f, skullY + 1.0f }, { c.x + 9.5f, skullY + 3.0f }, { c.x + 5.0f, skullY + 5.0f }, Fade(shadowBone, alpha));

            // Шолом для тиру 2 або 3
            if (tier >= 2) {
                DrawCircleSector({ c.x, skullY - 1.0f }, 10.5f, 180.0f, 360.0f, 12, Fade(pal.metal[2], alpha));
                DrawLineEx({ c.x - 11.0f, skullY - 1.0f }, { c.x + 11.0f, skullY - 1.0f }, 2.5f, Fade(pal.fireGold[3], alpha));
                if (tier == 3) {
                    DrawTriangle({ c.x - 3.0f, skullY - 10.0f }, { c.x + 3.0f, skullY - 10.0f }, { c.x, skullY - 18.0f }, Fade(pal.fireGold[3], alpha));
                }
            }

            // Очниці та світні зіниці (емісив)
            Vector2 eyeL = { c.x - 3.8f, skullY - 0.5f };
            Vector2 eyeR = { c.x + 3.8f, skullY - 0.5f };
            DrawCircleV(eyeL, 3.2f, Fade(pal.outline, alpha));
            DrawCircleV(eyeR, 3.2f, Fade(pal.outline, alpha));
            Color eyeGlow = (tier == 3) ? pal.fireGold[3] : (tier == 2 ? pal.moonMagic[3] : pal.bloodCloth[3]);
            DrawCircleV(eyeL, 1.8f, Fade(eyeGlow, alpha));
            DrawCircleV(eyeL, 0.8f, Fade(pal.metal[4], alpha));
            DrawCircleV(eyeR, 1.8f, Fade(eyeGlow, alpha));
            DrawCircleV(eyeR, 0.8f, Fade(pal.metal[4], alpha));

            // Грушоподібна носова порожнина
            DrawTriangle({ c.x, skullY + 1.8f }, { c.x - 1.8f, skullY + 4.8f }, { c.x + 1.8f, skullY + 4.8f }, Fade(pal.outline, alpha));

            // Нижня щелепа (Mandible) з окремими зубами
            DrawRectangle(static_cast<int>(c.x - 5.5f), static_cast<int>(skullY + 5.5f), 11, 4, Fade(boneCol, alpha));
            for (int t = -4; t <= 4; t += 2) {
                DrawLine(static_cast<int>(c.x + t), static_cast<int>(skullY + 5.5f), static_cast<int>(c.x + t), static_cast<int>(skullY + 9.5f), Fade(pal.outline, alpha * 0.85f));
            }

            // 2. Хребет і 4 пари ребер із тріщинами
            DrawLineEx({ c.x, skullY + 8.5f }, { c.x, pos.y + 35.0f }, 4.0f, Fade(boneCol, alpha));
            for (int r = 0; r < 4; ++r) {
                float ry = pos.y + 19.0f + r * 4.2f;
                float rSpan = 8.8f - r * 0.9f;
                // Ребро з вигином
                DrawLineEx({ c.x - rSpan, ry }, { c.x + rSpan, ry }, 2.4f, Fade(boneCol, alpha));
                DrawCircle(static_cast<int>(c.x - rSpan), static_cast<int>(ry), 1.3f, Fade(hiBone, alpha));
                DrawCircle(static_cast<int>(c.x + rSpan), static_cast<int>(ry), 1.3f, Fade(shadowBone, alpha));
                // Тріщини на ребрах
                if (r == 1 || r == 3) {
                    DrawLine(static_cast<int>(c.x - rSpan * 0.5f), static_cast<int>(ry - 1.0f), static_cast<int>(c.x - rSpan * 0.5f + 1.5f), static_cast<int>(ry + 1.0f), Fade(pal.outline, alpha * 0.75f));
                }
            }

            // Тазова кістка (Pelvic girdle)
            DrawRectangleRounded({ c.x - 7.5f, pos.y + 33.5f, 15.0f, 5.5f }, 0.45f, 4, Fade(boneCol, alpha));
            DrawCircle(static_cast<int>(c.x - 3.5f), static_cast<int>(pos.y + 36.0f), 1.8f, Fade(pal.outline, alpha));
            DrawCircle(static_cast<int>(c.x + 3.5f), static_cast<int>(pos.y + 36.0f), 1.8f, Fade(pal.outline, alpha));

            // 3. Рука з іржавим зазубреним клинком
            Vector2 shoulder = { c.x + facingDir * 6.0f, pos.y + 19.0f };
            Vector2 hand = { c.x + facingDir * 18.0f, pos.y + 25.0f };
            DrawLineEx(shoulder, hand, 3.2f, Fade(boneCol, alpha));

            // Іржавий зазубрений клинок (Rust-eaten serrated blade)
            Vector2 bladeTip = { hand.x + facingDir * 23.0f, hand.y - 15.0f };
            Color rustCol = pal.wood[2];
            DrawLineEx(hand, bladeTip, 4.8f, Fade(rustCol, alpha));
            DrawLineEx(hand, bladeTip, 2.2f, Fade(pal.metal[1], alpha));
            // Гарда
            DrawLineEx({ hand.x - facingDir * 3.5f, hand.y + 3.5f }, { hand.x + facingDir * 3.5f, hand.y - 3.5f }, 3.5f, Fade(pal.metal[2], alpha));
            // Зазубрини (serrations)
            for (int sr = 1; sr <= 3; ++sr) {
                float st = sr * 0.28f;
                Vector2 sPt = { hand.x + (bladeTip.x - hand.x) * st, hand.y + (bladeTip.y - hand.y) * st };
                DrawTriangle({ sPt.x, sPt.y }, { sPt.x - facingDir * 2.5f, sPt.y + 2.0f }, { sPt.x + facingDir * 1.5f, sPt.y - 1.5f }, Fade(pal.outline, alpha));
            }

            // 4. Ноги з суглобами, що крокують
            float step = std::sin(animTimer * 12.0f) * 6.0f;
            DrawLineEx({ c.x - 4.5f, pos.y + 36.0f }, { c.x - 6.0f + step, GetBottom() }, 3.5f, Fade(boneCol, alpha));
            DrawCircle(static_cast<int>(c.x - 6.0f + step), static_cast<int>(GetBottom() - 1.0f), 2.5f, Fade(shadowBone, alpha));
            DrawLineEx({ c.x + 4.5f, pos.y + 36.0f }, { c.x + 6.0f - step, GetBottom() }, 3.5f, Fade(boneCol, alpha));
            DrawCircle(static_cast<int>(c.x + 6.0f - step), static_cast<int>(GetBottom() - 1.0f), 2.5f, Fade(shadowBone, alpha));
            break;
        }

        case EnemyType::FloatingEye: {
            Color sclera = isTelegraphing ? pal.bloodCloth[3] : ColorTint(pal.bone[4], tierTint);
            Color iris   = (tier == 1) ? pal.fireGold[2] : ((tier == 2) ? pal.moonMagic[3] : pal.bloodCloth[3]);
            float floatPulse = 1.0f + 0.05f * std::sin(animTimer * 8.0f);

            // Тінь ширяння під оком
            DrawEllipse(static_cast<int>(c.x), static_cast<int>(pos.y + height + 6.0f), 13.0f, 4.0f, Fade(pal.outline, 0.35f * alpha));

            // 1. Чотири щупальця (синусоїда із затримкою по сегментах)
            for (int t = 0; t < 4; ++t) {
                float tBaseX = c.x - 9.0f + t * 6.0f;
                float tBaseY = pos.y + height - 6.0f;
                Vector2 prevNode = { tBaseX, tBaseY };
                for (int s = 1; s <= 4; ++s) {
                    float delay = t * 0.95f + s * 0.70f;
                    float sWave = std::sin(animTimer * 6.5f - delay) * (3.5f + s * 1.6f);
                    Vector2 nextNode = { tBaseX + sWave, tBaseY + s * 5.5f };
                    DrawLineEx(prevNode, nextNode, 4.2f - s * 0.65f, Fade(pal.bloodCloth[1], alpha));
                    prevNode = nextNode;
                }
                DrawCircleV(prevNode, 1.8f, Fade(pal.bloodCloth[3], alpha)); // отруйний кінчик
            }

            // 2. Темно-червона зубчаста оболонка (м'ясисті війкові шипи навколо ока)
            constexpr int SPIKES = 8;
            for (int sp = 0; sp < SPIKES; ++sp) {
                float sa = sp * (6.2831853f / SPIKES) + animTimer * 1.8f;
                Vector2 sBase1 = { c.x + std::cos(sa - 0.22f) * 14.5f, c.y + std::sin(sa - 0.22f) * 14.5f };
                Vector2 sBase2 = { c.x + std::cos(sa + 0.22f) * 14.5f, c.y + std::sin(sa + 0.22f) * 14.5f };
                Vector2 sTip   = { c.x + std::cos(sa) * 21.5f, c.y + std::sin(sa) * 21.5f };
                DrawTriangle(sBase1, sTip, sBase2, Fade(pal.bloodCloth[1], alpha));
                DrawTriangle(sBase1, sBase2, { c.x, c.y }, Fade(pal.bloodCloth[0], alpha));
            }

            // 3. Очне яблуко з кровоносними судинами
            DrawCircleV(c, 16.5f * floatPulse, Fade(sclera, alpha));
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 16.5f * floatPulse, Fade(pal.bloodCloth[0], alpha));

            // Капілярні судини
            for (int v = 0; v < 5; ++v) {
                float va = v * 1.25f + 0.35f;
                Vector2 v1 = { c.x + std::cos(va) * 15.0f, c.y + std::sin(va) * 15.0f };
                Vector2 v2 = { c.x + std::cos(va) * 8.5f, c.y + std::sin(va) * 8.5f };
                DrawLineEx(v1, v2, 1.3f, Fade(pal.bloodCloth[2], alpha * 0.85f));
            }

            // 4. Райдужка-градієнт
            DrawCircleV(c, 9.5f, Fade(pal.bloodCloth[1], alpha));
            DrawCircleV(c, 7.8f, Fade(iris, alpha));
            DrawCircleV(c, 5.0f, Fade(ColorTint(iris, pal.fireGold[3]), alpha));

            // 5. Вертикальна зіниця, що ЗАВЖДИ ДИВИТЬСЯ НА ГРАВЦЯ
            Vector2 toPlayer = { playerPosCached.x - c.x, playerPosCached.y - c.y };
            float dist = std::hypot(toPlayer.x, toPlayer.y);
            Vector2 pOffset = { 0.0f, 0.0f };
            if (dist > 1.0f) {
                pOffset = { (toPlayer.x / dist) * 4.0f, (toPlayer.y / dist) * 3.5f };
            }
            Vector2 pupilPos = { c.x + pOffset.x, c.y + pOffset.y };

            float pupilW = isTelegraphing ? 5.5f : 3.2f;
            DrawEllipse(static_cast<int>(pupilPos.x), static_cast<int>(pupilPos.y), pupilW, 7.5f, pal.outline);
            // Блік світла зверху-ліворуч
            DrawCircleV({ pupilPos.x - 2.2f, pupilPos.y - 2.8f }, 1.8f, Fade(pal.bone[4], alpha * 0.95f));

            if (isTelegraphing) {
                DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 22.0f + std::sin(animTimer * 20.0f) * 4.0f, Fade(pal.danger[2], 0.85f));
            }
            break;
        }

        case EnemyType::ShieldKnight: {
            Color armorBase = ColorTint(pal.metal[3], tierTint);
            Color armorDark = ColorTint(armorBase, pal.metal[1]);
            Color goldTrim  = (tier == 2) ? pal.moonMagic[3] : ((tier == 3) ? pal.fireGold[3] : pal.fireGold[2]);

            // 1. Розвіваючий плащ лицаря
            float capeWave = std::sin(animTimer * 10.0f) * 4.5f;
            DrawTriangle(
                { c.x - facingDir * 6.0f, pos.y + 14.0f },
                { c.x - facingDir * 26.0f, GetBottom() - 4.0f + capeWave },
                { c.x - facingDir * 10.0f, GetBottom() - 4.0f },
                Fade(pal.bloodCloth[1], alpha)
            );

            // 2. Поножі та сабатони
            float knightWalk = std::sin(animTimer * 10.0f) * 4.0f;
            DrawRectangleRec({ pos.x + 8.0f + knightWalk, GetBottom() - 15.0f, 9.0f, 15.0f }, Fade(armorBase, alpha));
            DrawRectangleRec({ pos.x + 23.0f - knightWalk, GetBottom() - 15.0f, 9.0f, 15.0f }, Fade(armorBase, alpha));
            DrawCircle(static_cast<int>(pos.x + 12.5f + knightWalk), static_cast<int>(GetBottom() - 15.0f), 4.5f, Fade(goldTrim, alpha));
            DrawCircle(static_cast<int>(pos.x + 27.5f - knightWalk), static_cast<int>(GetBottom() - 15.0f), 4.5f, Fade(goldTrim, alpha));

            // 3. Важка лицарська кіраса та шиповані наплічники
            Rectangle chestRec = { pos.x + 6.0f, pos.y + 16.0f, width - 12.0f, height - 31.0f };
            DrawRectangleRounded(chestRec, 0.25f, 4, Fade(armorBase, alpha));
            DrawRectangleRoundedLines(chestRec, 0.25f, 4, 2.0f, Fade(goldTrim, alpha));
            DrawLineEx({ c.x, chestRec.y + 2.0f }, { c.x, chestRec.y + chestRec.height - 4.0f }, 2.0f, Fade(goldTrim, alpha));

            float pauldX = (facingDir >= 0.0f) ? (pos.x + 8.0f) : (GetRight() - 8.0f);
            DrawCircle(static_cast<int>(pauldX), static_cast<int>(pos.y + 18.0f), 7.0f, Fade(armorDark, alpha));
            DrawCircleLines(static_cast<int>(pauldX), static_cast<int>(pos.y + 18.0f), 7.0f, Fade(goldTrim, alpha));

            // 4. Рогатий шолом лицаря (Great Horned Helm)
            Vector2 helmC = { c.x, pos.y + 11.0f };
            DrawCircleV(helmC, 11.0f, Fade(armorBase, alpha));
            DrawCircleLines(static_cast<int>(helmC.x), static_cast<int>(helmC.y), 11.0f, Fade(pal.outline, alpha));

            // Величні сталеві роги шолома (головна деталь силуету у F8)
            DrawTriangle({ helmC.x - 6.0f, helmC.y - 4.0f }, { helmC.x - facingDir * 16.0f, helmC.y - 20.0f }, { helmC.x - 2.0f, helmC.y - 9.0f }, Fade(goldTrim, alpha));
            DrawTriangle({ helmC.x + 2.0f, helmC.y - 9.0f }, { helmC.x + facingDir * 16.0f, helmC.y - 20.0f }, { helmC.x + 6.0f, helmC.y - 4.0f }, Fade(goldTrim, alpha));

            // Хрестоподібне забрало та палаючі очі
            DrawRectangle(static_cast<int>(helmC.x + facingDir * 3.0f - 4.0f), static_cast<int>(helmC.y - 2.5f), 8, 4, pal.outline);
            DrawCircleV({ helmC.x + facingDir * 5.0f, helmC.y - 1.0f }, 2.5f, isTelegraphing ? pal.fireGold[3] : (tier == 2 ? pal.moonMagic[3] : pal.bloodCloth[3]));

            // 5. Величезний баштовий щит (Tower Shield / Pavise) з умбоною і геральдичним хрестом
            Rectangle shRec = GetShieldHitbox();
            DrawRectangleRounded(shRec, 0.18f, 4, Fade(pal.metal[1], alpha));
            DrawRectangleRoundedLines(shRec, 0.18f, 4, 3.0f, Fade(goldTrim, alpha));

            // Великий геральдичний хрест на щиті
            Vector2 shC = { shRec.x + shRec.width * 0.5f, shRec.y + shRec.height * 0.45f };
            DrawLineEx({ shRec.x + 2.0f, shC.y }, { shRec.x + shRec.width - 2.0f, shC.y }, 3.5f, Fade(goldTrim, alpha));
            DrawLineEx({ shC.x, shRec.y + 4.0f }, { shC.x, shRec.y + shRec.height - 4.0f }, 3.5f, Fade(goldTrim, alpha));

            // Центральна сталева умбона з шипом (Umbo boss)
            DrawCircleV(shC, 6.8f, Fade(goldTrim, alpha));
            DrawCircleV(shC, 4.0f, Fade(pal.metal[3], alpha));
            DrawCircleV(shC, 1.8f, Fade(pal.metal[4], alpha)); // шип умбони

            // Кутові металеві заклепки
            DrawCircleV({ shRec.x + 4.5f, shRec.y + 5.5f }, 1.8f, Fade(goldTrim, alpha));
            DrawCircleV({ shRec.x + shRec.width - 4.5f, shRec.y + 5.5f }, 1.8f, Fade(goldTrim, alpha));
            DrawCircleV({ shRec.x + 4.5f, shRec.y + shRec.height - 5.5f }, 1.8f, Fade(goldTrim, alpha));
            DrawCircleV({ shRec.x + shRec.width - 4.5f, shRec.y + shRec.height - 5.5f }, 1.8f, Fade(goldTrim, alpha));

            // 6. Меч лицаря, що визирає з-за щита
            Vector2 swHilt = { c.x - facingDir * 8.0f, pos.y + 24.0f };
            Vector2 swTip  = { c.x + facingDir * (isTelegraphing ? 38.0f : 16.0f), pos.y + (isTelegraphing ? 8.0f : 2.0f) };
            DrawLineEx(swHilt, swTip, 5.0f, Fade(pal.metal[4], alpha));
            DrawLineEx({ swHilt.x - 3.0f, swHilt.y + 3.0f }, { swHilt.x + 3.0f, swHilt.y - 3.0f }, 4.0f, Fade(goldTrim, alpha));
            DrawCircleV(swTip, 2.5f, Fade(pal.metal[4], alpha));
            break;
        }

        case EnemyType::FireMage: {
            Color robeCol = ColorTint(pal.bloodCloth[1], tierTint);
            Color trimCol = (tier == 2) ? pal.moonMagic[3] : ((tier == 3) ? pal.fireGold[3] : pal.fireGold[2]);

            // Тінь левітації
            DrawEllipse(static_cast<int>(c.x), static_cast<int>(GetBottom() + 3.0f), 14.0f, 4.0f, Fade(pal.outline, 0.35f * alpha));

            // 1. Складчаста мантія чаклуна
            float hoverWave = std::sin(animTimer * 6.0f) * 3.0f;
            Vector2 robeTop = { c.x, pos.y + 8.0f + hoverWave };
            Vector2 robeBL  = { pos.x - 2.0f, GetBottom() - 2.0f + hoverWave };
            Vector2 robeBR  = { GetRight() + 2.0f, GetBottom() - 2.0f + hoverWave };
            DrawTriangle(robeBR, robeTop, robeBL, Fade(robeCol, alpha));

            // Золотий рунічний поділ мантії
            DrawLineEx(robeBL, robeBR, 3.5f, Fade(trimCol, alpha));
            for (int rk = 1; rk <= 3; ++rk) {
                float rkx = robeBL.x + (robeBR.x - robeBL.x) * (rk * 0.25f);
                DrawCircle(static_cast<int>(rkx), static_cast<int>(robeBL.y - 5.0f), 2.0f, Fade(trimCol, alpha));
            }

            // 2. Глибокий капюшон з пітьмою всередині
            Vector2 hoodC = { c.x, pos.y + 12.0f + hoverWave };
            DrawCircleV(hoodC, 10.0f, Fade(robeCol, alpha));
            DrawCircleV(hoodC, 7.5f, Fade(pal.outline, alpha));
            // Палаючі очі мага
            DrawCircleV({ hoodC.x + facingDir * 3.5f, hoodC.y - 0.5f }, 2.5f, isTelegraphing ? pal.fireGold[3] : pal.bloodCloth[3]);
            DrawCircleV({ hoodC.x + facingDir * 3.5f, hoodC.y - 0.5f }, 1.2f, pal.bone[4]);

            // 3. Магічний посох із вогняним кристалом
            Vector2 staffBase = { c.x - facingDir * 12.0f, GetBottom() + hoverWave };
            Vector2 staffTop  = { c.x - facingDir * 12.0f, pos.y - 6.0f + hoverWave };
            DrawLineEx(staffBase, staffTop, 3.5f, Fade(pal.wood[1], alpha));
            float orbPulse = 1.0f + 0.25f * std::sin(animTimer * 16.0f);
            DrawCircleV(staffTop, 6.5f * orbPulse, Fade(pal.fireGold[2], alpha * 0.85f));
            DrawCircleV(staffTop, 3.5f * orbPulse, Fade(pal.fireGold[4], alpha));

            if (isTelegraphing) {
                DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 26.0f + std::sin(animTimer * 25.0f) * 5.0f, Fade(pal.fireGold[3], 0.7f));
                for (int i = -1; i <= 1; ++i) {
                    Vector2 mark = { lockedAttackTarget.x + i * 52.0f, lockedAttackTarget.y };
                    DrawCircleV(mark, 12.0f + std::sin(animTimer * 20.0f) * 3.0f, Fade(pal.danger[2], 0.65f));
                    DrawCircleLines(static_cast<int>(mark.x), static_cast<int>(mark.y), 15.0f, pal.fireGold[3]);
                }
            }
            break;
        }

        case EnemyType::Warg: {
            // Обов'язково яскравіший за фон мінімум на 25% (фон <= 25%, варг >= 50%)
            Color furBase = Color{ 175, 125, 75, 255 };  // Тепле рудо-вовче хутро (~55% яскравість)
            Color furHi   = Color{ 215, 170, 115, 255 }; // Світловий блік зверху (~68%)
            Color furMane = Color{ 90,  55,  35, 255 };  // Темна загривка
            Color furBelly= Color{ 220, 195, 155, 255 }; // Світле черевце (~75%)

            if (tier == 2) {
                furBase = Color{ 150, 170, 205, 255 }; // Крижаний лютововк
                furHi   = Color{ 205, 225, 255, 255 };
                furMane = Color{ 55,  65,  95, 255 };
                furBelly= Color{ 225, 240, 255, 255 };
            } else if (tier == 3) {
                furBase = Color{ 215, 115, 65, 255 };  // Демонічний альфа-варг
                furHi   = Color{ 255, 185, 105, 255 };
                furMane = Color{ 105, 30,  20, 255 };
                furBelly= Color{ 255, 220, 160, 255 };
            }

            // 1. М'язисте вовче тіло з вираженим хребтом та скуйовдженим загривком
            Rectangle bodyRec = { pos.x + 6.0f, pos.y + 11.0f, width - 12.0f, height - 18.0f };
            DrawRectangleRounded(bodyRec, 0.40f, 4, Fade(furBase, alpha));
            // Світле підчерев'я
            DrawRectangleRounded({ bodyRec.x + 2.0f, bodyRec.y + bodyRec.height * 0.45f, bodyRec.width - 4.0f, bodyRec.height * 0.55f }, 0.35f, 4, Fade(furBelly, alpha));
            // Блік на спині зверху-ліворуч
            DrawLineEx({ bodyRec.x + 3.0f, bodyRec.y + 2.0f }, { bodyRec.x + bodyRec.width - 3.0f, bodyRec.y + 2.0f }, 2.5f, Fade(furHi, alpha));

            // Скуйовджений загривок (Shaggy jagged mane crest)
            for (int m = 0; m < 5; ++m) {
                float mx = bodyRec.x + 4.0f + m * 7.5f;
                DrawTriangle({ mx - 4.0f, bodyRec.y + 2.0f }, { mx + 4.0f, bodyRec.y + 2.0f }, { mx - facingDir * 1.5f, bodyRec.y - 7.5f }, Fade(furMane, alpha));
                DrawTriangle({ mx - 2.0f, bodyRec.y + 1.0f }, { mx + 2.0f, bodyRec.y + 1.0f }, { mx - facingDir * 1.5f, bodyRec.y - 6.0f }, Fade(furHi, alpha * 0.5f));
            }

            // 2. Голова лютововка з гострими вухами та хижими іклами
            float headX = (facingDir >= 0.0f) ? (GetRight() - 6.0f) : (pos.x + 6.0f);
            Vector2 headC = { headX, pos.y + 13.0f };
            DrawCircleV(headC, 10.0f, Fade(furBase, alpha));
            DrawCircleSector(headC, 10.0f, 180.0f, 360.0f, 12, Fade(furHi, alpha * 0.65f));

            // Гострі трикутні вуха
            DrawTriangle({ headC.x - facingDir * 4.0f, headC.y - 8.0f }, { headC.x + facingDir * 2.0f, headC.y - 8.0f }, { headC.x - facingDir * 2.5f, headC.y - 17.0f }, Fade(furMane, alpha));
            DrawTriangle({ headC.x - facingDir * 3.0f, headC.y - 8.0f }, { headC.x + facingDir * 1.0f, headC.y - 8.0f }, { headC.x - facingDir * 2.0f, headC.y - 14.5f }, Fade(furHi, alpha));

            // Довга хижа морда з іклами
            Vector2 snout = { headC.x + facingDir * 8.5f, headC.y + 3.0f };
            DrawRectangleRec({ snout.x - 3.5f, snout.y - 2.5f, 7.0f, 6.0f }, Fade(furBase, alpha));
            DrawRectangleRec({ snout.x + facingDir * 2.0f, snout.y - 2.0f, 3.5f, 2.5f }, Fade(pal.outline, alpha)); // мочка носа
            // Великі білі ікла (Fangs)
            DrawTriangle({ snout.x - 1.0f, snout.y + 3.0f }, { snout.x + 2.0f, snout.y + 3.0f }, { snout.x + facingDir * 1.5f, snout.y + 7.5f }, Fade(pal.bone[4], alpha));
            DrawTriangle({ snout.x - facingDir * 3.0f, snout.y + 3.0f }, { snout.x - facingDir * 1.0f, snout.y + 3.0f }, { snout.x - facingDir * 2.0f, snout.y + 6.0f }, Fade(pal.bone[4], alpha));

            // Палаючі світні очі хижака (емісив)
            Vector2 eyePos = { headC.x + facingDir * 4.2f, headC.y - 1.0f };
            DrawCircleV(eyePos, 3.0f, Fade(pal.outline, alpha));
            Color wargEyeGlow = (tier == 2) ? pal.moonMagic[3] : pal.fireGold[3];
            DrawCircleV(eyePos, 2.2f, Fade(wargEyeGlow, alpha));
            DrawCircleV(eyePos, 1.0f, Fade(pal.metal[4], alpha));

            // 3. М'язисті лапи — чотирифазний цикл бігу (4 phases: reach, contact, compression, push-off)
            float cycle = std::fmod(animTimer * 14.0f, 4.0f);
            int phase = static_cast<int>(std::floor(cycle));
            float fPhaseOffset = cycle - phase;

            float fLegAngle = 0.0f;
            float rLegAngle = 0.0f;
            switch (phase) {
                case 0: // Phase 0: Reach (передня лапа витягнута вперед, задня відштовхується)
                    fLegAngle = 7.0f + fPhaseOffset * 3.0f;
                    rLegAngle = -6.0f - fPhaseOffset * 2.0f;
                    break;
                case 1: // Phase 1: Contact (передня торкається землі, задня підтягується)
                    fLegAngle = 10.0f - fPhaseOffset * 8.0f;
                    rLegAngle = -8.0f + fPhaseOffset * 6.0f;
                    break;
                case 2: // Phase 2: Compression (усі лапи згруповані під спиною)
                    fLegAngle = 2.0f - fPhaseOffset * 9.0f;
                    rLegAngle = -2.0f + fPhaseOffset * 9.0f;
                    break;
                default: // Phase 3: Drive (задня лапа робить потужний поштовх)
                    fLegAngle = -7.0f + fPhaseOffset * 14.0f;
                    rLegAngle = 7.0f - fPhaseOffset * 13.0f;
                    break;
            }

            // Передня м'язиста лапа
            Vector2 fShoulder = { headX - facingDir * 5.0f, GetBottom() - 11.0f };
            Vector2 fPaw      = { headX + facingDir * fLegAngle, GetBottom() };
            DrawLineEx(fShoulder, fPaw, 4.8f, Fade(furBase, alpha));
            DrawCircleV(fPaw, 2.8f, Fade(furMane, alpha));
            DrawCircleV({ fPaw.x + facingDir * 1.5f, fPaw.y + 0.5f }, 1.2f, Fade(pal.outline, alpha)); // пазурі

            // Задня м'язиста лапа
            float rearX = (facingDir >= 0.0f) ? (pos.x + 10.0f) : (GetRight() - 10.0f);
            Vector2 rHip = { rearX, GetBottom() - 12.0f };
            Vector2 rPaw = { rearX + facingDir * rLegAngle, GetBottom() };
            DrawLineEx(rHip, rPaw, 5.2f, Fade(furBase, alpha));
            DrawCircleV(rPaw, 3.0f, Fade(furMane, alpha));
            DrawCircleV({ rPaw.x + facingDir * 1.5f, rPaw.y + 0.5f }, 1.2f, Fade(pal.outline, alpha));

            // 4. Пухнастий вовчий хвіст
            float tailBaseX = (facingDir >= 0.0f) ? pos.x : GetRight();
            float tailWave = std::sin(animTimer * 12.0f) * 4.5f;
            Vector2 t1 = { tailBaseX, pos.y + 13.0f };
            Vector2 t2 = { tailBaseX - facingDir * 9.0f, pos.y + 8.0f + tailWave * 0.5f };
            Vector2 t3 = { tailBaseX - facingDir * 17.0f, pos.y + 5.0f + tailWave };
            DrawLineEx(t1, t2, 5.0f, Fade(furBase, alpha));
            DrawLineEx(t2, t3, 4.0f, Fade(furHi, alpha));
            DrawCircleV(t3, 2.5f, Fade(furMane, alpha));
            break;
        }

        case EnemyType::WallTurret: {
            // Готична горгулья з рогами й іклами (Gothic Stone Gargoyle)
            Rectangle b = GetBounds();
            Color stoneBase   = ColorTint(pal.stoneTile[3], tierTint);
            Color stoneShadow = ColorTint(pal.stoneTile[1], tierTint);
            Color stoneHi     = ColorTint(pal.stoneTile[4], tierTint);

            // 1. Різьблений кам'яний кронштейн (Corbel bracket)
            DrawRectangleRec({ b.x, b.y + b.height - 10.0f, b.width, 10.0f }, stoneShadow);
            DrawLineEx({ b.x, b.y + b.height - 10.0f }, { b.x + b.width, b.y + b.height - 10.0f }, 2.0f, stoneHi);
            // Кам'яні кігті горгульї, що стискають карниз
            for (int cl = 0; cl < 3; ++cl) {
                float clx = b.x + 6.0f + cl * 8.0f;
                DrawRectangle(static_cast<int>(clx), static_cast<int>(b.y + b.height - 12.0f), 3, 5, stoneHi);
            }

            // 2. Згорблене кам'яне тіло горгульї зі складеними крилами
            Vector2 bodyC = { c.x - facingDir * 3.0f, c.y + 2.0f };
            DrawCircleV(bodyC, 13.0f, stoneBase);
            DrawCircleSector(bodyC, 13.0f, 135.0f, 315.0f, 10, stoneHi);
            // Складені крила кажана за спиною
            Vector2 wingBase = { bodyC.x - facingDir * 7.0f, bodyC.y - 2.0f };
            Vector2 wingTip  = { bodyC.x - facingDir * 15.0f, b.y + 2.0f };
            DrawTriangle(wingBase, wingTip, { wingBase.x, bodyC.y + 8.0f }, stoneShadow);
            DrawLineEx(wingBase, wingTip, 2.2f, stoneHi);

            // 3. Голова горгульї з рогами та вухами
            Vector2 headC = { c.x + facingDir * 5.0f, c.y - 5.0f };
            DrawCircleV(headC, 9.5f, stoneBase);
            DrawCircleLines(static_cast<int>(headC.x), static_cast<int>(headC.y), 9.5f, pal.outline);

            // Закручені кам'яні демонічні роги
            DrawTriangle({ headC.x - 3.0f, headC.y - 6.0f }, { headC.x - facingDir * 12.0f, headC.y - 18.0f }, { headC.x + 3.0f, headC.y - 7.0f }, stoneShadow);
            DrawLineEx({ headC.x, headC.y - 6.0f }, { headC.x - facingDir * 12.0f, headC.y - 18.0f }, 2.0f, stoneHi);

            // 4. Паща горгульї (телеграф: паща розжарюється перед пострілом)
            Vector2 mawC = { headC.x + facingDir * 7.0f, headC.y + 3.0f };
            float charge = isTelegraphing ? (0.75f + 0.25f * std::sin(animTimer * 28.0f)) : 0.08f;

            // Зяюча паща
            DrawCircleV(mawC, 8.0f, pal.outline);
            // Гострі кам'яні ікла (Fangs)
            DrawTriangle({ mawC.x - 2.0f, mawC.y - 7.0f }, { mawC.x + 2.0f, mawC.y - 7.0f }, { mawC.x, mawC.y - 2.0f }, stoneHi);
            DrawTriangle({ mawC.x - 2.0f, mawC.y + 7.0f }, { mawC.x + 2.0f, mawC.y + 7.0f }, { mawC.x, mawC.y + 2.0f }, stoneHi);

            // Розпечена лава всередині пащі (Maw glow)
            Color lavaCore = isTelegraphing ? pal.fireGold[4] : pal.bloodCloth[2];
            Color lavaGlow = isTelegraphing ? pal.danger[2] : pal.bloodCloth[1];
            DrawCircleV(mawC, 6.0f * (0.8f + charge * 0.4f), Fade(lavaGlow, charge));
            DrawCircleV(mawC, 3.2f * (0.8f + charge * 0.4f), Fade(lavaCore, charge));

            // Палаючі червоні очі горгульї
            Vector2 gEye = { headC.x + facingDir * 2.5f, headC.y - 2.5f };
            DrawCircleV(gEye, 2.2f, isTelegraphing ? pal.fireGold[3] : pal.bloodCloth[3]);
            DrawCircleV(gEye, 1.0f, pal.metal[4]);
            break;
        }

        default:
            break;
    }

    // Смужка здоров'я над пораненим ворогом та індикатор оглушення (Stun)
    if (hp < maxHp && !isDementiaPhantom && type != EnemyType::WallTurret) {
        float ratio = std::clamp(static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 1.0f);
        DrawRectangle(static_cast<int>(c.x - 18.0f), static_cast<int>(pos.y - 10.0f), 36, 5, pal.outline);
        DrawRectangle(static_cast<int>(c.x - 17.0f), static_cast<int>(pos.y - 9.0f), static_cast<int>(34.0f * ratio), 3, pal.bloodCloth[3]);
    }

    if (stunTimer > 0.0f) {
        float starX = c.x + std::cos(animTimer * 14.0f) * 12.0f;
        DrawCircleV({ starX, pos.y - 14.0f }, 4.0f, pal.fireGold[3]);
    }
}