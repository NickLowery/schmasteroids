#if DEBUG_BUILD
// TODO: Make this work again
inline void
DebugToggleMusic(metagame_state *Metagame)
{
    /*
    bool32 MusicPlaying = Metagame->Sounds.MusicLoop.Playing;
    Metagame->Sounds.MusicLoop.Playing = !MusicPlaying;
    */
}

#endif

internal game_compatible_wav 
CreateCompatibleWav(game_memory *GameMemory, memory_arena *Arena, platform_wav_load_info Info)
{
    i32 BytesPerSample = 2;
    game_compatible_wav Result = {};
    Result.Channels = (u16)Info.Channels;
    Result.AFrameCount = (u32)Info.SampleByteCount / (BytesPerSample*Info.Channels);
    Assert(Result.AFrameCount == Info.SampleByteCount / (BytesPerSample*Info.Channels));

    Result.Samples = (i16*)PushSize(Arena, Info.SampleByteCount);
    Result.OnePastEnd = Result.Samples + (Result.AFrameCount*Result.Channels);
    return Result;
}

internal game_compatible_wav LoadWav(game_memory *GameMemory, memory_arena *Arena, char*Filename)
{
    platform_wav_load_info WavInfo = GameMemory->PlatformGetWavLoadInfo(Filename);
    game_compatible_wav Result = CreateCompatibleWav(GameMemory, Arena, WavInfo);
    GameMemory->PlatformLoadWav(Filename, (void*)Result.Samples, WavInfo);
    return Result;
}


// TODO: Support a delay in seconds?
inline void
PlaySound(metagame_state *Metagame, game_compatible_wav *Wav, float Gain = DEFAULT_GAIN)
{
    sound_state *SoundState = &Metagame->SoundState;
    playing_sound_block *FirstBlock = SoundState->FirstBlock;
    if (!FirstBlock || FirstBlock->Count == ArrayCount(FirstBlock->Wav)) {
        if (SoundState->FirstFreeBlock) {
            SoundState->FirstBlock = SoundState->FirstFreeBlock;
            SoundState->FirstFreeBlock = SoundState->FirstFreeBlock->NextBlock;
        } else {
            SoundState->FirstBlock = PushStruct(&Metagame->PermanentArena, playing_sound_block);
        }
        SoundState->FirstBlock->Count = 0;
        SoundState->FirstBlock->NextBlock = FirstBlock;
        FirstBlock = SoundState->FirstBlock;
    }

    i32 SoundIndex = FirstBlock->Count++;
    FirstBlock->Wav[SoundIndex] = Wav;
    FirstBlock->CurrentSample[SoundIndex] = Wav->Samples;
    FirstBlock->Gain[SoundIndex] = DEFAULT_GAIN;
}

typedef struct {
    float PositionInClip;
    float PositionInMeasure;
    i32 MeasuresComplete;
} music_position_data;
// NOTE: The goal here is to get as nearly as possible, the position in the measure that will be 
// actually playing when the current frame of video is displayed. Currently we are just accepting 
// whatever latency the platform layer hands us but we'd like to improve that.

// TODO: Have GetPositionInMeasure also?
inline music_position_data
GetMusicPositionData(sound_state *SoundState)
    // NOTE: Calculate at end of sound output and retain? Would it be worth it?
{
    music_position_data Result;
    game_music_clip *CurrentClip = SoundState->CurrentClip;
    Assert(CurrentClip);

    game_compatible_wav *CurrentWav = &SoundState->CurrentClip->Wav;
    i32 MeasureCount = CurrentClip->MeasureCount;
    Result.PositionInClip = (float)(CurrentClip->CurrentSample - CurrentWav->Samples) /
                                  (float)(CurrentWav->OnePastEnd - CurrentWav->Samples);
    Result.MeasuresComplete = (i32)(Result.PositionInClip * MeasureCount);
    float StartOfCurrentMeasure = ((float)Result.MeasuresComplete / (float)MeasureCount);
    Result.PositionInMeasure = (Result.PositionInClip - StartOfCurrentMeasure) * (float)MeasureCount;
    return Result;
}

