// Majora's Mask: mm.o2r is generated on PC (2S2H desktop) and copied to
// sd:/3ds/2ship. 2ship.o2r ships in the app's romfs (a copy on the SD card
// overrides it). 2S2H's own InitOTR already validates each file's version;
// here we only make sure they exist before bringing up LUS.
#include <3ds.h>
#include <cstdio>
#include <string>
#include <sys/iosupport.h>
#include <ship/utils/logging_3ds.h>

namespace {
bool Exists(const char* path) {
    FILE* f = std::fopen(path, "rb");
    if (f != nullptr) std::fclose(f);
    return f != nullptr;
}

bool ValidateInstalled(std::string& error) {
    error.clear();
    if (!Exists("mm.o2r")) error += "missing mm.o2r\n";
    if (!Exists("2ship.o2r") && !Exists("romfs:/2ship.o2r")) error += "missing 2ship.o2r\n";
    return error.empty();
}
} // namespace

extern "C" bool Soh3dsGameArchiveReady(bool masterQuest) {
    return !masterQuest && Exists("mm.o2r");
}

extern "C" void Soh3dsRestoreConsoleOutput() {}

extern "C" bool Soh3dsEnsureGameData() {
    std::string error;
    if (ValidateInstalled(error)) return true;
    gfxInitDefault();
    consoleInit(GFX_TOP, nullptr);
    bool success = false;
    while (aptMainLoop()) {
        consoleClear();
        std::printf("2 Ship 2 Harkinian 3DS\n\n"
                    "Copy mm.o2r to /3ds/2ship\non the SD card.\n\n"
                    "Generate it with PC 2S2H 5.0.1\nand your own ROM.\n\n%s\nA: try again   START: exit\n",
                    error.c_str());
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
        hidScanInput();
        const u32 pressed = hidKeysDown();
        if (pressed & KEY_START) break;
        if ((pressed & KEY_A) && ValidateInstalled(error)) {
            success = true;
            break;
        }
    }
    Soh3dsConfigureDebugOutput();
    devoptab_list[STD_OUT] = devoptab_list[STD_ERR];
    setvbuf(stdout, nullptr, _IONBF, 0);
    gfxExit();
    return success;
}
