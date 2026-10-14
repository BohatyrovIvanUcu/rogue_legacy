// === FILE: src/states/OutskirtsState.cpp ===
#include "states/OutskirtsState.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "core/SaveManager.hpp"
#include "data/Registry.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/ProceduralRenderer.hpp"
#include "gfx/UiText.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <string>

namespace {
    constexpr float BLACKSMITH_X  = 310.0f;
    constexpr float ENCHANTRESS_X = 590.0f;
    constexpr float ARCHITECT_X   = 860.0f;
    constexpr float CHARON_GATE_X = 1110.0f;
    constexpr float GROUND_Y      = 580.0f;

    struct StarInfo {
        float x;
        float y;
        float size;
        float speed;
        float phase;
    };

    const StarInfo SKY_STARS[] = {
        { 55.0f, 42.0f, 1.8f, 3.2f, 0.4f },
        { 110.0f, 85.0f, 1.2f, 2.5f, 1.1f },
        { 175.0f, 32.0f, 2.0f, 4.0f, 2.3f },
        { 235.0f, 105.0f, 1.4f, 2.8f, 0.7f },
        { 305.0f, 60.0f, 1.6f, 3.5f, 1.8f },
        { 375.0f, 38.0f, 2.2f, 2.1f, 3.1f },
        { 435.0f, 90.0f, 1.3f, 4.2f, 0.9f },
        { 505.0f, 48.0f, 1.8f, 3.0f, 2.5f },
        { 570.0f, 115.0f, 1.1f, 2.7f, 1.4f },
        { 625.0f, 65.0f, 2.0f, 3.8f, 0.2f },
        { 705.0f, 42.0f, 1.5f, 2.3f, 2.9f },
        { 765.0f, 100.0f, 1.9f, 3.6f, 1.6f },
        { 830.0f, 52.0f, 1.3f, 2.9f, 0.5f },
        { 885.0f, 82.0f, 2.1f, 4.1f, 2.1f },
        { 1075.0f, 55.0f, 1.7f, 3.3f, 1.3f },
        { 1135.0f, 110.0f, 1.4f, 2.4f, 2.7f },
        { 1205.0f, 38.0f, 2.0f, 3.7f, 0.8f },
        { 1250.0f, 90.0f, 1.2f, 2.2f, 1.9f },
        { 145.0f, 155.0f, 1.5f, 3.1f, 3.4f },
        { 335.0f, 145.0f, 1.3f, 2.6f, 0.3f },
        { 515.0f, 170.0f, 1.8f, 3.9f, 2.2f },
        { 695.0f, 155.0f, 1.2f, 2.8f, 1.7f },
        { 855.0f, 175.0f, 1.6f, 3.4f, 0.6f },
        { 1105.0f, 165.0f, 1.4f, 2.5f, 2.8f },
    };

    void DrawPineTree(float x, float baseY, float height, float width, Color col) {
        Color trunkCol = Color{ 26, 18, 16, 255 };
        float trunkW = width * 0.16f;
        float trunkH = height * 0.22f;
        DrawRectangleRec({ x - trunkW * 0.5f, baseY - trunkH, trunkW, trunkH }, trunkCol);

        int tiers = 4;
        float tierH = (height - trunkH * 0.4f) / tiers;
        for (int i = 0; i < tiers; ++i) {
            float frac = static_cast<float>(i) / (tiers - 1);
            float tw = width * (1.0f - frac * 0.55f);
            float by = baseY - trunkH * 0.4f - i * tierH * 0.72f;
            float ty = by - tierH * 1.35f;
            DrawTriangle(
                { x, ty },
                { x - tw * 0.5f, by },
                { x + tw * 0.5f, by },
                col
            );
        }
    }

    void DrawStreetTorch(float x, float groundY, float time) {
        Color ironCol = Color{ 40, 42, 50, 255 };
        DrawRectangle(static_cast<int>(x - 3.0f), static_cast<int>(groundY - 84.0f), 6, 84, ironCol);
        DrawTriangle(
            { x - 10.0f, groundY },
            { x, groundY - 18.0f },
            { x + 10.0f, groundY },
            ironCol
        );

        DrawRectangle(static_cast<int>(x - 9.0f), static_cast<int>(groundY - 90.0f), 18, 7, Color{ 60, 64, 76, 255 });
        DrawTriangle(
            { x - 9.0f, groundY - 90.0f },
            { x, groundY - 80.0f },
            { x + 9.0f, groundY - 90.0f },
            ironCol
        );

        float flicker = 0.82f + 0.18f * std::sin(time * 11.0f + x);
        DrawCircleGradient(static_cast<int>(x), static_cast<int>(groundY - 94.0f), 68.0f * flicker, Fade(ORANGE, 0.28f * flicker), Fade(ORANGE, 0.0f));

        float flameH = 20.0f + 4.0f * std::sin(time * 15.0f + x * 2.0f);
        DrawTriangle(
            { x - 7.0f, groundY - 90.0f },
            { x, groundY - 90.0f - flameH },
            { x + 7.0f, groundY - 90.0f },
            RED
        );
        DrawTriangle(
            { x - 5.0f, groundY - 90.0f },
            { x, groundY - 90.0f - flameH * 0.75f },
            { x + 5.0f, groundY - 90.0f },
            ORANGE
        );
        DrawTriangle(
            { x - 2.5f, groundY - 90.0f },
            { x, groundY - 90.0f - flameH * 0.5f },
            { x + 2.5f, groundY - 90.0f },
            YELLOW
        );
    }
}

void OutskirtsState::OnEnter(GameContext& /*ctx*/) {
    heroX_ = 150.0f;
    heroY_ = GROUND_Y;
    heroVelX_ = 0.0f;
    heroVelY_ = 0.0f;
    onGround_ = true;
    facingDir_ = 1.0f;
    animTimer_ = 0.0f;
    attackTimer_ = 0.0f;
    activeMenu_ = ModalMenu::None;
    menuSlotTab_ = 0;
    menuItemRow_ = 0;
    charonSequenceActive_ = false;
    charonTimer_ = 0.0f;
    gatePortcullisY_ = 0.0f;
    flyingCoins_.clear();
}

void OutskirtsState::OnExit(GameContext& ctx) {
    SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
}

