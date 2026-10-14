// === FILE: src/entities/Enemy.cpp ===
#include "entities/Enemy.hpp"
#include "entities/Player.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ParticleSystem.hpp"
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

    Vector2 c = GetCenter();
    float alpha = isDementiaPhantom ? 0.55f : 1.0f;

    Color tierTint = WHITE;
    if (isDementiaPhantom) {
        tierTint = Color{ 190, 90, 255, 255 };
    } else if (tier == 2) {
        tierTint = Color{ 130, 205, 255, 255 };
    } else if (tier == 3) {
        tierTint = Color{ 255, 195, 70, 255 };
    }
    if (hurtFlashTimer > 0.0f) {
        tierTint = RED;
    }

    switch (type) {
        case EnemyType::Skeleton: {
            Color boneCol = isDementiaPhantom ? tierTint : ColorTint(Color{ 236, 232, 222, 255 }, tierTint);
            Color shadowBone = ColorTint(boneCol, Color{ 160, 155, 145, 255 });

            // 1. Череп з очницями та зубами
            float skullY = pos.y + 10.0f;
            DrawCircleV({ c.x, skullY }, 9.5f, Fade(boneCol, alpha));
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(skullY), 9.5f, Fade(shadowBone, alpha));

            // Шолом для тиру 2 або 3
            if (tier >= 2) {
                DrawCircleSector({ c.x, skullY - 1.0f }, 10.5f, 180.0f, 360.0f, 12, Fade(Color{ 75, 78, 88, 255 }, alpha));
                DrawLineEx({ c.x - 11.0f, skullY - 1.0f }, { c.x + 11.0f, skullY - 1.0f }, 2.5f, Fade(GOLD, alpha));
                if (tier == 3) {
                    DrawTriangle({ c.x - 2.0f, skullY - 11.0f }, { c.x + 2.0f, skullY - 11.0f }, { c.x, skullY - 17.0f }, Fade(GOLD, alpha));
                }
            }

            // Очниці та носова западина
            Vector2 eyeL = { c.x - 3.8f, skullY - 0.5f };
            Vector2 eyeR = { c.x + 3.8f, skullY - 0.5f };
            DrawCircleV(eyeL, 3.0f, Fade(BLACK, alpha));
            DrawCircleV(eyeR, 3.0f, Fade(BLACK, alpha));
            // Вогники очей
            DrawCircleV(eyeL, 1.6f, Fade(RED, alpha));
            DrawCircleV(eyeR, 1.6f, Fade(RED, alpha));
            // Носова порожнина
            DrawTriangle({ c.x, skullY + 2.0f }, { c.x - 1.6f, skullY + 4.5f }, { c.x + 1.6f, skullY + 4.5f }, Fade(BLACK, alpha));
            // Щелепа з зубами
            DrawRectangle(static_cast<int>(c.x - 5.0f), static_cast<int>(skullY + 5.0f), 10, 4, Fade(boneCol, alpha));
            for (int t = -3; t <= 3; t += 3) {
                DrawLine(static_cast<int>(c.x + t), static_cast<int>(skullY + 5.0f), static_cast<int>(c.x + t), static_cast<int>(skullY + 8.5f), Fade(BLACK, alpha * 0.7f));
            }

            // 2. Хребет та ребра
            DrawLineEx({ c.x, skullY + 8.0f }, { c.x, pos.y + 36.0f }, 4.0f, Fade(boneCol, alpha));
            for (int r = 0; r < 4; ++r) {
                float ry = pos.y + 20.0f + r * 4.5f;
                float rSpan = 8.5f - r * 0.8f;
                DrawLineEx({ c.x - rSpan, ry }, { c.x + rSpan, ry }, 2.5f, Fade(boneCol, alpha));
                DrawCircle(static_cast<int>(c.x - rSpan), static_cast<int>(ry), 1.2f, Fade(shadowBone, alpha));
                DrawCircle(static_cast<int>(c.x + rSpan), static_cast<int>(ry), 1.2f, Fade(shadowBone, alpha));
            }

            // Тазова кістка
            DrawRectangleRounded({ c.x - 7.0f, pos.y + 34.0f, 14.0f, 5.0f }, 0.4f, 4, Fade(boneCol, alpha));

            // 3. Рука зі старим мечем
            Vector2 shoulder = { c.x + facingDir * 6.0f, pos.y + 20.0f };
            Vector2 hand = { c.x + facingDir * 18.0f, pos.y + 26.0f };
            DrawLineEx(shoulder, hand, 3.0f, Fade(boneCol, alpha));
            // Іржавий меч скелета
            Vector2 bladeTip = { hand.x + facingDir * 22.0f, hand.y - 14.0f };
            DrawLineEx(hand, bladeTip, 4.5f, Fade(Color{ 145, 95, 75, 255 }, alpha));
            DrawLineEx({ hand.x - facingDir * 4.0f, hand.y + 3.0f }, { hand.x + facingDir * 4.0f, hand.y - 3.0f }, 3.0f, Fade(DARKGRAY, alpha)); // гарда
            DrawCircleV(bladeTip, 2.0f, Fade(LIGHTGRAY, alpha));

            // 4. Ноги, що крокують
            float step = std::sin(animTimer * 12.0f) * 6.0f;
            DrawLineEx({ c.x - 4.5f, pos.y + 36.0f }, { c.x - 6.0f + step, GetBottom() }, 3.5f, Fade(boneCol, alpha));
            DrawCircle(static_cast<int>(c.x - 6.0f + step), static_cast<int>(GetBottom() - 1.0f), 2.5f, Fade(shadowBone, alpha));
            DrawLineEx({ c.x + 4.5f, pos.y + 36.0f }, { c.x + 6.0f - step, GetBottom() }, 3.5f, Fade(boneCol, alpha));
            DrawCircle(static_cast<int>(c.x + 6.0f - step), static_cast<int>(GetBottom() - 1.0f), 2.5f, Fade(shadowBone, alpha));
            break;
        }

        case EnemyType::FloatingEye: {
            Color sclera = isTelegraphing ? Color{ 255, 115, 115, 255 } : ColorTint(Color{ 240, 235, 228, 255 }, tierTint);
            Color iris   = (tier == 1) ? Color{ 245, 150, 40, 255 } : ((tier == 2) ? Color{ 175, 45, 215, 255 } : Color{ 225, 35, 45, 255 });
            float floatPulse = 1.0f + 0.05f * std::sin(animTimer * 8.0f);

            // Тінь ширяння під оком
            DrawEllipse(static_cast<int>(c.x), static_cast<int>(pos.y + height + 4.0f), 12.0f, 4.0f, Fade(BLACK, 0.28f * alpha));

            // М'ясисті війкові шипи навколо ока
            for (int sp = 0; sp < 6; ++sp) {
                float sa = sp * (6.2831853f / 6.0f) + animTimer * 2.0f;
                Vector2 sBase = { c.x + std::cos(sa) * 15.0f, c.y + std::sin(sa) * 15.0f };
                Vector2 sTip  = { c.x + std::cos(sa) * 21.0f, c.y + std::sin(sa) * 21.0f };
                DrawLineEx(sBase, sTip, 2.5f, Fade(Color{ 160, 45, 60, 255 }, alpha));
            }

            // Очне яблуко
            DrawCircleV(c, 16.5f * floatPulse, Fade(sclera, alpha));
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 16.5f * floatPulse, Fade(Color{ 140, 35, 45, 255 }, alpha));

            // Червоні капілярні кровоносні судини
            for (int v = 0; v < 4; ++v) {
                float va = v * 1.57f + 0.3f;
                Vector2 v1 = { c.x + std::cos(va) * 15.0f, c.y + std::sin(va) * 15.0f };
                Vector2 v2 = { c.x + std::cos(va) * 9.0f, c.y + std::sin(va) * 9.0f };
                DrawLineEx(v1, v2, 1.2f, Fade(Color{ 200, 30, 45, 255 }, alpha * 0.75f));
            }

            // Райдужка з градієнтним сяйвом
            DrawCircleV(c, 9.0f, Fade(iris, alpha));
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 9.0f, Fade(ColorTint(iris, BLACK), alpha));

            // Зіниця (вертикальна хижа щілина)
            float pupilW = isTelegraphing ? 5.5f : 3.5f;
            DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), pupilW, 7.5f, BLACK);
            // Білий блик світла
            DrawCircleV({ c.x - 3.0f, c.y - 3.5f }, 2.0f, Fade(WHITE, alpha * 0.95f));

            if (isTelegraphing) {
                DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 22.0f + std::sin(animTimer * 20.0f) * 4.0f, Fade(RED, 0.75f));
            }
            break;
        }

        case EnemyType::ShieldKnight: {
            Color armorCol  = ColorTint(Color{ 95, 105, 125, 255 }, tierTint);
            Color armorDark = ColorTint(armorCol, Color{ 55, 60, 75, 255 });
            Color goldTrim  = (tier == 2) ? SKYBLUE : ((tier == 3) ? GOLD : Color{ 175, 145, 65, 255 });

            // 1. Поножі та сабатони
            float knightWalk = std::sin(animTimer * 10.0f) * 4.0f;
            DrawRectangleRec({ pos.x + 8.0f + knightWalk, GetBottom() - 15.0f, 9.0f, 15.0f }, Fade(armorCol, alpha));
            DrawRectangleRec({ pos.x + 23.0f - knightWalk, GetBottom() - 15.0f, 9.0f, 15.0f }, Fade(armorCol, alpha));
            // Наколінники
            DrawCircle(static_cast<int>(pos.x + 12.5f + knightWalk), static_cast<int>(GetBottom() - 15.0f), 4.5f, Fade(goldTrim, alpha));
            DrawCircle(static_cast<int>(pos.x + 27.5f - knightWalk), static_cast<int>(GetBottom() - 15.0f), 4.5f, Fade(goldTrim, alpha));

            // 2. Важка лицарська кіраса
            Rectangle chestRec = { pos.x + 6.0f, pos.y + 16.0f, width - 12.0f, height - 31.0f };
            DrawRectangleRounded(chestRec, 0.25f, 4, Fade(armorCol, alpha));
            DrawRectangleRoundedLines(chestRec, 0.25f, 4, 2.0f, Fade(goldTrim, alpha));
            DrawLineEx({ c.x, chestRec.y + 2.0f }, { c.x, chestRec.y + chestRec.height - 4.0f }, 2.0f, Fade(goldTrim, alpha));

            // Шиповані наплічники (Pauldrons)
            float pauldX = (facingDir >= 0.0f) ? (pos.x + 8.0f) : (GetRight() - 8.0f);
            DrawCircle(static_cast<int>(pauldX), static_cast<int>(pos.y + 18.0f), 7.0f, Fade(armorDark, alpha));
            DrawCircleLines(static_cast<int>(pauldX), static_cast<int>(pos.y + 18.0f), 7.0f, Fade(goldTrim, alpha));
            DrawTriangle({ pauldX - 3.0f, pos.y + 13.0f }, { pauldX + 3.0f, pos.y + 13.0f }, { pauldX, pos.y + 6.0f }, Fade(goldTrim, alpha));

            // 3. Великий лицарський шолом (Great Helm)
            Vector2 helmC = { c.x, pos.y + 11.0f };
            DrawCircleV(helmC, 11.0f, Fade(armorCol, alpha));
            DrawCircleLines(static_cast<int>(helmC.x), static_cast<int>(helmC.y), 11.0f, Fade(BLACK, alpha));

            // Плюмаж на шоломі лицаря
            Color plumeColor = (tier == 1) ? RED : ((tier == 2) ? SKYBLUE : GOLD);
            float pWave = std::sin(animTimer * 12.0f) * 3.5f;
            DrawLineEx({ helmC.x, helmC.y - 10.0f }, { helmC.x - facingDir * 12.0f, helmC.y - 18.0f + pWave }, 4.5f, Fade(plumeColor, alpha));

            // Хрестоподібне забрало та палаючі очі
            DrawRectangle(static_cast<int>(helmC.x + facingDir * 3.0f - 4.0f), static_cast<int>(helmC.y - 2.5f), 8, 4, BLACK);
            DrawCircleV({ helmC.x + facingDir * 5.0f, helmC.y - 1.0f }, 2.5f, isTelegraphing ? YELLOW : (tier == 2 ? SKYBLUE : RED));

            // 4. Величезний баштовий щит (Tower Shield / Pavise)
            Rectangle shRec = GetShieldHitbox();
            DrawRectangleRounded(shRec, 0.18f, 4, Fade(Color{ 45, 52, 68, 255 }, alpha));
            DrawRectangleRoundedLines(shRec, 0.18f, 4, 3.0f, Fade(goldTrim, alpha));
            // Металевий умбон щита та заклепки
            Vector2 shC = { shRec.x + shRec.width * 0.5f, shRec.y + shRec.height * 0.42f };
            DrawCircleV(shC, 6.5f, Fade(goldTrim, alpha));
            DrawCircleV(shC, 3.0f, Fade(WHITE, alpha * 0.8f));
            DrawCircleV({ shRec.x + 5.0f, shRec.y + 6.0f }, 2.0f, Fade(goldTrim, alpha));
            DrawCircleV({ shRec.x + shRec.width - 5.0f, shRec.y + 6.0f }, 2.0f, Fade(goldTrim, alpha));
            DrawCircleV({ shRec.x + 5.0f, shRec.y + shRec.height - 6.0f }, 2.0f, Fade(goldTrim, alpha));
            DrawCircleV({ shRec.x + shRec.width - 5.0f, shRec.y + shRec.height - 6.0f }, 2.0f, Fade(goldTrim, alpha));

            // 5. Меч лицаря, що визирає з-за щита
            Vector2 swHilt = { c.x - facingDir * 8.0f, pos.y + 24.0f };
            Vector2 swTip  = { c.x + facingDir * (isTelegraphing ? 38.0f : 16.0f), pos.y + (isTelegraphing ? 8.0f : 2.0f) };
            DrawLineEx(swHilt, swTip, 5.0f, Fade(Color{ 220, 228, 240, 255 }, alpha));
            DrawLineEx({ swHilt.x - 3.0f, swHilt.y + 3.0f }, { swHilt.x + 3.0f, swHilt.y - 3.0f }, 4.0f, Fade(goldTrim, alpha));
            DrawCircleV(swTip, 2.5f, Fade(WHITE, alpha));
            break;
        }

        case EnemyType::FireMage: {
            Color robeCol = ColorTint(Color{ 145, 32, 42, 255 }, tierTint);
            Color trimCol = (tier == 2) ? SKYBLUE : ((tier == 3) ? GOLD : ORANGE);

            // Тінь левітації
            DrawEllipse(static_cast<int>(c.x), static_cast<int>(GetBottom() + 3.0f), 14.0f, 4.0f, Fade(BLACK, 0.35f * alpha));

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
            DrawCircleV(hoodC, 7.5f, Fade(Color{ 16, 12, 22, 255 }, alpha)); // темрява всередині
            // Палаючі очі мага
            DrawCircleV({ hoodC.x + facingDir * 3.5f, hoodC.y - 0.5f }, 2.5f, isTelegraphing ? YELLOW : RED);
            DrawCircleV({ hoodC.x + facingDir * 3.5f, hoodC.y - 0.5f }, 1.2f, WHITE);

            // 3. Магічний посох із вогняним кристалом
            Vector2 staffBase = { c.x - facingDir * 12.0f, GetBottom() + hoverWave };
            Vector2 staffTop  = { c.x - facingDir * 12.0f, pos.y - 6.0f + hoverWave };
            DrawLineEx(staffBase, staffTop, 3.5f, Fade(Color{ 68, 48, 34, 255 }, alpha));
            // Вогняне ядро на верхівці посоха
            float orbPulse = 1.0f + 0.25f * std::sin(animTimer * 16.0f);
            DrawCircleV(staffTop, 6.5f * orbPulse, Fade(ORANGE, alpha * 0.85f));
            DrawCircleV(staffTop, 3.5f * orbPulse, Fade(YELLOW, alpha));

            if (isTelegraphing) {
                // Аура заряджання закляття навколо мага
                DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 26.0f + std::sin(animTimer * 25.0f) * 5.0f, Fade(ORANGE, 0.7f));
                for (int i = -1; i <= 1; ++i) {
                    Vector2 mark = { lockedAttackTarget.x + i * 52.0f, lockedAttackTarget.y };
                    DrawCircleV(mark, 12.0f + std::sin(animTimer * 20.0f) * 3.0f, Fade(RED, 0.65f));
                    DrawCircleLines(static_cast<int>(mark.x), static_cast<int>(mark.y), 15.0f, ORANGE);
                }
            }
            break;
        }

        case EnemyType::Warg: {
            Color furCol   = ColorTint(Color{ 85, 68, 58, 255 }, tierTint);
            Color darkMane = ColorTint(furCol, Color{ 48, 38, 32, 255 });

            // 1. М'язисте хутряне тіло з гребенем шерсті
            Rectangle bodyRec = { pos.x + 6.0f, pos.y + 10.0f, width - 12.0f, height - 16.0f };
            DrawRectangleRounded(bodyRec, 0.35f, 4, Fade(furCol, alpha));
            // Зубчаста загривка (mane)
            for (int m = 0; m < 4; ++m) {
                float mx = bodyRec.x + 6.0f + m * 8.0f;
                DrawTriangle({ mx - 4.0f, bodyRec.y }, { mx + 4.0f, bodyRec.y }, { mx, bodyRec.y - 5.0f }, Fade(darkMane, alpha));
            }

            // 2. Голова лютововка з гострими вухами та іклами
            float headX = (facingDir >= 0.0f) ? (GetRight() - 6.0f) : (pos.x + 6.0f);
            Vector2 headC = { headX, pos.y + 12.0f };
            DrawCircleV(headC, 9.5f, Fade(furCol, alpha));
            // Вухо
            DrawTriangle({ headC.x - facingDir * 3.0f, headC.y - 7.0f }, { headC.x + facingDir * 2.0f, headC.y - 7.0f }, { headC.x - facingDir * 2.0f, headC.y - 15.0f }, Fade(darkMane, alpha));
            // Морда та ікла
            Vector2 snout = { headC.x + facingDir * 8.0f, headC.y + 3.0f };
            DrawRectangleRec({ snout.x - 3.0f, snout.y - 2.0f, 6.0f, 5.0f }, Fade(furCol, alpha));
            DrawTriangle({ snout.x, snout.y + 3.0f }, { snout.x + facingDir * 2.0f, snout.y + 3.0f }, { snout.x + facingDir * 1.0f, snout.y + 7.0f }, Fade(WHITE, alpha)); // біле ікло
            // Око хижака
            DrawCircleV({ headC.x + facingDir * 4.0f, headC.y - 1.0f }, 2.8f, RED);
            DrawCircleV({ headC.x + facingDir * 4.0f, headC.y - 1.0f }, 1.2f, YELLOW);

            // 3. Лапи з пазурами, що біжать
            float legRun = std::sin(animTimer * 18.0f) * 7.0f;
            // Передня лапа
            Vector2 fLeg1 = { headX - facingDir * 4.0f, GetBottom() - 10.0f };
            Vector2 fLeg2 = { headX + facingDir * legRun, GetBottom() };
            DrawLineEx(fLeg1, fLeg2, 4.0f, Fade(darkMane, alpha));
            DrawCircleV(fLeg2, 2.5f, Fade(BLACK, alpha)); // кіготь
            // Задня лапа
            float rearX = (facingDir >= 0.0f) ? (pos.x + 10.0f) : (GetRight() - 10.0f);
            Vector2 rLeg1 = { rearX, GetBottom() - 10.0f };
            Vector2 rLeg2 = { rearX - facingDir * legRun, GetBottom() };
            DrawLineEx(rLeg1, rLeg2, 4.0f, Fade(darkMane, alpha));
            DrawCircleV(rLeg2, 2.5f, Fade(BLACK, alpha));

            // 4. Пухнастий вовчий хвіст
            float tailX = (facingDir >= 0.0f) ? pos.x : GetRight();
            float tailWave = std::sin(animTimer * 12.0f) * 4.0f;
            DrawLineEx({ tailX, pos.y + 14.0f }, { tailX - facingDir * 14.0f, pos.y + 8.0f + tailWave }, 4.5f, Fade(furCol, alpha));
            break;
        }

        case EnemyType::WallTurret: {
            Color stoneCol = isTelegraphing ? Color{ 225, 110, 75, 255 } : ColorTint(Color{ 105, 112, 126, 255 }, tierTint);
            Rectangle b = GetBounds();

            // Кам'яна основа з кладкою
            DrawRectangleRec(b, stoneCol);
            DrawRectangleLinesEx(b, 2.5f, Color{ 48, 52, 60, 255 });
            DrawLineEx({ b.x, b.y + b.height * 0.5f }, { b.x + b.width, b.y + b.height * 0.5f }, 1.5f, Color{ 48, 52, 60, 255 });

            // Металеві заклепки по кутах
            DrawCircle(static_cast<int>(b.x + 4.0f), static_cast<int>(b.y + 4.0f), 2.0f, GOLD);
            DrawCircle(static_cast<int>(b.x + b.width - 4.0f), static_cast<int>(b.y + 4.0f), 2.0f, GOLD);
            DrawCircle(static_cast<int>(b.x + 4.0f), static_cast<int>(b.y + b.height - 4.0f), 2.0f, GOLD);
            DrawCircle(static_cast<int>(b.x + b.width - 4.0f), static_cast<int>(b.y + b.height - 4.0f), 2.0f, GOLD);

            // Сопло гармати / паща горгульї
            Vector2 cannonC = { c.x + facingDir * 8.0f, c.y };
            DrawCircleV(cannonC, 10.0f, Color{ 36, 38, 44, 255 });
            DrawCircleLines(static_cast<int>(cannonC.x), static_cast<int>(cannonC.y), 10.0f, BLACK);

            // Палаюче жерло гармати перед пострілом
            float charge = isTelegraphing ? (0.7f + 0.3f * std::sin(animTimer * 25.0f)) : 0.2f;
            DrawCircleV(cannonC, 6.0f, Fade(isTelegraphing ? ORANGE : RED, charge * 0.9f));
            DrawCircleV(cannonC, 3.0f, Fade(YELLOW, charge));
            break;
        }

        default:
            break;
    }

    // Смужка здоров'я над пораненим ворогом та індикатор оглушення (Stun)
    if (hp < maxHp && !isDementiaPhantom && type != EnemyType::WallTurret) {
        float ratio = std::clamp(static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 1.0f);
        DrawRectangle(static_cast<int>(c.x - 18.0f), static_cast<int>(pos.y - 10.0f), 36, 5, BLACK);
        DrawRectangle(static_cast<int>(c.x - 17.0f), static_cast<int>(pos.y - 9.0f), static_cast<int>(34.0f * ratio), 3, RED);
    }

    if (stunTimer > 0.0f) {
        float starX = c.x + std::cos(animTimer * 14.0f) * 12.0f;
        DrawCircleV({ starX, pos.y - 14.0f }, 4.0f, YELLOW);
    }
}