#include <string_view>
#include <fstream>
#include <vector>

#include "scipicparser.hpp"
#include "scipicvectorizer.hpp"
#include "image.hpp"
#include "palette.hpp"

std::vector<uint8_t> loadFile(std::string_view fileName) {
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

void saveFile(std::string_view fileName, std::span<const uint8_t> data) {
    std::ofstream ofs(fileName, std::ios::binary);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open output");
    }

    ofs.write(reinterpret_cast<const char*>(data.data()), data.size());
}

using NamedPic = std::pair<Tigr*, std::string>;

void show(std::initializer_list<NamedPic> pics, std::function<void(int x, int y, bool tapped, Tigr* screen)> inspect) {
    auto* screen = tigrWindow(320, 200 + 10, "SCI Picture", 0);

    int picIndex = 0;
    int lastButtons = 0;

    while (!tigrClosed(screen)) {
        if (tigrKeyDown(screen, TK_ESCAPE)) {
            break;
        }

        const auto& pic = *(pics.begin() + picIndex);

        tigrClear(screen, { 0, 0, 0, 255 });
        tigrBlit(screen, pic.first, 0, 0, 0, 0, pic.first->w, pic.first->h);

        if (tigrKeyDown(screen, TK_SPACE)) {
            picIndex = 1 - picIndex;
        }
        tigrPrint(screen, tfont, 1, screen->h - 12, { 255, 255, 255, 255 }, "%s\n", pic.second.c_str());

        int mx, my, buttons;
        tigrMouse(screen, &mx, &my, &buttons);
        bool tapped = buttons != 0 && lastButtons == 0;
        lastButtons = buttons;
        inspect(mx, my, tapped, screen);

        tigrUpdate(screen);
    }
    tigrFree(screen);
}

void help() {
    fprintf(stderr, "HELP!\n");
}

void fatal(const char* message) {
    fprintf(stderr, "Fatal: %s\n", message);
    help();
    exit(1);
}

using Params = std::span<const std::string_view>;
using Flags = std::set<std::string_view>;
using Command = void(Params params, const Flags& flags);

void cmdShow(Params params, const Flags& flags) {
    if (params.size() != 1) {
        fatal("expected sci picture file argument");
    }

    const auto sciData = loadFile(params.front());
    SCIPicParser parser(sciData);
    parser.parse();
    const auto bmp = parser.bitmap();
    show({ { bmp.get(), "SCI" } }, [](int x, int y, bool tapped, Tigr* scr) {
    });
}

void cmdConvert(Params params, const Flags& flags) {
    if (params.size() < 1) {
        fatal("expected image file argument");
    }

    if (params.size() > 2) {
        fatal("unexpected arguments");
    }

    std::string savePath;
    if (params.size() == 2) {
        savePath = params[1];
    }

    const ImageFile img(params.front());
    auto imageBmp = img.asBitmap();

    Tigr* bmp = tigrBitmap(320, 190);
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

    if (!savePath.empty()) {
        saveFile(savePath, sciData);
    } else {
        fprintf(stderr, "No destination file given, no output written\n");
    }

    if (!flags.contains("-noverify")) {
        auto orig = ei.asBitmap();
        auto converted = parser.bitmap();
        if (orig->w != converted->w || orig->h != converted->h) {
            fprintf(stderr, "Input file dimension mismatch\n");
            if (!flags.contains("-nodimcheck")) {
                exit(1);
            }
        }
        for (int y = 0; y < std::min(orig->h, converted->h); y++) {
            for (int x = 0; x < std::min(orig->w, converted->w); x++) {
                const auto& o = tigrGet(orig.get(), x, y);
                const auto& c = tigrGet(converted.get(), x, y);
                if (o.a != c.a || o.r != c.r || o.g != c.g || o.b != c.b) {
                    fprintf(stderr, "Parsed file not equal to original\n");
                    exit(1);
                }
            }
        }
        fprintf(stderr, "Conversion verifies OK");
    }

    if (flags.contains("-show")) {
        float counter = 0;
        int limit = 1;

        auto orig = ei.asBitmap();
        auto converted = parser.bitmap();

        show({ { converted.get(), "Converted" }, { orig.get(), "Original" } },
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
    }
}

int main(int argc, const char** argv) {
    const auto args = std::span<const char*>(argv, argv + argc);
    std::vector<std::string_view> params;
    Flags flags;

    for (const auto& a : args.subspan(1)) {
        std::string_view arg(a);
        if (arg.starts_with("-")) {
            flags.insert(arg);
        } else {
            params.push_back(arg);
        }
    }

    if (flags.contains("-help")) {
        help();
        exit(0);
    }

    if (params.empty()) {
        fatal("expected command");
    }

    std::map<std::string_view, Command*> commands{ { "show", cmdShow }, { "convert", cmdConvert } };

    const auto& command = params.front();
    if (!commands.contains(command)) {
        fatal("unexpected command");
    }

    (commands.at(command))(std::span(params).subspan(1), flags);

    return 0;
}
