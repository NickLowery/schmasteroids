
#define TEXT_WIDTH (SCREEN_WIDTH * 0.9f)
#define MENU_H 0.75f
#define MENU_S 0.8f
#define MENU_C_L 4.0f
#define MENU_ZDISTSQ 4.0f
internal void
UpdateAndDrawLevelStartScreen(metagame_state *Metagame, render_buffer *Renderer, game_input *Input, float SceneAlpha = 255)
{
    sound_state *SoundState = &Metagame->SoundState;
    light_source Light;
    Light.H = 0.3f;
    Light.S = 0.5f;
    Light.C_L = 2.0f * SceneAlpha;
    Light.ZDistSq = 3.0f;
    float dTime = Input->SecondsElapsed;
    if (Metagame->CurrentGameMode == GameMode_LevelStartScreen &&
        SoundState->NextClip == 0) {
        PlayClipNextOrAfterFade(Metagame, &Metagame->Sounds.GameLoop);
    }
    // TODO: More robust way of deciding if the clip will be playing next frame... use whatever
    // we do to get pulsing lights on the beat
    if ((Metagame->CurrentGameMode == GameMode_LevelStartScreen) &&
        (UpdateAndCheckTimer(&Metagame->StartScreenTimer, dTime)) &&
        SoundState->CurrentClip == &Metagame->Sounds.GameLoop) 
    {
        // TODO: Something better than just short-circuiting the transition for this?
        Metagame->CurrentGameMode = GameMode_Playing;
        Metagame->NextGameMode = GameMode_None;
    } 


    v2 GlyphDim = CalculateGlyphDimFromWidth(Metagame, StringLength("level"), SCREEN_WIDTH/2);
    v2 CenterPoint = ScreenCenter;
    v2 FirstLineMinCorner = CenterPoint - V2((GlyphDim.X / 2.0f * 5.0f),
            GlyphDim.Y * 1.25f);
    v2 SecondLineMinCorner = CenterPoint + 
        V2(-(GlyphDim.X / 2.0f * StringLength(Metagame->LevelNumberString)),
                GlyphDim.Y * 0.25f);
    color TextColor = Color(0xFF8888FF);

    PrintFromMinCornerAndGlyphDim(Renderer, Metagame, &Light,
            "level", FirstLineMinCorner, GlyphDim);
    PrintFromMinCornerAndGlyphDim(Renderer, Metagame, &Light,
            Metagame->LevelNumberString, 
            SecondLineMinCorner, GlyphDim);
}

inline void 
PrintMenu(render_buffer *Renderer, metagame_state *Metagame, float MenuC_LFraction)
{
    float SelectedMenuH = 0.67f;
    float MenuZDistSq = 4.0f;
    light_source MenuLight = light_source{MENU_H, MENU_S, MENU_C_L, MENU_ZDISTSQ};
    float MenuMargin = CalculateLightMargin(&MenuLight, 1.0f/64.0f);
    MenuLight.C_L *= MenuC_LFraction;
    float MenuWidth = TEXT_WIDTH - (2.0f*MenuMargin);

    char *MenuOptions[MenuOption_Terminator];
    MenuOptions[MenuOption_Play] = "play";
    MenuOptions[MenuOption_ToggleFullScreen] = "toggle fullscreen";
    MenuOptions[MenuOption_Instructions] = "instructions";
    MenuOptions[MenuOption_Quit] = "quit";

    u32 CharCount = MaxStringLength(MenuOptions, ArrayCount(MenuOptions));
    v2 GlyphDim = CalculateGlyphDimFromWidth(Metagame, CharCount, MenuWidth);

    v2 CenterBottom = V2(ScreenCenter.X, ScreenRect.Bottom - MenuMargin);
    for (i32 MenuIndex = ArrayCount(MenuOptions) - 1; MenuIndex >= 0; --MenuIndex)
    {
        if (MenuIndex == Metagame->StartMenuOption) {
            MenuLight.H = SelectedMenuH;
        } else {
            MenuLight.H = MENU_H;
        }
        PrintFromGlyphSizeAndCenterBottom(Renderer, Metagame,
                &MenuLight, MenuOptions[MenuIndex], GlyphDim, CenterBottom);
        CenterBottom.Y -= (GlyphDim.Y + (2.0f*MenuMargin));
    }
}

