#include "scipic.hpp"
#include <sstream>

std::string hex(int value) {
    std::stringstream str;
    str << std::hex << value;
    return "0x" + str.str();
}

void SCIPicParser::parse() {
    if (_data[0] != 0x81 || _data[1] != 0x00) {
        throw std::runtime_error("Invalid SCI resource");
    }

    _pos = 2;

    while (_pos < _data.size()) {
        auto cmd = _data[_pos++];

        switch (cmd) {
            case setVisualColor:
                _visualColor = _data[_pos++];
                break;

            case disableVisual:
                break;

            case setPriorityColor:
                _priorityColor = _data[_pos++];
                break;

            case disablePriority:
                break;

            case setControlColor:
                _controlColor = _data[_pos++];
                break;

            case disableControl:
                break;

            case longLines:
                parseLongLines();
                break;

            case shortRelativeLines:
                parseShortRelativeLines();
                break;

            case mediumRelativeLines:
                parseMediumRelativeLines();
                break;

            case setPattern:
                _patternFlags = _data[_pos++];
                break;

            case shortRelativePatterns:
                parseShortRelativePatterns();
                break;

            case mediumRelativePatterns:
                parseMediumRelativePatterns();
                break;

            case longPatterns:
                parseLongPatterns();
                break;

            case floodFill:
                parseFloodFill();
                break;

            case extendedCommand:
                parseExtended(_data[_pos++]);
                break;

            case pictureEnd:
                return;

            default:
                throw std::runtime_error("Unhandled command " + hex(cmd));
        }
    }
}

std::pair<int, int> SCIPicParser::readCoordinate() {
    const auto upperXY = _data[_pos++];
    const auto lowerX = _data[_pos++];
    const auto lowerY = _data[_pos++];
    return std::make_pair((upperXY & 0xf0) << 4 | lowerX, upperXY & 0xf | lowerY);
}

bool SCIPicParser::nextIsCommand() const {
    return (_data[_pos] & 0xf0) == 0xf0;
}

void SCIPicParser::parseLongLines() {
    auto first = readCoordinate();
    auto second = readCoordinate();

    while (!nextIsCommand()) {
        first = second;
        second = readCoordinate();
    }
}

namespace {
int signMagnitudeOffset(uint8_t v) {
    const auto sign = -1 * (v & 0x80) >> 7;
    return (v & 0x7f) * sign;
}

int twosComplementOffset(uint8_t v) {
    return *reinterpret_cast<int8_t*>(&v);
}

std::pair<int, int> addFourBitOffsets(const std::pair<int, int>& p, uint8_t v) {
    int8_t xOffset = ((v & 0xf0) >> 4) | (0xf0 * ((v & 0x80) >> 7));
    int8_t yOffset = (v & 0xf) | (0xf0 * ((v & 0x8) >> 3));
    return std::make_pair(p.first + xOffset, p.second + yOffset);
}

}  // namespace

void SCIPicParser::parseShortRelativeLines() {
    auto first = readCoordinate();

    do {
        const auto second = addFourBitOffsets(first, _data[_pos++]);
        first = second;
    } while (!nextIsCommand());
}

void SCIPicParser::parseMediumRelativeLines() {
    auto first = readCoordinate();

    do {
        const auto yOffset = signMagnitudeOffset(_data[_pos++]);
        const auto xOffset = twosComplementOffset(_data[_pos++]);

        const auto second = std::make_pair(first.first + xOffset, first.second + yOffset);
        first = second;
    } while (!nextIsCommand());
}

void SCIPicParser::parseShortRelativePatterns() {
    uint8_t pattern = 0;
    if ((_patternFlags & 0x20) != 0) {
        pattern = _data[_pos++];
    }

    auto position = readCoordinate();
    pattern = pattern;
    // plot pattern at posision

    while (!nextIsCommand()) {
        if ((_patternFlags & 0x20) != 0) {
            pattern = _data[_pos++];
        }

        position = addFourBitOffsets(position, _data[_pos++]);
        pattern = pattern;
        // plot pattern at posision
    }
}

void SCIPicParser::parseMediumRelativePatterns() {
    uint8_t pattern = 0;
    if ((_patternFlags & 0x20) != 0) {
        pattern = _data[_pos++];
    }

    auto position = readCoordinate();
    pattern = pattern;
    // plot pattern at posision

    while (!nextIsCommand()) {
        if ((_patternFlags & 0x20) != 0) {
            pattern = _data[_pos++];
        }

        const auto yOffset = signMagnitudeOffset(_data[_pos++]);
        const auto xOffset = twosComplementOffset(_data[_pos++]);

        position = std::make_pair(position.first + xOffset, position.second + yOffset);

        pattern = pattern;
        // plot pattern at posision
    }
}

void SCIPicParser::parseLongPatterns() {
    uint8_t pattern = 0;
    if ((_patternFlags & 0x20) != 0) {
        pattern = _data[_pos++];
    }

    do {
        auto position = readCoordinate();
        pattern = pattern;
        position = position;
    } while (!nextIsCommand());
}

void SCIPicParser::parseFloodFill() {
    while (!nextIsCommand()) {
        auto position = readCoordinate();
        // flood fill at position
        position = position;
    }
}

void SCIPicParser::parseExtended(uint8_t cmd) {
    switch (cmd) {
        case setPaletteEntries: {
            while (_pos <= _data.size() && !nextIsCommand()) {
                const uint8_t i = _data[_pos++];
                const uint8_t color = _data[_pos++];
                for (auto& palette : _palettes) {
                    palette[i].first = (color & 0xf0) >> 4;
                    palette[i].second = color & 0xf;
                }
            }
        } break;

        case setEntirePalette: {
            const uint8_t p = _data[_pos++];
            if (p > 3) {
                throw std::runtime_error("Invalid palette index");
            }

            auto& palette = _palettes[p];

            for (auto i = 0; i < palette.size(); i++) {
                const uint8_t color = _data[_pos++];
                palette[i].first = (color & 0xf0) >> 4;
                palette[i].second = color & 0xf;
            }
        } break;

        default:
            throw std::runtime_error("Unhandled extended command " + hex(cmd));
    }
}