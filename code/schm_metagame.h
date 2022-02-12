
#ifndef SCHM_METAGAME_H
typedef struct _metagame_state {
    // NOTE: For now, nothing persists past frame boundaries in the transient arena, i.e.
    // it's all temporary memory
    memory_arena TransientArena;
    memory_arena PermanentArena;
    memory_arena WavArena;

    game_memory *GameMemory;

    game_state GameState;
    game_mode CurrentGameMode;
    game_mode NextGameMode;
    float ModeChangeTimer;
    float ModeChangeTimerMax;

    float GlyphYOverX;
    glyph Digits[10];
    glyph Letters[26];
    glyph Decimal;
    
    game_sounds Sounds;
    sound_state SoundState;

    light_params LightParams;

    //LEVEL START SCREEN
    char LevelNumberString[3];
    //START SCREEN
    float StartScreenTimer;
    start_screen_state StartScreenState;
    float InstructionsY;
    menu_options StartMenuOption;
    // END SCREEN
    float EndScreenLightH;
    

#if DEBUG_BUILD
    _edit_mode EditMode;
    editor_state EditorState;
#endif
} metagame_state;

inline game_state *
GetGameState(metagame_state *MetagameState);
    
inline void
SetModeChangeTimer(metagame_state *MetagameState, float Seconds);

inline void
SetUpLevel(metagame_state *MetagameState, i32 LevelNumber);

inline void
SetUpStartScreen(metagame_state *MetagameState);

inline void
SetUpLevelStartScreen(metagame_state *MetagameState, i32 LevelNumber);

#if DEBUG_BUILD
inline void
DebugToggleMusic(metagame_state *Metagame);
#endif

#define SCHM_METAGAME_H
#endif
