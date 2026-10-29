// MM 3DS: tela de baixo sempre mostrando as páginas do menu de pause (mesmas
// texturas, ícones, contornos e munição do kaleido), com o jogo rodando em cima.
// Páginas Itens / Máscaras / Mapa / Save, trocadas pelas setas (como Z/R).
// Arrastar (ou tocar item e depois o alvo) equipa nos 3 botões C e nos 4 slots
// de D-pad do 2S2H; tocar num slot de D-pad usa o item (injeta o botão por um
// frame, então o uso segue a regra do próprio jogo).
//
// Portado do dual screen do SoH-3DS (soh/Enhancements/dualscreen3ds): tudo é
// Fast3D 2D 320x240 em POLY_OPA_DISP entre gsSPSetFB(gBottomScreenFrameBuffer);
// o toque chega uma vez por frame do backend (Soh3dsTouchRead).
#ifdef __3DS__
#include <stdio.h>
#include <string.h>
#include "global.h"
#include "framebuffer_effects.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "BottomScreen3DS.h"
#include "2s2h/Enhancements/Saving/SavingEnhancements.h"
#include <stddef.h>
#include "interface/parameter_static/parameter_static.h"
#include "interface/icon_item_jpn_static/icon_item_jpn_static.h"
#include "archives/icon_item_static/icon_item_static_yar.h"
#include "interface/icon_item_field_static/icon_item_field_static.h"
#include "archives/icon_item_24_static/icon_item_24_static_yar.h"
#include "interface/icon_item_dungeon_static/icon_item_dungeon_static.h"

int Soh3dsTouchRead(int* x, int* y) __attribute__((weak));
extern const u8 default_font_bin[]; // fonte do console da libctru
extern u16 sMasksGivenOnMoonBits[]; // z_kaleido_mask.c
extern const char* gAmmoDigitTextures[10]; // z_parameter.c
extern const char* sCounterTextures[];       // z_parameter.c

// Mesma ordem do pause (R avança): Itens, Mapa, Quest Status, Máscaras; Save é a página extra.
enum { PAGE_ITEMS, PAGE_MAP, PAGE_QUEST, PAGE_MASKS, PAGE_SAVE, PAGE_COUNT };
// Alvos: 3 botões C + os 4 slots de D-pad do 2S2H (I/II = esquerda/direita).
enum { TGT_UP, TGT_I, TGT_C_LEFT, TGT_C_DOWN, TGT_C_RIGHT, TGT_II, TGT_DOWN, TGT_COUNT };

// Página do pause (240x160 em coordenadas do kaleido, centro na origem, y para
// cima) desenhada em escala 1,25: 300x200 a partir de (PAGE_X, 0).
#define PAGE_X 10
#define PAGE_SCALE_NUM 5
#define PAGE_SCALE_DEN 4
#define SX(px) (PAGE_X + ((px) + 120) * PAGE_SCALE_NUM / PAGE_SCALE_DEN)
#define SY(py) ((80 - (py)) * PAGE_SCALE_NUM / PAGE_SCALE_DEN)
#define CELL_PAGE 32 // célula da grade no kaleido (ITEM_GRID_CELL_WIDTH)
#define ICON 32
#define ICON_SCREEN 35 // quad de 28 px do kaleido em escala 1,25
#define BAR_Y 202
#define TGT_W 34
#define TGT_H 27
#define TGT_PITCH 36
#define TGT_X0 34
#define ARROW_W 30
#define SLOP_SQ 100
#define WORLD_MAP_IMAGE_WIDTH 216 // z_kaleido_scope.h
#define WORLD_MAP_IMAGE_HEIGHT 128

static const u8 sTargetDpad[TGT_COUNT] = { EQUIP_SLOT_D_UP, EQUIP_SLOT_D_LEFT, 0, 0, 0, EQUIP_SLOT_D_RIGHT,
                                           EQUIP_SLOT_D_DOWN };
static const u16 sTargetButton[TGT_COUNT] = { BTN_DUP, BTN_DLEFT, 0, 0, 0, BTN_DRIGHT, BTN_DDOWN };
static const char* sTargetLabel[TGT_COUNT] = { "D-pad^", "I", "C<", "Cv", "C>", "II", "D-padv" };

#define TGT_IS_C(t) ((t) >= TGT_C_LEFT && (t) <= TGT_C_RIGHT)

static struct {
    int page;
    int heldFrames;
    int down;
    int downX, downY;
    int x, y;
    int dragSlot; // slot do inventário sob o dedo no toque, ou -1
    int dragging;
    int selSlot; // slot selecionado por toque, ou -1
    u16 injectButtons; // botões a injetar no próximo GameState_GetInput
    int confirm;       // botão do save aguardando o 2º toque (slot*2 + carregar), ou -1
    int questSong;     // música selecionada na Quest Status (QUEST_SONG_*), ou -1
    int songPlaying;   // a melodia da questSong está tocando (playback da ocarina)
} sUi = { PAGE_ITEMS, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, -1, -1, 0 };

static const u8 sFormMasks[PLAYER_FORM_MAX] = {
    ITEM_MASK_FIERCE_DEITY, ITEM_MASK_GORON, ITEM_MASK_ZORA, ITEM_MASK_DEKU, ITEM_NONE,
};

// --- estado do save ---------------------------------------------------------

static int Bs_HasGrid(void) {
    return sUi.page == PAGE_ITEMS || sUi.page == PAGE_MASKS;
}

static int Bs_SlotBase(void) {
    return sUi.page == PAGE_MASKS ? ITEM_NUM_SLOTS : 0;
}

static u8 Bs_SlotItem(int slot) {
    if (slot >= ITEM_NUM_SLOTS) {
        int mask = slot - ITEM_NUM_SLOTS;
        if (gSaveContext.masksGivenOnMoon[sMasksGivenOnMoonBits[mask] >> 8] & (u8)sMasksGivenOnMoonBits[mask]) {
            return ITEM_NONE;
        }
    }
    return gSaveContext.save.saveInfo.inventory.items[slot];
}

static u8* Bs_TargetItem(int t) {
    return TGT_IS_C(t) ? &BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_LEFT + t - TGT_C_LEFT)
                       : &DPAD_BUTTON_ITEM_EQUIP(0, sTargetDpad[t]);
}

static u8* Bs_TargetSlot(int t) {
    return TGT_IS_C(t) ? &C_SLOT_EQUIP(0, EQUIP_SLOT_C_LEFT + t - TGT_C_LEFT) : &DPAD_SLOT_EQUIP(0, sTargetDpad[t]);
}

static void Bs_ReloadIcon(PlayState* play, int t) {
    if (TGT_IS_C(t)) {
        Interface_LoadItemIconImpl(play, EQUIP_SLOT_C_LEFT + t - TGT_C_LEFT);
    } else {
        Interface_Dpad_LoadItemIcon(play, sTargetDpad[t]);
    }
}

static int Bs_CanEquipNow(PlayState* play) {
    return play->pauseCtx.state == PAUSE_STATE_OFF && play->msgCtx.msgMode == MSGMODE_NONE && !Play_InCsMode(play) &&
           play->gameOverCtx.state == GAMEOVER_INACTIVE;
}