void OutskirtsState::Update(float dt, GameContext& ctx) {
    animTimer_ += dt;
    if (ctx.particles) {
        ctx.particles->Update(dt);
    }

    // 1. Анімація сплати мита Харону та входу у Замок
    if (charonSequenceActive_) {
        charonTimer_ += dt;
        gatePortcullisY_ = std::min(165.0f, charonTimer_ * 150.0f);

        Vector2 charonHand = { CHARON_GATE_X - 45.0f, GROUND_Y - 46.0f };
        for (auto& fc : flyingCoins_) {
            if (!fc.active) continue;
            Vector2 toHand = { charonHand.x - fc.pos.x, charonHand.y - fc.pos.y };
            float d = std::hypot(toHand.x, toHand.y);
            if (d < 18.0f) {
                fc.active = false;
                if (ctx.audio)     ctx.audio->PlaySfx(SfxId::CoinPickup, 1.1f, 0.5f);
                if (ctx.particles) ctx.particles->SpawnSparks(charonHand, GOLD, 4);
            } else {
                fc.vel.x += (toHand.x / d) * 1800.0f * dt;
                fc.vel.y += (toHand.y / d) * 1800.0f * dt;
                fc.pos.x += fc.vel.x * dt;
                fc.pos.y += fc.vel.y * dt;
            }
        }

        if (charonTimer_ >= 1.35f) {
            ctx.RequestStateChange(GameStateId::CastlePlay);
        }
        return;
    }

    // 2. Якщо відкрито модальне меню Коваля / Чарівниці / Архітектора
    if (activeMenu_ != ModalMenu::None) {
        UpdateModalMenu(ctx);
        return;
    }

    // Повернення у Родовий Маєток (M або вихід вліво)
    if (IsKeyPressed(KEY_M) || heroX_ < 60.0f) {
        ctx.RequestStateChange(GameStateId::Manor);
        return;
    }

    // Спавн фонових частинок диму з кузні та іскор від чарівниці
    if (ctx.particles) {
        if (ctx.GetManorLevel(ManorNodeId::Unlock_Blacksmith) > 0 && (std::rand() % 16 == 0)) {
            ctx.particles->SpawnSmoke({ BLACKSMITH_X + 48.0f, GROUND_Y - 175.0f }, Color{ 70, 75, 90, 110 }, 1);
        }
        if (ctx.GetManorLevel(ManorNodeId::Unlock_Enchantress) > 0 && (std::rand() % 14 == 0)) {
            ctx.particles->SpawnHealSparkles({ ENCHANTRESS_X + (-16.0f + static_cast<float>(std::rand() % 32)), GROUND_Y - 32.0f }, MAGENTA, 1);
        }
    }

    // 3. Пересування героя по бруківці
    float moveDir = 0.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  moveDir -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) moveDir += 1.0f;

    heroVelX_ = moveDir * 310.0f;
    if (moveDir != 0.0f) {
        facingDir_ = (moveDir > 0.0f) ? 1.0f : -1.0f;
    }
    heroX_ = std::clamp(heroX_ + heroVelX_ * dt, 50.0f, CHARON_GATE_X + 10.0f);

    // 4. Стрибок та вертикальна фізика
    bool nearBlacksmith  = (std::abs(heroX_ - BLACKSMITH_X) < 75.0f && ctx.GetManorLevel(ManorNodeId::Unlock_Blacksmith) > 0);
    bool nearEnchantress = (std::abs(heroX_ - ENCHANTRESS_X) < 75.0f && ctx.GetManorLevel(ManorNodeId::Unlock_Enchantress) > 0);
    bool nearArchitect   = (std::abs(heroX_ - ARCHITECT_X) < 75.0f && ctx.GetManorLevel(ManorNodeId::Unlock_Architect) > 0);
    bool nearCharonGate  = (std::abs(heroX_ - CHARON_GATE_X) < 85.0f);
    bool nearInteractive = (nearBlacksmith || nearEnchantress || nearArchitect || nearCharonGate);

    bool jumpPressed = IsKeyPressed(KEY_SPACE);
    if (!jumpPressed && (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) && !nearInteractive) {
        jumpPressed = true;
    }

    if (jumpPressed && onGround_) {
        heroVelY_ = Config::PLAYER_JUMP_VELOCITY;
        onGround_ = false;
        if (ctx.audio)     ctx.audio->PlaySfx(SfxId::Jump);
        if (ctx.particles) ctx.particles->SpawnDust({ heroX_, GROUND_Y }, 6);
    }

    // Зрізання стрибка при передчасному відпусканні клавіші
    if (!onGround_ && heroVelY_ < 0.0f) {
        if (IsKeyReleased(KEY_SPACE) || IsKeyReleased(KEY_W) || IsKeyReleased(KEY_UP)) {
            heroVelY_ *= Config::PLAYER_JUMP_CUT_MULTIPLIER;
        }
    }

    // Гравітація
    if (!onGround_) {
        heroVelY_ = std::min(heroVelY_ + Config::GRAVITY * dt, Config::MAX_FALL_SPEED);
        heroY_ += heroVelY_ * dt;
        if (heroY_ >= GROUND_Y) {
            heroY_ = GROUND_Y;
            heroVelY_ = 0.0f;
            onGround_ = true;
            if (ctx.particles) ctx.particles->SpawnDust({ heroX_, GROUND_Y }, 5);
            if (ctx.audio && attackTimer_ > 0.0f && (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))) {
                ctx.audio->PlaySfx(SfxId::HitShield);
            }
        }
    }

    // 5. Атака мечем (J / Z / ЛКМ)
    if (attackTimer_ > 0.0f) {
        attackTimer_ -= dt;
        if (attackTimer_ <= 0.0f) {
            attackTimer_ = 0.0f;
        }
    }

    bool attackPressed = IsKeyPressed(KEY_J) || IsKeyPressed(KEY_Z) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (attackPressed && attackTimer_ <= 0.0f) {
        attackTimer_ = Config::SWORD_ATTACK_DURATION;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::SwordSlash);
        if (ctx.particles) {
            Vector2 slashPos = { heroX_ + facingDir_ * 32.0f, heroY_ - 26.0f };
            ctx.particles->SpawnSparks(slashPos, Color{ 220, 235, 255, 255 }, 6);
        }
    }

    // 6. Взаємодія з NPC (W / E / UP / ENTER)
    bool interact = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_ENTER);
    if (interact) {
        if (nearBlacksmith) {
            activeMenu_ = ModalMenu::Blacksmith;
            menuSlotTab_ = 0;
            menuItemRow_ = 0;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild, 1.1f);
            return;
        }
        if (nearEnchantress) {
            activeMenu_ = ModalMenu::Enchantress;
            menuSlotTab_ = 0;
            menuItemRow_ = 0;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 1.1f);
            return;
        }
        if (nearArchitect) {
            activeMenu_ = ModalMenu::Architect;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitShield, 1.0f);
            return;
        }
    }

    // 7. Вхід у ворота Замку з Хароном
    if (heroX_ >= CHARON_GATE_X - 24.0f || (interact && nearCharonGate)) {
        if (!charonSequenceActive_) {
            activeMenu_ = ModalMenu::CharonToll;
            heroVelX_ = 0.0f;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::BossRoar, 0.7f, 0.6f);
            return;
        }
    }
}

void OutskirtsState::UpdateModalMenu(GameContext& ctx) {
    if (activeMenu_ == ModalMenu::CharonToll) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
            activeMenu_ = ModalMenu::None;
            heroX_ = std::min(heroX_, CHARON_GATE_X - 70.0f);
            return;
        }

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E)) {
            activeMenu_ = ModalMenu::None;
            charonSequenceActive_ = true;
            charonTimer_ = 0.0f;
            heroY_ = GROUND_Y;
            heroVelY_ = 0.0f;
            onGround_ = true;
            attackTimer_ = 0.0f;

            CalculatedStats st = ctx.ComputePlayerStats();
            int keptGold = static_cast<int>(std::round(ctx.gold * st.hagglingKeptRatio));
            ctx.charonTollPaid = std::max(0, ctx.gold - keptGold);
            ctx.gold = keptGold;

            int coinVisualCount = std::clamp(ctx.charonTollPaid / 15 + 5, 6, 24);
            for (int i = 0; i < coinVisualCount; ++i) {
                TollCoinVisual tc;
                tc.pos = { heroX_, GROUND_Y - 35.0f };
                tc.vel = {
                    -140.0f + static_cast<float>(std::rand() % 220),
                    -260.0f - static_cast<float>(std::rand() % 180)
                };
                flyingCoins_.push_back(tc);
            }

            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.8f);
            if (ctx.particles && ctx.charonTollPaid > 0) {
                ctx.particles->SpawnFloatingText(
                    { CHARON_GATE_X - 45.0f, GROUND_Y - 95.0f },
                    "CHARON TOLL: -" + std::to_string(ctx.charonTollPaid) + "G",
                    GOLD, 20, true
                );
            }
            return;
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_Q)) {
        activeMenu_ = ModalMenu::None;
        SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
        return;
    }

    if (activeMenu_ == ModalMenu::Blacksmith) {
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  menuSlotTab_ = (menuSlotTab_ + 4) % 5;
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) menuSlotTab_ = (menuSlotTab_ + 1) % 5;
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    menuItemRow_ = (menuItemRow_ + 4) % 5;
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  menuItemRow_ = (menuItemRow_ + 1) % 5;

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            size_t s = static_cast<size_t>(menuSlotTab_);
            size_t t = static_cast<size_t>(menuItemRow_);
            const EquipmentDef& eq = Registry::GetEquipment(static_cast<EquipSlot>(s), static_cast<EquipTier>(t));

            bool unlocked = ctx.unlockedBlueprints[s][t];
            bool owned    = ctx.ownedEquipment[s][t];

            if (!unlocked) {
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
                return;
            }

            if (!owned) {
                if (ctx.gold >= eq.cost) {
                    ctx.gold -= eq.cost;
                    ctx.ownedEquipment[s][t] = true;
                    owned = true;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild);
                } else {
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
                    return;
                }
            }

            if (owned) {
                // Якщо вже одягнено цей тир — знімаємо його
                if (ctx.equippedTier[s] == static_cast<int>(t)) {
                    ctx.equippedTier[s] = -1;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f);
                } else {
                    // Перевіряємо ліміт ваги спорядження перед одяганням
                    int oldTier = ctx.equippedTier[s];
                    ctx.equippedTier[s] = static_cast<int>(t);
                    if (ctx.GetCurrentEquipWeight() <= ctx.GetMaxEquipWeight()) {
                        if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitShield, 1.2f);
                    } else {
                        // Перевищено MaxWeight — відкочуємо
                        ctx.equippedTier[s] = oldTier;
                        if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
                    }
                }
            }
        }
    } else if (activeMenu_ == ModalMenu::Enchantress) {
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  menuSlotTab_ = (menuSlotTab_ + 4) % 5;
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) menuSlotTab_ = (menuSlotTab_ + 1) % 5;
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    menuItemRow_ = (menuItemRow_ + 7) % 8;
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  menuItemRow_ = (menuItemRow_ + 1) % 8;

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            size_t s = static_cast<size_t>(menuSlotTab_);
            size_t r = static_cast<size_t>(menuItemRow_);
            RuneType rType = static_cast<RuneType>(r);
            const RuneDef& rDef = Registry::GetRune(rType);

            bool unlocked = ctx.unlockedRunes[s][r];
            bool owned    = ctx.ownedRunes[s][r];

            if (!unlocked) {
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
                return;
            }
            if (!owned) {
                if (ctx.gold >= rDef.cost) {
                    ctx.gold -= rDef.cost;
                    ctx.ownedRunes[s][r] = true;
                    owned = true;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast);
                } else {
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
                    return;
                }
            }
            if (owned) {
                if (ctx.equippedRunes[s] == rType) {
                    ctx.equippedRunes[s] = RuneType::None;
                } else {
                    ctx.equippedRunes[s] = rType;
                }
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.15f);
            }
        }
    } else if (activeMenu_ == ModalMenu::Architect) {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            if (ctx.hasSavedCastle) {
                ctx.isCastleLocked = !ctx.isCastleLocked;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild);
            } else {
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
            }
        }
    }
}

