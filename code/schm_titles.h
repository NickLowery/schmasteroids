#ifndef SCHM_TITLES_H 
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

#define TEXT_WIDTH (SCREEN_WIDTH * 0.9f)
#define MENU_H 0.75f
#define MENU_S 0.8f
#define MENU_C_L 4.0f
#define MENU_ZDISTSQ 4.0f

inline void
SetUpStartScreen(metagame_state *Metagame)
{
    Metagame->StartScreenState = StartScreenState_TitleAppearing;
    Metagame->StartScreenTimer = TITLE_APPEAR_TIME;
    Metagame->StartMenuOption = MenuOption_Play;
}

inline void
SetUpLevelStartScreen(metagame_state *Metagame, i32 LevelNumber)
{
    Assert(LevelNumber <= 99);

    SetStringFromNumber(Metagame->LevelNumberString, LevelNumber, ArrayCount(Metagame->LevelNumberString));
}

inline void
SetModeChangeTimer(metagame_state *Metagame, float Seconds)
{
    Metagame->ModeChangeTimer = Seconds;
    Metagame->ModeChangeTimerMax = Seconds;
}

inline void
SetUpEndScreen(metagame_state *Metagame)
{
    Metagame->EndScreenLightH = 0.0f;
}

#define  SCHM_TITLES_H 1
#endif
