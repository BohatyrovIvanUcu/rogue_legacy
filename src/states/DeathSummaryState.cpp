// === FILE: src/states/DeathSummaryState.cpp ===
#include "states/DeathSummaryState.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "core/SaveManager.hpp"
#include "data/Registry.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/ProceduralRenderer.hpp"
#include "gfx/UiText.hpp"
#include <raylib.h>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <cmath>

void DeathSummaryState::OnEnter(GameContext& ctx) {
    animTimer_ = 0.0f;
    fireworkTimer_ = 0.0f;
    galleryIndex_ = -1; // За замовчуванням показуємо підсумки поточного забігу
    if (ctx.audio) {
        ctx.audio->SetBossMusic(false);
    }
}

void DeathSummaryState::OnExit(GameContext& /*ctx*/) {}

void DeathSummaryState::Update(float dt, GameContext& ctx) {
    animTimer_ += dt;
    if (ctx.particles) {
        ctx.particles->Update(dt);
    }

    // Святковий салют при перемозі
    if (ctx.lastRunVictory && ctx.particles) {
        fireworkTimer_ += dt;
        if (fireworkTimer_ >= 0.38f) {
            fireworkTimer_ = 0.0f;
            Vector2 fwPos = {
                static_cast<float>(180 + std::rand() % (Config::VIRTUAL_WIDTH - 360)),
                static_cast<float>(90 + std::rand() % 240)
            };
            Color cols[4] = { GOLD, SKYBLUE, MAGENTA, LIME };
            ctx.particles->SpawnExplosion(fwPos, 95.0f, cols[std::rand() % 4]);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 1.15f, 0.55f);
        }
    }

    int histSize = static_cast<int>(ctx.lineageHistory.size());

    // Оновлення горизонтальної анімації скролу галереї
    if (galleryIndex_ >= 0) {
        targetScrollX_ = static_cast<float>(galleryIndex_) * 460.0f;
        galleryScrollX_ += (targetScrollX_ - galleryScrollX_) * std::min(1.0f, 12.0f * dt);
    }

    // Перемикання вліво: перехід до галереї картин предків
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
        if (galleryIndex_ == -1) {
            if (histSize > 0) {
                galleryIndex_ = histSize - 1; // останній збережений предок
                targetScrollX_ = static_cast<float>(galleryIndex_) * 460.0f;
                galleryScrollX_ = targetScrollX_;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f);
            }
        } else if (galleryIndex_ > 0) {
            galleryIndex_--;
            targetScrollX_ = static_cast<float>(galleryIndex_) * 460.0f;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f);
        }
    }

    // Перемикання вправо: перехід до наступного предка або повернення
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
        if (galleryIndex_ == -1) {
            if (histSize > 0) {
                galleryIndex_ = 0; // найперший предок
                targetScrollX_ = static_cast<float>(galleryIndex_) * 460.0f;
                galleryScrollX_ = targetScrollX_;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f);
            }
        } else if (galleryIndex_ < histSize - 1) {
            galleryIndex_++;
            targetScrollX_ = static_cast<float>(galleryIndex_) * 460.0f;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f);
        }
    }

    // Перемикання між оглядом забігу та галереєю спадкоємців (TAB / ESC)
    if (IsKeyPressed(KEY_TAB) || (galleryIndex_ >= 0 && IsKeyPressed(KEY_ESCAPE))) {
        if (galleryIndex_ == -1 && histSize > 0) {
            galleryIndex_ = histSize - 1;
            targetScrollX_ = static_cast<float>(galleryIndex_) * 460.0f;
            galleryScrollX_ = targetScrollX_;
        } else {
            galleryIndex_ = -1;
        }
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.1f);
    }

    // Вибір наступного спадкоємця
    if (animTimer_ > 0.45f && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))) {
        ctx.GenerateNewHeirTrio();
        SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen);
        ctx.RequestStateChange(GameStateId::Lineage);
    }
}

