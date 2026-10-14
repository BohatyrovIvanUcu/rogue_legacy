#include "gfx/ProceduralRenderer.hpp"
#include "gfx/Palette.hpp"
#include "gfx/UiText.hpp"
#include "data/Registry.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include <cmath>
#include <algorithm>

Color ProceduralRenderer::GetTierPrimaryColor(EquipTier tier, Color defaultCol) {
    const auto& pal = GetPalette();
    switch (tier) {
        case EquipTier::Squire:   return pal.metal[3];
        case EquipTier::Silver:   return pal.metal[4];
        case EquipTier::Imperial: return pal.bloodCloth[3];
        case EquipTier::Royal:    return pal.fireGold[3];
        case EquipTier::Dark:     return pal.dungeon.stoneTile[3];
        default:                  return defaultCol;
    }
}

Color ProceduralRenderer::GetTierSecondaryColor(EquipTier tier, Color defaultCol) {
    const auto& pal = GetPalette();
    switch (tier) {
        case EquipTier::Squire:   return pal.bloodCloth[2];
        case EquipTier::Silver:   return pal.moonMagic[3];
        case EquipTier::Imperial: return pal.fireGold[3];
        case EquipTier::Royal:    return pal.moonMagic[4];
        case EquipTier::Dark:     return pal.bloodCloth[4];
        default:                  return defaultCol;
    }
}

namespace {
    inline uint32_t TileHash(int col, int row, uint32_t seed = 0) {
        uint32_t h = static_cast<uint32_t>(col) * 73856093u ^ static_cast<uint32_t>(row) * 19349663u ^ seed * 83492791u;
        h ^= h >> 13;
        h *= 0x5bd1e995u;
        h ^= h >> 15;
        return h;
    }

    void DrawDetailedSword(
        Vector2 handPos,
        Vector2 tipPos,
        EquipTier tier,
        float sx,
        float sy,
        float alpha,
        [[maybe_unused]] bool isAttacking,
        [[maybe_unused]] float attackProgress,
        bool isDownStriking,
        float animTime
    ) {
        const auto& pal = GetPalette();
        Color bladeBaseCol = ProceduralRenderer::GetTierPrimaryColor(tier, pal.metal[4]);
        Color hiltCol      = ProceduralRenderer::GetTierSecondaryColor(tier, pal.fireGold[3]);

        Vector2 dirVec = { tipPos.x - handPos.x, tipPos.y - handPos.y };
        float len = std::hypot(dirVec.x, dirVec.y);
        if (len < 1.0f) len = 1.0f;
        Vector2 u = { dirVec.x / len, dirVec.y / len };  // одиничний вектор вздовж клинка
        Vector2 n = { -u.y, u.x };                      // нормаль

        // Розміри елементів меча (Автентичний широкий палаш Rogue Legacy)
        float guardHalfW = 14.5f * sx;
        float pommelDist = 7.5f * sx;
        float bladeWidth = 13.8f * sy;
        float fullerLen  = len * 0.72f;

        Vector2 pommelPos = { handPos.x - u.x * pommelDist, handPos.y - u.y * pommelDist };
        Vector2 guardPos  = { handPos.x + u.x * (2.0f * sx), handPos.y + u.y * (2.0f * sx) };

        // 1. Навершя (Pommel)
        float pommelR = 4.4f * sx;
        DrawCircleV(pommelPos, pommelR + 0.8f, Fade(pal.outline, alpha));
        DrawCircleV(pommelPos, pommelR, Fade(hiltCol, alpha));
        DrawCircleV({ pommelPos.x - 1.0f * sx, pommelPos.y - 1.0f * sy }, 1.4f * sx, Fade(pal.metal[4], alpha * 0.8f));
        // Самоцвіт у наверші залежно від тиру
        if (tier == EquipTier::Silver) {
            DrawCircleV(pommelPos, pommelR * 0.55f, Fade(pal.moonMagic[3], alpha));
        } else if (tier == EquipTier::Imperial) {
            DrawCircleV(pommelPos, pommelR * 0.55f, Fade(pal.bloodCloth[3], alpha));
        } else if (tier == EquipTier::Royal) {
            DrawCircleV(pommelPos, pommelR * 0.55f, Fade(pal.moonMagic[4], alpha));
        } else if (tier == EquipTier::Dark) {
            DrawCircleV(pommelPos, pommelR * 0.55f, Fade(pal.bloodCloth[4], alpha));
        }

        // 2. Руків'я (Grip)
        Color gripCol = pal.wood[1];
        DrawLineEx(pommelPos, guardPos, 4.5f * sy, Fade(pal.outline, alpha));
        DrawLineEx(pommelPos, guardPos, 3.2f * sy, Fade(gripCol, alpha));
        for (int g = 1; g <= 3; ++g) {
            float gt = g * 0.25f;
            Vector2 gPt = { pommelPos.x + (guardPos.x - pommelPos.x) * gt, pommelPos.y + (guardPos.y - pommelPos.y) * gt };
            DrawLineEx({ gPt.x - n.x * 2.2f * sx, gPt.y - n.y * 2.2f * sx },
                       { gPt.x + n.x * 2.2f * sx, gPt.y + n.y * 2.2f * sx }, 1.5f, Fade(pal.fireGold[3], alpha));
        }

        // 3. Гарда (Crossguard) із золотими крилами та рубіном по центру
        Vector2 gL = { guardPos.x - n.x * guardHalfW, guardPos.y - n.y * guardHalfW };
        Vector2 gR = { guardPos.x + n.x * guardHalfW, guardPos.y + n.y * guardHalfW };
        DrawLineEx({ gL.x - u.x * 0.8f, gL.y - u.y * 0.8f }, { gR.x - u.x * 0.8f, gR.y - u.y * 0.8f }, 5.2f * sy, Fade(pal.outline, alpha));
        DrawLineEx(gL, gR, 4.2f * sy, Fade(hiltCol, alpha));
        DrawCircleV(gL, 3.0f * sx, Fade(hiltCol, alpha));
        DrawCircleV(gR, 3.0f * sx, Fade(hiltCol, alpha));

        // 4. Клинок (Blade) — широкий силует, подвійний скіс та вістря
        Vector2 bBaseL = { guardPos.x - n.x * (bladeWidth * 0.5f), guardPos.y - n.y * (bladeWidth * 0.5f) };
        Vector2 bBaseR = { guardPos.x + n.x * (bladeWidth * 0.5f), guardPos.y + n.y * (bladeWidth * 0.5f) };

        float taperStart = len * 0.80f;
        Vector2 bTaperL = { guardPos.x + u.x * taperStart - n.x * (bladeWidth * 0.5f), guardPos.y + u.y * taperStart - n.y * (bladeWidth * 0.5f) };
        Vector2 bTaperR = { guardPos.x + u.x * taperStart + n.x * (bladeWidth * 0.5f), guardPos.y + u.y * taperStart + n.y * (bladeWidth * 0.5f) };

        Color lightEdge  = ColorTint(bladeBaseCol, pal.metal[4]);
        Color shadedEdge = ColorTint(bladeBaseCol, pal.metal[2]);
        if (tier == EquipTier::Dark) {
            lightEdge  = pal.dungeon.stoneTile[3];
            shadedEdge = pal.dungeon.stoneTile[1];
        }

        // Контур леза
        Color outlineCol = pal.outline;
        DrawLineEx(bBaseL, bTaperL, 2.0f * sx, Fade(outlineCol, alpha));
        DrawLineEx(bTaperL, tipPos, 2.0f * sx, Fade(outlineCol, alpha));
        DrawLineEx(bBaseR, bTaperR, 2.0f * sx, Fade(outlineCol, alpha));
        DrawLineEx(bTaperR, tipPos, 2.0f * sx, Fade(outlineCol, alpha));

        // Тіло клинка: прямокутна частина та трикутне вістря
        Vector2 midCore = { guardPos.x + u.x * taperStart, guardPos.y + u.y * taperStart };
        // Ліва грань
        DrawTriangle(bBaseL, bTaperL, guardPos, Fade(lightEdge, alpha));
        DrawTriangle(guardPos, bTaperL, midCore, Fade(lightEdge, alpha));
        DrawTriangle(midCore, bTaperL, tipPos, Fade(lightEdge, alpha));
        // Права грань
        DrawTriangle(guardPos, bBaseR, bTaperR, Fade(shadedEdge, alpha));
        DrawTriangle(guardPos, bTaperR, midCore, Fade(shadedEdge, alpha));
        DrawTriangle(midCore, tipPos, bTaperR, Fade(shadedEdge, alpha));

        // 5. Автентичний сяючий блакитний дол меча
        Vector2 fullerStart = { guardPos.x + u.x * (3.0f * sx), guardPos.y + u.y * (3.0f * sx) };
        Vector2 fullerEnd   = { guardPos.x + u.x * fullerLen,   guardPos.y + u.y * fullerLen };
        
        Color fullerGlow = pal.moonMagic[3];
        if (tier == EquipTier::Imperial) fullerGlow = pal.fireGold[3];
        else if (tier == EquipTier::Royal) fullerGlow = pal.moonMagic[4];
        else if (tier == EquipTier::Dark) fullerGlow = pal.bloodCloth[3];

        // Темний жолобок долу
        DrawLineEx(fullerStart, fullerEnd, 5.0f * sy, Fade(pal.metal[0], alpha * 0.95f));
        // Яскравий неоновий промінь долу
        DrawLineEx(fullerStart, fullerEnd, 3.4f * sy, Fade(fullerGlow, alpha * 0.95f));
        // Біло-блакитне ядро сяйва
        DrawLineEx(fullerStart, fullerEnd, 1.5f * sy, Fade(pal.moonMagic[4], alpha * 0.95f));

        // Загострений кінець долу
        Vector2 fTipL = { fullerEnd.x - n.x * 2.0f * sy - u.x * 2.0f * sx, fullerEnd.y - n.y * 2.0f * sy - u.y * 2.0f * sx };
        Vector2 fTipR = { fullerEnd.x + n.x * 2.0f * sy - u.x * 2.0f * sx, fullerEnd.y + n.y * 2.0f * sy - u.y * 2.0f * sx };
        Vector2 fApex = { fullerEnd.x + u.x * 3.5f * sx, fullerEnd.y + u.y * 3.5f * sx };
        DrawTriangle(fTipL, fApex, fTipR, Fade(fullerGlow, alpha));

        // 6. Червоний рубін по центру гарди
        DrawCircleV(guardPos, 4.4f * sx, Fade(pal.outline, alpha));
        DrawCircleV(guardPos, 3.6f * sx, Fade(pal.bloodCloth[3], alpha));
        DrawCircleV({ guardPos.x - 1.0f * sx, guardPos.y - 1.0f * sy }, 1.3f * sx, Fade(pal.metal[4], alpha * 0.9f));

        // Спецефекти клинка за тирами
        if (tier == EquipTier::Silver) {
            float rPulse = 0.5f + 0.5f * std::sin(animTime * 6.0f);
            DrawLineEx(guardPos, fullerEnd, 2.2f * sx, Fade(pal.moonMagic[3], alpha * 0.7f * rPulse));
        } else if (tier == EquipTier::Imperial) {
            float fPulse = 0.6f + 0.4f * std::sin(animTime * 14.0f);
            DrawLineEx(bBaseL, tipPos, 2.2f * sx, Fade(pal.fireGold[3], alpha * 0.7f * fPulse));
            DrawLineEx(bBaseR, tipPos, 2.2f * sx, Fade(pal.bloodCloth[3], alpha * 0.7f * fPulse));
        } else if (tier == EquipTier::Royal) {
            float rPulse = 0.55f + 0.45f * std::sin(animTime * 8.0f);
            DrawLineEx(guardPos, fullerEnd, 2.5f * sx, Fade(pal.moonMagic[4], alpha * 0.85f * rPulse));
            DrawCircleV({ guardPos.x + u.x * (len * 0.4f), guardPos.y + u.y * (len * 0.4f) }, 2.8f * sx, Fade(pal.fireGold[3], alpha));
        } else if (tier == EquipTier::Dark) {
            DrawLineEx(bBaseL, tipPos, 2.2f * sx, Fade(pal.bloodCloth[3], alpha * 0.85f));
            DrawLineEx(bBaseR, tipPos, 2.2f * sx, Fade(pal.bloodCloth[4], alpha * 0.85f));
        }

        DrawCircleV(tipPos, 2.2f * sx, Fade(pal.metal[4], alpha * 0.95f));

        // Аеродинамічні лінії вітру при pogo downstrike
        if (isDownStriking) {
            DrawLineEx({ bTaperL.x - 4.0f * sx, bTaperL.y - 14.0f * sy }, { bTaperL.x - 4.0f * sx, bTaperL.y + 6.0f * sy }, 2.0f, Fade(pal.metal[4], 0.65f));
            DrawLineEx({ bTaperR.x + 4.0f * sx, bTaperR.y - 14.0f * sy }, { bTaperR.x + 4.0f * sx, bTaperR.y + 6.0f * sy }, 2.0f, Fade(pal.metal[4], 0.65f));
            DrawCircleV(tipPos, 5.0f * sx, Fade(pal.metal[4], 0.8f));
        }
    }

    void DrawDetailedShield(
        Vector2 centerPos,
        [[maybe_unused]] float facingDir,
        float sx,
        float sy,
        float alpha,
        ClassType classType,
        EquipTier tier
    ) {
        const auto& pal = GetPalette();
        float sw = 18.0f * sx;
        float sh = 38.0f * sy;
        float x = centerPos.x;
        float y = centerPos.y;

        Color shieldBase = (classType == ClassType::Paladin_Upgraded) ? pal.moonMagic[2] : pal.metal[2];
        Color rimColor   = pal.fireGold[3];
        if (tier == EquipTier::Silver) {
            shieldBase = pal.metal[4];
            rimColor   = pal.moonMagic[3];
        } else if (tier == EquipTier::Imperial) {
            shieldBase = pal.bloodCloth[2];
            rimColor   = pal.fireGold[3];
        } else if (tier == EquipTier::Royal) {
            shieldBase = pal.dungeon.stoneTile[2];
            rimColor   = pal.fireGold[3];
        } else if (tier == EquipTier::Dark) {
            shieldBase = pal.dungeon.stoneTile[1];
            rimColor   = pal.bloodCloth[3];
        }

        // Тіло геральдичного щита (Heater Shield)
        float topH = sh * 0.65f;
        Rectangle topRec = { x - sw * 0.5f, y - sh * 0.5f, sw, topH };
        DrawRectangleRec(topRec, Fade(shieldBase, alpha));
        DrawTriangle(
            { x - sw * 0.5f, y - sh * 0.5f + topH },
            { x + sw * 0.5f, y - sh * 0.5f + topH },
            { x, y + sh * 0.5f },
            Fade(shieldBase, alpha)
        );

        // Металевий обід з заклепками
        DrawRectangleLinesEx(topRec, 2.2f * sx, Fade(rimColor, alpha));
        DrawLineEx({ x - sw * 0.5f, y - sh * 0.5f + topH }, { x, y + sh * 0.5f }, 2.2f * sx, Fade(rimColor, alpha));
        DrawLineEx({ x + sw * 0.5f, y - sh * 0.5f + topH }, { x, y + sh * 0.5f }, 2.2f * sx, Fade(rimColor, alpha));

        // Центральний умбон
        DrawCircle(static_cast<int>(x), static_cast<int>(y - 3.0f * sy), 5.5f * sx, Fade(rimColor, alpha));
        DrawCircle(static_cast<int>(x), static_cast<int>(y - 3.0f * sy), 3.0f * sx, Fade(pal.metal[4], alpha * 0.75f));

        // Геральдичний золотий хрест
        DrawLineEx({ x - sw * 0.38f, y - 3.0f * sy }, { x + sw * 0.38f, y - 3.0f * sy }, 2.0f * sy, Fade(pal.fireGold[3], alpha * 0.85f));
        DrawLineEx({ x, y - sh * 0.35f }, { x, y + sh * 0.25f }, 2.0f * sx, Fade(pal.fireGold[3], alpha * 0.85f));
    }
}

