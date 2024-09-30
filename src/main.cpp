#include <string_view>
#include <fstream>
#include <vector>

#include "scipicparser.hpp"
#include "scipicvectorizer.hpp"
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

void show(Tigr* orig, Tigr* sci, std::function<void(int x, int y, Tigr* screen)> inspect) {
    auto* screen = tigrWindow(orig->w, orig->h, "SCI Picture", 0);

    Tigr* pics[] = { orig, sci };
    int picIndex = 1;

    while (!tigrClosed(screen)) {
        if (tigrKeyDown(screen, TK_ESCAPE)) {
            break;
        }
        if (tigrKeyDown(screen, TK_SPACE)) {
            picIndex = 1 - picIndex;
        }

        tigrBlit(screen, pics[picIndex], 0, 0, 0, 0, orig->w, orig->h);

        int mx, my, buttons;
        tigrMouse(screen, &mx, &my, &buttons);
        if (buttons != 0) {
            inspect(mx, my, screen);
        }

        tigrUpdate(screen);
    }
    tigrFree(screen);
}

int main(int argc, const char** argv) {
    const ImageFile img(argv[1]);
    auto bmp = img.asBitmap();

    const EGAImage ei(*bmp);

    auto vec = SCIPicVectorizer(ei);
    auto commands = vec.scan();

    std::vector<uint8_t> sciData{ 0x81, 0x00 };

    for (const auto& command : commands) {
        sciData.push_back(command.code);
        sciData.insert(sciData.end(), command.params.begin(), command.params.end());
    }

    sciData.push_back(SCICommandCode::pictureEnd);

    SCIPicParser parser(sciData);
    parser.parse();

    float counter = 0;

    show(bmp.get(), parser.bitmap(), [&vec, &counter](int x, int y, Tigr* scr) {
        // printf("(%d:%d)\n", x, y);
        counter += tigrTime() * 3;
        int shade = 50 * (static_cast<int>(counter) % 2);
        const auto* area = vec.areaAt(x, y);
        const auto marker = tigrRGBA(200, 100 + shade, 100 + shade, 180);
        if (area != nullptr) {
            for (int dy = 0; const auto& run : area->runs()) {
                const auto y = area->top() + dy;
                tigrLine(scr, run->start, y, run->start + run->length, y, marker);
                dy++;
            }
        }
    });

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