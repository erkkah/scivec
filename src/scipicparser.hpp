#pragma once
#include <span>
#include <memory>
#include <vector>
#include <cassert>

#include "tigr.h"
#include "image.hpp"
#include "scipic.hpp"

// clang-format off
#define SCI_COLORS \
    {0x0,0x0}, {0x1,0x1}, {0x2,0x2}, {0x3,0x3}, {0x4,0x4}, {0x5,0x5}, {0x6,0x6}, {0x7,0x7}, \
    {0x8,0x8}, {0x9,0x9}, {0xa,0xa}, {0xb,0xb}, {0xc,0xc}, {0xd,0xd}, {0xe,0xe}, {0x8,0x8}, \
    {0x8,0x8}, {0x0,0x1}, {0x0,0x2}, {0x0,0x3}, {0x0,0x4}, {0x0,0x5}, {0x0,0x6}, {0x8,0x8}, \
    {0x8,0x8}, {0xf,0x9}, {0xf,0xa}, {0xf,0xb}, {0xf,0xc}, {0xf,0xd}, {0xf,0xe}, {0xf,0xf}, \
    {0x0,0x8}, {0x9,0x1}, {0x2,0xa}, {0x3,0xb}, {0x4,0xc}, {0x5,0xd}, {0x6,0xe}, {0x8,0x8}
// clang-format on

const PaletteColor defaultSCIPalette[] = { SCI_COLORS, SCI_COLORS, SCI_COLORS, SCI_COLORS };

struct SCIPicParser {
    SCIPicParser(std::span<const uint8_t> data) : _data(data), _bmp(320, 190), _palette(defaultSCIPalette) {
    }

    void parse(int limit = -1);
    auto bitmap() {
        return _bmp.asBitmap();
    }
    const auto& palette() const {
        return _palette;
    }

   private:
    void parseExtended(uint8_t cmd);
    void parseShortRelativeLines();
    void parseMediumRelativeLines();
    void parseLongLines();
    void parseShortRelativePatterns();
    void parseMediumRelativePatterns();
    void parseLongPatterns();
    void parseFloodFill();

    bool nextIsCommand() const;
    std::pair<int, int> readCoordinate();

    void plot(int x, int y);
    void drawLine(int x0, int y0, int x1, int y1);
    void floodFill(int x, int y);
    void drawPattern(int x, int y, int pattern);

    std::uint8_t peek(size_t offset) const;
    std::uint8_t read();
    void skip(size_t count);
    bool atEnd() const;
    void reset();

    bool _drawLines{ true };
    bool _drawPatterns{ true };
    bool _drawFills{ true };

    std::span<const uint8_t> _data;
    size_t _pos{ 0 };
    bool _visualEnabled{ true };
    PaletteColor _color{ 0, 0 };
    uint8_t _patternFlags{ 0 };
    Palette _palette;
    EGAImage _bmp;
};
