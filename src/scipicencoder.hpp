#pragma once
#include "scipic.hpp"

std::vector<uint8_t> encodeCoordinate(int x, int y);
SCICommand encodeVisual(uint8_t color);
void encodeMultiLine(std::span<const Point> coordinates, std::vector<SCICommand>& sink);
SCICommand encodeFill(int x, int y);
