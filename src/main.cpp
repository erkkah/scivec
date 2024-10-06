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

void show(Tigr* orig, Tigr* sci, std::function<void(int x, int y, bool tapped, Tigr* screen)> inspect) {
    auto* screen = tigrWindow(orig->w, orig->h + 10, "SCI Picture", 0);

    Tigr* pics[] = { orig, sci };
    const char* screens[] = { "Original", "Converted" };
    int picIndex = 1;

    int lastButtons = 0;
    while (!tigrClosed(screen)) {
        if (tigrKeyDown(screen, TK_ESCAPE)) {
            break;
        }

        tigrClear(screen, { 0, 0, 0, 255 });
        tigrBlit(screen, pics[picIndex], 0, 0, 0, 0, orig->w, orig->h);

        if (tigrKeyDown(screen, TK_SPACE)) {
            picIndex = 1 - picIndex;
        }
        tigrPrint(screen, tfont, 1, screen->h - 9, { 255, 255, 255, 255 }, "%s\n", screens[picIndex]);

        int mx, my, buttons;
        tigrMouse(screen, &mx, &my, &buttons);
        bool tapped = buttons != 0 && lastButtons == 0;
        lastButtons = buttons;
        inspect(mx, my, tapped, screen);

        tigrUpdate(screen);
    }
    tigrFree(screen);
}

int main(int argc, const char** argv) {
    // {
    //     const auto sciData = loadFile(argv[1]);

    //     const auto sciPic = ImageFile(argv[2]);
    //     const auto orig = sciPic.asBitmap();

    //     SCIPicParser parser(sciData);
    //     parser.parse();
    //     const auto bmp = parser.bitmap();
    //     show(bmp.get(), orig.get(), [](int x, int y, bool tapped, Tigr* scr) {
    //     });
    //     exit(0);
    // }

    const ImageFile img(argv[1]);
    auto imageBmp = img.asBitmap();

    Tigr* bmp = tigrBitmap(320, 200);
    tigrClear(bmp, { 0, 0, 0, 0 });
    tigrBlit(bmp, imageBmp.get(), 0, 0, 0, 0, std::min(bmp->w, imageBmp->w), std::min(bmp->h, imageBmp->h));

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
    const auto& palette = parser.palette();

    float counter = 0;
    int limit = 1;

    auto orig = ei.asBitmap();
    auto converted = parser.bitmap();

    show(orig.get(),
        converted.get(),
        [&vec, &counter, &palette, &limit, &parser, &converted](int x, int y, bool tapped, Tigr* scr) {
            const auto preLimit = limit;
            if (tigrKeyDown(scr, TK_UP)) {
                limit++;
            }
            if (tigrKeyDown(scr, TK_DOWN) && limit > 1) {
                limit--;
            }
            if (tigrKeyHeld(scr, TK_RIGHT)) {
                limit++;
            }
            if (tigrKeyHeld(scr, TK_LEFT) && limit > 1) {
                limit--;
            }
            if (preLimit != limit) {
                parser.parse(limit);
                const auto newPic = parser.bitmap();
                tigrBlit(converted.get(), newPic.get(), 0, 0, 0, 0, newPic->w, newPic->h);
            }

            counter += tigrTime() * 3;
            int shade = 50 * (static_cast<int>(counter) % 2);
            const auto* area = vec.areaAt(x, y);
            const auto marker = tigrRGBA(200, 100 + shade, 100 + shade, 180);
            if (area != nullptr) {
                for (const auto& run : area->runs()) {
                    const auto y = run.row;
                    tigrLine(scr, run.start, y, run.start + run.length, y, marker);
                }
                if (tapped) {
                    printf("\n*** Area %d:%d (%d)->", area->id().first, area->id().second, area->color());
                    const auto& color = palette.get(area->color());
                    const auto& first = EGAImage::palette[color.first];
                    const auto& second = EGAImage::palette[color.second];
                    printf("{%d:%d}->[%d, %d, %d]/[%d, %d, %d]\n",
                        color.first,
                        color.second,
                        first.r,
                        first.g,
                        first.b,
                        second.r,
                        second.g,
                        second.b);
                    printf("Lines:\n");
                    for (const auto& line : area->lines()) {
                        for (const auto& point : line.points()) {
                            printf("(%d,%d)", point.x, point.y);
                        }
                        printf("\n");
                    }
                    printf("\nFills:\n");
                    for (const auto& line : area->fills()) {
                        printf("(%d,%d)", line.x, line.y);
                    }
                    printf("\n");
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