void OutskirtsState::Draw(GameContext& ctx) {
    // 1. Яскраве денне небо (Daytime azure gradient sky)
    DrawRectangleGradientV(0, 0, Config::VIRTUAL_WIDTH, static_cast<int>(GROUND_Y + 40.0f), Color{ 62, 138, 235, 255 }, Color{ 185, 222, 252, 255 });

    // Сяюче тепле сонце з ореолом та сонячними променями
    Vector2 sunPos = { 980.0f, 115.0f };
    DrawCircleGradient(static_cast<int>(sunPos.x), static_cast<int>(sunPos.y), 125.0f, Fade(Color{ 255, 240, 160, 255 }, 0.35f), BLANK);
    DrawCircleGradient(static_cast<int>(sunPos.x), static_cast<int>(sunPos.y), 70.0f, Fade(Color{ 255, 225, 110, 255 }, 0.55f), BLANK);
    DrawCircleV(sunPos, 36.0f, Color{ 255, 250, 210, 255 });
    DrawCircleV(sunPos, 24.0f, WHITE);

    for (int r = 0; r < 8; ++r) {
        float rayAng = animTimer_ * 0.15f + r * (3.14159f / 4.0f);
        Vector2 rEnd = { sunPos.x + std::cos(rayAng) * 90.0f, sunPos.y + std::sin(rayAng) * 90.0f };
        DrawLineEx(sunPos, rEnd, 2.5f, Fade(Color{ 255, 245, 180, 255 }, 0.25f));
    }

    // Денні об'ємні кучові хмари (Cumulus clouds)
    for (int i = 0; i < 5; ++i) {
        float cloudSpeed = 10.0f + i * 6.0f;
        float cx = std::fmod(animTimer_ * cloudSpeed + i * 310.0f, 1500.0f) - 100.0f;
        float cy = 65.0f + i * 32.0f;
        float cs = 0.85f + (i % 3) * 0.25f;

        Color cloudShadow = Color{ 220, 232, 248, 220 };
        Color cloudTop    = Color{ 255, 255, 255, 245 };

        DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 8.0f * cs), 80.0f * cs, 22.0f * cs, cloudShadow);
        DrawCircle(static_cast<int>(cx - 36.0f * cs), static_cast<int>(cy + 2.0f), 26.0f * cs, cloudTop);
        DrawCircle(static_cast<int>(cx + 34.0f * cs), static_cast<int>(cy + 4.0f), 24.0f * cs, cloudTop);
        DrawCircle(static_cast<int>(cx), static_cast<int>(cy - 10.0f * cs), 34.0f * cs, cloudTop);
        DrawCircle(static_cast<int>(cx + 16.0f * cs), static_cast<int>(cy - 4.0f * cs), 28.0f * cs, cloudTop);
    }

    // 2. Далекі синьо-фіолетові гірські хребти з білими сніговими шапками
    Color farMtnCol = Color{ 122, 145, 185, 255 };
    Color snowCol   = Color{ 245, 250, 255, 255 };

    DrawTriangle({ -50.0f, GROUND_Y }, { 180.0f, 320.0f }, { 420.0f, GROUND_Y }, farMtnCol);
    DrawTriangle({ 140.0f, 375.0f }, { 180.0f, 320.0f }, { 220.0f, 375.0f }, snowCol);

    DrawTriangle({ 280.0f, GROUND_Y }, { 520.0f, 340.0f }, { 760.0f, GROUND_Y }, farMtnCol);
    DrawTriangle({ 485.0f, 390.0f }, { 520.0f, 340.0f }, { 555.0f, 390.0f }, snowCol);

    DrawTriangle({ 640.0f, GROUND_Y }, { 880.0f, 310.0f }, { 1120.0f, GROUND_Y }, farMtnCol);
    DrawTriangle({ 845.0f, 360.0f }, { 880.0f, 310.0f }, { 915.0f, 360.0f }, snowCol);

    DrawTriangle({ 950.0f, GROUND_Y }, { 1200.0f, 330.0f }, { 1380.0f, GROUND_Y }, farMtnCol);
    DrawTriangle({ 1170.0f, 380.0f }, { 1200.0f, 330.0f }, { 1230.0f, 380.0f }, snowCol);

    Color midMtnCol = Color{ 65, 125, 88, 255 };
    DrawTriangle({ 60.0f, GROUND_Y }, { 320.0f, 410.0f }, { 580.0f, GROUND_Y }, midMtnCol);
    DrawTriangle({ 480.0f, GROUND_Y }, { 700.0f, 430.0f }, { 940.0f, GROUND_Y }, midMtnCol);
    DrawTriangle({ 800.0f, GROUND_Y }, { 1040.0f, 390.0f }, { 1320.0f, GROUND_Y }, midMtnCol);

    // 3. Величний кам'яний замок Гамсон у денному освітленні
    Color castleStone = Color{ 175, 172, 165, 255 };
    Color castleRoof  = Color{ 185, 65, 52, 255 };
    Color castleMrt   = Color{ 125, 122, 118, 255 };

    // Головна цитадель
    DrawRectangle(920, 240, 140, static_cast<int>(GROUND_Y - 240), castleStone);
    DrawRectangleLines(920, 240, 140, static_cast<int>(GROUND_Y - 240), castleMrt);
    for (int bx = 920; bx < 1060; bx += 24) {
        DrawRectangle(bx, 226, 14, 14, castleStone);
        DrawRectangleLines(bx, 226, 14, 14, castleMrt);
    }
    DrawTriangle({ 990.0f, 150.0f }, { 1015.0f, 226.0f }, { 965.0f, 226.0f }, castleRoof);

    // Ліва готична вежа замку
    DrawRectangle(840, 280, 70, static_cast<int>(GROUND_Y - 280), Color{ 155, 152, 146, 255 });
    DrawRectangleLines(840, 280, 70, static_cast<int>(GROUND_Y - 280), castleMrt);
    DrawTriangle({ 875.0f, 210.0f }, { 915.0f, 280.0f }, { 835.0f, 280.0f }, castleRoof);

    // Права готична вежа замку
    DrawRectangle(1070, 290, 80, static_cast<int>(GROUND_Y - 290), Color{ 155, 152, 146, 255 });
    DrawRectangleLines(1070, 290, 80, static_cast<int>(GROUND_Y - 290), castleMrt);
    DrawTriangle({ 1110.0f, 220.0f }, { 1155.0f, 290.0f }, { 1065.0f, 290.0f }, castleRoof);

    // Маленька сторожова башта
    DrawRectangle(770, 340, 55, static_cast<int>(GROUND_Y - 340), Color{ 150, 147, 142, 255 });
    DrawRectangleLines(770, 340, 55, static_cast<int>(GROUND_Y - 340), castleMrt);
    DrawTriangle({ 797.0f, 290.0f }, { 830.0f, 340.0f }, { 765.0f, 340.0f }, castleRoof);

    // Віконця замку з денними небесними відблисками
    struct CastleWindow { int x; int y; };
    static const CastleWindow C_WINS[] = {
        { 960, 270 }, { 1020, 270 }, { 990, 320 }, { 960, 380 }, { 1020, 380 },
        { 865, 310 }, { 885, 360 }, { 1100, 325 }, { 1125, 375 }, { 790, 370 }
    };
    for (size_t wi = 0; wi < 10; ++wi) {
        DrawRectangle(C_WINS[wi].x, C_WINS[wi].y, 6, 12, Color{ 120, 215, 255, 240 });
        DrawRectangleLines(C_WINS[wi].x, C_WINS[wi].y, 6, 12, Color{ 68, 55, 45, 255 });
    }

    // 4. Зелені смереки та сосни на узбіччі
    Color pineDay = Color{ 32, 98, 48, 255 };
    DrawPineTree(30.0f, GROUND_Y, 140.0f, 65.0f, pineDay);
    DrawPineTree(75.0f, GROUND_Y, 175.0f, 80.0f, Color{ 26, 88, 42, 255 });
    DrawPineTree(120.0f, GROUND_Y, 120.0f, 55.0f, pineDay);
    DrawPineTree(205.0f, GROUND_Y, 110.0f, 50.0f, Color{ 36, 108, 54, 255 });
    DrawPineTree(460.0f, GROUND_Y, 130.0f, 60.0f, pineDay);
    DrawPineTree(725.0f, GROUND_Y, 125.0f, 58.0f, pineDay);
    DrawPineTree(990.0f, GROUND_Y, 145.0f, 68.0f, Color{ 26, 88, 42, 255 });

    // 5. Вуличні чавунні ліхтарі
    DrawStreetTorch(175.0f, GROUND_Y, animTimer_);
    DrawStreetTorch(450.0f, GROUND_Y, animTimer_);
    DrawStreetTorch(730.0f, GROUND_Y, animTimer_);
    DrawStreetTorch(1015.0f, GROUND_Y, animTimer_);

    // 6. Дорога та бруківка
    DrawRectangle(0, static_cast<int>(GROUND_Y), Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT - static_cast<int>(GROUND_Y), Color{ 48, 52, 65, 255 });
    // Верхній бордюр з теплою кам'яною окантовкою
    DrawRectangle(0, static_cast<int>(GROUND_Y), Config::VIRTUAL_WIDTH, 6, Color{ 145, 140, 132, 255 });
    DrawRectangle(0, static_cast<int>(GROUND_Y + 6), Config::VIRTUAL_WIDTH, 4, Color{ 90, 88, 85, 255 });

    // Соковита зелена трава та квіти вздовж узбіччя
    for (int gx = 15; gx < Config::VIRTUAL_WIDTH; gx += 32) {
        DrawLine(gx, static_cast<int>(GROUND_Y + 1), gx - 3, static_cast<int>(GROUND_Y - 6), Color{ 65, 160, 58, 255 });
        DrawLine(gx + 2, static_cast<int>(GROUND_Y + 1), gx + 5, static_cast<int>(GROUND_Y - 7), Color{ 82, 185, 72, 255 });
        if (gx % 64 == 0) {
            DrawCircle(gx + 1, static_cast<int>(GROUND_Y - 7), 2.0f, (gx % 128 == 0) ? GOLD : (gx % 96 == 0 ? WHITE : Color{ 255, 110, 150, 255 }));
        }
    }

    // Шахові плити бруківки
    for (int row = 0; row < 4; ++row) {
        int rowY = static_cast<int>(GROUND_Y + 12 + row * 28);
        int xOffset = (row % 2 == 1) ? 36 : 0;
        for (int col = -1; col < (Config::VIRTUAL_WIDTH / 72) + 2; ++col) {
            int stoneX = col * 72 + xOffset;
            DrawRectangleRounded({ static_cast<float>(stoneX + 3), static_cast<float>(rowY + 2), 66.0f, 24.0f }, 0.25f, 4, Color{ 40, 44, 58, 255 });
            DrawRectangleLinesEx({ static_cast<float>(stoneX + 3), static_cast<float>(rowY + 2), 66.0f, 24.0f }, 1.5f, Color{ 24, 26, 36, 255 });
        }
    }

    // Вказівник повернення до Маєтку ліворуч (дерев'яний дороговказ)
    int signX = 28;
    int signY = static_cast<int>(GROUND_Y - 110.0f);
    DrawRectangle(signX + 44, signY, 8, static_cast<int>(GROUND_Y - signY), Color{ 68, 48, 32, 255 });
    DrawRectangleRounded({ static_cast<float>(signX), static_cast<float>(signY), 108.0f, 32.0f }, 0.25f, 4, Color{ 95, 68, 46, 255 });
    DrawRectangleLinesEx({ static_cast<float>(signX), static_cast<float>(signY), 108.0f, 32.0f }, 2.0f, GOLD);
    Ui::Text("<-- [M] MANOR", signX + 10, signY + 7, 14, GOLD);

    // 7. Кузня Коваля
    if (ctx.GetManorLevel(ManorNodeId::Unlock_Blacksmith) > 0) {
        int bx = static_cast<int>(BLACKSMITH_X - 85.0f);
        int by = static_cast<int>(GROUND_Y - 150.0f);
        // Кам'яна будівля майстерні
        DrawRectangle(bx, by, 170, 150, Color{ 68, 56, 52, 255 });
        DrawRectangleLinesEx({ static_cast<float>(bx), static_cast<float>(by), 170.0f, 150.0f }, 2.5f, Color{ 48, 38, 35, 255 });
        // Горизонтальні шви кам'яної кладки
        for (int myLine = by + 25; myLine < static_cast<int>(GROUND_Y); myLine += 25) {
            DrawLine(bx, myLine, bx + 170, myLine, Color{ 48, 38, 35, 255 });
        }
        // Дерев'яний навіс даху
        DrawTriangle(
            { static_cast<float>(bx - 12), static_cast<float>(by) },
            { static_cast<float>(bx + 85), static_cast<float>(by - 32) },
            { static_cast<float>(bx + 182), static_cast<float>(by) },
            Color{ 105, 48, 42, 255 }
        );
        // Димохід кузні
        DrawRectangle(bx + 130, by - 48, 24, 48, Color{ 54, 44, 42, 255 });
        DrawRectangleLines(bx + 130, by - 48, 24, 48, BLACK);

        // Горно з палаючим вогнем
        int hearthX = bx + 42;
        int hearthY = static_cast<int>(GROUND_Y - 42.0f);
        DrawRectangle(hearthX - 22, hearthY - 14, 44, 42, Color{ 38, 28, 26, 255 });
        DrawCircle(hearthX, hearthY + 4, 18.0f + std::sin(animTimer_ * 12.0f) * 3.0f, ORANGE);
        DrawCircle(hearthX, hearthY + 4, 11.0f + std::sin(animTimer_ * 16.0f) * 2.0f, YELLOW);

        // Важке ковадло
        int anvilX = bx + 105;
        int anvilY = static_cast<int>(GROUND_Y - 26.0f);
        DrawRectangle(anvilX - 6, anvilY + 12, 28, 14, Color{ 75, 52, 38, 255 }); // колода
        DrawRectangle(anvilX - 10, anvilY, 36, 12, Color{ 45, 48, 55, 255 });
        DrawTriangle({ static_cast<float>(anvilX - 18), static_cast<float>(anvilY) }, { static_cast<float>(anvilX - 10), static_cast<float>(anvilY) }, { static_cast<float>(anvilX - 10), static_cast<float>(anvilY + 12) }, Color{ 45, 48, 55, 255 }); // ріг ковадла
        DrawRectangle(anvilX + 4, anvilY - 8, 12, 4, Color{ 140, 145, 155, 255 }); // молот

        // Стійка зі зброєю (меч та щит)
        DrawLineEx({ static_cast<float>(bx + 152), static_cast<float>(GROUND_Y - 42.0f) }, { static_cast<float>(bx + 152), static_cast<float>(GROUND_Y) }, 3.0f, Color{ 190, 200, 215, 255 });
        DrawCircle(bx + 152, static_cast<int>(GROUND_Y - 24.0f), 10.0f, Color{ 78, 62, 32, 255 });
        DrawCircleLines(bx + 152, static_cast<int>(GROUND_Y - 24.0f), 10.0f, GOLD);

        // Вивіска
        DrawRectangleRounded({ static_cast<float>(bx + 25), static_cast<float>(by - 42), 120.0f, 26.0f }, 0.25f, 4, Color{ 32, 24, 22, 230 });
        DrawRectangleLinesEx({ static_cast<float>(bx + 25), static_cast<float>(by - 42), 120.0f, 26.0f }, 1.5f, GOLD);
        Ui::Text("BLACKSMITH", bx + 36, by - 38, 14, GOLD);

        if (std::abs(heroX_ - BLACKSMITH_X) < 75.0f) {
            DrawRectangleRounded({ static_cast<float>(bx + 10), static_cast<float>(by - 72), 150.0f, 24.0f }, 0.25f, 4, Fade(BLACK, 0.82f));
            DrawRectangleLinesEx({ static_cast<float>(bx + 10), static_cast<float>(by - 72), 150.0f, 24.0f }, 1.5f, LIME);
            Ui::Text("[W / E] FORGE GEAR", bx + 18, by - 68, 13, LIME);
        }
    }

    // 8. Намет Чарівниці (Enchantress Pavilion)
    if (ctx.GetManorLevel(ManorNodeId::Unlock_Enchantress) > 0) {
        int ex = static_cast<int>(ENCHANTRESS_X);
        int ey = static_cast<int>(GROUND_Y);

        // Опорні дерев'яні стовпи намету
        DrawRectangle(ex - 80, ey - 150, 6, 150, Color{ 82, 54, 38, 255 });
        DrawRectangle(ex + 74, ey - 150, 6, 150, Color{ 82, 54, 38, 255 });

        // Шовковий фіолетово-малиновий тент
        DrawTriangle(
            { static_cast<float>(ex + 88), static_cast<float>(ey) },
            { static_cast<float>(ex),      static_cast<float>(ey - 155) },
            { static_cast<float>(ex - 88), static_cast<float>(ey) },
            Color{ 95, 26, 115, 255 }
        );
        // Внутрішній темний розріз шатра
        DrawTriangle(
            { static_cast<float>(ex + 45), static_cast<float>(ey) },
            { static_cast<float>(ex),      static_cast<float>(ey - 105) },
            { static_cast<float>(ex - 45), static_cast<float>(ey) },
            Color{ 38, 14, 48, 255 }
        );
        // Золота облямівка шатра
        DrawLineEx({ static_cast<float>(ex - 88), static_cast<float>(ey) }, { static_cast<float>(ex), static_cast<float>(ey - 155) }, 3.0f, GOLD);
        DrawLineEx({ static_cast<float>(ex + 88), static_cast<float>(ey) }, { static_cast<float>(ex), static_cast<float>(ey - 155) }, 3.0f, GOLD);

        // Магічний котел з зіллям
        DrawCircle(ex, ey - 22, 18.0f, Color{ 36, 38, 46, 255 });
        DrawCircle(ex, ey - 26, 13.0f, MAGENTA);
        DrawCircle(ex - 4, ey - 28, 4.0f, RAYWHITE); // бульбашка
        DrawCircle(ex + 5, ey - 25, 3.0f, RAYWHITE);

        // Ширяюча магічна рунна сфера
        float orbWave = std::sin(animTimer_ * 4.0f) * 6.0f;
        Vector2 orbPos = { static_cast<float>(ex), static_cast<float>(ey - 72.0f) + orbWave };
        DrawCircleV(orbPos, 14.0f, Fade(PURPLE, 0.45f));
        DrawCircleV(orbPos, 9.0f, MAGENTA);
        DrawCircleV(orbPos, 4.0f, RAYWHITE);

        // Вивіска
        DrawRectangleRounded({ static_cast<float>(ex - 65), static_cast<float>(ey - 188), 130.0f, 26.0f }, 0.25f, 4, Color{ 32, 18, 44, 230 });
        DrawRectangleLinesEx({ static_cast<float>(ex - 65), static_cast<float>(ey - 188), 130.0f, 26.0f }, 1.5f, MAGENTA);
        Ui::Text("ENCHANTRESS", ex - 52, ey - 184, 14, MAGENTA);

        if (std::abs(heroX_ - ENCHANTRESS_X) < 75.0f) {
            DrawRectangleRounded({ static_cast<float>(ex - 75), static_cast<float>(ey - 218), 150.0f, 24.0f }, 0.25f, 4, Fade(BLACK, 0.82f));
            DrawRectangleLinesEx({ static_cast<float>(ex - 75), static_cast<float>(ey - 218), 150.0f, 24.0f }, 1.5f, LIME);
            Ui::Text("[W / E] EQUIP RUNES", ex - 67, ey - 214, 13, LIME);
        }
    }

    // 9. Стовп та альтанка Архітектора (Architect Drafting Gazebo)
    if (ctx.GetManorLevel(ManorNodeId::Unlock_Architect) > 0) {
        int ax = static_cast<int>(ARCHITECT_X - 55.0f);
        int ay = static_cast<int>(GROUND_Y - 145.0f);

        // Мармурові колони
        DrawRectangle(ax + 8, ay + 26, 12, 119, Color{ 78, 88, 108, 255 });
        DrawRectangle(ax + 90, ay + 26, 12, 119, Color{ 78, 88, 108, 255 });
        // Капітелі колон
        DrawRectangle(ax + 5, ay + 20, 18, 6, Color{ 110, 120, 140, 255 });
        DrawRectangle(ax + 87, ay + 20, 18, 6, Color{ 110, 120, 140, 255 });

        // Класичний трикутний фронтон
        DrawTriangle(
            { static_cast<float>(ax - 8),  static_cast<float>(ay + 20) },
            { static_cast<float>(ax + 55), static_cast<float>(ay - 18) },
            { static_cast<float>(ax + 118), static_cast<float>(ay + 20) },
            Color{ 58, 68, 88, 255 }
        );
        DrawTriangleLines(
            { static_cast<float>(ax - 8),  static_cast<float>(ay + 20) },
            { static_cast<float>(ax + 55), static_cast<float>(ay - 18) },
            { static_cast<float>(ax + 118), static_cast<float>(ay + 20) },
            SKYBLUE
        );

        // Стіл архітектора з кресленнями та сувоями
        int deskX = ax + 24;
        int deskY = static_cast<int>(GROUND_Y - 36.0f);
        DrawRectangle(deskX, deskY, 62, 36, Color{ 64, 45, 32, 255 });
        DrawRectangle(deskX + 6, deskY + 2, 50, 14, Color{ 220, 210, 185, 255 }); // розгорнутий сувій
        DrawLine(deskX + 10, deskY + 6, deskX + 52, deskY + 6, SKYBLUE); // лінії плану

        // Якщо замок замкнено Архітектором — магічний замок на столі
        if (ctx.isCastleLocked) {
            DrawCircle(deskX + 31, deskY - 14, 10.0f, ORANGE);
            DrawCircleLines(deskX + 31, deskY - 14, 10.0f, GOLD);
            Ui::Text("CASTLE LOCKED (70% GOLD)", ax - 45, ay - 68, 13, ORANGE);
        }

        // Вивіска
        DrawRectangleRounded({ static_cast<float>(ax), static_cast<float>(ay - 44), 110.0f, 26.0f }, 0.25f, 4, Color{ 24, 34, 48, 230 });
        DrawRectangleLinesEx({ static_cast<float>(ax), static_cast<float>(ay - 44), 110.0f, 26.0f }, 1.5f, SKYBLUE);
        Ui::Text("ARCHITECT", ax + 14, ay - 40, 14, SKYBLUE);

        if (std::abs(heroX_ - ARCHITECT_X) < 75.0f) {
            DrawRectangleRounded({ static_cast<float>(ax - 20), static_cast<float>(ay - 96), 150.0f, 24.0f }, 0.25f, 4, Fade(BLACK, 0.82f));
            DrawRectangleLinesEx({ static_cast<float>(ax - 20), static_cast<float>(ay - 96), 150.0f, 24.0f }, 1.5f, LIME);
            Ui::Text("[W / E] LOCK CASTLE", ax - 12, ay - 92, 13, LIME);
        }
    }

    // 10. Готична брама Замку та Харон
    int gx = static_cast<int>(CHARON_GATE_X);
    int gy = static_cast<int>(GROUND_Y);

    // Масивна кам'яна брама
    DrawRectangle(gx - 25, gy - 230, 175, 230, Color{ 48, 50, 65, 255 });
    DrawRectangleLinesEx({ static_cast<float>(gx - 25), static_cast<float>(gy - 230), 175.0f, 230.0f }, 3.0f, Color{ 32, 34, 44, 255 });

    // Зубці брами
    for (int bx = gx - 25; bx < gx + 150; bx += 28) {
        DrawRectangle(bx, gy - 246, 18, 16, Color{ 48, 50, 65, 255 });
    }

    // Вхідна арка у підземелля
    DrawRectangle(gx + 12, gy - 175, 115, 175, BLACK);
    DrawCircleSector({ static_cast<float>(gx + 70), static_cast<float>(gy - 175) }, 58.0f, 180.0f, 360.0f, 16, BLACK);

    // Підйомні залізні ґрати (Portcullis) зі шпичаками
    int portY = gy - 175;
    int portH = static_cast<int>(gatePortcullisY_);
    for (int px = gx + 22; px < gx + 120; px += 16) {
        DrawLineEx({ static_cast<float>(px), static_cast<float>(portY) }, { static_cast<float>(px), static_cast<float>(portY + portH) }, 4.0f, LIGHTGRAY);
        DrawTriangle(
            { static_cast<float>(px - 4), static_cast<float>(portY + portH) },
            { static_cast<float>(px + 4), static_cast<float>(portY + portH) },
            { static_cast<float>(px),     static_cast<float>(portY + portH + 8) },
            LIGHTGRAY
        );
    }

    // Смолоскипи обабіч брами
    DrawStreetTorch(gx - 10.0f, gy - 50.0f, animTimer_);
    DrawStreetTorch(gx + 140.0f, gy - 50.0f, animTimer_ + 1.2f);

    // Постать похмурого перевізника Харона перед входом
    int cx = gx - 45;
    int cy = gy;
    bool nearCharon = (std::abs(heroX_ - CHARON_GATE_X) < 85.0f);
    if (ctx.renderer) {
        ctx.renderer->DrawDetailedCharon({ static_cast<float>(cx), static_cast<float>(cy) }, animTimer_, nearCharon);
    }

    DrawRectangleRounded({ static_cast<float>(cx - 40), static_cast<float>(cy - 116), 80.0f, 22.0f }, 0.25f, 4, Color{ 20, 16, 28, 230 });
    DrawRectangleRoundedLines({ static_cast<float>(cx - 40), static_cast<float>(cy - 116), 80.0f, 22.0f }, 0.25f, 4, 1.5f, GOLD);
    Ui::Text("CHARON", cx - 28, cy - 112, 13, GOLD);

    if (nearCharon && activeMenu_ == ModalMenu::None && !charonSequenceActive_) {
        DrawRectangleRounded({ static_cast<float>(gx - 14), static_cast<float>(gy - 200), 168.0f, 26.0f }, 0.25f, 4, Fade(BLACK, 0.85f));
        DrawRectangleRoundedLines({ static_cast<float>(gx - 14), static_cast<float>(gy - 200), 168.0f, 26.0f }, 0.25f, 4, 1.5f, GOLD);
        Ui::Text("[E / ENTER] PAY TOLL", gx - 2, gy - 195, 13, GOLD);
    }

    // 11. Рендер лицаря (зі стрибком, падінням та ударом мечем)
    if (ctx.renderer) {
        HeroRenderParams hp{};
        CalculatedStats st = ctx.ComputePlayerStats();
        hp.centerPos = { heroX_, heroY_ - 27.0f * st.scaleY };
        hp.velocity = { heroVelX_, heroVelY_ };
        hp.scaleX = st.scaleX;
        hp.scaleY = st.scaleY;
        hp.facingDir = facingDir_;
        hp.classType = ctx.activeHeir.classType;
        hp.swordTier = static_cast<EquipTier>(ctx.equippedTier[0]);
        hp.helmTier  = static_cast<EquipTier>(ctx.equippedTier[1]);
        hp.chestTier = static_cast<EquipTier>(ctx.equippedTier[2]);
        hp.limbsTier = static_cast<EquipTier>(ctx.equippedTier[3]);
        hp.capeTier  = static_cast<EquipTier>(ctx.equippedTier[4]);
        hp.isAttacking = (attackTimer_ > 0.0f);
        hp.isDownStriking = (!onGround_ && (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) && attackTimer_ > 0.0f);
        hp.attackProgress = (Config::SWORD_ATTACK_DURATION > 0.0f) ? (1.0f - (attackTimer_ / Config::SWORD_ATTACK_DURATION)) : 1.0f;
        hp.headlampOn = true;
        hp.animTime = animTimer_;
        ctx.renderer->DrawHero(hp);
    }

    // Монети, що летять до Харона
    for (const auto& fc : flyingCoins_) {
        if (fc.active) {
            DrawCircleV(fc.pos, 6.0f, GOLD);
            DrawCircleLines(static_cast<int>(fc.pos.x), static_cast<int>(fc.pos.y), 6.0f, ORANGE);
        }
    }

    // 12. Нижня панель підказок керування у лобі
    int hintBarY = static_cast<int>(GROUND_Y + 98.0f);
    DrawRectangleRounded({ 160.0f, static_cast<float>(hintBarY), static_cast<float>(Config::VIRTUAL_WIDTH - 320), 32.0f }, 0.35f, 6, Color{ 14, 16, 26, 215 });
    DrawRectangleLinesEx({ 160.0f, static_cast<float>(hintBarY), static_cast<float>(Config::VIRTUAL_WIDTH - 320), 32.0f }, 1.5f, Color{ 60, 65, 85, 255 });
    Ui::Text("[A / D] MOVE   *   [SPACE] JUMP   *   [J / LMB] ATTACK   *   [W / E] INTERACT   *   [M] MANOR",
             205, hintBarY + 8, 14, Color{ 220, 228, 245, 255 });

    // 13. Верхня інфо-панель
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, 56, Fade(BLACK, 0.72f));
    Ui::Text("HEIR: " + ctx.activeHeir.name + " (" + Registry::GetClass(ctx.activeHeir.classType).name + ")", 24, 18, 18, GOLD);
    Ui::Text("GOLD: " + std::to_string(ctx.gold) + " G", 540, 18, 18, YELLOW);
    CalculatedStats curSt = ctx.ComputePlayerStats();
    Ui::Text("WEIGHT: " + std::to_string(curSt.currentWeight) + " / " + std::to_string(curSt.maxWeight), 780, 18, 18, SKYBLUE);

    if (ctx.particles) {
        ctx.particles->Draw();
    }

    // 14. Модальні вікна NPC
    if (activeMenu_ == ModalMenu::Blacksmith) {
        DrawBlacksmithModal(ctx);
    } else if (activeMenu_ == ModalMenu::Enchantress) {
        DrawEnchantressModal(ctx);
    } else if (activeMenu_ == ModalMenu::Architect) {
        DrawArchitectModal(ctx);
    } else if (activeMenu_ == ModalMenu::CharonToll) {
        DrawCharonModal(ctx);
    }
}