// O commit de KaleidoScope_UpdateItemEquip sem a animação. A regra de troca
// dele se resume a: outro botão com o mesmo slot recebe o conteúdo antigo do
// alvo. Arco com flecha mágica usa SLOT_BOW, então cai na mesma regra.
static void Bs_Equip(PlayState* play, int t, int slot) {
    u8 item = Bs_SlotItem(slot);
    u8* targetItem = Bs_TargetItem(t);
    u8* targetSlot = Bs_TargetSlot(t);
    s32 wornMask = Player_GetCurMaskItemId(play);

    if (!Bs_CanEquipNow(play) || item == ITEM_NONE ||
        // Máscara em uso (ou a da transformação atual) não sai do botão.
        (*targetItem != ITEM_NONE &&
         ((wornMask != ITEM_NONE && *targetItem == wornMask) || *targetItem == sFormMasks[GET_PLAYER_FORM]))) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    if (item == ITEM_ARROW_FIRE || item == ITEM_ARROW_ICE || item == ITEM_ARROW_LIGHT) {
        item = ITEM_BOW_FIRE + (item - ITEM_ARROW_FIRE);
        slot = SLOT_BOW;
    }
    for (int other = 0; other < TGT_COUNT; other++) {
        if (other != t && *Bs_TargetSlot(other) == slot) {
            *Bs_TargetItem(other) = *targetItem;
            *Bs_TargetSlot(other) = *targetItem == ITEM_NONE ? SLOT_NONE : *targetSlot;
            Bs_ReloadIcon(play, other);
        }
    }
    *targetItem = item;
    *targetSlot = slot;
    Bs_ReloadIcon(play, t);
    if (!TGT_IS_C(t) && !CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0)) {
        CVarSetInteger("gEnhancements.Dpad.DpadEquips", 1);
        CVarSave();
    }
    Audio_PlaySfx(NA_SE_SY_DECIDE);
}

// --- save rápido -------------------------------------------------------------
// Como o save de coruja (func_8014546C): flags do ciclo + prefixo do SaveContext
// até fileNum, mais o ponto exato para reaparecer. Um arquivo por slot do jogo
// e slot rápido; carregar não apaga (diferente da coruja).

#define QS_MAGIC 0x4D4D5153 // "MMQS"
#define QS_PREFIX offsetof(SaveContext, fileNum)

typedef struct {
    u32 magic;
    u32 prefixSize;
    s32 fileNum;
    u16 entrance;
    s16 yaw;
    s32 room;
    Vec3f pos;
} QuickSaveHeader;

static struct {
    int valid;
    int day;
    u16 time;
} sQsInfo[2];
static int sQsInfoLoaded;
// Resultado do último salvar/carregar, mostrado na página (antes só havia o
// som de erro e parecia que "nada acontecia").
static const char* sQsStatus = "";

static void Qs_Path(char* out, size_t size, int slot) {
    snprintf(out, size, "quick_f%d_%d.bin", (int)gSaveContext.fileNum, slot + 1);
}

static void Qs_RefreshInfo(void) {
    for (int slot = 0; slot < 2; slot++) {
        char path[32];
        QuickSaveHeader hdr;
        Qs_Path(path, sizeof(path), slot);
        sQsInfo[slot].valid = 0;
        FILE* f = fopen(path, "rb");
        if (f == NULL) {
            continue;
        }
        if (fread(&hdr, sizeof(hdr), 1, f) == 1 && hdr.magic == QS_MAGIC && hdr.prefixSize == QS_PREFIX &&
            fseek(f, offsetof(Save, day) + sizeof(hdr), SEEK_SET) == 0 &&
            fread(&sQsInfo[slot].day, sizeof(s32), 1, f) == 1 &&
            fseek(f, offsetof(Save, time) + sizeof(hdr), SEEK_SET) == 0 && fread(&sQsInfo[slot].time, 2, 1, f) == 1) {
            sQsInfo[slot].valid = 1;
        }
        fclose(f);
    }
    sQsInfoLoaded = 1;
}

static int Qs_CanUseNow(PlayState* play) {
    return Bs_CanEquipNow(play) && play->transitionTrigger == TRANS_TRIGGER_OFF &&
           play->transitionMode == TRANS_MODE_OFF && gSaveContext.fileNum != 0xFF;
}

static int Qs_Save(PlayState* play, int slot) {
    Player* player = GET_PLAYER(play);
    char path[32];
    if (!Qs_CanUseNow(play) || player == NULL || !SavingEnhancements_CanSave()) {
        // Mesmas regras do save do pause do 2S2H: nada antes de Clock Town,
        // em cutscene, diálogo, na Lua ou sem arquivo de save.
        sQsStatus = "Can't save here";
        return 0;
    }
    Play_SaveCycleSceneFlags(play);
    QuickSaveHeader hdr = { QS_MAGIC, QS_PREFIX, gSaveContext.fileNum, gSaveContext.save.entrance,
                            player->actor.shape.rot.y, play->roomCtx.curRoom.num, player->actor.world.pos };
    Qs_Path(path, sizeof(path), slot);
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        sQsStatus = "Write failed";
        return 0;
    }
    int ok = fwrite(&hdr, sizeof(hdr), 1, f) == 1 && fwrite(&gSaveContext, QS_PREFIX, 1, f) == 1;
    ok = fclose(f) == 0 && ok;
    sQsStatus = ok ? "Saved!" : "Write failed";
    fprintf(stderr, "mm-3ds quicksave: salvar slot %d -> %s\n", slot + 1, ok ? "ok" : "falhou");
    Qs_RefreshInfo();
    return ok;
}

static int Qs_Load(PlayState* play, int slot) {
    static u8 sBuf[QS_PREFIX];
    QuickSaveHeader hdr;
    char path[32];
    if (!Qs_CanUseNow(play)) {
        sQsStatus = "Can't load now";
        fprintf(stderr, "mm-3ds quicksave: carregar recusado (pause=%d msg=%d cs=%d trans=%d/%d file=%d)\n",
                play->pauseCtx.state, play->msgCtx.msgMode, Play_InCsMode(play), play->transitionTrigger,
                play->transitionMode, (int)gSaveContext.fileNum);
        return 0;
    }
    Qs_Path(path, sizeof(path), slot);
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    int ok = fread(&hdr, sizeof(hdr), 1, f) == 1 && hdr.magic == QS_MAGIC && hdr.prefixSize == QS_PREFIX &&
             hdr.fileNum == gSaveContext.fileNum && fread(sBuf, QS_PREFIX, 1, f) == 1;
    fclose(f);
    fprintf(stderr, "mm-3ds quicksave: carregar slot %d -> %s\n", slot + 1, ok ? "ok" : "invalido");
    if (!ok) {
        sQsStatus = "Save file invalid";
        return 0;
    }
    sQsStatus = "Loaded!";
    memcpy(&gSaveContext, sBuf, QS_PREFIX);
    // Reaparece no ponto salvo pelo caminho do void out (func_80169EFC).
    Play_SetRespawnData(play, RESPAWN_MODE_DOWN, hdr.entrance, hdr.room, PLAYER_PARAMS(0xFF, PLAYER_START_MODE_B),
                        &hdr.pos, hdr.yaw);
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempSwitchFlags = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].unk_18 = 0;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].tempCollectFlags = 0;
    play->nextEntrance = gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance;
    gSaveContext.respawnFlag = 1;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    return 1;
}

// Botões da página de save: slot 0..1 x {salvar, carregar}; -1 fora deles.
static int Qs_ButtonAt(int x, int y) {
    for (int slot = 0; slot < 2; slot++) {
        int y0 = 40 + slot * 76 + 16;
        if (y >= y0 && y < y0 + 32) {
            if (x >= PAGE_X + 148 && x < PAGE_X + 207) {
                return slot * 2;
            }
            if (x >= PAGE_X + 214 && x < PAGE_X + 273) {
                return slot * 2 + 1;
            }
        }
    }
    return -1;
}

