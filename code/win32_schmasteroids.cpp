#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <timeapi.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "schm_platform_includes.h"
#include "schm_main.h"
#include "schm_platform.h"
#include "schm_math.h"
#include "win32_schmasteroids.h"

static bool GlobalRunning;

static BITMAPINFO GlobalBackbufferBitmapInfo;
static i32 GlobalBackbufferInWindowX;
static i32 GlobalBackbufferInWindowY;

static HWND GlobalMainWindow;
static WINDOWPLACEMENT GlobalPrevWindowPos = { sizeof(GlobalPrevWindowPos) };
//NOTE: These are locked for now. Change later if it makes sense!
static platform_framebuffer GlobalBackbuffer;
static LONGLONG GlobalPerfFrequency;
static bool32 GlobalSleepIsGranular = false;

static u32 GlobalSamplesPerSecond = 48000;
static IAudioClient* GlobalSoundClient;
static IAudioRenderClient* GlobalSoundRenderClient;

#if DEBUG_BUILD
static bool GlobalDebugPause;
#endif

#if DEBUG_BUILD
void ErrorDescription(HRESULT hr) 
{ 
     if(FACILITY_WINDOWS == HRESULT_FACILITY(hr)) 
         hr = HRESULT_CODE(hr); 
     TCHAR* szErrMsg; 

     if(FormatMessage( 
       FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM, 
       NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
       (LPTSTR)&szErrMsg, 0, NULL) != 0) 
     { 
         OutputDebugString(szErrMsg); 
         LocalFree(szErrMsg); 
     } else {
         OutputDebugStringA("Couldn't parse error");
     }

}
#endif

PLATFORM_QUIT(Win32Quit)
{
    GlobalRunning = false;
}

