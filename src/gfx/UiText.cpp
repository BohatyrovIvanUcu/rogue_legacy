// === FILE: src/gfx/UiText.cpp ===
#include "gfx/UiText.hpp"
#include <string>
#include <vector>
#include <cctype>
#include <utility>
#include <cstdint>

namespace {
    Font g_font{};
    bool g_hasCustomFont = false;
    constexpr int FONT_BASE_SIZE = 48; // Оптимальний розмір для Full HD: чіткі лінії без розмиття
 
    const char* const FONT_CANDIDATES[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };

    Font ActiveFont() {
        return g_hasCustomFont ? g_font : GetFontDefault();
    }

    float Spacing(int fontSize) {
        return g_hasCustomFont ? 0.0f : fontSize / 10.0f;
    }
}

namespace Ui {

void Init() {
    // ASCII + маркер списку "•"
    std::vector<int> codepoints;
    for (int c = 32; c < 127; ++c) codepoints.push_back(c);
    codepoints.push_back(0x2022);

    for (const char* path : FONT_CANDIDATES) {
        if (!FileExists(path)) continue;
        Font f = LoadFontEx(path, FONT_BASE_SIZE, codepoints.data(), static_cast<int>(codepoints.size()));
        if (f.texture.id != 0 && f.glyphCount > 0) {
            g_font = f;
            g_hasCustomFont = true;
            GenTextureMipmaps(&g_font.texture);
            SetTextureFilter(g_font.texture, TEXTURE_FILTER_TRILINEAR);
            break;
        }
    }
}

void Shutdown() {
    if (g_hasCustomFont) {
        UnloadFont(g_font);
        g_hasCustomFont = false;
    }
}

void Text(const char* text, int x, int y, int fontSize, Color color) {
    DrawTextEx(ActiveFont(), text, { static_cast<float>(x), static_cast<float>(y) },
               static_cast<float>(fontSize), Spacing(fontSize), color);
}

int Measure(const char* text, int fontSize) {
    return static_cast<int>(MeasureTextEx(ActiveFont(), text, static_cast<float>(fontSize), Spacing(fontSize)).x);
}

int TextWrapped(const char* text, int x, int y, int maxWidth, int fontSize, Color color) {
    const int lineHeight = static_cast<int>(fontSize * 1.25f);
    std::string src = text;
    std::string line;
    int lineY = y;
    size_t pos = 0;

    while (pos <= src.size()) {
        size_t next = src.find(' ', pos);
        if (next == std::string::npos) next = src.size();
        std::string word = src.substr(pos, next - pos);
        std::string candidate = line.empty() ? word : line + " " + word;

        if (!line.empty() && Measure(candidate.c_str(), fontSize) > maxWidth) {
            Text(line.c_str(), x, lineY, fontSize, color);
            lineY += lineHeight;
            line = word;
        } else {
            line = candidate;
        }
        pos = next + 1;
    }
    if (!line.empty()) {
        Text(line.c_str(), x, lineY, fontSize, color);
        lineY += lineHeight;
    }
    return lineY - y;
}

std::string ScrambleDyslexia(std::string_view text) {
    std::string result(text);
    size_t i = 0;
    while (i < result.size()) {
        if (std::isalpha(static_cast<unsigned char>(result[i]))) {
            size_t start = i;
            while (i < result.size() && std::isalpha(static_cast<unsigned char>(result[i]))) {
                i++;
            }
            size_t end = i;
            size_t len = end - start;
            if (len >= 4) {
                size_t midStart = start + 1;
                size_t midLen = len - 2;
                if (midLen == 2) {
                    std::swap(result[midStart], result[midStart + 1]);
                } else if (midLen > 2) {
                    uint32_t seed = 2166136261u;
                    for (size_t k = start; k < end; ++k) {
                        seed = (seed ^ static_cast<unsigned char>(result[k])) * 16777619u;
                    }
                    for (size_t m = midLen - 1; m > 0; --m) {
                        seed = seed * 1664525u + 1013904223u;
                        size_t j = (seed >> 16) % (m + 1);
                        std::swap(result[midStart + m], result[midStart + j]);
                    }
                    // Переконуємось, що перестановка дійсно відрізняється від оригіналу
                    bool changed = false;
                    for (size_t k = 0; k < midLen; ++k) {
                        if (result[midStart + k] != text[midStart + k]) {
                            changed = true;
                            break;
                        }
                    }
                    if (!changed && midLen >= 2) {
                        std::swap(result[midStart], result[midStart + midLen - 1]);
                    }
                }
            }
        } else {
            i++;
        }
    }
    return result;
}

} // namespace Ui

