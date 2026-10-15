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

    // Статура за рисами (Ectomorph — вужчий витягнутий риг, Endomorph — ширший масивний)
    if (p.isEctomorph) {
        sx *= 0.82f;
        sy *= 1.04f;
    } else if (p.isEndomorph) {
        sx *= 1.22f;
        sy *= 0.95f;
    }

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

    // 1. Плащ з двома сегментами, що відстає при русі (або крила дракона)
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
        // Колір плаща за класом / тиром
        Color capeOuter = GetTierSecondaryColor(p.capeTier, pal.bloodCloth[2]);
        if (p.classType == ClassType::Mage_Base || p.classType == ClassType::Archmage_Upgraded) {
            capeOuter = pal.moonMagic[2];
        } else if (p.classType == ClassType::Shinobi_Base || p.classType == ClassType::Hokage_Upgraded) {
            capeOuter = pal.metal[1];
        } else if (p.classType == ClassType::Lich_Base || p.classType == ClassType::LichKing_Upgraded) {
            capeOuter = pal.poisonSlime[2];
        } else if (p.classType == ClassType::Barbarian_Base || p.classType == ClassType::BarbarianKing_Upgraded) {
            capeOuter = pal.wood[2];
        }

        Color capeInner = ColorTint(capeOuter, pal.metal[0]);
        Color capeHi    = ColorTint(capeOuter, pal.metal[4]);
        float speedRatio = std::clamp(std::abs(p.velocity.x) / 290.0f, 0.0f, 1.4f);
        float velLagY = p.velocity.y * 0.014f;
        constexpr int CAPE_NODES = 6;

        // Сегмент 1: Внутрішній нижній шар плаща (тіньовий)
        Vector2 prevInner = shoulderPos;
        for (int i = 1; i <= CAPE_NODES; ++i) {
            float t = static_cast<float>(i) / CAPE_NODES;
            float wave = std::sin(p.animTime * (7.5f + speedRatio * 6.0f) - i * 0.75f) * (4.0f + speedRatio * 5.5f) * t;
            float cx = shoulderPos.x - dir * (t * (20.0f + speedRatio * 20.0f) * sx);
            float cy = shoulderPos.y + t * (27.0f + speedRatio * 9.0f) * sy - velLagY * t + wave;
            Vector2 nextInner = { cx, cy };
            DrawLineEx(prevInner, nextInner, (6.0f + t * 6.5f) * ((sx + sy) * 0.5f), Fade(capeInner, alpha));
            prevInner = nextInner;
        }

        // Сегмент 2: Зовнішній верхній шар плаща (зі світловою окантовкою)
        Vector2 prevOuter = { shoulderPos.x + dir * 1.5f * sx, shoulderPos.y - 1.0f * sy };
        for (int i = 1; i <= CAPE_NODES; ++i) {
            float t = static_cast<float>(i) / CAPE_NODES;
            float wave = std::sin(p.animTime * (8.5f + speedRatio * 7.0f) - i * 0.85f + 0.45f) * (3.0f + speedRatio * 6.0f) * t;
            float cx = shoulderPos.x - dir * (t * (16.0f + speedRatio * 17.0f) * sx);
            float cy = shoulderPos.y + t * (23.0f + speedRatio * 7.5f) * sy - velLagY * t + wave;
            Vector2 nextOuter = { cx, cy };
            Color col = (i == 1 || i == 2) ? capeHi : capeOuter;
            DrawLineEx(prevOuter, nextOuter, (4.5f + t * 5.0f) * ((sx + sy) * 0.5f), Fade(col, alpha));
            prevOuter = nextOuter;
        }
    }

    // 2. Ноги з наколінниками та сабатонами (Sabaton boots) — 3-тональний cel-shading
    Color limbBase   = GetTierPrimaryColor(p.limbsTier, pal.metal[2]);
    Color limbShadow = ColorTint(limbBase, pal.metal[1]);
    Color limbHi     = ColorTint(limbBase, pal.metal[4]);
    float runCycle = (std::abs(p.velocity.x) > 15.0f) ? std::sin(p.animTime * 16.0f) * 8.0f * sx : 0.0f;
    float legW = 7.5f * sx;
    float legH = 14.5f * sy;

    auto DrawDetailedLeg = [&](float legX, float legY, float cycleOffset) {
        float curX = legX + cycleOffset;
        // Стегно (cuisse)
        DrawRectangleRec({ curX - legW * 0.5f, legY, legW * 0.6f, legH * 0.55f }, Fade(limbHi, alpha));
        DrawRectangleRec({ curX - legW * 0.5f + legW * 0.6f, legY, legW * 0.4f, legH * 0.55f }, Fade(limbShadow, alpha));
        // Наколінник (Poleyn) з рельєфною окантовкою
        DrawCircle(static_cast<int>(curX), static_cast<int>(legY + legH * 0.5f), 4.5f * sx, Fade(limbShadow, alpha));
        DrawCircle(static_cast<int>(curX - dir * 0.8f * sx), static_cast<int>(legY + legH * 0.5f - 0.8f * sy), 3.8f * sx, Fade(limbHi, alpha));
        DrawCircle(static_cast<int>(curX), static_cast<int>(legY + legH * 0.5f), 1.6f * sx, Fade(pal.fireGold[3], alpha)); // золота заклепка
        // Поножі (greaves) та сабатон (латний черевик)
        DrawRectangleRec({ curX - legW * 0.5f, legY + legH * 0.5f, legW * 0.65f, legH * 0.5f }, Fade(limbBase, alpha));
        DrawRectangleRec({ curX - legW * 0.5f + legW * 0.65f, legY + legH * 0.5f, legW * 0.35f, legH * 0.5f }, Fade(limbShadow, alpha));
        // Загострений носок сабатона
        DrawRectangleRec({ curX - legW * 0.5f + dir * 1.5f * sx, legY + legH - 3.5f * sy, legW + 3.0f * sx, 3.5f * sy }, Fade(limbShadow, alpha));
        DrawLineEx({ curX - legW * 0.5f + dir * 1.5f * sx, legY + legH - 3.5f * sy }, { curX + legW * 0.5f + dir * 3.0f * sx, legY + legH - 3.5f * sy }, 1.2f, Fade(limbHi, alpha));
    };

    DrawDetailedLeg(c.x - 4.5f * sx, c.y + 12.5f * sy, runCycle);
    DrawDetailedLeg(c.x + 4.5f * sx, c.y + 12.5f * sy, -runCycle);

    // 3. Тулуб у готичній кірасі з гербом, поясом та наплічниками
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

    Color chestBase   = GetTierPrimaryColor(p.chestTier, defaultTorso);
    Color chestShadow = ColorTint(chestBase, pal.metal[1]);
    Color chestHi     = ColorTint(chestBase, pal.metal[4]);
    Color trimCol     = GetTierSecondaryColor(p.chestTier, pal.fireGold[3]);

    Rectangle torsoRec = { c.x - bodyW * 0.5f, c.y - bodyH * 0.42f, bodyW, bodyH };
    // 3-тональний рельєф кіраси
    DrawRectangleRounded(torsoRec, 0.22f, 4, Fade(chestShadow, alpha));
    DrawRectangleRounded({ torsoRec.x, torsoRec.y, torsoRec.width * 0.65f, torsoRec.height }, 0.22f, 4, Fade(chestBase, alpha));
    DrawRectangleRounded({ torsoRec.x + 2.0f * sx, torsoRec.y + 2.0f * sy, torsoRec.width * 0.35f, torsoRec.height - 4.0f * sy }, 0.22f, 4, Fade(chestHi, alpha));
    DrawRectangleRoundedLines(torsoRec, 0.22f, 4, std::max(1.5f, 2.0f * sx), Fade(trimCol, alpha));

    // Центральне ребро жорсткості кіраси (Tapul)
    DrawLineEx(
        { c.x + dir * 2.0f * sx, torsoRec.y + 3.0f * sy },
        { c.x + dir * 2.0f * sx, torsoRec.y + bodyH - 6.0f * sy },
        2.0f * sx, Fade(trimCol, alpha)
    );

    // Герб на нагрудній пластині (Heraldic Coat of Arms)
    Vector2 crestCenter = { c.x - dir * 2.0f * sx, torsoRec.y + bodyH * 0.38f };
    if (p.classType == ClassType::Mage_Base || p.classType == ClassType::Archmage_Upgraded) {
        // Магічний півмісяць
        DrawCircleV(crestCenter, 3.5f * sx, Fade(pal.moonMagic[3], alpha));
        DrawCircleV({ crestCenter.x + 1.2f * sx, crestCenter.y }, 2.8f * sx, Fade(chestBase, alpha));
    } else if (p.classType == ClassType::Shinobi_Base || p.classType == ClassType::Hokage_Upgraded) {
        // Сюрікен / Ромб ніндзя
        DrawRectanglePro({ crestCenter.x, crestCenter.y, 4.5f * sx, 4.5f * sy }, { 2.2f * sx, 2.2f * sy }, 45.0f, Fade(pal.bloodCloth[3], alpha));
    } else if (p.classType == ClassType::Lich_Base || p.classType == ClassType::LichKing_Upgraded) {
        // Рунічний кристал душі
        DrawCircleV(crestCenter, 3.2f * sx, Fade(pal.poisonSlime[3], alpha));
        DrawCircleLines(static_cast<int>(crestCenter.x), static_cast<int>(crestCenter.y), 3.2f * sx, Fade(pal.fireGold[3], alpha));
    } else {
        // Лицарський золотий геральдичний хрест
        DrawLineEx({ crestCenter.x - 3.8f * sx, crestCenter.y }, { crestCenter.x + 3.8f * sx, crestCenter.y }, 1.8f * sy, Fade(pal.fireGold[3], alpha));
        DrawLineEx({ crestCenter.x, crestCenter.y - 4.5f * sy }, { crestCenter.x, crestCenter.y + 4.5f * sy }, 1.8f * sx, Fade(pal.fireGold[3], alpha));
    }

    // Золотий комір (Горжет)
    DrawRectangleRec({ c.x - 7.0f * sx, torsoRec.y, 14.0f * sx, 4.0f * sy }, Fade(trimCol, alpha));

    // Шкіряний пояс із пряжкою
    float beltY = torsoRec.y + bodyH - 6.0f * sy;
    DrawRectangleRec({ torsoRec.x + 2.0f * sx, beltY, bodyW - 4.0f * sx, 5.0f * sy }, Fade(pal.wood[0], alpha));
    DrawRectangleRec({ c.x + dir * 1.0f * sx - 3.5f * sx, beltY, 7.0f * sx, 5.0f * sy }, Fade(pal.fireGold[3], alpha));
    DrawRectangleRec({ c.x + dir * 1.0f * sx - 1.5f * sx, beltY + 1.0f * sy, 3.0f * sx, 3.0f * sy }, Fade(pal.metal[1], alpha)); // рамка пряжки

    // Наплічники з окантовкою (Pauldrons)
    auto DrawPauldron = [&](Vector2 pPos) {
        DrawCircle(static_cast<int>(pPos.x), static_cast<int>(pPos.y), 5.8f * sx, Fade(chestShadow, alpha));
        DrawCircle(static_cast<int>(pPos.x - dir * 1.0f * sx), static_cast<int>(pPos.y - 1.0f * sy), 5.0f * sx, Fade(chestHi, alpha));
        DrawCircleLines(static_cast<int>(pPos.x), static_cast<int>(pPos.y), 5.8f * sx, Fade(trimCol, alpha));
        DrawCircle(static_cast<int>(pPos.x), static_cast<int>(pPos.y), 1.8f * sx, Fade(trimCol, alpha));
    };
    DrawPauldron({ c.x - dir * 10.0f * sx, torsoRec.y + 3.0f * sy });
    DrawPauldron({ c.x + dir * 9.0f * sx, torsoRec.y + 4.0f * sy });

    // 4. Голова і шолом-бацинет із загостреним забралом (houndskull)
    Color helmBase   = GetTierPrimaryColor(p.helmTier, pal.metal[3]);
    Color helmShadow = ColorTint(helmBase, pal.metal[1]);
    Color helmHi     = ColorTint(helmBase, pal.metal[4]);
    Color plumeCol   = GetTierSecondaryColor(p.helmTier, pal.bloodCloth[3]);

    if (p.classType == ClassType::Paladin_Upgraded) {
        plumeCol = pal.fireGold[3];
    } else if (p.classType == ClassType::Mage_Base || p.classType == ClassType::Archmage_Upgraded) {
        plumeCol = pal.moonMagic[3];
    } else if (p.classType == ClassType::Lich_Base || p.classType == ClassType::LichKing_Upgraded) {
        plumeCol = pal.poisonSlime[3];
    }

    // Купол бацинета (конічний купол шолома)
    DrawCircleV(headPos, headR, Fade(helmShadow, alpha));
    DrawCircleSector(headPos, headR, 90.0f, 270.0f, 12, Fade(helmBase, alpha));
    DrawCircle(static_cast<int>(headPos.x - dir * 2.8f * sx), static_cast<int>(headPos.y - headR * 0.45f), headR * 0.42f, Fade(helmHi, alpha * 0.65f));

    // Гребінь шолома: форма залежить від статі (Sir / Lady)
    if (p.isFemale) {
        // Lady: витончена крилата корона-гребінь
        Vector2 crestL = { headPos.x - dir * 5.0f * sx, headPos.y - headR * 1.35f };
        Vector2 crestR = { headPos.x + dir * 8.0f * sx, headPos.y - headR * 0.95f };
        DrawTriangle({ headPos.x, headPos.y - headR * 0.6f }, crestL, { headPos.x, headPos.y - headR * 0.95f }, Fade(pal.fireGold[3], alpha));
        DrawTriangle({ headPos.x, headPos.y - headR * 0.6f }, { headPos.x, headPos.y - headR * 0.95f }, crestR, Fade(pal.fireGold[2], alpha));
    } else {
        // Sir: гострий бойовий гребінь бацинета
        Vector2 spineTop = { headPos.x - dir * 2.0f * sx, headPos.y - headR * 1.35f };
        DrawTriangle(
            { headPos.x - dir * 6.0f * sx, headPos.y - headR * 0.75f },
            spineTop,
            { headPos.x + dir * 6.0f * sx, headPos.y - headR * 0.75f },
            Fade(helmHi, alpha)
        );
        DrawLineEx({ headPos.x - dir * 2.0f * sx, headPos.y - headR * 0.75f }, spineTop, 1.8f * sx, Fade(pal.metal[1], alpha));
    }

    // Загострене забрало (Houndskull / Pointed Visor)
    Vector2 visorApex = { headPos.x + dir * (headR * 1.15f), headPos.y + headR * 0.15f };
    Vector2 visorTop  = { headPos.x + dir * (headR * 0.25f), headPos.y - headR * 0.35f };
    Vector2 visorBot  = { headPos.x + dir * (headR * 0.30f), headPos.y + headR * 0.65f };
    DrawTriangle(visorTop, visorApex, visorBot, Fade(helmBase, alpha));
    DrawLineEx(visorTop, visorApex, 1.5f * sx, Fade(helmHi, alpha));
    DrawLineEx(visorBot, visorApex, 1.5f * sx, Fade(helmShadow, alpha));

    // Вузька очна щілина шолома з палаючим вогником
    Vector2 slitStart = { headPos.x + dir * (headR * 0.35f), headPos.y - headR * 0.05f };
    Vector2 slitEnd   = { headPos.x + dir * (headR * 0.85f), headPos.y + headR * 0.05f };
    DrawLineEx(slitStart, slitEnd, 2.4f * sy, Fade(pal.outline, alpha));

    // Світло очей крізь забрало
    Vector2 eyeFirePos = { slitStart.x + (slitEnd.x - slitStart.x) * 0.55f, slitStart.y + (slitEnd.y - slitStart.y) * 0.55f };
    if (p.classType == ClassType::Lich_Base || p.classType == ClassType::LichKing_Upgraded) {
        DrawCircleV(eyeFirePos, 2.5f * sx, Fade(pal.poisonSlime[3], alpha));
        DrawCircleV(eyeFirePos, 1.2f * sx, Fade(pal.metal[4], alpha));
    } else {
        DrawCircleV(eyeFirePos, 2.2f * sx, Fade(pal.fireGold[3], alpha));
        DrawCircleV(eyeFirePos, 1.0f * sx, Fade(pal.metal[4], alpha));
    }

    // Вентиляційні отвори на "носі" забрала
    for (int vs = 0; vs < 3; ++vs) {
        float vt = 0.45f + vs * 0.16f;
        Vector2 slitHole = { visorBot.x + (visorApex.x - visorBot.x) * vt, visorBot.y + (visorApex.y - visorBot.y) * vt - 2.0f * sy };
        DrawCircle(static_cast<int>(slitHole.x), static_cast<int>(slitHole.y), 0.9f * sx, Fade(pal.outline, alpha * 0.85f));
    }

    // Класові головні убори та багатосегментний плюмаж (Spring inertia)
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
            if (!p.isBald) {
                // Багатосегментний плюмаж лицаря з інерційним хитанням (Spring inertia)
                float springLag = (p.velocity.x * -0.0035f);
                Vector2 plumeRoot = { headPos.x - dir * 1.0f * sx, headPos.y - headR * 0.98f };
                constexpr int PLUME_SEGS = 4;
                Vector2 prevPlume = plumeRoot;

                for (int pl = 1; pl <= PLUME_SEGS; ++pl) {
                    float t = static_cast<float>(pl) / PLUME_SEGS;
                    float pWave = std::sin(p.animTime * (10.0f + pl * 1.2f) - pl * 0.7f) * (3.5f + pl * 1.8f) * sy;
                    float px = plumeRoot.x - dir * (t * 22.0f * sx) + springLag * (pl * 4.0f);
                    float py = plumeRoot.y - (1.0f - t) * 6.0f * sy + (t * 8.0f * sy) + pWave - (p.velocity.y * 0.010f * t);
                    Vector2 nextPlume = { px, py };
                    float thick = (5.5f - pl * 0.85f) * ((sx + sy) * 0.5f);
                    Color segCol = (pl % 2 == 0) ? plumeCol : ColorTint(plumeCol, pal.fireGold[3]);
                    DrawLineEx(prevPlume, nextPlume, thick, Fade(segCol, alpha));
                    DrawCircleV(nextPlume, thick * 0.45f, Fade(segCol, alpha));
                    prevPlume = nextPlume;
                }
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

        // Дуговий шлейф удару меча (Slash Arc Fan): 8 трикутних сегментів у BLEND_ADDITIVE
        float baseAngle = -85.0f;
        int arcSegments = 8;
        float stepA = (swingAngleDeg - baseAngle) / arcSegments;
        if (stepA > 0.5f) {
            BeginBlendMode(BLEND_ADDITIVE);
            for (int i = 0; i < arcSegments; ++i) {
                float a1 = (baseAngle + i * stepA) * (3.14159265f / 180.0f);
                float a2 = (baseAngle + (i + 1) * stepA) * (3.14159265f / 180.0f);
                float tFrac = static_cast<float>(i + 1) / static_cast<float>(arcSegments);
                float arcAlpha = std::pow(tFrac, 1.4f) * 0.75f;

                Vector2 inner1 = { handPos.x + dir * std::cos(a1) * (bladeLen * 0.40f), handPos.y + std::sin(a1) * (bladeLen * 0.40f) };
                Vector2 inner2 = { handPos.x + dir * std::cos(a2) * (bladeLen * 0.40f), handPos.y + std::sin(a2) * (bladeLen * 0.40f) };
                Vector2 outer1 = { handPos.x + dir * std::cos(a1) * (bladeLen * 1.10f), handPos.y + std::sin(a1) * (bladeLen * 1.10f) };
                Vector2 outer2 = { handPos.x + dir * std::cos(a2) * (bladeLen * 1.10f), handPos.y + std::sin(a2) * (bladeLen * 1.10f) };

                Color arcColor = bladeCol;
                if (p.swordTier == EquipTier::Imperial) arcColor = pal.fireGold[3];
                else if (p.swordTier == EquipTier::Royal) arcColor = pal.moonMagic[3];
                else if (p.swordTier == EquipTier::Dark) arcColor = pal.bloodCloth[3];

                DrawTriangle(inner1, outer1, outer2, Fade(arcColor, arcAlpha * 0.65f));
                DrawTriangle(inner1, outer2, inner2, Fade(arcColor, arcAlpha * 0.65f));
                DrawLineEx(outer1, outer2, 2.5f * sx, Fade(pal.metal[4], arcAlpha));
            }
            EndBlendMode();
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
        } else if (t == TraitType::Baldness) {
            hp.isBald = true;
        } else if (t == TraitType::Vertigo) {
            hp.scaleY = -hp.scaleY;
        }
    }

    hp.facingDir = 1.0f;
    hp.isFemale = heir.isFemale;
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

    // Готичні геральдичні гобелени / штандарти з ніжним колиханням (Tapestry sway)
    float timeSec = static_cast<float>(GetTime());
    for (int i = 0; i < 2; ++i) {
        int tx = 450 + i * 380;
        int ty = 230 + ((seed + i * 3) % 2) * 50;
        float sway = std::sin(timeSec * 2.2f + i * 1.57f) * 2.5f;

        // Кована планка / карниз гобелена
        DrawLineEx({ tx - 28.0f, static_cast<float>(ty) }, { tx + 28.0f, static_cast<float>(ty) }, 3.5f, pal.fireGold[3]);
        DrawCircle(tx - 28, ty, 3.0f, pal.fireGold[4]);
        DrawCircle(tx + 28, ty, 3.0f, pal.fireGold[4]);

        // Полотно гобелена з плавним зсувом низу від протягу в замку
        Color tapColor = (biome == BiomeType::CastleHamson || biome == BiomeType::TheMaya) ? pal.bloodCloth[1] : 
                         (biome == BiomeType::ForestAbkhazia ? pal.poisonSlime[1] : pal.dungeon.stoneTile[1]);
        Color tapTrim  = pal.fireGold[2];

        Vector2 topL = { tx - 22.0f, static_cast<float>(ty + 2) };
        Vector2 topR = { tx + 22.0f, static_cast<float>(ty + 2) };
        Vector2 botL = { tx - 22.0f + sway, static_cast<float>(ty + 95) };
        Vector2 botR = { tx + 22.0f + sway, static_cast<float>(ty + 95) };
        Vector2 tipM = { tx + sway * 1.2f, static_cast<float>(ty + 115) }; // V-подібний нижній кут

        DrawTriangle(topL, botL, topR, Fade(tapColor, 0.40f));
        DrawTriangle(topR, botL, botR, Fade(tapColor, 0.40f));
        DrawTriangle(botL, tipM, botR, Fade(tapColor, 0.40f));

        // Геральдичний золотий хрест / емблема
        DrawLineEx({ tx - 10.0f + sway * 0.4f, static_cast<float>(ty + 40) }, { tx + 10.0f + sway * 0.4f, static_cast<float>(ty + 40) }, 2.0f, Fade(tapTrim, 0.45f));
        DrawLineEx({ tx + sway * 0.4f, static_cast<float>(ty + 25) }, { tx + sway * 0.4f, static_cast<float>(ty + 65) }, 2.0f, Fade(tapTrim, 0.45f));
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

namespace {
    inline uint32_t SeedHash(uint32_t x) {
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        return x;
    }

    inline float Noise1D(float x) {
        int i0 = static_cast<int>(std::floor(x));
        int i1 = i0 + 1;
        float f = x - static_cast<float>(i0);
        float u = f * f * (3.0f - 2.0f * f);
        float h0 = static_cast<float>(SeedHash(static_cast<uint32_t>(i0)) & 0xFFFF) / 65535.0f;
        float h1 = static_cast<float>(SeedHash(static_cast<uint32_t>(i1)) & 0xFFFF) / 65535.0f;
        return (1.0f - u) * h0 + u * h1;
    }

    inline float Fbm1D(float x, int octaves = 4) {
        float val = 0.0f;
        float amp = 0.5f;
        float freq = 1.0f;
        for (int i = 0; i < octaves; ++i) {
            val += Noise1D(x * freq) * amp;
            freq *= 2.0f;
            amp *= 0.5f;
        }
        return val;
    }

    struct StarDef {
        float x;
        float y;
        float size;
        float speed;
        float seed;
    };

    const std::vector<StarDef>& GetGothicSkyStars() {
        static std::vector<StarDef> stars = []() {
            std::vector<StarDef> res;
            res.reserve(120);
            for (int i = 0; i < 120; ++i) {
                uint32_t h = TileHash(i * 37 + 101, i * 83 + 47, 9823);
                float x = static_cast<float>(h % 1270 + 5);
                float y = static_cast<float>((h >> 11) % 360 + 15);
                float size = ((h >> 4) % 10 < 3) ? 2.0f : 1.0f;
                float speed = 1.8f + static_cast<float>((h >> 7) % 30) * 0.1f;
                float seed = static_cast<float>((h >> 15) % 628) * 0.01f;
                res.push_back({ x, y, size, speed, seed });
            }
            return res;
        }();
        return stars;
    }

    void DrawGothicMoonDisk(float moonX, float moonY) {
        float rDisk = 32.0f;
        Color moonBase = Color{ 232, 240, 255, 255 };
        DrawCircle(static_cast<int>(moonX), static_cast<int>(moonY), rDisk, moonBase);

        // Crater details
        struct Crater { float ox; float oy; float r; };
        const Crater craters[] = {
            { -10.0f, -6.0f, 6.0f },
            {  12.0f, -4.0f, 7.5f },
            {  -4.0f, 10.0f, 5.0f },
            {   8.0f, 12.0f, 4.0f },
            {   2.0f, -14.0f, 4.5f },
            { -14.0f,  8.0f, 3.5f }
        };
        for (const auto& c : craters) {
            Vector2 cp = { moonX + c.ox, moonY + c.oy };
            DrawCircleV(cp, c.r, Color{ 176, 194, 232, 190 });
            DrawCircleSector(cp, c.r, 135.0f, 315.0f, 8, Color{ 255, 255, 255, 140 });
        }

        // Crescent shadow on lower-right edge
        DrawCircleSector({ moonX + 3.0f, moonY + 3.0f }, rDisk - 1.0f, 330.0f, 110.0f, 16, Color{ 32, 38, 65, 80 });
    }

    void DrawGothicClouds(float time) {
        Color cloudBody = Color{ 28, 34, 60, 220 };
        Color rimLight  = Color{ 127, 176, 240, 190 };

        struct CloudStrand {
            float y;
            float speed;
            float width;
            float height;
            float offset;
        };
        const CloudStrand strands[] = {
            {  55.0f,  6.0f, 240.0f, 14.0f,   80.0f },
            {  85.0f,  8.0f, 310.0f, 18.0f,  650.0f },
            { 130.0f, 12.0f, 260.0f, 16.0f,  200.0f },
            { 165.0f, 11.0f, 360.0f, 22.0f,  980.0f },
            { 205.0f, 15.0f, 280.0f, 18.0f,  450.0f },
            { 245.0f, 18.0f, 340.0f, 24.0f, 1200.0f }
        };

        for (const auto& cs : strands) {
            float cx = std::fmod(time * cs.speed + cs.offset, 1600.0f) - 250.0f;
            Rectangle r = { cx, cs.y, cs.width, cs.height };
            DrawRectangleRounded(r, 0.45f, 6, cloudBody);
            DrawRectangleRounded({ cx + cs.width * 0.15f, cs.y - cs.height * 0.25f, cs.width * 0.5f, cs.height * 0.8f }, 0.45f, 6, cloudBody);
            // Cold moonlight rim light along upper contour
            DrawLineEx({ cx + 12.0f, cs.y - 1.0f }, { cx + cs.width - 12.0f, cs.y - 1.0f }, 1.5f, rimLight);
            DrawLineEx({ cx + cs.width * 0.18f, cs.y - cs.height * 0.25f }, { cx + cs.width * 0.62f, cs.y - cs.height * 0.25f }, 1.5f, rimLight);
        }
    }

    void DrawGothicFog(float time) {
        Color fog1 = Color{ 65, 75, 125, 24 };
        Color fog2 = Color{ 50, 58, 100, 34 };
        Color fog3 = Color{ 42, 35,  75, 30 };

        float off1 = std::fmod(time * 14.0f, 1280.0f);
        DrawRectangleGradientV(0, 420, 1280, 80, BLANK, fog1);
        DrawRectangle(0, 450, 1280, 40, fog1);
        for (int i = -1; i < 3; ++i) {
            float fx = i * 600.0f + off1;
            DrawEllipse(static_cast<int>(fx), 480, 300.0f, 35.0f, fog1);
        }

        float off2 = std::fmod(time * 22.0f, 1280.0f);
        for (int i = -1; i < 3; ++i) {
            float fx = i * 480.0f + off2;
            DrawEllipse(static_cast<int>(fx), 515, 220.0f, 26.0f, fog2);
        }

        float off3 = std::fmod(time * 34.0f, 1280.0f);
        for (int i = -1; i < 3; ++i) {
            float fx = i * 520.0f + off3;
            DrawEllipse(static_cast<int>(fx), 555, 260.0f, 30.0f, fog3);
        }
    }

    void DrawGothicPine(float x, float baseY, float h, float w) {
        Color woodDark = Color{ 12, 15, 29, 255 };
        DrawRectangle(static_cast<int>(x - 3.0f), static_cast<int>(baseY - 14.0f), 6, 14, woodDark);
        int tiers = 4;
        float tierH = (h - 14.0f) / tiers;
        for (int i = 0; i < tiers; ++i) {
            float frac = static_cast<float>(i) / (tiers - 1);
            float tw = w * (1.0f - frac * 0.5f);
            float by = baseY - 10.0f - i * tierH * 0.75f;
            float ty = by - tierH * 1.35f;
            DrawTriangle({ x, ty }, { x - tw * 0.5f, by }, { x + tw * 0.5f, by }, woodDark);
        }
    }

    void DrawGothicTwistedTree(float x, float baseY, float h, float w) {
        Color woodDark = Color{ 12, 15, 29, 255 };
        Vector2 p0 = { x, baseY };
        Vector2 p1 = { x - 4.0f, baseY - h * 0.35f };
        Vector2 p2 = { x + 6.0f, baseY - h * 0.70f };
        Vector2 p3 = { x + 2.0f, baseY - h };
        DrawLineEx(p0, p1, 7.0f, woodDark);
        DrawLineEx(p1, p2, 5.0f, woodDark);
        DrawLineEx(p2, p3, 3.5f, woodDark);

        DrawLineEx(p1, { p1.x - w * 0.45f, p1.y - h * 0.25f }, 3.5f, woodDark);
        DrawLineEx({ p1.x - w * 0.45f, p1.y - h * 0.25f }, { p1.x - w * 0.65f, p1.y - h * 0.40f }, 2.0f, woodDark);

        DrawLineEx(p2, { p2.x + w * 0.5f, p2.y - h * 0.2f }, 3.0f, woodDark);
        DrawLineEx({ p2.x + w * 0.5f, p2.y - h * 0.2f }, { p2.x + w * 0.75f, p2.y - h * 0.35f }, 1.8f, woodDark);

        DrawLineEx(p3, { p3.x - w * 0.25f, p3.y - h * 0.15f }, 2.0f, woodDark);
        DrawLineEx(p3, { p3.x + w * 0.3f, p3.y - h * 0.18f }, 2.0f, woodDark);
    }

    void DrawSwayingGrass(float groundY, float time) {
        Color grassDark1 = Color{ 22, 29, 26, 255 };
        Color grassDark2 = Color{ 14, 20, 18, 255 };
        for (int gx = 8; gx < Config::VIRTUAL_WIDTH; gx += 16) {
            float sway = std::sin(time * 3.5f + gx * 0.08f) * 4.5f;
            DrawLine(gx, static_cast<int>(groundY), static_cast<int>(gx + sway - 3.0f), static_cast<int>(groundY - 10.0f), grassDark1);
            DrawLine(gx + 3, static_cast<int>(groundY), static_cast<int>(gx + sway + 4.0f), static_cast<int>(groundY - 14.0f), grassDark2);
            DrawLine(gx + 6, static_cast<int>(groundY), static_cast<int>(gx + sway + 1.0f), static_cast<int>(groundY - 8.0f), grassDark1);
        }
    }

    void DrawGothicLantern(float x, float groundY, float time) {
        Color ironCol = Color{ 36, 40, 52, 255 };
        Color ironHi  = Color{ 68, 75, 96, 255 };

        DrawRectangle(static_cast<int>(x - 3.0f), static_cast<int>(groundY - 88.0f), 6, 88, ironCol);
        DrawLine(static_cast<int>(x - 3.0f), static_cast<int>(groundY - 88.0f), static_cast<int>(x - 3.0f), static_cast<int>(groundY), ironHi);
        DrawTriangle({ x - 9.0f, groundY }, { x, groundY - 14.0f }, { x + 9.0f, groundY }, ironCol);

        int ly = static_cast<int>(groundY - 102.0f);
        DrawRectangle(static_cast<int>(x - 9.0f), ly, 18, 16, Color{ 25, 20, 15, 230 });
        DrawRectangleLines(static_cast<int>(x - 9.0f), ly, 18, 16, ironCol);
        DrawLine(static_cast<int>(x), ly, static_cast<int>(x), ly + 16, ironCol);

        DrawTriangle({ x - 11.0f, static_cast<float>(ly) }, { x, static_cast<float>(ly - 8) }, { x + 11.0f, static_cast<float>(ly) }, ironCol);
        DrawCircle(static_cast<int>(x), ly - 9, 2.0f, ironHi);

        float flicker = 0.85f + 0.15f * std::sin(time * 11.0f + x);
        DrawCircle(static_cast<int>(x), ly + 8, 4.5f * flicker, Color{ 255, 179, 71, 255 });
        DrawCircle(static_cast<int>(x), ly + 8, 2.0f * flicker, Color{ 255, 240, 180, 255 });

        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleGradient(static_cast<int>(x), ly + 8, 72.0f * flicker, Color{ 255, 179, 71, 52 }, Color{ 255, 179, 71, 0 });
        EndBlendMode();
    }

    void DrawGothicFenceAndGraves(float groundY) {
        Color ironCol   = Color{ 27, 29, 38, 255 };
        Color ironHi    = Color{ 46, 49, 63, 255 };
        Color stoneHead = Color{ 23, 29, 54, 255 };
        Color stoneRim  = Color{ 38, 45, 71, 255 };

        auto DrawFenceSection = [&](int startX, int endX) {
            DrawLineEx({ static_cast<float>(startX), groundY - 28.0f }, { static_cast<float>(endX), groundY - 28.0f }, 2.5f, ironCol);
            DrawLineEx({ static_cast<float>(startX), groundY - 10.0f }, { static_cast<float>(endX), groundY - 10.0f }, 2.5f, ironCol);
            for (int fx = startX + 6; fx < endX; fx += 14) {
                DrawLine(fx, static_cast<int>(groundY), fx, static_cast<int>(groundY - 38.0f), ironCol);
                DrawLine(fx + 1, static_cast<int>(groundY), fx + 1, static_cast<int>(groundY - 38.0f), ironHi);
                DrawTriangle({ static_cast<float>(fx - 3), groundY - 38.0f },
                             { static_cast<float>(fx), groundY - 45.0f },
                             { static_cast<float>(fx + 3), groundY - 38.0f }, ironHi);
            }
        };

        DrawFenceSection(40, 240);
        DrawFenceSection(620, 750);

        auto DrawCross = [&](float cx, float cy, float h) {
            float w = h * 0.65f;
            DrawRectangle(static_cast<int>(cx - 3.0f), static_cast<int>(cy - h), 6, static_cast<int>(h), stoneHead);
            DrawRectangle(static_cast<int>(cx - w * 0.5f), static_cast<int>(cy - h * 0.72f), static_cast<int>(w), 5, stoneHead);
            DrawLine(static_cast<int>(cx - 3.0f), static_cast<int>(cy - h), static_cast<int>(cx - 3.0f), static_cast<int>(cy), stoneRim);
            DrawLine(static_cast<int>(cx - w * 0.5f), static_cast<int>(cy - h * 0.72f), static_cast<int>(cx + w * 0.5f), static_cast<int>(cy - h * 0.72f), stoneRim);
        };
        auto DrawStele = [&](float sx, float sy, float w, float h) {
            DrawRectangle(static_cast<int>(sx - w * 0.5f), static_cast<int>(sy - h + w * 0.5f), static_cast<int>(w), static_cast<int>(h - w * 0.5f), stoneHead);
            DrawCircle(static_cast<int>(sx), static_cast<int>(sy - h + w * 0.5f), w * 0.5f, stoneHead);
            DrawCircleLines(static_cast<int>(sx), static_cast<int>(sy - h + w * 0.5f), w * 0.5f, stoneRim);
            DrawLine(static_cast<int>(sx - w * 0.5f), static_cast<int>(sy - h + w * 0.5f), static_cast<int>(sx - w * 0.5f), static_cast<int>(sy), stoneRim);
        };

        DrawCross(280.0f, groundY, 42.0f);
        DrawStele(325.0f, groundY, 18.0f, 32.0f);
        DrawCross(570.0f, groundY, 36.0f);
        DrawStele(790.0f, groundY, 22.0f, 38.0f);
    }

    void DrawStaticCastleStructure(const GameContext& ctx, float castleCenterX, int baseY) {
        const auto& pal = GetPalette();
        Color stoneShadow  = Color{ 22, 25, 40, 255 }; // #161928
        Color stoneMoonlit = Color{ 56, 65, 97, 255 }; // #384161
        Color outlineCol   = pal.outline;
        Color roofBase     = Color{ 23, 28, 46, 255 }; // #171C2E
        Color roofMoonlit  = Color{ 52, 62, 96, 255 }; // #343E60
        Color goldAccent   = Color{ 255, 184, 77, 255 };

        int keepLvl = ctx.GetManorLevel(ManorNodeId::Stat_Health) + ctx.GetManorLevel(ManorNodeId::Stat_Mana);
        int keepH = 175 + std::min(160, keepLvl * 6);
        int keepW = 150;
        int keepLeft = static_cast<int>(castleCenterX - keepW * 0.5f);

        // 1. Цоколь (Flared base plinth)
        DrawRectangle(keepLeft - 8, baseY - 16, keepW + 16, 16, stoneShadow);
        DrawLine(keepLeft - 8, baseY - 16, keepLeft + keepW + 8, baseY - 16, stoneMoonlit);
        DrawLine(keepLeft - 8, baseY - 16, keepLeft - 8, baseY, stoneMoonlit);

        // 2. Головна Цитадель (Central Keep): ліва місячна грань, права тіньова
        int halfW = keepW / 2;
        DrawRectangle(keepLeft, baseY - keepH, halfW, keepH, stoneMoonlit);
        DrawRectangle(keepLeft + halfW, baseY - keepH, halfW, keepH, stoneShadow);
        DrawRectangleLines(keepLeft, baseY - keepH, keepW, keepH, outlineCol);
        DrawLine(keepLeft + halfW, baseY - keepH, keepLeft + halfW, baseY, outlineCol);

        // Контрфорси (Buttresses)
        int buttH = keepH / 2;
        DrawTriangle({ static_cast<float>(keepLeft - 18), static_cast<float>(baseY) },
                     { static_cast<float>(keepLeft), static_cast<float>(baseY - buttH) },
                     { static_cast<float>(keepLeft), static_cast<float>(baseY) }, stoneMoonlit);
        DrawLine(keepLeft - 18, baseY, keepLeft, baseY - buttH, stoneMoonlit);

        DrawTriangle({ static_cast<float>(keepLeft + keepW), static_cast<float>(baseY) },
                     { static_cast<float>(keepLeft + keepW), static_cast<float>(baseY - buttH) },
                     { static_cast<float>(keepLeft + keepW + 18), static_cast<float>(baseY) }, stoneShadow);
        DrawLine(keepLeft + keepW + 18, baseY, keepLeft + keepW, baseY - buttH, outlineCol);

        // Горизонтальні шви кам'яної кладки
        for (int y = baseY - keepH + 20; y < baseY; y += 22) {
            DrawLine(keepLeft, y, keepLeft + keepW, y, outlineCol);
            DrawLine(keepLeft, y + 1, keepLeft + halfW, y + 1, ColorAlpha(stoneMoonlit, 0.4f));
        }

        // Зубці з машикулями (Machicolated Battlements)
        // Машикулі (консольний ряд під парапетом)
        int machY = baseY - keepH;
        DrawRectangle(keepLeft - 6, machY, keepW + 12, 6, stoneShadow);
        DrawLine(keepLeft - 6, machY, keepLeft + keepW + 6, machY, stoneMoonlit);
        for (int bx = keepLeft - 6; bx < keepLeft + keepW + 6; bx += 14) {
            DrawRectangle(bx, machY + 2, 4, 8, stoneShadow);
            DrawLine(bx, machY + 2, bx, machY + 10, outlineCol);
        }

        // Зубці парапету
        for (int bx = keepLeft - 6; bx < keepLeft + keepW + 6; bx += 20) {
            DrawRectangle(bx, machY - 14, 13, 14, (bx < keepLeft + halfW) ? stoneMoonlit : stoneShadow);
            DrawRectangleLines(bx, machY - 14, 13, 14, outlineCol);
            // Темна бійниця (arrow slit)
            DrawRectangle(bx + 5, machY - 10, 3, 7, outlineCol);
        }

        // Гостроверхий готичний дах
        Vector2 roofApex = { castleCenterX, static_cast<float>(machY - 95) };
        DrawTriangle({ static_cast<float>(keepLeft - 8), static_cast<float>(machY - 14) },
                     roofApex,
                     { static_cast<float>(castleCenterX), static_cast<float>(machY - 14) }, roofMoonlit);
        DrawTriangle({ static_cast<float>(castleCenterX), static_cast<float>(machY - 14) },
                     roofApex,
                     { static_cast<float>(keepLeft + keepW + 8), static_cast<float>(machY - 14) }, roofBase);
        DrawLineEx({ static_cast<float>(keepLeft - 8), static_cast<float>(machY - 14) }, roofApex, 2.0f, stoneMoonlit);
        DrawLineEx({ static_cast<float>(keepLeft + keepW + 8), static_cast<float>(machY - 14) }, roofApex, 2.0f, outlineCol);

        // Шпиль і флюгер (Weathervane)
        Vector2 spireTop = { roofApex.x, roofApex.y - 28.0f };
        DrawLineEx(roofApex, spireTop, 3.0f, goldAccent);
        DrawCircleV(spireTop, 3.5f, goldAccent);

        // 3. Стрілчасті вікна (Lancet Windows)
        for (int wy = baseY - 50; wy > baseY - keepH + 35; wy -= 44) {
            for (float wx : { castleCenterX - 45.0f, castleCenterX + 25.0f }) {
                int ix = static_cast<int>(wx);
                DrawRectangle(ix, wy, 16, 24, goldAccent);
                DrawCircle(ix + 8, wy, 8.0f, goldAccent);
                DrawRectangleLines(ix, wy, 16, 24, outlineCol);
                DrawLine(ix + 8, wy - 8, ix + 8, wy + 24, outlineCol);
                DrawLine(ix, wy + 10, ix + 16, wy + 10, outlineCol);
            }
        }

        // 4. Ліве військове крило (Left Wing)
        int leftLvl = ctx.GetManorLevel(ManorNodeId::Stat_Attack)
                    + ctx.GetManorLevel(ManorNodeId::Stat_Armor)
                    + ctx.GetManorLevel(ManorNodeId::Class_Barbarian) * 5
                    + ctx.GetManorLevel(ManorNodeId::Class_Shinobi) * 5;
        if (leftLvl > 0) {
            int lH = 120 + std::min(180, leftLvl * 7);
            int lW = 100;
            int lLeft = keepLeft - lW + 8;
            DrawRectangle(lLeft, baseY - lH, lW, lH, stoneMoonlit);
            DrawRectangleLines(lLeft, baseY - lH, lW, lH, outlineCol);

            // Контрфорс
            DrawTriangle({ static_cast<float>(lLeft - 16), static_cast<float>(baseY) },
                         { static_cast<float>(lLeft), static_cast<float>(baseY - lH / 2) },
                         { static_cast<float>(lLeft), static_cast<float>(baseY) }, stoneMoonlit);

            // Зубці
            for (int bx = lLeft; bx < lLeft + lW; bx += 18) {
                DrawRectangle(bx, baseY - lH - 12, 12, 12, stoneMoonlit);
                DrawRectangleLines(bx, baseY - lH - 12, 12, 12, outlineCol);
            }
            // Вікна
            for (int wy = baseY - 55; wy > baseY - lH + 25; wy -= 45) {
                DrawRectangle(lLeft + 42, wy, 14, 22, goldAccent);
                DrawCircle(lLeft + 49, wy, 7.0f, goldAccent);
                DrawRectangleLines(lLeft + 42, wy, 14, 22, outlineCol);
            }
        }

        // 5. Праве магічне крило (Right Wing)
        int rightLvl = ctx.GetManorLevel(ManorNodeId::Stat_MagicDmg)
                     + ctx.GetManorLevel(ManorNodeId::Class_Mage) * 5
                     + ctx.GetManorLevel(ManorNodeId::Class_Lich) * 5;
        if (rightLvl > 0) {
            int rH = 130 + std::min(190, rightLvl * 7);
            int rW = 95;
            int rLeft = keepLeft + keepW - 8;
            DrawRectangle(rLeft, baseY - rH, rW, rH, stoneShadow);
            DrawRectangleLines(rLeft, baseY - rH, rW, rH, outlineCol);

            // Готичний шатровий дах вежі магів
            Vector2 rApex = { static_cast<float>(rLeft + rW / 2), static_cast<float>(baseY - rH - 85) };
            DrawTriangle({ static_cast<float>(rLeft - 4), static_cast<float>(baseY - rH) },
                         rApex,
                         { static_cast<float>(rLeft + rW + 4), static_cast<float>(baseY - rH) }, roofBase);
            DrawLineEx({ static_cast<float>(rLeft - 4), static_cast<float>(baseY - rH) }, rApex, 1.5f, stoneMoonlit);

            // Вікна
            for (int wy = baseY - 55; wy > baseY - rH + 25; wy -= 45) {
                DrawRectangle(rLeft + 38, wy, 14, 22, Color{ 140, 220, 255, 255 });
                DrawCircle(rLeft + 45, wy, 7.0f, Color{ 140, 220, 255, 255 });
                DrawRectangleLines(rLeft + 38, wy, 14, 22, outlineCol);
            }
        }

        // 6. Обсерваторія Зірок (Master Observatory)
        if (ctx.GetManorLevel(ManorNodeId::Upgrade_AllBasicClasses) > 0) {
            int topY = machY - 95;
            DrawCircle(static_cast<int>(castleCenterX), topY, 32.0f, Color{ 35, 45, 75, 255 });
            DrawCircleLines(static_cast<int>(castleCenterX), topY, 32.0f, goldAccent);
            DrawLineEx({ castleCenterX, static_cast<float>(topY) },
                       { castleCenterX + 38.0f, static_cast<float>(topY - 32) }, 6.0f, goldAccent);
            DrawCircle(static_cast<int>(castleCenterX + 40.0f), topY - 34, 5.0f, Color{ 140, 220, 255, 255 });
        }

        // 7. Гніздо Дракона (Dragon Aerie)
        if (ctx.GetManorLevel(ManorNodeId::Class_Dragon) > 0) {
            int topY = machY - 130;
            DrawTriangle({ castleCenterX - 28.0f, static_cast<float>(topY + 14) },
                         { castleCenterX, static_cast<float>(topY - 18) },
                         { castleCenterX + 28.0f, static_cast<float>(topY + 14) }, Color{ 48, 18, 22, 255 });
            DrawCircle(static_cast<int>(castleCenterX), topY, 14.0f, Color{ 145, 30, 36, 255 });
            DrawCircleLines(static_cast<int>(castleCenterX), topY, 14.0f, goldAccent);
        }
    }

    void DrawAnimatedCastleGlows(const GameContext& ctx, float castleCenterX, int baseY, float time) {
        int keepLvl = ctx.GetManorLevel(ManorNodeId::Stat_Health) + ctx.GetManorLevel(ManorNodeId::Stat_Mana);
        int keepH = 175 + std::min(160, keepLvl * 6);
        int keepW = 150;
        int keepLeft = static_cast<int>(castleCenterX - keepW * 0.5f);
        int machY = baseY - keepH;

        // Прапор на шпилі цитаделі
        Vector2 spireTop = { castleCenterX, static_cast<float>(machY - 123) };
        float flagWave = std::sin(time * 6.5f) * 6.0f;
        DrawTriangle(spireTop,
                     { spireTop.x + 28.0f + flagWave, spireTop.y + 6.0f },
                     { spireTop.x, spireTop.y + 16.0f }, Color{ 180, 30, 42, 255 });

        // Адитивне тепле сяйво вікон замку
        BeginBlendMode(BLEND_ADDITIVE);
        float winFlicker = 0.88f + 0.12f * std::sin(time * 8.0f + 1.2f);
        for (int wy = baseY - 50; wy > baseY - keepH + 35; wy -= 44) {
            for (float wx : { castleCenterX - 45.0f, castleCenterX + 25.0f }) {
                DrawCircleGradient(static_cast<int>(wx + 8.0f), wy + 10, 28.0f * winFlicker,
                                   Color{ 255, 184, 77, 45 }, Color{ 255, 184, 77, 0 });
            }
        }

        // Адитивне сяйво магічного кришталя вежі магів
        int rightLvl = ctx.GetManorLevel(ManorNodeId::Stat_MagicDmg)
                     + ctx.GetManorLevel(ManorNodeId::Class_Mage) * 5
                     + ctx.GetManorLevel(ManorNodeId::Class_Lich) * 5;
        if (rightLvl > 0) {
            int rH = 130 + std::min(190, rightLvl * 7);
            int rW = 95;
            int rLeft = keepLeft + keepW - 8;
            Vector2 gemPos = { static_cast<float>(rLeft + rW / 2), static_cast<float>(baseY - rH - 90) };
            float gemPulse = 1.0f + 0.25f * std::sin(time * 4.5f);
            DrawCircleGradient(static_cast<int>(gemPos.x), static_cast<int>(gemPos.y), 26.0f * gemPulse,
                               Color{ 120, 200, 255, 95 }, Color{ 120, 200, 255, 0 });
            DrawCircleV(gemPos, 4.0f, WHITE);
        }
        EndBlendMode();
    }

    void DrawDistantCastleSilhouette(float cx, float groundY) {
        Color castSil = Color{ 14, 17, 30, 255 };
        Color castRim = Color{ 38, 45, 71, 255 };

        // Цитадель
        DrawRectangle(static_cast<int>(cx - 50.0f), static_cast<int>(groundY - 210.0f), 100, 210, castSil);
        DrawLine(static_cast<int>(cx - 50.0f), static_cast<int>(groundY - 210.0f), static_cast<int>(cx - 50.0f), static_cast<int>(groundY), castRim);
        // Гостроверхий дах
        DrawTriangle({ cx - 55.0f, groundY - 210.0f }, { cx, groundY - 275.0f }, { cx + 55.0f, groundY - 210.0f }, castSil);
        DrawLine(static_cast<int>(cx - 55.0f), static_cast<int>(groundY - 210.0f), static_cast<int>(cx), static_cast<int>(groundY - 275.0f), castRim);

        // Бічні вежі
        DrawRectangle(static_cast<int>(cx - 105.0f), static_cast<int>(groundY - 170.0f), 50, 170, castSil);
        DrawTriangle({ cx - 110.0f, groundY - 170.0f }, { cx - 80.0f, groundY - 225.0f }, { cx - 50.0f, groundY - 170.0f }, castSil);

        DrawRectangle(static_cast<int>(cx + 55.0f), static_cast<int>(groundY - 180.0f), 55, 180, castSil);
        DrawTriangle({ cx + 50.0f, groundY - 180.0f }, { cx + 82.0f, groundY - 240.0f }, { cx + 115.0f, groundY - 180.0f }, castSil);
    }
}

ProceduralRenderer::~ProceduralRenderer() {
    Cleanup();
}

void ProceduralRenderer::Cleanup() {
    if (manorBackdrop_.id > 0) {
        UnloadRenderTexture(manorBackdrop_);
        manorBackdrop_ = {};
    }
    if (outskirtsBackdrop_.id > 0) {
        UnloadRenderTexture(outskirtsBackdrop_);
        outskirtsBackdrop_ = {};
    }
    manorBaked_ = false;
    outskirtsBaked_ = false;
    lastManorHash_ = -1;
}

void ProceduralRenderer::InvalidateBackdrops() {
    manorBaked_ = false;
    outskirtsBaked_ = false;
    lastManorHash_ = -1;
}

void ProceduralRenderer::EnsureManorBackdrop(const GameContext& ctx) const {
    int curHash = ctx.GetTotalManorLevels()
                + ctx.GetManorLevel(ManorNodeId::Unlock_Blacksmith) * 100
                + ctx.GetManorLevel(ManorNodeId::Unlock_Enchantress) * 200
                + ctx.GetManorLevel(ManorNodeId::Unlock_Architect) * 400
                + ctx.GetManorLevel(ManorNodeId::Class_Barbarian) * 50
                + ctx.GetManorLevel(ManorNodeId::Class_Mage) * 50;

    if (manorBaked_ && manorBackdrop_.id > 0 && curHash == lastManorHash_) {
        return;
    }

    if (manorBackdrop_.id == 0) {
        manorBackdrop_ = LoadRenderTexture(Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT);
        SetTextureFilter(manorBackdrop_.texture, TEXTURE_FILTER_POINT);
    }

    lastManorHash_ = curHash;
    manorBaked_ = true;

    BeginTextureMode(manorBackdrop_);
    ClearBackground(BLACK);

    // 1. Небо: вертикальний градієнт з 3 зон (#070913 -> #161B33 -> #2A1E3F)
    DrawRectangleGradientV(0, 0, Config::VIRTUAL_WIDTH, 260, Color{ 7, 9, 19, 255 }, Color{ 22, 27, 51, 255 });
    DrawRectangleGradientV(0, 260, Config::VIRTUAL_WIDTH, 300, Color{ 22, 27, 51, 255 }, Color{ 42, 30, 63, 255 });

    // 2. Гори: ламана лінія з 1D шуму (4 октави), силует #111426, світліша 1px лінія хребта #262D47
    Color mtnSilhouette = Color{ 17, 20, 38, 255 };
    Color mtnRidge      = Color{ 38, 45, 71, 255 };
    for (int x = 0; x < Config::VIRTUAL_WIDTH; ++x) {
        float yRidge = 260.0f + Fbm1D(static_cast<float>(x) * 0.0038f + 14.2f, 4) * 165.0f;
        DrawLine(x, static_cast<int>(yRidge), x, 560, mtnSilhouette);
        DrawPixel(x, static_cast<int>(yRidge), mtnRidge);
    }

    // 3. Ліс: силуети ялин і голих покручених дерев #0C0F1D
    for (int x = 20; x < Config::VIRTUAL_WIDTH; x += 38) {
        uint32_t th = TileHash(x, 129);
        float baseY = 480.0f + static_cast<float>(th % 60);
        if ((th % 3) == 0) {
            DrawGothicTwistedTree(static_cast<float>(x), baseY, 55.0f + static_cast<float>((th >> 3) % 25), 34.0f);
        } else {
            DrawGothicPine(static_cast<float>(x), baseY, 70.0f + static_cast<float>((th >> 4) % 30), 38.0f);
        }
    }

    // 4. Місяць: запечений диск з кратерами (moonX = 740, moonY = 130)
    DrawGothicMoonDisk(740.0f, 130.0f);

    // 5. Замок: база #23283D, тіньова сторона #161928, місячний блік зліва #384161
    DrawStaticCastleStructure(ctx, 920.0f, 560);

    // 6. Передній план: кам'яний насип #141824, кована огорожа з піками, надгробки
    DrawRectangle(0, 560, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT - 560, Color{ 20, 24, 36, 255 });
    DrawLine(0, 560, Config::VIRTUAL_WIDTH, 560, Color{ 38, 45, 71, 255 });
    DrawGothicFenceAndGraves(560.0f);

    EndTextureMode();
}

void ProceduralRenderer::EnsureOutskirtsBackdrop(const GameContext& /*ctx*/) const {
    if (outskirtsBaked_ && outskirtsBackdrop_.id > 0) {
        return;
    }

    if (outskirtsBackdrop_.id == 0) {
        outskirtsBackdrop_ = LoadRenderTexture(Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT);
        SetTextureFilter(outskirtsBackdrop_.texture, TEXTURE_FILTER_POINT);
    }
    outskirtsBaked_ = true;

    BeginTextureMode(outskirtsBackdrop_);
    ClearBackground(BLACK);

    // 1. Небо: вертикальний градієнт з 3 зон (#070913 -> #161B33 -> #2A1E3F)
    DrawRectangleGradientV(0, 0, Config::VIRTUAL_WIDTH, 270, Color{ 7, 9, 19, 255 }, Color{ 22, 27, 51, 255 });
    DrawRectangleGradientV(0, 270, Config::VIRTUAL_WIDTH, 310, Color{ 22, 27, 51, 255 }, Color{ 42, 30, 63, 255 });

    // 2. Гори: ламана лінія з 1D шуму (4 октави), силует #111426, світліша 1px лінія хребта
    Color mtnSilhouette = Color{ 17, 20, 38, 255 };
    Color mtnRidge      = Color{ 38, 45, 71, 255 };
    for (int x = 0; x < Config::VIRTUAL_WIDTH; ++x) {
        float yRidge = 275.0f + Fbm1D(static_cast<float>(x) * 0.0035f + 8.7f, 4) * 160.0f;
        DrawLine(x, static_cast<int>(yRidge), x, 580, mtnSilhouette);
        DrawPixel(x, static_cast<int>(yRidge), mtnRidge);
    }

    // 3. Ліс: силуети ялин і голих покручених дерев #0C0F1D
    for (int x = 15; x < Config::VIRTUAL_WIDTH; x += 36) {
        uint32_t th = TileHash(x, 241);
        float baseY = 510.0f + static_cast<float>(th % 55);
        if ((th % 3) == 1) {
            DrawGothicTwistedTree(static_cast<float>(x), baseY, 58.0f + static_cast<float>((th >> 3) % 25), 36.0f);
        } else {
            DrawGothicPine(static_cast<float>(x), baseY, 75.0f + static_cast<float>((th >> 4) % 30), 40.0f);
        }
    }

    // 4. Далекий силует Замку Гамсон
    DrawDistantCastleSilhouette(1020.0f, 580.0f);

    // 5. Місяць
    DrawGothicMoonDisk(1040.0f, 125.0f);

    // 6. Дорога та бруківка #141824
    DrawRectangle(0, 580, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT - 580, Color{ 20, 24, 36, 255 });
    DrawLine(0, 580, Config::VIRTUAL_WIDTH, 580, Color{ 46, 49, 63, 255 });
    for (int r = 0; r < 4; ++r) {
        int ry = 588 + r * 30;
        int off = (r % 2 == 1) ? 36 : 0;
        for (int c = -1; c < (Config::VIRTUAL_WIDTH / 72) + 2; ++c) {
            int cx = c * 72 + off;
            DrawRectangle(cx + 2, ry + 2, 68, 26, Color{ 27, 31, 46, 255 });
            DrawRectangleLines(cx + 2, ry + 2, 68, 26, Color{ 12, 10, 18, 255 });
        }
    }

    // 7. Огорожа та надгробки обабіч дороги
    DrawGothicFenceAndGraves(580.0f);

    EndTextureMode();
}

void ProceduralRenderer::DrawManorCastle(const GameContext& ctx, float time) const {
    EnsureManorBackdrop(ctx);

    // 1. Виводимо запечений бекдроп
    Rectangle srcRec = { 0.0f, 0.0f, static_cast<float>(manorBackdrop_.texture.width), -static_cast<float>(manorBackdrop_.texture.height) };
    Rectangle dstRec = { 0.0f, 0.0f, static_cast<float>(Config::VIRTUAL_WIDTH), static_cast<float>(Config::VIRTUAL_HEIGHT) };
    DrawTexturePro(manorBackdrop_.texture, srcRec, dstRec, { 0.0f, 0.0f }, 0.0f, WHITE);

    // 2. Анімовані зірки (120 зірок, ~40% мерехтять)
    const auto& stars = GetGothicSkyStars();
    for (size_t i = 0; i < stars.size(); ++i) {
        const auto& s = stars[i];
        if (i < 72) {
            DrawRectangle(static_cast<int>(s.x), static_cast<int>(s.y), static_cast<int>(s.size), static_cast<int>(s.size), Color{ 190, 210, 245, 200 });
        } else {
            float pulse = 0.6f + 0.4f * std::sin(time * s.speed + s.seed);
            DrawRectangle(static_cast<int>(s.x), static_cast<int>(s.y), static_cast<int>(s.size), static_cast<int>(s.size), ColorAlpha(Color{ 220, 235, 255, 255 }, pulse));
        }
    }

    // 3. Адитивне гало місяця (R ≈ 180, color {120,160,255}, alpha 0.22)
    BeginBlendMode(BLEND_ADDITIVE);
    float pulse = 1.0f + 0.03f * std::sin(time * 2.2f);
    DrawCircleGradient(740, 130, 180.0f * pulse, Color{ 120, 160, 255, 56 }, Color{ 120, 160, 255, 0 });
    DrawCircleGradient(740, 130, 75.0f * pulse, Color{ 150, 180, 255, 75 }, Color{ 150, 180, 255, 0 });
    EndBlendMode();

    // 4. Хмари: витягнуті багатошарові пасма з місячним rim light
    DrawGothicClouds(time);

    // 5. 3 шари фіолетово-блакитного туману
    DrawGothicFog(time);

    // 6. Анімовані ефекти замку (сяйво вікон, прапор на вітрі)
    DrawAnimatedCastleGlows(ctx, 920.0f, 560, time);

    // 7. Вуличні ліхтарі з теплим світлом
    DrawGothicLantern(240.0f, 560.0f, time);
    DrawGothicLantern(620.0f, 560.0f, time);
    DrawGothicLantern(1180.0f, 560.0f, time);

    // 8. Темна трава, що гнеться від вітру
    DrawSwayingGrass(560.0f, time);
}

void ProceduralRenderer::DrawOutskirtsLandscape(const GameContext& ctx, float time) const {
    EnsureOutskirtsBackdrop(ctx);

    // 1. Виводимо запечений бекдроп
    Rectangle srcRec = { 0.0f, 0.0f, static_cast<float>(outskirtsBackdrop_.texture.width), -static_cast<float>(outskirtsBackdrop_.texture.height) };
    Rectangle dstRec = { 0.0f, 0.0f, static_cast<float>(Config::VIRTUAL_WIDTH), static_cast<float>(Config::VIRTUAL_HEIGHT) };
    DrawTexturePro(outskirtsBackdrop_.texture, srcRec, dstRec, { 0.0f, 0.0f }, 0.0f, WHITE);

    // 2. Анімовані зірки
    const auto& stars = GetGothicSkyStars();
    for (size_t i = 0; i < stars.size(); ++i) {
        const auto& s = stars[i];
        if (i < 72) {
            DrawRectangle(static_cast<int>(s.x), static_cast<int>(s.y), static_cast<int>(s.size), static_cast<int>(s.size), Color{ 190, 210, 245, 200 });
        } else {
            float pulse = 0.6f + 0.4f * std::sin(time * s.speed + s.seed);
            DrawRectangle(static_cast<int>(s.x), static_cast<int>(s.y), static_cast<int>(s.size), static_cast<int>(s.size), ColorAlpha(Color{ 220, 235, 255, 255 }, pulse));
        }
    }

    // 3. Адитивне гало місяця
    BeginBlendMode(BLEND_ADDITIVE);
    float pulse = 1.0f + 0.03f * std::sin(time * 2.2f);
    DrawCircleGradient(1040, 125, 180.0f * pulse, Color{ 120, 160, 255, 56 }, Color{ 120, 160, 255, 0 });
    DrawCircleGradient(1040, 125, 75.0f * pulse, Color{ 150, 180, 255, 75 }, Color{ 150, 180, 255, 0 });
    EndBlendMode();

    // 4. Хмари
    DrawGothicClouds(time);

    // 5. Туман
    DrawGothicFog(time);

    // 6. Ліхтарі з теплим світлом
    DrawGothicLantern(180.0f, 580.0f, time);
    DrawGothicLantern(450.0f, 580.0f, time);
    DrawGothicLantern(730.0f, 580.0f, time);
    DrawGothicLantern(1015.0f, 580.0f, time);

    // 7. Темна трава, що колишеться
    DrawSwayingGrass(580.0f, time);
}

void ProceduralRenderer::DrawBlacksmithForge(Vector2 pos, float time, bool nearHero) const {
    const auto& pal = GetPalette();
    int bx = static_cast<int>(pos.x - 85.0f);
    int by = static_cast<int>(pos.y - 150.0f);

    Color stoneWall   = Color{ 35, 40, 61, 255 };
    Color stoneMrt    = pal.outline;
    Color roofSlate   = Color{ 23, 28, 46, 255 };
    Color ironCol     = pal.metal[2];
    Color ironHi      = pal.metal[4];

    // Кам'яна споруда кузні
    DrawRectangle(bx, by, 170, 150, stoneWall);
    DrawRectangleLinesEx({ static_cast<float>(bx), static_cast<float>(by), 170.0f, 150.0f }, 2.0f, stoneMrt);
    for (int y = by + 25; y < static_cast<int>(pos.y); y += 25) {
        DrawLine(bx, y, bx + 170, y, stoneMrt);
    }

    // Гостроверхий навіс
    DrawTriangle({ static_cast<float>(bx - 12), static_cast<float>(by) },
                 { static_cast<float>(bx + 85), static_cast<float>(by - 32) },
                 { static_cast<float>(bx + 182), static_cast<float>(by) }, roofSlate);
    DrawLine(bx - 12, by, bx + 85, by - 32, pal.metal[4]);

    // Димохід
    DrawRectangle(bx + 130, by - 48, 24, 48, Color{ 28, 32, 48, 255 });
    DrawRectangleLines(bx + 130, by - 48, 24, 48, stoneMrt);

    // Дим з димаря
    for (int p = 0; p < 4; ++p) {
        float py = (by - 55.0f) - std::fmod(time * 30.0f + p * 20.0f, 65.0f);
        float px = (bx + 142.0f) + std::sin(time * 3.0f + p * 1.5f) * 12.0f;
        DrawCircle(static_cast<int>(px), static_cast<int>(py), 6.0f + p * 2.5f, Fade(Color{ 160, 175, 195, 255 }, 0.35f));
    }

    // Горно з палаючим вогнем та жаринами
    int hearthX = bx + 42;
    int hearthY = static_cast<int>(pos.y - 42.0f);
    DrawRectangle(hearthX - 24, hearthY - 14, 48, 42, Color{ 18, 14, 16, 255 });
    DrawRectangleLines(hearthX - 24, hearthY - 14, 48, 42, stoneMrt);

    float flamePulse = 1.0f + 0.15f * std::sin(time * 12.0f);
    DrawCircle(hearthX, hearthY + 6, 16.0f * flamePulse, Color{ 232, 115, 28, 255 });
    DrawCircle(hearthX, hearthY + 4, 10.0f * flamePulse, Color{ 255, 179, 71, 255 });
    DrawCircle(hearthX, hearthY + 2, 5.0f * flamePulse, Color{ 255, 240, 180, 255 });

    // Адитивне відблискування полум'я горна
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient(hearthX, hearthY + 4, 60.0f * flamePulse, Color{ 232, 115, 28, 65 }, Color{ 232, 115, 28, 0 });
    EndBlendMode();

    // Важке ковадло
    int anvilX = bx + 105;
    int anvilY = static_cast<int>(pos.y - 26.0f);
    DrawRectangle(anvilX - 6, anvilY + 12, 28, 14, Color{ 58, 42, 32, 255 });
    DrawRectangle(anvilX - 10, anvilY, 36, 12, ironCol);
    DrawTriangle({ static_cast<float>(anvilX - 18), static_cast<float>(anvilY) },
                 { static_cast<float>(anvilX - 10), static_cast<float>(anvilY) },
                 { static_cast<float>(anvilX - 10), static_cast<float>(anvilY + 12) }, ironCol);
    DrawRectangle(anvilX + 4, anvilY - 8, 12, 4, ironHi);

    // Стійка зі зброєю
    DrawLineEx({ static_cast<float>(bx + 152), pos.y - 42.0f }, { static_cast<float>(bx + 152), pos.y }, 3.0f, ironHi);
    DrawCircle(bx + 152, static_cast<int>(pos.y - 24.0f), 10.0f, Color{ 48, 52, 68, 255 });
    DrawCircleLines(bx + 152, static_cast<int>(pos.y - 24.0f), 10.0f, pal.fireGold[3]);

    // Вивіска
    DrawRectangleRounded({ static_cast<float>(bx + 25), static_cast<float>(by - 42), 120.0f, 26.0f }, 0.25f, 4, Color{ 20, 24, 36, 230 });
    DrawRectangleRoundedLines({ static_cast<float>(bx + 25), static_cast<float>(by - 42), 120.0f, 26.0f }, 0.25f, 4, 1.5f, pal.fireGold[3]);

    if (nearHero) {
        DrawRectangleRounded({ static_cast<float>(bx + 10), static_cast<float>(by - 72), 150.0f, 24.0f }, 0.25f, 4, Fade(BLACK, 0.82f));
        DrawRectangleRoundedLines({ static_cast<float>(bx + 10), static_cast<float>(by - 72), 150.0f, 24.0f }, 0.25f, 4, 1.5f, Color{ 140, 235, 100, 255 });
    }
}

void ProceduralRenderer::DrawEnchantressTent(Vector2 pos, float time, bool nearHero) const {
    const auto& pal = GetPalette();
    int ex = static_cast<int>(pos.x);
    int ey = static_cast<int>(pos.y);

    Color tentVelvet = Color{ 42, 14, 55, 255 }; // #2A0E37
    Color tentShadow = Color{ 24, 8, 34, 255 };
    Color goldTassel = pal.fireGold[3];
    Color woodPillar = Color{ 82, 54, 38, 255 };

    // 1. Різьблені спіральні стовпи намету
    DrawRectangle(ex - 82, ey - 150, 7, 150, woodPillar);
    DrawRectangle(ex + 75, ey - 150, 7, 150, woodPillar);
    for (int sy = ey - 140; sy < ey; sy += 18) {
        DrawLine(ex - 82, sy, ex - 75, sy + 8, pal.metal[3]);
        DrawLine(ex + 75, sy, ex + 82, sy + 8, pal.metal[3]);
    }

    // 2. Шатро з провислими схилами даху (Drooped catenary roofline)
    // Малюємо через вигнуті полігони
    Vector2 peak = { static_cast<float>(ex), static_cast<float>(ey - 165) };
    Vector2 leftCorner = { static_cast<float>(ex - 92), static_cast<float>(ey - 15) };
    Vector2 rightCorner = { static_cast<float>(ex + 92), static_cast<float>(ey - 15) };

    DrawTriangle(leftCorner, peak, { static_cast<float>(ex), static_cast<float>(ey - 15) }, tentVelvet);
    DrawTriangle({ static_cast<float>(ex), static_cast<float>(ey - 15) }, peak, rightCorner, tentShadow);

    // Провислі дуги схилу даху
    DrawLineBezier(peak, leftCorner, 3.5f, goldTassel);
    DrawLineBezier(peak, rightCorner, 3.5f, goldTassel);

    // Золоті китиці по карнизу (Golden tassels)
    for (int tx = ex - 84; tx <= ex + 84; tx += 14) {
        float frac = std::abs(static_cast<float>(tx - ex)) / 84.0f;
        float ty = (ey - 15) - (1.0f - frac) * 6.0f;
        DrawLine(tx, static_cast<int>(ty), tx, static_cast<int>(ty + 8), goldTassel);
        DrawCircle(tx, static_cast<int>(ty + 9), 1.8f, goldTassel);
    }

    // Внутрішній розріз шатра
    DrawTriangle({ static_cast<float>(ex - 42), static_cast<float>(ey) },
                 { static_cast<float>(ex), static_cast<float>(ey - 105) },
                 { static_cast<float>(ex + 42), static_cast<float>(ey) }, Color{ 14, 5, 20, 255 });

    // 3. Магічний ліхтар / жаровня (синьо-фіолетове світло)
    Vector2 lanternPos = { static_cast<float>(ex), static_cast<float>(ey - 110) };
    float orbWave = std::sin(time * 3.5f) * 5.0f;
    Vector2 orbPos = { lanternPos.x, lanternPos.y + orbWave };

    // Підвісний ланцюг
    DrawLine(ex, ey - 165, static_cast<int>(orbPos.x), static_cast<int>(orbPos.y - 14), pal.metal[2]);

    // Магічна сфера з пульсуючим сяйвом
    float pulse = 1.0f + 0.18f * std::sin(time * 5.0f);
    DrawCircleV(orbPos, 12.0f * pulse, Color{ 160, 60, 220, 200 });
    DrawCircleV(orbPos, 7.0f * pulse, Color{ 120, 200, 255, 240 });
    DrawCircleV(orbPos, 3.0f, WHITE);

    // Адитивне синьо-фіолетове світло
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient(static_cast<int>(orbPos.x), static_cast<int>(orbPos.y), 75.0f * pulse,
                       Color{ 140, 70, 235, 75 }, Color{ 140, 70, 235, 0 });
    EndBlendMode();

    // Казан зі зіллям
    DrawCircle(ex, ey - 22, 18.0f, Color{ 28, 30, 42, 255 });
    DrawCircle(ex, ey - 25, 13.0f, Color{ 150, 40, 180, 255 });
    DrawCircle(ex - 4, ey - 27, 3.5f, Color{ 220, 160, 255, 255 });

    // Вивіска
    DrawRectangleRounded({ static_cast<float>(ex - 65), static_cast<float>(ey - 188), 130.0f, 26.0f }, 0.25f, 4, Color{ 26, 14, 38, 230 });
    DrawRectangleRoundedLines({ static_cast<float>(ex - 65), static_cast<float>(ey - 188), 130.0f, 26.0f }, 0.25f, 4, 1.5f, Color{ 180, 70, 235, 255 });

    if (nearHero) {
        DrawRectangleRounded({ static_cast<float>(ex - 75), static_cast<float>(ey - 218), 150.0f, 24.0f }, 0.25f, 4, Fade(BLACK, 0.82f));
        DrawRectangleRoundedLines({ static_cast<float>(ex - 75), static_cast<float>(ey - 218), 150.0f, 24.0f }, 0.25f, 4, 1.5f, Color{ 140, 235, 100, 255 });
    }
}

void ProceduralRenderer::DrawArchitectGazebo(Vector2 pos, float /*time*/, bool isLocked, bool nearHero) const {
    const auto& pal = GetPalette();
    int ax = static_cast<int>(pos.x - 55.0f);
    int ay = static_cast<int>(pos.y - 145.0f);

    Color marbleDark = Color{ 45, 50, 68, 255 };
    Color marbleHi   = Color{ 75, 85, 110, 255 };
    Color outlineCol = pal.outline;

    // Мармурові колони з канелюрами
    DrawRectangle(ax + 8, ay + 26, 14, 119, marbleDark);
    DrawLine(ax + 8, ay + 26, ax + 8, ay + 145, marbleHi);
    DrawLine(ax + 15, ay + 26, ax + 15, ay + 145, outlineCol);

    DrawRectangle(ax + 88, ay + 26, 14, 119, marbleDark);
    DrawLine(ax + 88, ay + 26, ax + 88, ay + 145, marbleHi);
    DrawLine(ax + 95, ay + 26, ax + 95, ay + 145, outlineCol);

    // Капітелі
    DrawRectangle(ax + 5, ay + 20, 20, 6, marbleHi);
    DrawRectangle(ax + 85, ay + 20, 20, 6, marbleHi);

    // Фронтон з різьбленим архітравом
    DrawTriangle({ static_cast<float>(ax - 8), static_cast<float>(ay + 20) },
                 { static_cast<float>(ax + 55), static_cast<float>(ay - 18) },
                 { static_cast<float>(ax + 118), static_cast<float>(ay + 20) }, marbleDark);
    DrawLineEx({ static_cast<float>(ax - 8), static_cast<float>(ay + 20) },
               { static_cast<float>(ax + 55), static_cast<float>(ay - 18) }, 2.0f, marbleHi);
    DrawLineEx({ static_cast<float>(ax + 118), static_cast<float>(ay + 20) },
               { static_cast<float>(ax + 55), static_cast<float>(ay - 18) }, 2.0f, outlineCol);

    // Стіл архітектора з кресленнями
    int deskX = ax + 24;
    int deskY = static_cast<int>(pos.y - 36.0f);
    DrawRectangle(deskX, deskY, 62, 36, Color{ 48, 36, 28, 255 });
    DrawRectangle(deskX + 6, deskY + 2, 50, 14, Color{ 210, 200, 175, 255 });
    DrawLine(deskX + 10, deskY + 6, deskX + 50, deskY + 6, Color{ 70, 120, 180, 255 });

    if (isLocked) {
        DrawCircle(deskX + 31, deskY - 14, 10.0f, Color{ 232, 115, 28, 255 });
        DrawCircleLines(deskX + 31, deskY - 14, 10.0f, pal.fireGold[3]);
    }

    // Вивіска
    DrawRectangleRounded({ static_cast<float>(ax), static_cast<float>(ay - 44), 110.0f, 26.0f }, 0.25f, 4, Color{ 20, 28, 42, 230 });
    DrawRectangleRoundedLines({ static_cast<float>(ax), static_cast<float>(ay - 44), 110.0f, 26.0f }, 0.25f, 4, 1.5f, Color{ 120, 180, 240, 255 });

    if (nearHero) {
        DrawRectangleRounded({ static_cast<float>(ax - 20), static_cast<float>(ay - 96), 150.0f, 24.0f }, 0.25f, 4, Fade(BLACK, 0.82f));
        DrawRectangleRoundedLines({ static_cast<float>(ax - 20), static_cast<float>(ay - 96), 150.0f, 24.0f }, 0.25f, 4, 1.5f, Color{ 140, 235, 100, 255 });
    }
}

void ProceduralRenderer::DrawCastleGate(Vector2 pos, float portcullisH, float time, bool nearHero) const {
    const auto& pal = GetPalette();
    int gx = static_cast<int>(pos.x);
    int gy = static_cast<int>(pos.y);

    Color stoneWall   = Color{ 35, 40, 61, 255 };
    Color stoneShadow = Color{ 22, 25, 40, 255 };
    Color outlineCol  = pal.outline;

    // 1. Масивна кам'яна брама замку
    DrawRectangle(gx - 25, gy - 230, 175, 230, stoneWall);
    DrawRectangle(gx + 62, gy - 230, 88, 230, stoneShadow);
    DrawRectangleLinesEx({ static_cast<float>(gx - 25), static_cast<float>(gy - 230), 175.0f, 230.0f }, 3.0f, outlineCol);

    // Зубці брами з машикулями
    for (int bx = gx - 25; bx < gx + 150; bx += 28) {
        DrawRectangle(bx, gy - 246, 18, 16, stoneWall);
        DrawRectangleLines(bx, gy - 246, 18, 16, outlineCol);
    }

    // 2. Вхідна арка: ТЕПЛЕ СВІТЛО ЗСЕРЕДИНИ (Dungeon hearth glow), а не чорна діра!
    int archW = 114;
    int archH = 175;
    int archX = gx + 12;
    int archY = gy - archH;

    // Внутрішнє тепле сяйво замкових факелів
    DrawRectangle(archX, archY, archW, archH, Color{ 32, 18, 14, 255 });
    DrawCircleSector({ static_cast<float>(archX + archW / 2), static_cast<float>(archY) },
                     static_cast<float>(archW / 2), 180.0f, 360.0f, 16, Color{ 32, 18, 14, 255 });

    // Теплий внутрішній градієнт вогню
    float glowFlicker = 0.85f + 0.15f * std::sin(time * 7.5f);
    DrawCircleGradient(archX + archW / 2, gy - 80, 75.0f * glowFlicker, Color{ 232, 115, 28, 130 }, Color{ 232, 115, 28, 0 });
    DrawCircleGradient(archX + archW / 2, gy - 50, 45.0f * glowFlicker, Color{ 255, 179, 71, 160 }, Color{ 255, 179, 71, 0 });

    // Клиноподібне склепіння арки (Voussoirs)
    DrawCircleSectorLines({ static_cast<float>(archX + archW / 2), static_cast<float>(archY) },
                          static_cast<float>(archW / 2), 180.0f, 360.0f, 16, outlineCol);
    DrawLine(archX, archY, archX, gy, outlineCol);
    DrawLine(archX + archW, archY, archX + archW, gy, outlineCol);

    // 3. Підйомна залізна решітка (Portcullis)
    int portY = gy - 175;
    int pH = static_cast<int>(portcullisH);
    for (int px = gx + 22; px < gx + 120; px += 16) {
        DrawLineEx({ static_cast<float>(px), static_cast<float>(portY) },
                   { static_cast<float>(px), static_cast<float>(portY + pH) }, 4.0f, pal.metal[3]);
        DrawTriangle({ static_cast<float>(px - 4), static_cast<float>(portY + pH) },
                     { static_cast<float>(px + 4), static_cast<float>(portY + pH) },
                     { static_cast<float>(px),     static_cast<float>(portY + pH + 8) }, pal.metal[4]);
    }

    // Смолоскипи обабіч брами
    DrawGothicLantern(gx - 12.0f, gy - 40.0f, time);
    DrawGothicLantern(gx + 142.0f, gy - 40.0f, time + 1.2f);

    if (nearHero) {
        DrawRectangleRounded({ static_cast<float>(gx - 14), static_cast<float>(gy - 200), 168.0f, 26.0f }, 0.25f, 4, Fade(BLACK, 0.85f));
        DrawRectangleRoundedLines({ static_cast<float>(gx - 14), static_cast<float>(gy - 200), 168.0f, 26.0f }, 0.25f, 4, 1.5f, pal.fireGold[3]);
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
        case TraitType::Gay: {
            // Dual linked circles with arrow
            DrawCircleLines(static_cast<int>(c.x - size * 0.3f), static_cast<int>(c.y), size * 0.5f, col);
            DrawCircleLines(static_cast<int>(c.x + size * 0.3f), static_cast<int>(c.y), size * 0.5f, col);
            DrawLineEx({ c.x + size * 0.5f, c.y - size * 0.3f }, { c.x + size * 0.9f, c.y - size * 0.7f }, 2.0f, col);
            break;
        }
        case TraitType::Dyslexia: {
            // Jumbled text runes
            DrawText("A", static_cast<int>(c.x - size * 0.6f), static_cast<int>(c.y - size * 0.5f), static_cast<int>(size * 0.8f), col);
            DrawText("Z", static_cast<int>(c.x + 1), static_cast<int>(c.y - size * 0.2f), static_cast<int>(size * 0.8f), WHITE);
            break;
        }
        case TraitType::Baldness: {
            // Smooth polished helmet dome without plume
            DrawCircleSector({ c.x, c.y + size * 0.2f }, size * 0.8f, 180.0f, 360.0f, 12, col);
            DrawLineEx({ c.x - size * 0.8f, c.y + size * 0.2f }, { c.x + size * 0.8f, c.y + size * 0.2f }, 2.0f, WHITE);
            break;
        }
        case TraitType::Alzheimers: {
            // Question mark inside head
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.8f, col);
            DrawText("?", static_cast<int>(c.x - size * 0.25f), static_cast<int>(c.y - size * 0.5f), static_cast<int>(size * 1.1f), WHITE);
            break;
        }
        case TraitType::Coprolalia: {
            // Speech bubble with exclamation and symbols
            DrawRectangleRounded({ c.x - size * 0.85f, c.y - size * 0.7f, size * 1.7f, size * 1.1f }, 0.4f, 4, col);
            DrawTriangle({ c.x - size * 0.4f, c.y + size * 0.4f }, { c.x, c.y + size * 0.4f }, { c.x - size * 0.6f, c.y + size * 0.85f }, col);
            DrawText("!", static_cast<int>(c.x - 3), static_cast<int>(c.y - size * 0.55f), static_cast<int>(size * 0.9f), BLACK);
            break;
        }
        case TraitType::OCD: {
            // Sparkle broom / neat symmetry cross
            DrawLineEx({ c.x, c.y - size * 0.8f }, { c.x, c.y + size * 0.8f }, 2.0f, col);
            DrawLineEx({ c.x - size * 0.8f, c.y }, { c.x + size * 0.8f, c.y }, 2.0f, col);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.25f, WHITE);
            break;
        }
        case TraitType::Hypergonadism: {
            // Heavy muscle fist / anvil
            DrawRectangle(static_cast<int>(c.x - size * 0.7f), static_cast<int>(c.y - size * 0.4f), static_cast<int>(size * 1.4f), static_cast<int>(size * 0.8f), col);
            DrawTriangle({ c.x - size * 0.7f, c.y - size * 0.4f }, { c.x + size * 0.7f, c.y - size * 0.4f }, { c.x, c.y - size * 0.9f }, col);
            break;
        }
        case TraitType::MuscleWk: {
            // Limp drooping arm
            DrawLineEx({ c.x - size * 0.7f, c.y - size * 0.5f }, { c.x, c.y }, 2.0f, col);
            DrawLineEx({ c.x, c.y }, { c.x + size * 0.4f, c.y + size * 0.8f }, 2.0f, Fade(col, 0.6f));
            break;
        }
        case TraitType::StereoBlind: {
            // 2D flat paper card folding in 3D
            DrawRectangleLines(static_cast<int>(c.x - size * 0.35f), static_cast<int>(c.y - size * 0.75f), static_cast<int>(size * 0.7f), static_cast<int>(size * 1.5f), col);
            DrawLineEx({ c.x, c.y - size * 0.75f }, { c.x, c.y + size * 0.75f }, 1.5f, WHITE);
            break;
        }
        case TraitType::TunnelVision: {
            // Two vertical peripheral blinders
            DrawRectangle(static_cast<int>(c.x - size * 0.85f), static_cast<int>(c.y - size * 0.75f), static_cast<int>(size * 0.45f), static_cast<int>(size * 1.5f), col);
            DrawRectangle(static_cast<int>(c.x + size * 0.4f), static_cast<int>(c.y - size * 0.75f), static_cast<int>(size * 0.45f), static_cast<int>(size * 1.5f), col);
            break;
        }
        case TraitType::Ambilevous: {
            // Reversed backward casting arrow
            DrawLineEx({ c.x + size * 0.6f, c.y }, { c.x - size * 0.6f, c.y }, 2.0f, col);
            DrawTriangle({ c.x - size * 0.8f, c.y }, { c.x - size * 0.4f, c.y - size * 0.4f }, { c.x - size * 0.4f, c.y + size * 0.4f }, col);
            break;
        }
        case TraitType::Alektorophobia: {
            // Chicken head comb & beak
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.55f, WHITE);
            DrawTriangle({ c.x, c.y - size * 0.4f }, { c.x + size * 0.3f, c.y - size * 0.8f }, { c.x - size * 0.3f, c.y - size * 0.8f }, RED);
            DrawTriangle({ c.x + size * 0.4f, c.y }, { c.x + size * 0.85f, c.y + size * 0.1f }, { c.x + size * 0.4f, c.y + size * 0.25f }, GOLD);
            break;
        }
        case TraitType::Hypochondriac: {
            // Giant exclamation mark and pulse
            DrawText("!", static_cast<int>(c.x - size * 0.3f), static_cast<int>(c.y - size * 0.9f), static_cast<int>(size * 1.8f), RED);
            break;
        }
        case TraitType::Flexible: {
            // 180-degree U-turn flexible arc
            DrawCircleSector(c, size * 0.75f, 90.0f, 270.0f, 10, col);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.45f, BLACK);
            break;
        }
        case TraitType::EideticMemory: {
            // Eye with pinpoint memory dot
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.75f, col);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 3.0f, RED);
            break;
        }
        case TraitType::TheOne: {
            // Digital terminal glyphs [>_]
            DrawText(">", static_cast<int>(c.x - size * 0.6f), static_cast<int>(c.y - size * 0.6f), static_cast<int>(size * 1.1f), GREEN);
            DrawLineEx({ c.x + size * 0.1f, c.y + size * 0.4f }, { c.x + size * 0.6f, c.y + size * 0.4f }, 2.0f, GREEN);
            break;
        }
        case TraitType::Clumsy: {
            // Smashed shattered shards
            DrawTriangle({ c.x - size * 0.6f, c.y }, { c.x - size * 0.2f, c.y - size * 0.7f }, { c.x, c.y }, col);
            DrawTriangle({ c.x, c.y + size * 0.2f }, { c.x + size * 0.7f, c.y - size * 0.4f }, { c.x + size * 0.4f, c.y + size * 0.7f }, Fade(col, 0.7f));
            break;
        }
        case TraitType::EHS: {
            // Permanent lightning energy platform
            DrawLineEx({ c.x - size * 0.8f, c.y + size * 0.4f }, { c.x + size * 0.8f, c.y + size * 0.4f }, 3.0f, SKYBLUE);
            DrawLineEx({ c.x - size * 0.3f, c.y - size * 0.5f }, { c.x, c.y + size * 0.2f }, 2.0f, GOLD);
            break;
        }
        case TraitType::Glaucoma: {
            // Heavy black iris with tiny bright pinhole
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.8f, BLACK);
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), size * 0.8f, col);
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y), 2.5f, WHITE);
            break;
        }
        case TraitType::Clonus: {
            // Seismograph vibration wave
            Vector2 p1 = { c.x - size * 0.8f, c.y };
            Vector2 p2 = { c.x - size * 0.3f, c.y - size * 0.7f };
            Vector2 p3 = { c.x, c.y + size * 0.7f };
            Vector2 p4 = { c.x + size * 0.4f, c.y - size * 0.5f };
            Vector2 p5 = { c.x + size * 0.8f, c.y };
            DrawLineStrip(std::array<Vector2, 5>{ p1, p2, p3, p4, p5 }.data(), 5, col);
            break;
        }
        case TraitType::Prosopagnosia: {
            // Blank portrait silhouette with question mark
            DrawCircle(static_cast<int>(c.x), static_cast<int>(c.y - size * 0.3f), size * 0.45f, Fade(col, 0.5f));
            DrawText("?", static_cast<int>(c.x - size * 0.2f), static_cast<int>(c.y - size * 0.45f), static_cast<int>(size * 0.9f), WHITE);
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
        case TraitType::OCD:
        case TraitType::Hypergonadism:
        case TraitType::Flexible:
        case TraitType::EideticMemory:
        case TraitType::EHS:
            themeCol = Color{ 85, 230, 140, 255 }; // Green buff
            break;
        case TraitType::Gigantism:
        case TraitType::Endomorph:
        case TraitType::Vertigo:
        case TraitType::Gay:
        case TraitType::Baldness:
        case TraitType::StereoBlind:
        case TraitType::Ambilevous:
        case TraitType::Clumsy:
            themeCol = Color{ 255, 185, 55, 255 }; // Amber/Yellow
            break;
        case TraitType::Dwarfism:
        case TraitType::Ectomorph:
        case TraitType::NearSighted:
        case TraitType::FarSighted:
        case TraitType::TheOne:
        case TraitType::Hypochondriac:
            themeCol = Color{ 120, 210, 255, 255 }; // Cyan
            break;
        case TraitType::Dextrocardia:
        case TraitType::Dementia:
        case TraitType::CIP:
        case TraitType::Alzheimers:
        case TraitType::Coprolalia:
        case TraitType::MuscleWk:
        case TraitType::TunnelVision:
        case TraitType::Alektorophobia:
        case TraitType::Glaucoma:
        case TraitType::Clonus:
        case TraitType::Prosopagnosia:
        case TraitType::Dyslexia:
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
