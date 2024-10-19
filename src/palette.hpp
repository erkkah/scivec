#pragma once

#include <utility>
#include <unordered_map>
#include <vector>
#include <span>
#include <cstdint>

constexpr size_t paletteSize = 40;
constexpr size_t maxColors = 4 * paletteSize;

using PaletteColor = std::pair<uint8_t, uint8_t>;

uint8_t effectiveColor(const PaletteColor& col, int x, int y);

struct ColorHash {
    std::size_t operator()(const PaletteColor& c) const noexcept {
        std::size_t h1 = std::hash<int>{}(c.first);
        std::size_t h2 = std::hash<int>{}(c.second);
        return h1 ^ (h2 << 1);
    }
};

struct Palette {
    Palette(std::span<const PaletteColor> colors);
    const PaletteColor& get(size_t index) const;
    void set(size_t index, const PaletteColor& color);
    size_t size() const;
    int index(const PaletteColor& color) const;
    int match(int x, int y, uint8_t egaColor) const;
    std::span<const PaletteColor> colors() const;

   private:
    void updateIndexMap() const;
    std::vector<PaletteColor> _colors;
    mutable bool _indexMapIsStale{ true };
    mutable std::unordered_map<PaletteColor, size_t, ColorHash> _indexMap;
};
