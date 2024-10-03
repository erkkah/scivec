#pragma once
#include <span>
#include <memory>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <cassert>

#include "tigr.h"
#include "image.hpp"
#include "palette.hpp"
#include "scipic.hpp"

struct PixelRun {
    PixelRun(int row, int start, int length, uint8_t color) : row(row), start(start), length(length), color(color) {
    }

    // ??? Add feathering?
    // bool overlaps(const PixelRun& other) const {
    //     return (other.color == color) &&
    //            ((other.start >= start && other.start < start + length) ||
    //                (other.start + other.length - 1 >= start && other.start + other.length - 1 < start + length));
    // }

    void extendTo(int column) {
        assert(column >= start);
        length = column - start + 1;
    }

    // void swap(PixelRun& other) {
    //     std::swap(row, other.row);
    //     std::swap(start, other.start);
    //     std::swap(length, other.length);
    //     std::swap(color, other.color);
    // }

    int row;
    int start;
    int length;
    uint8_t color;
};

using PixelAreaID = std::pair<int, int>;

struct PixelArea {
    PixelArea(const PixelRun& run) : _top{ run.row } {
    }

    PixelArea(int row, int start, uint8_t color) : _top{ row } {
        _runs.push_back(PixelRun(row, start, 1, color));
    }

    PixelArea() {
    }

    // void add(const PixelRun& run) {
    //     _runs.push_back(run);
    // }
    bool contains(int x, int y) const {
        // Use a map?
        for (const auto& run : _runs) {
            if (run.row != y) {
                continue;
            }
            if (run.start <= x && x < run.start + run.length) {
                return true;
            }
        }
        return false;
    }

    void extendLastRunTo(int column) {
        assert(!_runs.empty());
        _runs.back().extendTo(column);
    }

    void merge(PixelArea& other) {
        assert(color() == other.color());
        _runs.splice(_runs.end(), other._runs);
    }

    uint8_t color() const {
        assert(!_runs.empty());
        return _runs.front().color;
    }

    PixelAreaID id() const {
        assert(!_runs.empty());
        const auto& last = _runs.front();
        return { last.row, last.start };
    }

    bool empty() const {
        return _runs.empty();
    }

    void traceLines(const ByteImage& source);
    void optimizeLines();
    void findFills(ByteImage& workArea, uint8_t bg);

    const std::list<PixelRun>& runs() const {
        return _runs;
    }

    int top() const {
        return _top;
    }

    std::span<const Point> lines() const {
        return _lines;
    }

    std::span<const Point> fills() const {
        return _fills;
    }

   private:
    const int _top{ 0 };
    std::list<PixelRun> _runs;
    std::vector<Point> _lines;
    std::vector<Point> _fills;
    bool _closed{ false };
};

using PixelRunList = std::vector<PixelRun>;

struct SCIPicVectorizer {
    SCIPicVectorizer(const EGAImage& bmp)
        : _source(bmp), _colors(buildPalette(bmp)), _paletteImage(bmp.width(), bmp.height()) {
        printf("Found %zu distinct palette colors\n", _colors.size());
    }

    std::vector<SCICommand> scan();
    const PixelArea* areaAt(int x, int y) const;

   private:
    // std::shared_ptr<PixelArea> joinAreas(std::shared_ptr<PixelArea> a, std::shared_ptr<PixelArea> b);
    int pickColor(int x, int y, int previousColor, std::span<const uint8_t> previousRow) const;
    void createPaletteImage();
    void scanRow(int y, std::vector<PixelAreaID>& columnAreas);

    const EGAImage& _source;
    const Palette _colors;
    ByteImage _paletteImage;

    std::map<PixelAreaID, PixelArea> _areas;
    // std::map<std::pair<int, int>, std::shared_ptr<PixelArea>> _pixelAreas;
};
