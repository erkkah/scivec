#pragma once
#include <span>
#include <memory>
#include <vector>
#include <cassert>

#include "tigr.h"
#include "image.hpp"
#include "scipic.hpp"

struct SCIPicParser {
    SCIPicParser(std::span<const uint8_t> data) : _data(data), _bmp(tigrBitmap(320, 200), &tigrFree) {
    }

    void parse();
    Tigr* bitmap() {
        return _bmp.get();
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
    uint8_t _paletteIndex{ 0 };
    uint8_t _color{ 0 };
    uint8_t _patternFlags{ 0 };
    std::array<SCIPalette, 4> _palettes;
    std::unique_ptr<Tigr, decltype(&tigrFree)> _bmp;
};
