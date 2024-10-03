#include "scipicvectorizer.hpp"
#include <cassert>
#include <span>
#include <ranges>
#include <set>
#include <map>
#include <vector>

void PixelArea::traceLines(const ByteImage& source) {
    if (_runs.empty()) {
        return;
    }

    if (_runs.size() == 1) {
        auto run = _runs.front();
        _lines.push_back(Point(run.start, run.row, run.color));
        _lines.push_back(Point(run.start + run.length - 1, run.row, run.color));
        return;
    }

    ByteImage workArea(source.width(), source.height());

    int startX = 0;
    int startY = 0;
    int color = -1;

    int minX = _runs.front().start;
    int maxX = minX;
    int minY = _runs.front().row;
    int maxY = minY;

    _runs.sort([](const auto& a, const auto& b) {
        if (a.row == b.row) {
            return a.start <= b.start;
        }
        return a.row < b.row;
    });

    for (auto& run : _runs) {
        int thisRow = run.row;
        int thisStart = run.start;
        int thisEnd = run.start + run.length - 1;
        int thisColor = run.color;

        if (color == -1) {
            color = thisColor;
            // ??? why + 1?
            workArea.clear(color + 1);
        }

        if (minX > thisStart) {
            minX = thisStart;
        }
        if (maxX < thisEnd) {
            maxX = thisEnd;
        }
        if (minY > thisRow) {
            minY = thisRow;
        }
        if (maxY < thisRow) {
            maxY = thisRow;
        }

        workArea.put(thisStart, thisRow, thisColor);

        for (int x = thisStart + 1; x < thisEnd; x++) {
            if ((thisRow == 0 || thisRow == source.height() - 1) || (source.get(x, thisRow - 1) != thisColor) ||
                (source.get(x, thisRow + 1) != thisColor)) {
                workArea.put(x, thisRow, thisColor);
            }
        }

        workArea.put(thisEnd, thisRow, thisColor);
    }

    while (true) {
        bool found = false;

        for (int searchY = minY; !found && searchY <= maxY; searchY++) {
            for (int searchX = minX; !found && searchX <= maxX; searchX++) {
                if (workArea.get(searchX, searchY) == color) {
                    startX = searchX;
                    startY = searchY;
                    found = true;
                }
            }
        }

        if (!found) {
            break;
        }

        int x = startX;
        int y = startY;
        int xDelta = 0;
        int yDelta = 1;
        int count = 0;

        const auto safeIsColor = [&workArea, &color](int x, int y) -> bool {
            if (x < 0 || y < 0 || x >= workArea.width() || y >= workArea.height()) {
                return false;
            }
            return workArea.get(x, y) == color;
        };

        const auto checkDirections = [&x, &y, &xDelta, &yDelta, &safeIsColor]() -> bool {
            constexpr std::pair<int, int> directions[] = {
                { -1, 1 }, { 0, 1 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 0, -1 }, { -1, -1 }, { -1, 0 }
            };

            for (const auto& d : directions) {
                int dx = d.first;
                int dy = d.second;
                if (safeIsColor(x + dx, y + dy)) {
                    x += dx;
                    y += dy;
                    xDelta = dx;
                    yDelta = dy;
                    return true;
                }
            }
            return false;
        };

        while (true) {
            count++;

            _lines.push_back(Point(x, y, color));
            workArea.put(x, y, color + 1);
            // ???
            if (count == 3) {
                workArea.put(startX, startY, color);
            }
            if (safeIsColor(x + xDelta, y + yDelta)) {
                x += xDelta;
                y += yDelta;
            } else if (!checkDirections()) {
                if (x != startX || y != startY) {
                    _lines.push_back(Point(-1, -1));
                    bool found = false;
                    for (int searchY = minY; !found && searchY < maxY; searchY++) {
                        for (int searchX = minX; !found && searchX < maxX; searchX++) {
                            if (workArea.get(searchX, searchY) == color) {
                                if (searchX == startX && searchY == startY) {
                                    continue;
                                }
                                x = searchX;
                                y = searchY;
                                count = 0;
                                found = true;
                            }
                        }
                    }
                    if (!found) {
                        break;
                    }
                }
            }

            if (x == startX && y == startY) {
                workArea.put(startX, startY, color + 1);
                _lines.push_back(Point(startX, startY, color));
                _lines.push_back(Point(-1, -1));
                _closed = true;
                break;
            }
        }
    }
}

