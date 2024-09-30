#include "scipicvectorizer.hpp"
#include <cassert>
#include <span>
#include <ranges>
#include <set>
#include <map>
#include <vector>

void PixelArea::convert(const PaletteImage& source) {
    if (_runs.empty()) {
        return;
    }

    if (_runs.size() == 1) {
        auto run = _runs.front();
        _lines.push_back(Point(run->start, run->row, run->color));
        _lines.push_back(Point(run->start + run->length - 1, run->row, run->color));
        return;
    }

    PaletteImage workArea(source.width(), source.height());

    int startX = 0;
    int startY = 0;
    int color = -1;

    int minX = _runs.front()->start;
    int maxX = minX;
    int minY = _runs.front()->row;
    int maxY = minY;

    std::sort(_runs.begin(), _runs.end(), [](const auto& a, const auto& b) {
        if (a->row == b->row) {
            return a->start <= b->start;
        }
        return a->row < b->row;
    });

    for (auto& run : _runs) {
        int thisRow = run->row;
        int thisStart = run->start;
        int thisEnd = run->start + run->length - 1;
        int thisColor = run->color;

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

        for (int searchX = minX; !found && searchX <= maxX; searchX++) {
            for (int searchY = minY; !found && searchY <= maxY; searchY++) {
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

        while (true) {
            count++;

            _lines.push_back(Point(x, y, color));
            workArea.put(x, y, color + 1);
            if (count == 3) {
                workArea.put(startX, startY, color);
            }
            if (safeIsColor(x + xDelta, y + yDelta)) {
                x += xDelta;
                y += yDelta;
            } else if (safeIsColor(x, y + 1)) {
                y++;
                xDelta = 0;
                yDelta = 1;
            } else if (safeIsColor(x + 1, y)) {
                x++;
                xDelta = 1;
                yDelta = 0;
            } else if (safeIsColor(x, y - 1)) {
                y--;
                xDelta = 0;
                yDelta = -1;
            } else if (safeIsColor(x - 1, y)) {
                x--;
                xDelta = -1;
                yDelta = 0;
            } else if (safeIsColor(x + 1, y + 1)) {
                x++;
                y++;
                xDelta = 1;
                yDelta = 1;
            } else if (safeIsColor(x + 1, y - 1)) {
                x++;
                y--;
                xDelta = 1;
                yDelta = -1;
            } else if (safeIsColor(x - 1, y + 1)) {
                x--;
                y++;
                xDelta = -1;
                yDelta = 1;
            } else if (safeIsColor(x - 1, y - 1)) {
                x--;
                y--;
                xDelta = -1;
                yDelta = -1;
            } else {
                if (x != startX || y != startY) {
                    _lines.push_back(Point(-1, -1));
                    bool found = 0;
                    for (int searchX = minX; !found && searchX < maxX; searchX++) {
                        for (int searchY = minY; !found && searchY < maxY; searchY++) {
                            if (workArea.get(searchX, searchY) == color) {
                                if (searchX == startX && searchY == startY) {
                                    continue;
                                }
                                x = searchX;
                                y = searchY;
                                count = 0;
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
                _filled = true;
                break;
            }
        }
    }
}

void PixelArea::optimize() {
    std::vector<Point> optimized;

    optimized.push_back(_lines.front());
    auto candidate = _lines[1];

    for (const auto& nextPoint : std::span(_lines).subspan(2)) {
        const auto& p0 = optimized.back();

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

// void PixelArea::optimize() {
//     int fill = 15;

//     Point p0, p1, p2;
//     int xDiff = 0;
//     int yDiff = 0;

//     auto i = _lines.begin();
//     p0 = *i;
//     i++;
//     p1 = *i;

//     if (p0.color == fill || p1.color == fill) {
//         _lines.clear();
//         return;
//     }

//     do {
//         if (p1.x == p0.x) {
//             // Redundant vertical

//             i++;
//             if (i == _lines.end()) {
//                 break;
//             }
//             p2 = *i;

//             if (p0.y == p1.y || p2.x == p0.x) {
//                 //_lines.remove(p1);
//             } else {
//                 p0 = p1;
//             }
//             p1 = p2;
//         } else if (p1.y == p0.y) {
//             // Redundant horizontal

//             i++;
//             if (i == _lines.end()) {
//                 break;
//             }
//             p2 = *i;

//             if (p2.y == p0.y || p0.x == p1.x) {
//                 // _lines.remove(p1);
//             } else {
//                 p0 = p1;
//             }
//             p1 = p2;
//         } else if (abs(xDiff = p1.x - p0.x) == 1 && abs(yDiff = p1.y - p0.y) == 1) {
//             // Redundant slanted
//             i++;

//             if (i == _lines.end()) {
//                 break;
//             }
//             p2 = *i;

//             if (p2.x - p1.x == xDiff && p2.y - p1.y == yDiff) {
//                 // _lines.remove(p1);
//             } else {
//                 p0 = p1;
//             }
//             p1 = p2;
//         } else {
//             // Keep
//             p0 = p1;
//             i++;
//             if (i == _lines.end()) {
//                 break;
//             }
//             p1 = *i;
//         }
//     } while (i != _lines.end());
// }

std::shared_ptr<PixelArea> SCIPicVectorizer::joinAreas(std::shared_ptr<PixelArea> a, std::shared_ptr<PixelArea> b) {
    a->addAll(*b);

    // for (const auto& run : b->runs()) {
    //     for (int i = 0; i < run->length; i++) {
    //         const auto coord = std::make_pair(run->start + i, run->row);
    //         _pixelAreas[coord] = a;
    //     }
    // }
    _areas.erase(b);
    return a;
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

void SCIPicVectorizer::scanRow(int y, std::vector<std::shared_ptr<PixelArea>>& columnAreas) {
    int startColumn = 0;
    auto currentColor = _paletteImage.get(startColumn, y);

    auto run = std::make_shared<PixelRun>(y, startColumn, 1, currentColor);
    std::shared_ptr<PixelArea> area;

    if (y > 0 && currentColor == _paletteImage.get(startColumn, y - 1)) {
        // const auto matchingArea = _pixelAreas.at(std::make_pair(startColumn, y - 1));
        const auto matchingArea = columnAreas[startColumn];
        assert(matchingArea);
        matchingArea->add(run);
        area = matchingArea;
    } else {
        area = std::make_shared<PixelArea>();
        area->add(run);
        _areas.insert(area);
        columnAreas[startColumn] = area;
    }

    //_pixelAreas[std::make_pair(startColumn, y)] = area;
    //_pixelAreas[{ startColumn, y }] = area;

    for (int x = 1; x < _paletteImage.width(); x++) {
        if (y == 22 && x == 207) {
            printf("What!\n");
        }
        auto color = _paletteImage.get(x, y);
        if (color == currentColor) {
            if (y > 0 && color == _paletteImage.get(x, y - 1)) {
                // const auto matchingArea = _pixelAreas.at(std::make_pair(x, y - 1));
                const auto matchingArea = columnAreas[x];
                assert(matchingArea);
                if (matchingArea != area) {
                    area = joinAreas(matchingArea, area);
                }
            }
            //_pixelAreas[std::make_pair(x, y)] = area;
            columnAreas[x] = area;
            continue;
        }
        run->extendTo(x - 1);

        currentColor = color;
        startColumn = x;

        run = std::make_shared<PixelRun>(y, startColumn, 1, currentColor);

        if (y > 0 && color == _paletteImage.get(x, y - 1)) {
            // const auto matchingArea = _pixelAreas.at(std::make_pair(x, y - 1));
            const auto matchingArea = columnAreas[x];
            assert(matchingArea);
            matchingArea->add(run);
            area = matchingArea;
        } else {
            area = std::make_shared<PixelArea>();
            area->add(run);
            _areas.insert(area);
        }
        //_pixelAreas[std::make_pair(startColumn, y)] = area;
        columnAreas[x] = area;
    }
    run->extendTo(_source.width() - 1);
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

SCICommand encodeFill(int x, int y, uint8_t color) {
    return SCICommand{ .code = SCICommandCode::floodFill, .params = encodeCoordinate(x, y) };
}

void convertArea(const PixelArea& area, std::vector<SCICommand>& sink) {
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
}

void convertAreas(const std::vector<std::shared_ptr<PixelArea>>& areas, std::vector<SCICommand>& sink) {
    for (const auto& area : areas) {
        convertArea(*area, sink);
    }
}

std::vector<SCICommand> SCIPicVectorizer::scan() {
    _areas.clear();
    createPaletteImage();

    std::vector<std::shared_ptr<PixelArea>> rowMemory(_source.width());

    for (int y = 0; y < _source.height(); y++) {
        printf("Processing row %d\n", y);
        scanRow(y, rowMemory);
    }

    for (auto& area : _areas) {
        area->convert(_paletteImage);
        area->optimize();
    }

    std::vector sortedAreas(_areas.begin(), _areas.end());
    std::sort(sortedAreas.begin(), sortedAreas.end(), [](const auto& a, const auto& b) {
        return a->color() <= b->color();
    });

    printf("Scanner found %zu areas\n", _areas.size());

    int totalPixels = 0;

    for (const auto& area : _areas) {
        const auto rows = area->runs();
        for (const auto& row : rows) {
            totalPixels += row->length;
        }
    }
    // assert(totalPixels == _source.height() * _source.width());

    std::vector<SCICommand> commands;

    std::vector<uint8_t> params{ SCIExtendedCommandCode::setPaletteEntries };
    for (int i = 0; const auto& color : _colors.colors()) {
        params.push_back(i++);
        auto colorValue = color.first << 4 | color.second;
        params.push_back(colorValue);
    }
    commands.push_back(SCICommand{ .code = SCICommandCode::extendedCommand, .params = params });

    convertAreas(sortedAreas, commands);
    printf("Produced %zu commands\n", commands.size());

    int totalBytes = 0;
    for (const auto& command : commands) {
        totalBytes += command.params.size() + 1;
    }
    printf("Size: %d bytes\n", totalBytes);

    return commands;
}

const PixelArea* SCIPicVectorizer::areaAt(int x, int y) const {
    // for (const auto& area : _areas) {
    //     if (y < area->top() || y >= area->top() + area->height()) {
    //         continue;
    //     }
    //     for (int dy = 0; const auto& row : area.rows()) {
    //         if (y == area.top() + dy && x >= row.start && x < row.start + row.length) {
    //             return &area;
    //         }
    //         dy++;
    //     }
    // }
    return nullptr;
}
