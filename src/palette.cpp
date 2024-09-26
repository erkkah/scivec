#include "palette.hpp"
#include <unordered_set>
#include <cassert>
#include <algorithm>

constexpr size_t paletteSize = 40;
constexpr size_t maxColors = 4 * paletteSize;

int missingColors(std::vector<const PaletteColor>& colors) {
    assert(colors.size() > maxColors);

    std::unordered_set<int> usedFirstColors;
    std::unordered_set<int> usedSecondColors;

    for (int i = 0; i < paletteSize; i++) {
        usedFirstColors.insert(colors[i].first);
        usedSecondColors.insert(colors[i].second);
    }

    std::unordered_set<int> missingFirstColors;
    std::unordered_set<int> missingSecondColors;

    for (auto it = colors.begin() + maxColors; it != colors.end(); it++) {
        if (!usedFirstColors.contains(it->first)) {
            missingFirstColors.insert(it->first);
        }
        if (!usedSecondColors.contains(it->second)) {
            missingSecondColors.insert(it->second);
        }
    }

    return missingFirstColors.size() + missingSecondColors.size();
}

Palette buildPalette(const EGAImage& bmp) {
    std::unordered_set<PaletteColor, ColorHash> colors;
    std::unordered_map<PaletteColor, int, ColorHash> colorCount;

    for (int x = 0; x < bmp.width() - 1; x++) {
        for (int y = 0; y < bmp.height(); y++) {
            const auto a = bmp.get(x, y);
            const auto b = bmp.get(x + 1, y);
            if (((x + y) % 2) != 0) {
                colorCount[std::make_pair(a, b)]++;
                colors.insert(std::make_pair(a, b));
            } else {
                colorCount[std::make_pair(b, a)]++;
                colors.insert(std::make_pair(b, a));
            }
        }
    }

    std::vector<std::pair<PaletteColor, int>> sortedColors;

    for (const auto& cc : colorCount) {
        sortedColors.push_back(cc);
    }

    std::sort(sortedColors.begin(),
        sortedColors.end(),
        [](const std::pair<PaletteColor, int>& a, const std::pair<PaletteColor, int>& b) {
            return a.second > b.second;
        });

    std::vector<const PaletteColor> palette;

    for (const auto& color : sortedColors) {
        palette.push_back(color.first);
    }

    if (palette.size() > maxColors) {
        int missing = missingColors(palette);
        if (missing > 0) {
            printf("Hm, the image is too colorful, %d colors will be approximated!\n", missing);
        }

        palette.resize(maxColors);
    }

    return palette;
}

Palette::Palette(std::vector<const PaletteColor> colors) : _colors(colors) {
    for (int i = 0; i < _colors.size(); i++) {
        _indexMap[_colors[i]] = i;
    }
}

PaletteColor Palette::operator[](size_t index) const {
    assert(index < _colors.size());
    return _colors[index];
}

size_t Palette::size() const {
    return _colors.size();
}

size_t Palette::index(const PaletteColor& color) const {
    if (_indexMap.contains(color)) {
        return _indexMap.at(color);
    }
    return -1;
}

size_t Palette::match(int x, int y, uint8_t egaColor) const {
    for (int i = 0; i < _colors.size(); i++) {
        auto c = _colors.at(i);
        if (effectiveColor(c, x, y) == egaColor) {
            return i;
        }
    }
    return -1;
}

uint8_t effectiveColor(const PaletteColor& col, int x, int y) {
    return ((x + y) % 2) ? col.first : col.second;
}
