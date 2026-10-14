// === FILE: src/entities/Interactable.cpp ===
#include "entities/Interactable.hpp"
#include "entities/Player.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "data/Registry.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ParticleSystem.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace {
    inline float RandSym(float mag) {
        float r = (static_cast<float>(std::rand() % 2000) / 1000.0f) - 1.0f;
        return r * mag;
    }
}

Interactable Interactable::MakeTorch(Vector2 centerPos) {
    Interactable it;
    it.kind = InteractableKind::Torch;
    it.width = 22.0f;
    it.height = 38.0f;
    it.SetCenter(centerPos);
    it.hp = 1;
    it.maxHp = 1;
    return it;
}

Interactable Interactable::MakeBreakableProp(Vector2 bottomCenter, int variant) {
    Interactable it;
    it.kind = InteractableKind::BreakableProp;
    it.subVariant = variant % 3;
    it.width = 34.0f;
    it.height = (it.subVariant == 1) ? 46.0f : 36.0f;
    it.pos = { bottomCenter.x - it.width * 0.5f, bottomCenter.y - it.height };
    it.hp = 1;
    it.maxHp = 1;
    return it;
}

Interactable Interactable::MakeChest(Vector2 bottomCenter, bool silver, bool alreadyOpened) {
    Interactable it;
    it.kind = InteractableKind::Chest;
    it.isSilverChest = silver;
    it.isOpened = alreadyOpened;
    it.width = 44.0f;
    it.height = 34.0f;
    it.pos = { bottomCenter.x - it.width * 0.5f, bottomCenter.y - it.height };
    return it;
}

Interactable Interactable::MakeFairyChest(Vector2 bottomCenter, FairyCondition cond, bool alreadyOpened) {
    Interactable it;
    it.kind = InteractableKind::FairyChest;
    it.fairyCond = cond;
    it.isOpened = alreadyOpened;
    it.isLocked = !alreadyOpened;
    it.width = 46.0f;
    it.height = 36.0f;
    it.pos = { bottomCenter.x - it.width * 0.5f, bottomCenter.y - it.height };
    return it;
}

Interactable Interactable::MakeTeleporter(Vector2 bottomCenter) {
    Interactable it;
    it.kind = InteractableKind::Teleporter;
    it.width = 56.0f;
    it.height = 72.0f;
    it.pos = { bottomCenter.x - it.width * 0.5f, bottomCenter.y - it.height };
    return it;
}

Interactable Interactable::MakeBossDoor(Vector2 bottomCenter, BossType targetBoss) {
    Interactable it;
    it.kind = InteractableKind::BossDoor;
    it.bossDoorTarget = targetBoss;
    it.width = 88.0f;
    it.height = 128.0f;
    it.pos = { bottomCenter.x - it.width * 0.5f, bottomCenter.y - it.height };
    return it;
}

void Interactable::SpawnLootBurst(
    std::vector<Interactable>& outDrops,
    Vector2 origin,
    int baseGoldAmount,
    bool canDropConsumables
) {
    int remaining = std::max(0, baseGoldAmount);
    while (remaining >= 50) {
        remaining -= 50;
        Interactable bag;
        bag.kind = InteractableKind::DroppedCoin;
        bag.subVariant = 1; // Gold Bag (50g)
        bag.goldValue = 50;
        bag.width = 20.0f;
        bag.height = 22.0f;
        bag.SetCenter(origin);
        bag.vel = { RandSym(190.0f), -260.0f - static_cast<float>(std::rand() % 180) };
        bag.pickupDelay = 0.22f;
        outDrops.push_back(bag);
    }

    int coinCount = std::max(0, remaining / 10);
    if (baseGoldAmount > 0 && coinCount == 0 && outDrops.empty()) {
        coinCount = 1;
    }

    for (int i = 0; i < coinCount; ++i) {
        Interactable coin;
        coin.kind = InteractableKind::DroppedCoin;
        coin.subVariant = 0; // Gold Coin (10g)
        coin.goldValue = 10;
        coin.width = 14.0f;
        coin.height = 14.0f;
        coin.SetCenter(origin);
        coin.vel = { RandSym(220.0f), -220.0f - static_cast<float>(std::rand() % 200) };
        coin.pickupDelay = 0.18f;
        outDrops.push_back(coin);
    }

    if (canDropConsumables) {
        int roll = std::rand() % 100;
        if (roll < 9) {
            Interactable meat;
            meat.kind = InteractableKind::MeatDrop;
            meat.width = 24.0f;
            meat.height = 18.0f;
            meat.SetCenter(origin);
            meat.vel = { RandSym(110.0f), -280.0f };
            meat.pickupDelay = 0.25f;
            outDrops.push_back(meat);
        } else if (roll < 20) {
            Interactable potion;
            potion.kind = InteractableKind::ManaDrop;
            potion.width = 18.0f;
            potion.height = 22.0f;
            potion.SetCenter(origin);
            potion.vel = { RandSym(110.0f), -280.0f };
            potion.pickupDelay = 0.25f;
            outDrops.push_back(potion);
        }
    }
}

