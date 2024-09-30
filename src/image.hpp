#pragma once

#include <memory>
#include <vector>
#include <span>
#include <cassert>
#include "stb_image.h"
#include "tigr.h"

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

    int width() const {
        return _width;
    }

    int height() const {
        return _height;
    }

    uint8_t get(int x, int y) const {
        return _bitmap[y * _width + x];
    }

    std::span<const uint8_t> row(int y) const;

    std::unique_ptr<Tigr, decltype(&tigrFree)> asBitmap() const;

   private:
    int _width{ 0 };
    int _height{ 0 };
    std::vector<uint8_t> _bitmap;
};

struct PaletteImage {
    PaletteImage(int width, int height) : _width{ width }, _height{ height }, _bitmap(width * height){};

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

   private:
    int _width;
    int _height;
    std::vector<uint8_t> _bitmap;
};
