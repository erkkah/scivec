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

struct SCICommand {
    SCICommandCode code;
    std::vector<uint8_t> params;
};

using SCIPalette = std::array<PaletteColor, 40>;
