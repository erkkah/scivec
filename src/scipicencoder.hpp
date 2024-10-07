#pragma once
#include "scipic.hpp"

std::vector<uint8_t> encodeCoordinate(int x, int y);
SCICommand encodeVisual(uint8_t color);
SCICommand encodeMultiLine(std::span<const Point> coordinates);
SCICommand encodeFill(int x, int y);