void ProceduralRenderer::DrawHero(const HeroRenderParams& p) const {
    if (p.isInvulnerable && (static_cast<int>(p.animTime * 22.0f) % 2 == 0)) {
        return;
    }

    const auto& pal = GetPalette();
    float alpha = p.isMistForm ? 0.38f : 1.0f;
    float sx = p.scaleX;
    float sy = p.scaleY;
    float dir = (p.facingDir >= 0.0f) ? 1.0f : -1.0f;

    float bodyW = 24.0f * sx;
    float bodyH = 26.0f * sy;
    float headR = 11.5f * ((sx + sy) * 0.5f);

    Vector2 c = p.centerPos;
    if (p.height > 0.0f) {
        float bottomY = p.centerPos.y + p.height * 0.5f;
        c.y = bottomY - 27.0f * sy;
    }
    Vector2 headPos = { c.x + dir * 2.0f * sx, c.y - 14.0f * sy };
    Vector2 shoulderPos = { c.x - dir * 6.0f * sx, c.y - 6.0f * sy };

    // 1. Плащ або крила дракона
    if (p.classType == ClassType::Dragon_Special) {
        float wingFlap = std::sin(p.animTime * 12.0f) * 14.0f * sy;
        Vector2 w1 = shoulderPos;
        Vector2 w2 = { shoulderPos.x - dir * 34.0f * sx, shoulderPos.y - 20.0f * sy + wingFlap };
        Vector2 w3 = { shoulderPos.x - dir * 24.0f * sx, shoulderPos.y + 12.0f * sy + wingFlap * 0.5f };
        DrawTriangle(w1, w2, w3, Fade(pal.bloodCloth[2], alpha));
        DrawTriangle(w3, w2, w1, Fade(pal.bloodCloth[1], alpha));
        DrawLineEx(w1, w2, 3.0f * sx, Fade(pal.fireGold[3], alpha));
        DrawCircleV(w2, 3.0f * sx, Fade(pal.outline, alpha)); // кіготь крила
    } else {
        Color capeOuter = GetTierSecondaryColor(p.capeTier, pal.bloodCloth[2]);
        Color capeInner = ColorTint(capeOuter, pal.metal[1]);
        float speedRatio = std::clamp(std::abs(p.velocity.x) / 290.0f, 0.0f, 1.4f);
        constexpr int CAPE_SEGMENTS = 7;
        Vector2 prevPt = shoulderPos;

        for (int i = 1; i <= CAPE_SEGMENTS; ++i) {
            float t = static_cast<float>(i) / CAPE_SEGMENTS;
            float wave = std::sin(p.animTime * (8.0f + speedRatio * 7.0f) - i * 0.85f) * (3.0f + speedRatio * 5.0f) * t;
            float cx = shoulderPos.x - dir * (t * (18.0f + speedRatio * 18.0f) * sx);
            float cy = shoulderPos.y + t * (25.0f + speedRatio * 8.0f) * sy - (p.velocity.y * 0.015f * t) + wave;
            Vector2 nextPt = { cx, cy };
            float thickness = (5.5f + t * 6.5f) * ((sx + sy) * 0.5f);
            DrawLineEx(prevPt, nextPt, thickness, Fade((i % 2 == 0) ? capeOuter : capeInner, alpha));
            prevPt = nextPt;
        }
    }

    // 2. Ноги з наколінниками та сабатонами (Sabaton boots)
    Color limbColor = GetTierPrimaryColor(p.limbsTier, pal.metal[2]);
    Color limbHighlight = ColorTint(limbColor, pal.metal[4]);
    float runCycle = (std::abs(p.velocity.x) > 15.0f) ? std::sin(p.animTime * 16.0f) * 8.0f * sx : 0.0f;
    float legW = 7.5f * sx;
    float legH = 14.5f * sy;

    auto DrawDetailedLeg = [&](float legX, float legY, float cycleOffset) {
        float curX = legX + cycleOffset;
        // Стегно
        DrawRectangleRec({ curX - legW * 0.5f, legY, legW, legH * 0.55f }, Fade(limbColor, alpha));
        // Наколінник (Poleyn)
        DrawCircle(static_cast<int>(curX), static_cast<int>(legY + legH * 0.5f), 4.2f * sx, Fade(limbHighlight, alpha));
        DrawCircle(static_cast<int>(curX), static_cast<int>(legY + legH * 0.5f), 1.6f * sx, Fade(pal.fireGold[3], alpha)); // золота заклепка
        // Поножі та сабатон (латний черевик)
        DrawRectangleRec({ curX - legW * 0.5f, legY + legH * 0.5f, legW, legH * 0.5f }, Fade(limbColor, alpha));
        DrawRectangleRec({ curX - legW * 0.5f + dir * 1.5f * sx, legY + legH - 3.5f * sy, legW + 3.0f * sx, 3.5f * sy }, Fade(pal.metal[1], alpha));
    };

    DrawDetailedLeg(c.x - 4.5f * sx, c.y + 12.5f * sy, runCycle);
    DrawDetailedLeg(c.x + 4.5f * sx, c.y + 12.5f * sy, -runCycle);

    // 3. Тулуб у готичній кірасі з поясом та настегнниками
    Color defaultTorso = pal.metal[2];
    if (p.classType == ClassType::Mage_Base || p.classType == ClassType::Archmage_Upgraded) {
        defaultTorso = pal.moonMagic[1];
    } else if (p.classType == ClassType::Shinobi_Base || p.classType == ClassType::Hokage_Upgraded) {
        defaultTorso = pal.metal[0];
    } else if (p.classType == ClassType::Lich_Base || p.classType == ClassType::LichKing_Upgraded) {
        defaultTorso = pal.dungeon.stoneTile[1];
    } else if (p.classType == ClassType::Miner_Base || p.classType == ClassType::Spelunker_Upgraded) {
        defaultTorso = pal.wood[2];
    }

    Color chestCol = GetTierPrimaryColor(p.chestTier, defaultTorso);
    Color trimCol  = GetTierSecondaryColor(p.chestTier, pal.fireGold[3]);

    Rectangle torsoRec = { c.x - bodyW * 0.5f, c.y - bodyH * 0.42f, bodyW, bodyH };
    DrawRectangleRounded(torsoRec, 0.22f, 4, Fade(chestCol, alpha));
    DrawRectangleRoundedLines(torsoRec, 0.22f, 4, std::max(1.5f, 2.0f * sx), Fade(trimCol, alpha));

    // Центральне ребро жорсткості кіраси
    DrawLineEx(
        { c.x + dir * 2.0f * sx, torsoRec.y + 3.0f * sy },
        { c.x + dir * 2.0f * sx, torsoRec.y + bodyH - 6.0f * sy },
        2.0f * sx, Fade(trimCol, alpha)
    );

    // Золотий комір (Горжет)
    DrawRectangleRec({ c.x - 7.0f * sx, torsoRec.y, 14.0f * sx, 4.0f * sy }, Fade(trimCol, alpha));

    // Шкіряний пояс із пряжкою
    float beltY = torsoRec.y + bodyH - 6.0f * sy;
    DrawRectangleRec({ torsoRec.x + 2.0f * sx, beltY, bodyW - 4.0f * sx, 5.0f * sy }, Fade(pal.wood[0], alpha));
    DrawRectangleRec({ c.x + dir * 1.0f * sx - 3.5f * sx, beltY, 7.0f * sx, 5.0f * sy }, Fade(pal.fireGold[3], alpha));

    // Наплічники (Pauldrons)
    auto DrawPauldron = [&](Vector2 pPos) {
        DrawCircle(static_cast<int>(pPos.x), static_cast<int>(pPos.y), 5.5f * sx, Fade(chestCol, alpha));
        DrawCircleLines(static_cast<int>(pPos.x), static_cast<int>(pPos.y), 5.5f * sx, Fade(trimCol, alpha));
        DrawCircle(static_cast<int>(pPos.x), static_cast<int>(pPos.y), 2.0f * sx, Fade(trimCol, alpha));
    };
    DrawPauldron({ c.x - dir * 10.0f * sx, torsoRec.y + 3.0f * sy });
    DrawPauldron({ c.x + dir * 9.0f * sx, torsoRec.y + 4.0f * sy });

    // 4. Голова і шолом
    Color helmCol  = GetTierPrimaryColor(p.helmTier, pal.metal[3]);
    Color plumeCol = GetTierSecondaryColor(p.helmTier, pal.bloodCloth[3]);

    // Купол шолома
    DrawCircleV(headPos, headR, Fade(helmCol, alpha));
    DrawCircleLines(static_cast<int>(headPos.x), static_cast<int>(headPos.y), headR, Fade(pal.outline, alpha));
    // Блик світла на куполі шолома
    DrawCircle(static_cast<int>(headPos.x - dir * 2.5f * sx), static_cast<int>(headPos.y - headR * 0.45f), headR * 0.4f, Fade(pal.metal[4], alpha * 0.35f));

    // Очна щілина шолома (Visor) з вогником очей лицаря
    Vector2 visorCenter = { headPos.x + dir * headR * 0.40f, headPos.y - headR * 0.05f };
    Rectangle visorRec = { visorCenter.x - 4.5f * sx, visorCenter.y - 2.5f * sy, 9.0f * sx, 5.0f * sy };
    DrawRectangleRec(visorRec, Fade(pal.outline, alpha));
    DrawRectangleLinesEx(visorRec, 1.2f * sx, Fade(pal.metal[2], alpha));

    // Світло очей крізь забрало
    if (p.classType == ClassType::Lich_Base || p.classType == ClassType::LichKing_Upgraded) {
        DrawCircleV({ visorCenter.x + dir * 1.0f * sx, visorCenter.y }, 2.5f * sx, Fade(pal.poisonSlime[3], alpha));
    } else {
        DrawCircleV({ visorCenter.x + dir * 1.0f * sx, visorCenter.y }, 2.0f * sx, Fade(pal.fireGold[3], alpha));
    }

    // Вентиляційні отвори на підборідді шолома (bevor slits)
    for (int vs = 0; vs < 3; ++vs) {
        DrawCircle(static_cast<int>(headPos.x + dir * (3.0f + vs * 2.5f) * sx), static_cast<int>(headPos.y + headR * 0.45f), 1.0f * sx, Fade(pal.outline, alpha * 0.7f));
    }

    // Класові головні убори та прикраси
    switch (p.classType) {
        case ClassType::Barbarian_Base:
        case ClassType::BarbarianKing_Upgraded: {
            // Величні роги вікінга
            DrawTriangle(
                { headPos.x - 4.0f * sx, headPos.y - headR * 0.6f },
                { headPos.x + 4.0f * sx, headPos.y - headR * 0.6f },
                { headPos.x + dir * 16.0f * sx, headPos.y - headR * 1.8f },
                Fade(pal.bone[4], alpha)
            );
            DrawTriangle(
                { headPos.x - 4.0f * sx, headPos.y - headR * 0.6f },
                { headPos.x - dir * 16.0f * sx, headPos.y - headR * 1.7f },
                { headPos.x + 2.0f * sx, headPos.y - headR * 0.6f },
                Fade(pal.bone[4], alpha)
            );
            // Хутряний комір
            DrawRectangleRounded({ torsoRec.x - 2.0f * sx, torsoRec.y, bodyW + 4.0f * sx, 8.0f * sy }, 0.4f, 4, Fade(pal.wood[1], alpha));
            break;
        }
        case ClassType::Mage_Base:
        case ClassType::Archmage_Upgraded: {
            // Гостроверхий чаклунський капелюх із крисами
            DrawEllipse(static_cast<int>(headPos.x), static_cast<int>(headPos.y - headR * 0.65f), headR * 1.35f, 4.0f * sy, Fade(pal.moonMagic[0], alpha));
            DrawTriangle(
                { headPos.x + headR * 1.15f, headPos.y - headR * 0.65f },
                { headPos.x - dir * 6.0f * sx, headPos.y - headR * 2.6f },
                { headPos.x - headR * 1.15f, headPos.y - headR * 0.65f },
                Fade(pal.moonMagic[1], alpha)
            );
            DrawCircleV({ headPos.x - dir * 6.0f * sx, headPos.y - headR * 2.6f }, 3.5f * sx, Fade(pal.fireGold[3], alpha));
            break;
        }
        case ClassType::Shinobi_Base:
        case ClassType::Hokage_Upgraded: {
            // Пов'язка ніндзя з металевою пластиною
            DrawRectangleRec({ headPos.x - headR, headPos.y - 4.5f * sy, headR * 2.0f, 6.0f * sy }, Fade(pal.bloodCloth[2], alpha));
            DrawRectangleRec({ headPos.x - 5.0f * sx, headPos.y - 4.5f * sy, 10.0f * sx, 5.0f * sy }, Fade(pal.metal[3], alpha));
            // Хвости пов'язки, що майорять на вітрі
            float tailWave = std::sin(p.animTime * 14.0f) * 4.0f * sy;
            DrawLineEx(
                { headPos.x - dir * headR, headPos.y - 3.0f * sy },
                { headPos.x - dir * (headR + 18.0f * sx), headPos.y - 5.0f * sy + tailWave },
                3.2f * sy, Fade(pal.bloodCloth[2], alpha)
            );
            break;
        }
        case ClassType::Miner_Base:
        case ClassType::Spelunker_Upgraded: {
            // Шахтарська каска з ліхтарем
            DrawCircleSector(headPos, headR * 1.15f, 180.0f, 360.0f, 12, Fade(pal.fireGold[3], alpha));
            Vector2 lampPos = { headPos.x + dir * headR * 0.88f, headPos.y - headR * 0.50f };
            DrawRectangleRec({ lampPos.x - 3.0f * sx, lampPos.y - 3.0f * sy, 6.0f * sx, 6.0f * sy }, Fade(pal.metal[1], alpha));
            DrawCircleV(lampPos, 4.5f * sx, p.headlampOn ? pal.fireGold[4] : pal.metal[2]);
            if (p.headlampOn) {
                // Конус променя шахтарського ліхтаря
                DrawCircleV(lampPos, 9.0f * sx, Fade(pal.fireGold[3], 0.40f));
                Vector2 beam1 = { lampPos.x + dir * 140.0f * sx, lampPos.y - 35.0f * sy };
                Vector2 beam2 = { lampPos.x + dir * 140.0f * sx, lampPos.y + 45.0f * sy };
                DrawTriangle(lampPos, beam2, beam1, Fade(pal.fireGold[3], 0.08f));
            }
            break;
        }
        case ClassType::Lich_Base:
        case ClassType::LichKing_Upgraded: {
            // Зубчаста корона ліча
            for (int k = -1; k <= 1; ++k) {
                float cxk = headPos.x + k * 5.5f * sx;
                DrawTriangle(
                    { cxk + 3.0f * sx, headPos.y - headR * 0.8f },
                    { cxk, headPos.y - headR * (1.75f + (k == 0 ? 0.35f : 0.0f)) },
                    { cxk - 3.0f * sx, headPos.y - headR * 0.8f },
                    Fade(pal.fireGold[3], alpha)
                );
            }
            break;
        }
        default: {
            // Пишний тришаровий плюмаж лицаря
            for (int pl = 0; pl < 3; ++pl) {
                float wave = std::sin(p.animTime * 10.0f + pl * 0.8f) * 3.0f * sy;
                Vector2 pTop = {
                    headPos.x - dir * (8.0f + pl * 4.5f) * sx,
                    headPos.y - headR * (1.55f + pl * 0.25f) + wave
                };
                DrawLineEx({ headPos.x, headPos.y - headR }, pTop, (5.0f - pl * 1.0f) * sx, Fade(plumeCol, alpha * (1.0f - pl * 0.15f)));
            }
            break;
        }
    }

    // 5. Блок щитом
    if (p.isBlocking) {
        DrawDetailedShield(
            { c.x + dir * 14.0f * sx, c.y - 4.0f * sy },
            dir, sx, sy, alpha, p.classType, p.chestTier
        );
    }

    // 6. Меч та атака
    float bladeLen = 48.0f * sx;
    Color bladeCol = GetTierPrimaryColor(p.swordTier, pal.metal[4]);

    if (p.isDownStriking) {
        // Pogo Downstrike: клинок спрямований вертикально вниз
        Vector2 pogoStart = { c.x, c.y + 12.0f * sy };
        Vector2 pogoTip   = { c.x, c.y + (12.0f + 48.0f) * sy };
        DrawDetailedSword(pogoStart, pogoTip, p.swordTier, sx, sy, alpha, false, 0.0f, true, p.animTime);
    } else if (p.isAttacking) {
        float swingAngleDeg = -85.0f + p.attackProgress * 170.0f;
        float rad = swingAngleDeg * (3.14159265f / 180.0f);
        Vector2 handPos = { c.x + dir * 8.0f * sx, c.y - 3.0f * sy };
        Vector2 tipPos = {
            handPos.x + dir * std::cos(rad) * bladeLen,
            handPos.y + std::sin(rad) * bladeLen
        };

        // Дуговий шлейф удару меча (Slash Arc Fan)
        float baseAngle = -85.0f;
        int arcSegments = 7;
        float stepA = (swingAngleDeg - baseAngle) / arcSegments;
        if (stepA > 1.0f) {
            for (int i = 0; i < arcSegments; ++i) {
                float a1 = (baseAngle + i * stepA) * (3.14159265f / 180.0f);
                float a2 = (baseAngle + (i + 1) * stepA) * (3.14159265f / 180.0f);
                float tFrac = static_cast<float>(i) / arcSegments;
                float arcAlpha = tFrac * 0.38f;

                Vector2 inner1 = { handPos.x + dir * std::cos(a1) * (bladeLen * 0.45f), handPos.y + std::sin(a1) * (bladeLen * 0.45f) };
                Vector2 inner2 = { handPos.x + dir * std::cos(a2) * (bladeLen * 0.45f), handPos.y + std::sin(a2) * (bladeLen * 0.45f) };
                Vector2 outer1 = { handPos.x + dir * std::cos(a1) * (bladeLen * 1.06f), handPos.y + std::sin(a1) * (bladeLen * 1.06f) };
                Vector2 outer2 = { handPos.x + dir * std::cos(a2) * (bladeLen * 1.06f), handPos.y + std::sin(a2) * (bladeLen * 1.06f) };

                Color arcColor = bladeCol;
                if (p.swordTier == EquipTier::Imperial) arcColor = pal.fireGold[2];
                else if (p.swordTier == EquipTier::Royal) arcColor = pal.fireGold[3];
                else if (p.swordTier == EquipTier::Dark) arcColor = pal.bloodCloth[3];

                DrawTriangle(inner1, outer1, outer2, Fade(arcColor, arcAlpha));
                DrawTriangle(inner1, outer2, inner2, Fade(arcColor, arcAlpha));
                DrawLineEx(outer1, outer2, 2.2f * sx, Fade(pal.metal[4], arcAlpha * 2.2f));
            }
        }

        DrawDetailedSword(handPos, tipPos, p.swordTier, sx, sy, alpha, true, p.attackProgress, false, p.animTime);
    } else {
        // Меч у спокої біля стегна
        Vector2 handPos = { c.x + dir * 10.0f * sx, c.y + 3.0f * sy };
        Vector2 tipPos  = { handPos.x + dir * 26.0f * sx, handPos.y - 18.0f * sy };
        DrawDetailedSword(handPos, tipPos, p.swordTier, sx, sy, alpha, false, 0.0f, false, p.animTime);
    }
}

