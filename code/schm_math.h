#ifndef SCHMASTEROIDS_MATH_H

// NOTE: Only works if A and B have no side effects!
#define Min(A,B) (((A) < (B))?(A):(B))
#define Max(A,B) (((A) > (B))?(A):(B))

inline float 
Lerp(float Start, float T, float Finish)
{
    float Result = (1.0f-T)*Start + T*Finish;
    return Result;
}

inline i32
RoundUpToMultipleOf(u32 Divisor, i32 Value)
{
    i32 Mod = Value % Divisor;
    i32 Result = (Mod != 0)? Value + Divisor - Mod: Value;
    return Result;
}

inline i32 
RoundDownToMultipleOf(u32 Divisor, i32 Value)
{
    i32 Mod = Value % Divisor;
    i32 Result = Value - Mod;
    return Result;
}

inline float 
NormalizeToRange(float Min, float Val, float Max)
{
    Assert(Min < Max);
    // NOTE: See if there are precision problems with this?
    float Result = (Val - Min)/(Max - Min);
    return Result;
}

constexpr inline float 
Pow(float Base, u32 Exp)
{
    float Result = 1.0f;
    for (u32 i = 0;
         i < Exp;
         ++i)
    {
        Result *= Base;
    }
    return Result;
}

inline float
Square(float Value)
{
    return Value * Value;
}

inline float
Sqrt(float Value)
{
    return sqrtf(Value);
}

internal inline int
RoundToInt(float F);

inline i32
Floor(float F);

typedef struct _v2 {
    union {
        float coords[2];
        struct {
            float X;
            float Y;
        };
    };
} v2;

inline bool32 operator==(v2 &A, v2 &B)
{
    bool32 Result = ((A.X == B.X) && (A.Y == B.Y));
    return Result;
}

inline v2 operator+(v2 A, v2 B) 
{
    v2 Result = {};
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    return Result;
}

inline v2 & operator+=(v2 &A, v2 B)
{
    A = A + B;
    return A;
}

inline v2 operator-(v2 A, v2 B) 
{
    v2 Result = {};
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    return Result;
}

inline v2 & operator-=(v2 &A, v2 B)
{
    A = A - B;
    return A;
}

inline v2 operator*(float A, v2 B)
{
    v2 Result = {};
    Result.X = A * B.X;
    Result.Y = A * B.Y;
    return Result;
}

inline v2 operator*(v2 A, float B)
{
    return B * A;
}

inline v2 & operator*=(v2 &A, float B)
{
    A = A * B;
    return A;
}

inline v2 operator/(v2 A, float B)
{
    v2 Result = {};
    Result.X = A.X / B;
    Result.Y = A.Y / B;
    return Result;
}

inline v2 & operator/=(v2 &A, float B)
{
    A = A / B;
    return A;
}

inline v2
V2(float X, float Y)
{
    v2 Result = {X,Y};
    return Result;
}

inline v2
XOnly(v2 Val)
{
    v2 Result;
    Result.X = Val.X;
    Result.Y = 0.0f;
    return Result;
}

inline v2
YOnly(v2 Val)
{
    v2 Result;
    Result.X = 0.0f;
    Result.Y = Val.Y;
    return Result;
}

inline v2
MinX(v2 A, v2 B)
{
    if (B.X < A.X)
    {
        return B;
    } else {
        return A;
    }
}

inline v2
MinY(v2 A, v2 B)
{
    if (B.Y < A.Y)
    {
        return B;
    } else {
        return A;
    }
}

typedef union {
    struct {
    float Left;
    float Top;
    float Right;
    float Bottom;
    };
    struct {
        v2 Min;
        v2 Max;
    };
} rect;

inline rect operator+(rect Rect, v2 Offset)
{
    rect Result;
    Result.Min = Rect.Min + Offset;
    Result.Max = Rect.Max + Offset;
    return Result;
}

inline rect & operator+=(rect &Rect, v2 Offset)
{
    Rect = Rect + Offset;
    return Rect;
}

inline v2 Dim(rect Rect) 
{
    v2 Result = V2(Rect.Right - Rect.Left, Rect.Bottom - Rect.Top);
    return Result;
}


typedef struct {
    v2 First, Second;
} line_seg;

struct clamp_result
{
    float Value;
    bool32 Clamped;
};

inline rect
ExpandRectToContainPoint(rect Rect, v2 Point)
{
    if (Point.X > Rect.Max.X) {Rect.Max.X = Point.X;}
    if (Point.Y > Rect.Max.Y) {Rect.Max.Y = Point.Y;}
    if (Point.X < Rect.Min.X) {Rect.Min.X = Point.X;}
    if (Point.Y < Rect.Min.Y) {Rect.Min.Y = Point.Y;}
    return Rect;
}