PLATFORM_TOGGLE_FULLSCREEN(Win32ToggleFullScreen)
{
    // Toggle fullscreen a la Raymond Chen
    // https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
    DWORD Style = GetWindowLong(GlobalMainWindow, GWL_STYLE);
    if (Style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (GetWindowPlacement(GlobalMainWindow, &GlobalPrevWindowPos) &&
                GetMonitorInfo(MonitorFromWindow(GlobalMainWindow,
                        MONITOR_DEFAULTTOPRIMARY), &MonitorInfo)) {
            SetWindowLong(GlobalMainWindow, GWL_STYLE,
                    Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(GlobalMainWindow, HWND_TOP,
                    MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                    MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                    MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(GlobalMainWindow, GWL_STYLE,
                Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(GlobalMainWindow, &GlobalPrevWindowPos);
        SetWindowPos(GlobalMainWindow, NULL, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

PLATFORM_DEBUG_READ_FILE(Win32DebugReadFile)
{
    DWORD BytesRead;
    HANDLE FileHandle = CreateFileA(Filename,
            GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize)) {
            u32 FileSize32 = (u32)FileSize.QuadPart;
            Assert(FileSize32 == FileSize.QuadPart);
            Assert(Memory);
            if(!ReadFile(FileHandle, Memory, ReadSize, &BytesRead, 0)) { 
                OutputDebugStringA("Couldn't read file:\n");
                OutputDebugStringA(Filename);
                OutputDebugStringA("\n");
                return 0;
            } else if (ReadSize != BytesRead) {
                OutputDebugStringA("Less bytes than expected read from:\n");
                OutputDebugStringA(Filename);
                OutputDebugStringA("\n");
            }

        } else {
            return 0;
        }
        CloseHandle(FileHandle);
        return BytesRead;
    }
    else {
        OutputDebugStringA("Couldn't open file:\n");
        OutputDebugStringA(Filename);
        OutputDebugStringA("\n");
        return 0;
    }
}

PLATFORM_DEBUG_WRITE_FILE(Win32DebugWriteFile)
{
    HANDLE FileHandle = CreateFileA(Filename,
            GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS,
            0, 0);
    DWORD BytesWritten;
    DWORD BytesToWrite = (DWORD)WriteSize;
    Assert(BytesToWrite == WriteSize);
    WriteFile(FileHandle, Memory, 
            BytesToWrite, &BytesWritten, NULL);
    Assert(BytesWritten == BytesToWrite);
    CloseHandle(FileHandle);
}

PLATFORM_DEBUG_SAVE_FRAMEBUFFER_AS_BMP(Win32DebugSaveFramebufferAsBMP)
{
    BITMAPFILEHEADER FileHeader = {};
    size_t FileSize  = GlobalBackbuffer.MemorySize + sizeof(GlobalBackbufferBitmapInfo) +
                          sizeof(FileHeader);
    DWORD TotalBytesToWrite = (DWORD)FileSize;
    Assert(TotalBytesToWrite == FileSize);
    FileHeader.bfType = 0x4D42;
    FileHeader.bfSize = TotalBytesToWrite;
    FileHeader.bfOffBits = sizeof(GlobalBackbufferBitmapInfo) + sizeof(FileHeader);
    HANDLE FileHandle = CreateFileA(Filename,
            GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS,
            0, 0);
    DWORD TotalBytesWritten = 0;
    DWORD BytesWritten;
    WriteFile(FileHandle, &FileHeader, 
            (DWORD)sizeof(FileHeader), &BytesWritten, NULL);
    TotalBytesWritten += BytesWritten;
    WriteFile(FileHandle, &GlobalBackbufferBitmapInfo, 
            (DWORD)sizeof(GlobalBackbufferBitmapInfo), &BytesWritten, NULL);
    TotalBytesWritten += BytesWritten;
    WriteFile(FileHandle, GlobalBackbuffer.Memory, 
            (DWORD)GlobalBackbuffer.MemorySize, &BytesWritten, NULL);
    TotalBytesWritten += BytesWritten;

    Assert(TotalBytesWritten == TotalBytesToWrite);
    CloseHandle(FileHandle);
}

// NOTE: At least for now we only accept PCM, 16 bits, mono or stereo, sample rate matching global
PLATFORM_GET_WAV_LOAD_INFO(Win32GetWavLoadInfo) 
{
    platform_wav_load_info Result = {};
    HANDLE FileHandle = CreateFileA(Filename,
            GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE) {
        DWORD BytesRead;
        wav_header WavHeader = {};
        wav_chunk Chunk = {};
        if(ReadFile(FileHandle, &WavHeader, sizeof(WavHeader), &BytesRead, 0) &&
                    (sizeof(WavHeader) == BytesRead)) 
        {
            Result.StartOfSamplesInFile += BytesRead;
            Assert(strncmp(WavHeader.ChunkHeader.ckID, "RIFF", 4) == 0);
            Assert(strncmp(WavHeader.WAVEID, "WAVE", 4) == 0);
        }
        else {
            OutputDebugStringA("Couldn't read file:\n");
            OutputDebugStringA(Filename);
            OutputDebugStringA("\n");
        } 
        // TODO: More robust checking?
        ReadFile(FileHandle, &Chunk, sizeof(Chunk.ChunkHeader), &BytesRead, 0);
        Result.StartOfSamplesInFile += BytesRead;
        Assert(strncmp(Chunk.ChunkHeader.ckID, "fmt ", 4) == 0);
        ReadFile(FileHandle, &Chunk.wFormatTag, Chunk.ChunkHeader.ckSize, &BytesRead, 0);
        Result.StartOfSamplesInFile += BytesRead;
        Assert(Chunk.wFormatTag == 1);
        Assert(Chunk.nChannels <= 2);
        Result.Channels = Chunk.nChannels;
        Assert(Chunk.nSamplesPerSec == GlobalSamplesPerSecond);
        // TODO: Sample rate conversion support?
        Assert(Chunk.wBitsPerSample == 16);

        ReadFile(FileHandle, &Chunk, sizeof(Chunk.ChunkHeader), &BytesRead, 0);
        Result.StartOfSamplesInFile += BytesRead;
        while(strncmp(Chunk.ChunkHeader.ckID, "data", 4)) {
            u32 BytesToTrash = Chunk.ChunkHeader.ckSize;
            while (BytesToTrash > sizeof(Chunk)) {
                ReadFile(FileHandle, &Chunk, sizeof(Chunk), &BytesRead, 0);
                Result.StartOfSamplesInFile += BytesRead;
                BytesToTrash -= sizeof(Chunk);
            }
            ReadFile(FileHandle, &Chunk, BytesToTrash, &BytesRead, 0);
            Result.StartOfSamplesInFile += BytesRead;

            ReadFile(FileHandle, &Chunk, sizeof(Chunk.ChunkHeader), &BytesRead, 0);
            Result.StartOfSamplesInFile += BytesRead;
        }
        Result.SampleByteCount = Chunk.ChunkHeader.ckSize;
        CloseHandle(FileHandle);
    }
    else {
        OutputDebugStringA("Couldn't open wav file:\n");
        OutputDebugStringA(Filename);
        OutputDebugStringA("\n");
    }
    return Result;
}

PLATFORM_LOAD_WAV(Win32LoadWav)
{
    HANDLE FileHandle = CreateFileA(Filename,
            GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE) {
        DWORD StartOfSamplesInFile = (DWORD)Info.StartOfSamplesInFile;
        Assert(StartOfSamplesInFile == Info.StartOfSamplesInFile);
        DWORD SampleByteCount = (DWORD)Info.SampleByteCount;
        Assert(SampleByteCount == Info.SampleByteCount);

        DWORD BytesRead = 0;
        wav_chunk_header Header = {};
        DWORD BytesToChunkHeader = StartOfSamplesInFile - sizeof(Header);

        while(BytesToChunkHeader > SampleByteCount) {
            ReadFile(FileHandle, Memory, SampleByteCount, &BytesRead, 0);
            BytesToChunkHeader -= BytesRead;
        }
        ReadFile(FileHandle, Memory, BytesToChunkHeader, &BytesRead, 0);
        ReadFile(FileHandle, &Header, sizeof(Header), &BytesRead, 0);
        Assert(strncmp(Header.ckID, "data", 4) == 0);
        Assert(Header.ckSize == Info.SampleByteCount);
        ReadFile(FileHandle, Memory, SampleByteCount, &BytesRead, 0);
        Assert(BytesRead == SampleByteCount);
        
        CloseHandle(FileHandle);
    } else {
        OutputDebugStringA("Couldn't open wav file:\n");
        OutputDebugStringA(Filename);
        OutputDebugStringA("\n");
    }
}

internal FILETIME Win32GetLastWriteTime(char *Filename)
{
    FILETIME Result = {};
    WIN32_FIND_DATAA FileData;
    HANDLE FindHandle = FindFirstFileA(Filename, &FileData);
    if (FindHandle != INVALID_HANDLE_VALUE) {
        Result = FileData.ftLastWriteTime;
        FindClose(FindHandle);
    }
    return Result;
}

#define LOOP_RANDOM_SEED 526
// TODO: We could get rid of this if we had our own PRNG that keeps state in game memory

// TODO: Write game state to memory-mapped file instead of a regular file to speed this up
// TODO: Record memory and input separately for greater flexibility?
internal void
Win32StartRecording(win32_state *WinState, win32_game_code Game, u8 Index) 
{

    Game.SeedRandom(LOOP_RANDOM_SEED);
    Assert(Index < 99);
    Assert(WinState->BuildDirPathLen < MAX_PATH - 9);
    char RecordFilename[9];
    sprintf_s(RecordFilename, "ir%d.gri", Index);
    char RecordFilepath[MAX_PATH];

    strcpy_s(RecordFilepath, MAX_PATH - 1, WinState->BuildDirPath);
    strcat_s(RecordFilepath, MAX_PATH - 1, RecordFilename);
    WinState->GameRecordHandle = CreateFileA(RecordFilepath,
            GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS,
            0, 0);
    DWORD BytesWritten;
    DWORD BytesToWrite = (DWORD)WinState->GameMemory.PermanentStorageSize;
    Assert(BytesToWrite == WinState->GameMemory.PermanentStorageSize);
    WriteFile(WinState->GameRecordHandle, WinState->GameMemory.PermanentStorage, 
            BytesToWrite, &BytesWritten, NULL);
    Assert(BytesWritten == BytesToWrite);
    WinState->GameRecordIndex = Index;
    
}

internal void
Win32EndRecording(win32_state* WinState) 
{
    CloseHandle(WinState->GameRecordHandle);
    WinState->GameRecordHandle = 0;
    WinState->GameRecordIndex = 0;
}

internal void
Win32StartPlayback(win32_state *WinState, win32_game_code Game, u8 Index) 
{
    Game.SeedRandom(LOOP_RANDOM_SEED);
    Assert(Index < 99);
    Assert(WinState->BuildDirPathLen < MAX_PATH - 9);
    char Filename[9];
    sprintf_s(Filename, "ir%d.gri", Index);
    char Filepath[MAX_PATH];
    strcpy_s(Filepath, MAX_PATH - 1, WinState->BuildDirPath);
    strcat_s(Filepath, MAX_PATH - 1, Filename);
    WinState->GamePlaybackHandle = CreateFileA(Filepath,
            GENERIC_READ, 0, NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, 0);
    DWORD BytesRead;
    DWORD BytesToRead = (DWORD)WinState->GameMemory.PermanentStorageSize;
    Assert(BytesToRead == WinState->GameMemory.PermanentStorageSize);
    ReadFile(WinState->GamePlaybackHandle, WinState->GameMemory.PermanentStorage,
            BytesToRead, &BytesRead, NULL);
    Assert(BytesRead == WinState->GameMemory.PermanentStorageSize);
    WinState->GamePlaybackIndex = Index;
    // set state index
}

internal void
Win32StartBenchmarkPlayback(win32_state *WinState)
{
    Assert(WinState->BuildDirPathLen < MAX_PATH - 9);
    char* Filename = "game_benchmark.gri";
    char Filepath[MAX_PATH];
    strcpy_s(Filepath, MAX_PATH - 1, WinState->BuildDirPath);
    strcat_s(Filepath, MAX_PATH - 1, Filename);
    WinState->GamePlaybackHandle = CreateFileA(Filepath,
            GENERIC_READ, 0, NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, 0);
    DWORD BytesRead;
    DWORD BytesToRead = (DWORD)WinState->GameMemory.PermanentStorageSize;
    Assert(BytesToRead == WinState->GameMemory.PermanentStorageSize);
    ReadFile(WinState->GamePlaybackHandle, WinState->GameMemory.PermanentStorage,
            BytesToRead, &BytesRead, NULL);
    Assert(BytesRead == WinState->GameMemory.PermanentStorageSize);
    WinState->GamePlaybackIndex = 1;
    // set state index
}

internal void
Win32EndPlayback(win32_state* WinState)
{
    CloseHandle(WinState->GamePlaybackHandle);
    WinState->GamePlaybackHandle = 0;
    WinState->GamePlaybackIndex = 0;
}

internal void
Win32PlaybackInput(win32_state* WinState, win32_game_code Game, game_input *Input)
{
    DWORD BytesToRead = (DWORD)(sizeof(game_input));
    Assert(BytesToRead == sizeof(game_input));
    DWORD BytesRead;
    ReadFile(WinState->GamePlaybackHandle, (void*)Input, 
            BytesToRead, &BytesRead, NULL);
    if (BytesRead == 0) {
        u8 Index = WinState->GamePlaybackIndex;
        Win32EndPlayback(WinState);
        Win32StartPlayback(WinState, Game, Index);
        ReadFile(WinState->GamePlaybackHandle, (void*)Input, 
                BytesToRead, &BytesRead, NULL);
    }
    Assert(BytesRead == BytesToRead);

}

internal void
Win32RecordInput(win32_state* WinState, game_input *Input)
{
    DWORD BytesToWrite = (DWORD)(sizeof(game_input));
    Assert(BytesToWrite == sizeof(game_input));
    DWORD BytesWritten;
    WriteFile(WinState->GameRecordHandle, (void*)Input, 
            BytesToWrite, &BytesWritten, NULL);
    Assert(BytesWritten == BytesToWrite);
}

internal win32_game_code GetGameCode(win32_exe_info *EXEInfo)
{
    win32_game_code Result = {};
    WIN32_FILE_ATTRIBUTE_DATA Ignored;
    if(!GetFileAttributesEx(EXEInfo->GameCodeLockFile, GetFileExInfoStandard, &Ignored))
    {
        bool FileCopied = false;
        while (!FileCopied) {
            EXEInfo->LastSourceDLLWriteTime = Win32GetLastWriteTime(EXEInfo->SourceGameDLLFullPath);
            char Buffer[256];
            sprintf_s(Buffer, "%d\n", EXEInfo->LastSourceDLLWriteTime.dwLowDateTime);
            OutputDebugStringA(Buffer);
            Sleep(5);
            FileCopied = CopyFile(EXEInfo->SourceGameDLLFullPath, EXEInfo->TempGameDLLFullPath, FALSE);
            if (!FileCopied) {
                OutputDebugStringA("DLL File copy failed\n");
            }
        }

        OutputDebugStringA("DLL File copied\n");
        Result.GameDLL = LoadLibraryA(EXEInfo->TempGameDLLFullPath);
        if (Result.GameDLL)
        {
            Result.GetSoundOutput = (game_get_sound_output*)GetProcAddress(Result.GameDLL, "GameGetSoundOutput");
            Result.Initialize = (game_initialize*)GetProcAddress(Result.GameDLL, "GameInitialize");
            Result.UpdateAndRender = (game_update_and_render*)GetProcAddress(Result.GameDLL, "GameUpdateAndRender");
            Result.SeedRandom = (game_seed_prng*)GetProcAddress(Result.GameDLL, "SeedRandom");
            Result.IsValid = (Result.GetSoundOutput && Result.Initialize && Result.UpdateAndRender);
        }
        if (!Result.IsValid)
        {
            Result.GetSoundOutput = GameGetSoundOutputStub;
            Result.Initialize = GameInitializeStub;
            Result.UpdateAndRender = GameUpdateAndRenderStub;
            Result.SeedRandom = GameSeedPRNGStub;
        }
    }
    return Result;
}

internal void ReleaseGameCode(win32_game_code *Game)
{
    if(Game->GameDLL) {
        FreeLibrary(Game->GameDLL);
    }
    Game->GameDLL = 0;
    Game->IsValid = false;
    Game->GetSoundOutput = GameGetSoundOutputStub;
    Game->Initialize = GameInitializeStub;
    Game->UpdateAndRender = GameUpdateAndRenderStub;
}

// NOTE: We're forcing the Width and Height to be multiples of 4 for alignment purposes.
void Win32ResizeBackbuffer(i32 Width, i32 Height)
{
    GlobalBackbuffer.Width = RoundDownToMultipleOf(4,Width);
    GlobalBackbuffer.Height = RoundDownToMultipleOf(4,Height);
    i32 UnadjustedPitch = GlobalBackbuffer.Width * BYTES_PER_PIXEL;
    i32 SSEAlignedPitch = RoundUpToMultipleOf(16, UnadjustedPitch);
    GlobalBackbuffer.Pitch = SSEAlignedPitch;
    Assert(GlobalBackbuffer.Pitch % 16 == 0);
    GlobalBackbuffer.MemorySize = 
        GlobalBackbuffer.Pitch * GlobalBackbuffer.Height;
    if(GlobalBackbuffer.Memory)
    {
        VirtualFree(GlobalBackbuffer.Memory, 0, MEM_RELEASE);
    }
    GlobalBackbuffer.Memory = (u8*)VirtualAlloc(0, GlobalBackbuffer.MemorySize, MEM_COMMIT, PAGE_READWRITE);
    GlobalBackbufferBitmapInfo.bmiHeader.biWidth = GlobalBackbuffer.Pitch/BYTES_PER_PIXEL;
    GlobalBackbufferBitmapInfo.bmiHeader.biHeight = -GlobalBackbuffer.Height;
}

typedef struct {
    i32 BlitW;
    i32 BlitH;
    i32 DestinationX;
    i32 DestinationY;
} win32_blit_params;

internal win32_blit_params
CalculateBlitParams(RECT ClientRect) {
    win32_blit_params Result = {};
    i32 ClientW = ClientRect.right - ClientRect.left;
    i32 ClientH = ClientRect.bottom - ClientRect.top;
    if (ClientW < GlobalBackbuffer.Width) {
        Result.BlitW = ClientW;
        Result.DestinationX = 0;
    } else {
        Result.BlitW = GlobalBackbuffer.Width;
        Result.DestinationX = (ClientW - Result.BlitW) / 2;
    }
    if (ClientH < GlobalBackbuffer.Height) {
        Result.BlitH = ClientH;
        Result.DestinationY = 0;
    } else {
        Result.BlitH = GlobalBackbuffer.Height;
        Result.DestinationY = (ClientH - Result.BlitH) / 2;
    }

    return Result;
}

void DrawBackbufferToWindow(HWND Window, HDC WindowDC) 
{
    RECT ClientRect;
    if (GetClientRect(Window, &ClientRect)) {
        // TODO: Is it necessary to do this here, or could I just save the
        // results whenever a WM_SIZE call happens? I guess what I don't know
        // is whether the WM_SIZE call will always beat this when the window size changes
        win32_blit_params BlitParams = CalculateBlitParams(ClientRect);

        int StretchDIBitsResult =  StretchDIBits(
                WindowDC, BlitParams.DestinationX, BlitParams.DestinationY, 
                BlitParams.BlitW, BlitParams.BlitH,
                0, 0, BlitParams.BlitW, BlitParams.BlitH,
                GlobalBackbuffer.Memory,
                &GlobalBackbufferBitmapInfo,
                DIB_RGB_COLORS,
                SRCCOPY
                );
        if(!StretchDIBitsResult) {
            OutputDebugStringA("StretchDIBits failed\n");
        }
        GlobalBackbufferInWindowX = BlitParams.DestinationX;
        GlobalBackbufferInWindowY = BlitParams.DestinationY;
    } else {
        OutputDebugStringA("Couldn't get client rect");
    }
}

// NOTE: We could pass the key_input by value and return the new version? Better performance-wise?
internal void HandleKeyboardMessage(MSG Message, game_button_input *KeyInput)
{
    static i32 TransitionState = 1<<31;
    static i32 PreviousState = 1<<30;
    bool32 IsDown = ((Message.lParam & (TransitionState)) == 0);
    KeyInput->IsDown = IsDown;
}

internal void InitWASAPI(i32 SamplesPerSecond, i32 BufferRequestedAFrames)
{
    // TODO: Fix so it works with my USB interface!
    // NOTE: Revisit if I'm multithreading.
    if(FAILED(CoInitializeEx(0, COINIT_SPEED_OVER_MEMORY))) {
        Assert(!"WASAPI Error");
    }
    IMMDeviceEnumerator* Enumerator;
    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&Enumerator)))) {
        Assert(!"WASAPI Error");
    }

    IMMDevice* Device;
    if (FAILED(Enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &Device))) {
        Assert(!"WASAPI Error");
    }

    if (FAILED(Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (LPVOID*)&GlobalSoundClient))) {
        Assert(!"WASAPI Error");
    }

    WAVEFORMATEXTENSIBLE WaveFormat;
    WAVEFORMATEXTENSIBLE Closest;
    WAVEFORMATEX *PointerClosest = &Closest.Format;

    WaveFormat.Format.cbSize = sizeof(WaveFormat);
    WaveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    WaveFormat.Format.wBitsPerSample = 16;
    WaveFormat.Format.nChannels = 2;
    WaveFormat.Format.nSamplesPerSec = (DWORD)SamplesPerSecond;
    WaveFormat.Format.nBlockAlign = (WORD)(WaveFormat.Format.nChannels * WaveFormat.Format.wBitsPerSample / 8);
    WaveFormat.Format.nAvgBytesPerSec = WaveFormat.Format.nSamplesPerSec * WaveFormat.Format.nBlockAlign;
    WaveFormat.Samples.wValidBitsPerSample = 16;
    WaveFormat.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    WaveFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    HRESULT FormatSupported = GlobalSoundClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &WaveFormat.Format, &PointerClosest);

    REFERENCE_TIME BufferDuration = 10000000ULL * BufferRequestedAFrames / SamplesPerSecond; // buffer size in 100 nanoseconds
    HRESULT InitializeResult = GlobalSoundClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 
                    AUDCLNT_STREAMFLAGS_NOPERSIST, BufferDuration, 0,
                    &WaveFormat.Format, nullptr);
    if (FAILED(InitializeResult)) {
        switch(InitializeResult) {
            case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED: {
                Assert(!"WASAPI Error");
            } break;
            case AUDCLNT_E_BUFFER_SIZE_ERROR: {
                Assert(!"WASAPI Error");
            } break;
            case AUDCLNT_E_DEVICE_IN_USE: {
                Assert(!"WASAPI Error");
            } break;
            case AUDCLNT_E_UNSUPPORTED_FORMAT: {
                Assert(!"WASAPI Error");
            } break;
            default:
                Assert(!"WASAPI Error");
        }
    }

    if (FAILED(GlobalSoundClient->GetService(IID_PPV_ARGS(&GlobalSoundRenderClient)))) {
        Assert(!"WASAPI Error");
    }

    UINT32 SoundFrameCount;
    if (FAILED(GlobalSoundClient->GetBufferSize(&SoundFrameCount))) {
        Assert(!"WASAPI Error");
    }

    Assert(BufferRequestedAFrames <= (i32)SoundFrameCount);

}