// ??? Fix - does not handle empty points
void PixelArea::optimizeLines() {
    std::vector<Point> optimized;

    optimized.push_back(_lines.front());
    auto candidate = _lines[1];

    for (const auto& nextPoint : std::span(_lines).subspan(2)) {
        const auto& p0 = optimized.back();

        if (nextPoint.empty()) {
            optimized.push_back(candidate);
            optimized.push_back(nextPoint);
            continue;
        }

        if (candidate == p0) {
            candidate = nextPoint;
            continue;
        }

        auto xDiff = candidate.x - p0.x;
        auto yDiff = candidate.y - p0.y;

        if (xDiff == 0 && nextPoint.x == p0.x) {
            candidate = nextPoint;
        } else if (yDiff == 0 && nextPoint.y == p0.y) {
            candidate = nextPoint;
        } else if (abs(xDiff) == 1 && abs(yDiff) == 1 && (nextPoint.x - candidate.x == xDiff) &&
                   (nextPoint.y - candidate.y == yDiff)) {
            candidate = nextPoint;
        } else {
            optimized.push_back(candidate);
            candidate = nextPoint;
        }
    }

    optimized.push_back(candidate);
    std::swap(optimized, _lines);
}

void PixelArea::findFills(ByteImage& workArea, uint8_t bg) {
    if (!_closed) {
        return;
    }

    const auto c = color();

    if (c == bg) {
        return;
    }

    workArea.clear(bg);
    bool fillOK = true;

    // First, try to fill without drawing lines
    for (const auto& run : _runs) {
        const int row = run.row;
        for (int col = run.start; col < run.start + run.length; col++) {
            if (workArea.get(col, row) == bg) {
                fillOK = workArea.fillWhere(col, row, c, bg, [this](int x, int y) {
                    return contains(x, y);
                });
                if (!fillOK) {
                    break;
                }
                _fills.push_back({ col, row });
            }
        }
        if (!fillOK) {
            break;
        }
    }

    if (fillOK) {
        _lines.clear();
        return;
    }

    workArea.clear(bg);
    _fills.clear();

    Point p0;
    Point p1;

    for (auto p = _lines.begin(); p != _lines.end(); p++) {
        if (p0.empty()) {
            p0 = *p;
            workArea.put(p0.x, p0.y, c);
            p++;
            if (p == _lines.end()) {
                break;
            }
        }
        p1 = *p;

        if (p1.empty()) {
            p0 = Point();
            continue;
        }

        workArea.line(p0.x, p0.y, p1.x, p1.y, c);
        p0 = p1;
    }

    // ???
    if (!p0.empty() && !p1.empty()) {
        workArea.line(p0.x, p0.y, p1.x, p1.y, c);
    }

    for (const auto& run : _runs) {
        int row = run.row;
        for (int col = run.start; col < run.start + run.length; col++) {
            if (workArea.get(col, row) == bg) {
                _fills.push_back({ col, row });
                workArea.fillWhere(col, row, c, bg, [](int, int) {
                    return true;
                });
            }
        }
    }
}