inline rect
ExpandRectByRadius(rect Rect, v2 Radius)
{
    Assert(Radius.X >= 0 && Radius.Y >= 0);
    Rect.Min -= Radius;
    Rect.Max += Radius;
    return Rect;
}


inline float 
Dot(v2 First, v2 Second) {
    float Result = (First.X * Second.X) + (First.Y * Second.Y);
    return Result;
}

inline float
LengthSq(v2 Test) {
    float Result = Dot(Test, Test);
    return Result;
}


inline float
Length(v2 Test) {
    float Result = Sqrt(LengthSq(Test));
    return Result;
}

inline v2
Rotate(v2 V, float Angle)
{
    v2 Result;
    float Sin = (float)sin(Angle);
    float Cos = (float)cos(Angle);
    Result.Y = (Sin * (V.X)) + (Cos * (V.Y));
    Result.X = (Cos * (V.X)) - (Sin * (V.Y));
    return Result;
}

inline v2
V2FromAngleAndMagnitude(float Angle, float Magnitude) 
{
    v2 Result = Rotate(V2(Magnitude, 0), Angle);
    return Result;
}


inline rect
RectFromCenterAndDimensions(v2 Center, v2 Dimensions);

inline rect
RectFromMinCornerAndDimensions(v2 MinCorner, v2 Dimensions);

internal bool32 
InRect(v2 V, rect Rect);

internal v2
ProjectNormalizedV2ToRect(v2 Norm, rect Rect);

inline float
Width(rect Rect) {
    float Result = Rect.Right - Rect.Left;
    return Result;
}

inline float
Height(rect Rect) {
    float Result = Rect.Bottom - Rect.Top;
    return Result;
}

inline v2 
Center(rect Rect) {
    v2 Result;
    Result.X = Lerp(Rect.Left, 0.5f, Rect.Right);
    Result.Y = Lerp(Rect.Top, 0.5f, Rect.Bottom);
    return Result;
}

inline float CenterY(rect Rect) 
{
    float Result = Lerp(Rect.Top, 0.5f, Rect.Bottom);
    return Result;
}

inline float CenterX(rect Rect) 
{
    float Result = Lerp(Rect.Left, 0.5f, Rect.Right);
    return Result;
}

inline v2 BottomLeft(rect Rect)
{
    v2 Result;
    Result.X = Rect.Left;
    Result.Y = Rect.Bottom;
    return Result;
}

inline v2 Lerp(v2 A, float T, v2 B)
{
    v2 Result = ((1.0f-T)*A) + (T*B);
    return Result;
}

internal v2
ScaleV2ToMagnitude(v2 OldV, float NewMagnitude);

internal v2
ClipEndPointToRect(v2 VIn, v2 VOut, rect Rect);

//Takes warping into account
internal v2 
ShortestPath(v2 From, v2 To);

internal float
GetAbsoluteDistance(v2 FirstV, v2 SecondV);


typedef struct {
    __m128 Val;
} float4;

// float4 operations
inline float4
Float4(float In)
{
    float4 Result;
    Result.Val = _mm_set_ps1(In);
    return Result;
}

inline float4
operator+(float4 A, float4 B)
{
   float4 Result;
   Result.Val = _mm_add_ps(A.Val, B.Val);
   return Result;
}

inline float4
operator-(float4 A, float4 B)
{
   float4 Result;
   Result.Val = _mm_sub_ps(A.Val, B.Val);
   return Result;
}

inline float4
operator*(float4 A, float4 B)
{
    float4 Result;
    Result.Val = _mm_mul_ps(A.Val, B.Val);
    return Result;
}

inline float4
operator/(float4 A, float4 B)
{
    float4 Result;
    Result.Val = _mm_div_ps(A.Val, B.Val);
    return Result;
}
inline float4
operator-(float4 In)
{
    float4 Result;
    Result.Val = _mm_xor_ps(In.Val, _mm_set1_ps(-0.f));
    return Result;
}

inline float4 Square(float4 Val) 
{
    float4 Result = Val * Val;
    return Result;
}

inline float4 Float4Max(float4 A, float4 B)
{
    float4 Result;
    Result.Val = _mm_max_ps(A.Val, B.Val);
    return Result;
}