void OutskirtsState::DrawBlacksmithModal(const GameContext& ctx) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.78f));

    int mw = 940;
    int mh = 550;
    int mx = (Config::VIRTUAL_WIDTH - mw) / 2;
    int my = (Config::VIRTUAL_HEIGHT - mh) / 2;

    Rectangle modalRec = { static_cast<float>(mx), static_cast<float>(my), static_cast<float>(mw), static_cast<float>(mh) };
    DrawRectangleRounded(modalRec, 0.04f, 4, Color{ 22, 24, 38, 252 });
    DrawRectangleRoundedLines(modalRec, 0.04f, 4, 3.5f, GOLD);

    // Золоті кутові ковані акценти
    DrawLineEx({ modalRec.x, modalRec.y + 24.0f }, { modalRec.x, modalRec.y }, 4.0f, GOLD);
    DrawLineEx({ modalRec.x, modalRec.y }, { modalRec.x + 24.0f, modalRec.y }, 4.0f, GOLD);
    DrawLineEx({ modalRec.x + modalRec.width - 24.0f, modalRec.y }, { modalRec.x + modalRec.width, modalRec.y }, 4.0f, GOLD);
    DrawLineEx({ modalRec.x + modalRec.width, modalRec.y }, { modalRec.x + modalRec.width, modalRec.y + 24.0f }, 4.0f, GOLD);

    Ui::Text("ROYAL BLACKSMITH FORGE", mx + 32, my + 18, 22, GOLD);
    Ui::Text("[A/D] Category   [W/S] Tier   [SPACE/ENTER] Buy / Equip   [ESC] Close", mx + 380, my + 22, 14, Color{ 220, 225, 235, 255 });

    // 5 вкладок категорій (Sword, Helm, Chest, Limbs, Cape) з іконками
    for (int s = 0; s < 5; ++s) {
        int tx = mx + 28 + s * 178;
        int ty = my + 64;
        bool activeTab = (s == menuSlotTab_);
        Rectangle tabRec = { static_cast<float>(tx), static_cast<float>(ty), 166.0f, 38.0f };
        DrawRectangleRounded(tabRec, 0.25f, 4, activeTab ? Color{ 82, 64, 26, 255 } : Color{ 34, 36, 52, 255 });
        DrawRectangleRoundedLines(tabRec, 0.25f, 4, activeTab ? 2.0f : 1.0f, activeTab ? GOLD : GRAY);

        if (ctx.renderer) {
            ctx.renderer->DrawEquipSlotIcon({ static_cast<float>(tx + 24), static_cast<float>(ty + 19) }, static_cast<EquipSlot>(s), activeTab);
        }
        std::string sName = Registry::GetEquipSlotName(static_cast<EquipSlot>(s));
        Ui::Text(sName.c_str(), tx + 46, ty + 10, 15, activeTab ? GOLD : RAYWHITE);
    }

    // Список 5 тирів обраного слота
    size_t slotIdx = static_cast<size_t>(menuSlotTab_);
    for (int t = 0; t < 5; ++t) {
        size_t tierIdx = static_cast<size_t>(t);
        const EquipmentDef& eq = Registry::GetEquipment(static_cast<EquipSlot>(slotIdx), static_cast<EquipTier>(tierIdx));
        bool unlocked = ctx.unlockedBlueprints[slotIdx][tierIdx];
        bool owned    = ctx.ownedEquipment[slotIdx][tierIdx];
        bool equipped = (ctx.equippedTier[slotIdx] == t);
        bool sel      = (t == menuItemRow_);

        int ry = my + 120 + t * 62;
        Rectangle rowRec = { static_cast<float>(mx + 28), static_cast<float>(ry), 475.0f, 54.0f };
        DrawRectangleRounded(rowRec, 0.25f, 4, sel ? Color{ 52, 56, 88, 255 } : Color{ 30, 32, 48, 255 });
        DrawRectangleRoundedLines(rowRec, 0.25f, 4, sel ? 2.0f : 1.0f, sel ? GOLD : Color{ 70, 75, 95, 255 });

        if (ctx.renderer) {
            ctx.renderer->DrawEquipTierBadge({ static_cast<float>(mx + 56), static_cast<float>(ry + 27) }, static_cast<EquipTier>(t));
        }

        Color nameCol = unlocked ? RAYWHITE : DARKGRAY;
        if (equipped) nameCol = LIME;
        Ui::Text(eq.name.c_str(), mx + 82, ry + 8, 17, nameCol);

        std::string statusStr;
        Color statusCol = GRAY;
        if (!unlocked) {
            statusStr = "LOCKED (Find Blueprint in Castle)";
            statusCol = RED;
        } else if (equipped) {
            statusStr = "EQUIPPED  [Weight: " + std::to_string(eq.weight) + "]";
            statusCol = LIME;
        } else if (owned) {
            statusStr = "OWNED - [SPACE] Equip  [Weight: " + std::to_string(eq.weight) + "]";
            statusCol = SKYBLUE;
        } else {
            statusStr = "FORGE: " + std::to_string(eq.cost) + " G  [Weight: " + std::to_string(eq.weight) + "]";
            statusCol = (ctx.gold >= eq.cost) ? YELLOW : RED;
        }
        Ui::Text(statusStr.c_str(), mx + 82, ry + 30, 13, statusCol);
    }

    // Права панель порівняння статів (Зелений '+' / Червоний '-') та шкала Ваги
    int px = mx + 524;
    int py = my + 120;
    Rectangle previewRec = { static_cast<float>(px), static_cast<float>(py), 388.0f, 304.0f };
    DrawRectangleRounded(previewRec, 0.08f, 4, Color{ 16, 18, 28, 255 });
    DrawRectangleRoundedLines(previewRec, 0.08f, 4, 2.0f, GOLD);

    const EquipmentDef& candEq = Registry::GetEquipment(static_cast<EquipSlot>(slotIdx), static_cast<EquipTier>(menuItemRow_));
    EquipmentDef curEq{};
    curEq.weight = 0;
    if (ctx.equippedTier[slotIdx] >= 0) {
        curEq = Registry::GetEquipment(static_cast<EquipSlot>(slotIdx), static_cast<EquipTier>(ctx.equippedTier[slotIdx]));
    }

    Ui::Text(candEq.name.c_str(), px + 20, py + 14, 20, GOLD);
    Ui::TextWrapped(candEq.description.c_str(), px + 20, py + 40, 348, 13, Color{ 210, 215, 225, 255 });

    auto DrawDiffLine = [&](const char* label, int curV, int newV, int yPos) {
        int diff = newV - curV;
        Ui::Text(label, px + 20, yPos, 16, RAYWHITE);
        std::string valStr = std::to_string(newV) + "  (" + (diff >= 0 ? "+" : "") + std::to_string(diff) + ")";
        Color c = (diff > 0) ? LIME : ((diff < 0) ? RED : Color{ 175, 180, 195, 255 });
        Ui::Text(valStr.c_str(), px + 195, yPos, 16, c);
    };

    DrawDiffLine("HP Bonus:",    curEq.hpBonus,    candEq.hpBonus,    py + 78);
    DrawDiffLine("MP Bonus:",    curEq.mpBonus,    candEq.mpBonus,    py + 106);
    DrawDiffLine("STR Bonus:",   curEq.strBonus,   candEq.strBonus,   py + 134);
    DrawDiffLine("INT Bonus:",   curEq.intBonus,   candEq.intBonus,   py + 162);
    DrawDiffLine("Armor Bonus:", curEq.armorBonus, candEq.armorBonus, py + 190);
    DrawDiffLine("Item Weight:", curEq.weight,     candEq.weight,     py + 218);

    // Шкала CurrentWeight / MaxWeight
    int curTotalWt = ctx.GetCurrentEquipWeight();
    int maxWt      = ctx.GetMaxEquipWeight();
    int projectedWt = curTotalWt - curEq.weight + candEq.weight;

    int barY = my + 448;
    Ui::Text(("EQUIP WEIGHT CAPACITY: " + std::to_string(curTotalWt) + " / " + std::to_string(maxWt) +
             "  (Projected: " + std::to_string(projectedWt) + ")").c_str(), mx + 32, barY, 15, (projectedWt <= maxWt) ? SKYBLUE : RED);

    Rectangle barRec = { static_cast<float>(mx + 28), static_cast<float>(barY + 26), static_cast<float>(mw - 56), 24.0f };
    DrawRectangleRounded(barRec, 0.35f, 4, BLACK);
    float wtRatio = std::clamp(static_cast<float>(projectedWt) / static_cast<float>(std::max(1, maxWt)), 0.0f, 1.0f);
    if (wtRatio > 0.01f) {
        Rectangle fillRec = { barRec.x, barRec.y, barRec.width * wtRatio, barRec.height };
        DrawRectangleRounded(fillRec, 0.35f, 4, (projectedWt <= maxWt) ? SKYBLUE : RED);
    }
    DrawRectangleRoundedLines(barRec, 0.35f, 4, 1.5f, GOLD);
}

