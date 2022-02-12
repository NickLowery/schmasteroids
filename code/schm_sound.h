#ifndef SCHM_SOUND_H
#define SCHM_SOUND_H 1

#define DEFAULT_GAIN 0.5f
#define BYTES_PER_SAMPLE 2
#define BEATS_PER_MINUTE 122


// Format must be PCM.
// Channels must be 1 or 2
// Samples per second must equal GlobalSamplesPerSecond in platform
typedef struct {
    u16 Channels;
    i16* Samples;
    u32 AFrameCount;
    i16* OnePastEnd;
} game_compatible_wav;

typedef struct playing_sound_block_ {
    game_compatible_wav* Wav[8];
    i16* CurrentSample[8];
    float Gain[8];
    playing_sound_block_* NextBlock;
    i32 Count;
} playing_sound_block;

typedef struct {
    game_compatible_wav Wav;
    u32 MeasureCount;
    float Gain;
    i16* CurrentSample;
} game_music_clip;


typedef struct {
    union {
        game_compatible_wav Wavs[7];
        struct {
            game_compatible_wav ShipLaser;
            game_compatible_wav AsteroidExplosion;
            game_compatible_wav ShipExplosion;
            game_compatible_wav SaucerExplosion;
            game_compatible_wav SaucerSpawn;
            game_compatible_wav SaucerLaser;
            game_compatible_wav Terminator;
        };
    };
    game_music_clip GameLoop;
    game_music_clip OneBarKick;
    game_music_clip TitleLoop;
    game_music_clip TitleIntro;
} game_sounds;

typedef struct {
    game_music_clip *CurrentClip;
    game_music_clip *NextClip;
    game_music_clip *FadingInClip;
    game_music_clip *NextClipAfterFade;
    bool32 MusicFadingOut;
    float SecondsLeftInFade;
    playing_sound_block *FirstBlock;
    playing_sound_block *FirstFreeBlock;
} sound_state;

typedef struct {
    float StartVolume;
    float FinishVolume;
    float SecondsToOutput;
    bool32 Finishing;
    i32 AFrameIndexToFinishFade;
} fade_data;

internal fade_data
CalculateFadeData(sound_state *SoundState, platform_sound_output_buffer *SoundBuffer)
{
    i32 SampleRate = SoundBuffer->AFramesPerSecond;
    fade_data Result;

    Result.StartVolume = SoundState->CurrentClip->Gain;
    Result.SecondsToOutput = (float)SoundBuffer->AFramesToWrite / (float)SampleRate;
    Result.Finishing = (Result.SecondsToOutput > SoundState->SecondsLeftInFade);
    Result.FinishVolume = (Result.Finishing) ? 
                            0.0f:
                            Result.StartVolume * (1.0f - Result.SecondsToOutput/SoundState->SecondsLeftInFade);
    Result.AFrameIndexToFinishFade = (Result.Finishing) ?
                (i32)(SoundBuffer->AFramesToWrite / (SampleRate * (SoundState->SecondsLeftInFade/ Result.SecondsToOutput))):
                SoundBuffer->AFramesToWrite;

    return Result;
}

internal void
UpdateFadeState(sound_state *SoundState, fade_data Fade)
{
    if (Fade.Finishing) {
        Assert(SoundState->FadingInClip || SoundState->MusicFadingOut);
        SoundState->CurrentClip->Gain = DEFAULT_GAIN;
        if (SoundState->FadingInClip) {
            SoundState->CurrentClip = SoundState->FadingInClip;
            SoundState->CurrentClip->Gain = DEFAULT_GAIN;
            SoundState->FadingInClip = 0;
            if (SoundState->NextClipAfterFade) {
                SoundState->NextClip = SoundState->NextClipAfterFade;
                SoundState->NextClipAfterFade = 0;
            } else {
                SoundState->NextClip = 0;
            }
        } else {
            SoundState->CurrentClip = 0;
            SoundState->MusicFadingOut = 0;
        }
    } else {
        SoundState->SecondsLeftInFade -= Fade.SecondsToOutput;
    }
}


#endif
