#include "scipicparser.hpp"
#include <sstream>
#include <set>

// http://sci.sierrahelp.com/Documentation/SCISpecifications/16-SCI0-SCI01PICResource.html
// http://sciwiki.sierrahelp.com/index.php?title=Picture_Resource
// http://agi.sierrahelp.com/Documentation/Specifications/5-1-PICTURE.html

namespace {

int lastX = 0;
int lastY = 0;

int signMagnitudeOffset(uint8_t v) {
    if ((v & 0x80) != 0) {
        return -(v & 0x7f);
    }
    return v;
}

int twosComplementOffset(uint8_t v) {
    return *reinterpret_cast<int8_t*>(&v);
}

std::pair<int, int> addFourBitOffsets(const std::pair<int, int>& p, uint8_t v) {
    int xOffset = (v & 0x80) != 0 ? -((v & 0x70) >> 4) : (v >> 4);
    int yOffset = (v & 0x08) != 0 ? -(v & 7) : (v & 7);
    return std::make_pair(p.first + xOffset, p.second + yOffset);
}

std::string hex(int value) {
    std::stringstream str;
    str << std::hex << value;
    return "0x" + str.str();
}

}  // namespace

void SCIPicParser::parse(int limit) {
    reset();

    if (peek(0) != 0x81 || peek(1) != 0x00) {
        throw std::runtime_error("Invalid SCI resource");
    }

    _bmp.clear(0x0f);

    skip(2);

    int count = 0;
    while (!atEnd() && (limit < 0 || count < limit)) {
        auto cmd = read();

        switch (cmd) {
            case setVisualColor: {
                auto colorCode = read();
                if (colorCode > 159) {
                    throw std::runtime_error("Invalid color index");
                }

                auto index = colorCode % 40;
                if (_lockedColors.contains(index)) {
                    colorCode = index;
                }
                _color = _palette.get(colorCode);

                _visualEnabled = true;
            } break;

            case disableVisual:
                _visualEnabled = false;
                break;

            case setPriorityColor:
                skip(1);
                break;

            case disablePriority:
                break;

            case setControlColor:
                skip(1);
                break;

            case disableControl:
                break;

            case longLines:
                count++;
                parseLongLines();
                break;

            case shortRelativeLines:
                count++;
                parseShortRelativeLines();
                break;

            case mediumRelativeLines:
                count++;
                parseMediumRelativeLines();
                break;

            case setPattern:
                _patternFlags = read();
                break;

            case shortRelativePatterns:
                count++;
                parseShortRelativePatterns();
                break;

            case mediumRelativePatterns:
                count++;
                parseMediumRelativePatterns();
                break;

            case longPatterns:
                count++;
                parseLongPatterns();
                break;

            case SCICommandCode::floodFill:
                count++;
                parseFloodFill();
                break;

            case extendedCommand:
                parseExtended(read());
                break;

            case pictureEnd:
                return;

            default:
                throw std::runtime_error("Unhandled command " + hex(cmd));
        }
    }
    // if (limit >= 1) {
    //     printf("Last coordinate: (%d:%d)\n", lastX, lastY);
    // }
}

/// Data stream stuff

std::uint8_t SCIPicParser::peek(size_t offset) const {
    if (_pos + offset < _data.size()) {
        return _data[_pos + offset];
    }
    throw std::runtime_error("Unexpected end of pic");
}

std::uint8_t SCIPicParser::read() {
    if (_pos < _data.size()) {
        return _data[_pos++];
    }
    throw std::runtime_error("Unexpected end of pic");
}

bool SCIPicParser::atEnd() const {
    return _pos >= _data.size();
}

void SCIPicParser::reset() {
    _pos = 0;
    _lockedColors.clear();
}

void SCIPicParser::skip(size_t count) {
    _pos += count;
}

/// Drawing

