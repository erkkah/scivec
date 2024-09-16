#include "scipic.hpp"
#include <string_view>
#include <fstream>
#include <vector>

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
    const auto data = loadFile(argv[1]);
    SCIPicParser parser(data);
    parser.parse();

    show(parser.bitmap());
    return 0;
}
