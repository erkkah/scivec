#include "scipicencoder.hpp"

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

SCICommand encodeMultiLine(std::span<const Point> coordinates) {
    std::vector<uint8_t> params;

    for (const auto& coord : coordinates) {
        auto coordinateData = encodeCoordinate(coord.x, coord.y);
        params.insert(params.end(), coordinateData.begin(), coordinateData.end());
    }

    return SCICommand{ .code = SCICommandCode::longLines, .params = params };
}

SCICommand encodeFill(int x, int y) {
    return SCICommand{ .code = SCICommandCode::floodFill, .params = encodeCoordinate(x, y) };
}