void ProceduralRenderer::DrawHeroPreview(
    Vector2 centerPos,
    float uniformScale,
    const HeirCandidate& heir,
    const GameContext& ctx,
    float animTime
) const {
    HeroRenderParams hp{};
    hp.centerPos = centerPos;
    hp.velocity = { 80.0f, 0.0f };
    hp.scaleX = uniformScale;
    hp.scaleY = uniformScale;

    for (TraitType t : heir.traits) {
        if (t == TraitType::Dwarfism) {
            hp.scaleX *= 0.65f;
            hp.scaleY *= 0.65f;
        } else if (t == TraitType::Gigantism) {
            hp.scaleX *= 1.30f;
            hp.scaleY *= 1.30f;
        } else if (t == TraitType::Ectomorph) {
            hp.scaleX *= 0.68f;
        } else if (t == TraitType::Endomorph) {
            hp.scaleX *= 1.35f;
        }
    }

    hp.facingDir = 1.0f;
    hp.classType = heir.classType;
    hp.swordTier = static_cast<EquipTier>(ctx.equippedTier[static_cast<size_t>(EquipSlot::Sword)]);
    hp.helmTier  = static_cast<EquipTier>(ctx.equippedTier[static_cast<size_t>(EquipSlot::Helm)]);
    hp.chestTier = static_cast<EquipTier>(ctx.equippedTier[static_cast<size_t>(EquipSlot::Chest)]);
    hp.limbsTier = static_cast<EquipTier>(ctx.equippedTier[static_cast<size_t>(EquipSlot::Limbs)]);
    hp.capeTier  = static_cast<EquipTier>(ctx.equippedTier[static_cast<size_t>(EquipSlot::Cape)]);
    hp.headlampOn = (heir.classType == ClassType::Miner_Base || heir.classType == ClassType::Spelunker_Upgraded);
    hp.animTime = animTime;

    DrawHero(hp);
}

void ProceduralRenderer::DrawBiomeBackground(BiomeType biome, int roomGx, int roomGy) const {
    const auto& pal = GetPalette();
    const auto& bp = pal.GetBiome(biome);

    Color bgTop    = bp.stoneBg[1]; // Luminance ~8%
    Color bgBottom = bp.stoneBg[0]; // Luminance ~5%
    Color brickLine = bp.stoneBg[2]; // Luminance ~12%

    DrawRectangleGradientV(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, bgTop, bgBottom);

    for (int y = 0; y < Config::VIRTUAL_HEIGHT; y += 48) {
        DrawLine(0, y, Config::VIRTUAL_WIDTH, y, brickLine);
        int offset = ((y / 48) % 2) * 48;
        for (int x = offset; x < Config::VIRTUAL_WIDTH; x += 96) {
            DrawLine(x, y, x, y + 48, brickLine);
        }
    }

    int seed = (roomGx * 31 + roomGy * 17);
    for (int i = 0; i < 3; ++i) {
        int wx = 260 + i * 380;
        int wy = 210 + ((seed + i) % 2) * 60;
        DrawRectangle(wx - 36, wy, 72, 140, Fade(pal.outline, 0.45f));
        DrawCircle(wx, wy, 36.0f, Fade(pal.outline, 0.45f));
        if (biome == BiomeType::CastleHamson || biome == BiomeType::TheMaya) {
            DrawCircle(wx, wy, 28.0f, Fade(pal.moonMagic[2], 0.12f));
            DrawRectangle(wx - 28, wy, 56, 132, Fade(pal.moonMagic[2], 0.08f));
        } else if (biome == BiomeType::ForestAbkhazia) {
            DrawCircle(wx, wy, 28.0f, Fade(pal.poisonSlime[2], 0.12f));
        } else {
            DrawCircle(wx, wy, 28.0f, Fade(pal.bloodCloth[2], 0.12f));
        }
    }
}

void ProceduralRenderer::DrawTile(
    char tileCode,
    int col,
    int row,
    BiomeType biome,
    float trapTimer,
    float illusionAlpha,
    uint8_t neighborMask,
    uint8_t diagMask
) const {
    const auto& pal = GetPalette();
    const auto& bp = pal.GetBiome(biome);

    int x = Config::ROOM_OFFSET_X + col * Config::TILE_SIZE;
    int y = Config::ROOM_OFFSET_Y + row * Config::TILE_SIZE;
    int s = Config::TILE_SIZE;

    bool n = (neighborMask & 1) != 0;
    bool e = (neighborMask & 2) != 0;
    bool s_solid = (neighborMask & 4) != 0;
    bool w = (neighborMask & 8) != 0;

    bool nw = (diagMask & 1) != 0;
    bool ne = (diagMask & 2) != 0;
    bool sw = (diagMask & 4) != 0;
    bool se = (diagMask & 8) != 0;

    if (tileCode == '#' || tileCode == 'X' || tileCode == 'I') {
        // 1. Автотайлінг стіни за маскою 4-біт (16 варіацій)
        Color baseCol = (neighborMask == 0x0F) ? bp.stoneTile[1] : bp.stoneTile[2];
        Color borderCol = pal.outline;
        Color highlightCol = bp.stoneTile[4];
        Color shadowCol = bp.stoneTile[0];
        Color capCol = bp.stoneTile[3];

        if (tileCode == 'I') {
            baseCol = ColorAlpha(baseCol, illusionAlpha);
            borderCol = ColorAlpha(borderCol, illusionAlpha);
            highlightCol = ColorAlpha(highlightCol, illusionAlpha);
            shadowCol = ColorAlpha(shadowCol, illusionAlpha);
            capCol = ColorAlpha(capCol, illusionAlpha);
        }

        // Базовий блок каменю
        DrawRectangle(x, y, s, s, baseCol);

        // 4. Скісні зрізи (45°) на зовнішніх кутах (зрізаємо 4px кута для готичної фаски)
        const float cut = 4.0f;
        if (!n && !w) {
            DrawTriangle({ static_cast<float>(x), static_cast<float>(y) },
                         { static_cast<float>(x + cut), static_cast<float>(y) },
                         { static_cast<float>(x), static_cast<float>(y + cut) }, pal.outline);
            DrawLine(x, y + 4, x + 4, y, highlightCol);
        }
        if (!n && !e) {
            DrawTriangle({ static_cast<float>(x + s), static_cast<float>(y) },
                         { static_cast<float>(x + s), static_cast<float>(y + cut) },
                         { static_cast<float>(x + s - cut), static_cast<float>(y) }, pal.outline);
            DrawLine(x + s - 4, y, x + s, y + 4, highlightCol);
        }
        if (!s_solid && !w) {
            DrawTriangle({ static_cast<float>(x), static_cast<float>(y + s) },
                         { static_cast<float>(x), static_cast<float>(y + s - cut) },
                         { static_cast<float>(x + cut), static_cast<float>(y + s) }, pal.outline);
            DrawLine(x, y + s - 4, x + 4, y + s, shadowCol);
        }
        if (!s_solid && !e) {
            DrawTriangle({ static_cast<float>(x + s), static_cast<float>(y + s) },
                         { static_cast<float>(x + s - cut), static_cast<float>(y + s) },
                         { static_cast<float>(x + s), static_cast<float>(y + s - cut) }, pal.outline);
            DrawLine(x + s - 4, y + s, x + s, y + s - 4, shadowCol);
        }

        // Внутрішні кути (inner corners crevice details)
        if (n && w && !nw) {
            DrawPixel(x, y, borderCol);
            DrawPixel(x + 1, y, borderCol);
            DrawPixel(x, y + 1, borderCol);
        }
        if (n && e && !ne) {
            DrawPixel(x + s - 1, y, borderCol);
            DrawPixel(x + s - 2, y, borderCol);
            DrawPixel(x + s - 1, y + 1, borderCol);
        }
        if (s_solid && w && !sw) {
            DrawPixel(x, y + s - 1, borderCol);
            DrawPixel(x + 1, y + s - 1, borderCol);
            DrawPixel(x, y + s - 2, borderCol);
        }
        if (s_solid && e && !se) {
            DrawPixel(x + s - 1, y + s - 1, borderCol);
            DrawPixel(x + s - 2, y + s - 1, borderCol);
            DrawPixel(x + s - 1, y + s - 2, borderCol);
        }

        // 2. Детерміновані варіації кладки (5 патернів через TileHash)
        uint32_t th = TileHash(col, row);
        uint32_t pattern = th % 5;
        int halfS = s / 2;

        switch (pattern) {
            case 0: { // Класична перев'язка 2:1
                DrawLine(x, y + halfS, x + s - 1, y + halfS, borderCol);
                DrawLine(x, y + halfS + 1, x + s - 1, y + halfS + 1, bp.stoneTile[3]);
                int offTop = ((row % 2) == 0) ? (s / 2) : (s / 3);
                DrawLine(x + offTop, y, x + offTop, y + halfS, borderCol);
                DrawLine(x + offTop + 1, y, x + offTop + 1, y + halfS, bp.stoneTile[3]);
                int offBot = ((row % 2) == 0) ? (s / 4) : (3 * s / 4);
                DrawLine(x + offBot, y + halfS, x + offBot, y + s, borderCol);
                DrawLine(x + offBot + 1, y + halfS, x + offBot + 1, y + s, bp.stoneTile[3]);
                break;
            }
            case 1: { // Великий моноліт з бічними блоками
                int leftSplit = s / 3;
                int rightSplit = 2 * s / 3;
                DrawLine(x + leftSplit, y, x + leftSplit, y + s, borderCol);
                DrawLine(x + leftSplit + 1, y, x + leftSplit + 1, y + s, bp.stoneTile[3]);
                DrawLine(x + rightSplit, y, x + rightSplit, y + s, borderCol);
                DrawLine(x + rightSplit + 1, y, x + rightSplit + 1, y + s, bp.stoneTile[3]);
                DrawLine(x, y + halfS, x + leftSplit, y + halfS, borderCol);
                DrawLine(x + rightSplit, y + halfS, x + s - 1, y + halfS, borderCol);
                break;
            }
            case 2: { // Асиметрична 3-рядна кладка
                int y1 = y + 16;
                int y2 = y + 32;
                DrawLine(x, y1, x + s - 1, y1, borderCol);
                DrawLine(x, y2, x + s - 1, y2, borderCol);
                DrawLine(x + 14, y, x + 14, y1, borderCol);
                DrawLine(x + 30, y1, x + 30, y2, borderCol);
                DrawLine(x + 20, y2, x + 20, y + s, borderCol);
                break;
            }
            case 3: { // Рустовані кам'яні плити
                int cx = x + (s * 3) / 5;
                int cy = y + (s * 2) / 5;
                DrawLine(x, cy, x + s - 1, cy, borderCol);
                DrawLine(cx, y, cx, cy, borderCol);
                DrawLine(x + s / 3, cy, x + s / 3, y + s, borderCol);
                break;
            }
            case 4: { // Плити зі зміщеним швом
                DrawLine(x, y + halfS, x + s - 1, y + halfS, borderCol);
                int seam = x + 16 + ((th >> 4) % 16);
                DrawLine(seam, y, seam, y + halfS, borderCol);
                int seam2 = x + 10 + ((th >> 7) % 22);
                DrawLine(seam2, y + halfS, seam2, y + s, borderCol);
                break;
            }
        }

        // Детерміновані деталі поверхні:
        // 1 з 5 тайлів (20%): Тонка тріщина
        if ((th % 5) == 0) {
            int cx1 = 6 + ((th >> 2) % 18);
            int cy1 = 2 + ((th >> 4) % 10);
            int cx2 = cx1 + 5 + ((th >> 6) % 8);
            int cy2 = cy1 + 10 + ((th >> 8) % 12);
            int cx3 = cx2 - 3 + ((th >> 10) % 8);
            int cy3 = cy2 + 12 + ((th >> 12) % 10);
            DrawLine(x + cx1, y + cy1, x + cx2, y + cy2, borderCol);
            DrawLine(x + cx2, y + cy2, x + cx3, y + cy3, borderCol);
            DrawLine(x + cx2 + 1, y + cy2, x + cx3 + 1, y + cy3, bp.stoneTile[3]);
        }

        // 1 з 8 тайлів (12.5%): Вибита цеглина
        if ((th % 8) == 1) {
            int bx = x + 8 + ((th >> 3) % 22);
            int by = y + 8 + ((th >> 5) % 22);
            DrawRectangle(bx, by, 9, 6, shadowCol);
            DrawLine(bx, by + 6, bx + 9, by + 6, bp.stoneTile[3]);
        }

        // 1 з 10 тайлів (10%): Залізна кована скоба
        if ((th % 10) == 2) {
            int ix = x + 10 + ((th >> 4) % 20);
            int iy = y + halfS - 3;
            DrawRectangle(ix, iy, 12, 6, pal.metal[1]);
            DrawRectangle(ix + 1, iy + 1, 10, 4, pal.metal[3]);
            DrawCircle(ix + 3, iy + 3, 1.0f, pal.metal[4]);
            DrawCircle(ix + 9, iy + 3, 1.0f, pal.metal[4]);
        }

        // Верхня грань (Top cap / трав'яний чи кам'яний відблиск)
        if (!n) {
            DrawRectangle(x, y, s, 3, capCol);
            DrawLine(x, y, x + s - 1, y, highlightCol);
            DrawLine(x, y + 1, x + s - 1, y + 1, highlightCol);

            // Сад: звисаючий мох / плющ
            if (biome == BiomeType::ForestAbkhazia) {
                for (int i = 2; i < s - 2; i += 4) {
                    uint32_t mh = TileHash(col * 31 + i, row * 17);
                    int mossLen = 3 + (mh % 5);
                    DrawLine(x + i, y, x + i, y + mossLen, pal.poisonSlime[2]);
                    DrawPixel(x + i, y + mossLen, pal.poisonSlime[3]);
                }
            }
        }

        // Нижня грань: глибока тінь
        if (!s_solid) {
            DrawRectangle(x, y + s - 3, s, 3, shadowCol);
            DrawLine(x, y + s - 1, x + s - 1, y + s - 1, borderCol);
        }

        // Бокові грані: темний контур та кладка з торця
        if (!w) {
            DrawLine(x, y, x, y + s - 1, borderCol);
            DrawLine(x + 1, y, x + 1, y + s - 1, highlightCol);
        }
        if (!e) {
            DrawLine(x + s - 1, y, x + s - 1, y + s - 1, borderCol);
            DrawLine(x + s - 2, y, x + s - 2, y + s - 1, shadowCol);
        }

        // Особливість секретної стіни 'X': глибокі сяючі золоті розломи
        if (tileCode == 'X') {
            float pulse = 0.5f + 0.5f * std::sin(trapTimer * 4.0f + col * 0.7f + row * 1.1f);
            Color crackGlow = ColorAlpha(pal.fireGold[3], 0.45f + 0.35f * pulse);

            Vector2 p0 = { static_cast<float>(x + 8), static_cast<float>(y) };
            Vector2 p1 = { static_cast<float>(x + 18), static_cast<float>(y + 14) };
            Vector2 p2 = { static_cast<float>(x + 12), static_cast<float>(y + 26) };
            Vector2 p3 = { static_cast<float>(x + 28), static_cast<float>(y + 36) };
            Vector2 p4 = { static_cast<float>(x + 38), static_cast<float>(y + s) };

            DrawLineEx(p0, p1, 3.0f, borderCol);
            DrawLineEx(p1, p2, 3.0f, borderCol);
            DrawLineEx(p2, p3, 3.0f, borderCol);
            DrawLineEx(p3, p4, 3.0f, borderCol);

            DrawLineEx(p0, p1, 1.5f, crackGlow);
            DrawLineEx(p1, p2, 1.5f, crackGlow);
            DrawLineEx(p2, p3, 1.5f, crackGlow);
            DrawLineEx(p3, p4, 1.5f, crackGlow);

            DrawCircle(x + 14, y + 20, 1.5f, crackGlow);
            DrawCircle(x + 24, y + 32, 1.5f, crackGlow);
        }

        // Особливість ілюзорної стіни 'I': мерехтіння місячних рун
        if (tileCode == 'I' && illusionAlpha < 0.95f) {
            Color runeGlow = ColorAlpha(pal.moonMagic[3], (1.0f - illusionAlpha) * 0.6f);
            DrawPolyLines({ static_cast<float>(x + s / 2), static_cast<float>(y + s / 2) }, 6, 12.0f, trapTimer * 45.0f, runeGlow);
        }
    } else if (tileCode == '=') {
        // 3. Платформи ('='): тонка дерев'яна планка (6 px) з кованими залізними накладками по краях і текстурою
        int platH = 6;
        Color woodMain  = pal.wood[2];
        Color woodShade = pal.wood[0];
        Color woodHi    = pal.wood[3];
        Color ironCol   = pal.metal[2];
        Color ironHi    = pal.metal[4];

        // М'яка тінь під дошкою
        DrawRectangle(x, y + platH, s, 3, Fade(pal.outline, 0.40f));

        // Основна дерев'яна планка
        DrawRectangle(x, y, s, platH, woodMain);

        // Дерев'яна текстура: 2-3 лінії різного тону
        DrawLine(x, y, x + s - 1, y, woodHi);
        DrawLine(x, y + 2, x + s - 1, y + 2, woodShade);
        DrawLine(x, y + platH - 1, x + s - 1, y + platH - 1, pal.outline);

        // Ковані залізні накладки з заклепками по краях
        bool leftEnd = !w || (col % 4 == 0);
        bool rightEnd = !e || ((col + 1) % 4 == 0);

        if (leftEnd) {
            DrawRectangle(x + 1, y - 1, 6, platH + 2, ironCol);
            DrawLine(x + 1, y - 1, x + 1, y + platH, ironHi);
            DrawCircle(x + 4, y + 2, 1.0f, ironHi);
        }
        if (rightEnd) {
            DrawRectangle(x + s - 7, y - 1, 6, platH + 2, ironCol);
            DrawLine(x + s - 7, y - 1, x + s - 7, y + platH, ironHi);
            DrawCircle(x + s - 4, y + 2, 1.0f, ironHi);
        }
    } else if (tileCode == '^') {
        // 5. Шипи ('^'): нерівні зазубрені залізні піки різної висоти з краплями застиглої крові
        Color spikeDark  = pal.metal[1];
        Color spikeMid   = pal.metal[3];
        Color spikeLight = pal.metal[4];
        Color spikeBase  = pal.metal[0];

        // Залізна балка основи з кованими заклепками
        DrawRectangle(x, y + s - 6, s, 6, spikeBase);
        DrawLine(x, y + s - 6, x + s - 1, y + s - 6, spikeMid);
        for (int i = 0; i < 3; ++i) {
            DrawCircle(x + 4 + i * 18, y + s - 3, 1.5f, spikeLight);
        }

        // 3 зазубрені нерівні піки різної висоти
        for (int i = 0; i < 3; ++i) {
            uint32_t sh = TileHash(col * 19 + i, row * 23);
            float h = 33.0f + static_cast<float>(sh % 10);
            if (i == 1) h += 4.0f;

            float bx = static_cast<float>(x + i * 16 + 2);
            float by = static_cast<float>(y + s - 6);
            float apexX = bx + 6.0f;
            float apexY = by - h;

            // Грані піки
            DrawTriangle({ apexX, apexY }, { bx, by }, { apexX, by }, spikeMid);
            DrawTriangle({ bx + 12.0f, by }, { apexX, apexY }, { apexX, by }, spikeDark);
            DrawLineV({ apexX, apexY }, { apexX, by }, spikeLight);

            // Контур
            DrawLineV({ bx, by }, { apexX, apexY }, pal.outline);
            DrawLineV({ apexX, apexY }, { bx + 12.0f, by }, pal.outline);

            // Зубці та зазубрини на бокових гранях
            float notchY = by - h * 0.45f;
            DrawTriangle({ bx - 2.0f, notchY }, { bx + 2.0f, notchY - 3.0f }, { bx + 1.0f, notchY + 3.0f }, spikeMid);
            DrawTriangle({ bx + 14.0f, notchY + 4.0f }, { bx + 10.0f, notchY + 1.0f }, { bx + 11.0f, notchY + 7.0f }, spikeDark);

            // Застигла кров на вістрі та гранях
            DrawTriangle({ bx + 3.0f, apexY + 12.0f }, { apexX, apexY + 2.0f }, { bx + 9.0f, apexY + 12.0f }, pal.bloodCloth[1]);
            DrawTriangle({ bx + 4.5f, apexY + 7.0f }, { apexX, apexY }, { bx + 7.5f, apexY + 7.0f }, pal.bloodCloth[3]);
            DrawCircle(static_cast<int>(apexX + 2.0f), static_cast<int>(apexY + 14.0f), 1.5f, pal.bloodCloth[2]);
        }
    } else if (tileCode == 'v') {
        // 5. Стельові шипи ('v'): зазубрені залізні піки, що звисають зі стелі
        Color spikeDark  = pal.metal[1];
        Color spikeMid   = pal.metal[3];
        Color spikeLight = pal.metal[4];
        Color spikeBase  = pal.metal[0];

        DrawRectangle(x, y, s, 6, spikeBase);
        DrawLine(x, y + 5, x + s - 1, y + 5, spikeMid);
        for (int i = 0; i < 3; ++i) {
            DrawCircle(x + 4 + i * 18, y + 3, 1.5f, spikeLight);
        }

        for (int i = 0; i < 3; ++i) {
            uint32_t sh = TileHash(col * 19 + i, row * 23);
            float h = 33.0f + static_cast<float>(sh % 10);
            if (i == 1) h += 4.0f;

            float bx = static_cast<float>(x + i * 16 + 2);
            float ty = static_cast<float>(y + 6);
            float apexX = bx + 6.0f;
            float apexY = ty + h;

            DrawTriangle({ bx, ty }, { apexX, apexY }, { apexX, ty }, spikeMid);
            DrawTriangle({ apexX, apexY }, { bx + 12.0f, ty }, { apexX, ty }, spikeDark);
            DrawLineV({ apexX, ty }, { apexX, apexY }, spikeLight);

            DrawLineV({ bx, ty }, { apexX, apexY }, pal.outline);
            DrawLineV({ apexX, apexY }, { bx + 12.0f, ty }, pal.outline);

            float notchY = ty + h * 0.45f;
            DrawTriangle({ bx - 2.0f, notchY }, { bx + 2.0f, notchY + 3.0f }, { bx + 1.0f, notchY - 3.0f }, spikeMid);
            DrawTriangle({ bx + 14.0f, notchY - 4.0f }, { bx + 10.0f, notchY - 1.0f }, { bx + 11.0f, notchY - 7.0f }, spikeDark);

            DrawTriangle({ bx + 3.0f, apexY - 12.0f }, { apexX, apexY - 2.0f }, { bx + 9.0f, apexY - 12.0f }, pal.bloodCloth[1]);
            DrawTriangle({ bx + 4.5f, apexY - 7.0f }, { apexX, apexY }, { bx + 7.5f, apexY - 7.0f }, pal.bloodCloth[3]);
            DrawCircle(static_cast<int>(apexX - 2.0f), static_cast<int>(apexY - 14.0f), 1.5f, pal.bloodCloth[2]);
        }
    } else if (tileCode == '~') {
        // Retractable mechanical spike plate
        Color plateCol   = pal.metal[2];
        Color plateEdge  = pal.metal[3];
        Color slotCol    = pal.outline;

        DrawRectangle(x, y + s - 8, s, 8, plateCol);
        DrawLine(x, y + s - 8, x + s, y + s - 8, plateEdge);

        // Trap slot openings
        for (int i = 0; i < 3; ++i) {
            DrawRectangle(x + 4 + i * 16, y + s - 7, 8, 4, slotCol);
        }

        float ext = std::clamp(std::sin(trapTimer * 3.5f) * 1.4f, 0.0f, 1.0f);
        if (ext > 0.08f) {
            float h = 30.0f * ext;
            Color tipGlow = (ext > 0.8f) ? pal.danger[3] : pal.danger[2];

            for (int i = 0; i < 3; ++i) {
                float bx = static_cast<float>(x + i * 16);
                float by = static_cast<float>(y + s - 7);
                float apexX = bx + 8.0f;
                float apexY = by - h;

                DrawTriangle({ bx + 13.0f, by }, { apexX, apexY }, { apexX, by }, pal.metal[2]);
                DrawTriangle({ apexX, by }, { apexX, apexY }, { bx + 3.0f, by }, pal.metal[3]);
                DrawLineV({ apexX, apexY }, { apexX, by }, pal.metal[4]);
                DrawTriangle({ bx + 6.0f, by - h * 0.7f }, { apexX, apexY }, { bx + 10.0f, by - h * 0.7f }, tipGlow);
            }
        }
    } else if (tileCode == 'D') {
        // Деталізований гном'ячий тунель-арка (Dwarf Tunnel Crawlspace)
        Color archStone   = bp.stoneTile[2];
        Color archTrim    = bp.stoneTile[3];
        Color innerDark   = pal.outline;
        Color bronzeGlow  = pal.fireGold[3];

        // Кам'яна кладка обрамлення
        DrawRectangle(x, y, s, s, archStone);
        DrawRectangleLines(x, y, s, s, archTrim);

        // Темний заокруглений отвір проходу
        DrawRectangle(x + 6, y + 14, s - 12, s - 14, innerDark);
        DrawCircle(x + s / 2, y + 16, static_cast<float>(s / 2 - 6), innerDark);

        // Окантовка замкового каменя склепіння
        DrawCircleLines(x + s / 2, y + 16, static_cast<float>(s / 2 - 6), archTrim);
        DrawLine(x + 6, y + 16, x + 6, y + s - 1, archTrim);
        DrawLine(x + s - 7, y + 16, x + s - 7, y + s - 1, archTrim);

        // М'яке бурштинове сяйво з глибини тунелю
        DrawCircleGradient(x + s / 2, y + s / 2 + 4, 15.0f, Fade(pal.fireGold[3], 0.28f), BLANK);

        // Рунічний гліф шолома Карлика над склепінням
        Vector2 glyphPos = { static_cast<float>(x + s / 2), static_cast<float>(y + 9) };
        DrawCircleV(glyphPos, 3.5f, bronzeGlow);
        DrawTriangle({ glyphPos.x - 7.0f, glyphPos.y + 1.0f },
                     { glyphPos.x - 2.0f, glyphPos.y - 2.0f },
                     { glyphPos.x - 2.0f, glyphPos.y + 3.0f }, bronzeGlow);
        DrawTriangle({ glyphPos.x + 7.0f, glyphPos.y + 1.0f },
                     { glyphPos.x + 2.0f, glyphPos.y + 3.0f },
                     { glyphPos.x + 2.0f, glyphPos.y - 2.0f }, bronzeGlow);

        // Бронзовий поріг
        DrawRectangle(x + 4, y + s - 3, s - 8, 3, bronzeGlow);
    }
}