void OutskirtsState::DrawEnchantressModal(const GameContext& ctx) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.78f));

    int mw = 940;
    int mh = 570;
    int mx = (Config::VIRTUAL_WIDTH - mw) / 2;
    int my = (Config::VIRTUAL_HEIGHT - mh) / 2;

    Rectangle modalRec = { static_cast<float>(mx), static_cast<float>(my), static_cast<float>(mw), static_cast<float>(mh) };
    DrawRectangleRounded(modalRec, 0.04f, 4, Color{ 26, 18, 38, 252 });
    DrawRectangleRoundedLines(modalRec, 0.04f, 4, 3.5f, MAGENTA);

    // Аметистові магічні кути
    DrawLineEx({ modalRec.x, modalRec.y + 24.0f }, { modalRec.x, modalRec.y }, 4.0f, MAGENTA);
    DrawLineEx({ modalRec.x, modalRec.y }, { modalRec.x + 24.0f, modalRec.y }, 4.0f, MAGENTA);
    DrawLineEx({ modalRec.x + modalRec.width - 24.0f, modalRec.y }, { modalRec.x + modalRec.width, modalRec.y }, 4.0f, MAGENTA);
    DrawLineEx({ modalRec.x + modalRec.width, modalRec.y }, { modalRec.x + modalRec.width, modalRec.y + 24.0f }, 4.0f, MAGENTA);

    Ui::Text("ENCHANTRESS RUNE SANCTUM", mx + 32, my + 18, 22, MAGENTA);
    Ui::Text("Identical Runes in different slots STACK their effects!", mx + 430, my + 22, 14, GOLD);

    // Вкладки слотів з іконками
    for (int s = 0; s < 5; ++s) {
        int tx = mx + 28 + s * 178;
        int ty = my + 60;
        bool activeTab = (s == menuSlotTab_);
        Rectangle tabRec = { static_cast<float>(tx), static_cast<float>(ty), 166.0f, 44.0f };
        DrawRectangleRounded(tabRec, 0.25f, 4, activeTab ? Color{ 92, 34, 115, 255 } : Color{ 36, 26, 52, 255 });
        DrawRectangleRoundedLines(tabRec, 0.25f, 4, activeTab ? 2.0f : 1.0f, activeTab ? MAGENTA : GRAY);

        if (ctx.renderer) {
            ctx.renderer->DrawEquipSlotIcon({ static_cast<float>(tx + 24), static_cast<float>(ty + 22) }, static_cast<EquipSlot>(s), activeTab);
        }
        std::string sName = Registry::GetEquipSlotName(static_cast<EquipSlot>(s));
        Ui::Text(sName.c_str(), tx + 46, ty + 6, 14, activeTab ? GOLD : RAYWHITE);

        RuneType eqR = ctx.equippedRunes[static_cast<size_t>(s)];
        std::string rSub = (eqR == RuneType::None) ? "[Empty]" : Registry::GetRune(eqR).name;
        Ui::Text(rSub.c_str(), tx + 46, ty + 24, 12, (eqR == RuneType::None) ? Color{ 170, 175, 190, 255 } : SKYBLUE);
    }

    // Список 8 рун з індивідуальними procedural іконками
    size_t slotIdx = static_cast<size_t>(menuSlotTab_);
    for (int r = 0; r < 8; ++r) {
        size_t rIdx = static_cast<size_t>(r);
        RuneType rType = static_cast<RuneType>(r);
        const RuneDef& rDef = Registry::GetRune(rType);

        bool unlocked = ctx.unlockedRunes[slotIdx][rIdx];
        bool owned    = ctx.ownedRunes[slotIdx][rIdx];
        bool equipped = (ctx.equippedRunes[slotIdx] == rType);
        bool sel      = (r == menuItemRow_);

        int ry = my + 118 + r * 53;
        Rectangle rowRec = { static_cast<float>(mx + 28), static_cast<float>(ry), static_cast<float>(mw - 56), 47.0f };
        DrawRectangleRounded(rowRec, 0.25f, 4, sel ? Color{ 64, 40, 92, 255 } : Color{ 32, 22, 48, 255 });
        DrawRectangleRoundedLines(rowRec, 0.25f, 4, sel ? 2.0f : 1.0f, sel ? GOLD : Color{ 75, 55, 95, 255 });

        // Унікальна рунна скрижаль ліворуч
        if (ctx.renderer) {
            ctx.renderer->DrawRuneIcon({ static_cast<float>(mx + 62), static_cast<float>(ry + 23) }, rType, unlocked, animTimer_);
        }

        Ui::Text(rDef.name.c_str(), mx + 96, ry + 6, 17, unlocked ? (equipped ? LIME : GOLD) : DARKGRAY);
        Ui::Text(rDef.description.c_str(), mx + 96, ry + 26, 13, unlocked ? Color{ 215, 220, 230, 255 } : DARKGRAY);

        // Статус-бейдж праворуч
        std::string rightStatus;
        Color rightCol = GRAY;
        if (!unlocked) {
            rightStatus = "LOCKED (Fairy Chest)";
            rightCol = RED;
        } else if (equipped) {
            rightStatus = "EQUIPPED";
            rightCol = LIME;
        } else if (owned) {
            rightStatus = "OWNED - [SPACE] Equip";
            rightCol = SKYBLUE;
        } else {
            rightStatus = "ENCHANT: " + std::to_string(rDef.cost) + " G";
            rightCol = (ctx.gold >= rDef.cost) ? YELLOW : RED;
        }

        int rw = Ui::Measure(rightStatus.c_str(), 14);
        Rectangle pillRec = { static_cast<float>(mx + mw - rw - 58), static_cast<float>(ry + 10), static_cast<float>(rw + 18), 26.0f };
        DrawRectangleRounded(pillRec, 0.4f, 4, Color{ 20, 16, 30, 240 });
        DrawRectangleRoundedLines(pillRec, 0.4f, 4, 1.2f, rightCol);
        Ui::Text(rightStatus.c_str(), static_cast<int>(pillRec.x + 9.0f), ry + 16, 14, rightCol);
    }
}

