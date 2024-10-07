#include "scipicvectorizer.hpp"
#include "scipicencoder.hpp"
#include <cassert>
#include <span>
#include <ranges>
#include <set>
#include <map>
#include <vector>

void PixelArea::fillWithLines() {
    // ??? Simple, stupid line fill
    for (const auto& run : _runs) {
        Line l;
        l.add(Point(run.start, run.row));
        l.add(Point(run.start + run.length - 1, run.row));
        _lines.push_back(l);
    }
}

void PixelArea::traceLines(const ByteImage& source) {
    if (_runs.empty()) {
        return;
    }

    Line line;

    if (_runs.size() == 1) {
        auto run = _runs.front();
        line.add(Point(run.start, run.row, run.color));
        line.add(Point(run.start + run.length - 1, run.row, run.color));
        _lines.push_back(line);
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
            // "+1" makes sure we use a unique new color
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

        bool endOfTheLine = false;

        while (!endOfTheLine) {
            count++;

            line.add(Point(x, y, color));
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
                    _lines.push_back(line);
                    line.clear();
                }
                endOfTheLine = true;
            }

            workArea.put(startX, startY, color + 1);
            if (x == startX && y == startY) {
                line.add(Point(startX, startY, color));
                _lines.push_back(line);
                line.clear();
                _closed = true;
            }
        }
    }

    for (int y = minY; y < maxY; y++) {
        for (int x = minX; x < maxX; x++) {
            if (x == startX && y == startY) {
                continue;
            }
            assert(workArea.get(x, y) != color);
        }
    }
}

void PixelArea::optimizeLines() {
    for (auto& line : _lines) {
        line.optimize();
    }
}

void Line::optimize() {
    if (_points.size() < 3) {
        return;
    }
    std::vector<Point> optimized;

    optimized.push_back(_points.front());
    auto candidate = _points[1];

    for (const auto& nextPoint : std::span(_points).subspan(2)) {
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
    std::swap(optimized, _points);
}

void PixelArea::findFills(PaletteImage& canvas, uint8_t bg) {
    // Remember - our canvas pixel values are indices into the SCI palette.
    // Flood fills are based on areas of same effective color.

    const auto c = color();

    PaletteImage workArea(canvas);

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
        canvas.swap(workArea);
        return;
    }

    _fills.clear();

    for (const auto& line : _lines) {
        const auto points = line.points();

        Point p0 = points.front();
        canvas.put(p0.x, p0.y, c);

        for (const auto& p : points.subspan(1)) {
            canvas.line(p0.x, p0.y, p.x, p.y, c);
            p0 = p;
        }
    }

    workArea.copyFrom(canvas);

    for (const auto& run : _runs) {
        int row = run.row;
        for (int col = run.start; col < run.start + run.length; col++) {
            if (workArea.get(col, row) == bg) {
                fillOK = workArea.fillWhere(col, row, c, bg, [this](int x, int y) {
                    return contains(x, y);
                });
                if (fillOK) {
                    _fills.push_back({ col, row });
                } else {
                    _fills.clear();
                    return;
                }
            }
        }
    }

    workArea.swap(canvas);
}

int SCIPicVectorizer::pickColor(int x, int y, int leftColor, std::span<const uint8_t> previousRow) const {
    const auto colorAt = [this](int x, int y, int dx, int dy) {
        assert(abs(dx) == 1 || abs(dy) == 1);
        assert(x + dx >= 0);
        assert(y + dy >= 0);
        assert(x + dx < _source.width());
        assert(y + dy < _source.height());

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

    std::vector<std::array<int, 4>> deltas{
        { 0, 0, 1, 0 },
        { 0, 0, -1, 0 },
        { 0, 0, 0, 1 },
        { 0, 0, 0, -1 },
        { 1, 0, 1, 0 },
        { -2, 0, 1, 0 },
        { 0, 1, 0, 1 },
        { 0, -2, 0, 1 },
    };

    std::map<int, int> counts;

    for (const auto& delta : deltas) {
        int xa = x + delta[0];
        int ya = y + delta[1];
        int dx = delta[2];
        int dy = delta[3];

        assert(dx == 0 || dy == 0);

        if (xa < 0 || xa >= _source.width()) {
            continue;
        }
        if (ya < 0 || ya >= _source.height()) {
            continue;
        }
        if (xa + dx < 0 || xa + dx >= _source.width()) {
            continue;
        }
        if (ya + dy < 0 || ya + dy >= _source.height()) {
            continue;
        }
        auto c = colorAt(xa, ya, dx, dy);
        if (c != -1) {
            if (xa != x || ya != y) {
                if (counts[c] != 0) {
                    counts[c]++;
                }
            } else {
                counts[c]++;
            }
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

void encodeAreaLines(const PixelArea& area, std::vector<SCICommand>& sink) {
    sink.push_back(encodeVisual(area.color()));

    for (const auto& line : area.lines()) {
        sink.push_back(encodeMultiLine(line.points()));
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
        encodeAreaFills(areas.at(area.first), sink);
    }
}

void SCIPicVectorizer::scan() {
    _areas.clear();
    createPaletteImage();

    std::vector<PixelAreaID> rowMemory(_source.width(), { -1, -1 });

    for (int y = 0; y < _source.height(); y++) {
        scanRow(y, rowMemory);
    }

    PaletteImage canvas(_source.width(), _source.height(), _colors);
    canvas.clear(0xf);

    int totalLines = 0;
    int totalFills = 0;
    int linesRemoved = 0;

    for (auto& kv : _areas) {
        auto& area = _areas.at(kv.first);
        const auto& color = _colors.get(area.color());
        if (color.first == 0xf && color.second == 0xf) {
            continue;
        }
        if (color.first == 0xf || color.second == 0xf) {
            area.fillWithLines();
        } else {
            area.traceLines(_paletteImage);
            linesRemoved += area.lines().size();
            totalLines += area.lines().size();
            area.optimizeLines();
            linesRemoved -= area.lines().size();

            area.findFills(canvas, 0xf);
            totalFills += area.fills().size();
        }
    }

    // std::vector sortedAreas(_areas.begin(), _areas.end());
    // std::sort(sortedAreas.begin(), sortedAreas.end(), [](const auto& a, const auto& b) {
    //     return a.color() <= b.color();
    // });

    printf("Scanner found %zu areas, %d lines and %d fills\n", _areas.size(), totalLines, totalFills);
    printf("%d lines removed\n", linesRemoved);
}

std::vector<SCICommand> SCIPicVectorizer::encode() const {
    std::vector<SCICommand> commands;

    std::vector<uint8_t> params{ SCIExtendedCommandCode::setPaletteEntries };
    for (int i = 0; const auto& color : _colors.colors()) {
        params.push_back(i++);
        uint8_t colorValue = (color.first << 4) | color.second;
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
