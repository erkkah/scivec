#include "image.hpp"
#include "scipic.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <unordered_set>

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
    tigrRGB(0x00, 0x00, 0x00),
    tigrRGB(0x00, 0x00, 0xaa),
    tigrRGB(0x00, 0xaa, 0x00),
    tigrRGB(0x00, 0xaa, 0xaa),
    tigrRGB(0xaa, 0x00, 0x00),
    tigrRGB(0xaa, 0x00, 0xaa),
    tigrRGB(0xaa, 0x55, 0x00),
    tigrRGB(0xaa, 0xaa, 0xaa),
    tigrRGB(0x55, 0x55, 0x55),
    tigrRGB(0x55, 0x55, 0xff),
    tigrRGB(0x00, 0xff, 0x55),
    tigrRGB(0x55, 0xff, 0xff),
    tigrRGB(0xff, 0x55, 0x55),
    tigrRGB(0xff, 0x55, 0xff),
    tigrRGB(0xff, 0xff, 0x55),
    tigrRGB(0xff, 0xff, 0xff)
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

int missingColors(std::vector<const PaletteColor>& colors) {
    assert(colors.size() > maxColors);

    std::unordered_set<int> usedFirstColors;
    std::unordered_set<int> usedSecondColors;

    for (int i = 0; i < maxColors; i++) {
        usedFirstColors.insert(colors[i].first);
        usedSecondColors.insert(colors[i].second);
    }

    std::unordered_set<int> missingFirstColors;
    std::unordered_set<int> missingSecondColors;

    for (auto it = colors.begin() + maxColors; it != colors.end(); it++) {
        if (!usedFirstColors.contains(it->first)) {
            missingFirstColors.insert(it->first);
        }
        if (!usedSecondColors.contains(it->second)) {
            missingSecondColors.insert(it->second);
        }
    }

    return missingFirstColors.size() + missingSecondColors.size();
}

Palette buildPalette(const EGAImage& bmp) {
    std::unordered_set<PaletteColor, ColorHash> colors;
    std::unordered_map<PaletteColor, int, ColorHash> colorCount;

    for (int x = 0; x < bmp.width() - 1; x++) {
        for (int y = 0; y < bmp.height(); y++) {
            const auto a = bmp.get(x, y);
            const auto b = bmp.get(x + 1, y);

            auto color = std::make_pair(a, a);

            if (a != b && x < bmp.width() - 2) {
                const auto c = bmp.get(x + 2, y);
                // A pattern must be at least three pixels long
                if (c == a) {
                    if (((x + y) % 2) != 0) {
                        color = std::make_pair(a, b);
                    } else {
                        color = std::make_pair(b, a);
                    }
                }
            }

            colorCount[color]++;
            colors.insert(color);
        }
    }

    std::vector<std::pair<PaletteColor, int>> sortedColors;

    for (const auto& cc : colorCount) {
        sortedColors.push_back(cc);
    }

    std::sort(sortedColors.begin(),
        sortedColors.end(),
        [](const std::pair<PaletteColor, int>& a, const std::pair<PaletteColor, int>& b) {
            return a.second > b.second;
        });

    std::vector<const PaletteColor> palette;

    for (const auto& color : sortedColors) {
        palette.push_back(color.first);
    }

    if (palette.size() > maxColors) {
        int missing = missingColors(palette);
        if (missing > 0) {
            printf("Hm, the image is too colorful, %d colors will be approximated!\n", missing);
        }

        palette.resize(maxColors);
    }

    return Palette(palette);
}

bool ByteImage::fillWhere(int x, int y, uint8_t c, uint8_t bg, std::function<bool(int, int)> condition) {
    if (c == bg) {
        return true;
    }
    if (get(x, y) != bg) {
        return true;
    }
    std::vector<Point> fills;
    put(x, y, c);
    fills.push_back({ x, y });

    const auto check = [this, &fills, &c, &bg](int x, int y) {
        if (x < 0 || x >= width() || y < 0 || y >= height()) {
            return;
        }

        if (get(x, y) == bg) {
            put(x, y, c);
            fills.push_back({ x, y });
        }
    };

    while (!fills.empty()) {
        const auto& fill = fills.back();
        int x = fill.x;
        int y = fill.y;
        fills.pop_back();
        if (!condition(x, y)) {
            return false;
        }
        check(x + 1, y);
        check(x - 1, y);
        check(x, y + 1);
        check(x, y - 1);
        if (fills.size() > 32768) {
            throw std::runtime_error("Fill stack overflow");
        }
    }
    return true;
}

void ByteImage::line(int x0, int y0, int x1, int y1, uint8_t c) {
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        put(x0, y0, c);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

std::unique_ptr<Tigr, decltype(&tigrFree)> ByteImage::asBitmap(Palette& palette) const {
    auto bmp = std::unique_ptr<Tigr, decltype(&tigrFree)>(tigrBitmap(_width, _height), &tigrFree);
    for (auto y = 0; y < _height; y++) {
        for (auto x = 0; x < _width; x++) {
            const auto i = y * _width + x;
            const auto& sci = palette.get(_bitmap[i]);
            const auto& ega = effectiveColor(sci, x, y);
            bmp->pix[i] = EGAImage::palette[ega];
        }
    }
    return bmp;
}