LRESULT CALLBACK 
WindowCallback(
        HWND   MainWindow,
        UINT   Message,
        WPARAM wParam,
        LPARAM lParam)
{
    LRESULT Result = 0;
    switch(Message) {
        case WM_CREATE: 
            {
                Result = 0;
                OutputDebugStringA("WM_CREATE\n");
            } break;
        case WM_SIZE: 
            {
                OutputDebugStringA("WM_SIZE\n");
                HDC WindowDC = GetDC(MainWindow);
                RECT ClientRect;
                if (GetClientRect(MainWindow, &ClientRect)) {
                    i32 ClientW = ClientRect.right - ClientRect.left;
                    i32 ClientH = ClientRect.bottom - ClientRect.top;
                    u8 GrayLevel = 64;
                    SelectObject(WindowDC,GetStockObject(DC_BRUSH));
                    SetDCBrushColor(WindowDC,RGB(GrayLevel, GrayLevel, GrayLevel));
                    PatBlt(WindowDC, 0, 0, ClientW, ClientH, PATCOPY);

                    float MaxScaleX = (float)ClientW/(float)DEFAULT_BACKBUFFERW;
                    float MaxScaleY = (float)ClientH/(float)DEFAULT_BACKBUFFERH;
                    float Scale = (MaxScaleX < MaxScaleY ? MaxScaleX : MaxScaleY);

                    i32 NewWidth = (int)(Scale*DEFAULT_BACKBUFFERW);
                    i32 NewHeight = (int)(Scale*DEFAULT_BACKBUFFERH);
                    Win32ResizeBackbuffer(NewWidth, NewHeight);
                    win32_blit_params BlitParams = CalculateBlitParams(ClientRect);
                    GlobalBackbufferInWindowX = BlitParams.DestinationX;
                    GlobalBackbufferInWindowY = BlitParams.DestinationY;
                    if (!ReleaseDC(MainWindow, WindowDC)) {
                        OutputDebugStringA("Couldn't release DC");
                    }
                }
                else{
                    OutputDebugStringA("Couldn't get client rect");
                }

                Result = 0;
            } break;
        case WM_DESTROY:
        case WM_QUIT: 
            {
                OutputDebugStringA("WM_DESTROY or WM_QUIT\n");
                GlobalRunning = false;
                Result = 0;
            } break;
        case WM_PAINT: 
            {
                OutputDebugStringA("WM_PAINT\n");
                PAINTSTRUCT Paint;
                HDC DC = BeginPaint(MainWindow, &Paint);
                DrawBackbufferToWindow(MainWindow, DC);
                EndPaint(MainWindow, &Paint);
                Result = 0;
            } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: 
            {
                Assert(!"Keyboard message outside message loop!");
            } break;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEMOVE:
            {
                Assert(!"Mouse message outside message loop!");
            } break;
        default: 
            {
                Result = DefWindowProcA(MainWindow, Message, wParam, lParam);
            } break;
    }
    return Result;
}

