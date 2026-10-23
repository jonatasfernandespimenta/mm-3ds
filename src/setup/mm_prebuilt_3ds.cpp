// Majora's Mask: os .o2r são gerados no PC (2S2H desktop) e copiados para
// sd:/3ds/2ship. A versão de cada arquivo o próprio InitOTR do 2S2H já valida;
// aqui só garantimos que existem antes de subir a LUS.
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
    if (!Exists("mm.o2r")) error += "faltando mm.o2r\n";
    if (!Exists("2ship.o2r")) error += "faltando 2ship.o2r\n";
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
                    "Copie mm.o2r e 2ship.o2r para\n/3ds/2ship no cartao SD.\n\n"
                    "Gere os arquivos com o 2S2H de PC.\n\n%s\nA: tentar de novo   START: sair\n",
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
