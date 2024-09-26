#include "scipic.hpp"
#include <cassert>
#include <span>
#include <ranges>
#include <set>

struct PixelRun {
    PixelRun(size_t start, size_t length, uint8_t color) : start(start), length(length), color(color) {
    }

    bool overlaps(const PixelRun& other) const {
        return (other.start >= start && other.start < start + length) ||
               (other.start + length >= start && other.start + length <= start + length);
    }

    size_t start;
    size_t length;
    uint8_t color;
};

struct PixelArea {
    PixelArea(int top, const PixelRun& run) : _top(top) {
        _rows.push_back(run);
    }

    PixelArea() {
    }

    int height() const {
        return _rows.size();
    }

    bool matches(const PixelRun& run) {
        if (_rows.empty()) {
            return false;
        }
        return _rows.back().overlaps(run);
    }

    void add(const PixelRun& run) {
        assert(matches(run));
        _rows.push_back(run);
    }

    std::span<const PixelRun> rows() const {
        return _rows;
    }

    int top() const {
        return _top;
    }

   private:
    int _top;
    std::vector<PixelRun> _rows;
};

using PixelRunList = std::vector<PixelRun>;

PixelRunList pixelRuns(int y, std::span<const uint8_t> rowData, const Palette& palette) {
    assert(rowData.size() > 1);

    PixelRunList result;

    int previousColor = -1;
    size_t runStart = 0;

    for (int x = 0; x < rowData.size() - 1; x++) {
        auto first = rowData[x];
        auto second = rowData[x + 1];
        // return ((x + y) % 2) ? col.first : col.second;
        if ((x + y) % 2 == 0) {
            std::swap(first, second);
        }
        PaletteColor c(first, second);

        auto currentColor = palette.index(c);

        if (currentColor != -1) {
            // A full two-pixel match, so skip a pixel!
            x++;
        } else {
            currentColor = palette.match(x, y, first);
        }

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
        const auto lastColor = palette.match(lastX, y, lastPixel);

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

    auto matchingArea = [&previousRowAreas, &allAreas](const PixelRun& run) -> size_t {
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

        for (int y = area.top(); const auto& row : rows) {
            coords.push_back(Point{ .x = static_cast<int>(row.start + row.length - 1), .y = y });
            y++;
        }

        coords.push_back(Point{ .x = static_cast<int>(bottomLine.start), .y = area.top() + area.height() - 1 });

        for (int y = area.height() - 1; const auto& row : std::ranges::reverse_view{ rows }) {
            coords.push_back(Point{ .x = static_cast<int>(row.start), .y = y });
            y--;
        }

        sink.push_back(encodeMultiLine(coords));

        // ??? Add fills here!
    }
}

void convertAreas(const std::vector<PixelArea>& areas, std::vector<SCICommand>& sink) {
    for (const auto& area : areas) {
        convertArea(area, sink);
    }
}

void SCIPicVectorizer::scan() {
    std::vector<PixelRunList> scannedRows;

    for (int y = 0; y < _bmp.height(); y++) {
        auto row = _bmp.row(y);
        const auto runs = pixelRuns(y, row, _colors);
        scannedRows.push_back(runs);
    }

    auto areas = buildAreas(scannedRows);
    printf("Scanner found %zu areas\n", areas.size());

    int totalPixels = 0;

    for (const auto& area : areas) {
        const auto rows = area.rows();
        for (const auto& row : rows) {
            totalPixels += row.length;
        }
    }
    assert(totalPixels == _bmp.height() * _bmp.width());

    std::vector<SCICommand> commands;
    convertAreas(areas, commands);
    printf("Produced %zu commands\n", commands.size());

    int totalBytes = 0;
    for (const auto& command : commands) {
        totalBytes += command.params.size() + 1;
    }
    printf("Size: %d bytes\n", totalBytes);
}
