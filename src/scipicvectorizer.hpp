#pragma once
#include <span>
#include <memory>
#include <vector>
#include <cassert>

#include "tigr.h"
#include "image.hpp"
#include "palette.hpp"
#include "scipic.hpp"

struct PixelRun {
    PixelRun(size_t start, size_t length, uint8_t color) : start(start), length(length), color(color) {
    }

    // ??? Add feathering?
    bool overlaps(const PixelRun& other) const {
        return (other.color == color) &&
               ((other.start >= start && other.start < start + length) ||
                   (other.start + other.length - 1 >= start && other.start + other.length - 1 < start + length));
    }

    const size_t start;
    const size_t length;
    const uint8_t color;
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
        return _rows.back().overlaps(run) || run.overlaps(_rows.back());
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
    const int _top{ 0 };
    std::vector<PixelRun> _rows;
};

using PixelRunList = std::vector<PixelRun>;

struct SCIPicVectorizer {
    SCIPicVectorizer(const EGAImage& bmp)
        : _bmp(bmp), _colors(buildPalette(bmp)), _sciImage(bmp.width(), bmp.height()) {
        printf("Found %zu distinct palette colors\n", _colors.size());
    }

    std::vector<SCICommand> scan();
    const PixelArea* areaAt(int x, int y) const;

   private:
    PixelRunList pixelRuns(int y, std::span<const uint8_t> rowData);

    const EGAImage& _bmp;
    const Palette _colors;
    SCIImage _sciImage;
    std::vector<PixelArea> _areas;
};