static void Qs_Tap(PlayState* play, int x, int y) {
    int button = Qs_ButtonAt(x, y);
    if (button < 0) {
        sUi.confirm = -1;
        return;
    }
    if (button % 2 == 1 && !sQsInfo[button / 2].valid) {
        fprintf(stderr, "mm-3ds quicksave: slot %d sem save valido\n", button / 2 + 1);
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return;
    }
    if (sUi.confirm != button) {
        sUi.confirm = button; // 1º toque: pede confirmação
        sQsStatus = "";
        Audio_PlaySfx(NA_SE_SY_CURSOR);
        return;
    }
    sUi.confirm = -1;
    int ok = button % 2 == 0 ? Qs_Save(play, button / 2) : Qs_Load(play, button / 2);
    Audio_PlaySfx(ok ? NA_SE_SY_DECIDE : NA_SE_SY_ERROR);
}

// --- toque ------------------------------------------------------------------

// Canto superior-esquerdo da célula (col, row) do kaleido, em coordenadas da
// página: x = -96 + col*32, y = 58 - row*32 (z_kaleido_scope_NES.c).
static int Bs_CellX(int col) {
    return SX(-(ITEM_GRID_COLS * CELL_PAGE) / 2 + col * CELL_PAGE + 2);
}

static int Bs_CellY(int row) {
    return SY((ITEM_GRID_ROWS * CELL_PAGE) / 2 - 6 - row * CELL_PAGE - 2);
}

static int Bs_SlotAt(int x, int y) {
    const int pitch = CELL_PAGE * PAGE_SCALE_NUM / PAGE_SCALE_DEN;
    int col = (x - Bs_CellX(0) + 2) / pitch;
    int row = (y - Bs_CellY(0) + 2) / pitch;
    if (x < Bs_CellX(0) - 2 || y < Bs_CellY(0) - 2 || col >= ITEM_GRID_COLS || row >= ITEM_GRID_ROWS) {
        return -1;
    }
    return Bs_SlotBase() + row * ITEM_GRID_COLS + col;
}

static int Bs_TargetAt(int x, int y) {
    if (y < BAR_Y) {
        return -1;
    }
    for (int t = 0; t < TGT_COUNT; t++) {
        int x0 = TGT_X0 + t * TGT_PITCH;
        if (x >= x0 && x < x0 + TGT_W) {
            return t;
        }
    }
    return -1;
}

// Quest Status: tocar numa música mostra as notas dela na pauta (como o
// cursor do pause); tocar no Bombers' Notebook abre o caderno pelo mesmo
// caminho do pause (Play_Update; L/B/START fecham).
static const s16 sQuestHitLeft[] = { 45, 78, 10, 45, 80, 11, -109, -87, -65, -41, -19, -18,
                                     -109, -87, -65, -41, -19, -18, -103 };
static const s16 sQuestHitTop[] = { 62, 42, 42, 20, -9, -9, -20, -20, -20, -20, -20, -20,
                                    2, 2, 2, 2, 2, 2, 54 };

// Fim do playback (ou interrupção): instrumento desligado, como o kaleido faz
// ao sair de PAUSE_MAIN_STATE_SONG_PLAYBACK; a ocarina do jogo não fica ligada.
static void Bs_StopSong(void) {
    if (sUi.songPlaying) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
        sUi.songPlaying = 0;
    }
}

static void Bs_UpdateSong(PlayState* play) {
    if (!sUi.songPlaying) {
        return;
    }
    if (AudioOcarina_GetPlaybackStaff()->state == 0 || sUi.page != PAGE_QUEST || !Bs_CanEquipNow(play)) {
        Bs_StopSong();
    }
}

static int Bs_QuestAt(int x, int y) {
    for (int q = QUEST_SONG_SONATA; q <= QUEST_BOMBERS_NOTEBOOK; q++) {
        int w = q == QUEST_BOMBERS_NOTEBOOK ? 32 : 16, h = q == QUEST_BOMBERS_NOTEBOOK ? 32 : 24;
        if (x >= SX(sQuestHitLeft[q]) && x < SX(sQuestHitLeft[q] + w) && y >= SY(sQuestHitTop[q]) &&
            y < SY(sQuestHitTop[q] - h)) {
            return q;
        }
    }
    return -1;
}

static void Bs_QuestTap(PlayState* play, int x, int y) {
    int q = Bs_QuestAt(x, y);
    if (q == QUEST_BOMBERS_NOTEBOOK) {
        if (CHECK_QUEST_ITEM(QUEST_BOMBERS_NOTEBOOK) && Bs_CanEquipNow(play)) {
            play->pauseCtx.bombersNotebookOpen = true;
            Audio_PlaySfx(NA_SE_SY_DECIDE);
        } else {
            Audio_PlaySfx(NA_SE_SY_ERROR);
        }
    } else if (q >= QUEST_SONG_SONATA &&
               (CHECK_QUEST_ITEM(q) || (q == QUEST_SONG_LULLABY && CHECK_QUEST_ITEM(QUEST_SONG_LULLABY_INTRO)))) {
        if (sUi.questSong != q) {
            Bs_StopSong();
            sUi.questSong = q;
            Audio_PlaySfx(NA_SE_SY_CURSOR);
        } else if (!sUi.songPlaying && Bs_CanEquipNow(play)) {
            // 2º toque: toca a melodia como o pause (PAUSE_MAIN_STATE_SONG_PLAYBACK_INIT).
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
            AudioOcarina_SetPlaybackSong(gOcarinaSongItemMap[q - QUEST_SONG_SONATA] + 1, 1);
            AudioOcarina_GetPlaybackStaff()->pos = 0;
            sUi.songPlaying = 1;
        }
    }
}

static void Bs_SetPage(int page) {
    sUi.page = (page + PAGE_COUNT) % PAGE_COUNT;
    sUi.selSlot = -1;
    sUi.confirm = -1;
    Bs_StopSong();
    sUi.questSong = -1;
    if (sUi.page == PAGE_SAVE) {
        sQsStatus = "";
        Qs_RefreshInfo();
    }
    Audio_PlaySfx(NA_SE_SY_WIN_SCROLL_LEFT);
}

static void Bs_Release(PlayState* play, int x, int y) {
    fprintf(stderr, "mm-3ds bottom: toque (%d,%d) pagina %d\n", x, y, sUi.page);
    if (sUi.dragging) {
        int t = Bs_TargetAt(x, y);
        if (t >= 0) {
            Bs_Equip(play, t, sUi.dragSlot);
        }
        return;
    }
    if (y >= BAR_Y && x < ARROW_W) {
        Bs_SetPage(sUi.page - 1);
        return;
    }
    if (y >= BAR_Y && x >= 320 - ARROW_W) {
        Bs_SetPage(sUi.page + 1);
        return;
    }
    if (sUi.page == PAGE_SAVE && y < BAR_Y) {
        Qs_Tap(play, x, y);
        return;
    }
    if (sUi.page == PAGE_QUEST && y < BAR_Y) {
        Bs_QuestTap(play, x, y);
        return;
    }
    int slot = Bs_HasGrid() ? Bs_SlotAt(x, y) : -1;
    if (slot >= 0) {
        if (Bs_SlotItem(slot) != ITEM_NONE) {
            sUi.selSlot = slot == sUi.selSlot ? -1 : slot;
            Audio_PlaySfx(NA_SE_SY_CURSOR);
        }
        return;
    }
    int t = Bs_TargetAt(x, y);
    if (t < 0) {
        return;
    }
    if (sUi.selSlot >= 0) {
        Bs_Equip(play, t, sUi.selSlot);
        sUi.selSlot = -1;
    } else if (!TGT_IS_C(t) && *Bs_TargetItem(t) != ITEM_NONE) {
        // Toque num slot de D-pad sem item na mão: usa o item, como no MM3D.
        sUi.injectButtons |= sTargetButton[t];
    }
}

