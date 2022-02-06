#ifndef SCHMASTEROIDS_STRINGS_H
#define SCHMASTEROIDS_STRINGS_H 1

internal u32 
StringLength(const char* CString)
{
    i32 Result = 0;
    while (*CString++) {
        ++Result;
    }
    return Result;
}

internal u32
MaxStringLength(const char** Strings, u32 StringCount)
{
    u32 Result = 0;
    for (u32 StringIndex = 0; StringIndex < StringCount; ++StringIndex)
    {
        Result = Max(Result, StringLength(Strings[StringIndex]));
    }
    return Result;
}

internal u32
SetStringFromNumber(char *String, u64 Number, u32 MaxBytes)
{
    Assert(MaxBytes >= 1);
    // NOTE: We will always end with a null char.
    if (Number == 0) {
        Assert(MaxBytes >= 2);
        String[0] = '0';
        String[1] = 0;
        return 1;
    }

    Assert(Number > 0);
    u32 DigitCount = 1;
    u64 Scratch = Number;
    while ((Scratch /= 10) > 0) {
        ++DigitCount;
    }
    Assert(DigitCount + 1 <= MaxBytes);

    Scratch = Number;
    String[DigitCount] = 0;
    for(char* Digit = String + DigitCount - 1;
        Digit >= String;
        --Digit)
    {
        *Digit = '0' + (Scratch % 10);
        Scratch /= 10;
    }
    return DigitCount;
}

internal char* StringCopy(char* Dest, char* Source)
{
    while(*Source != 0) {
        *Dest++ = *Source++;
    }
    *Dest = 0;
    return Dest;
}

internal char* StringCopyCounted(char* Dest, const char* Source, u32 MaxStringLength)
{
    u32 Copied = 0;
    while(*Source != 0 && Copied < MaxStringLength) {
        *Dest++ = *Source++;
        Copied++;
    }
    *Dest = 0;
    return Dest;
}
#endif
