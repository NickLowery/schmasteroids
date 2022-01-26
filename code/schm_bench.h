#ifndef SCHM_BENCH_H
#define SCHM_BENCH_H 1

#if BENCHMARK
static u32 GlobalFrameCount;
static u32 GlobalFramesToRender;
static constexpr size_t GlobalLogLineBufferSize = 512;
static char GlobalLogLineBuffer[GlobalLogLineBufferSize];
static char RenderBenchmarkLog[Kilobytes(128)];
static char *RenderBenchmarkLogCursor = RenderBenchmarkLog;
static char RenderBenchmarkLogCSV[Kilobytes(128)];
static char *RenderBenchmarkLogCSVCursor = RenderBenchmarkLogCSV;

inline void WriteToBenchmarkLog(char* String)
{
    i32 Length = StringLength(String);
    Assert(RenderBenchmarkLogCursor + Length < RenderBenchmarkLog + ArrayCount(RenderBenchmarkLog));
    while(*String != 0) {
        *RenderBenchmarkLogCursor++ = *String++;
    }
    *RenderBenchmarkLogCursor = 0;
}

#define BENCH_LOG(...) \
    sprintf_s(GlobalLogLineBuffer, GlobalLogLineBufferSize, __VA_ARGS__); \
    WriteToBenchmarkLog(GlobalLogLineBuffer);

#define BENCH_DEFINE_CYCLE_COUNT(Name) \
    static u64 RunningTotalCycles##Name = 0; \
    static u64 RunningTotalCount##Name = 0; \
    static u64 FrameCycles##Name = 0; \
    static u64 FrameCount##Name = 0; \
    static u64 MaxFrameCycles##Name = 0; \
    static u64 MinFrameCycles##Name = UINT64_MAX; \
    static u64 MaxFrameCount##Name = 0; \
    static u64 MinFrameCount##Name = UINT64_MAX; \

#define BENCH_DEFINE_CYCLE_USECOND_COUNT(Name) \
    BENCH_DEFINE_CYCLE_COUNT(Name) \
    static u32 RunningTotalUSeconds##Name = 0; \
    static u32 FrameUSeconds##Name = 0; \
    static u32 MaxFrameUSeconds##Name = 0; \
    static u32 MinFrameUSeconds##Name = UINT32_MAX;

#define BENCH_SCOPE_CYCLE_COUNT(Name) \
    u64 StartCycles##Name;\
    u64 FinishCycles##Name;


// All the game benchmark-specific stuff goes here.


#if BENCHMARK
#define BENCH_DO_ALL_CYCLES_AND_USECONDS(MacroName) \
    MacroName(DrawGame) \
    MacroName(UpdateGame) \

#define BENCH_DO_ALL_CYCLES(MacroName) \
    MacroName(SwizzleFrameBuffer) \
    MacroName(PlaySound) \

#define BENCH_DO_ALL(MacroName) \
    BENCH_DO_ALL_CYCLES(MacroName) \
    BENCH_DO_ALL_CYCLES_AND_USECONDS(MacroName)


BENCH_DO_ALL_CYCLES(BENCH_DEFINE_CYCLE_COUNT)
BENCH_DO_ALL_CYCLES_AND_USECONDS(BENCH_DEFINE_CYCLE_USECOND_COUNT)
BENCH_DO_ALL(BENCH_SCOPE_CYCLE_COUNT)
#endif


// NOTE: I don't think there's any reason not to scope all these things globally? Maybe I'll find
// out I'm wrong at some point.


#define BENCH_START_FRAME_CYCLE_COUNT(Name) \
    FrameCycles##Name = 0; \
    FrameCount##Name = 0; 
    
#define BENCH_START_FRAME_CYCLE_USECOND_COUNT(Name) \
    BENCH_START_FRAME_CYCLE_COUNT(Name) \
    FrameUSeconds##Name = 0; \

#define BENCH_START_FRAME_ALL \
    BENCH_DO_ALL_CYCLES_AND_USECONDS(BENCH_START_FRAME_CYCLE_USECOND_COUNT) \
    BENCH_DO_ALL_CYCLES(BENCH_START_FRAME_CYCLE_COUNT)