void OutskirtsState::DrawArchitectModal(const GameContext& ctx) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.78f));

    int mw = 640;
    int mh = 320;
    int mx = (Config::VIRTUAL_WIDTH - mw) / 2;
    int my = (Config::VIRTUAL_HEIGHT - mh) / 2;

    DrawRectangle(mx, my, mw, mh, Color{ 20, 28, 42, 250 });
    DrawRectangleLinesEx({ static_cast<float>(mx), static_cast<float>(my), static_cast<float>(mw), static_cast<float>(mh) }, 3.5f, SKYBLUE);

    Ui::Text("THE ROYAL ARCHITECT", mx + 28, my + 22, 24, SKYBLUE);
    Ui::Text("Lock the exact 9x9 Castle Layout from your previous run", mx + 28, my + 68, 16, RAYWHITE);
    Ui::Text("so you can return straight to unopened Fairy Chests and Bosses.", mx + 28, my + 92, 16, RAYWHITE);
    Ui::Text("Penalty: All Gold collected while locked is reduced to 70%.", mx + 28, my + 125, 16, ORANGE);

    std::string stateTxt = std::string("CURRENT STATUS: ") + (ctx.isCastleLocked ? "CASTLE LOCKED (70% GOLD)" : "FRESH CASTLE (100% GOLD)");
    Ui::Text(stateTxt, mx + 28, my + 180, 20, ctx.isCastleLocked ? GOLD : LIME);

    Ui::Text("Press [SPACE / ENTER] to Toggle Lock   |   Press [ESC] to Close", mx + 28, my + 252, 16, Color{ 220, 225, 235, 255 });
}