void ProceduralRenderer::DrawGoldenBossDoor(Vector2 bottomCenter, const std::array<bool, 5>& bossDefeated, float time) const {
    const auto& pal = GetPalette();
    float doorW = 124.0f;
    float doorH = 176.0f;
    float left = bottomCenter.x - doorW * 0.5f;
    float top  = bottomCenter.y - doorH;

    bool allFourSlain = bossDefeated[0] && bossDefeated[1] && bossDefeated[2] && bossDefeated[3];

    // Heavy gothic stone archway surround
    DrawRectangleRec({ left - 14.0f, top - 18.0f, doorW + 28.0f, doorH + 18.0f }, pal.dungeon.stoneTile[1]);
    DrawRectangleLinesEx({ left - 14.0f, top - 18.0f, doorW + 28.0f, doorH + 18.0f }, 3.0f, pal.dungeon.stoneTile[3]);

    // Keystone arch peak
    DrawTriangle(
        { bottomCenter.x - 22.0f, top - 18.0f },
        { bottomCenter.x, top - 32.0f },
        { bottomCenter.x + 22.0f, top - 18.0f },
        pal.dungeon.stoneTile[2]
    );
    DrawTriangleLines(
        { bottomCenter.x - 22.0f, top - 18.0f },
        { bottomCenter.x, top - 32.0f },
        { bottomCenter.x + 22.0f, top - 18.0f },
        pal.fireGold[3]
    );

    // Inner door frame
    Color doorFill = allFourSlain ? pal.outline : pal.fireGold[1];
    DrawRectangleRec({ left, top, doorW, doorH }, doorFill);
    DrawRectangleLinesEx({ left, top, doorW, doorH }, 6.0f, pal.fireGold[3]);

    // Ornate dual door panels
    float halfW = doorW * 0.5f;
    DrawRectangleRec({ left + 6.0f, top + 6.0f, halfW - 8.0f, doorH - 12.0f }, pal.fireGold[0]);
    DrawRectangleRec({ left + halfW + 2.0f, top + 6.0f, halfW - 8.0f, doorH - 12.0f }, pal.fireGold[0]);

    if (!allFourSlain) {
        // Massive central gold seam & heavy iron sealing bar
        DrawLineEx({ bottomCenter.x, top }, { bottomCenter.x, bottomCenter.y }, 4.0f, pal.fireGold[0]);
        // Heavy locking crossbars
        DrawRectangleRec({ left + 16.0f, bottomCenter.y - 100.0f, doorW - 32.0f, 16.0f }, pal.wood[1]);
        DrawRectangleLinesEx({ left + 16.0f, bottomCenter.y - 100.0f, doorW - 32.0f, 16.0f }, 2.0f, pal.fireGold[3]);

        // Lion-head knocker / handle rings
        DrawCircle(static_cast<int>(bottomCenter.x - 18.0f), static_cast<int>(bottomCenter.y - 92.0f), 7.0f, pal.fireGold[3]);
        DrawCircle(static_cast<int>(bottomCenter.x - 18.0f), static_cast<int>(bottomCenter.y - 92.0f), 4.0f, pal.wood[1]);
        DrawCircle(static_cast<int>(bottomCenter.x + 18.0f), static_cast<int>(bottomCenter.y - 92.0f), 7.0f, pal.fireGold[3]);
        DrawCircle(static_cast<int>(bottomCenter.x + 18.0f), static_cast<int>(bottomCenter.y - 92.0f), 4.0f, pal.wood[1]);
    } else {
        // Void portal energy vortex swirling when unlocked
        float pulse = 0.8f + 0.2f * std::sin(time * 5.0f);
        DrawCircleGradient(
            static_cast<int>(bottomCenter.x),
            static_cast<int>(bottomCenter.y - doorH * 0.5f),
            56.0f * pulse,
            pal.moonMagic[3],
            pal.outline
        );
        // Golden celestial rays leaking from portal
        for (int r = 0; r < 8; ++r) {
            float ang = time * 2.0f + r * (2.0f * 3.14159f / 8.0f);
            Vector2 rayEnd = {
                bottomCenter.x + std::cos(ang) * 52.0f,
                (bottomCenter.y - doorH * 0.5f) + std::sin(ang) * 52.0f
            };
            DrawLineEx({ bottomCenter.x, bottomCenter.y - doorH * 0.5f }, rayEnd, 2.0f, Fade(pal.fireGold[3], 0.45f));
        }
    }

    // 4 Boss Seal Gem Medallions:
    // 0: Khidr (SkyBlue Eye), 1: Alexander (Red Skull), 2: Ponce (Orange Fire), 3: Herodotus (Lime Slime)
    const std::array<Vector2, 4> sealOffsets = {{
        { -30.0f, -126.0f },
        {  30.0f, -126.0f },
        { -30.0f,  -58.0f },
        {  30.0f,  -58.0f }
    }};
    const std::array<Color, 4> sealColors = {{
        pal.moonMagic[3], pal.bloodCloth[3], pal.fireGold[3], pal.poisonSlime[3]
    }};

    for (size_t i = 0; i < 4; ++i) {
        Vector2 sp = { bottomCenter.x + sealOffsets[i].x, bottomCenter.y + sealOffsets[i].y };
        // Outer filigree medallion ring
        DrawCircleV(sp, 18.0f, pal.dungeon.stoneBg[1]);
        DrawCircleLines(static_cast<int>(sp.x), static_cast<int>(sp.y), 18.0f, pal.fireGold[3]);
        DrawCircleLines(static_cast<int>(sp.x), static_cast<int>(sp.y), 15.0f, pal.fireGold[1]);

        if (bossDefeated[i]) {
            // Radiant boss soul gem
            float gemPulse = 1.0f + 0.15f * std::sin(time * 4.0f + i * 1.57f);
            DrawCircleGradient(static_cast<int>(sp.x), static_cast<int>(sp.y), 13.0f * gemPulse, sealColors[i], Fade(sealColors[i], 0.2f));
            DrawCircleV(sp, 9.0f, sealColors[i]);
            DrawCircleV(sp, 4.0f, pal.metal[4]); // Gem facet glint
        } else {
            // Dormant empty socket with dark runic core
            DrawCircleV(sp, 10.0f, pal.outline);
            DrawCircleV(sp, 5.0f, pal.dungeon.stoneTile[1]);
        }
    }
}