// PRNG
// NOTE: We're using xoshiro128++ here. Simple xorshift would probably be 
// just fine. But I think this should be plenty fast and certainly high enough
// quality for our purposes. If it's slow do more testing.
typedef struct {
    uint32_t s[4];
} xoshiro128pp_state;

u64 SplitMixNext(u64 *State) 
{
    u64 z = (*State += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}


inline void SeedXoshiro128pp(xoshiro128pp_state *State, u64 Seed)
{
    State->s[0] = (uint32_t)SplitMixNext(&Seed);
    State->s[1] = (uint32_t)SplitMixNext(&Seed);
    State->s[2] = (uint32_t)SplitMixNext(&Seed);
    State->s[3] = (uint32_t)SplitMixNext(&Seed);
}

inline u32 Rotl32(u32 x, int k)
{
    u32 Result = (x << k) | (x >> (32 - k));
    return Result;
}

inline u32 Xoshiro128ppNext(xoshiro128pp_state *State) 
{
    u32 Result = Rotl32(State->s[0] + State->s[3], 7) + State->s[0];
    u32 t = State->s[1] << 9;
    State->s[2] ^= State->s[0];
	State->s[3] ^= State->s[1];
	State->s[1] ^= State->s[2];
	State->s[0] ^= State->s[3];

	State->s[2] ^= t;

	State->s[3] = Rotl32(State->s[3], 11);

	return Result;
}

inline u32 RandU32(xoshiro128pp_state *State)
{
#if 1
    u32 Result = Xoshiro128ppNext(State);
    return Result;
#else
    return 0;
#endif

}


/* Returns a pseudorandom float between 0 and 1 */
internal inline float
RandFloat01(xoshiro128pp_state *State)
{
    // NOTE: We could do this in a more robust way a la https://allendowney.com/research/rand/ but I don't think it's at all necessary for our purposes
    u32 U32Result = Xoshiro128ppNext(State);
    float Result = (float)(U32Result >> 8) * 0x1.0p-24f;
    Assert(Result >= 0);
    Assert(Result < 1.0f);
    return Result;
}

internal inline float
RandFloatRange(float Min, float Max, xoshiro128pp_state *State)
{
    return ((RandFloat01(State) * (Max - Min)) + Min);
}

internal inline float
RandHeading(xoshiro128pp_state *State)
{
    return (RandFloat01(State) * 2.0f * PI);
}
internal inline v2
RandV2InRadius(float R, xoshiro128pp_state *State) 
{
    return V2FromAngleAndMagnitude(RandHeading(State), 
            RandFloatRange(0.0f, R, State));
}

internal inline __m128
RandFloat01_4(xoshiro128pp_state *State)
{
    __m128 Result = _mm_set_ps(RandFloat01(State),  RandFloat01(State), RandFloat01(State),  RandFloat01(State));
    return Result;
}

internal inline __m128
RandHeading_4(xoshiro128pp_state *State)
{
    __m128 Result = _mm_mul_ps(RandFloat01_4(State), _mm_set_ps1(2.0f * PI));
    return Result;
}




// v2_4 operations
typedef struct {
    float4 X, Y;
} v2_4;

inline v2_4 operator-(v2_4 A, v2_4 B) 
{
    v2_4 Result = {};
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    return Result;
}

inline v2_4 operator+(v2_4 A, v2_4 B) 
{
    v2_4 Result = {};
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    return Result;
}

inline v2_4 operator*(float4 Scalar, v2_4 In)
{
    v2_4 Result;
    Result.X = Scalar * In.X;
    Result.Y = Scalar * In.Y;
    return Result;
}

inline v2_4
V2_4FromV2(v2 In)
{
    v2_4 Result;
    Result.X.Val = _mm_set_ps1(In.X);
    Result.Y.Val = _mm_set_ps1(In.Y);
    return Result;

}

inline v2_4
Perp(v2_4 In)
{
    v2_4 Result;
    Result.X = -In.Y;
    Result.Y = In.X;
    return Result;
}

inline float4
Dot(v2_4 A, v2_4 B)
{
    float4 Result = (A.X * B.X) + (A.Y * B.Y);
    //__m128 Result = _mm_add_ps((_mm_mul_ps(A.X, B.X)), (_mm_mul_ps(A.Y, B.Y)));
    return Result;
}

inline float4
LengthSq(v2_4 In)
{
    float4 Result = Dot(In, In);
    return Result;
}
inline float4
Length(v2_4 In)
{
    float4 Result;
    Result.Val = _mm_sqrt_ps(LengthSq(In).Val);
    return Result;
}

#define SCHMASTEROIDS_MATH_H 1
#endif