static void Bs_Update(PlayState* play) {
    int x = 0, y = 0;
    int held = Soh3dsTouchRead != NULL && Soh3dsTouchRead(&x, &y);
    if (held) {
        // Duas amostras seguidas antes de valer: o Azahar solta toques de um
        // frame na emenda das telas e o 1º contato do painel é o mais ruidoso.
        if (++sUi.heldFrames < 2) {
            return;
        }
        sUi.x = x;
        sUi.y = y;
        if (!sUi.down) {
            sUi.down = 1;
            sUi.downX = x;
            sUi.downY = y;
            sUi.dragging = 0;
            sUi.dragSlot = -1;
            if (Bs_HasGrid()) {
                int slot = Bs_SlotAt(x, y);
                if (slot >= 0 && Bs_SlotItem(slot) != ITEM_NONE) {
                    sUi.dragSlot = slot;
                }
            }
        } else if (sUi.dragSlot >= 0 && !sUi.dragging) {
            int dx = x - sUi.downX, dy = y - sUi.downY;
            if (dx * dx + dy * dy > SLOP_SQ) {
                sUi.dragging = 1;
                sUi.selSlot = -1;
            }
        }
        return;
    }
    sUi.heldFrames = 0;
    if (sUi.down) {
        sUi.down = 0;
        Bs_Release(play, sUi.x, sUi.y);
        sUi.dragging = 0;
        sUi.dragSlot = -1;
    }
}

void Mm3dsBottomScreen_InjectInput(Input* input) {
    if (sUi.injectButtons != 0) {
        input->cur.button |= sUi.injectButtons;
        input->press.button |= sUi.injectButtons;
        sUi.injectButtons = 0;
    }
}

// --- desenho ----------------------------------------------------------------

static Gfx* Bs_FillState(Gfx* gfx) {
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    return gfx;
}

static Gfx* Bs_Fill(Gfx* gfx, int x0, int y0, int x1, int y1, u8 r, u8 g, u8 b, u8 a) {
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, r, g, b, a);
    gDPFillRectangle(gfx++, x0, y0, x1, y1);
    return gfx;
}

static Gfx* Bs_IconState(Gfx* gfx) {
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    return gfx;
}

static Gfx* Bs_Icon(Gfx* gfx, u8 item, int x, int y, u8 alpha) {
    if (item >= ARRAY_COUNT(gItemIcons)) {
        return gfx;
    }
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
    gDPLoadTextureBlock(gfx++, gItemIcons[item], G_IM_FMT_RGBA, G_IM_SIZ_32b, ICON, ICON, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle(gfx++, x << 2, y << 2, (x + ICON) << 2, (y + ICON) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
                        1 << 10);
    return gfx;
}

static Gfx* Bs_TextState(Gfx* gfx) {
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    return gfx;
}

// Fonte 8x8 do console da libctru expandida para I8 (glifos com vida de
// processo, então o cache de texturas nunca guarda ponteiro temporário).
static u8 sGlyphs[95][64] __attribute__((aligned(8)));
static u8 sAdvance[95];

static void Bs_InitFont(void) {
    static int sReady;
    if (sReady) {
        return;
    }
    for (int c = 32; c < 127; ++c) {
        const u8* rows = default_font_bin + c * 8;
        u8 occupied = 0;
        for (int y = 0; y < 8; ++y) {
            occupied |= rows[y];
        }
        int left = 0, right = 7;
        if (occupied != 0) {
            while (!(occupied & (0x80 >> left))) ++left;
            while (!(occupied & (0x80 >> right))) --right;
        }
        for (int y = 0; y < 8; ++y) {
            u8 row = (u8)(rows[y] << left);
            for (int x = 0; x < 8; ++x) {
                sGlyphs[c - 32][y * 8 + x] = (row & (0x80 >> x)) ? 255 : 0;
            }
        }
        sAdvance[c - 32] = occupied ? (u8)(right - left + 2) : 4;
    }
    sReady = 1;
}

static int Bs_TextWidth(const char* s) {
    Bs_InitFont();
    int w = 0;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        w += sAdvance[(c >= 32 && c < 127 ? c : '?') - 32];
    }
    return w;
}

static Gfx* Bs_Text(Gfx* gfx, const char* s, int x, int y, u8 r, u8 g, u8 b) {
    Bs_InitFont();
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPSetPrimColor(gfx++, 0, 0, r, g, b, 255);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        int glyph = (c >= 32 && c < 127 ? c : '?') - 32;
        if (glyph != 0) {
            gDPLoadTextureBlock(gfx++, sGlyphs[glyph], G_IM_FMT_I, G_IM_SIZ_8b, 8, 8, 0, G_TX_CLAMP, G_TX_CLAMP,
                                G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(gfx++, x << 2, (y + 2) << 2, (x + 8) << 2, (y + 10) << 2, G_TX_RENDERTILE, 0, 0,
                                1024, 1024);
        }
        x += sAdvance[glyph];
    }
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    return gfx;
}

// Fundo da página: 15 quads IA8 80x32 (3 colunas x 5 linhas), mesma cor do
// kaleido (prim 180,180,120), em escala 1,25.
static Gfx* Bs_DrawPageBg(Gfx* gfx, TexturePtr* textures) {
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetPrimColor(gfx++, 0, 0, 180, 180, 120, 255);
    for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 5; row++) {
            int x0 = SX(-120 + col * 80), y0 = SY(80 - row * 32);
            int x1 = SX(-120 + (col + 1) * 80), y1 = SY(80 - (row + 1) * 32);
            gDPLoadTextureBlock(gfx++, textures[col * 5 + row], G_IM_FMT_IA, G_IM_SIZ_8b, 80, 32, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(gfx++, x0 << 2, y0 << 2, x1 << 2, y1 << 2, G_TX_RENDERTILE, 0, 0,
                                (80 << 10) / (x1 - x0), (32 << 10) / (y1 - y0));
        }
    }
    return gfx;
}

