#pragma once

#include <memory>
#include <vector>
#include <span>
#include <cassert>

#include "stb_image.h"
#include "tigr.h"
#include "palette.hpp"

struct ImageFile {
    ImageFile(const char* fileName);

    int width() const {
        return _width;
    }

    int height() const {
        return _height;
    }

    TPixel get(int x, int y) const;

    std::unique_ptr<Tigr, decltype(&tigrFree)> asBitmap() const;

   private:
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> _data{ nullptr, &stbi_image_free };
    int _width{ 0 };
    int _height{ 0 };
};

struct EGAImage {
    static const std::array<const TPixel, 16> palette;

    EGAImage(Tigr& bitmap);
    EGAImage(int w, int h) : _width(w), _height(h), _bitmap(w * h) {
    }

    int width() const {
        return _width;
    }

    int height() const {
        return _height;
    }

    uint8_t get(int x, int y) const {
        const auto index = y * _width + x;
        assert(index < _bitmap.size());
        return _bitmap[index];
    }

    void put(int x, int y, uint8_t p) {
        assert(p < 16);
        const auto index = y * _width + x;
        assert(index < _bitmap.size());
        _bitmap[index] = p;
    }

    void clear(uint8_t color) {
        assert(color < 16);
        std::fill(_bitmap.begin(), _bitmap.end(), color);
    }

    std::span<const uint8_t> row(int y) const;

    std::unique_ptr<Tigr, decltype(&tigrFree)> asBitmap() const;

   private:
    int _width{ 0 };
    int _height{ 0 };
    std::vector<uint8_t> _bitmap;
};

Palette buildPalette(const EGAImage& img);

struct ByteImage {
    ByteImage(int width, int height) : _width{ width }, _height{ height }, _bitmap(width * height){};
    ByteImage(const ByteImage& other) = default;

    void swap(ByteImage& other) {
        std::swap(_width, other._width);
        std::swap(_height, other._height);
        std::swap(_bitmap, other._bitmap);
    }

    int width() const {
        return _width;
    }

    int height() const {
        return _height;
    }

    uint8_t get(int x, int y) const {
        const auto index = y * _width + x;
        assert(index < _bitmap.size());
        return _bitmap[index];
    }

    void put(int x, int y, uint8_t p) {
        const auto index = y * _width + x;
        assert(index < _bitmap.size());
        _bitmap[index] = p;
    }

    std::span<const uint8_t> row(int y) const {
        assert(y < _height);
        return std::span(_bitmap.data() + _width * y, _width);
    }

    void clear(uint8_t color) {
        std::fill(_bitmap.begin(), _bitmap.end(), color);
    }

    void copyFrom(const ByteImage& other);

    std::unique_ptr<Tigr, decltype(&tigrFree)> asBitmap(Palette& palette) const;

   private:
    int _width;
    int _height;
    std::vector<uint8_t> _bitmap;
};

struct PaletteImage : public ByteImage {
    PaletteImage(int width, int height, const Palette& palette) : ByteImage(width, height), _palette(palette) {
    }

    void put(int x, int y, uint8_t colorIndex);
    bool fillWhere(int x, int y, uint8_t colorIndex, uint8_t bgColorValue, std::function<bool(int, int)> condition);
    void line(int x0, int y0, int x1, int y1, uint8_t colorIndex);

   private:
    const Palette& _palette;
};