int SCIPicVectorizer::pickColor(int x, int y, int leftColor, std::span<const uint8_t> previousRow) const {
    const auto colorAt = [this](int x, int y, int dx, int dy) {
        assert(abs(dx) == 1 || abs(dy) == 1);
        assert(x + dx >= 0);
        assert(y + dy >= 0);

        auto first = _source.get(x, y);
        auto second = _source.get(x + dx, y + dy);

        if ((x + y) % 2 == 0) {
            std::swap(first, second);
        }

        const auto c = PaletteColor(first, second);
        const auto colorIndex = _colors.index(c);
        if (colorIndex != -1) {
            return colorIndex;
        }
        return _colors.match(x, y, _source.get(x, y));
    };

    if (leftColor != -1 && effectiveColor(_colors[leftColor], x, y) == _source.get(x, y)) {
        return leftColor;
    }

    if (!previousRow.empty()) {
        auto upperColor = previousRow[x];
        if (effectiveColor(_colors[upperColor], x, y) == _source.get(x, y)) {
            return upperColor;
        }
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

void SCIPicVectorizer::createPaletteImage() {
    int previousColor = -1;
    std::span<const uint8_t> previousRow({});

    for (int y = 0; y < _source.height(); y++) {
        for (int x = 0; x < _source.width(); x++) {
            const auto c = pickColor(x, y, previousColor, previousRow);
            _paletteImage.put(x, y, c);
            previousColor = c;
        }
        previousRow = _paletteImage.row(y);
    }
}

void SCIPicVectorizer::scanRow(int y, std::vector<PixelAreaID>& columnAreas) {
    int startColumn = 0;
    auto currentColor = _paletteImage.get(startColumn, y);

    PixelArea startArea(y, startColumn, currentColor);
    PixelAreaID currentArea = startArea.id();

    if (y > 0 && currentColor == _paletteImage.get(startColumn, y - 1)) {
        const auto matchingID = columnAreas[startColumn];
        auto& matchingArea = _areas[matchingID];
        assert(!matchingArea.empty());
        matchingArea.merge(startArea);
        currentArea = matchingArea.id();
    } else {
        _areas.insert({ currentArea, startArea });
        columnAreas[startColumn] = startArea.id();
    }

    for (int x = 1; x < _paletteImage.width(); x++) {
        auto color = _paletteImage.get(x, y);
        if (color == currentColor) {
            if (y > 0 && color == _paletteImage.get(x, y - 1)) {
                const auto matchingID = columnAreas[x];
                auto& matchingArea = _areas[matchingID];
                assert(!matchingArea.empty());
                if (matchingArea.id() != currentArea) {
                    matchingArea.merge(_areas[currentArea]);
                    for (auto& ca : columnAreas) {
                        if (ca == currentArea) {
                            ca = matchingArea.id();
                        }
                    }
                    _areas.erase(currentArea);
                    currentArea = matchingArea.id();
                }
            }
            columnAreas[x] = currentArea;
            continue;
        }
        _areas[currentArea].extendLastRunTo(x - 1);

        currentColor = color;
        startColumn = x;

        PixelArea newArea(y, startColumn, currentColor);

        if (y > 0 && color == _paletteImage.get(x, y - 1)) {
            const auto matchingID = columnAreas[x];
            auto& matchingArea = _areas[matchingID];
            assert(!matchingArea.empty());
            matchingArea.merge(newArea);
            currentArea = matchingArea.id();
        } else {
            currentArea = newArea.id();
            _areas.insert({ currentArea, newArea });
            columnAreas[startColumn] = currentArea;
        }
    }
    _areas[currentArea].extendLastRunTo(_source.width() - 1);
}

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

SCICommand encodeFill(int x, int y) {
    return SCICommand{ .code = SCICommandCode::floodFill, .params = encodeCoordinate(x, y) };
}

void encodeAreaLines(const PixelArea& area, std::vector<SCICommand>& sink) {
    sink.push_back(encodeVisual(area.color()));

    std::vector<Point> coords;
    for (const auto& point : area.lines()) {
        if (point.empty()) {
            sink.push_back(encodeMultiLine(coords));
            coords.clear();
        } else {
            coords.push_back(point);
        }
    }
    if (!coords.empty()) {
        assert(coords.size() > 1);
        sink.push_back(encodeMultiLine(coords));
    }
}

void encodeAreaFills(const PixelArea& area, std::vector<SCICommand>& sink) {
    sink.push_back(encodeVisual(area.color()));
    for (const auto& fill : area.fills()) {
        sink.push_back(encodeFill(fill.x, fill.y));
    }
}

void encodeAreas(const std::map<PixelAreaID, PixelArea>& areas, std::vector<SCICommand>& sink) {
    for (const auto& area : areas) {
        encodeAreaLines(areas.at(area.first), sink);
    }

    for (const auto& area : areas) {
        encodeAreaFills(areas.at(area.first), sink);
    }
}

std::vector<SCICommand> SCIPicVectorizer::scan() {
    _areas.clear();
    createPaletteImage();

    std::vector<PixelAreaID> rowMemory(_source.width(), { -1, -1 });

    for (int y = 0; y < _source.height(); y++) {
        printf("Processing row %d\n", y);
        scanRow(y, rowMemory);
    }

    ByteImage workArea(_source.width(), _source.height());

    int totalLines = 0;
    int totalFills = 0;
    int linesRemoved = 0;

    for (auto& kv : _areas) {
        auto& area = _areas.at(kv.first);
        area.traceLines(_paletteImage);
        linesRemoved += area.lines().size();
        totalLines += area.lines().size();
        // area.optimizeLines();
        linesRemoved -= area.lines().size();
        area.findFills(workArea, 0x0f);
        totalFills += area.fills().size();
    }

    // std::vector sortedAreas(_areas.begin(), _areas.end());
    // std::sort(sortedAreas.begin(), sortedAreas.end(), [](const auto& a, const auto& b) {
    //     return a.color() <= b.color();
    // });

    printf("Scanner found %zu areas, %d lines and %d fills\n", _areas.size(), totalLines, totalFills);
    printf("%d lines removed\n", linesRemoved);

    // int totalPixels = 0;

    // for (const auto& area : _areas) {
    //     const auto rows = area->runs();
    //     for (const auto& row : rows) {
    //         totalPixels += row->length;
    //     }
    // }
    // assert(totalPixels == _source.height() * _source.width());

    std::vector<SCICommand> commands;

    std::vector<uint8_t> params{ SCIExtendedCommandCode::setPaletteEntries };
    for (int i = 0; const auto& color : _colors.colors()) {
        params.push_back(i++);
        auto colorValue = color.first << 4 | color.second;
        params.push_back(colorValue);
    }
    commands.push_back(SCICommand{ .code = SCICommandCode::extendedCommand, .params = params });

    encodeAreas(_areas, commands);
    printf("Produced %zu commands\n", commands.size());

    int totalBytes = 0;
    for (const auto& command : commands) {
        totalBytes += command.params.size() + 1;
    }
    printf("Size: %d bytes\n", totalBytes);

    return commands;
}

const PixelArea* SCIPicVectorizer::areaAt(int x, int y) const {
    for (const auto& kv : _areas) {
        const auto& area = _areas.at(kv.first);
        if (area.contains(x, y)) {
            return &area;
        }
    }
    return nullptr;
}
