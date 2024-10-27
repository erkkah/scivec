#pragma once
#include "scipic.hpp"

std::vector<uint8_t> encodeCoordinate(int x, int y);
SCICommand encodeVisual(uint8_t color);
SCICommand encodeSolidCirclePattern(uint8_t size);
void encodeMultiLine(std::span<const Point> coordinates, std::vector<SCICommand>& sink);
SCICommand encodePatterns(std::span<const Point> coordinates);
SCICommand encodeFill(int x, int y);
void encodeColors(const Palette& colors, std::vector<SCICommand>& sink);
