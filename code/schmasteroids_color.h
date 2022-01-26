#ifndef SCHMASTEROIDS_COLOR_H
typedef union {
    i32 Full;
    u8 Bytes[4];
    struct {
        u8 B;
        u8 G;
        u8 R;
        u8 A;
    };
} color;

typedef struct {
    float H;
    float S;
    float L;
} hsl_color;


inline color
Color(u32 Value)
{
    color Result;
    Result.Full = Value;
    return Result;
}

inline color 
NullColor(void)
{
    return Color(0);
}


internal i32 EuclideanDivide(float Numerator, float Divisor) {
    Assert(Divisor > 0.0f);
    i32 Result = Floor(Numerator/Divisor);
    return Result;
}

internal float EuclideanMod(float Value, float Modulus) {
    Assert(Modulus > 0.0f);
    float Result = Value - Modulus*EuclideanDivide(Value, Modulus);
    Assert(Result >= 0.0f);
    return Result;
}

typedef union {
    color Colors[4];
    __m128i IVec;
    __m128 Vec;
} rgba_4;

inline rgba_4
AddRGBSaturating(rgba_4 First, rgba_4 Second)
{
    rgba_4 Result;
    Result.IVec = _mm_adds_epu8(First.IVec, Second.IVec);
    return Result;
}

#define SCHMASTEROIDS_COLOR_H 1
#endif
