#pragma once
#include <vector>

#include "palette.hpp"

enum SCICommandCode {
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

enum SCIExtendedCommandCode {
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

constexpr uint8_t patternFlagRectangle = 0x10;
constexpr uint8_t patternFlagUsePattern = 0x20;

struct Point {
    Point() = default;
    Point(const Point& other) = default;
    Point& operator=(const Point& other) = default;

    Point(int x, int y) : x(x), y(y) {
    }

    Point(int x, int y, int color) : x(x), y(y), color(color) {
    }

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y && color == other.color;
    }

    bool empty() const {
        return x == -1 && y == -1 && color == -1;
    }

    int x{ -1 };
    int y{ -1 };
    int color{ -1 };
};

struct SCICommand {
    SCICommandCode code;
    std::vector<uint8_t> params;
};