static Gfx* Bs_DrawTex(Gfx* gfx, TexturePtr tex, int fmtIA, int texSize, int x, int y, int size) {
    if (fmtIA) {
        gDPLoadTextureBlock(gfx++, tex, G_IM_FMT_IA, G_IM_SIZ_8b, texSize, texSize, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    } else {
        gDPLoadTextureBlock(gfx++, tex, G_IM_FMT_RGBA, G_IM_SIZ_32b, texSize, texSize, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    }
    gSPTextureRectangle(gfx++, x << 2, y << 2, (x + size) << 2, (y + size) << 2, G_TX_RENDERTILE, 0, 0,
                        (texSize << 10) / size, (texSize << 10) / size);
    return gfx;
}

static Gfx* Bs_PageRect(Gfx* gfx, int px, int py, int w, int h, int texW, int texH) {
    int x0 = SX(px), y0 = SY(py), x1 = SX(px + w), y1 = SY(py - h);
    gSPTextureRectangle(gfx++, x0 << 2, y0 << 2, x1 << 2, y1 << 2, G_TX_RENDERTILE, 0, 0, (texW << 10) / (x1 - x0),
                        (texH << 10) / (y1 - y0));
    return gfx;
}

// Número de munição com os dígitos do kaleido (KaleidoScope_DrawAmmoCount).
static Gfx* Bs_DrawAmmo(Gfx* gfx, int slot, u8 item, int x, int y) {
    int ammo;
    if (gAmmoItems[slot] == ITEM_NONE) {
        return gfx;
    }
    ammo = item == ITEM_PICTOGRAPH_BOX ? (CHECK_QUEST_ITEM(QUEST_PICTOGRAPH) ? 1 : 0) : AMMO(item);
    gDPPipeSync(gfx++);
    if (!gPlayerFormSlotRestrictions[GET_PLAYER_FORM][slot]) {
        gDPSetPrimColor(gfx++, 0, 0, 100, 100, 100, 255);
    } else if (ammo == 0) {
        gDPSetPrimColor(gfx++, 0, 0, 130, 130, 130, 255);
    } else {
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    }
    if (ammo >= 10) {
        gfx = Bs_DrawTex(gfx, gAmmoDigitTextures[(ammo / 10) % 10], 1, 8, x, y, 9);
    }
    return Bs_DrawTex(gfx, gAmmoDigitTextures[ammo % 10], 1, 8, x + 7, y, 9);
}

static Gfx* Bs_DrawGrid(Gfx* gfx) {
    const int base = Bs_SlotBase();
    const int pitch = CELL_PAGE * PAGE_SCALE_NUM / PAGE_SCALE_DEN;

    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

    // Contorno branco nos itens equipados (como o pause), amarelo no selecionado.
    for (int t = 0; t < TGT_COUNT; t++) {
        int slot = *Bs_TargetSlot(t);
        if (*Bs_TargetItem(t) != ITEM_NONE && slot >= base && slot < base + ITEM_NUM_SLOTS) {
            int i = slot - base;
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
            gfx = Bs_DrawTex(gfx, gEquippedItemOutlineTex, 1, 32, Bs_CellX(i % ITEM_GRID_COLS) - 2,
                             Bs_CellY(i / ITEM_GRID_COLS) - 2, pitch);
        }
    }
    if (sUi.selSlot >= base && sUi.selSlot < base + ITEM_NUM_SLOTS) {
        int i = sUi.selSlot - base;
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 0, 255);
        gfx = Bs_DrawTex(gfx, gEquippedItemOutlineTex, 1, 32, Bs_CellX(i % ITEM_GRID_COLS) - 2,
                         Bs_CellY(i / ITEM_GRID_COLS) - 2, pitch);
    }

    for (int i = 0; i < ITEM_NUM_SLOTS; i++) {
        int slot = base + i;
        u8 item = Bs_SlotItem(slot);
        if (item == ITEM_NONE || (sUi.dragging && slot == sUi.dragSlot)) {
            continue;
        }
        int x = Bs_CellX(i % ITEM_GRID_COLS), y = Bs_CellY(i / ITEM_GRID_COLS);
        int big = slot == sUi.selSlot ? 2 : 0; // o pause aumenta o item sob o cursor
        // Como no pause (gSPGrayscale): item que a forma atual não usa fica cinza.
        int restricted = !gPlayerFormItemRestrictions[GET_PLAYER_FORM][item];
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
        if (restricted) {
            gDPSetGrayscaleColor(gfx++, 109, 109, 109, 255);
            gSPGrayscale(gfx++, true);
        }
        gfx = Bs_DrawTex(gfx, gItemIcons[item], 0, ICON, x - big, y - big, ICON_SCREEN + 2 * big);
        if (restricted) {
            gSPGrayscale(gfx++, false);
        }
    }
    if (base == 0) {
        for (int i = 0; i < ITEM_NUM_SLOTS; i++) {
            u8 item = Bs_SlotItem(i);
            if (item != ITEM_NONE) {
                gfx = Bs_DrawAmmo(gfx, i, item, Bs_CellX(i % ITEM_GRID_COLS) + 2,
                                  Bs_CellY(i / ITEM_GRID_COLS) + ICON_SCREEN - 9);
            }
        }
    }
    return gfx;
}

// Barra de baixo: setas de página e os 7 alvos (D-pad cima, I, C<, Cv, C>, II,
// D-pad baixo), cada um com o rótulo embaixo. Amarelo = botão C (como no N64),
// azul = slot de D-pad (tocar usa o item).
static Gfx* Bs_DrawBar(Gfx* gfx) {
    gfx = Bs_FillState(gfx);
    gfx = Bs_Fill(gfx, 0, BAR_Y, 319, 239, 16, 16, 14, 255);
    gfx = Bs_Fill(gfx, 1, BAR_Y + 1, ARROW_W - 3, 238, 60, 58, 46, 255);
    gfx = Bs_Fill(gfx, 320 - ARROW_W + 2, BAR_Y + 1, 318, 238, 60, 58, 46, 255);
    for (int t = 0; t < TGT_COUNT; t++) {
        int x0 = TGT_X0 + t * TGT_PITCH;
        int c = TGT_IS_C(t);
        gfx = Bs_Fill(gfx, x0, BAR_Y + 1, x0 + TGT_W - 1, BAR_Y + TGT_H, c ? 230 : 70, c ? 190 : 130, c ? 0 : 200,
                      255);
        gfx = Bs_Fill(gfx, x0 + 2, BAR_Y + 3, x0 + TGT_W - 3, BAR_Y + TGT_H - 2, 28, 28, 24, 255);
    }
    gfx = Bs_IconState(gfx);
    for (int t = 0; t < TGT_COUNT; t++) {
        u8 item = *Bs_TargetItem(t);
        if (item != ITEM_NONE) {
            gDPPipeSync(gfx++);
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
            gfx = Bs_DrawTex(gfx, gItemIcons[item], 0, ICON, TGT_X0 + t * TGT_PITCH + 6, BAR_Y + 3, 22);
        }
    }
    if (sUi.dragging) {
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
        gfx = Bs_DrawTex(gfx, gItemIcons[Bs_SlotItem(sUi.dragSlot)], 0, ICON, sUi.x - ICON / 2, sUi.y - ICON / 2,
                         ICON);
    }
    gfx = Bs_TextState(gfx);
    gfx = Bs_Text(gfx, "<", 10, BAR_Y + 7, 255, 255, 255);
    gfx = Bs_Text(gfx, ">", 306, BAR_Y + 7, 255, 255, 255);
    gfx = Bs_Text(gfx, "Page", 2, BAR_Y + 23, 200, 196, 170);
    gfx = Bs_Text(gfx, "Page", 293, BAR_Y + 23, 200, 196, 170);
    for (int t = 0; t < TGT_COUNT; t++) {
        const char* label = sTargetLabel[t];
        int c = TGT_IS_C(t);
        gfx = Bs_Text(gfx, label, TGT_X0 + t * TGT_PITCH + (TGT_W - Bs_TextWidth(label)) / 2, BAR_Y + TGT_H + 1,
                      c ? 240 : 150, c ? 200 : 190, c ? 60 : 240);
    }
    return gfx;
}

// Quest Status: mesmas posições (sQuestVtxRect*) e cores do kaleido.
static const s16 sQuestLeft[] = { 45, 78, 10, 45, 80, 11, -109, -87, -65, -41, -19, -18,
                                  -109, -87, -65, -41, -19, -18, -103, 7, 82, -110, -54 };
static const s16 sQuestTop[] = { 62, 42, 42, 20, -9, -9, -20, -20, -20, -20, -20, -20,
                                 2, 2, 2, 2, 2, 2, 54, -44, -44, 34, 58 };
static const u8 sSongColor[12][3] = { { 150, 255, 100 }, { 255, 80, 40 }, { 100, 150, 255 }, { 255, 160, 0 },
                                      { 255, 100, 255 }, { 255, 240, 100 }, { 255, 255, 255 }, { 255, 255, 255 },
                                      { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 } };

static Gfx* Bs_QuestIcon(Gfx* gfx, int quest, TexturePtr tex, int fmtIA, int texSize, int pageSize) {
    int x = SX(sQuestLeft[quest]), y = SY(sQuestTop[quest]);
    return Bs_DrawTex(gfx, tex, fmtIA, texSize, x, y, pageSize * PAGE_SCALE_NUM / PAGE_SCALE_DEN);
}

static Gfx* Bs_DrawQuest(Gfx* gfx) {
    static TexturePtr sQuiver[3] = { gItemIconQuiver30Tex, gItemIconQuiver40Tex, gItemIconQuiver50Tex };
    static TexturePtr sBombBag[3] = { gItemIconBombBag20Tex, gItemIconBombBag30Tex, gItemIconBombBag40Tex };

    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    for (int i = QUEST_REMAINS_ODOLWA; i <= QUEST_REMAINS_TWINMOLD; i++) {
        if (CHECK_QUEST_ITEM(i)) {
            gfx = Bs_QuestIcon(gfx, i, gItemIcons[ITEM_REMAINS_ODOLWA + i], 0, 32, 32);
        }
    }
    if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) != EQUIP_VALUE_SHIELD_NONE) {
        gfx = Bs_QuestIcon(gfx, QUEST_SHIELD, gItemIcons[(ITEM_SHIELD_HERO - 1) + GET_CUR_EQUIP_VALUE(1)], 0, 32, 32);
    }
    if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) != EQUIP_VALUE_SWORD_NONE) {
        gfx = Bs_QuestIcon(gfx, QUEST_SWORD, gItemIcons[(ITEM_SWORD_KOKIRI - 1) + GET_CUR_EQUIP_VALUE(0)], 0, 32, 32);
    }
    if (CHECK_QUEST_ITEM(QUEST_BOMBERS_NOTEBOOK)) {
        gfx = Bs_QuestIcon(gfx, QUEST_BOMBERS_NOTEBOOK, gItemIconBombersNotebookTex, 0, 32, 32);
    }
    if (GET_CUR_UPG_VALUE(UPG_QUIVER) != 0) {
        gfx = Bs_QuestIcon(gfx, QUEST_QUIVER, sQuiver[GET_CUR_UPG_VALUE(UPG_QUIVER) - 1], 0, 32, 32);
    }
    if (GET_CUR_UPG_VALUE(UPG_BOMB_BAG) != 0) {
        gfx = Bs_QuestIcon(gfx, QUEST_BOMB_BAG, sBombBag[GET_CUR_UPG_VALUE(UPG_BOMB_BAG) - 1], 0, 32, 32);
    }
    // Notas das músicas aprendidas (16x24, recuadas 2 px como no kaleido).
    for (int i = 0; i < 12; i++) {
        int quest = QUEST_SONG_SONATA + i;
        if (CHECK_QUEST_ITEM(quest) || (quest == QUEST_SONG_LULLABY && CHECK_QUEST_ITEM(QUEST_SONG_LULLABY_INTRO))) {
            int x = SX(sQuestLeft[quest] + 2), y = SY(sQuestTop[quest] - 2);
            gDPPipeSync(gfx++);
            gDPSetPrimColor(gfx++, 0, 0, sSongColor[i][0], sSongColor[i][1], sSongColor[i][2], 255);
            gDPLoadTextureBlock(gfx++, gItemIconSongNoteTex, G_IM_FMT_IA, G_IM_SIZ_8b, 16, 24, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(gfx++, x << 2, y << 2, (x + 15) << 2, (y + 25) << 2, G_TX_RENDERTILE, 0, 0,
                                (16 << 10) / 15, (24 << 10) / 25);
        }
    }
    // Pedaços de coração (IA8 48x48, cor do kaleido no repouso).
    if (GET_QUEST_HEART_PIECE_COUNT != 0) {
        gDPPipeSync(gfx++);
        gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                          ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
        gDPSetPrimColor(gfx++, 0, 0, 255, 70, 50, 255);
        gDPSetEnvColor(gfx++, 0, 0, 0, 255);
        gfx = Bs_QuestIcon(gfx, QUEST_HEART_PIECE, gItemIcons[0x7A + GET_QUEST_HEART_PIECE_COUNT], 1, 48, 48);
    }
    // Música selecionada: botões da ocarina na pauta, mesmas alturas e cores do
    // kaleido (PAUSE_MAIN_STATE_IDLE_CURSOR_ON_SONG).
    if (sUi.questSong >= 0) {
        static TexturePtr sButtons[] = { gOcarinaATex, gOcarinaCDownTex, gOcarinaCRightTex, gOcarinaCLeftTex,
                                         gOcarinaCUpTex };
        static const s16 sButtonY[] = { -62, -56, -49, -46, -41 };
        s16 quest = sUi.questSong;
        OcarinaSongButtons* song = &gOcarinaSongButtons[gOcarinaSongItemMap[quest - QUEST_SONG_SONATA]];
        int x = SX(sQuestLeft[quest]) - 2, y = SY(sQuestTop[quest]) - 2;
        gfx = Bs_FillState(gfx);
        gfx = Bs_Fill(gfx, x, y, x + 20, y + 32, 255, 255, 0, 120); // realce da nota escolhida
        gDPPipeSync(gfx++);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        for (int k = 0; k < song->numButtons && k < 8; k++) {
            u8 button = song->buttonIndex[k];
            // Tocando: notas ainda não tocadas ficam apagadas (cinza do kaleido).
            int lit = !sUi.songPlaying || k < AudioOcarina_GetPlaybackStaff()->pos;
            gDPPipeSync(gfx++);
            if (!lit) {
                gDPSetPrimColor(gfx++, 0, 0, 150, 150, 150, 150);
            } else if (button == OCARINA_BTN_A) {
                gDPSetPrimColor(gfx++, 0, 0, 80, 150, 255, 255);
            } else {
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 50, 255);
            }
            gDPLoadTextureBlock(gfx++, sButtons[button], G_IM_FMT_IA, G_IM_SIZ_8b, 16, 16, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gfx = Bs_PageRect(gfx, -98 + k * 12 + 2, sButtonY[button], 12, 12, 16, 16);
        }
    }
    return gfx;
}