void DeathSummaryState::DrawCastleCorridorBackground(float time) const {
    // 1. Темний готичний цегляний фон замкового коридору
    DrawRectangleGradientV(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Color{ 22, 18, 30, 255 }, Color{ 12, 10, 18, 255 });

    // Горизонтальні та вертикальні шви цегляної кладки стіни коридору
    Color mortarCol = Color{ 36, 30, 46, 255 };
    for (int y = 0; y < 580; y += 40) {
        DrawLine(0, y, Config::VIRTUAL_WIDTH, y, mortarCol);
        int offset = ((y / 40) % 2 == 1) ? 48 : 0;
        for (int x = offset; x < Config::VIRTUAL_WIDTH; x += 96) {
            DrawLine(x, y, x, y + 40, mortarCol);
        }
    }

    // Верхні стрілчасті готичні склепіння коридору (Vaulted stone arches)
    for (int ax = 160; ax < Config::VIRTUAL_WIDTH; ax += 320) {
        DrawLineEx({ static_cast<float>(ax - 160), 0.0f }, { static_cast<float>(ax), 70.0f }, 3.5f, Color{ 48, 42, 62, 255 });
        DrawLineEx({ static_cast<float>(ax + 160), 0.0f }, { static_cast<float>(ax), 70.0f }, 3.5f, Color{ 48, 42, 62, 255 });
        DrawCircle(ax, 70, 7.0f, Color{ 68, 58, 85, 255 });
    }

    // 2. Чавунні настінні смолоскипи (Wall Sconce Torches) з теплим ореолом вогню
    const int torchX[4] = { 160, 480, 800, 1120 };
    for (int tx : torchX) {
        int ty = 210;
        // М'яке радіальне світло від смолоскипа
        float flicker = 0.85f + 0.15f * std::sin(time * 11.0f + tx);
        DrawCircleGradient(tx, ty - 12, 160.0f * flicker, Fade(Color{ 255, 165, 40, 255 }, 0.22f), BLANK);

        // Чавунне кріплення смолоскипа на кам'яній стіні
        DrawRectangle(tx - 6, ty + 10, 12, 18, Color{ 38, 34, 45, 255 });
        DrawLineEx({ static_cast<float>(tx - 6), static_cast<float>(ty + 20) }, { static_cast<float>(tx), static_cast<float>(ty) }, 3.0f, Color{ 60, 55, 70, 255 });
        DrawRectangle(tx - 8, ty - 6, 16, 16, Color{ 48, 44, 58, 255 });
        DrawRectangleLines(tx - 8, ty - 6, 16, 16, Color{ 85, 78, 100, 255 });

        // Живе мерехтливе полум'я
        float fh = 20.0f + std::sin(time * 16.0f + tx) * 4.0f;
        DrawTriangle(
            { static_cast<float>(tx - 7), static_cast<float>(ty - 6) },
            { static_cast<float>(tx), static_cast<float>(ty - 6 - fh) },
            { static_cast<float>(tx + 7), static_cast<float>(ty - 6) },
            Color{ 235, 60, 20, 240 }
        );
        DrawTriangle(
            { static_cast<float>(tx - 4), static_cast<float>(ty - 6) },
            { static_cast<float>(tx), static_cast<float>(ty - 6 - fh * 0.75f) },
            { static_cast<float>(tx + 4), static_cast<float>(ty - 6) },
            ORANGE
        );
        DrawTriangle(
            { static_cast<float>(tx - 2), static_cast<float>(ty - 6) },
            { static_cast<float>(tx), static_cast<float>(ty - 6 - fh * 0.45f) },
            { static_cast<float>(tx + 2), static_cast<float>(ty - 6) },
            YELLOW
        );
    }

    // 3. Кам'яна бруківка підлоги коридору з лініями перспективи
    DrawRectangle(0, 580, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT - 580, Color{ 18, 16, 24, 255 });
    DrawLineEx({ 0.0f, 580.0f }, { static_cast<float>(Config::VIRTUAL_WIDTH), 580.0f }, 3.5f, Color{ 65, 58, 80, 255 });
    for (int fx = 0; fx < Config::VIRTUAL_WIDTH; fx += 160) {
        DrawLine(fx, 580, fx - 40, Config::VIRTUAL_HEIGHT, Color{ 34, 30, 44, 255 });
    }
}

