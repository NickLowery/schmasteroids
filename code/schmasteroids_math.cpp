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

inline int
RoundToInt(float F)
{
    return (int)(F+0.5f);
}

inline float
Sin(float F)
{
    float Result = sinf(F);
    return Result;
}

inline i32
Floor(float F) 
{
    i32 Result = (i32)floorf(F);
    return Result;
}

inline i32
Ceil(float F)
{
    i32 Result = (i32)ceilf(F);
    return Result;
}

internal v2
ScaleV2ToMagnitude(v2 OldV, float NewMagnitude)
{
    float OldMagnitude = Length(OldV);
    v2 Result = (NewMagnitude/OldMagnitude) * OldV;
    return Result;
}


inline v2
V2FromAngleAndMagnitude(float Angle, float Magnitude) 
{
    v2 Result = Rotate(V2(Magnitude, 0), Angle);
    return Result;
}

internal v2
Rotate(v2 V, float Angle)
{
    v2 Result;
    float Sin = (float)sin(Angle);
    float Cos = (float)cos(Angle);
    Result.Y = (Sin * (V.X)) + (Cos * (V.Y));
    Result.X = (Cos * (V.X)) - (Sin * (V.Y));
    return Result;
}

inline rect
RectFromCenterAndDimensions(v2 Center, v2 Dimensions)
{
    rect Result;
    v2 HalfDim = V2(Dimensions.X/2.0f, Dimensions.Y/2.0f);
    Result.Left = Center.X - HalfDim.X;
    Result.Top = Center.Y - HalfDim.Y;
    Result.Bottom = Center.Y + HalfDim.Y;
    Result.Right = Center.X + HalfDim.X;
    return Result;
}

inline v2
CenterOfRect(rect Rect)
{
    v2 Result;
    float Width = Rect.Right - Rect.Left;
    float Height = Rect.Bottom - Rect.Top;
    Result.X = Rect.Left + (Width/2);
    Result.Y = Rect.Top + (Height/2);
    return Result;
}

inline float
CenterYOfRect(rect Rect)
{
    float Height = Rect.Bottom - Rect.Top;
    float Result = Rect.Top + (Height/2);
    return Result;
}

inline rect
RectFromMinCornerAndDimensions(v2 MinCorner, v2 Dimensions)
{
    rect Result;
    Result.Left = MinCorner.X;
    Result.Top = MinCorner.Y;
    Result.Bottom = MinCorner.Y + Dimensions.Y;
    Result.Right = MinCorner.X + Dimensions.X;
    return Result;
}

internal bool32 
InRect(v2 V, rect Rect)
{
    if (V.X > Rect.Right)
        return false;
    if (V.X < Rect.Left)
        return false;
    if (V.Y > Rect.Bottom)
        return false;
    if (V.Y < Rect.Top)
        return false;
    else
        return true;
}

internal v2
ProjectNormalizedV2ToRect(v2 Norm, rect Rect)
{
    v2 Result;
    float Width = Rect.Right - Rect.Left;
    float Height = Rect.Bottom - Rect.Top;
    Result.X = Rect.Left + (Norm.X * Width);
    Result.Y = Rect.Top + (Norm.Y * Height);
    return Result;
}

internal v2
NormalizeV2FromRect(v2 In, rect Rect) {
    float Width = Rect.Right - Rect.Left;
    float Height = Rect.Bottom - Rect.Top;
    Assert(InRect(In, Rect));
    v2 Result;
    Result.X = (In.X - Rect.Left) / Width;
    Result.Y = (In.Y - Rect.Top) / Height;
    return Result;
}

inline v2
ClosestPointOnLineSeg(v2 TestPoint, v2 First, v2 Second)
{
    v2 Result;
    v2 FirstToSecond = Second - First;
    v2 FirstToTestPoint = TestPoint - First;
    float tIntersect = Dot(FirstToSecond, FirstToTestPoint) / LengthSq(FirstToSecond);
    if (tIntersect > 1.0f) {
        Result = Second;
    } else if (tIntersect < 0.0f) {
        Result = First;
    } else {
        Result = First + (tIntersect * FirstToSecond);
    }
    return Result;
}

inline v2
ClosestPointOnLineSeg(v2 TestPoint, line_seg *Seg) 
{
    v2 Result = ClosestPointOnLineSeg(TestPoint, Seg->First, Seg->Second);
    return Result;
}

inline void
ClampToRange(float Min, float *Val, float Max)
{
    Assert(Min < Max);
    if (*Val < Min) {
        *Val = Min;
    } else if (*Val > Max) {
        *Val = Max;
    }
}

internal clamp_result ClampToHalfOpenRange(float In, float Min, float Max) 
{
    clamp_result Result;

    float Epsilon = 0.05f;
    if (In >= Max) {
        Result.Value = Max - Epsilon;
        Result.Clamped = true;
    } else if (In < Min) {
        Result.Value = Min;
        Result.Clamped = true;
    } else {
        Result.Value = In;
        Result.Clamped = false;
    }
    return Result;
}

internal v2
ClampPointToRect(v2 Point, rect Rect) {
    v2 Result = Point;
    if (Result.X < Rect.Left) {
        Result.X = Rect.Left;
    } else if (Result.X > Rect.Right) {
        Result.X = Rect.Right;
    }
    if (Result.Y < Rect.Top) {
        Result.Y = Rect.Top;
    } else if (Result.Y > Rect.Bottom) {
        Result.Y = Rect.Bottom;
    }
    return Result;
}

internal v2
ClipEndPointToRect(v2 VIn, v2 VOut, rect Rect) 
{
    Assert(InRect(VIn, Rect));
    Assert(!InRect(VOut, Rect));
    float Slope = (VOut.Y - VIn.Y)/(VOut.X - VIn.X);

    clamp_result XClamp = ClampToHalfOpenRange(VOut.X, Rect.Left, Rect.Right);
    if (XClamp.Clamped) {
        VOut.X = XClamp.Value;
        VOut.Y = (Slope * (VOut.X - VIn.X)) + VIn.Y;
    }

    clamp_result YClamp = ClampToHalfOpenRange(VOut.Y, Rect.Top, Rect.Bottom);
    if (YClamp.Clamped) {
        VOut.Y = YClamp.Value;
        VOut.X = ((VOut.Y - VIn.Y) / Slope) + VIn.X;
    }

    Assert(InRect(VOut, Rect));
    return VOut;
}

