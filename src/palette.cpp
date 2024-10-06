#include "palette.hpp"
#include <unordered_set>
#include <cassert>
#include <algorithm>

Palette::Palette(std::span<const PaletteColor> colors) : _colors(colors.begin(), colors.end()) {
}

void Palette::updateIndexMap() const {
    if (!_indexMapIsStale) {
        return;
    }
    _indexMap.erase(_indexMap.begin(), _indexMap.end());
    for (int i = 0; i < _colors.size(); i++) {
        _indexMap[_colors[i]] = i;
    }
    _indexMapIsStale = false;
}

const PaletteColor& Palette::get(size_t index) const {
    assert(index < _colors.size());
    return _colors[index];
}

void Palette::set(size_t index, const PaletteColor& color) {
    assert(index < _colors.size());
    _colors[index] = color;
    _indexMapIsStale = true;
}

size_t Palette::size() const {
    return _colors.size();
}

int Palette::index(const PaletteColor& color) const {
    updateIndexMap();
    if (_indexMap.contains(color)) {
        return _indexMap.at(color);
    }
    return -1;
}

int Palette::match(int x, int y, uint8_t egaColor) const {
    const int fullMatch = index({ egaColor, egaColor });
    if (fullMatch != -1) {
        return fullMatch;
    }
    for (int i = 0; i < _colors.size(); i++) {
        auto c = _colors.at(i);
        if (effectiveColor(c, x, y) == egaColor) {
            return i;
        }
    }
    return -1;
}

std::span<const PaletteColor> Palette::colors() const {
    return _colors;
}

uint8_t effectiveColor(const PaletteColor& col, int x, int y) {
    return ((x + y) % 2) ? col.first : col.second;
}