void DeathSummaryState::DrawRunOverviewModal(GameContext& ctx) {
    std::string banner = ctx.lastRunVictory
        ? ("VICTORY!  NEW GAME+ " + std::to_string(ctx.ngPlusLevel) + " UNLOCKED!")
        : "YOUR HERO HAS FALLEN";
    Color bannerCol = ctx.lastRunVictory ? GOLD : RED;

    int bw = Ui::Measure(banner.c_str(), 32);
    Rectangle titlePill = { static_cast<float>((Config::VIRTUAL_WIDTH - bw) / 2 - 24), 22.0f, static_cast<float>(bw + 48), 48.0f };
    DrawRectangleRounded(titlePill, 0.35f, 4, Color{ 18, 14, 26, 235 });
    DrawRectangleRoundedLines(titlePill, 0.35f, 4, 2.5f, bannerCol);

    Ui::Text(banner.c_str(), titlePill.x + 24 + 2, titlePill.y + 8 + 2, 32, BLACK);
    Ui::Text(banner.c_str(), titlePill.x + 24, titlePill.y + 8, 32, bannerCol);

    // Велика заокруглена кам'яна меморіальна плита
    int boxW = 1120;
    int boxH = 510;
    int bx = (Config::VIRTUAL_WIDTH - boxW) / 2;
    int by = 86;

    Rectangle cardRec = { static_cast<float>(bx), static_cast<float>(by), static_cast<float>(boxW), static_cast<float>(boxH) };
    DrawRectangleRounded(cardRec, 0.04f, 4, Color{ 20, 18, 30, 248 });
    DrawRectangleRoundedLines(cardRec, 0.04f, 4, 3.5f, bannerCol);

    // Золоті кутові ковані накладки
    DrawLineEx({ cardRec.x, cardRec.y + 24.0f }, { cardRec.x, cardRec.y }, 4.0f, bannerCol);
    DrawLineEx({ cardRec.x, cardRec.y }, { cardRec.x + 24.0f, cardRec.y }, 4.0f, bannerCol);
    DrawLineEx({ cardRec.x + cardRec.width - 24.0f, cardRec.y }, { cardRec.x + cardRec.width, cardRec.y }, 4.0f, bannerCol);
    DrawLineEx({ cardRec.x + cardRec.width, cardRec.y }, { cardRec.x + cardRec.width, cardRec.y + 24.0f }, 4.0f, bannerCol);

    // 1. Ліва частина: Портретна рама героя
    Rectangle pRec = { static_cast<float>(bx + 28), static_cast<float>(by + 24), 270.0f, 380.0f };
    DrawRectangleRounded(pRec, 0.08f, 4, Color{ 14, 12, 22, 255 });
    DrawRectangleRoundedLines(pRec, 0.08f, 4, 2.0f, GOLD);

    // П'єдестал та прев'ю героя
    Vector2 heroPos = { pRec.x + pRec.width * 0.5f, pRec.y + 190.0f };
    DrawCircleGradient(static_cast<int>(heroPos.x), static_cast<int>(heroPos.y), 65.0f, Fade(Color{ 80, 120, 170, 255 }, 0.28f), BLANK);
    DrawEllipse(static_cast<int>(heroPos.x), static_cast<int>(heroPos.y + 48.0f), 65.0f, 14.0f, Color{ 36, 40, 56, 255 });
    DrawEllipseLines(static_cast<int>(heroPos.x), static_cast<int>(heroPos.y + 48.0f), 65.0f, 14.0f, GOLD);

    if (ctx.renderer) {
        ctx.renderer->DrawHeroPreview(heroPos, 1.85f, ctx.activeHeir, ctx, animTimer_);
    }

    // Плашка з іменем під портретом
    int nhw = Ui::Measure(ctx.activeHeir.name.c_str(), 20);
    Ui::Text(ctx.activeHeir.name.c_str(), pRec.x + (pRec.width - nhw) / 2, pRec.y + 280, 20, GOLD);
    std::string clsName = "Class: " + Registry::GetClass(ctx.activeHeir.classType).name;
    int chw = Ui::Measure(clsName.c_str(), 15);
    Ui::Text(clsName.c_str(), pRec.x + (pRec.width - chw) / 2, pRec.y + 312, 15, SKYBLUE);

    std::string genText = "Generation " + std::to_string(ctx.generationCount);
    int ghw = Ui::Measure(genText.c_str(), 14);
    Ui::Text(genText.c_str(), pRec.x + (pRec.width - ghw) / 2, pRec.y + 338, 14, Color{ 200, 205, 220, 255 });

    // 2. Середня частина: Підсумкова статистика забігу
    int sx = bx + 328;
    int sy = by + 32;

    Ui::Text("EXPEDITION RECORD", sx, sy, 22, GOLD);
    DrawLine(sx, sy + 30, sx + 270, sy + 30, Color{ 145, 110, 45, 255 });

    std::string fateLine = (ctx.lastRunVictory ? "Triumph: " : "Slain by: ") + ctx.lastKillerName;
    if (ctx.CurrentHeirHasTrait(TraitType::Dyslexia)) {
        fateLine = Ui::ScrambleDyslexia(fateLine);
    }
    Ui::Text(fateLine.c_str(), sx, sy + 46, 17, ctx.lastRunVictory ? LIME : RED);

    Ui::Text(("Gold Plundered in Run: " + std::to_string(ctx.runGoldCollected) + " G").c_str(), sx, sy + 88, 16, YELLOW);
    Ui::Text(("Royal Treasury for Lineage: " + std::to_string(ctx.gold) + " G").c_str(), sx, sy + 124, 16, GOLD);
    Ui::Text(("Total Monsters Slain: " + std::to_string(ctx.runKills)).c_str(), sx, sy + 160, 16, RAYWHITE);
    Ui::Text(("Charon Toll Incurred: " + std::to_string(ctx.charonTollPaid) + " G").c_str(), sx, sy + 196, 16, Color{ 220, 140, 70, 255 });

    // Кнопка швидкого переходу до галереї картин
    Rectangle galleryBtn = { static_cast<float>(sx), static_cast<float>(sy + 250), 280.0f, 44.0f };
    DrawRectangleRounded(galleryBtn, 0.35f, 4, Color{ 42, 34, 62, 255 });
    DrawRectangleRoundedLines(galleryBtn, 0.35f, 4, 1.8f, GOLD);
    Ui::Text("[A / D] ANCESTRAL GALLERY", galleryBtn.x + 22, galleryBtn.y + 12, 16, GOLD);

    // 3. Права частина: СПИСОК ВБИТИХ МОНСТРІВ З ЇХНІМИ ПРОЦЕДУРНИМИ МОДЕЛЬКАМИ (Monsters Slain Breakdown)
    int rx = bx + 640;
    int ry = by + 24;
    int rw = 452;
    int rh = 460;

    Rectangle beastsRec = { static_cast<float>(rx), static_cast<float>(ry), static_cast<float>(rw), static_cast<float>(rh) };
    DrawRectangleRounded(beastsRec, 0.05f, 4, Color{ 14, 12, 22, 255 });
    DrawRectangleRoundedLines(beastsRec, 0.05f, 4, 1.8f, GOLD);

    Ui::Text("BEASTS SLAIN THIS EXPEDITION", rx + 20, ry + 16, 18, GOLD);
    DrawLine(rx + 20, ry + 42, rx + rw - 20, ry + 42, Color{ 90, 75, 45, 255 });

    // 6 рядків для кожного типу ворогів: Skeleton, FloatingEye, ShieldKnight, FireMage, Warg, WallTurret
    const char* enemyNames[6] = {
        "Skeleton Soldier",
        "Floating Doom Eye",
        "Castle Shield Knight",
        "Pyromancer Mage",
        "Blood Warg",
        "Wall Cannon Turret"
    };

    for (int e = 0; e < 6; ++e) {
        EnemyType et = static_cast<EnemyType>(e);
        int count = ctx.runSlainByType[static_cast<size_t>(e)];
        int rowY = ry + 54 + e * 64;

        Rectangle itemRec = { static_cast<float>(rx + 16), static_cast<float>(rowY), static_cast<float>(rw - 32), 54.0f };
        DrawRectangleRounded(itemRec, 0.25f, 4, (count > 0) ? Color{ 26, 22, 36, 255 } : Color{ 18, 16, 24, 255 });
        DrawRectangleRoundedLines(itemRec, 0.25f, 4, 1.2f, (count > 0) ? Color{ 78, 70, 96, 255 } : Color{ 40, 36, 48, 255 });

        // Мініатюрна процедурна моделька монстра
        if (ctx.renderer) {
            ctx.renderer->DrawEnemyIcon({ itemRec.x + 28.0f, itemRec.y + 27.0f }, et, 28.0f);
        }

        // Назва монстра
        Ui::Text(enemyNames[e], itemRec.x + 60.0f, itemRec.y + 10.0f, 15, (count > 0) ? RAYWHITE : DARKGRAY);

        // Кількість убитих
        std::string countStr = "x " + std::to_string(count);
        Color cCol = (count > 0) ? GOLD : DARKGRAY;
        int cw = Ui::Measure(countStr.c_str(), 18);
        Ui::Text(countStr.c_str(), itemRec.x + itemRec.width - cw - 18.0f, itemRec.y + 16.0f, 18, cCol);
    }

    // Нижня підказка
    const char* prompt = "PRESS [ENTER / SPACE] TO CHOOSE YOUR NEXT HEIR";
    int pw = Ui::Measure(prompt, 20);
    Ui::Text(prompt, (Config::VIRTUAL_WIDTH - pw) / 2, 642, 20, GOLD);
}

