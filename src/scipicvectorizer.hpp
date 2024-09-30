#pragma once
#include <span>
#include <memory>
#include <vector>
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

    const int row;
    const int start;
    int length;
    const uint8_t color;
};

struct PixelArea {
    // PixelArea(int top, const PixelRun& run) : _top(top) {
    //     _rows.push_back(run);
    // }

    PixelArea() {
    }

    // int height() const {
    //     return _runs.size();
    // }

    // bool matches(const PixelRun& run) {
    //     if (_rows.empty()) {
    //         return false;
    //     }
    //     return _rows.back().overlaps(run) || run.overlaps(_rows.back());
    // }

    void add(std::shared_ptr<PixelRun> run) {
        assert(_runs.size() <= 1000);
        assert(std::find(_runs.begin(), _runs.end(), run) == _runs.end());
        _runs.push_back(run);
    }

    void addAll(const PixelArea& other) {
        for (const auto& run : other._runs) {
            assert(_runs.size() <= 1000);
            assert(std::find(_runs.begin(), _runs.end(), run) == _runs.end());
            _runs.push_back(run);
        }
    }

    const uint8_t color() const {
        assert(!_runs.empty());
        return _runs.front()->color;
    }

    void convert(const PaletteImage& source);
    void optimize();

    const std::vector<std::shared_ptr<PixelRun>>& runs() const {
        return _runs;
    }

    int top() const {
        return _top;
    }

    std::span<const Point> lines() const {
        return _lines;
    }

   private:
    const int _top{ 0 };
    std::vector<std::shared_ptr<PixelRun>> _runs;
    std::vector<Point> _lines;
    std::vector<Point> _fills;
    bool _filled{ false };
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
    std::shared_ptr<PixelArea> joinAreas(std::shared_ptr<PixelArea> a, std::shared_ptr<PixelArea> b);
    int pickColor(int x, int y, int previousColor, std::span<const uint8_t> previousRow) const;
    void createPaletteImage();
    void scanRow(int y, std::vector<std::shared_ptr<PixelArea>>& columnAreas);

    const EGAImage& _source;
    const Palette _colors;
    PaletteImage _paletteImage;

    std::set<std::shared_ptr<PixelArea>> _areas;
    // std::map<std::pair<int, int>, std::shared_ptr<PixelArea>> _pixelAreas;
};
