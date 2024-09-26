#include <string_view>
#include <fstream>
#include <vector>

#include "scipic.hpp"
#include "image.hpp"
#include "palette.hpp"

std::vector<uint8_t> loadFile(const char* fileName) {
    std::ifstream ifs(fileName, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open input");
    }
    const auto pos = ifs.tellg();

    if (pos == 0) {
        return {};
    }

    std::vector<uint8_t> data(pos);

    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(data.data()), pos);

    return data;
}

void show(Tigr* bmp) {
    auto* screen = tigrWindow(320, 200, "SCI Picture", 0);

    while (!tigrClosed(screen)) {
        if (tigrKeyDown(screen, TK_ESCAPE)) {
            break;
        }
        tigrBlit(screen, bmp, 0, 0, 0, 0, bmp->w, bmp->h);
        tigrUpdate(screen);
    }
    tigrFree(screen);
}

int main(int argc, const char** argv) {
    const ImageFile img(argv[1]);
    auto bmp = img.asBitmap();

    const EGAImage ei(*bmp);

    auto vec = SCIPicVectorizer(ei);
    vec.scan();

    // auto eb = ei.asBitmap();
    // show(eb.get());

    // const auto data = loadFile(argv[1]);
    // SCIPicParser parser(data);
    // parser.parse();

    // show(parser.bitmap());
    return 0;
}

/*
Palette default_palette =
     <(0,0), (1,1), (2,2), (3,3), (4,4), (5,5), (6,6), (7,7),
      (8,8), (9,9), (a,a), (b,b), (c,c), (d,d), (e,e), (8,8),
      (8,8), (0,1), (0,2), (0,3), (0,4), (0,5), (0,6), (8,8),
      (8,8), (f,9), (f,a), (f,b), (f,c), (f,d), (f,e), (f,f),
      (0,8), (9,1), (2,a), (3,b), (4,c), (5,d), (6,e), (8,8)>;

*/