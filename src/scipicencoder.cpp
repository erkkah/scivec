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

void encodeShortLines(std::span<const Point> coordinates, std::vector<SCICommand>& sink) {
    assert(coordinates.size() > 1);
    auto p0 = coordinates.front();
    std::vector<uint8_t> params = encodeCoordinate(p0.x, p0.y);

    for (auto p : coordinates.subspan(1)) {
        auto xDiff = p.x - p0.x;
        auto yDiff = p.y - p0.y;

        assert(xDiff > -8 && xDiff < 8);
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

    sink.push_back(SCICommand{ .code = SCICommandCode::shortRelativeLines, .params = params });
}

void encodeMediumLines(std::span<const Point> coordinates, std::vector<SCICommand>& sink) {
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
        params.push_back(yDelta);
        params.push_back(xDiff);
        p0 = p;
    }

    sink.push_back(SCICommand{ .code = SCICommandCode::mediumRelativeLines, .params = params });
}

void encodeLongLines(std::span<const Point> coordinates, std::vector<SCICommand>& sink) {
    std::vector<uint8_t> params;

    for (const auto& coord : coordinates) {
        auto coordinateData = encodeCoordinate(coord.x, coord.y);
        params.insert(params.end(), coordinateData.begin(), coordinateData.end());
    }

    sink.push_back(SCICommand{ .code = SCICommandCode::longLines, .params = params });
}

enum class LineMode { shortLine, mediumLine, longLine };

LineMode modeFromPoints(const Point& p0, const Point& p1) {
    auto xDistance = std::abs(p0.x - p1.x);
    auto yDistance = std::abs(p0.y - p1.y);
    auto distance = std::max(xDistance, yDistance);

    // short encodes
    // -6 <= y <= 7
    // -7 <= x <= 7
    if (distance < 7) {
        return LineMode::shortLine;
    }

    // medium encodes
    // -127 <= y <= 127
    // -128 <= x <= 127
    if (distance < 128) {
        return LineMode::mediumLine;
    }

    return LineMode::longLine;
}

void encodeLineSegment(std::span<const Point> coordinates, LineMode mode, std::vector<SCICommand>& sink) {
    switch (mode) {
        case LineMode::longLine:
            encodeLongLines(coordinates, sink);
            break;
        case LineMode::mediumLine:
            encodeMediumLines(coordinates, sink);
            break;
        case LineMode::shortLine:
            encodeShortLines(coordinates, sink);
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
            encodeLineSegment(segment, currentMode, sink);
            segment.clear();
            segment.push_back(p0);
            segment.push_back(p1);
            currentMode = mode;
        }
        p0 = p1;
    }

    if (!segment.empty()) {
        encodeLineSegment(segment, currentMode, sink);
    }
}

SCICommand encodeSolidCirclePattern(uint8_t size) {
    return SCICommand{ .code = SCICommandCode::setPattern, .params = { size } };
}

SCICommand encodePatterns(std::span<const Point> coordinates) {
    std::vector<uint8_t> params;
    for (const auto& c : coordinates) {
        auto coordData = encodeCoordinate(c.x, c.y);
        params.insert(params.end(), coordData.begin(), coordData.end());
    }
    return SCICommand{ .code = SCICommandCode::longPatterns, .params = params };
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