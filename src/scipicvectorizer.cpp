#include "scipicvectorizer.hpp"
#include <cassert>
#include <span>
#include <ranges>
#include <set>
#include <map>
#include <vector>

int SCIPicVectorizer::pickColor(int x, int y, int leftColor, std::span<int> previousRow) const {
    const auto colorAt = [this](int x, int y, int dx, int dy) {
        assert(abs(dx) == 1 || abs(dy) == 1);
        assert(x + dx >= 0);
        assert(y + dy >= 0);

        auto first = _bmp.get(x, y);
        auto second = _bmp.get(x + dx, y + dy);

        if ((x + y) % 2 == 0) {
            std::swap(first, second);
        }

        const auto c = PaletteColor(first, second);
        const auto colorIndex = _colors.index(c);
        if (colorIndex != -1) {
            return colorIndex;
        }
        return _colors.match(x, y, _bmp.get(x, y));
    };

    if (leftColor != -1 && effectiveColor(_colors[leftColor], x, y) == _bmp.get(x, y)) {
        return leftColor;
    }

    auto upperColor = previousRow[x];
    if (upperColor != -1 && effectiveColor(_colors[upperColor], x, y) == _bmp.get(x, y)) {
        return upperColor;
    }

    std::vector<std::array<int, 2>> deltas{ { 1, 0 }, { 0, 1 } };
    if (x > 0) {
        deltas.push_back({ -1, 0 });
    }
    if (y > 0) {
        deltas.push_back({ 0, -1 });
    }

    std::map<int, int> counts;

    for (const auto& delta : deltas) {
        const auto c = colorAt(x, y, delta[0], delta[1]);
        if (c != -1) {
            counts[c]++;
        }
    }

    int maxCount = -1;
    int maxColor = -1;

    for (const auto& count : counts) {
        if (count.second > maxCount) {
            maxCount = count.second;
            maxColor = count.first;
        }
    }

    return maxColor;
}

PixelRunList SCIPicVectorizer::pixelRuns(int y, std::span<int> previousRow) {
    auto rowData = _bmp.row(y);

    PixelRunList result;

    int previousColor = -1;
    size_t runStart = 0;

    for (int x = 0; x < rowData.size() - 1; x++) {
        const auto currentColor = pickColor(x, y, previousColor, previousRow);
        previousRow[x] = currentColor;

        if (currentColor == -1) {
            throw std::runtime_error("Unhandled color case");
        }

        if (currentColor != previousColor && previousColor != -1) {
            result.emplace_back(runStart, x - runStart, static_cast<uint8_t>(previousColor));
            runStart = x;
        }

        previousColor = currentColor;
    }

    if (runStart < rowData.size()) {
        const auto lastX = rowData.size() - 1;
        const auto lastPixel = rowData.back();
        const auto lastColor = _colors.match(lastX, y, lastPixel);

        if (lastColor != previousColor) {
            result.emplace_back(runStart, lastX - runStart, static_cast<uint8_t>(previousColor));
            runStart = lastX;
        }

        result.emplace_back(runStart, rowData.size() - runStart, static_cast<uint8_t>(lastColor));
    }

    int total = 0;
    for (const auto& run : result) {
        total += run.length;
    }
    assert(total == rowData.size());

    return result;
}

std::vector<PixelArea> buildAreas(const std::vector<PixelRunList>& rows) {
    std::vector<PixelArea> allAreas;
    std::set<size_t> previousRowAreas;
    std::set<size_t> currentRowAreas;

    auto matchingArea = [&previousRowAreas, &allAreas](const PixelRun& run) -> int {
        for (auto& area : previousRowAreas) {
            if (allAreas[area].matches(run)) {
                return area;
            }
        }
        return -1;
    };

    for (int y = 0; const auto& row : rows) {
        for (const auto& run : row) {
            auto match = matchingArea(run);
            if (match != -1) {
                auto& area = allAreas.at(match);
                previousRowAreas.erase(match);
                currentRowAreas.insert(match);
                area.add(run);
            } else {
                currentRowAreas.insert(allAreas.size());
                allAreas.emplace_back(y, run);
            }
        }
        previousRowAreas = currentRowAreas;
        currentRowAreas.clear();
        y++;
    }

    return allAreas;
}

struct Point {
    int x;
    int y;
};

using Line = std::pair<Point, Point>;

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

SCICommand encodeLine(const Line& line) {
    auto coordinates = encodeCoordinate(line.first.x, line.first.y);
    auto endCoordinate = encodeCoordinate(line.second.x, line.second.y);
    coordinates.insert(coordinates.end(), endCoordinate.begin(), endCoordinate.end());
    return SCICommand{ .code = SCICommandCode::longLines, .params = coordinates };
}

SCICommand encodeMultiLine(std::span<const Point> coordinates) {
    std::vector<uint8_t> params;

    for (const auto& coord : coordinates) {
        auto coordinateData = encodeCoordinate(coord.x, coord.y);
        params.insert(params.end(), coordinateData.begin(), coordinateData.end());
    }

    return SCICommand{ .code = SCICommandCode::longLines, .params = params };
}

