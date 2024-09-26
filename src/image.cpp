#include "image.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

ImageFile::ImageFile(const char* fileName) {
    int components = 0;
    auto* image = stbi_load(fileName, &_width, &_height, &components, 4);
    if (image == nullptr) {
        throw std::runtime_error("Failed to load image file");
    }
    _data.reset(image);
}

TPixel ImageFile::get(int x, int y) const {
    const auto* p = _data.get() + (y * _width + x) * 4;
    return TPixel{ .r = p[0], .g = p[1], .b = p[2], .a = 255 };
}

std::unique_ptr<Tigr, decltype(&tigrFree)> ImageFile::asBitmap() const {
    auto bmp = std::unique_ptr<Tigr, decltype(&tigrFree)>(tigrBitmap(_width, _height), &tigrFree);
    const auto pixels = _width * _height;
    std::copy((TPixel*)_data.get(), (TPixel*)_data.get() + pixels, bmp->pix);
    return bmp;
}

const std::array<const TPixel, 16> EGAImage::palette{ //
    tigrRGB(0, 0, 0),
    tigrRGB(0, 0, 159),
    tigrRGB(0, 159, 0),
    tigrRGB(0, 159, 159),
    tigrRGB(159, 0, 0),
    tigrRGB(127, 0, 159),
    tigrRGB(159, 79, 0),
    tigrRGB(159, 159, 159),
    tigrRGB(79, 79, 79),
    tigrRGB(79, 79, 255),
    tigrRGB(0, 255, 79),
    tigrRGB(79, 255, 255),
    tigrRGB(255, 79, 79),
    tigrRGB(255, 79, 255),
    tigrRGB(255, 255, 79),
    tigrRGB(255, 255, 255)
};

int pixelDistance(const TPixel& a, const TPixel& b) {
    return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b);
}

int egaColor(const TPixel& pixel) {
    int minDistance = 100000;
    int minIndex = -1;

    for (int i = 0; auto& candidate : EGAImage::palette) {
        const auto distance = pixelDistance(pixel, candidate);
        if (distance < minDistance) {
            minDistance = distance;
            minIndex = i;
        }
        i++;
    }

    return minIndex;
}

EGAImage::EGAImage(Tigr& bmp) : _width(bmp.w), _height(bmp.h), _bitmap(bmp.w * bmp.h) {
    for (int x = 0; x < bmp.w; x++) {
        for (int y = 0; y < bmp.h; y++) {
            const auto c = egaColor(tigrGet(&bmp, x, y));
            _bitmap[y * _width + x] = c;
        }
    }
}

std::unique_ptr<Tigr, decltype(&tigrFree)> EGAImage::asBitmap() const {
    auto bmp = std::unique_ptr<Tigr, decltype(&tigrFree)>(tigrBitmap(_width, _height), &tigrFree);
    const auto pixels = _width * _height;
    for (auto i = 0; i < pixels; i++) {
        bmp->pix[i] = palette[_bitmap[i]];
    }
    return bmp;
}

std::span<const uint8_t> EGAImage::row(int y) const {
    assert(y < _height);
    return std::span(_bitmap.data() + _width * y, _width);
}