inline void
StartMusicCrossfade(metagame_state *Metagame, game_music_clip *ClipToFade, float SecondsToFade) 
{
    sound_state *SoundState = &Metagame->SoundState;
    game_music_clip *CurrentClip = SoundState->CurrentClip;
    game_compatible_wav *CurrentWav = &SoundState->CurrentClip->Wav;
    // TODO: Allow case where a clip is already fading?
    Assert(!SoundState->MusicFadingOut);
    Assert(!SoundState->FadingInClip);
    SoundState->FadingInClip = ClipToFade;
    SoundState->SecondsLeftInFade = SecondsToFade;
    i32 MeasureCount = CurrentClip->MeasureCount;
    music_position_data Playhead = GetMusicPositionData(SoundState);
    i32 MeasuresToGo = (Playhead.PositionInMeasure == 0.0f) ?
                         MeasureCount - Playhead.MeasuresComplete:
                         MeasureCount - Playhead.MeasuresComplete - 1;
    i32 MeasuresIntoNextClipToStart = Playhead.MeasuresComplete % ClipToFade->MeasureCount;
    float PositionInNextClip = ((float)MeasuresIntoNextClipToStart / (float)ClipToFade->MeasureCount) +
                                Playhead.PositionInMeasure/(float)ClipToFade->MeasureCount;
    i32 AFramesIntoNextClipToStart = (i32)(PositionInNextClip * (float)(ClipToFade->Wav.AFrameCount));
    ClipToFade->CurrentSample = ClipToFade->Wav.Samples +
                                  (AFramesIntoNextClipToStart * ClipToFade->Wav.Channels);
    Assert(ClipToFade->CurrentSample <= ClipToFade->Wav.OnePastEnd);
}

inline void
PlayClipNextOrAfterFade(metagame_state *Metagame, game_music_clip *Clip) 
{
    sound_state *SoundState = &Metagame->SoundState;
    if (SoundState->FadingInClip) {
        SoundState->NextClipAfterFade = Clip;
    } else {
        SoundState->NextClip = Clip;
    }
}

inline void
ResetClip(game_music_clip *Clip) 
{
    Clip->CurrentSample = Clip->Wav.Samples;
    Clip->Gain = DEFAULT_GAIN;
}

