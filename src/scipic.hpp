#pragma once
#include <span>
#include <memory>
#include "tigr.h"

enum SCICommand {
    setVisualColor = 0xf0,
    disableVisual = 0xf1,
    setPriorityColor = 0xf2,
    disablePriority = 0xf3,
    shortRelativePatterns = 0xf4,
    mediumRelativeLines = 0xf5,
    longLines = 0xf6,
    shortRelativeLines = 0xf7,
    floodFill = 0xf8,
    setPattern = 0xf9,
    longPatterns = 0xfa,
    setControlColor = 0xfb,
    disableControl = 0xfc,
    mediumRelativePatterns = 0xfd,
    extendedCommand = 0xfe,
    pictureEnd = 0xff,
};

enum SCIExtendedCommand {
    setPaletteEntries = 0,
    setEntirePalette = 1,
    setMonoPalette = 2,
    setMonoVisual = 3,
    setMonoDisableVisual = 4,
    setMonoDirectVisual = 5,
    setMonoDisableDirectVisual = 6,
    embedCel = 7,
    setPriorityBands = 8,
};

using SCIPalette = std::array<std::pair<uint8_t, uint8_t>, 40>;

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

    void line(int x0, int y0, int x1, int y1);

    std::span<const uint8_t> _data;
    size_t _pos{ 0 };
    uint8_t _visualColor{ 0 };
    uint8_t _priorityColor{ 0 };
    uint8_t _controlColor{ 0 };
    uint8_t _patternFlags{ 0 };
    std::array<SCIPalette, 4> _palettes;
    std::unique_ptr<Tigr, decltype(&tigrFree)> _bmp;
};