void Interactable::Update(
    float dt,
    GameContext& ctx,
    const ITileMap* tileMap,
    Player& player,
    std::vector<Interactable>& spawnedDrops
) {
    if (!active) return;
    animTimer += dt;

    // 1. Фізика падаючих монет, курячих ніжок та зілля мани
    if (kind == InteractableKind::DroppedCoin ||
        kind == InteractableKind::MeatDrop ||
        kind == InteractableKind::ManaDrop) {

        if (pickupDelay > 0.0f) {
            pickupDelay -= dt;
        }

        lifetime -= dt;
        if (lifetime <= 0.0f) {
            active = false;
            return;
        }

        vel.y += Config::GRAVITY * 0.85f * dt;
        if (vel.y > Config::MAX_FALL_SPEED) vel.y = Config::MAX_FALL_SPEED;

        float nextX = pos.x + vel.x * dt;
        if (tileMap) {
            int col = tileMap->WorldToCol(nextX + (vel.x >= 0.0f ? width : 0.0f));
            int row = tileMap->WorldToRow(GetCenter().y);
            if (tileMap->IsSolidAt(col, row, false)) {
                vel.x = -vel.x * 0.5f;
                nextX = pos.x;
            }
        }
        pos.x = nextX;

        float prevBottom = GetBottom();
        pos.y += vel.y * dt;
        if (tileMap && vel.y >= 0.0f) {
            int cMid = tileMap->WorldToCol(GetCenter().x);
            int rBot = tileMap->WorldToRow(GetBottom());
            if (tileMap->IsSolidAt(cMid, rBot, false) ||
                (tileMap->IsOneWayAt(cMid, rBot) && prevBottom <= tileMap->GetTileRect(cMid, rBot).y + 10.0f)) {
                Rectangle tRec = tileMap->GetTileRect(cMid, rBot);
                pos.y = tRec.y - height;
                if (std::abs(vel.y) > 140.0f) {
                    vel.y = -vel.y * 0.42f;
                    vel.x *= 0.72f;
                } else {
                    vel.y = 0.0f;
                    vel.x *= 0.80f;
                }
            }
        }

        // Магнітне підтягування монет зблизька та підбір гравцем
        if (pickupDelay <= 0.0f) {
            Vector2 pCenter = player.GetCenter();
            Vector2 myCenter = GetCenter();
            float dist = std::hypot(pCenter.x - myCenter.x, pCenter.y - myCenter.y);
            if (kind == InteractableKind::DroppedCoin && dist < 92.0f && dist > 1.0f) {
                vel.x += ((pCenter.x - myCenter.x) / dist) * 950.0f * dt;
                vel.y += ((pCenter.y - myCenter.y) / dist) * 950.0f * dt;
            }

            if (CheckCollisionRecs(GetBounds(), player.GetBounds())) {
                if (kind == InteractableKind::DroppedCoin) {
                    int gained = std::max(1, static_cast<int>(std::round(goldValue * player.stats.goldGainMult)));
                    ctx.gold += gained;
                    ctx.runGoldCollected += gained;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup);
                    if (ctx.particles) {
                        ctx.particles->SpawnSparks(myCenter, GOLD, 6);
                        ctx.particles->SpawnFloatingText(myCenter, "+" + std::to_string(gained) + "G", GOLD, 18, false);
                    }
                } else if (kind == InteractableKind::MeatDrop) {
                    int healAmt = std::max(10, static_cast<int>(std::round(player.maxHp * 0.20f * player.stats.potionEfficacyMult)));
                    player.HealHp(healAmt, ctx);
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.75f);
                } else if (kind == InteractableKind::ManaDrop) {
                    int manaAmt = std::max(10, static_cast<int>(std::round(player.stats.maxMp * 0.30f * player.stats.potionEfficacyMult)));
                    player.RestoreMp(manaAmt, ctx);
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.20f);
                }
                active = false;
            }
        }
        return;
    }

    // 2. Взаємодія зі звичайними скринями '$' та розблокованими Скринями Фей 'F'
    if (kind == InteractableKind::Chest || kind == InteractableKind::FairyChest) {
        if (!isOpened && !isLocked && CheckCollisionRecs(GetBounds(), player.GetBounds())) {
            bool interactPressed = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_UP);
            if (interactPressed) {
                isOpened = true;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen);
                if (ctx.particles) ctx.particles->SpawnSparks(GetCenter(), GOLD, 22);

                if (kind == InteractableKind::FairyChest) {
                    GrantRuneOrGold(ctx);
                } else if (isSilverChest) {
                    GrantBlueprintOrGold(ctx, BiomeType::CastleHamson);
                } else {
                    SpawnLootBurst(spawnedDrops, GetCenter(), 50 + (std::rand() % 4) * 10, false);
                }
            }
        }
    }
}