void ProceduralRenderer::DrawManorCastle(const GameContext& ctx, float time) const {
    // 1. Яскраве денне лазурне небо (Daytime azure gradient sky)
    DrawRectangleGradientV(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Color{ 58, 136, 235, 255 }, Color{ 185, 225, 252, 255 });

    // 2. Сяюче тепле сонце з ореолом та променями (Glowing golden sun)
    Vector2 sunPos = { 960.0f, 115.0f };
    DrawCircleGradient(static_cast<int>(sunPos.x), static_cast<int>(sunPos.y), 130.0f, Fade(Color{ 255, 245, 170, 255 }, 0.35f), BLANK);
    DrawCircleGradient(static_cast<int>(sunPos.x), static_cast<int>(sunPos.y), 75.0f, Fade(Color{ 255, 225, 110, 255 }, 0.55f), BLANK);
    DrawCircleV(sunPos, 38.0f, Color{ 255, 252, 215, 255 });
    DrawCircleV(sunPos, 26.0f, WHITE);

    // М'які сонячні промені
    for (int r = 0; r < 8; ++r) {
        float rayAng = time * 0.15f + r * (3.14159f / 4.0f);
        Vector2 rEnd = { sunPos.x + std::cos(rayAng) * 95.0f, sunPos.y + std::sin(rayAng) * 95.0f };
        DrawLineEx(sunPos, rEnd, 2.5f, Fade(Color{ 255, 250, 190, 255 }, 0.22f));
    }

    // 3. Пишні білосніжні хмари, що пливуть небокраєм (Cumulus clouds)
    for (int i = 0; i < 5; ++i) {
        float speed = 8.0f + i * 5.0f;
        float cx = std::fmod(time * speed + i * 290.0f, 1500.0f) - 120.0f;
        float cy = 60.0f + i * 36.0f;
        float cscale = 0.85f + (i % 3) * 0.25f;

        Color cloudShadow = Color{ 220, 232, 248, 220 };
        Color cloudTop    = Color{ 255, 255, 255, 245 };

        // Тіньова основа хмари
        DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 10.0f * cscale), 80.0f * cscale, 24.0f * cscale, cloudShadow);
        // Об'ємні білі куполи
        DrawCircle(static_cast<int>(cx - 38.0f * cscale), static_cast<int>(cy + 2.0f), 28.0f * cscale, cloudTop);
        DrawCircle(static_cast<int>(cx + 36.0f * cscale), static_cast<int>(cy + 4.0f), 26.0f * cscale, cloudTop);
        DrawCircle(static_cast<int>(cx), static_cast<int>(cy - 12.0f * cscale), 36.0f * cscale, cloudTop);
        DrawCircle(static_cast<int>(cx + 18.0f * cscale), static_cast<int>(cy - 6.0f * cscale), 30.0f * cscale, cloudTop);
    }

    // 4. Величний далекий гірський хребет з білими засніженими піками
    Color farMtnCol  = Color{ 120, 145, 185, 255 };
    Color snowCol    = Color{ 245, 250, 255, 255 };

    // Далекі піки
    struct MountainPeak { float x; float y; float w; };
    const MountainPeak peaks[4] = {
        { 160.0f, 260.0f, 280.0f },
        { 480.0f, 290.0f, 320.0f },
        { 820.0f, 250.0f, 340.0f },
        { 1160.0f, 275.0f, 300.0f }
    };
    for (const auto& pk : peaks) {
        DrawTriangle({ pk.x - pk.w * 0.5f, 560.0f }, { pk.x, pk.y }, { pk.x + pk.w * 0.5f, 560.0f }, farMtnCol);
        // Снігова шапка на вершині
        float snowH = (560.0f - pk.y) * 0.32f;
        DrawTriangle(
            { pk.x - pk.w * 0.16f, pk.y + snowH },
            { pk.x, pk.y },
            { pk.x + pk.w * 0.16f, pk.y + snowH },
            snowCol
        );
    }

    // Середній хребет лісистих пагорбів
    Color midHillsCol = Color{ 58, 118, 78, 255 };
    DrawTriangle({ -80.0f, 560.0f }, { 220.0f, 370.0f }, { 520.0f, 560.0f }, midHillsCol);
    DrawTriangle({ 320.0f, 560.0f }, { 680.0f, 390.0f }, { 1040.0f, 560.0f }, midHillsCol);
    DrawTriangle({ 780.0f, 560.0f }, { 1100.0f, 360.0f }, { 1400.0f, 560.0f }, midHillsCol);

    // 5. Зелені смарагдові пагорби переднього плану (Lush rolling green hills)
    DrawCircle(180, 810, 350.0f, Color{ 68, 155, 62, 255 });
    DrawCircle(1100, 820, 360.0f, Color{ 68, 155, 62, 255 });
    DrawCircle(640, 1020, 520.0f, Color{ 82, 175, 74, 255 });

    // Окремі мальовничі сосни та смереки на схилах пагорбів
    auto DrawHillPine = [](float x, float y, float h, float w, Color col) {
        DrawRectangle(static_cast<int>(x - 3.0f), static_cast<int>(y - 12.0f), 6, 12, Color{ 85, 55, 38, 255 });
        DrawTriangle({ x - w * 0.5f, y - 8.0f }, { x, y - h * 0.5f }, { x + w * 0.5f, y - 8.0f }, col);
        DrawTriangle({ x - w * 0.42f, y - h * 0.38f }, { x, y - h * 0.78f }, { x + w * 0.42f, y - h * 0.38f }, col);
        DrawTriangle({ x - w * 0.32f, y - h * 0.65f }, { x, y - h }, { x + w * 0.32f, y - h * 0.65f }, ColorTint(col, Color{ 220, 245, 220, 255 }));
    };

    DrawHillPine(80.0f, 520.0f, 85.0f, 44.0f, Color{ 32, 95, 48, 255 });
    DrawHillPine(140.0f, 535.0f, 105.0f, 52.0f, Color{ 28, 85, 42, 255 });
    DrawHillPine(210.0f, 545.0f, 75.0f, 38.0f, Color{ 35, 105, 52, 255 });
    DrawHillPine(1060.0f, 540.0f, 95.0f, 48.0f, Color{ 28, 85, 42, 255 });
    DrawHillPine(1140.0f, 525.0f, 115.0f, 56.0f, Color{ 32, 95, 48, 255 });
    DrawHillPine(1210.0f, 535.0f, 80.0f, 40.0f, Color{ 35, 105, 52, 255 });

    // Квіти та травинки на лузі
    for (int fx = 40; fx < Config::VIRTUAL_WIDTH; fx += 55) {
        float fy = 540.0f + std::sin(fx * 0.08f) * 12.0f;
        DrawCircle(fx, static_cast<int>(fy), 2.5f, (fx % 110 == 0) ? GOLD : (fx % 165 == 0 ? WHITE : Color{ 255, 120, 160, 255 }));
    }

    // 6. Величний Родовий Замок (Ancestral Manor Keep) у денному освітленні
    int baseY = 550;
    Color stoneLight = Color{ 192, 190, 185, 255 }; // Теплий вапняк під сонцем
    Color stoneDark  = Color{ 148, 145, 142, 255 }; // Затінена сторона кладки
    Color stoneMrt   = Color{ 120, 118, 115, 255 }; // Шви кам'яної кладки
    Color roofCol    = Color{ 188, 64, 48, 255 };   // Сонячна теракотова черепиця
    Color roofDark   = Color{ 145, 45, 34, 255 };
    Color winGlass   = Color{ 120, 210, 255, 230 }; // Небесно-блакитне скло з відблисками
    Color winFrame   = Color{ 68, 55, 45, 255 };

    int keepLvl = ctx.GetManorLevel(ManorNodeId::Stat_Health) + ctx.GetManorLevel(ManorNodeId::Stat_Mana);
    int keepH = 165 + std::min(160, keepLvl * 6);

    // Головна Цитадель (Central Keep)
    DrawRectangle(560, baseY - keepH, 160, keepH, stoneLight);
    DrawRectangle(640, baseY - keepH, 80, keepH, stoneDark); // легка тінь праворуч
    DrawRectangleLines(560, baseY - keepH, 160, keepH, stoneMrt);

    // Горизонтальні шви кам'яних блоків
    for (int y = baseY - keepH + 20; y < baseY; y += 22) {
        DrawLine(560, y, 720, y, stoneMrt);
    }

    // Зубці вежі цитаделі (Battlements)
    for (int bx = 560; bx < 720; bx += 20) {
        DrawRectangle(bx, baseY - keepH - 12, 14, 12, stoneLight);
        DrawRectangleLines(bx, baseY - keepH - 12, 14, 12, stoneMrt);
    }

    // Головний теракотовий дах
    DrawTriangle(
        { 735.0f, static_cast<float>(baseY - keepH) },
        { 640.0f, static_cast<float>(baseY - keepH - 85) },
        { 545.0f, static_cast<float>(baseY - keepH) },
        roofCol
    );
    DrawTriangle(
        { 735.0f, static_cast<float>(baseY - keepH) },
        { 640.0f, static_cast<float>(baseY - keepH - 85) },
        { 640.0f, static_cast<float>(baseY - keepH) },
        roofDark
    );

    // Золотий шпиль та червоний прапор, що майорить на вітрі
    Vector2 spireTop = { 640.0f, static_cast<float>(baseY - keepH - 110) };
    DrawLineEx({ 640.0f, static_cast<float>(baseY - keepH - 85) }, spireTop, 3.0f, GOLD);
    float flagWave = std::sin(time * 6.5f) * 6.0f;
    DrawTriangle(
        spireTop,
        { spireTop.x + 28.0f + flagWave, spireTop.y + 6.0f },
        { spireTop.x, spireTop.y + 16.0f },
        Color{ 220, 38, 48, 255 }
    );

    // Арочні вікна цитаделі з перехресними палітурками
    for (int wy = baseY - 50; wy > baseY - keepH + 30; wy -= 44) {
        for (int wx : { 600, 665 }) {
            DrawRectangle(wx, wy, 16, 26, winGlass);
            DrawCircle(wx + 8, wy, 8.0f, winGlass);
            DrawRectangleLines(wx, wy, 16, 26, winFrame);
            DrawLine(wx + 8, wy - 8, wx + 8, wy + 26, winFrame);
            DrawLine(wx, wy + 10, wx + 16, wy + 10, winFrame);
        }
    }

    // Ліве військове крило (Left Wing - Attack / Armor / Barbarian / Shinobi)
    int leftLvl = ctx.GetManorLevel(ManorNodeId::Stat_Attack)
                + ctx.GetManorLevel(ManorNodeId::Stat_Armor)
                + ctx.GetManorLevel(ManorNodeId::Class_Barbarian) * 5
                + ctx.GetManorLevel(ManorNodeId::Class_Shinobi) * 5;
    if (leftLvl > 0) {
        int lH = 110 + std::min(190, leftLvl * 7);
        DrawRectangle(455, baseY - lH, 105, lH, stoneDark);
        DrawRectangleLines(455, baseY - lH, 105, lH, stoneMrt);
        for (int bx = 455; bx < 560; bx += 22) {
            DrawRectangle(bx, baseY - lH - 14, 14, 14, stoneLight);
            DrawRectangleLines(bx, baseY - lH - 14, 14, 14, stoneMrt);
        }
        for (int wy = baseY - 55; wy > baseY - lH + 20; wy -= 45) {
            DrawRectangle(498, wy, 16, 24, winGlass);
            DrawRectangleLines(498, wy, 16, 24, winFrame);
        }
    }

    // Праве магічне крило (Right Wing - Magic / Mage / Lich)
    int rightLvl = ctx.GetManorLevel(ManorNodeId::Stat_MagicDmg)
                 + ctx.GetManorLevel(ManorNodeId::Class_Mage) * 5
                 + ctx.GetManorLevel(ManorNodeId::Class_Lich) * 5;
    if (rightLvl > 0) {
        int rH = 120 + std::min(200, rightLvl * 7);
        DrawRectangle(720, baseY - rH, 100, rH, stoneDark);
        DrawRectangleLines(720, baseY - rH, 100, rH, stoneMrt);
        DrawTriangle(
            { 828.0f, static_cast<float>(baseY - rH) },
            { 770.0f, static_cast<float>(baseY - rH - 90) },
            { 712.0f, static_cast<float>(baseY - rH) },
            Color{ 85, 48, 145, 255 }
        );
        // Магічний кришталь на верхівці
        float gemPulse = 1.0f + 0.2f * std::sin(time * 5.0f);
        DrawCircleV({ 770.0f, static_cast<float>(baseY - rH - 96) }, 7.0f * gemPulse, SKYBLUE);
        DrawCircleV({ 770.0f, static_cast<float>(baseY - rH - 96) }, 3.5f, WHITE);

        for (int wy = baseY - 55; wy > baseY - rH + 20; wy -= 45) {
            DrawRectangle(762, wy, 16, 24, Color{ 140, 220, 255, 255 });
            DrawRectangleLines(762, wy, 16, 24, winFrame);
        }
    }

    // Майстерня Коваля (Blacksmith Forge)
    if (ctx.GetManorLevel(ManorNodeId::Unlock_Blacksmith) > 0) {
        DrawRectangle(345, baseY - 95, 110, 95, Color{ 110, 85, 75, 255 });
        DrawRectangleLines(345, baseY - 95, 110, 95, Color{ 75, 55, 48, 255 });
        DrawRectangle(365, baseY - 138, 28, 43, Color{ 85, 68, 62, 255 });
        // Дим з димаря кузні
        for (int p = 0; p < 3; ++p) {
            float py = (baseY - 145) - std::fmod(time * 25.0f + p * 18.0f, 50.0f);
            float px = 379.0f + std::sin(time * 3.0f + p) * 10.0f;
            DrawCircle(static_cast<int>(px), static_cast<int>(py), 7.0f + p * 3.0f, Fade(Color{ 180, 190, 205, 255 }, 0.45f));
        }
        // Вогняний відблиск горна
        float flamePulse = 14.0f + std::sin(time * 12.0f) * 4.0f;
        DrawCircle(400, baseY - 28, flamePulse, ORANGE);
        DrawCircle(400, baseY - 28, flamePulse * 0.6f, YELLOW);
    }

    // Намет Чарівниці (Enchantress Tent)
    if (ctx.GetManorLevel(ManorNodeId::Unlock_Enchantress) > 0) {
        DrawTriangle(
            { 928.0f, static_cast<float>(baseY) },
            { 870.0f, static_cast<float>(baseY - 100) },
            { 812.0f, static_cast<float>(baseY) },
            Color{ 145, 42, 165, 255 }
        );
        DrawCircle(870, baseY - 48, 13.0f + std::sin(time * 5.0f) * 2.5f, MAGENTA);
        DrawCircle(870, baseY - 48, 6.0f, WHITE);
    }

    // Ротонда Архітектора (Architect Dome)
    if (ctx.GetManorLevel(ManorNodeId::Unlock_Architect) > 0) {
        DrawRectangle(930, baseY - 115, 78, 115, stoneLight);
        DrawRectangleLines(930, baseY - 115, 78, 115, stoneMrt);
        DrawCircle(969, baseY - 115, 39.0f, Color{ 85, 145, 168, 255 });
        DrawCircleLines(969, baseY - 115, 39.0f, GOLD);
    }

    // Обсерваторія Зірок (Master Observatory)
    if (ctx.GetManorLevel(ManorNodeId::Upgrade_AllBasicClasses) > 0) {
        int topY = baseY - keepH - 85;
        DrawCircle(640, topY, 44.0f, Color{ 215, 185, 85, 255 });
        DrawCircleLines(640, topY, 44.0f, GOLD);
        DrawLineEx(
            { 640.0f, static_cast<float>(topY - 10) },
            { 688.0f, static_cast<float>(topY - 50) },
            8.0f, GOLD
        );
        DrawCircle(692, topY - 54, 7.0f, SKYBLUE);
    }

    // Гніздо Дракона (Dragon Aerie)
    if (ctx.GetManorLevel(ManorNodeId::Class_Dragon) > 0) {
        int topY = baseY - keepH - 125;
        DrawCircle(640, topY, 20.0f, RED);
        DrawTriangle(
            { 640.0f, static_cast<float>(topY) },
            { 592.0f, static_cast<float>(topY - 34) },
            { 615.0f, static_cast<float>(topY + 10) },
            ORANGE
        );
        DrawTriangle(
            { 640.0f, static_cast<float>(topY) },
            { 665.0f, static_cast<float>(topY + 10) },
            { 688.0f, static_cast<float>(topY - 34) },
            ORANGE
        );
    }
}

// ============================================================================
// UI & Procedural Icons Implementation
// ============================================================================