internal void
UpdateAndDrawTitleScreen(metagame_state *Metagame, render_buffer *Renderer, game_input *Input, float SceneAlpha = 1.0f)
{
    float dTime = Input->SecondsElapsed;
    char* Title = "schmasteroids";
    float TitleH = 0.0f;
    float TitleS = 1.0f;
    float FinishedTitleC_L = 8.0f;
    float TitleC_L = 0.0f;
    float TitleZDistSq = 20.0f;
    light_source FinishedTitleLight = light_source{TitleH, TitleS, FinishedTitleC_L, TitleZDistSq};
    float TitleMargin = CalculateLightMargin(&FinishedTitleLight, 1.0f/64.0f);
    v2 TitleGlyphDim = CalculateGlyphDimFromWidth(Metagame, StringLength(Title), SCREEN_WIDTH);
    float FinalTitleY = ScreenRect.Top + TitleMargin + TitleGlyphDim.Y/2;
    float TitleY = FinalTitleY;

    if (!Metagame->SoundState.CurrentClip) {
        Metagame->SoundState.CurrentClip = &Metagame->Sounds.TitleIntro;
        ResetClip(Metagame->SoundState.CurrentClip);

        Metagame->SoundState.NextClip = &Metagame->Sounds.TitleLoop;
    }

    if (!Metagame->NextGameMode) {
        if (WentDown(&Input->Keyboard.Fire)) {
            if (Metagame->StartScreenState == StartScreenState_MenuHolding) {
                switch (Metagame->StartMenuOption) {
                    case MenuOption_Play: {
                        Metagame->NextGameMode = GameMode_LevelStartScreen;
                        SetModeChangeTimer(Metagame, LEVEL_STATE_SCREEN_APPEAR_TIME);
                        StartMusicCrossfade(Metagame, &Metagame->Sounds.OneBarKick, 2.0f);
                        PlayClipNextOrAfterFade(Metagame, &Metagame->Sounds.GameLoop);
                    } break;
                    case MenuOption_Instructions: {
                        Metagame->StartScreenState = StartScreenState_DisplayingInstructions;
                    } break;
                    case MenuOption_ToggleFullScreen: {
                        Metagame->GameMemory->PlatformToggleFullscreen();
                    } break;
                    case MenuOption_Quit: {
                        Metagame->GameMemory->PlatformQuit();
                    } break;
                    InvalidDefault;
                }
            } else {
                Metagame->StartScreenState = StartScreenState_MenuHolding;
            }
        }
        if (WentDown(&Input->Keyboard.MoveDown) &&
            (Metagame->StartScreenState == StartScreenState_MenuHolding || 
             Metagame->StartScreenState == StartScreenState_MenuAppearing )) 
        {
            Metagame->StartMenuOption = (menu_options)((Metagame->StartMenuOption + 1) % MenuOption_Terminator);
        }
        if (WentDown(&Input->Keyboard.MoveUp) &&
            (Metagame->StartScreenState == StartScreenState_MenuHolding || 
             Metagame->StartScreenState == StartScreenState_MenuAppearing )) 
        {
            Metagame->StartMenuOption = (menu_options)((Metagame->StartMenuOption + MenuOption_Terminator - 1) % MenuOption_Terminator);
        }
    }

    switch (Metagame->StartScreenState) {
        case (StartScreenState_TitleAppearing):
        {
            TitleY = ScreenCenter.Y;
            if (UpdateAndCheckTimer(&Metagame->StartScreenTimer, dTime)) {
                TitleC_L = FinishedTitleC_L;
                Metagame->StartScreenTimer += TITLE_HOLD_TIME;
                Metagame->StartScreenState = StartScreenState_TitleHolding;
                break;
            }
            else {
                TitleC_L = (FinishedTitleC_L * 
                        (1.0f - (Metagame->StartScreenTimer/TITLE_APPEAR_TIME)));
                
                break;
            }
        }
        case (StartScreenState_TitleHolding):
        {
            TitleY = ScreenCenter.Y;
            if (UpdateAndCheckTimer(&Metagame->StartScreenTimer, dTime)) {
                Metagame->StartScreenTimer += TITLE_MOVE_TIME;
                Metagame->StartScreenState = StartScreenState_TitleMoving;
            }
            else {
                TitleC_L = FinishedTitleC_L;
                break;
            }
        } 
        case (StartScreenState_TitleMoving):
        {
            if (UpdateAndCheckTimer(&Metagame->StartScreenTimer, dTime)) {
                Metagame->StartScreenTimer += INSTRUCTION_APPEAR_TIME;
                Metagame->StartScreenState = StartScreenState_MenuAppearing;
            }
            else {
                TitleC_L = FinishedTitleC_L;
                TitleY = Lerp(FinalTitleY, (Metagame->StartScreenTimer / TITLE_MOVE_TIME), 
                                            SCREEN_HEIGHT/2);
                break;
            }
        }
        case (StartScreenState_MenuAppearing):
        {
            if (UpdateAndCheckTimer(&Metagame->StartScreenTimer, dTime)) {
                Metagame->StartScreenState = StartScreenState_MenuHolding;
            }
            else {
                TitleC_L = FinishedTitleC_L;
                float MenuC_LFraction = 1.0f - Metagame->StartScreenTimer/INSTRUCTION_APPEAR_TIME;
                PrintMenu(Renderer, Metagame, MenuC_LFraction * SceneAlpha);
                break;
            }
        } 
        case (StartScreenState_MenuHolding):
        {
            TitleC_L = FinishedTitleC_L;
            PrintMenu(Renderer, Metagame, SceneAlpha);
        } break;
        case (StartScreenState_DisplayingInstructions):
        {
            light_source InstructionsLight = light_source{MENU_H, MENU_S, MENU_C_L, MENU_ZDISTSQ};
            TitleC_L = FinishedTitleC_L;
            char *Instructions[] = {"space to shoot", "arrows to fly", "go for it", "press space now"};
            float InstructionsMargin = CalculateLightMargin(&InstructionsLight, 1.0f/64.0f);
            u32 CharCount = MaxStringLength(Instructions, ArrayCount(Instructions));
            v2 GlyphDim = CalculateGlyphDimFromWidth(Metagame, CharCount, TEXT_WIDTH);

            v2 CenterBottom = V2(ScreenCenter.X, ScreenRect.Bottom - InstructionsMargin);
            for (i32 InstructionsIndex = ArrayCount(Instructions) - 1; InstructionsIndex >= 0; --InstructionsIndex)
            {
                PrintFromGlyphSizeAndCenterBottom(Renderer, Metagame,
                        &InstructionsLight, Instructions[InstructionsIndex], GlyphDim, CenterBottom);
                CenterBottom.Y -= (GlyphDim.Y + (2.0f*InstructionsMargin));
            }
                    
        } break;
        InvalidDefault;
    }

    light_source TitleLight = light_source{TitleH, TitleS, TitleC_L * SceneAlpha, TitleZDistSq};
    PrintFromXBoundsAndCenterY(Renderer,  Metagame, &TitleLight, Title, 
                               ScreenRect.Left + TitleMargin, ScreenRect.Right - TitleMargin, TitleY);



}