bool Interactable::OnHitByPlayer(
    GameContext& ctx,
    Player& player,
    std::vector<Interactable>& outDrops
) {
    if (!active) return false;

    if (kind == InteractableKind::Torch || kind == InteractableKind::BreakableProp) {
        active = false;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitEnemy, 1.3f, 0.7f);
        if (ctx.particles) {
            Color debrisCol = (kind == InteractableKind::Torch)
                              ? ORANGE
                              : ((subVariant == 1) ? LIGHTGRAY : Color{ 140, 92, 46, 255 });
            ctx.particles->SpawnDebris(GetCenter(), debrisCol, 12);
        }

        int goldRoll = (std::rand() % 100 < 65) ? ((1 + std::rand() % 2) * 10) : 0;
        SpawnLootBurst(outDrops, GetCenter(), goldRoll, true);

        if (player.isDownStriking) {
            player.TriggerPogoBounce(ctx);
        }
        return true;
    }

    // Відкриття звичайної скрині ударом меча також дозволено!
    if (kind == InteractableKind::Chest && !isOpened) {
        isOpened = true;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen);
        if (ctx.particles) ctx.particles->SpawnSparks(GetCenter(), GOLD, 20);
        if (isSilverChest) {
            GrantBlueprintOrGold(ctx, BiomeType::CastleHamson);
        } else {
            SpawnLootBurst(outDrops, GetCenter(), 50 + (std::rand() % 4) * 10, false);
        }
        if (player.isDownStriking) {
            player.TriggerPogoBounce(ctx);
        }
        return true;
    }

    return false;
}