void OutskirtsState::DrawCharonModal(const GameContext& ctx) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.78f));

    int mw = 680;
    int mh = 420;
    int mx = (Config::VIRTUAL_WIDTH - mw) / 2;
    int my = (Config::VIRTUAL_HEIGHT - mh) / 2;

    Rectangle modalRec = { static_cast<float>(mx), static_cast<float>(my), static_cast<float>(mw), static_cast<float>(mh) };
    DrawRectangleRounded(modalRec, 0.04f, 4, Color{ 18, 16, 26, 252 });
    DrawRectangleRoundedLines(modalRec, 0.04f, 4, 2.5f, GOLD);

    // Header bar
    DrawRectangle(mx + 2, my + 2, mw - 4, 38, Color{ 36, 26, 48, 255 });
    DrawLine(mx, my + 40, mx + mw, my + 40, GOLD);
    Ui::Text("CHARON'S TOLL - FERRYMAN OF CASTLE HAMSON", mx + 24, my + 11, 17, GOLD);

    // Left side: Charon figure showcase
    if (ctx.renderer) {
        ctx.renderer->DrawDetailedCharon({ static_cast<float>(mx + 115), static_cast<float>(my + 280) }, animTimer_, true);
    }

    // Right side: Dialogue & Gold ledger
    int rx = mx + 230;
    int rw = mw - 260;

    Ui::Text("\"All who seek entry into Castle Hamson", rx, my + 56, 13, Color{ 235, 220, 180, 255 });
    Ui::Text(" must pay the Ferryman his toll.", rx, my + 74, 13, Color{ 235, 220, 180, 255 });
    Ui::Text(" Earthly riches cannot cross into the underworld.\"", rx, my + 92, 13, Color{ 235, 220, 180, 255 });

    // Ledger frame
    Rectangle ledgerRec = { static_cast<float>(rx), static_cast<float>(my + 118), static_cast<float>(rw), 188.0f };
    DrawRectangleRounded(ledgerRec, 0.05f, 4, Color{ 26, 22, 36, 255 });
    DrawRectangleRoundedLines(ledgerRec, 0.05f, 4, 1.5f, Color{ 90, 85, 115, 255 });

    CalculatedStats st = ctx.ComputePlayerStats();
    int totalGold = ctx.gold;
    int keptGold = static_cast<int>(std::round(totalGold * st.hagglingKeptRatio));
    int tollPaid = std::max(0, totalGold - keptGold);
    int discountPercent = static_cast<int>(std::round(st.hagglingKeptRatio * 100.0f));

    Ui::Text("GOLD SETTLEMENT LEDGER", rx + 16, my + 130, 13, GOLD);
    DrawLine(rx + 16, my + 148, rx + rw - 16, my + 148, Color{ 70, 65, 88, 255 });

    std::string goldCarried = "Total Gold Carried: " + std::to_string(totalGold) + " G";
    Ui::Text(goldCarried.c_str(), rx + 16, my + 158, 14, Color{ 255, 235, 80, 255 });

    std::string haggleStr = "Haggling Skill Retention: " + std::to_string(discountPercent) + "% (Kept: " + std::to_string(keptGold) + " G)";
    Ui::Text(haggleStr.c_str(), rx + 16, my + 186, 12, (keptGold > 0) ? LIME : Color{ 180, 185, 200, 255 });

    DrawLine(rx + 16, my + 214, rx + rw - 16, my + 214, Color{ 70, 65, 88, 255 });

    std::string tollStr = "Charon's Ferry Fee: -" + std::to_string(tollPaid) + " G";
    Ui::Text(tollStr.c_str(), rx + 16, my + 226, 15, (tollPaid > 0) ? Color{ 255, 110, 110, 255 } : LIME);

    std::string remainingStr = "Gold Remaining in Pocket: " + std::to_string(keptGold) + " G";
    Ui::Text(remainingStr.c_str(), rx + 16, my + 258, 13, GOLD);

    // Action buttons at bottom
    Rectangle enterBtn = { static_cast<float>(rx), static_cast<float>(my + 328), static_cast<float>(rw), 38.0f };
    DrawRectangleRec(enterBtn, Color{ 42, 92, 48, 255 });
    DrawRectangleLinesEx(enterBtn, 2.0f, GOLD);
    const char* enterTxt = "PAY TOLL & ENTER CASTLE [ENTER / SPACE]";
    int etw = Ui::Measure(enterTxt, 12);
    Ui::Text(enterTxt, rx + (rw - etw) / 2, static_cast<int>(enterBtn.y + 12.0f), 12, WHITE);

    Rectangle backBtn = { static_cast<float>(mx + 30), static_cast<float>(my + 328), 170.0f, 38.0f };
    DrawRectangleRec(backBtn, Color{ 48, 38, 55, 255 });
    DrawRectangleLinesEx(backBtn, 1.5f, Color{ 110, 100, 125, 255 });
    const char* backTxt = "CAMP [ESC / TAB]";
    int btw = Ui::Measure(backTxt, 13);
    Ui::Text(backTxt, mx + 30 + (170 - btw) / 2, static_cast<int>(backBtn.y + 11.0f), 13, Color{ 220, 220, 235, 255 });
}