#define BENCH_START_COUNTING_CYCLES(Name) \
    ++FrameCount##Name; \
    StartCycles##Name = __rdtsc(); \

#define BENCH_START_COUNTING_CYCLES_INC(Name, Inc) \
    FrameCount##Name += Inc; \
    StartCycles##Name = __rdtsc(); \

#define BENCH_START_COUNTING_CYCLES_NO_INC(Name) \
    StartCycles##Name = __rdtsc(); \

#define BENCH_STOP_COUNTING_CYCLES(Name) \
    FinishCycles##Name = __rdtsc();\
    FrameCycles##Name += FinishCycles##Name - StartCycles##Name;

#define BENCH_START_COUNTING_CYCLES_USECONDS(Name) \
    BENCH_START_COUNTING_CYCLES(Name) \
    LARGE_INTEGER StartWallClock##Name; \
    QueryPerformanceCounter(&StartWallClock##Name);

#define BENCH_START_COUNTING_CYCLES_USECONDS_NO_INC(Name) \
    BENCH_START_COUNTING_CYCLES_NO_INC(Name) \
    LARGE_INTEGER StartWallClock##Name; \
    QueryPerformanceCounter(&StartWallClock##Name);

#define BENCH_STOP_COUNTING_CYCLES_USECONDS(Name) \
    BENCH_STOP_COUNTING_CYCLES(Name) \
    LARGE_INTEGER FinishWallClock##Name; \
    QueryPerformanceCounter(&FinishWallClock##Name); \
    { \
        float Seconds = (float)(FinishWallClock##Name.QuadPart - StartWallClock##Name.QuadPart)/(float)DEBUGPerfFrequency; \
        u32 Microseconds = (u32)(Seconds * 1000000.0f); \
        FrameUSeconds##Name += Microseconds; \
    }

#define BENCH_UPDATE_STATS(Name)  \
        if(Frame##Name > MaxFrame##Name) { \
            MaxFrame##Name = Frame##Name; \
        } \
        if (Frame##Name < MinFrame##Name) { \
            MinFrame##Name = Frame##Name; \
        } \
        RunningTotal##Name += Frame##Name;

#define BENCH_FINISH_FRAME_CYCLE_COUNT(Name) \
    BENCH_UPDATE_STATS(Cycles##Name) \
    BENCH_UPDATE_STATS(Count##Name) \
    BENCH_LOG("  Cycles In " #Name ": %I64u\n", FrameCycles##Name) \
    BENCH_LOG("  Count " #Name ": %I64u\n", FrameCount##Name) \
    if (FrameCount##Name > 0) { \
    BENCH_LOG("  Cycles Per " #Name ": %I64u\n", FrameCycles##Name/FrameCount##Name)  \
    }
    
#define BENCH_FINISH_FRAME_CYCLE_USECOND_COUNT(Name) \
    BENCH_FINISH_FRAME_CYCLE_COUNT(Name) \
    BENCH_UPDATE_STATS(USeconds##Name) \
    BENCH_LOG("  uSeconds In " #Name ": %I32u\n", FrameUSeconds##Name) 

#define BENCH_CALCULATE_FRAME_AVG(Name) \
    float AvgFrame##Name = (float)RunningTotal##Name / (float)GlobalFrameCount; \

#define BENCH_FINISH_CYCLE_COUNT(Name) \
    BENCH_CALCULATE_FRAME_AVG(Cycles##Name) \
    BENCH_CALCULATE_FRAME_AVG(Count##Name) \
    u64 AvgCyclesPer##Name = (RunningTotalCount##Name > 0) ? \
        RunningTotalCycles##Name/RunningTotalCount##Name : -1; \
    BENCH_LOG(#Name " Stats:\n" \
                 "  Min Frame Cycles:       %12I64u\n" \
                 "  Max Frame Cycles:       %12I64u\n" \
                 "  Avg Frame Cycles:       %f\n" \
                 "  Overall Avg Cycles Per: %12I64u\n" \
                 "  Min Count Per Frame:    %12I64u\n" \
                 "  Max Count Per Frame:    %12I64u\n" \
                 "  Avg Count Per Frame:    %f\n", \
                 MinFrameCycles##Name, MaxFrameCycles##Name, AvgFrameCycles##Name, \
                 AvgCyclesPer##Name, \
                 MinFrameCount##Name, MaxFrameCount##Name, AvgFrameCount##Name) 

#define BENCH_FINISH_CYCLE_USECOND_COUNT(Name) \
    BENCH_CALCULATE_FRAME_AVG(USeconds##Name) \
    BENCH_FINISH_CYCLE_COUNT(Name) \
    BENCH_LOG("  Min uSeconds Per Frame:    %12I32u\n" \
                 "  Max uSeconds Per Frame:    %12I32u\n" \
                 "  Avg uSeconds Per Frame:    %f\n", \
                 MinFrameUSeconds##Name, MaxFrameUSeconds##Name, AvgFrameUSeconds##Name)
    

#define BENCH_FINISH_FRAME_ALL \
    ++GlobalFrameCount; \
    BENCH_LOG("Frame %d:\n", GlobalFrameCount) \
    BENCH_DO_ALL_CYCLES(BENCH_FINISH_FRAME_CYCLE_COUNT) \
    BENCH_DO_ALL_CYCLES_AND_USECONDS(BENCH_FINISH_FRAME_CYCLE_USECOND_COUNT) 
        
#define BENCH_FINISH_ALL \
    BENCH_DO_ALL_CYCLES(BENCH_FINISH_CYCLE_COUNT) \
    BENCH_DO_ALL_CYCLES_AND_USECONDS(BENCH_FINISH_CYCLE_USECOND_COUNT) 

#define BENCH_DUMP_FRAME_BMP \
    sprintf_s(GlobalLogLineBuffer, GlobalLogLineBufferSize, "frame_dumps\\frame%02d.bmp", GlobalFrameCount); \
    GameMemory->PlatformDebugSaveFramebufferAsBMP(GlobalLogLineBuffer);

#define BENCH_WRITE_LOG_TO_FILE(BENCHFilename) \
            u32 BENCHFileSize = (u32)(RenderBenchmarkLogCursor - RenderBenchmarkLog); \
            GameMemory->PlatformDebugWriteFile(BENCHFilename, (void*)RenderBenchmarkLog, BENCHFileSize);
#define BENCH_WRITE_CSV_TO_FILE(BENCHCSVFilename) \
            u32 BENCHCSVFileSize = (u32)(RenderBenchmarkLogCSVCursor - RenderBenchmarkLogCSV); \
            GameMemory->PlatformDebugWriteFile(BENCHCSVFilename, (void*)RenderBenchmarkLogCSV, BENCHCSVFileSize);


#else
#define BENCH_LOG(...) 
#define BENCH_DEFINE_CYCLE_COUNT(Name) 
#define BENCH_DEFINE_CYCLE_USECOND_COUNT(Name)
#define BENCH_START_FRAME_CYCLE_COUNT(Name)
#define BENCH_START_FRAME_CYCLE_USECOND_COUNT(Name)
#define BENCH_START_FRAME_ALL
#define BENCH_START_COUNTING_CYCLES(Name)
#define BENCH_STOP_COUNTING_CYCLES(Name)
#define BENCH_START_COUNTING_CYCLES_USECONDS(Name)
#define BENCH_STOP_COUNTING_CYCLES_USECONDS(Name)
#define BENCH_UPDATE_STATS(Name) 
#define BENCH_FINISH_FRAME_CYCLE_COUNT(Name)
#define BENCH_CALCULATE_FRAME_AVG(Name)
#define BENCH_FINISH_CYCLE_COUNT(Name)
#define BENCH_FINISH_CYCLE_USECOND_COUNT(Name)
#define BENCH_FINISH_FRAME_ALL
#define BENCH_DUMP_FRAME_BMP
#define BENCH_SCOPE_CYCLECOUNT(Name)
#define BENCH_START_COUNTING_CYCLES_INC(Name, Inc) 
#define BENCH_START_COUNTING_CYCLES_NO_INC(Name) 
#define BENCH_WRITE_LOG_TO_FILE(BENCHFilename) 
#define BENCH_START_COUNTING_CYCLES_USECONDS_NO_INC(Name) 
#endif

#endif
