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
Square(float Value);

inline float
Sqrt(float Value);

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
V2FromAngleAndMagnitude(float Angle, float Magnitude);

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
Rotate(v2 V, float Angle);

internal v2
ClipEndPointToRect(v2 VIn, v2 VOut, rect Rect);

internal void SeedPRNG(u32 Seed)
{
    srand(Seed);
}

internal inline i32
RandI32()
{
    return rand();
}

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

/* Returns a pseudorandom float between 0 and 1 */
internal inline float
RandFloat01()
{
    return (float)rand() / (float)RAND_MAX;
    //TODO: There's probably a way better way to do this. Get a random significand, set exponent to 0, subtract 1?
    //Something like that.
}

internal inline float
RandFloatRange(float Min, float Max)
{
    return ((RandFloat01() * (Max - Min)) + Min);
    //TODO: There's probably a way better way to do this
}

internal inline float
RandHeading()
{
    return (RandFloat01() * 2.0f * PI);
}
internal inline v2
RandV2InRadius(float R) 
{
    return V2FromAngleAndMagnitude(RandHeading(), RandFloatRange(0.0f, R));
}

internal inline __m128
RandFloat01_4()
{
    __m128 Result = _mm_set_ps(RandFloat01(),  RandFloat01(), RandFloat01(),  RandFloat01());
    return Result;
}

internal inline __m128
RandHeading_4()
{
    __m128 Result = _mm_mul_ps(RandFloat01_4(), _mm_set_ps1(2.0f * PI));
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