void ProceduralRenderer::DrawTraitIcon(Vector2 c, TraitType trait, float size, Color col) const {
    switch (trait) {
        case TraitType::Colorblind: {
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.8f, col);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.35f, col);
            DrawLineEx({ c.x - size * 0.85f, c.y - size * 0.85f }, { c.x + size * 0.85f, c.y + size * 0.85f }, 2.0f, col);
            break;
        }
        case TraitType::Nostalgic: {
            // Antique Hourglass
            float hw = size * 0.75f;
            float hh = size * 0.95f;
            DrawLineEx({ c.x - hw, c.y - hh }, { c.x + hw, c.y - hh }, 2.0f, col);
            DrawLineEx({ c.x - hw, c.y + hh }, { c.x + hw, c.y + hh }, 2.0f, col);
            DrawTriangle({ c.x + hw, c.y - hh }, { c.x, c.y }, { c.x - hw, c.y - hh }, Fade(col, 0.45f));
            DrawTriangle({ c.x - hw, c.y + hh }, { c.x, c.y }, { c.x + hw, c.y + hh }, Fade(col, 0.45f));
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y + hh * 0.5f), 2.0f, col);
            break;
        }
        case TraitType::Vertigo: {
            // Spiral / inverted loop arrow
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.7f, col);
            DrawTriangle({ c.x + size * 0.7f, c.y - 3.0f }, { c.x + size * 0.7f - 4.0f, c.y + 4.0f }, { c.x + size * 0.7f + 4.0f, c.y + 4.0f }, col);
            DrawTriangle({ c.x - size * 0.7f, c.y + 3.0f }, { c.x - size * 0.7f + 4.0f, c.y - 4.0f }, { c.x - size * 0.7f - 4.0f, c.y - 4.0f }, col);
            break;
        }
        case TraitType::Myopia: {
            // Concave lens / eye glass
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.8f, col);
            DrawLineEx({ c.x + size * 0.6f, c.y + size * 0.6f }, { c.x + size * 1.1f, c.y + size * 1.1f }, 2.5f, col);
            DrawCircle(static_cast<int>(c.x - size * 0.2f), static_cast<int>(c.y - size * 0.2f), 2.0f, WHITE);
            break;
        }
        case TraitType::Hyperopia: {
            // Brass telescope
            DrawRectanglePro({ c.x - size * 0.4f, c.y, size * 0.8f, size * 0.45f }, { 0, 0 }, -35.0f, col);
            DrawRectanglePro({ c.x, c.y - size * 0.3f, size * 0.6f, size * 0.35f }, { 0, 0 }, -35.0f, Fade(col, 0.8f));
            DrawLineEx({ c.x - size * 0.6f, c.y + size * 0.4f }, { c.x + size * 0.8f, c.y - size * 0.6f }, 1.5f, col);
            break;
        }
        case TraitType::Dwarfism: {
            // Scale down / mini helm
            DrawTriangle({ c.x, c.y + size * 0.9f }, { c.x - size * 0.7f, c.y }, { c.x + size * 0.7f, c.y }, col);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - size * 0.4f), size * 0.45f, col);
            break;
        }
        case TraitType::Gigantism: {
            // Scale up / giant crown
            DrawTriangle({ c.x, c.y - size * 0.9f }, { c.x + size * 0.7f, c.y }, { c.x - size * 0.7f, c.y }, col);
            DrawRectangle(static_cast<int>(c.x - size * 0.6f), static_cast<int>(c.y + 2.0f), static_cast<int>(size * 1.2f), static_cast<int>(size * 0.6f), col);
            break;
        }
        case TraitType::ADHD: {
            // Speed Lightning Bolt
            Vector2 p1 = { c.x + size * 0.2f, c.y - size };
            Vector2 p2 = { c.x - size * 0.5f, c.y - size * 0.1f };
            Vector2 p3 = { c.x + size * 0.1f, c.y - size * 0.1f };
            Vector2 p4 = { c.x - size * 0.3f, c.y + size };
            Vector2 p5 = { c.x + size * 0.5f, c.y + size * 0.05f };
            DrawTriangle(p1, p2, p3, col);
            DrawTriangle(p3, p4, p5, col);
            DrawLineEx(p1, p4, 1.5f, WHITE);
            break;
        }
        case TraitType::Ectomorph: {
            // Slender feather / needle
            DrawLineEx({ c.x, c.y - size }, { c.x, c.y + size }, 2.5f, col);
            DrawLine(static_cast<int>(c.x - size * 0.5f), static_cast<int>(c.y - size * 0.3f), static_cast<int>(c.x), static_cast<int>(c.y - size * 0.5f), col);
            DrawLine(static_cast<int>(c.x + size * 0.5f), static_cast<int>(c.y - size * 0.3f), static_cast<int>(c.x), static_cast<int>(c.y - size * 0.5f), col);
            DrawLine(static_cast<int>(c.x - size * 0.5f), static_cast<int>(c.y + size * 0.2f), static_cast<int>(c.x), static_cast<int>(c.y), col);
            DrawLine(static_cast<int>(c.x + size * 0.5f), static_cast<int>(c.y + size * 0.2f), static_cast<int>(c.x), static_cast<int>(c.y), col);
            break;
        }
        case TraitType::Endomorph: {
            // Heavy broad shield
            DrawRectangle(static_cast<int>(c.x - size * 0.8f), static_cast<int>(c.y - size * 0.6f), static_cast<int>(size * 1.6f), static_cast<int>(size * 1.2f), col);
            DrawRectangleLines(static_cast<int>(c.x - size * 0.8f), static_cast<int>(c.y - size * 0.6f), static_cast<int>(size * 1.6f), static_cast<int>(size * 1.2f), WHITE);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 2.5f, GOLD);
            break;
        }
        case TraitType::CIP: {
            // Heart with question mark
            DrawCircle(static_cast<int>(c.x - size * 0.35f), static_cast<int>(c.y - size * 0.25f), size * 0.4f, col);
            DrawCircle(static_cast<int>(c.x + size * 0.35f), static_cast<int>(c.y - size * 0.25f), size * 0.4f, col);
            DrawTriangle({ c.x - size * 0.7f, c.y - size * 0.1f }, { c.x + size * 0.7f, c.y - size * 0.1f }, { c.x, c.y + size * 0.85f }, col);
            Ui::Text("?", c.x - 4, c.y - 7, 12, WHITE);
            break;
        }
        case TraitType::Dextrocardia: {
            // Swapped dual arrows (HP <-> MP)
            DrawCircle(static_cast<int>(c.x - size * 0.4f), static_cast<int>(c.y), size * 0.35f, RED);
            DrawCircle(static_cast<int>(c.x + size * 0.4f), static_cast<int>(c.y), size * 0.35f, SKYBLUE);
            DrawLineEx({ c.x - size * 0.4f, c.y - size * 0.6f }, { c.x + size * 0.4f, c.y - size * 0.6f }, 1.5f, WHITE);
            DrawTriangle({ c.x + size * 0.5f, c.y - size * 0.6f }, { c.x + size * 0.25f, c.y - size * 0.8f }, { c.x + size * 0.25f, c.y - size * 0.4f }, WHITE);
            DrawLineEx({ c.x + size * 0.4f, c.y + size * 0.6f }, { c.x - size * 0.4f, c.y + size * 0.6f }, 1.5f, WHITE);
            DrawTriangle({ c.x - size * 0.5f, c.y + size * 0.6f }, { c.x - size * 0.25f, c.y + size * 0.4f }, { c.x - size * 0.25f, c.y + size * 0.8f }, WHITE);
            break;
        }
        case TraitType::PAD: {
            // Armored boot jumping over spike
            DrawTriangle({ c.x, c.y + size * 0.9f }, { c.x - size * 0.6f, c.y + size * 0.9f }, { c.x - size * 0.3f, c.y + size * 0.2f }, col);
            DrawRectangle(static_cast<int>(c.x + 1), static_cast<int>(c.y - size * 0.4f), static_cast<int>(size * 0.7f), static_cast<int>(size * 0.45f), WHITE);
            DrawLineEx({ c.x + size * 0.2f, c.y - size * 0.7f }, { c.x + size * 0.2f, c.y - size * 0.1f }, 2.0f, WHITE);
            break;
        }
        case TraitType::Dementia: {
            // Ghostly Phantom Spirit
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - size * 0.2f), size * 0.65f, col);
            DrawRectangle(static_cast<int>(c.x - size * 0.65f), static_cast<int>(c.y - size * 0.2f), static_cast<int>(size * 1.3f), static_cast<int>(size * 0.7f), col);
            DrawCircle(static_cast<int>(c.x - size * 0.25f), static_cast<int>(c.y - size * 0.25f), 2.0f, BLACK);
            DrawCircle(static_cast<int>(c.x + size * 0.25f), static_cast<int>(c.y - size * 0.25f), 2.0f, BLACK);
            DrawLine(static_cast<int>(c.x - size * 0.6f), static_cast<int>(c.y + size * 0.5f), static_cast<int>(c.x - size * 0.2f), static_cast<int>(c.y + size * 0.9f), col);
            DrawLine(static_cast<int>(c.x + size * 0.2f), static_cast<int>(c.y + size * 0.9f), static_cast<int>(c.x + size * 0.6f), static_cast<int>(c.y + size * 0.5f), col);
            break;
        }
        case TraitType::Savant: {
            // Arcane Star / Polyhedral Die
            DrawPoly(c, 6, size * 0.85f, 30.0f, col);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.35f, WHITE);
            DrawLine(static_cast<int>(c.x - size * 0.8f), static_cast<int>(c.y), static_cast<int>(c.x + size * 0.8f), static_cast<int>(c.y), Fade(BLACK, 0.4f));
            break;
        }
        case TraitType::IBS: {
            // Gas puff clouds
            DrawCircle(static_cast<int>(c.x - size * 0.3f), static_cast<int>(c.y), size * 0.45f, col);
            DrawCircle(static_cast<int>(c.x + size * 0.3f), static_cast<int>(c.y), size * 0.45f, col);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - size * 0.3f), size * 0.5f, col);
            DrawLine(static_cast<int>(c.x - size * 0.8f), static_cast<int>(c.y + size * 0.5f), static_cast<int>(c.x - size * 0.2f), static_cast<int>(c.y + size * 0.5f), col);
            break;
        }
        default:
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.6f, col);
            break;
    }
}

void ProceduralRenderer::DrawTraitBadge(Vector2 pos, float width, float height, TraitType trait, bool isSelected, float animTime) const {
    (void)animTime;
    const TraitDef& td = Registry::GetTrait(trait);

    Color themeCol = GOLD;
    switch (trait) {
        case TraitType::ADHD:
        case TraitType::PAD:
        case TraitType::Savant:
            themeCol = Color{ 85, 230, 140, 255 }; // Green buff
            break;
        case TraitType::Gigantism:
        case TraitType::Endomorph:
        case TraitType::Vertigo:
            themeCol = Color{ 255, 185, 55, 255 }; // Amber/Yellow
            break;
        case TraitType::Dwarfism:
        case TraitType::Ectomorph:
        case TraitType::Myopia:
        case TraitType::Hyperopia:
            themeCol = Color{ 120, 210, 255, 255 }; // Cyan
            break;
        case TraitType::Dextrocardia:
        case TraitType::Dementia:
        case TraitType::CIP:
            themeCol = Color{ 235, 100, 215, 255 }; // Purple/Magenta
            break;
        case TraitType::Colorblind:
        case TraitType::Nostalgic:
        case TraitType::IBS:
        default:
            themeCol = Color{ 200, 180, 160, 255 }; // Neutral/Parchment
            break;
    }

    // Pill background
    Rectangle rec = { pos.x, pos.y, width, height };
    DrawRectangleRounded(rec, 0.45f, 4, Color{ 22, 20, 32, 240 });
    DrawRectangleRoundedLines(rec, 0.45f, 4, isSelected ? 2.0f : 1.2f, isSelected ? themeCol : Fade(themeCol, 0.7f));

    // Icon circle disc on the left
    Vector2 iconCenter = { pos.x + height * 0.5f, pos.y + height * 0.5f };
    DrawCircleV(iconCenter, height * 0.38f, Color{ 14, 12, 20, 255 });
    DrawCircleLines(static_cast<int>(iconCenter.x), static_cast<int>(iconCenter.y), height * 0.38f, Fade(themeCol, 0.6f));
    DrawTraitIcon(iconCenter, trait, height * 0.28f, themeCol);

    // Trait Name
    Ui::Text(td.name.c_str(), pos.x + height * 1.05f, pos.y + (height - 12.0f) * 0.5f, 12, WHITE);

    // Special bonus badge tag on right (e.g. +15% Gold)
    if (td.goldBonus > 0.001f) {
        std::string bonusStr = "+" + std::to_string(static_cast<int>(td.goldBonus * 100.0f)) + "%G";
        int bw = Ui::Measure(bonusStr.c_str(), 10);
        DrawRectangleRounded({ pos.x + width - bw - 10.0f, pos.y + 4.0f, bw + 6.0f, height - 8.0f }, 0.4f, 2, Color{ 60, 50, 15, 255 });
        Ui::Text(bonusStr.c_str(), pos.x + width - bw - 7.0f, pos.y + (height - 10.0f) * 0.5f, 10, GOLD);
    }
}

void ProceduralRenderer::DrawStatBadge(Vector2 pos, float width, float height, const char* label, int multPercent, Color accentCol) const {
    Rectangle rec = { pos.x, pos.y, width, height };
    DrawRectangleRounded(rec, 0.35f, 4, Color{ 18, 18, 28, 235 });
    DrawRectangleRoundedLines(rec, 0.35f, 4, 1.5f, accentCol);

    // Icon glyph on left
    Vector2 ic = { pos.x + 13.0f, pos.y + height * 0.5f };
    if (std::string(label) == "HP") {
        DrawCircle(static_cast<int>(ic.x - 3), static_cast<int>(ic.y - 2), 3.5f, RED);
        DrawCircle(static_cast<int>(ic.x + 3), static_cast<int>(ic.y - 2), 3.5f, RED);
        DrawTriangle({ ic.x - 6.5f, ic.y }, { ic.x, ic.y + 6.0f }, { ic.x + 6.5f, ic.y }, RED);
    } else if (std::string(label) == "MP") {
        DrawCircle(static_cast<int>(ic.x), static_cast<int>(ic.y + 1), 4.5f, SKYBLUE);
        DrawTriangle({ ic.x - 4.5f, ic.y + 1 }, { ic.x + 4.5f, ic.y + 1 }, { ic.x, ic.y - 6.0f }, SKYBLUE);
    } else if (std::string(label) == "STR") {
        DrawLineEx({ ic.x - 5, ic.y + 5 }, { ic.x + 5, ic.y - 5 }, 2.0f, ORANGE);
        DrawLineEx({ ic.x - 5, ic.y - 5 }, { ic.x + 5, ic.y + 5 }, 2.0f, ORANGE);
    } else { // INT
        DrawRectangle(static_cast<int>(ic.x - 5), static_cast<int>(ic.y - 5), 10, 10, Color{ 185, 120, 245, 255 });
        DrawLine(static_cast<int>(ic.x), static_cast<int>(ic.y - 5), static_cast<int>(ic.x), static_cast<int>(ic.y + 5), WHITE);
    }

    std::string valStr = std::to_string(multPercent) + "%";
    Ui::Text(label, pos.x + 24.0f, pos.y + (height - 11.0f) * 0.5f, 11, accentCol);
    Ui::Text(valStr.c_str(), pos.x + width - Ui::Measure(valStr.c_str(), 11) - 6.0f, pos.y + (height - 11.0f) * 0.5f, 11, WHITE);
}

void ProceduralRenderer::DrawAbilitySlot(Vector2 pos, float width, float height, const char* keyText, const char* name, int manaCost, Color accentCol) const {
    Rectangle rec = { pos.x, pos.y, width, height };
    DrawRectangleRounded(rec, 0.25f, 4, Color{ 20, 18, 30, 240 });
    DrawRectangleRoundedLines(rec, 0.25f, 4, 1.5f, accentCol);

    // Keycap badge
    Rectangle keyRec = { pos.x + 6.0f, pos.y + 5.0f, 26.0f, height - 10.0f };
    DrawRectangleRounded(keyRec, 0.3f, 4, Color{ 36, 32, 50, 255 });
    DrawRectangleRoundedLines(keyRec, 0.3f, 4, 1.5f, accentCol);
    int kw = Ui::Measure(keyText, 12);
    Ui::Text(keyText, keyRec.x + (keyRec.width - kw) * 0.5f, keyRec.y + (keyRec.height - 12.0f) * 0.5f, 12, WHITE);

    // Ability title
    Ui::Text(name, pos.x + 38.0f, pos.y + (height - 13.0f) * 0.5f, 13, accentCol);

    // Mana cost diamond gem if applicable
    if (manaCost > 0) {
        std::string manaStr = std::to_string(manaCost) + " MP";
        int mw = Ui::Measure(manaStr.c_str(), 11);
        Vector2 gemC = { pos.x + width - mw - 14.0f, pos.y + height * 0.5f };
        DrawPoly(gemC, 4, 5.0f, 45.0f, SKYBLUE);
        Ui::Text(manaStr.c_str(), pos.x + width - mw - 6.0f, pos.y + (height - 11.0f) * 0.5f, 11, SKYBLUE);
    }
}

