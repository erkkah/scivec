#pragma once

#include <utility>
#include <unordered_map>

#include "image.hpp"

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
    Palette(std::vector<const PaletteColor> colors);
    PaletteColor operator[](size_t index) const;
    size_t size() const;
    int index(const PaletteColor& color) const;
    int match(int x, int y, uint8_t egaColor) const;
    std::span<const PaletteColor> colors() const;

   private:
    const std::vector<const PaletteColor> _colors;
    std::unordered_map<PaletteColor, size_t, ColorHash> _indexMap;
};

Palette buildPalette(const EGAImage& img);
