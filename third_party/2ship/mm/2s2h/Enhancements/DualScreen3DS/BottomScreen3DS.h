#ifndef MM3DS_BOTTOM_SCREEN_3DS_H
#define MM3DS_BOTTOM_SCREEN_3DS_H
#ifdef __3DS__

struct PlayState;
struct Input;

// Fast3D framebuffer that IS the bottom screen (framebuffer_effects.c).
extern s32 gBottomScreenFrameBuffer;
// z_kaleido_item.c: items each form can use.
extern u8 gPlayerFormSlotRestrictions[PLAYER_FORM_MAX][ITEM_NUM_SLOTS];

// Play_PostWorldDraw: bottom screen touch + drawing.
void Mm3dsBottomScreen_Draw(struct PlayState* play);
// Play_Main: extra updates per frame while fast-forward is held.
int Mm3dsBottomScreen_FastForwardUpdates(struct PlayState* play);
// GameState_GetInput: buttons fired by touch (slots I/II).
void Mm3dsBottomScreen_InjectInput(struct Input* input);

#endif
#endif