static Gfx* Bs_DrawCenterMessage(Gfx* gfx, const char* line1, const char* line2) {
    gfx = Bs_TextState(gfx);
    gfx = Bs_Text(gfx, line1, 160 - Bs_TextWidth(line1) / 2, 92, 255, 255, 255);
    return Bs_Text(gfx, line2, 160 - Bs_TextWidth(line2) / 2, 106, 220, 220, 200);
}

static TexturePtr sPageItems[15] = {
    gPauseSelectItem00ENGTex, gPauseSelectItem01Tex,    gPauseSelectItem02Tex, gPauseSelectItem03Tex,
    gPauseSelectItem04Tex,    gPauseSelectItem10ENGTex, gPauseSelectItem11Tex, gPauseSelectItem12Tex,
    gPauseSelectItem13Tex,    gPauseSelectItem14Tex,    gPauseSelectItem20ENGTex, gPauseSelectItem21Tex,
    gPauseSelectItem22Tex,    gPauseSelectItem23Tex,    gPauseSelectItem24Tex,
};
static TexturePtr sPageMasks[15] = {
    gPauseMasks00Tex, gPauseMasks01Tex, gPauseMasks02Tex, gPauseMasks03Tex, gPauseMasks04Tex,
    gPauseMasks10ENGTex, gPauseMasks11Tex, gPauseMasks12Tex, gPauseMasks13Tex, gPauseMasks14Tex,
    gPauseMasks20Tex, gPauseMasks21Tex, gPauseMasks22Tex, gPauseMasks23Tex, gPauseMasks24Tex,
};
static TexturePtr sPageMap[15] = {
    gPauseMap00Tex, gPauseMap01Tex, gPauseMap02Tex, gPauseMap03Tex, gPauseMap04Tex,
    gPauseMap10ENGTex, gPauseMap11Tex, gPauseMap12Tex, gPauseMap13Tex, gPauseMap14Tex,
    gPauseMap20Tex, gPauseMap21Tex, gPauseMap22Tex, gPauseMap23Tex, gPauseMap24Tex,
};
// Mapa-múndi (KaleidoScope_DrawWorldMap): imagem CI8 216x128, nuvens nas
// regiões não reveladas, pontos nas visitadas e o rosto do Link na atual.
// Coordenadas da página do kaleido. ponytail: dentro de dungeon o pause troca
// para o mapa da dungeon; aqui fica o mapa-múndi.
static const s16 sCloudX[15] = { -41, 9, 43, -21, -37, 43, -69, -69, -32, -109, -45, -109, -45, 59, -5 };
static const s16 sCloudY[15] = { 11, 22, -16, -16, -16, 59, 24, 59, -11, -5, -5, 59, 59, 39, 31 };
static const s16 sCloudW[15] = { 80, 64, 64, 64, 16, 64, 112, 112, 48, 64, 64, 64, 64, 48, 64 };
static const s16 sCloudH[15] = { 40, 31, 53, 53, 53, 52, 35, 35, 32, 64, 64, 64, 64, 82, 61 };
static const s16 sRegionX[REGION_MAX] = { -70, -52, -11, -2, 8, 2, 1, 43, 52, 54, 62 };
static const s16 sRegionY[REGION_MAX] = { -10, -36, -19, -53, -27, -2, 41, 11, -3, 45, 18 };
// sWorldMapCursorsRect* estão em coordenadas de tela do mapa plano (x0 = 51 = página -109, y0 = 62 = página 59).
static const s16 sFaceLeft[REGION_MAX] = { 86, 104, 145, 153, 163, 159, 157, 199, 208, 210, 218 };
static const s16 sFaceTop[REGION_MAX] = { 127, 153, 138, 171, 146, 119, 77, 106, 120, 73, 99 };