void Interactable::GrantBlueprintOrGold(GameContext& ctx, BiomeType /*currentBiome*/) {
    // Шукаємо випадкове ще не відкрите креслення Коваля
    std::vector<std::pair<int, int>> lockedList;
    for (int s = 0; s < 5; ++s) {
        for (int t = 0; t < 5; ++t) {
            if (!ctx.unlockedBlueprints[static_cast<size_t>(s)][static_cast<size_t>(t)]) {
                lockedList.push_back({ s, t });
            }
        }
    }

    if (!lockedList.empty()) {
        // Надаємо перевагу нижчим тирам, але дозволяємо вищі на вищих NG+
        std::sort(lockedList.begin(), lockedList.end(), [](const auto& a, const auto& b) {
            return a.second < b.second;
        });
        size_t window = std::min<size_t>(lockedList.size(), 5);
        auto chosen = lockedList[static_cast<size_t>(std::rand()) % window];
        ctx.unlockedBlueprints[static_cast<size_t>(chosen.first)][static_cast<size_t>(chosen.second)] = true;

        const EquipmentDef& eq = Registry::GetEquipment(
            static_cast<EquipSlot>(chosen.first),
            static_cast<EquipTier>(chosen.second)
        );
        if (ctx.particles) {
            ctx.particles->SpawnFloatingText(
                { GetCenter().x, pos.y - 16.0f },
                "BLUEPRINT: " + eq.name + "!",
                SKYBLUE, 22, true
            );
        }
    } else {
        int bonusGold = static_cast<int>(std::round(150.0f * ctx.ComputePlayerStats().goldGainMult));
        ctx.gold += bonusGold;
        ctx.runGoldCollected += bonusGold;
        if (ctx.particles) {
            ctx.particles->SpawnFloatingText(GetCenter(), "+" + std::to_string(bonusGold) + "G", GOLD, 22, true);
        }
    }
}

void Interactable::GrantRuneOrGold(GameContext& ctx) {
    std::vector<std::pair<int, int>> lockedRunes;
    for (int s = 0; s < 5; ++s) {
        for (int r = 0; r < 8; ++r) {
            if (!ctx.unlockedRunes[static_cast<size_t>(s)][static_cast<size_t>(r)]) {
                lockedRunes.push_back({ s, r });
            }
        }
    }

    if (!lockedRunes.empty()) {
        auto chosen = lockedRunes[static_cast<size_t>(std::rand()) % lockedRunes.size()];
        ctx.unlockedRunes[static_cast<size_t>(chosen.first)][static_cast<size_t>(chosen.second)] = true;

        const RuneDef& rDef = Registry::GetRune(static_cast<RuneType>(chosen.second));
        std::string slotName = Registry::GetEquipSlotName(static_cast<EquipSlot>(chosen.first));
        if (ctx.particles) {
            ctx.particles->SpawnFloatingText(
                { GetCenter().x, pos.y - 18.0f },
                "RUNE UNLOCKED: " + rDef.name + " (" + slotName + ")!",
                MAGENTA, 22, true
            );
        }
    } else {
        int bonusGold = static_cast<int>(std::round(220.0f * ctx.ComputePlayerStats().goldGainMult));
        ctx.gold += bonusGold;
        ctx.runGoldCollected += bonusGold;
        if (ctx.particles) {
            ctx.particles->SpawnFloatingText(GetCenter(), "+" + std::to_string(bonusGold) + "G", GOLD, 22, true);
        }
    }
}

