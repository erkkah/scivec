#include "scipicencoder.hpp"
#include <cassert>

std::vector<uint8_t> encodeCoordinate(int x, int y) {
    const int upperX = x & 0xf00;
    const int upperY = y & 0xf00;
    const uint8_t upperXY = (upperX >> 4) | (upperY >> 8);
    const uint8_t lowerX = x & 0xff;
    const uint8_t lowerY = y & 0xff;
    return { upperXY, lowerX, lowerY };
}

SCICommand encodeVisual(uint8_t color) {
    return SCICommand{ .code = SCICommandCode::setVisualColor, .params = { color } };
}

namespace {

void encodeShortCommand(SCICommandCode command, std::span<const Point> coordinates, std::vector<SCICommand>& sink) {
    assert(coordinates.size() > 1);
    auto p0 = coordinates.front();
    std::vector<uint8_t> params = encodeCoordinate(p0.x, p0.y);

    for (auto p : coordinates.subspan(1)) {
        auto xDiff = p.x - p0.x;
        auto yDiff = p.y - p0.y;

        assert(xDiff > -7 && xDiff < 8);
        assert(yDiff > -8 && yDiff < 8);

        uint8_t delta = std::abs(xDiff) << 4;
        if (xDiff < 0) {
            delta |= 0x80;
        }
        delta |= std::abs(yDiff);
        if (yDiff < 0) {
            delta |= 0x08;
        }
        params.push_back(delta);

        p0 = p;
    }

    sink.push_back(SCICommand{ .code = command, .params = params });
}

void encodeMediumCommand(SCICommandCode command, std::span<const Point> coordinates, std::vector<SCICommand>& sink) {
    assert(coordinates.size() > 1);
    auto p0 = coordinates.front();
    std::vector<uint8_t> params = encodeCoordinate(p0.x, p0.y);

    for (auto p : coordinates.subspan(1)) {
        auto xDiff = p.x - p0.x;
        auto yDiff = p.y - p0.y;

        assert(xDiff > -128 && xDiff < 128);
        assert(yDiff > -128 && yDiff < 128);

        uint8_t yDelta = std::abs(yDiff);
        if (yDiff < 0) {
            yDelta |= 0x80;
        }
        assert(yDelta < 0xf0);
        assert(xDiff < 0xf0);
        params.push_back(yDelta);
        params.push_back(xDiff);
        p0 = p;
    }

    sink.push_back(SCICommand{ .code = command, .params = params });
}

void encodeLongCommand(SCICommandCode command, std::span<const Point> coordinates, std::vector<SCICommand>& sink) {
    std::vector<uint8_t> params;

    for (const auto& coord : coordinates) {
        auto coordinateData = encodeCoordinate(coord.x, coord.y);
        params.insert(params.end(), coordinateData.begin(), coordinateData.end());
    }

    sink.push_back(SCICommand{ .code = command, .params = params });
}

enum class CoordinateMode { shortCoord, mediumCoord, longCoord };

CoordinateMode modeFromPoints(const Point& p0, const Point& p1) {
    auto xDistance = std::abs(p0.x - p1.x);
    auto yVector = p1.y - p0.y;
    auto yDistance = std::abs(yVector);
    auto distance = std::max(xDistance, yDistance);

    // short encodes
    // -6 <= x <= 7
    // -7 <= y <= 7
    if (distance < 7) {
        return CoordinateMode::shortCoord;
    }

    // medium encodes
    // -128 <= x <= 127
    // -111 <= y <= 127
    // 112 + 128 (sign bit) = 240 == 0xf0, which must be avoided
    if (yVector > -111 && distance < 128) {
        return CoordinateMode::mediumCoord;
    }

    return CoordinateMode::longCoord;
}

SCICommandCode lineCodeFromMode(CoordinateMode mode) {
    switch (mode) {
        case CoordinateMode::shortCoord:
            return SCICommandCode::shortRelativeLines;
        case CoordinateMode::mediumCoord:
            return SCICommandCode::mediumRelativeLines;
        case CoordinateMode::longCoord:
            return SCICommandCode::longLines;
        default:
            assert(false);
    }
}

SCICommandCode patternCodeFromMode(CoordinateMode mode) {
    switch (mode) {
        case CoordinateMode::shortCoord:
            return SCICommandCode::shortRelativePatterns;
        case CoordinateMode::mediumCoord:
            return SCICommandCode::mediumRelativePatterns;
        case CoordinateMode::longCoord:
            return SCICommandCode::longPatterns;
        default:
            assert(false);
    }
}

void encodeSegment(SCICommandCode command,
    std::span<const Point> coordinates,
    CoordinateMode mode,
    std::vector<SCICommand>& sink) {
    switch (mode) {
        case CoordinateMode::longCoord:
            encodeLongCommand(command, coordinates, sink);
            break;
        case CoordinateMode::mediumCoord:
            encodeMediumCommand(command, coordinates, sink);
            break;
        case CoordinateMode::shortCoord:
            encodeShortCommand(command, coordinates, sink);
            break;
    }
}

}  // namespace