void DeathSummaryState::DrawAncestorGalleryModal(GameContext& ctx) {
    int histSize = static_cast<int>(ctx.lineageHistory.size());
    if (histSize == 0 || galleryIndex_ < 0 || galleryIndex_ >= histSize) {
        galleryIndex_ = -1;
        return;
    }

    // Верхній геральдичний заголовок галереї
    std::string header = "ROYAL ANCESTRAL MEMORIAL GALLERY";
    int hw = Ui::Measure(header.c_str(), 28);
    Ui::Text(header.c_str(), (Config::VIRTUAL_WIDTH - hw) / 2 + 1, 23, 28, BLACK);
    Ui::Text(header.c_str(), (Config::VIRTUAL_WIDTH - hw) / 2, 22, 28, GOLD);

    std::string subHeader = "PORTRAIT " + std::to_string(galleryIndex_ + 1) + " OF " + std::to_string(histSize);
    int shw = Ui::Measure(subHeader.c_str(), 16);
    Ui::Text(subHeader.c_str(), (Config::VIRTUAL_WIDTH - shw) / 2 + 1, 55, 16, BLACK);
    Ui::Text(subHeader.c_str(), (Config::VIRTUAL_WIDTH - shw) / 2, 54, 16, SKYBLUE);

    // Малювання безперервного ряду картин на стіні коридору замку зі скролом
    constexpr float SPACING = 460.0f;

    for (int i = 0; i < histSize; ++i) {
        float cx = (Config::VIRTUAL_WIDTH * 0.5f) + (i * SPACING) - galleryScrollX_;

        // Кулінг картин, що вийшли за межі екрана
        if (cx < -280.0f || cx > Config::VIRTUAL_WIDTH + 280.0f) {
            continue;
        }

        const SlainRecord& rec = ctx.lineageHistory[static_cast<size_t>(i)];

        float distFromCenter = std::abs(cx - Config::VIRTUAL_WIDTH * 0.5f);
        float focus = std::clamp(1.0f - distFromCenter / 450.0f, 0.0f, 1.0f);
        float alpha = 0.55f + 0.45f * focus;
        float scale = 0.88f + 0.12f * focus;

        int pw = static_cast<int>(360.0f * scale);
        int ph = static_cast<int>(370.0f * scale);
        int px = static_cast<int>(cx - pw * 0.5f);
        int py = static_cast<int>(84.0f + (370.0f - ph) * 0.5f);

        // Настінний смолоскип між картинами
        if (i < histSize - 1) {
            float torchX = cx + SPACING * 0.5f;
            if (torchX >= 0.0f && torchX <= Config::VIRTUAL_WIDTH) {
                DrawRectangle(static_cast<int>(torchX - 5), 230, 10, 45, Color{ 48, 38, 32, 255 });
                DrawCircle(static_cast<int>(torchX), 220, 14.0f, Fade(ORANGE, 0.45f));
                DrawCircle(static_cast<int>(torchX), 220, 8.0f + std::sin(animTimer_ * 10.0f + i) * 2.0f, GOLD);
                DrawCircle(static_cast<int>(torchX), 218, 4.0f, YELLOW);
            }
        }

        // 1. Золота барокова різьблена рама картини
        Rectangle outerFrame = { static_cast<float>(px), static_cast<float>(py), static_cast<float>(pw), static_cast<float>(ph) };
        DrawRectangleRounded(outerFrame, 0.05f, 4, Fade(Color{ 88, 68, 24, 255 }, alpha));
        DrawRectangleRoundedLines(outerFrame, 0.05f, 4, 5.0f * scale, Fade(focus > 0.8f ? GOLD : Color{ 175, 140, 55, 255 }, alpha));

        // Внутрішнє оксамитове паспарту картини
        Rectangle innerMat = { outerFrame.x + 12.0f * scale, outerFrame.y + 12.0f * scale, outerFrame.width - 24.0f * scale, outerFrame.height - 24.0f * scale };
        DrawRectangleRounded(innerMat, 0.04f, 4, Fade(Color{ 20, 16, 26, 255 }, alpha));
        DrawRectangleRoundedLines(innerMat, 0.04f, 4, 2.0f, Fade(Color{ 145, 110, 45, 255 }, alpha));

        // Полотно картини з готичним градієнтом
        Rectangle canvasRec = { innerMat.x + 6.0f, innerMat.y + 6.0f, innerMat.width - 12.0f, innerMat.height - 12.0f };
        DrawRectangleGradientV(
            static_cast<int>(canvasRec.x),
            static_cast<int>(canvasRec.y),
            static_cast<int>(canvasRec.width),
            static_cast<int>(canvasRec.height),
            Fade(rec.wasVictory ? Color{ 36, 48, 72, 255 } : Color{ 34, 24, 38, 255 }, alpha),
            Fade(Color{ 14, 10, 18, 255 }, alpha)
        );

        // Парадний портрет предка в повний зріст
        Vector2 heroPos = { canvasRec.x + canvasRec.width * 0.5f, canvasRec.y + canvasRec.height * 0.62f };
        DrawCircleGradient(static_cast<int>(heroPos.x), static_cast<int>(heroPos.y - 15.0f), 70.0f * scale, Fade(Color{ 120, 160, 220, 255 }, 0.22f * alpha), BLANK);
        DrawEllipse(static_cast<int>(heroPos.x), static_cast<int>(heroPos.y + 44.0f * scale), 68.0f * scale, 14.0f * scale, Fade(Color{ 42, 38, 56, 255 }, alpha));
        DrawEllipseLines(static_cast<int>(heroPos.x), static_cast<int>(heroPos.y + 44.0f * scale), 68.0f * scale, 14.0f * scale, Fade(GOLD, alpha));

        if (ctx.renderer) {
            HeirCandidate tempHeir{};
            tempHeir.name = rec.heroName;
            tempHeir.classType = rec.heroClass;
            tempHeir.spell = SpellType::Dagger;
            tempHeir.visualSeed = 1000 + static_cast<uint32_t>(i * 791);
            ctx.renderer->DrawHeroPreview(heroPos, 1.85f * scale, tempHeir, ctx, animTimer_);
        }

        // Лавровий вінок переможця на полотні
        if (rec.wasVictory) {
            Rectangle vBadge = { canvasRec.x + canvasRec.width - 96.0f * scale, canvasRec.y + 10.0f, 86.0f * scale, 24.0f };
            DrawRectangleRounded(vBadge, 0.4f, 4, Fade(Color{ 85, 65, 18, 255 }, alpha));
            DrawRectangleRoundedLines(vBadge, 0.4f, 4, 1.5f, Fade(GOLD, alpha));
            Ui::Text("VICTOR", static_cast<int>(vBadge.x + 12.0f), static_cast<int>(vBadge.y + 5.0f), 12, GOLD);
        }

        // 2. Латунна табличка під картиною
        int plaqueW = static_cast<int>(400.0f * scale);
        int plaqueH = 92;
        int plaqueX = static_cast<int>(cx - plaqueW * 0.5f);
        int plaqueY = py + ph + 10;

        Rectangle plaqueRec = { static_cast<float>(plaqueX), static_cast<float>(plaqueY), static_cast<float>(plaqueW), static_cast<float>(plaqueH) };
        DrawRectangleRounded(plaqueRec, 0.15f, 4, Fade(Color{ 36, 28, 18, 245 }, alpha));
        DrawRectangleRoundedLines(plaqueRec, 0.15f, 4, 2.2f, Fade(focus > 0.8f ? GOLD : Color{ 160, 130, 45, 255 }, alpha));

        // Гвинтики на кутах таблички
        DrawCircle(plaqueX + 8, plaqueY + 8, 2.5f, Fade(GOLD, alpha));
        DrawCircle(plaqueX + plaqueW - 8, plaqueY + 8, 2.5f, Fade(GOLD, alpha));
        DrawCircle(plaqueX + 8, plaqueY + plaqueH - 8, 2.5f, Fade(GOLD, alpha));
        DrawCircle(plaqueX + plaqueW - 8, plaqueY + plaqueH - 8, 2.5f, Fade(GOLD, alpha));

        // Текст таблички
        int tnw = Ui::Measure(rec.heroName.c_str(), 18);
        Ui::Text(rec.heroName.c_str(), plaqueX + (plaqueW - tnw) / 2, plaqueY + 10, 18, Fade(GOLD, alpha));

        std::string fateDesc = rec.wasVictory
            ? "Triumphant Overlord"
            : ("Slain by: " + rec.killerName);
        int fdw = Ui::Measure(fateDesc.c_str(), 14);
        Ui::Text(fateDesc.c_str(), plaqueX + (plaqueW - fdw) / 2, plaqueY + 36, 14, Fade(rec.wasVictory ? LIME : RED, alpha));

        std::string statsLine = Registry::GetClass(rec.heroClass).name +
                                " * Slain: " + std::to_string(rec.enemiesSlain) +
                                " * Gold: " + std::to_string(rec.goldCollected) + " G";
        int slw = Ui::Measure(statsLine.c_str(), 13);
        Ui::Text(statsLine.c_str(), plaqueX + (plaqueW - slw) / 2, plaqueY + 62, 13, Fade(YELLOW, alpha));
    }

    // 3. Інтерактивні навігаційні кнопки зі стрілками по боках
    if (galleryIndex_ > 0) {
        Rectangle leftArrow = { 40.0f, 290.0f, 150.0f, 50.0f };
        DrawRectangleRounded(leftArrow, 0.35f, 4, Color{ 26, 22, 36, 235 });
        DrawRectangleRoundedLines(leftArrow, 0.35f, 4, 1.8f, GOLD);
        Ui::Text("<-- [A] PREV", static_cast<int>(leftArrow.x + 18), static_cast<int>(leftArrow.y + 15), 16, GOLD);
    }

    if (galleryIndex_ < histSize - 1) {
        Rectangle rightArrow = { static_cast<float>(Config::VIRTUAL_WIDTH - 190), 290.0f, 150.0f, 50.0f };
        DrawRectangleRounded(rightArrow, 0.35f, 4, Color{ 26, 22, 36, 235 });
        DrawRectangleRoundedLines(rightArrow, 0.35f, 4, 1.8f, GOLD);
        Ui::Text("NEXT [D] -->", static_cast<int>(rightArrow.x + 22), static_cast<int>(rightArrow.y + 15), 16, GOLD);
    }

    // Нижні підказки керування
    const char* hint = "[<-- A / D -->] Slide Along Hallway Wall   *   [TAB / ESC] Run Overview   *   [ENTER / SPACE] Choose Next Heir";
    int hmw = Ui::Measure(hint, 16);
    Ui::Text(hint, (Config::VIRTUAL_WIDTH - hmw) / 2 + 1, 659, 16, BLACK);
    Ui::Text(hint, (Config::VIRTUAL_WIDTH - hmw) / 2, 658, 16, Color{ 220, 225, 240, 255 });
}

void DeathSummaryState::Draw(GameContext& ctx) {
    // 1. Замковий коридор на задньому плані з факелами
    DrawCastleCorridorBackground(animTimer_);

    // 2. Або огляд забігу з модельками монстрів, або галерея картин предків
    if (galleryIndex_ == -1) {
        DrawRunOverviewModal(ctx);
    } else {
        DrawAncestorGalleryModal(ctx);
    }

    if (ctx.particles) {
        ctx.particles->Draw();
    }
}