static int Bs_CurrentRegion(PlayState* play) {
    for (int n = 0; n < REGION_MAX; n++) {
        for (int j = 0; j < 27 && gSceneIdsPerRegion[n][j] != 0xFFFF; j++) {
            if (gSceneIdsPerRegion[n][j] == play->sceneId) {
                return n;
            }
        }
    }
    return -1;
}

static Gfx* Bs_DrawWorldMap(PlayState* play, Gfx* gfx) {
    static TexturePtr sClouds[15] = {
        gWorldMapClockTownCloud1Tex, gWorldMapClockTownCloud2Tex, gWorldMapWoodfallCloud1Tex,
        gWorldMapWoodfallCloud2Tex,  gWorldMapWoodfallCloud3Tex,  gWorldMapSnowheadCloud1Tex,
        gWorldMapSnowheadCloud2Tex,  gWorldMapSnowheadCloud3Tex,  gWorldMapRomaniRanchCloudTex,
        gWorldMapGreatBayCloud1Tex,  gWorldMapGreatBayCloud2Tex,  gWorldMapGreatBayCloud3Tex,
        gWorldMapGreatBayCloud4Tex,  gWorldMapStoneTowerCloud1Tex, gWorldMapStoneTowerCloud2Tex,
    };

    // Imagem: 16 faixas de 8 linhas (mesmo LoadMultiTile do kaleido).
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPLoadTLUT_pal256(gfx++, gWorldMapImageTLUT);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    for (int j = 0; j < 16; j++) {
        gDPLoadMultiTile(gfx++, gWorldMapImageTex, 0, G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_8b, WORLD_MAP_IMAGE_WIDTH,
                         WORLD_MAP_IMAGE_HEIGHT, 0, j * 8, WORLD_MAP_IMAGE_WIDTH - 1, (j + 1) * 8 - 1, 0,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                         G_TX_NOLOD);
        gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, (WORLD_MAP_IMAGE_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC,
                       (8 - 1) << G_TEXTURE_IMAGE_FRAC);
        gfx = Bs_PageRect(gfx, -109, 59 - j * 8, WORLD_MAP_IMAGE_WIDTH, 8, WORLD_MAP_IMAGE_WIDTH, 8);
    }
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);

    // Nuvens e pontos: prim/env do kaleido.
    gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(gfx++, 0, 0, 215, 235, 235, 255);
    gDPSetEnvColor(gfx++, 40, 60, 100, 0);
    for (int n = 0; n < 15; n++) {
        if (!(gSaveContext.save.saveInfo.worldMapCloudVisibility & gBitFlags[n])) {
            gDPLoadTextureBlock(gfx++, sClouds[n], G_IM_FMT_IA, G_IM_SIZ_8b, sCloudW[n], sCloudH[n], 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gfx = Bs_PageRect(gfx, sCloudX[n], sCloudY[n], sCloudW[n], sCloudH[n], sCloudW[n], sCloudH[n]);
        }
    }
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 255, 255);
    gDPSetEnvColor(gfx++, 255, 255, 0, 0);
    gDPLoadTextureBlock(gfx++, gWorldMapDotTex, G_IM_FMT_IA, G_IM_SIZ_8b, 8, 8, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    for (int i = 0; i < REGION_MAX; i++) {
        if ((gSaveContext.save.saveInfo.regionsVisited >> i) & 1) {
            gfx = Bs_PageRect(gfx, sRegionX[i], sRegionY[i], 8, 8, 8, 8);
        }
    }

    int region = Bs_CurrentRegion(play);
    if (region >= 0) {
        gDPPipeSync(gfx++);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
        gDPLoadTextureBlock(gfx++, gQuestIconLinkHumanFaceTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 16, 0,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gfx = Bs_PageRect(gfx, sFaceLeft[region] - 160, 121 - sFaceTop[region], 16, 16, 16, 16);
    }
    return gfx;
}

// Mapa de dungeon (KaleidoScope_DrawDungeonMap): o pause usa esta versão
// quando a sala tem mapa (Map_CurRoomHasMapI, mesmo teste que liga
// sInDungeonScene). Título, chave do chefe/bússola/mapa e fadas perdidas nas
// posições do kaleido. ponytail: sem a lista de andares nem o mapa da sala
// (MapDisp_DrawDungeonFloorSelect depende do sMapDisp que só o pause aloca);
// ligar MapDisp_AllocDungeonMap aqui se fizer falta.
static Gfx* Bs_DrawDungeonMap(Gfx* gfx) {
    static TexturePtr sTitles[4] = { gPauseWoodfallTitleENGTex, gPauseSnowheadTitleENGTex,
                                     gPauseGreatBayTitleENGTex, gPauseStoneTowerTitleENGTex };
    static TexturePtr sItems[3] = { gQuestIconBossKeyTex, gQuestIconCompassTex, gQuestIconDungeonMapTex };
    static TexturePtr sFairies[4] = { gStrayFairyWoodfallIconTex, gStrayFairySnowheadIconTex,
                                      gStrayFairyGreatBayIconTex, gStrayFairyStoneTowerIconTex };
    static const u8 sFairyPrim[4][3] = { { 255, 110, 160 }, { 90, 255, 100 }, { 120, 255, 255 }, { 245, 245, 90 } };
    static const u8 sFairyEnv[4][3] = { { 255, 255, 255 }, { 255, 255, 255 }, { 255, 255, 255 }, { 225, 170, 0 } };
    static const s16 sItemX[3] = { -104, -76, -48 };
    const int dungeon = gSaveContext.dungeonSceneSharedIndex;
    if (dungeon < 0 || dungeon >= 4) {
        return gfx;
    }

    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPLoadTextureBlock(gfx++, sTitles[dungeon], G_IM_FMT_IA, G_IM_SIZ_8b, 128, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gfx = Bs_PageRect(gfx, -21, 54, 128, 16, 128, 16);
    for (int i = DUNGEON_BOSS_KEY; i <= DUNGEON_MAP; i++) {
        if (CHECK_DUNGEON_ITEM(i, dungeon)) {
            gDPLoadTextureBlock(gfx++, sItems[i], G_IM_FMT_RGBA, G_IM_SIZ_32b, 24, 24, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gfx = Bs_PageRect(gfx, sItemX[i], -42, 24, 24, 24, 24);
        }
    }

    // Fada: o kaleido desenha em tela (54,140) 32x24; tela do pause -> página: x-160, 121-y.
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                      ENVIRONMENT, TEXEL0, ENVIRONMENT, TEXEL0, 0, PRIMITIVE, 0);
    gDPSetPrimColor(gfx++, 0, 0, sFairyPrim[dungeon][0], sFairyPrim[dungeon][1], sFairyPrim[dungeon][2], 255);
    gDPSetEnvColor(gfx++, sFairyEnv[dungeon][0], sFairyEnv[dungeon][1], sFairyEnv[dungeon][2], 0);
    gDPLoadTextureBlock(gfx++, sFairies[dungeon], G_IM_FMT_RGBA, G_IM_SIZ_32b, 32, 24, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gfx = Bs_PageRect(gfx, 54 - 160, 121 - 140, 32, 24, 32, 24);

    // Contagem "NN/MM" (KaleidoScope_DrawDungeonStrayFairyCount): dígitos I8 8x16
    // com sombra preta deslocada 1 px, em tela x 88/96, barra 106, total 116/124.
    int have = gSaveContext.save.saveInfo.inventory.strayFairies[dungeon];
    TexturePtr glyphs[5] = { sCounterTextures[(have / 10) % 10], sCounterTextures[have % 10],
                             gStrayFairyMapCounterSlashTex, sCounterTextures[STRAY_FAIRY_SCATTERED_TOTAL / 10],
                             sCounterTextures[STRAY_FAIRY_SCATTERED_TOTAL % 10] };
    static const s16 sGlyphX[5] = { 88, 96, 106, 116, 124 };
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (int pass = 0; pass < 2; pass++) {
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, pass ? 255 : 0, pass ? 255 : 0, pass ? 255 : 0, 255);
        for (int g = 0; g < 5; g++) {
            gDPLoadTextureBlock(gfx++, glyphs[g], G_IM_FMT_I, G_IM_SIZ_8b, 8, 16, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gfx = Bs_PageRect(gfx, sGlyphX[g] + (pass ? 0 : 1) - 160, 121 - (pass ? 145 : 146), 8, 16, 8, 16);
        }
    }
    return gfx;
}

// Página extra (não existe no pause): painel próprio com os 2 slots.
static Gfx* Bs_DrawSavePage(Gfx* gfx) {
    static const char* sDays[] = { "Day 0", "Day 1", "Day 2", "Day 3", "Day 4" };
    if (!sQsInfoLoaded) {
        Qs_RefreshInfo();
    }
    gfx = Bs_FillState(gfx);
    gfx = Bs_Fill(gfx, PAGE_X, 0, PAGE_X + 299, 199, 34, 30, 22, 255);
    gfx = Bs_Fill(gfx, PAGE_X + 4, 4, PAGE_X + 295, 195, 70, 64, 44, 255);
    for (int i = 0; i < 2; i++) {
        int y = 40 + i * 76;
        gfx = Bs_Fill(gfx, PAGE_X + 16, y, PAGE_X + 283, y + 64, 24, 22, 16, 255);
        int hot = sUi.confirm == i * 2;
        gfx = Bs_Fill(gfx, PAGE_X + 148, y + 16, PAGE_X + 206, y + 48, hot ? 200 : 60, hot ? 170 : 110, hot ? 40 : 60,
                      255);
        hot = sUi.confirm == i * 2 + 1;
        int usable = sQsInfo[i].valid;
        gfx = Bs_Fill(gfx, PAGE_X + 214, y + 16, PAGE_X + 272, y + 48, hot ? 200 : usable ? 60 : 50,
                      hot ? 170 : usable ? 80 : 50, hot ? 40 : usable ? 130 : 50, 255);
    }
    gfx = Bs_TextState(gfx);
    gfx = Bs_Text(gfx, "QUICK SAVE", 160 - Bs_TextWidth("QUICK SAVE") / 2, 14, 255, 236, 160);
    for (int i = 0; i < 2; i++) {
        int y = 40 + i * 76;
        char info[24];
        gfx = Bs_Text(gfx, i == 0 ? "Slot 1" : "Slot 2", PAGE_X + 26, y + 10, 255, 255, 255);
        if (sQsInfo[i].valid) {
            int minutes = (int)((u32)sQsInfo[i].time * 24 * 60 / 0x10000);
            int day = sQsInfo[i].day >= 0 && sQsInfo[i].day <= 4 ? sQsInfo[i].day : 0;
            snprintf(info, sizeof(info), "%s %02d:%02d", sDays[day], minutes / 60, minutes % 60);
        } else {
            snprintf(info, sizeof(info), "Empty");
        }
        gfx = Bs_Text(gfx, info, PAGE_X + 26, y + 30, 200, 200, 180);
        const char* save = sUi.confirm == i * 2 ? "Confirm?" : "Save";
        const char* load = sUi.confirm == i * 2 + 1 ? "Confirm?" : "Load";
        gfx = Bs_Text(gfx, save, PAGE_X + 177 - Bs_TextWidth(save) / 2, y + 28, 255, 255, 255);
        gfx = Bs_Text(gfx, load, PAGE_X + 243 - Bs_TextWidth(load) / 2, y + 28, 255, 255, 255);
    }
    if (sQsStatus[0] != '\0') {
        gfx = Bs_Text(gfx, sQsStatus, 160 - Bs_TextWidth(sQsStatus) / 2, 182, 255, 236, 160);
    }
    return gfx;
}

static TexturePtr sPageQuest[15] = {
    gPauseQuestStatus00ENGTex, gPauseQuestStatus01Tex, gPauseQuestStatus02Tex, gPauseQuestStatus03Tex,
    gPauseQuestStatus04Tex,    gPauseQuestStatus10ENGTex, gPauseQuestStatus11Tex, gPauseQuestStatus12Tex,
    gPauseQuestStatus13Tex,    gPauseQuestStatus14Tex, gPauseQuestStatus20ENGTex, gPauseQuestStatus21Tex,
    gPauseQuestStatus22Tex,    gPauseQuestStatus23Tex, gPauseQuestStatus24Tex,
};

void Mm3dsBottomScreen_Draw(PlayState* play) {
    static TexturePtr* const sPageBg[PAGE_COUNT] = { sPageItems, sPageMap, sPageQuest, sPageMasks, NULL };
    if (gBottomScreenFrameBuffer == -1) {
        return;
    }
    Bs_Update(play);
    Bs_UpdateSong(play);

    OPEN_DISPS(play->state.gfxCtx);
    gsSPSetFB(POLY_OPA_DISP++, gBottomScreenFrameBuffer);
    Gfx_SetupDL39_Opa(play->state.gfxCtx);
    Gfx* gfx = POLY_OPA_DISP;
    gfx = Bs_FillState(gfx);
    gfx = Bs_Fill(gfx, 0, 0, 319, 239, 0, 0, 0, 255);
    if (sPageBg[sUi.page] != NULL) {
        gfx = Bs_DrawPageBg(gfx, sPageBg[sUi.page]);
    }
    if (Bs_HasGrid()) {
        gfx = Bs_DrawGrid(gfx);
    } else if (sUi.page == PAGE_QUEST) {
        gfx = Bs_DrawQuest(gfx);
    } else if (sUi.page == PAGE_MAP && Map_CurRoomHasMapI(play)) {
        gfx = Bs_DrawDungeonMap(gfx);
    } else if (sUi.page == PAGE_MAP) {
        gfx = Bs_DrawWorldMap(play, gfx);
    } else {
        gfx = Bs_DrawSavePage(gfx);
    }
    gfx = Bs_DrawBar(gfx);
    gDPPipeSync(gfx++);
    gsSPResetFB(gfx++);
    POLY_OPA_DISP = gfx;
    CLOSE_DISPS(play->state.gfxCtx);
}
#endif