void encodeMultiLine(std::span<const Point> coordinates, std::vector<SCICommand>& sink) {
    assert(coordinates.size() > 1);

    std::vector<Point> segment;

    auto p0 = coordinates[0];
    auto p1 = coordinates[1];

    segment.push_back(p0);
    auto currentMode = modeFromPoints(p0, p1);

    for (auto p = coordinates.begin() + 1; p != coordinates.end(); p++) {
        p1 = *p;
        auto mode = modeFromPoints(p0, p1);
        if (mode == currentMode) {
            segment.push_back(p1);
        } else {
            encodeSegment(lineCodeFromMode(currentMode), segment, currentMode, sink);
            segment.clear();
            segment.push_back(p0);
            segment.push_back(p1);
            currentMode = mode;
        }
        p0 = p1;
    }

    if (!segment.empty()) {
        encodeSegment(lineCodeFromMode(currentMode), segment, currentMode, sink);
    }
}

SCICommand encodeSolidCirclePattern(uint8_t size) {
    return SCICommand{ .code = SCICommandCode::setPattern, .params = { size } };
}

void encodePatterns(std::span<const Point> coordinates, std::vector<SCICommand>& sink) {
    assert(!coordinates.empty());

    auto p0 = coordinates[0];
    auto currentMode = coordinates.size() == 1 ? CoordinateMode::longCoord : CoordinateMode::shortCoord;

    for (auto p = coordinates.begin() + 1; currentMode != CoordinateMode::longCoord && p != coordinates.end(); p++) {
        auto p1 = *p;
        const auto mode = modeFromPoints(p0, p1);
        if (mode > currentMode) {
            currentMode = mode;
        }
        p0 = p1;
    }

    encodeSegment(patternCodeFromMode(currentMode), coordinates, currentMode, sink);
}

SCICommand encodeFill(int x, int y) {
    return SCICommand{ .code = SCICommandCode::floodFill, .params = encodeCoordinate(x, y) };
}

void encodeColors(const Palette& palette, std::vector<SCICommand>& sink) {
    auto colors = palette.colors();
    int colorsLeft = colors.size();
    int colorIndex = 0;
    int paletteIndex = 0;

    while (colorsLeft >= 40) {
        std::vector<uint8_t> params{ SCIExtendedCommandCode::setEntirePalette };
        params.push_back(paletteIndex);
        const auto paletteColors = colors.subspan(paletteIndex * 40, 40);
        for (const auto color : paletteColors) {
            uint8_t colorValue = (color.first << 4) | color.second;
            params.push_back(colorValue);
        }
        sink.push_back(SCICommand{ .code = SCICommandCode::extendedCommand, .params = params });
        colorsLeft -= 40;
        paletteIndex++;
    }

    {
        std::vector<uint8_t> params{ SCIExtendedCommandCode::setPaletteEntries };
        const auto remainder = colors.subspan(paletteIndex * 40);
        for (int i = paletteIndex * 40; const auto& color : remainder) {
            params.push_back(i++);
            uint8_t colorValue = (color.first << 4) | color.second;
            params.push_back(colorValue);
        }
        sink.push_back(SCICommand{ .code = SCICommandCode::extendedCommand, .params = params });
    }
}