SCICommand encodeFill(int x, int y, uint8_t color) {
    return SCICommand{ .code = SCICommandCode::floodFill, .params = encodeCoordinate(x, y) };
}

std::vector<Point> optimizeLines(std::span<Point> coords) {
    std::vector<Point> result;

    result.push_back(coords.front());
    int sameXCount = 0;
    int sameYCount = 0;

    for (const auto& coord : coords.subspan(1)) {
        auto& last = result.back();

        if (coord.x == last.x) {
            sameYCount = 0;
            sameXCount++;
        } else if (coord.y == last.y) {
            sameXCount = 0;
            sameYCount++;
        } else {
            sameXCount = 0;
            sameYCount = 0;
        }

        if (sameXCount > 2) {
            last.y = coord.y;
        } else if (sameYCount > 2) {
            last.x = coord.x;
        } else {
            // Also check for same angle here?
            result.push_back(coord);
        }
    }

    return result;
}

void convertArea(const PixelArea& area, std::vector<SCICommand>& sink) {
    sink.push_back(encodeVisual(area.rows().front().color));

    if (area.height() < 4) {
        // treat as horizontal lines
        for (int y = area.top(); const auto& row : area.rows()) {
            sink.push_back(encodeLine(Line(Point{ .x = static_cast<int>(row.start), .y = y },
                Point{ .x = static_cast<int>(row.start + row.length - 1), .y = y })));
            y++;
        }
    } else {
        // draw a top line, a series of vertical/horizontal lines and a bottom line,
        // then flood-fill.
        const auto rows = area.rows();
        const auto& topLine = rows.front();
        const auto& bottomLine = rows.back();

        std::vector<Point> coords;

        coords.push_back(Point{ .x = static_cast<int>(topLine.start), .y = area.top() });

        int lastX = static_cast<int>(topLine.start + topLine.length - 1);

        for (int y = area.top(); const auto& row : rows) {
            int x = static_cast<int>(row.start + row.length - 1);
            if (x < lastX) {
                coords.push_back(Point{ .x = x, .y = y - 1 });
            } else if (x > lastX) {
                coords.push_back(Point{ .x = lastX, .y = y });
            }
            coords.push_back(Point{ .x = x, .y = y });
            lastX = x;
            y++;
        }

        lastX = static_cast<int>(rows.back().start);

        for (int y = area.top() + area.height() - 1; const auto& row : std::ranges::reverse_view{ rows }) {
            int x = static_cast<int>(row.start);
            if (x > lastX) {
                coords.push_back(Point{ .x = x, .y = y + 1 });
            } else if (x < lastX) {
                coords.push_back(Point{ .x = lastX, .y = y });
            }
            coords.push_back(Point{ .x = x, .y = y });
            lastX = x;
            y--;
        }

        coords = optimizeLines(coords);

        sink.push_back(encodeMultiLine(coords));

        // ??? Add fills here!
    }
}

void convertAreas(const std::vector<PixelArea>& areas, std::vector<SCICommand>& sink) {
    for (const auto& area : areas) {
        convertArea(area, sink);
    }
}

std::vector<SCICommand> SCIPicVectorizer::scan() {
    _areas.clear();

    std::vector<PixelRunList> scannedRows;

    std::vector<int> rowMemory(_bmp.width(), -1);

    for (int y = 0; y < _bmp.height(); y++) {
        const auto runs = pixelRuns(y, rowMemory);
        scannedRows.push_back(runs);
    }

    _areas = buildAreas(scannedRows);
    printf("Scanner found %zu areas\n", _areas.size());

    int totalPixels = 0;

    for (const auto& area : _areas) {
        const auto rows = area.rows();
        for (const auto& row : rows) {
            totalPixels += row.length;
        }
    }
    assert(totalPixels == _bmp.height() * _bmp.width());

    std::vector<SCICommand> commands;

    std::vector<uint8_t> params{ SCIExtendedCommandCode::setPaletteEntries };
    for (int i = 0; const auto& color : _colors.colors()) {
        params.push_back(i++);
        auto colorValue = color.first << 4 | color.second;
        params.push_back(colorValue);
    }
    commands.push_back(SCICommand{ .code = SCICommandCode::extendedCommand, .params = params });

    convertAreas(_areas, commands);
    printf("Produced %zu commands\n", commands.size());

    int totalBytes = 0;
    for (const auto& command : commands) {
        totalBytes += command.params.size() + 1;
    }
    printf("Size: %d bytes\n", totalBytes);

    return commands;
}

const PixelArea* SCIPicVectorizer::areaAt(int x, int y) const {
    for (const auto& area : _areas) {
        if (y < area.top() || y >= area.top() + area.height()) {
            continue;
        }
        for (int dy = 0; const auto& row : area.rows()) {
            if (y == area.top() + dy && x >= row.start && x < row.start + row.length) {
                return &area;
            }
            dy++;
        }
    }
    return nullptr;
}