void Interactable::Draw(const GameContext& /*ctx*/) const {
    if (!active) return;

    Vector2 c = GetCenter();
    switch (kind) {
        case InteractableKind::Torch: {
            // Готичний кований настінний смолоскип
            // Кронштейн та чаша
            DrawRectangle(static_cast<int>(c.x - 3.0f), static_cast<int>(c.y - 2.0f), 6, 22, Color{ 48, 50, 58, 255 });
            DrawTriangle({ c.x - 8.0f, c.y + 14.0f }, { c.x + 8.0f, c.y + 14.0f }, { c.x, c.y + 2.0f }, Color{ 48, 50, 58, 255 });
            DrawRectangle(static_cast<int>(c.x - 8.0f), static_cast<int>(c.y - 6.0f), 16, 6, Color{ 70, 75, 88, 255 });

            // Тепле радіальне сяйво на стіну
            float flicker = std::sin(animTimer * 16.0f) * 2.5f;
            DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(c.y - 12.0f), 26.0f + flicker, Fade(ORANGE, 0.28f), Fade(BLACK, 0.0f));

            // Багатошарове живе полум'я
            float fH = 14.0f + std::sin(animTimer * 20.0f) * 3.0f;
            DrawTriangle({ c.x - 7.0f, c.y - 5.0f }, { c.x + 7.0f, c.y - 5.0f }, { c.x, c.y - 5.0f - fH }, RED);
            DrawTriangle({ c.x - 5.0f, c.y - 5.0f }, { c.x + 5.0f, c.y - 5.0f }, { c.x, c.y - 5.0f - fH * 0.75f }, ORANGE);
            DrawTriangle({ c.x - 2.5f, c.y - 5.0f }, { c.x + 2.5f, c.y - 5.0f }, { c.x, c.y - 5.0f - fH * 0.45f }, YELLOW);
            DrawCircleV({ c.x, c.y - 5.0f - fH * 0.25f }, 2.0f, WHITE);
            break;
        }

        case InteractableKind::BreakableProp: {
            if (subVariant == 0) {
                // Дерев'яна дубова бочка з кованими обручами
                Rectangle b = GetBounds();
                DrawRectangleRounded(b, 0.35f, 4, Color{ 132, 82, 42, 255 });
                DrawRectangleRoundedLines(b, 0.35f, 4, 1.5f, Color{ 68, 38, 18, 255 });
                // Вертикальні дошки бочки
                for (int st = 1; st <= 2; ++st) {
                    float stx = b.x + b.width * (st * 0.33f);
                    DrawLine(static_cast<int>(stx), static_cast<int>(b.y + 2.0f), static_cast<int>(stx), static_cast<int>(b.y + b.height - 2.0f), Color{ 88, 52, 25, 255 });
                }
                // Залізні обручі з заклепками
                DrawRectangle(static_cast<int>(b.x), static_cast<int>(b.y + 7.0f), static_cast<int>(b.width), 4, Color{ 58, 62, 72, 255 });
                DrawRectangle(static_cast<int>(b.x), static_cast<int>(b.y + b.height - 11.0f), static_cast<int>(b.width), 4, Color{ 58, 62, 72, 255 });
                DrawCircle(static_cast<int>(b.x + 6.0f), static_cast<int>(b.y + 9.0f), 1.5f, LIGHTGRAY);
                DrawCircle(static_cast<int>(b.x + b.width - 6.0f), static_cast<int>(b.y + 9.0f), 1.5f, LIGHTGRAY);
            } else if (subVariant == 1) {
                // Тренувальний манекен у шоломі зі щитом
                DrawLineEx({ c.x, pos.y + 8.0f }, { c.x, GetBottom() }, 6.0f, Color{ 95, 65, 38, 255 });
                DrawLineEx({ c.x - 14.0f, pos.y + 20.0f }, { c.x + 14.0f, pos.y + 20.0f }, 5.0f, Color{ 95, 65, 38, 255 });
                // Мішковина торсу
                DrawRectangleRounded({ c.x - 12.0f, pos.y + 14.0f, 24.0f, 22.0f }, 0.3f, 4, Color{ 175, 142, 98, 255 });
                DrawRectangleRoundedLines({ c.x - 12.0f, pos.y + 14.0f, 24.0f, 22.0f }, 0.3f, 4, 1.5f, Color{ 115, 88, 52, 255 });
                // Солома, що стирчить
                DrawLine(static_cast<int>(c.x - 10.0f), static_cast<int>(pos.y + 36.0f), static_cast<int>(c.x - 14.0f), static_cast<int>(pos.y + 42.0f), YELLOW);
                DrawLine(static_cast<int>(c.x + 8.0f), static_cast<int>(pos.y + 36.0f), static_cast<int>(c.x + 12.0f), static_cast<int>(pos.y + 41.0f), YELLOW);
                // Пом'ятий тренувальний шолом
                DrawCircle(static_cast<int>(c.x), static_cast<int>(pos.y + 8.0f), 8.5f, Color{ 120, 125, 138, 255 });
                DrawRectangle(static_cast<int>(c.x - 5.0f), static_cast<int>(pos.y + 8.0f), 10, 2, BLACK);
            } else {
                // Готичний столик із золотим кубком
                Rectangle b = GetBounds();
                DrawRectangle(static_cast<int>(b.x), static_cast<int>(b.y + 8.0f), static_cast<int>(b.width), 8, Color{ 110, 68, 38, 255 });
                DrawRectangle(static_cast<int>(b.x + 4.0f), static_cast<int>(b.y + 16.0f), 5, static_cast<int>(b.height - 16.0f), Color{ 80, 48, 25, 255 });
                DrawRectangle(static_cast<int>(b.x + b.width - 9.0f), static_cast<int>(b.y + 16.0f), 5, static_cast<int>(b.height - 16.0f), Color{ 80, 48, 25, 255 });
                // Золотий кубок
                DrawTriangle({ c.x + 6.0f, b.y + 7.0f }, { c.x, b.y - 5.0f }, { c.x - 6.0f, b.y + 7.0f }, GOLD);
                DrawCircle(static_cast<int>(c.x), static_cast<int>(b.y - 5.0f), 4.5f, GOLD);
                DrawCircle(static_cast<int>(c.x), static_cast<int>(b.y - 6.0f), 3.0f, MAROON); // вино всередині
            }
            break;
        }

        case InteractableKind::Chest: {
            Color bodyCol = isSilverChest ? Color{ 180, 195, 220, 255 } : Color{ 145, 85, 38, 255 };
            Color woodDark = isSilverChest ? Color{ 130, 145, 175, 255 } : Color{ 95, 52, 22, 255 };
            Color trimCol = isSilverChest ? SKYBLUE : GOLD;
            Rectangle b = GetBounds();

            if (!isOpened) {
                // Закрита скриня: опукла кришка з кованими смугами
                DrawRectangleRounded(b, 0.22f, 4, bodyCol);
                DrawRectangleRoundedLines(b, 0.22f, 4, 2.5f, woodDark);
                // Горизонтальний шов кришки
                float lidSeamY = b.y + b.height * 0.38f;
                DrawLineEx({ b.x, lidSeamY }, { b.x + b.width, lidSeamY }, 2.0f, Color{ 45, 25, 12, 255 });

                // Металеві оковки по краях та в центрі
                DrawRectangle(static_cast<int>(b.x + 3.0f), static_cast<int>(b.y + 2.0f), 5, static_cast<int>(b.height - 4.0f), trimCol);
                DrawRectangle(static_cast<int>(b.x + b.width - 8.0f), static_cast<int>(b.y + 2.0f), 5, static_cast<int>(b.height - 4.0f), trimCol);
                DrawRectangle(static_cast<int>(c.x - 3.5f), static_cast<int>(b.y + 2.0f), 7, static_cast<int>(b.height - 4.0f), trimCol);

                // Замкова щілина
                DrawCircle(static_cast<int>(c.x), static_cast<int>(lidSeamY + 3.0f), 4.0f, trimCol);
                DrawCircle(static_cast<int>(c.x), static_cast<int>(lidSeamY + 3.0f), 2.0f, BLACK);
            } else {
                // Відкрита скриня: видно золото всередині
                DrawRectangle(static_cast<int>(b.x), static_cast<int>(b.y + 8.0f), static_cast<int>(b.width), static_cast<int>(b.height - 8.0f), bodyCol);
                DrawRectangleLinesEx({ b.x, b.y + 8.0f, b.width, b.height - 8.0f }, 2.0f, trimCol);
                // Відкинута кришка скрині
                DrawRectangle(static_cast<int>(b.x + 2.0f), static_cast<int>(b.y), static_cast<int>(b.width - 4.0f), 8, woodDark);
                // Сяючі золоті монети всередині
                DrawCircle(static_cast<int>(c.x - 6.0f), static_cast<int>(b.y + 13.0f), 5.0f, GOLD);
                DrawCircle(static_cast<int>(c.x + 6.0f), static_cast<int>(b.y + 13.0f), 5.0f, GOLD);
                DrawCircle(static_cast<int>(c.x), static_cast<int>(b.y + 11.0f), 5.5f, YELLOW);
            }
            break;
        }

        case InteractableKind::FairyChest: {
            Color stoneCol = isLocked ? Color{ 58, 48, 68, 255 } : Color{ 36, 145, 115, 255 };
            Color trimCol  = isLocked ? RED : MAGENTA;
            Rectangle b = GetBounds();

            DrawRectangleRounded(b, 0.25f, 4, stoneCol);
            DrawRectangleRoundedLines(b, 0.25f, 4, 3.0f, trimCol);

            // Рунічні гравіювання на кам'яному тілі
            DrawLineEx({ b.x + 6.0f, b.y + b.height * 0.5f }, { b.x + b.width - 6.0f, b.y + b.height * 0.5f }, 2.0f, Fade(trimCol, 0.6f));

            // Ширяючий магічний кристал / рунна печатка
            float crystalWave = std::sin(animTimer * 6.0f) * 3.5f;
            Vector2 cryPos = { c.x, c.y + (isOpened ? 4.0f : crystalWave) };
            if (!isOpened) {
                DrawCircleV(cryPos, 9.0f, Fade(trimCol, 0.35f));
                DrawCircleV(cryPos, 6.0f, isLocked ? RED : SKYBLUE);
                DrawCircleV(cryPos, 2.5f, WHITE);
            } else {
                DrawCircleV(c, 5.0f, DARKGRAY);
            }
            break;
        }

        case InteractableKind::Teleporter: {
            // Магічний портальний круг
            DrawRectangle(static_cast<int>(pos.x), static_cast<int>(GetBottom() - 10.0f), static_cast<int>(width), 10, Color{ 75, 82, 98, 255 });
            DrawRectangleLines(static_cast<int>(pos.x), static_cast<int>(GetBottom() - 10.0f), static_cast<int>(width), 10, GOLD);

            // Пульсуюче коло порталу
            float pulse = 0.75f + 0.25f * std::sin(animTimer * 5.0f);
            DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(c.y - 2.0f), 24.0f * pulse, Fade(SKYBLUE, 0.45f), Fade(BLACK, 0.0f));
            DrawCircleV({ c.x, c.y - 2.0f }, 14.0f * pulse, Fade(SKYBLUE, 0.75f));
            DrawCircleV({ c.x, c.y - 2.0f }, 7.0f * pulse, WHITE);
            // Магічні іскри, що піднімаються
            for (int k = -1; k <= 1; ++k) {
                float sy = std::fmod(animTimer * 28.0f + k * 14.0f, 32.0f);
                DrawCircle(static_cast<int>(c.x + k * 10.0f), static_cast<int>(GetBottom() - 10.0f - sy), 2.0f, SKYBLUE);
            }
            break;
        }

        case InteractableKind::BossDoor: {
            Rectangle b = GetBounds();
            DrawRectangleRec(b, Color{ 32, 16, 24, 255 });
            DrawRectangleLinesEx(b, 4.0f, RED);
            // Залізні заклепки на дверях боса
            for (int by = static_cast<int>(b.y + 16); by < static_cast<int>(b.y + b.height); by += 24) {
                DrawCircle(static_cast<int>(b.x + 8.0f), by, 3.0f, GOLD);
                DrawCircle(static_cast<int>(b.x + b.width - 8.0f), by, 3.0f, GOLD);
            }
            // Демонічний череп на воротах
            float glow = 0.6f + 0.4f * std::sin(animTimer * 4.5f);
            DrawCircleV({ c.x, c.y - 12.0f }, 22.0f, Color{ 210, 205, 195, 255 });
            DrawCircleV({ c.x - 7.5f, c.y - 14.0f }, 5.0f * glow, RED);
            DrawCircleV({ c.x + 7.5f, c.y - 14.0f }, 5.0f * glow, RED);
            DrawCircleV({ c.x - 7.5f, c.y - 14.0f }, 2.0f, WHITE);
            DrawCircleV({ c.x + 7.5f, c.y - 14.0f }, 2.0f, WHITE);
            break;
        }

        case InteractableKind::DroppedCoin: {
            if (subVariant == 0) {
                // Золота монета з обертанням у 3D та бликом
                float spinW = std::max(2.5f, 7.5f * std::abs(std::cos(animTimer * 8.5f)));
                DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), spinW, 7.5f, GOLD);
                DrawEllipseLines(static_cast<int>(c.x), static_cast<int>(c.y), spinW, 7.5f, ORANGE);
                DrawCircleV({ c.x, c.y }, 2.5f, YELLOW);
                DrawCircleV({ c.x - 1.5f, c.y - 1.5f }, 1.2f, WHITE);
            } else {
                // Шкіряний мішечок золота (50g)
                DrawCircleV({ c.x, c.y + 3.0f }, 10.5f, Color{ 175, 125, 65, 255 });
                DrawTriangle({ c.x + 7.0f, c.y - 8.0f }, { c.x - 7.0f, c.y - 8.0f }, { c.x, c.y - 1.0f }, Color{ 175, 125, 65, 255 });
                // Мотузка з золотою зав'язкою
                DrawLineEx({ c.x - 6.0f, c.y - 4.0f }, { c.x + 6.0f, c.y - 4.0f }, 2.5f, GOLD);
                DrawCircleV({ c.x, c.y + 4.0f }, 4.0f, GOLD);
                DrawCircleV({ c.x, c.y + 4.0f }, 2.0f, YELLOW);
            }
            break;
        }

        case InteractableKind::MeatDrop: {
            // Апетитний смажений окіст на кістці
            DrawEllipse(static_cast<int>(c.x - 3.0f), static_cast<int>(c.y), 11.5f, 8.0f, Color{ 195, 95, 38, 255 });
            DrawEllipseLines(static_cast<int>(c.x - 3.0f), static_cast<int>(c.y), 11.5f, 8.0f, Color{ 135, 55, 18, 255 });
            // Смужки запікання на грилі
            DrawLine(static_cast<int>(c.x - 7.0f), static_cast<int>(c.y - 4.0f), static_cast<int>(c.x - 2.0f), static_cast<int>(c.y + 4.0f), Color{ 110, 42, 12, 255 });
            DrawLine(static_cast<int>(c.x - 2.0f), static_cast<int>(c.y - 4.0f), static_cast<int>(c.x + 3.0f), static_cast<int>(c.y + 4.0f), Color{ 110, 42, 12, 255 });
            // Біла кісточка
            DrawRectangle(static_cast<int>(c.x + 6.0f), static_cast<int>(c.y - 2.5f), 9, 5, RAYWHITE);
            DrawCircle(static_cast<int>(c.x + 15.0f), static_cast<int>(c.y - 3.0f), 3.2f, RAYWHITE);
            DrawCircle(static_cast<int>(c.x + 15.0f), static_cast<int>(c.y + 3.0f), 3.2f, RAYWHITE);
            break;
        }

        case InteractableKind::ManaDrop: {
            // Скляна колба з сяючим блакитним зіллям мани
            DrawCircleV({ c.x, c.y + 3.0f }, 9.0f, Fade(Color{ 45, 140, 245, 255 }, 0.85f));
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y + 3.0f), 9.0f, SKYBLUE);
            // Бульбашки та блік скла
            DrawCircleV({ c.x - 3.0f, c.y + 1.0f }, 3.5f, WHITE);
            DrawCircleV({ c.x + 2.0f, c.y + 4.0f }, 2.0f, WHITE);
            // Горлечко з корком
            DrawRectangle(static_cast<int>(c.x - 3.5f), static_cast<int>(c.y - 8.5f), 7, 5, LIGHTGRAY);
            DrawRectangle(static_cast<int>(c.x - 2.5f), static_cast<int>(c.y - 12.0f), 5, 4, Color{ 135, 95, 55, 255 });
            break;
        }
    }
}