internal float 
GetSecondsElapsed(LARGE_INTEGER PreCounter, LARGE_INTEGER PostCounter)
{
    return ((float)(PostCounter.QuadPart - PreCounter.QuadPart)/
            (float)(GlobalPerfFrequency));
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    WNDCLASSEXA MainWindowClass = {};
    MainWindowClass.cbSize = sizeof(WNDCLASSEXA);
    MainWindowClass.style = CS_HREDRAW | CS_VREDRAW; 
    MainWindowClass.lpfnWndProc = WindowCallback;
    MainWindowClass.hInstance = Instance;
    // TODO: Fancy icon in MainWindowClass.hIcon / small icon
    MainWindowClass.hCursor = LoadCursorA(NULL, IDC_CROSS);
    static char* MainWindowClassName = "Nick's Game Class";
    static char* MainWindowName = "Schmasteroids";
    MainWindowClass.lpszClassName = MainWindowClassName;

    ATOM RegisterClassResult = RegisterClassExA(&MainWindowClass);
    if (RegisterClassResult) {
        OutputDebugStringA("Window class registered\n");
        DWORD ExtendedStyle = WS_EX_APPWINDOW;
        DWORD Style = WS_VISIBLE | WS_OVERLAPPEDWINDOW;
        HMENU hMenu = 0;

        // Get Window rect according to Backbuffer size
        GlobalBackbufferBitmapInfo = {};
        GlobalBackbuffer = {};
        // TODO: Detect size of the monitor we're on and try to center the window and make it a reasonable size?
        Win32ResizeBackbuffer(DEFAULT_BACKBUFFERW, DEFAULT_BACKBUFFERH);
        GlobalBackbufferBitmapInfo.bmiHeader.biSize = sizeof(GlobalBackbufferBitmapInfo.bmiHeader);
        GlobalBackbufferBitmapInfo.bmiHeader.biWidth = GlobalBackbuffer.Width;
        GlobalBackbufferBitmapInfo.bmiHeader.biHeight = -GlobalBackbuffer.Height;
        GlobalBackbufferBitmapInfo.bmiHeader.biPlanes = 1;
        GlobalBackbufferBitmapInfo.bmiHeader.biBitCount = 32;
        GlobalBackbufferBitmapInfo.bmiHeader.biCompression = BI_RGB;
        int ClientRectX = 40;
        int ClientRectY = 40;
        RECT WindowRect;
        WindowRect.left = ClientRectX; 
        WindowRect.top = ClientRectY; 
        WindowRect.right = WindowRect.left + GlobalBackbuffer.Width;
        WindowRect.bottom = WindowRect.top + GlobalBackbuffer.Height;
        AdjustWindowRect(&WindowRect, Style, 0);
        i32 WindowStartWidth = WindowRect.right - WindowRect.left;
        i32 WindowStartHeight = WindowRect.bottom - WindowRect.top;

        GlobalMainWindow = CreateWindowExA(
                ExtendedStyle,
                MainWindowClassName,
                MainWindowName,
                Style,
                WindowRect.left,
                WindowRect.top,
                WindowStartWidth,
                WindowStartHeight,
                0,
                hMenu,
                Instance,
                0);
        if (GlobalMainWindow) {
            OutputDebugStringA("Window created\n");
            MSG Message;
            GlobalRunning = true;

            // Initialize sound
            i32 SecondaryBufferSizeAFrames = GlobalSamplesPerSecond * 2;
            InitWASAPI(GlobalSamplesPerSecond, SecondaryBufferSizeAFrames);
            bool32 SoundClientStarted = false;

            // Initialize clocks
            LARGE_INTEGER PerformanceFrequency;
            QueryPerformanceFrequency(&PerformanceFrequency);
            GlobalPerfFrequency = PerformanceFrequency.QuadPart;
            LARGE_INTEGER LastPreUpdateCounter;
            QueryPerformanceCounter(&LastPreUpdateCounter);
            MMRESULT TBPResult = timeBeginPeriod(1);
            GlobalSleepIsGranular = (TBPResult == TIMERR_NOERROR);
            // Initialize RNG
            srand((unsigned int)LastPreUpdateCounter.QuadPart);

            // Initialize input
            game_input GameInputs[2];
            for (int i=0;i<2;i++) {
                GameInputs[i] = {};
            }
            game_input *ThisInput = &GameInputs[0];
            game_input *LastInput = &GameInputs[1];
            Assert(&(ThisInput->Keyboard.Terminator) == &(ThisInput->Keyboard.Keys[ArrayCount(ThisInput->Keyboard.Keys)-1]));
#if DEBUG_BUILD
            Assert(&(ThisInput->DebugKeys.Terminator) == &(ThisInput->DebugKeys.Keys[ArrayCount(ThisInput->DebugKeys.Keys)-1]));
#endif

#if DEBUG_BUILD
            LPVOID GameMemoryBase = (LPVOID)Terabytes(2);
#else
            LPVOID GameMemoryBase = 0;
#endif
            win32_state WinState = {};
            WinState.GameMemory.PermanentStorageSize = PERMANENT_STORE_SIZE;
            WinState.GameMemory.TransientStorageSize = TRANSIENT_STORE_SIZE;
            size_t CombinedMemorySize = (size_t)(WinState.GameMemory.PermanentStorageSize +
                    WinState.GameMemory.TransientStorageSize);
            WinState.GameMemory.PermanentStorage = VirtualAlloc(GameMemoryBase, CombinedMemorySize, 
                    MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            WinState.GameMemory.TransientStorage = (char*)WinState.GameMemory.PermanentStorage +
                WinState.GameMemory.PermanentStorageSize;

            // Create pointers to platform services
            WinState.GameMemory.PlatformLoadWav = &Win32LoadWav;
            WinState.GameMemory.PlatformGetWavLoadInfo = &Win32GetWavLoadInfo;
            WinState.GameMemory.PlatformDebugReadFile = &Win32DebugReadFile;
            WinState.GameMemory.PlatformDebugWriteFile = &Win32DebugWriteFile;
            WinState.GameMemory.PlatformDebugSaveFramebufferAsBMP = &Win32DebugSaveFramebufferAsBMP;
            WinState.GameMemory.PlatformQuit = &Win32Quit;
            WinState.GameMemory.PlatformToggleFullscreen = &Win32ToggleFullScreen;

            if (WinState.GameMemory.PermanentStorage) {
                // VirtualAlloc sets memory to 0 per MSDN
                WinState.GameMemory.IsSetToZero = true;

                // Get game code
                win32_exe_info EXEInfo = {};
                DWORD GMFNResult = GetModuleFileNameA(0, EXEInfo.EXEFilename, MAX_PATH);
                if (GMFNResult != MAX_PATH) {
                    // TODO: Deal with longer paths or just don't have this code in user builds
                    char* LastBackslash = strrchr(EXEInfo.EXEFilename, '\\');
                    if (LastBackslash != NULL) {
                        i64 DirLength = LastBackslash - EXEInfo.EXEFilename + 1;
                        Assert(DirLength < 65535);
                        WinState.BuildDirPathLen = (u16)DirLength;
                        strncat_s(WinState.BuildDirPath, MAX_PATH, EXEInfo.EXEFilename, WinState.BuildDirPathLen);
                        strncat_s(EXEInfo.SourceGameDLLFullPath, 
                                MAX_PATH, WinState.BuildDirPath, WinState.BuildDirPathLen);
                        strncat_s(EXEInfo.TempGameDLLFullPath, 
                                MAX_PATH, WinState.BuildDirPath, WinState.BuildDirPathLen);
                        strncat_s(EXEInfo.GameCodeLockFile, 
                                MAX_PATH, WinState.BuildDirPath, WinState.BuildDirPathLen);
                    }
                    strcat_s(EXEInfo.SourceGameDLLFullPath, MAX_PATH-WinState.BuildDirPathLen, "schmasteroids_main.dll");
                    strcat_s(EXEInfo.TempGameDLLFullPath, MAX_PATH-WinState.BuildDirPathLen, "schmasteroids_main-temp.dll");
                    strcat_s(EXEInfo.GameCodeLockFile, MAX_PATH-WinState.BuildDirPathLen, "lock.tmp");
                    win32_game_code Game = GetGameCode(&EXEInfo);

                    if(Game.IsValid) {
                        Game.Initialize(&GlobalBackbuffer, &WinState.GameMemory);
                    }
                    float TargetSecondsPerFrame = 1.0f / (float)TARGETFPS;
#if RENDER_BENCHMARK
                    Win32ToggleFullScreen();
#endif
#if RUN_GAME_BENCHMARK
                    Win32StartBenchmarkPlayback(&WinState);
#endif

                    while(GlobalRunning) {
                        FILETIME DLLWriteTime = Win32GetLastWriteTime(EXEInfo.SourceGameDLLFullPath);
                        if(CompareFileTime(&DLLWriteTime, &EXEInfo.LastSourceDLLWriteTime)) {
                            ReleaseGameCode(&Game);
                            Game = GetGameCode(&EXEInfo);
                        }

                        //NOTE: For now we peek at all messages. Possibly filter in the future.

                        for(int KeyIndex = 0; KeyIndex < ArrayCount(ThisInput->Keyboard.Keys); KeyIndex++) {
                            ThisInput->Keyboard.Keys[KeyIndex].IsDown = 
                                ThisInput->Keyboard.Keys[KeyIndex].WasDown =
                                LastInput->Keyboard.Keys[KeyIndex].IsDown;
                        }
#if DEBUG_BUILD
                        for(int KeyIndex = 0; KeyIndex < ArrayCount(ThisInput->DebugKeys.Keys); KeyIndex++) {
                            ThisInput->DebugKeys.Keys[KeyIndex].IsDown = 
                                ThisInput->DebugKeys.Keys[KeyIndex].WasDown =
                                LastInput->DebugKeys.Keys[KeyIndex].IsDown;
                        }
#endif
                        for(int ButtonIndex = 0; ButtonIndex < ArrayCount(ThisInput->Mouse.Buttons); ButtonIndex++)
                        {
                            ThisInput->Mouse.Buttons[ButtonIndex].IsDown = 
                                ThisInput->Mouse.Buttons[ButtonIndex].WasDown =
                                LastInput->Mouse.Buttons[ButtonIndex].IsDown;
                        }
                        ThisInput->Mouse.WindowX = ThisInput->Mouse.LastWindowX = LastInput->Mouse.WindowX;
                        ThisInput->Mouse.WindowY = ThisInput->Mouse.LastWindowY = LastInput->Mouse.WindowY;

                        while (PeekMessageA(&Message, GlobalMainWindow,0,0, PM_REMOVE)) {

                            WPARAM c = Message.wParam;
                            switch(Message.message) {
                                case WM_SYSKEYUP:
                                    if (c == VK_F4) {
                                        GlobalRunning = false;
                                    }
                                case WM_KEYUP:
                                    if (c == VK_SHIFT) {
                                        HandleKeyboardMessage(Message, &ThisInput->Keyboard.Shift);
                                    } else
#if DEBUG_BUILD
                                    if (c == VK_F1) {
                                        // Reset
                                        memset(WinState.GameMemory.PermanentStorage, 
                                                0, WinState.GameMemory.PermanentStorageSize);
                                        WinState.GameMemory.IsSetToZero = true;
                                        if(Game.IsValid) {
                                            Game.Initialize(&GlobalBackbuffer, &WinState.GameMemory);
                                        }
                                    } else if (c == VK_F2) {
                                        // Hard pause
                                        GlobalDebugPause = !GlobalDebugPause;
                                    } else if (c == VK_F3) {
                                        // Game state Recording and playback
                                        // NOTE: For now, let's start recording, start playback, end playback
                                        // all on F3. We can make it more flexible later with same functions.
                                        // NOTE: Multiple recorded loops can be easily added here if we need them
                                        if(WinState.GameRecordIndex) {
                                            Win32EndRecording(&WinState);
                                            Win32StartPlayback(&WinState, Game, 1);
                                        } else if (WinState.GamePlaybackIndex) {
                                            Win32EndPlayback(&WinState);
                                        }
                                        else {
                                            Win32StartRecording(&WinState, Game, 1);
                                        }
                                    } else if (c == VK_F4) {
                                        // Toggle fullscreen a la Raymond Chen
                                        // https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
                                        Style = GetWindowLong(GlobalMainWindow, GWL_STYLE);
                                        if (Style & WS_OVERLAPPEDWINDOW) {
                                            MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
                                            if (GetWindowPlacement(GlobalMainWindow, &GlobalPrevWindowPos) &&
                                                    GetMonitorInfo(MonitorFromWindow(GlobalMainWindow,
                                                            MONITOR_DEFAULTTOPRIMARY), &MonitorInfo)) {
                                                SetWindowLong(GlobalMainWindow, GWL_STYLE,
                                                        Style & ~WS_OVERLAPPEDWINDOW);
                                                SetWindowPos(GlobalMainWindow, HWND_TOP,
                                                        MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                                                        MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                                                        MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                                                        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                                            }
                                        } else {
                                            SetWindowLong(GlobalMainWindow, GWL_STYLE,
                                                    Style | WS_OVERLAPPEDWINDOW);
                                            SetWindowPlacement(GlobalMainWindow, &GlobalPrevWindowPos);
                                            SetWindowPos(GlobalMainWindow, NULL, 0, 0, 0, 0,
                                                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                                                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                                        }
                                    } else if (c == VK_F5) {
                                        // Screenshot
                                        SYSTEMTIME Time;
                                        GetSystemTime(&Time);
                                        char Filename[64];
                                        sprintf_s(Filename, 64, "screenshot_%d_%d_%d-%d_%d_%02d_%03d.bmp",
                                                  Time.wYear, Time.wMonth, Time.wDay,
                                                  Time.wHour, Time.wMinute, Time.wSecond, Time.wMilliseconds);
                                        Win32DebugSaveFramebufferAsBMP(Filename);
                                    } 
#endif

                                case WM_SYSKEYDOWN:
                                case WM_KEYDOWN:
                                    {
                                        if (c == VK_UP) {
                                            HandleKeyboardMessage(Message, &ThisInput->Keyboard.MoveUp);
                                        } else if (c == VK_DOWN) {
                                            HandleKeyboardMessage(Message, &ThisInput->Keyboard.MoveDown);
                                        } else if (c == VK_LEFT) {
                                            HandleKeyboardMessage(Message, &ThisInput->Keyboard.MoveLeft);
                                        } else if (c == VK_RIGHT) {
                                            HandleKeyboardMessage(Message, &ThisInput->Keyboard.MoveRight);
                                        } else if (c == VK_SPACE) {
                                            HandleKeyboardMessage(Message, &ThisInput->Keyboard.Fire);
                                        } else if (c == VK_SHIFT) {
                                            HandleKeyboardMessage(Message, &ThisInput->Keyboard.Shift);
                                        } 
#if DEBUG_BUILD
                                        else if (c == VK_F6) {
                                            HandleKeyboardMessage(Message, &ThisInput->DebugKeys.F6);
                                        } else if (c == VK_F7) {
                                            HandleKeyboardMessage(Message, &ThisInput->DebugKeys.F7);
                                        } else if (c == VK_F8) {
                                            HandleKeyboardMessage(Message, &ThisInput->DebugKeys.F8);
                                        } else if (c == VK_F9) {
                                            HandleKeyboardMessage(Message, &ThisInput->DebugKeys.F9);
                                        } else if (c == VK_F10) {
                                            HandleKeyboardMessage(Message, &ThisInput->DebugKeys.F10);
                                        } else if (c == VK_F11) {
                                            HandleKeyboardMessage(Message, &ThisInput->DebugKeys.F11);
                                        } else if (c == VK_F12) {
                                            HandleKeyboardMessage(Message, &ThisInput->DebugKeys.F12);
                                        }
#endif
                                    } break;
                                case WM_LBUTTONDOWN:
                                    {
                                        ThisInput->Mouse.LButton.IsDown = true;
                                    } break;
                                case WM_LBUTTONUP:
                                    {
                                        ThisInput->Mouse.LButton.IsDown = false;
                                    } break;
                                case WM_RBUTTONDOWN:
                                    {
                                        ThisInput->Mouse.RButton.IsDown = true;
                                    } break;
                                case WM_RBUTTONUP:
                                    {
                                        ThisInput->Mouse.RButton.IsDown = false;
                                    } break;
                                case WM_MBUTTONDOWN:
                                    {
                                        ThisInput->Mouse.MButton.IsDown = true;
                                    } break;
                                case WM_MBUTTONUP:
                                    {
                                        ThisInput->Mouse.MButton.IsDown = false;
                                    } break;
                                case WM_MOUSEMOVE:
                                    {
                                        // NOTE: I'm trying to avoid bringing in windows.h. If I want to run in 
                                        // 32-bit windows this needs to be tested.
                                        short ShortX = (short) Message.lParam;
                                        short ShortY = (short) (Message.lParam >> BITS_IN_SHORT);
                                        ThisInput->Mouse.WindowX = ((i32)ShortX - GlobalBackbufferInWindowX);
                                        ThisInput->Mouse.WindowY = ((i32)ShortY - GlobalBackbufferInWindowY);
                                    } break;
                                default:
                                    {
                                        // NOTE: Maybe we don't need to call TranslateMessage? It sounds like
                                        // it is only for generating "character messages", which i don't think
                                        // we care about as long as we're getting KeyUp and KeyDown messages.
                                        // On the other hand, we might want character input if we're naming our spaceship
                                        // and things like that.
                                        TranslateMessage(&Message);
                                        DispatchMessageA(&Message);
                                    }
                            }
                        }

                        LARGE_INTEGER PreUpdateCounter;
                        QueryPerformanceCounter(&PreUpdateCounter);
                        // NOTE: For now timestep is based on desired framerate. If we have problems
                        // then we can make it predictive.
                        ThisInput->SecondsElapsed = TargetSecondsPerFrame;
#if DEBUG_BUILD
                        if(!GlobalDebugPause)
                        {
#endif
                            if (WinState.GamePlaybackIndex) {
                                Win32PlaybackInput(&WinState, Game, ThisInput);
                            } else if (WinState.GameRecordIndex) {
                                Win32RecordInput(&WinState, ThisInput);
                            }
                            // TODO: Try polling the mouse position here and see if it makes the cursor 
                            // lag less with respect to OS cursor
                            if (Game.IsValid) {
                                Game.UpdateAndRender(&GlobalBackbuffer, ThisInput, &WinState.GameMemory);
                            }
#if DEBUG_BUILD
                        }
#endif

                        game_input* tempInput = ThisInput;
                        ThisInput = LastInput;
                        LastInput = tempInput;

                        // Draw from backbuffer
                        HDC WindowDC = GetDC(GlobalMainWindow);
                        if (WindowDC) {
                            DrawBackbufferToWindow(GlobalMainWindow, WindowDC);
                            if (!ReleaseDC(GlobalMainWindow, WindowDC)) {
                                OutputDebugStringA("Failed to release window DC\n");
                            }
                        } else {
                            OutputDebugStringA("Couldn't get Window DC\n");
                        }

                        // NOTE: SoundPaddingSize gets returned in audio frames
                        u32 SoundPaddingSize;
                        u32 DesiredPaddingSize = 2 * (u32)((float)GlobalSamplesPerSecond * TargetSecondsPerFrame);
                        if(SUCCEEDED(GlobalSoundClient->GetCurrentPadding(&SoundPaddingSize))) {
                            u32 AFramesToWrite = DesiredPaddingSize - SoundPaddingSize;
                            BYTE* SoundBufferData;
                            if (SUCCEEDED(GlobalSoundRenderClient->GetBuffer(AFramesToWrite, &SoundBufferData))) {
                                // NOTE: Is it safe to pass the pointer straight to the game? Don't see why not at
                                // this point
                                platform_sound_output_buffer SoundFromGame = {};
                                SoundFromGame.AFramesPerSecond = GlobalSamplesPerSecond;
                                SoundFromGame.AFramesToWrite = AFramesToWrite;
                                SoundFromGame.SampleData = (i16*)SoundBufferData;
                                DWORD WASAPIFlags = 0;
#if DEBUG_BUILD
                                if(!GlobalDebugPause)
                                {
#endif
                                    if (Game.IsValid) {
                                        Game.GetSoundOutput(&SoundFromGame, &WinState.GameMemory);
                                    }
#if DEBUG_BUILD
                                }  else {
                                    WASAPIFlags = AUDCLNT_BUFFERFLAGS_SILENT;
                                }
#endif

                                // NOTE: If we want to silence sound, it would be easy to do it here by setting
                                // second parameter to AUDCLNT_BUFFERFLAGS_SILENT.
                                GlobalSoundRenderClient->ReleaseBuffer(AFramesToWrite, WASAPIFlags);
                            } else {
                                OutputDebugStringA("Couldn't get buffer\n");
                                Assert(!"WASAPI error");
                            }

                        } else {
                            OutputDebugStringA("WASAPI couldn't get padding\n");
                            //ERROR checking?
                        }

                        if(!SoundClientStarted) {
                            GlobalSoundClient->Start();
                            SoundClientStarted = true;
                        }

                        LARGE_INTEGER LoopEndCounter;
                        QueryPerformanceCounter(&LoopEndCounter);
                        float SecondsInUpdate = GetSecondsElapsed(PreUpdateCounter, LoopEndCounter);

                        if (GlobalSleepIsGranular) {
                            if (SecondsInUpdate < TargetSecondsPerFrame) {
                                int SleepMS = (int)((TargetSecondsPerFrame - SecondsInUpdate) * 1000.0f);
                                /*
                                   sprintf(DebugCharBuffer, "Update seconds: %.3f Sleep milliseconds: %d\n",
                                   SecondsInUpdate, SleepMS);
                                   OutputDebugStringA(DebugCharBuffer);
                                   */
                                Sleep(SleepMS);
                            }
                        }

                        LastPreUpdateCounter = PreUpdateCounter;
                    }
                }
                else {
                    // Filename too long
                }
            } else {
                // Not enough memory
            }
        }
        else {
            // Couldn't create window
            // GetLastError?
        }
    }
    else {
        // Registration failed
        // Call GetLastError for info?
        // TODO: Error out with a messagebox?
    }
    return 0;
}