inline void
ResetOrAdvanceCurrentClipIfFinished(sound_state *SoundState) {
    game_music_clip *CurrentClip = SoundState->CurrentClip;
    game_compatible_wav *CurrentWav = &CurrentClip->Wav;
    if (CurrentClip->CurrentSample == CurrentWav->OnePastEnd) {
        if (SoundState->NextClip) {
            SoundState->CurrentClip = SoundState->NextClip;
            SoundState->NextClip = 0;
            ResetClip(SoundState->CurrentClip);
        } else {
            ResetClip(CurrentClip);
        }
    }
}
extern "C"
GAME_GET_SOUND_OUTPUT(GameGetSoundOutput)
{
    BENCH_START_FRAME_CYCLE_COUNT(PlaySound)
    BENCH_START_COUNTING_CYCLES(PlaySound)
    Assert(sizeof(metagame_state) < GameMemory->PermanentStorageSize);
    metagame_state *Metagame = (metagame_state*)GameMemory->PermanentStorage;
    sound_state *SoundState = &Metagame->SoundState;

    i16* SampleOut = SoundBuffer->SampleData;
    for(i32 AFrameIndex = 0; AFrameIndex < SoundBuffer->AFramesToWrite; AFrameIndex++) {
        i16 OutValue[2] = {0,0};
        playing_sound_block *FirstBlock = SoundState->FirstBlock;
        for(playing_sound_block *Block = FirstBlock;
            Block;
            Block = Block->NextBlock) 
        {
            for(i32 SoundIndex = 0; SoundIndex < Block->Count; SoundIndex++)
            {
                game_compatible_wav *Wav = Block->Wav[SoundIndex];
                Assert(Block->CurrentSample[SoundIndex] < Wav->OnePastEnd);
                Assert(Block->CurrentSample[SoundIndex] >= Wav->Samples);
                float Gain = Block->Gain[SoundIndex];
                OutValue[0] += (i16)(Gain * (float)*(Block->CurrentSample[SoundIndex]++));
                OutValue[1] += (i16)(Gain * (float)*(Block->CurrentSample[SoundIndex]++));
                if (Block->CurrentSample[SoundIndex] == Wav->OnePastEnd) {
                    i32 SourceFirstBlockIndex = --FirstBlock->Count;
                    Block->Wav[SoundIndex] = FirstBlock->Wav[SourceFirstBlockIndex];
                    Block->CurrentSample[SoundIndex] = FirstBlock->CurrentSample[SourceFirstBlockIndex];
                    Block->Gain[SoundIndex] = FirstBlock->Gain[SourceFirstBlockIndex];

                    if (FirstBlock->Count == 0) {
                        SoundState->FirstBlock = FirstBlock->NextBlock;
                        FirstBlock->NextBlock = SoundState->FirstFreeBlock;
                        SoundState->FirstFreeBlock = FirstBlock;
                        FirstBlock = SoundState->FirstBlock;
                        Assert(FirstBlock == 0 || FirstBlock->Count == ArrayCount(FirstBlock->Wav));
                    }
                }
            }
        }
            
        *SampleOut++ = OutValue[0];
        *SampleOut++ = OutValue[1];
    }

    SampleOut = SoundBuffer->SampleData;
    if (SoundState->FadingInClip) {
        fade_data Fade = CalculateFadeData(SoundState, SoundBuffer);
        for(i32 AFrameIndex = 0; AFrameIndex < SoundBuffer->AFramesToWrite; AFrameIndex++) {
            i16 OutValue[2] = {0,0};
            float CurrentClipVolume = (AFrameIndex > Fade.AFrameIndexToFinishFade) ?
                                        0.0f:
                                        Lerp(Fade.StartVolume, 
                                             (float)AFrameIndex / (float)Fade.AFrameIndexToFinishFade, 
                                             Fade.FinishVolume);
            float FadingInClipVolume = DEFAULT_GAIN - CurrentClipVolume;
            game_music_clip *FadingInClip = SoundState->FadingInClip;
            game_music_clip *CurrentClip = SoundState->CurrentClip;


            OutValue[0] += (i16)(CurrentClipVolume * (float)*CurrentClip->CurrentSample++);
            OutValue[1] += (i16)(CurrentClipVolume * (float)*CurrentClip->CurrentSample++);
            OutValue[0] += (i16)(FadingInClipVolume * (float)*FadingInClip->CurrentSample++);
            OutValue[1] += (i16)(FadingInClipVolume * (float)*FadingInClip->CurrentSample++);
            ResetOrAdvanceCurrentClipIfFinished(SoundState);
            if (FadingInClip->CurrentSample == FadingInClip->Wav.OnePastEnd) {
                ResetClip(FadingInClip);
            }

            *SampleOut++ += OutValue[0];
            *SampleOut++ += OutValue[1];
        }
        SoundState->CurrentClip->Gain = Fade.FinishVolume;
        UpdateFadeState(SoundState, Fade);

    } else if (SoundState->MusicFadingOut) {
        fade_data Fade = CalculateFadeData(SoundState, SoundBuffer);
        game_music_clip *CurrentClip = SoundState->CurrentClip;
        for(i32 AFrameIndex = 0; AFrameIndex < SoundBuffer->AFramesToWrite; AFrameIndex++) {
            i16 OutValue[2] = {0,0};
            float CurrentClipVolume = (AFrameIndex > Fade.AFrameIndexToFinishFade) ?
                                        0.0f:
                                        Lerp(Fade.StartVolume, 
                                             (float)AFrameIndex / (float)Fade.AFrameIndexToFinishFade, 
                                             Fade.FinishVolume);

            OutValue[0] += (i16)(CurrentClipVolume * (float)*CurrentClip->CurrentSample++);
            OutValue[1] += (i16)(CurrentClipVolume * (float)*CurrentClip->CurrentSample++);
            ResetOrAdvanceCurrentClipIfFinished(SoundState);

            *SampleOut++ += OutValue[0];
            *SampleOut++ += OutValue[1];
        }
        CurrentClip->Gain = Fade.FinishVolume;
        UpdateFadeState(SoundState, Fade);

    } else {
        if (SoundState->CurrentClip) {
            for(i32 AFrameIndex = 0; AFrameIndex < SoundBuffer->AFramesToWrite; AFrameIndex++) {
                i16 OutValue[2] = {0,0};
                game_music_clip *CurrentClip = SoundState->CurrentClip;
                OutValue[0] += (i16)(CurrentClip->Gain * (float)*CurrentClip->CurrentSample++);
                OutValue[1] += (i16)(CurrentClip->Gain * (float)*CurrentClip->CurrentSample++);
                ResetOrAdvanceCurrentClipIfFinished(SoundState);

                *SampleOut++ += OutValue[0];
                *SampleOut++ += OutValue[1];
            }
        }
    }

    BENCH_STOP_COUNTING_CYCLES(PlaySound)
    BENCH_FINISH_FRAME_ALL
}
