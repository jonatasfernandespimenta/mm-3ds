#ifndef MM3DS_BOTTOM_SCREEN_3DS_H
#define MM3DS_BOTTOM_SCREEN_3DS_H
#ifdef __3DS__

struct PlayState;
struct Input;

// Framebuffer Fast3D que É a tela de baixo (framebuffer_effects.c).
extern s32 gBottomScreenFrameBuffer;
// z_kaleido_item.c: itens que cada forma pode usar.
extern u8 gPlayerFormSlotRestrictions[PLAYER_FORM_MAX][ITEM_NUM_SLOTS];

// Play_PostWorldDraw: toque + desenho da tela de baixo.
void Mm3dsBottomScreen_Draw(struct PlayState* play);
// Play_Main: atualizações extras por frame enquanto o avanço rápido é segurado.
int Mm3dsBottomScreen_FastForwardUpdates(struct PlayState* play);
// GameState_GetInput: botões disparados por toque (slots I/II).
void Mm3dsBottomScreen_InjectInput(struct Input* input);

#endif
#endif