void SCIPicParser::drawLine(int x0, int y0, int x1, int y1) {
    if (!_visualEnabled || !_drawLines) {
        return;
    }

    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        plot(x0, y0);

        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void SCIPicParser::plot(int x, int y) {
    _bmp.put(x, y, effectiveColor(_color, x, y));
}

void SCIPicParser::floodFill(int x, int y) {
    if (!_visualEnabled || !_drawFills) {
        return;
    }

    const auto target = _bmp.get(x, y);

    // Only fill white (0x0f) pixels
    if (target != 0x0f) {
        return;
    }

    std::vector<Point> fills;
    std::set<std::pair<int, int>> filled;

    auto effective = effectiveColor(_color, x, y);
    _bmp.put(x, y, effective);
    fills.push_back({ x, y });
    filled.insert({ x, y });

    const auto check = [this, &fills, &filled](int x, int y) {
        if (x < 0 || x >= _bmp.width() || y < 0 || y >= _bmp.height()) {
            return;
        }

        if (filled.contains({ x, y })) {
            return;
        }

        if (_bmp.get(x, y) == 0x0f) {
            _bmp.put(x, y, effectiveColor(_color, x, y));
            fills.push_back({ x, y });
        }
        filled.insert({ x, y });
    };

    while (!fills.empty()) {
        const auto& fill = fills.back();
        int x = fill.x;
        int y = fill.y;
        fills.pop_back();

        check(x + 1, y);
        check(x - 1, y);
        check(x, y + 1);
        check(x, y - 1);
        if (fills.size() > 32768) {
            throw std::runtime_error("Fill stack overflow");
        }
    }
}

namespace {
// clang-format off

constexpr std::array<uint8_t, 30> circlePatterns[] = {
    { 0x80 },
    { 0x4e, 0x40 },
    { 0x73, 0xef, 0xbe, 0x70 },
    { 0x38, 0x7c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x00 },
    { 0x1c, 0x1f, 0xcf, 0xfb, 0xfe, 0xff, 0xbf, 0xef,
      0xf9, 0xfc, 0x1c },
    { 0x0e, 0x03, 0xf8, 0x7f, 0xc7, 0xfc, 0xff, 0xef,
      0xfe, 0xff, 0xe7, 0xfc, 0x7f, 0xc3, 0xf8, 0x1f,
      0x00 },
    { 0x0f, 0x80, 0xff, 0x87, 0xff, 0x1f, 0xfc, 0xff,
      0xfb, 0xff, 0xef, 0xff, 0xbf, 0xfe, 0xff, 0xf9,
      0xff, 0xc7, 0xff, 0x0f, 0xf8, 0x0f, 0x80 },
    { 0x07, 0xc0, 0x1f, 0xf0, 0x3f, 0xf8, 0x7f, 0xfc,
      0x7f, 0xfc, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe,
      0xff, 0xfe, 0xff, 0xfe, 0x7f, 0xfc, 0x7f, 0xfc,
      0x3f, 0xf8, 0x1f, 0xf0, 0x07, 0xc0 }
};

std::array<uint8_t, 32> patternData {
    0x20, 0x94, 0x02, 0x24, 0x90, 0x82, 0xa4, 0xa2,
    0x82, 0x09, 0x0a, 0x22, 0x12, 0x10, 0x42, 0x14,
    0x91, 0x4a, 0x91, 0x11, 0x08, 0x12, 0x25, 0x10,
    0x22, 0xa8, 0x14, 0x24, 0x00, 0x50, 0x24, 0x04,
};

std::array<uint8_t, 128> patternIndicies {
    0x00, 0x18, 0x30, 0xc4, 0xdc, 0x65, 0xeb, 0x48,
    0x60, 0xbd, 0x89, 0x05, 0x0a, 0xf4, 0x7d, 0x7d,
    0x85, 0xb0, 0x8e, 0x95, 0x1f, 0x22, 0x0d, 0xdf,
    0x2a, 0x78, 0xd5, 0x73, 0x1c, 0xb4, 0x40, 0xa1,
    0xb9, 0x3c, 0xca, 0x58, 0x92, 0x34, 0xcc, 0xce,
    0xd7, 0x42, 0x90, 0x0f, 0x8b, 0x7f, 0x32, 0xed,
    0x5c, 0x9d, 0xc8, 0x99, 0xad, 0x4e, 0x56, 0xa6,
    0xf7, 0x68, 0xb7, 0x25, 0x82, 0x37, 0x3a, 0x51,
    0x69, 0x26, 0x38, 0x52, 0x9e, 0x9a, 0x4f, 0xa7,
    0x43, 0x10, 0x80, 0xee, 0x3d, 0x59, 0x35, 0xcf,
    0x79, 0x74, 0xb5, 0xa2, 0xb1, 0x96, 0x23, 0xe0,
    0xbe, 0x05, 0xf5, 0x6e, 0x19, 0xc5, 0x66, 0x49,
    0xf0, 0xd1, 0x54, 0xa9, 0x70, 0x4b, 0xa4, 0xe2,
    0xe6, 0xe5, 0xab, 0xe4, 0xd2, 0xaa, 0x4c, 0xe3,
    0x06, 0x6f, 0xc6, 0x4a, 0xa4, 0x75, 0x97, 0xe1,
};

// clang-format on
}  // namespace

void SCIPicParser::drawPattern(int x, int y, int pattern) {
    if (!_visualEnabled || !_drawPatterns) {
        return;
    }

    int size = _patternFlags & 0x7;

    x = std::clamp(x, size, 319 - size);
    y = std::clamp(y, size, 189 - size);

    int patternIndex = (pattern >> 1) & 0x7f;
    if (patternIndex >= patternIndicies.size()) {
        return;
    }

    auto patternBit = patternIndicies[patternIndex];

    const auto drawRectangle = (_patternFlags & patternFlagRectangle) != 0;
    const auto usePattern = (_patternFlags & patternFlagUsePattern) != 0;

    if (drawRectangle) {
        for (int py = y - size; py <= y + size; py++) {
            for (int px = x - size; px <= (x + size + 1); px++) {
                if (usePattern) {
                    if ((patternData[patternBit >> 3] >> (7 - (patternBit & 7))) & 1) {
                        plot(px, py);
                    }
                    patternBit++;
                    if (patternBit == 0xff) {
                        patternBit = 0;
                    }
                } else {
                    plot(px, py);
                }
            }
        }
    } else {
        int circleBit = 0;
        for (int py = y - size; py <= y + size; py++) {
            for (int px = x - size; px <= x + size + 1; px++) {
                if ((circlePatterns[size][circleBit >> 3] >> (7 - (circleBit & 7))) & 1) {
                    if (usePattern) {
                        if ((patternData[patternBit >> 3] >> (7 - (patternBit & 7))) & 1) {
                            plot(px, py);
                        }
                        patternBit++;
                        if (patternBit == 0xff) {
                            patternBit = 0;
                        }
                    } else {
                        plot(px, py);
                    }
                }
                circleBit++;
            }
        }
    }
}

/// Parsing

std::pair<int, int> SCIPicParser::readCoordinate() {
    const auto upperXY = read();
    const auto lowerX = read();
    const auto lowerY = read();
    const auto coord = std::make_pair((upperXY & 0xf0) << 4 | lowerX, (upperXY & 0xf) << 8 | lowerY);
    lastX = coord.first;
    lastY = coord.second;
    return coord;
}

bool SCIPicParser::nextIsCommand() const {
    return peek(0) >= 0xf0;
}

void SCIPicParser::parseShortRelativeLines() {
    auto first = readCoordinate();

    do {
        const auto second = addFourBitOffsets(first, read());
        drawLine(first.first, first.second, second.first, second.second);
        first = second;
    } while (!nextIsCommand());
}

void SCIPicParser::parseMediumRelativeLines() {
    auto first = readCoordinate();

    do {
        const auto yOffset = signMagnitudeOffset(read());
        const auto xOffset = twosComplementOffset(read());
        const auto x0 = first.first;
        const auto y0 = first.second;
        auto x1 = x0 + xOffset;
        auto y1 = y0 + yOffset;
        x1 = std::clamp(x1, 0, 319);
        y1 = std::clamp(y1, 0, 189);
        drawLine(x0, y0, x1, y1);
        first = { x1, y1 };
    } while (!nextIsCommand());
}

void SCIPicParser::parseLongLines() {
    auto first = readCoordinate();

    while (!nextIsCommand()) {
        const auto second = readCoordinate();
        drawLine(first.first, first.second, second.first, second.second);
        first = second;
    }
}

void SCIPicParser::parseShortRelativePatterns() {
    uint8_t pattern = 0;
    if ((_patternFlags & patternFlagUsePattern) != 0) {
        pattern = read();
    }

    auto position = readCoordinate();
    drawPattern(position.first, position.second, pattern);

    while (!nextIsCommand()) {
        if ((_patternFlags & patternFlagUsePattern) != 0) {
            pattern = read();
        }

        position = addFourBitOffsets(position, read());
        drawPattern(position.first, position.second, pattern);
    }
}

void SCIPicParser::parseMediumRelativePatterns() {
    uint8_t pattern = 0;
    if ((_patternFlags & patternFlagUsePattern) != 0) {
        pattern = read();
    }

    auto position = readCoordinate();
    drawPattern(position.first, position.second, pattern);

    while (!nextIsCommand()) {
        if ((_patternFlags & patternFlagUsePattern) != 0) {
            pattern = read();
        }

        const auto yOffset = signMagnitudeOffset(read());
        const auto xOffset = twosComplementOffset(read());

        position = std::make_pair(position.first + xOffset, position.second + yOffset);

        drawPattern(position.first, position.second, pattern);
    }
}

void SCIPicParser::parseLongPatterns() {
    while (!nextIsCommand()) {
        uint8_t pattern = 0;
        if ((_patternFlags & patternFlagUsePattern) != 0) {
            pattern = read();
        }

        auto position = readCoordinate();
        drawPattern(position.first, position.second, pattern);
    }
}

void SCIPicParser::parseFloodFill() {
    while (!nextIsCommand()) {
        auto position = readCoordinate();
        floodFill(position.first, position.second);
    }
}

void SCIPicParser::parseExtended(uint8_t cmd) {
    switch (cmd) {
        case setPaletteEntries: {
            while (_pos <= _data.size() && !nextIsCommand()) {
                const uint8_t i = read();
                const uint8_t color = read();
                if (i > 159) {
                    throw std::runtime_error("Invalid palette entry");
                }
                _palette.set(i, { (color & 0xf0) >> 4, color & 0xf });
                if (i / 40 == 0) {
                    _lockedColors.insert(i % 40);
                }
            }
        } break;

        case setEntirePalette: {
            const uint8_t p = read();
            if (p > 3) {
                throw std::runtime_error("Invalid palette index");
            }

            auto base = p * 40;

            for (auto i = 0; i < 40; i++) {
                const uint8_t color = read();
                _palette.set(base + i, { (color & 0xf0) >> 4, color & 0xf });
            }
        } break;

        default:
            throw std::runtime_error("Unhandled extended command " + hex(cmd));
    }
}