void ProceduralRenderer::DrawManorNodeIcon(Vector2 c, ManorNodeId id, bool unlocked, bool maxed, float animTime) const {
    (void)animTime;
    Color mainCol = unlocked ? (maxed ? LIME : GOLD) : Color{ 120, 125, 140, 255 };

    switch (id) {
        case ManorNodeId::Stat_Health: {
            // Radiant Ruby Heart
            DrawCircle(static_cast<int>(c.x - 5), static_cast<int>(c.y - 4), 6.5f, RED);
            DrawCircle(static_cast<int>(c.x + 5), static_cast<int>(c.y - 4), 6.5f, RED);
            DrawTriangle({ c.x - 11.5f, c.y - 2.0f }, { c.x, c.y + 11.0f }, { c.x + 11.5f, c.y - 2.0f }, RED);
            DrawCircle(static_cast<int>(c.x - 4), static_cast<int>(c.y - 6), 2.0f, WHITE);
            break;
        }
        case ManorNodeId::Stat_Mana: {
            // Hexagonal Sapphire Crystal
            DrawPoly(c, 6, 12.0f, 0.0f, SKYBLUE);
            DrawPolyLines(c, 6, 12.0f, 0.0f, WHITE);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 4.0f, WHITE);
            break;
        }
        case ManorNodeId::Stat_Attack: {
            // Crossed Broadswords
            DrawLineEx({ c.x - 10, c.y + 10 }, { c.x + 10, c.y - 10 }, 3.0f, Color{ 220, 225, 235, 255 });
            DrawLineEx({ c.x + 10, c.y + 10 }, { c.x - 10, c.y - 10 }, 3.0f, Color{ 220, 225, 235, 255 });
            DrawCircle(static_cast<int>(c.x - 7), static_cast<int>(c.y + 7), 2.5f, GOLD);
            DrawCircle(static_cast<int>(c.x + 7), static_cast<int>(c.y + 7), 2.5f, GOLD);
            break;
        }
        case ManorNodeId::Stat_MagicDmg: {
            // Wizard Staff with glowing gem
            DrawLineEx({ c.x - 7, c.y + 10 }, { c.x + 6, c.y - 7 }, 3.0f, Color{ 140, 95, 50, 255 });
            DrawCircle(static_cast<int>(c.x + 7), static_cast<int>(c.y - 8), 6.0f, PURPLE);
            DrawCircle(static_cast<int>(c.x + 7), static_cast<int>(c.y - 8), 3.0f, WHITE);
            break;
        }
        case ManorNodeId::Stat_Armor: {
            // Heater Kite Shield
            DrawRectangle(static_cast<int>(c.x - 8), static_cast<int>(c.y - 8), 16, 10, Color{ 160, 170, 185, 255 });
            DrawTriangle({ c.x - 8, c.y + 2 }, { c.x + 8, c.y + 2 }, { c.x, c.y + 11 }, Color{ 160, 170, 185, 255 });
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 3.0f, GOLD);
            break;
        }
        case ManorNodeId::Stat_EquipWeight: {
            // Heavy Anvil with feather
            DrawRectangle(static_cast<int>(c.x - 9), static_cast<int>(c.y), 18, 8, Color{ 90, 95, 105, 255 });
            DrawRectangle(static_cast<int>(c.x - 5), static_cast<int>(c.y - 4), 10, 4, Color{ 120, 125, 140, 255 });
            DrawLineEx({ c.x - 8, c.y - 8 }, { c.x + 7, c.y - 4 }, 2.0f, GOLD);
            break;
        }
        case ManorNodeId::Stat_CritChance: {
            // Reticle / Sniper Star
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 8.0f, mainCol);
            DrawLine(static_cast<int>(c.x - 11), static_cast<int>(c.y), static_cast<int>(c.x + 11), static_cast<int>(c.y), mainCol);
            DrawLine(static_cast<int>(c.x), static_cast<int>(c.y - 11), static_cast<int>(c.x), static_cast<int>(c.y + 11), mainCol);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 2.0f, WHITE);
            break;
        }
        case ManorNodeId::Stat_CritDmg: {
            // Serrated Dagger dripping spark
            DrawLineEx({ c.x - 6, c.y + 8 }, { c.x + 7, c.y - 7 }, 3.0f, Color{ 230, 235, 245, 255 });
            DrawCircle(static_cast<int>(c.x + 9), static_cast<int>(c.y - 9), 3.0f, RED);
            break;
        }
        case ManorNodeId::Stat_GoldGain: {
            // Coin Pouch & Gold
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y + 2), 9.0f, Color{ 145, 95, 40, 255 });
            DrawRectangle(static_cast<int>(c.x - 4), static_cast<int>(c.y - 8), 8, 4, GOLD);
            DrawCircle(static_cast<int>(c.x + 6), static_cast<int>(c.y + 7), 3.5f, GOLD);
            break;
        }
        case ManorNodeId::Stat_Haggling: {
            // Merchant Scales
            DrawLineEx({ c.x, c.y - 8 }, { c.x, c.y + 9 }, 2.0f, GOLD);
            DrawLineEx({ c.x - 10, c.y - 6 }, { c.x + 10, c.y - 6 }, 2.0f, GOLD);
            DrawTriangle({ c.x - 12, c.y + 2 }, { c.x - 6, c.y + 2 }, { c.x - 9, c.y - 3 }, GOLD);
            DrawTriangle({ c.x + 6, c.y + 2 }, { c.x + 12, c.y + 2 }, { c.x + 9, c.y - 3 }, GOLD);
            break;
        }
        case ManorNodeId::Stat_PotionEfficacy: {
            // Glass Flask
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y + 3), 7.5f, Color{ 220, 60, 80, 255 });
            DrawRectangle(static_cast<int>(c.x - 3), static_cast<int>(c.y - 8), 6, 6, WHITE);
            DrawRectangle(static_cast<int>(c.x - 4), static_cast<int>(c.y - 10), 8, 3, Color{ 160, 110, 60, 255 });
            break;
        }
        case ManorNodeId::Stat_InvulnTime: {
            // Golden Aegis / Wings
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 10.0f, GOLD);
            DrawPoly(c, 3, 7.0f, 0.0f, WHITE);
            break;
        }
        case ManorNodeId::Stat_DownStrikeBonus: {
            // Downward Thrust Blade
            DrawLineEx({ c.x, c.y - 10 }, { c.x, c.y + 8 }, 3.0f, Color{ 225, 235, 250, 255 });
            DrawTriangle({ c.x - 4, c.y + 6 }, { c.x + 4, c.y + 6 }, { c.x, c.y + 11 }, WHITE);
            DrawLine(static_cast<int>(c.x - 7), static_cast<int>(c.y + 11), static_cast<int>(c.x + 7), static_cast<int>(c.y + 11), GOLD);
            break;
        }
        case ManorNodeId::Unlock_Blacksmith: {
            // Hammer striking anvil with sparks
            DrawRectangle(static_cast<int>(c.x - 7), static_cast<int>(c.y + 1), 14, 7, Color{ 80, 85, 95, 255 });
            DrawLineEx({ c.x - 6, c.y - 2 }, { c.x + 5, c.y - 9 }, 3.0f, Color{ 160, 110, 60, 255 });
            DrawRectangle(static_cast<int>(c.x + 3), static_cast<int>(c.y - 12), 6, 6, Color{ 140, 145, 160, 255 });
            DrawCircle(static_cast<int>(c.x - 4), static_cast<int>(c.y - 1), 2.0f, ORANGE);
            break;
        }
        case ManorNodeId::Unlock_Enchantress: {
            // Witch Cauldron with purple vapours
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y + 2), 8.0f, Color{ 45, 38, 55, 255 });
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - 2), 5.0f, MAGENTA);
            DrawCircle(static_cast<int>(c.x - 3), static_cast<int>(c.y - 7), 2.5f, PURPLE);
            DrawCircle(static_cast<int>(c.x + 3), static_cast<int>(c.y - 9), 2.0f, MAGENTA);
            break;
        }
        case ManorNodeId::Unlock_Architect: {
            // Sextant & Blueprint
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 9.0f, SKYBLUE);
            DrawLineEx({ c.x - 6, c.y - 6 }, { c.x + 6, c.y + 6 }, 2.0f, GOLD);
            DrawLineEx({ c.x - 6, c.y + 6 }, { c.x + 6, c.y - 6 }, 2.0f, GOLD);
            break;
        }
        case ManorNodeId::Class_Barbarian: {
            // Horned Helmet / Battleaxe
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y + 1), 7.0f, Color{ 140, 145, 160, 255 });
            DrawLineEx({ c.x - 6, c.y }, { c.x - 11, c.y - 7 }, 2.5f, WHITE);
            DrawLineEx({ c.x + 6, c.y }, { c.x + 11, c.y - 7 }, 2.5f, WHITE);
            break;
        }
        case ManorNodeId::Class_Mage: {
            // Pointed Wizard Hat
            DrawTriangle({ c.x - 9, c.y + 7 }, { c.x + 9, c.y + 7 }, { c.x, c.y - 10 }, Color{ 75, 45, 145, 255 });
            DrawLineEx({ c.x - 12, c.y + 7 }, { c.x + 12, c.y + 7 }, 3.0f, GOLD);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - 2), 2.0f, YELLOW);
            break;
        }
        case ManorNodeId::Class_Knave: {
            // Twin Stiletto Daggers
            DrawLineEx({ c.x - 7, c.y - 7 }, { c.x + 7, c.y + 7 }, 2.5f, Color{ 180, 190, 205, 255 });
            DrawLineEx({ c.x + 7, c.y - 7 }, { c.x - 7, c.y + 7 }, 2.5f, Color{ 180, 190, 205, 255 });
            break;
        }
        case ManorNodeId::Class_Shinobi: {
            // Shuriken
            DrawPoly(c, 4, 10.0f, 45.0f, Color{ 180, 190, 205, 255 });
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 3.0f, Color{ 22, 20, 32, 255 });
            break;
        }
        case ManorNodeId::Class_Miner: {
            // Pickaxe & Gold
            DrawLineEx({ c.x - 7, c.y + 8 }, { c.x + 7, c.y - 6 }, 2.5f, Color{ 145, 100, 50, 255 });
            DrawTriangle({ c.x + 4, c.y - 10 }, { c.x + 10, c.y - 4 }, { c.x + 11, c.y - 11 }, Color{ 175, 180, 195, 255 });
            DrawCircle(static_cast<int>(c.x - 5), static_cast<int>(c.y + 4), 3.0f, GOLD);
            break;
        }
        case ManorNodeId::Class_Lich: {
            // Crowned Skull
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y + 1), 6.5f, RAYWHITE);
            DrawCircle(static_cast<int>(c.x - 2.5f), static_cast<int>(c.y), 1.8f, MAGENTA);
            DrawCircle(static_cast<int>(c.x + 2.5f), static_cast<int>(c.y), 1.8f, MAGENTA);
            DrawTriangle({ c.x - 6, c.y - 4 }, { c.x + 6, c.y - 4 }, { c.x, c.y - 10 }, GOLD);
            break;
        }
        case ManorNodeId::Class_Dragon: {
            // Dragon Head breathing flame
            DrawTriangle({ c.x - 7, c.y + 6 }, { c.x - 7, c.y - 7 }, { c.x + 7, c.y }, RED);
            DrawCircle(static_cast<int>(c.x - 2), static_cast<int>(c.y - 2), 2.0f, YELLOW);
            DrawCircle(static_cast<int>(c.x + 10), static_cast<int>(c.y), 3.5f, ORANGE);
            break;
        }
        case ManorNodeId::Upgrade_AllBasicClasses: {
            // Celestial Telescope
            DrawLineEx({ c.x - 8, c.y + 8 }, { c.x + 7, c.y - 7 }, 3.5f, GOLD);
            DrawCircle(static_cast<int>(c.x + 9), static_cast<int>(c.y - 9), 2.5f, WHITE);
            DrawCircle(static_cast<int>(c.x + 11), static_cast<int>(c.y - 3), 1.5f, YELLOW);
            break;
        }
        default:
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 8.0f, mainCol);
            break;
    }
}

void ProceduralRenderer::DrawDetailedCharon(Vector2 pos, float animTime, bool highlight) const {
    const auto& pal = GetPalette();
    float cx = pos.x;
    float cy = pos.y;

    // Drifting dark soul mist at feet
    float mistW = 60.0f + std::sin(animTime * 2.5f) * 10.0f;
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 3.0f), mistW, 10.0f, Fade(pal.dungeon.stoneBg[2], 0.45f));
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy - 6.0f), mistW * 0.7f, 6.0f, Fade(pal.dungeon.stoneBg[1], 0.35f));

    // Flowing dark cowl / ragged robes
    Color robeBase = pal.dungeon.stoneBg[1];
    Color robeFold = pal.dungeon.stoneBg[2];
    Color robeHi   = highlight ? pal.fireGold[3] : pal.dungeon.stoneTile[2];

    // Body cloak
    DrawTriangle({ cx - 26.0f, cy }, { cx + 26.0f, cy }, { cx, cy - 78.0f }, robeBase);
    DrawTriangle({ cx - 18.0f, cy }, { cx + 4.0f, cy }, { cx - 3.0f, cy - 72.0f }, robeFold);
    DrawTriangle({ cx + 6.0f, cy }, { cx + 22.0f, cy }, { cx + 3.0f, cy - 72.0f }, robeFold);

    // Deep shadowed hood
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy - 72.0f), 15.0f, pal.outline);
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy - 72.0f), 15.0f, robeHi);

    // Twin glowing red/crimson soul eyes
    float eyePulse = 0.8f + 0.2f * std::sin(animTime * 4.0f);
    DrawCircleV({ cx - 4.5f, cy - 72.0f }, 3.5f * eyePulse, pal.bloodCloth[3]);
    DrawCircleV({ cx + 4.5f, cy - 72.0f }, 3.5f * eyePulse, pal.bloodCloth[3]);
    DrawCircleV({ cx - 4.5f, cy - 72.0f }, 1.5f, pal.metal[4]);
    DrawCircleV({ cx + 4.5f, cy - 72.0f }, 1.5f, pal.metal[4]);

    // Skeletal right arm holding tall Reaper Scythe
    Vector2 handPos = { cx - 18.0f, cy - 42.0f };
    DrawLineEx({ cx - 8.0f, cy - 55.0f }, handPos, 3.0f, pal.bone[4]);

    // Tall curved Reaper Scythe
    Vector2 scytheBase = { cx - 22.0f, cy };
    Vector2 scytheTop  = { cx - 14.0f, cy - 105.0f };
    DrawLineEx(scytheBase, scytheTop, 3.5f, pal.wood[2]);
    // Scythe blade
    Vector2 bladeTip = { cx + 20.0f, cy - 98.0f };
    DrawTriangle(scytheTop, { scytheTop.x - 8.0f, scytheTop.y + 12.0f }, bladeTip, pal.metal[4]);
    DrawLineEx(scytheTop, bladeTip, 2.0f, pal.metal[4]);
    DrawLineEx({ scytheTop.x - 8.0f, scytheTop.y + 12.0f }, bladeTip, 1.5f, pal.metal[2]);

    // Skeletal left arm holding brass Soul Lantern
    Vector2 lanternArm = { cx + 20.0f, cy - 48.0f };
    DrawLineEx({ cx + 8.0f, cy - 56.0f }, lanternArm, 2.5f, pal.bone[4]);
    // Chain
    DrawLineEx(lanternArm, { lanternArm.x + 4.0f, lanternArm.y + 12.0f }, 1.5f, pal.fireGold[3]);
    Vector2 lanternCenter = { lanternArm.x + 4.0f, lanternArm.y + 20.0f };

    // Brass Lantern frame
    DrawRectangle(static_cast<int>(lanternCenter.x - 6.0f), static_cast<int>(lanternCenter.y - 8.0f), 12, 16, pal.wood[1]);
    DrawRectangleLines(static_cast<int>(lanternCenter.x - 6.0f), static_cast<int>(lanternCenter.y - 8.0f), 12, 16, pal.fireGold[3]);
    // Dancing turquoise soul flame
    float flameGlow = 8.0f + 3.0f * std::sin(animTime * 7.0f);
    DrawCircleGradient(static_cast<int>(lanternCenter.x), static_cast<int>(lanternCenter.y), flameGlow * 1.8f, Fade(pal.moonMagic[3], 0.45f), BLANK);
    DrawCircleV(lanternCenter, flameGlow * 0.7f, pal.moonMagic[3]);
    DrawCircleV(lanternCenter, flameGlow * 0.35f, pal.moonMagic[4]);
}

// ============================================================================
// Rune, Equipment & Enemy Procedural Icons
// ============================================================================

