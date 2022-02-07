#ifndef SCHMASTEROIDS_START_SCREEN_H
#define TITLE_APPEAR_TIME 3.0f
#define TITLE_HOLD_TIME 3.0f
#define TITLE_MOVE_TIME 2.0f
#define INSTRUCTION_APPEAR_TIME 1.0f
enum start_screen_state {
    StartScreenState_TitleAppearing,
    StartScreenState_TitleHolding,
    StartScreenState_TitleMoving,
    StartScreenState_MenuAppearing,
    StartScreenState_MenuHolding,
    StartScreenState_DisplayingInstructions,
};

#define LEVEL_START_SCREEN_HOLD_TIME 1.0f
#define LEVEL_STATE_SCREEN_APPEAR_TIME 1.0f

#define  SCHMASTEROIDS_START_SCREEN_H 1
#endif