void ProceduralRenderer::DrawRuneIcon(Vector2 c, RuneType rune, bool unlocked, float time) const {
    Color bgStone = unlocked ? Color{ 32, 28, 44, 255 } : Color{ 22, 20, 28, 255 };
    Color runeAccent = DARKGRAY;

    switch (rune) {
        case RuneType::Vault:       runeAccent = SKYBLUE; break;
        case RuneType::Sprint:      runeAccent = Color{ 45, 220, 200, 255 }; break;
        case RuneType::Vampire:     runeAccent = RED; break;
        case RuneType::Siphon:      runeAccent = Color{ 175, 95, 255, 255 }; break;
        case RuneType::Retaliation: runeAccent = ORANGE; break;
        case RuneType::Bounty:      runeAccent = GOLD; break;
        case RuneType::Haste:       runeAccent = YELLOW; break;
        case RuneType::Grace:       runeAccent = Color{ 230, 240, 255, 255 }; break;
        default:                    runeAccent = DARKGRAY; break;
    }

    if (!unlocked) {
        runeAccent = Color{ 80, 85, 95, 255 };
    }

    // 1. Шестигранна кам'яна рунна скрижаль (Hexagonal runic tablet)
    DrawPoly(c, 6, 21.0f, 30.0f, bgStone);
    DrawPolyLines(c, 6, 21.0f, 30.0f, unlocked ? runeAccent : Color{ 50, 50, 60, 255 });
    DrawPolyLines(c, 6, 18.0f, 30.0f, Fade(runeAccent, unlocked ? 0.35f : 0.15f));

    // Сяйво активної руни
    if (unlocked) {
        float pulse = 0.85f + 0.15f * std::sin(time * 3.5f);
        DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(c.y), 16.0f * pulse, Fade(runeAccent, 0.25f), BLANK);
    }

    // 2. Унікальний рельєфний символ руни
    switch (rune) {
        case RuneType::Vault: {
            // Подвійні крила вільного польоту (Angelic Feathered Wings)
            // Ліве крило
            DrawTriangle({ c.x - 12.0f, c.y + 6.0f }, { c.x - 2.0f, c.y + 2.0f }, { c.x - 12.0f, c.y - 10.0f }, runeAccent);
            DrawTriangle({ c.x - 12.0f, c.y - 10.0f }, { c.x - 2.0f, c.y + 2.0f }, { c.x - 4.0f, c.y - 12.0f }, WHITE);
            // Праве крило
            DrawTriangle({ c.x + 2.0f, c.y + 2.0f }, { c.x + 12.0f, c.y + 6.0f }, { c.x + 12.0f, c.y - 10.0f }, runeAccent);
            DrawTriangle({ c.x + 2.0f, c.y + 2.0f }, { c.x + 12.0f, c.y - 10.0f }, { c.x + 4.0f, c.y - 12.0f }, WHITE);
            // Центральна вертикальна іскра
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - 2.0f), 2.5f, WHITE);
            break;
        }

        case RuneType::Sprint: {
            // Крилатий чобіт Гермеса та стрімкі потоки вітру (Wind Dash)
            DrawRectangle(static_cast<int>(c.x - 8.0f), static_cast<int>(c.y + 1.0f), 14, 5, runeAccent);
            DrawCircle(static_cast<int>(c.x + 6.0f), static_cast<int>(c.y + 3.0f), 2.5f, runeAccent);
            DrawRectangle(static_cast<int>(c.x - 6.0f), static_cast<int>(c.y - 6.0f), 5, 8, runeAccent);
            // Перо на п'яті
            DrawTriangle({ c.x - 12.0f, c.y - 2.0f }, { c.x - 5.0f, c.y - 1.0f }, { c.x - 14.0f, c.y - 10.0f }, WHITE);
            // Смуги вітру
            DrawLineEx({ c.x - 8.0f, c.y + 8.0f }, { c.x + 8.0f, c.y + 8.0f }, 2.0f, runeAccent);
            DrawLineEx({ c.x - 4.0f, c.y - 9.0f }, { c.x + 8.0f, c.y - 9.0f }, 1.5f, WHITE);
            break;
        }

        case RuneType::Vampire: {
            // Крапля животворної крові з магічним відблиском (Blood Teardrop)
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y + 2.0f), 6.5f, runeAccent);
            DrawTriangle(
                { c.x - 6.2f, c.y + 2.0f },
                { c.x, c.y - 9.5f },
                { c.x + 6.2f, c.y + 2.0f },
                runeAccent
            );
            // Внутрішній кривавий відблиск та білий відблиск
            DrawCircle(static_cast<int>(c.x - 2.0f), static_cast<int>(c.y + 1.0f), 2.2f, WHITE);
            DrawCircle(static_cast<int>(c.x + 1.5f), static_cast<int>(c.y + 4.0f), 1.5f, Color{ 255, 120, 140, 255 });
            break;
        }

        case RuneType::Siphon: {
            // Вир мані та поглинання душ (Mana Vortex)
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 8.5f, runeAccent);
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 5.0f, SKYBLUE);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 2.5f, WHITE);
            // 3 вигнуті спіральні дуги
            for (int k = 0; k < 3; ++k) {
                float a = time * 3.0f + k * (2.0f * 3.14159f / 3.0f);
                Vector2 sp = { c.x + std::cos(a) * 8.5f, c.y + std::sin(a) * 8.5f };
                DrawCircleV(sp, 2.0f, SKYBLUE);
            }
            break;
        }

        case RuneType::Retaliation: {
            // Шипований лицарський щит відплати (Barbed Retaliation Aegis)
            DrawRectangle(static_cast<int>(c.x - 8.0f), static_cast<int>(c.y - 8.0f), 16, 8, runeAccent);
            DrawTriangle({ c.x - 8.0f, c.y }, { c.x, c.y + 10.0f }, { c.x + 8.0f, c.y }, runeAccent);
            // Центральний шип
            DrawTriangle({ c.x - 3.0f, c.y - 2.0f }, { c.x, c.y - 9.0f }, { c.x + 3.0f, c.y - 2.0f }, WHITE);
            // Бічні гострі леза
            DrawLineEx({ c.x - 8.0f, c.y - 5.0f }, { c.x - 12.0f, c.y - 8.0f }, 2.0f, ORANGE);
            DrawLineEx({ c.x + 8.0f, c.y - 5.0f }, { c.x + 12.0f, c.y - 8.0f }, 2.0f, ORANGE);
            break;
        }

        case RuneType::Bounty: {
            // Золота монета з діамантовою зіркою (Golden Star Bounty)
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 8.5f, GOLD);
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 8.5f, Color{ 145, 95, 20, 255 });
            // 4-променева зірка
            DrawLineEx({ c.x - 6.0f, c.y }, { c.x + 6.0f, c.y }, 2.0f, WHITE);
            DrawLineEx({ c.x, c.y - 6.0f }, { c.x, c.y + 6.0f }, 2.0f, WHITE);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 2.0f, YELLOW);
            break;
        }

        case RuneType::Haste: {
            // Блискавка швидкої ходи (Swift Lightning Bolt)
            Vector2 p1 = { c.x + 3.0f, c.y - 11.0f };
            Vector2 p2 = { c.x - 4.0f, c.y - 1.0f };
            Vector2 p3 = { c.x + 2.0f, c.y - 1.0f };
            Vector2 p4 = { c.x - 3.0f, c.y + 11.0f };
            DrawLineEx(p1, p2, 3.0f, runeAccent);
            DrawLineEx(p2, p3, 3.0f, runeAccent);
            DrawLineEx(p3, p4, 3.0f, runeAccent);
            DrawLineEx(p1, p2, 1.5f, WHITE);
            DrawLineEx(p3, p4, 1.5f, WHITE);
            break;
        }

        case RuneType::Grace: {
            // Ореол та біле ангельське перо благодаті (Grace Feather & Halo)
            // Ореол
            DrawEllipseLines(static_cast<int>(c.x), static_cast<int>(c.y - 7.0f), 8.0f, 3.5f, GOLD);
            // Перо
            DrawLineEx({ c.x - 6.0f, c.y + 8.0f }, { c.x + 5.0f, c.y - 4.0f }, 2.5f, WHITE);
            DrawTriangle({ c.x - 5.0f, c.y + 6.0f }, { c.x + 6.0f, c.y - 6.0f }, { c.x + 1.0f, c.y + 2.0f }, runeAccent);
            DrawCircle(static_cast<int>(c.x - 5.0f), static_cast<int>(c.y + 8.0f), 1.5f, GOLD);
            break;
        }

        default:
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 4.0f, DARKGRAY);
            break;
    }
}

void ProceduralRenderer::DrawEquipSlotIcon(Vector2 c, EquipSlot slot, bool active) const {
    Color col = active ? GOLD : Color{ 175, 185, 205, 255 };

    switch (slot) {
        case EquipSlot::Sword: {
            // Широкий меч
            DrawLineEx({ c.x - 7.0f, c.y + 7.0f }, { c.x + 7.0f, c.y - 7.0f }, 3.0f, col);
            DrawLineEx({ c.x - 6.0f, c.y + 2.0f }, { c.x - 2.0f, c.y + 6.0f }, 2.5f, active ? ORANGE : GRAY);
            DrawCircle(static_cast<int>(c.x - 7.0f), static_cast<int>(c.y + 7.0f), 2.0f, GOLD);
            break;
        }
        case EquipSlot::Helm: {
            // Лицарський шолом
            DrawRectangle(static_cast<int>(c.x - 6.0f), static_cast<int>(c.y - 6.0f), 12, 12, col);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - 6.0f), 6.0f, col);
            DrawLine(static_cast<int>(c.x - 4.0f), static_cast<int>(c.y), static_cast<int>(c.x + 4.0f), static_cast<int>(c.y), BLACK);
            break;
        }
        case EquipSlot::Chest: {
            // Кіраса / нагрудник
            DrawRectangle(static_cast<int>(c.x - 7.0f), static_cast<int>(c.y - 6.0f), 14, 10, col);
            DrawTriangle({ c.x - 7.0f, c.y + 4.0f }, { c.x, c.y + 8.0f }, { c.x + 7.0f, c.y + 4.0f }, col);
            break;
        }
        case EquipSlot::Limbs: {
            // Поножі / латні чоботи
            DrawRectangle(static_cast<int>(c.x - 6.0f), static_cast<int>(c.y - 6.0f), 5, 11, col);
            DrawRectangle(static_cast<int>(c.x + 1.0f), static_cast<int>(c.y - 6.0f), 5, 11, col);
            DrawRectangle(static_cast<int>(c.x - 6.0f), static_cast<int>(c.y + 3.0f), 7, 3, col);
            DrawRectangle(static_cast<int>(c.x + 1.0f), static_cast<int>(c.y + 3.0f), 7, 3, col);
            break;
        }
        case EquipSlot::Cape: {
            // Шовковий плащ із фібулою
            DrawTriangle({ c.x - 7.0f, c.y + 7.0f }, { c.x, c.y - 6.0f }, { c.x + 7.0f, c.y + 7.0f }, col);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - 5.0f), 3.0f, GOLD);
            break;
        }
        default:
            break;
    }
}

void ProceduralRenderer::DrawEquipTierBadge(Vector2 c, EquipTier tier) const {
    Color tCol = GetTierPrimaryColor(tier, RAYWHITE);
    DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 10.0f, Color{ 24, 22, 34, 255 });
    DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 10.0f, tCol);
    DrawPoly(c, 4, 6.0f, 45.0f, tCol);
}

void ProceduralRenderer::DrawEnemyIcon(Vector2 c, EnemyType type, float size) const {
    float s = size / 24.0f;

    switch (type) {
        case EnemyType::Skeleton: {
            // Череп зі схрещеними кістками та мечем
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - 3.0f * s), 7.0f * s, RAYWHITE);
            DrawRectangle(static_cast<int>(c.x - 4.0f * s), static_cast<int>(c.y + 2.0f * s), static_cast<int>(8.0f * s), static_cast<int>(5.0f * s), RAYWHITE);
            // Очниці
            DrawCircle(static_cast<int>(c.x - 2.5f * s), static_cast<int>(c.y - 3.0f * s), 2.0f * s, Color{ 22, 20, 28, 255 });
            DrawCircle(static_cast<int>(c.x + 2.5f * s), static_cast<int>(c.y - 3.0f * s), 2.0f * s, Color{ 22, 20, 28, 255 });
            // Зуби
            DrawLine(static_cast<int>(c.x - 3.0f * s), static_cast<int>(c.y + 4.0f * s), static_cast<int>(c.x + 3.0f * s), static_cast<int>(c.y + 4.0f * s), BLACK);
            break;
        }

        case EnemyType::FloatingEye: {
            // Криваве ширяюче око з райдужкою та зіницею
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 9.0f * s, RAYWHITE);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 5.5f * s, RED);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 3.0f * s, BLACK);
            DrawCircle(static_cast<int>(c.x - 2.0f * s), static_cast<int>(c.y - 2.0f * s), 1.5f * s, WHITE);
            // Маленькі крильця ока
            DrawTriangle({ c.x - 8.0f * s, c.y }, { c.x - 14.0f * s, c.y - 6.0f * s }, { c.x - 6.0f * s, c.y - 4.0f * s }, Color{ 200, 45, 55, 255 });
            DrawTriangle({ c.x + 8.0f * s, c.y }, { c.x + 6.0f * s, c.y - 4.0f * s }, { c.x + 14.0f * s, c.y - 6.0f * s }, Color{ 200, 45, 55, 255 });
            break;
        }

        case EnemyType::ShieldKnight: {
            // Важкий лицарський шолом та гербовий щит
            DrawRectangle(static_cast<int>(c.x - 7.0f * s), static_cast<int>(c.y - 9.0f * s), static_cast<int>(14.0f * s), static_cast<int>(11.0f * s), Color{ 160, 170, 185, 255 });
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - 9.0f * s), 7.0f * s, Color{ 160, 170, 185, 255 });
            DrawLine(static_cast<int>(c.x - 4.0f * s), static_cast<int>(c.y - 3.0f * s), static_cast<int>(c.x + 4.0f * s), static_cast<int>(c.y - 3.0f * s), BLACK);
            // Щит
            DrawRectangle(static_cast<int>(c.x - 6.0f * s), static_cast<int>(c.y + 1.0f * s), static_cast<int>(12.0f * s), static_cast<int>(6.0f * s), Color{ 75, 85, 105, 255 });
            DrawTriangle({ c.x - 6.0f * s, c.y + 7.0f * s }, { c.x, c.y + 12.0f * s }, { c.x + 6.0f * s, c.y + 7.0f * s }, Color{ 75, 85, 105, 255 });
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y + 5.0f * s), 2.5f * s, GOLD);
            break;
        }

        case EnemyType::FireMage: {
            // Капюшон темного мага з палаючими очима
            DrawTriangle({ c.x - 9.0f * s, c.y + 8.0f * s }, { c.x, c.y - 11.0f * s }, { c.x + 9.0f * s, c.y + 8.0f * s }, Color{ 145, 32, 42, 255 });
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - 1.0f * s), 6.5f * s, Color{ 18, 14, 24, 255 });
            DrawCircle(static_cast<int>(c.x - 2.5f * s), static_cast<int>(c.y - 1.0f * s), 1.8f * s, YELLOW);
            DrawCircle(static_cast<int>(c.x + 2.5f * s), static_cast<int>(c.y - 1.0f * s), 1.8f * s, YELLOW);
            // Іскра вогню
            DrawCircle(static_cast<int>(c.x + 8.0f * s), static_cast<int>(c.y - 6.0f * s), 3.0f * s, ORANGE);
            break;
        }

        case EnemyType::Warg: {
            // Лютий вовк із пащею та гострими вухами
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 7.5f * s, Color{ 85, 68, 58, 255 });
            // Вуха
            DrawTriangle({ c.x - 7.0f * s, c.y - 2.0f * s }, { c.x - 6.0f * s, c.y - 11.0f * s }, { c.x - 1.0f * s, c.y - 5.0f * s }, Color{ 85, 68, 58, 255 });
            DrawTriangle({ c.x + 1.0f * s, c.y - 5.0f * s }, { c.x + 6.0f * s, c.y - 11.0f * s }, { c.x + 7.0f * s, c.y - 2.0f * s }, Color{ 85, 68, 58, 255 });
            // Морда та очі
            DrawTriangle({ c.x - 4.0f * s, c.y + 2.0f * s }, { c.x, c.y + 8.0f * s }, { c.x + 4.0f * s, c.y + 2.0f * s }, Color{ 48, 38, 32, 255 });
            DrawCircle(static_cast<int>(c.x - 3.0f * s), static_cast<int>(c.y - 1.0f * s), 1.5f * s, RED);
            DrawCircle(static_cast<int>(c.x + 3.0f * s), static_cast<int>(c.y - 1.0f * s), 1.5f * s, RED);
            // Білі ікла
            DrawLine(static_cast<int>(c.x - 2.0f * s), static_cast<int>(c.y + 6.0f * s), static_cast<int>(c.x - 2.0f * s), static_cast<int>(c.y + 9.0f * s), WHITE);
            DrawLine(static_cast<int>(c.x + 2.0f * s), static_cast<int>(c.y + 6.0f * s), static_cast<int>(c.x + 2.0f * s), static_cast<int>(c.y + 9.0f * s), WHITE);
            break;
        }

        case EnemyType::WallTurret: {
            // Механічна турель / бронзова гармата
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 8.5f * s, Color{ 110, 85, 60, 255 });
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 8.5f * s, GOLD);
            DrawRectangle(static_cast<int>(c.x + 2.0f * s), static_cast<int>(c.y - 3.0f * s), static_cast<int>(10.0f * s), static_cast<int>(6.0f * s), Color{ 68, 52, 38, 255 });
            DrawCircle(static_cast<int>(c.x - 2.0f * s), static_cast<int>(c.y), 3.0f * s, RED);
            break;
        }

        default:
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 8.0f * s, RED);
            break;
    }
}
