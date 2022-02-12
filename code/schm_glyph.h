#ifndef SCHM_GLYPH_H
#define GLYPH_MAX_SEGS 8
typedef struct {
    i32 SegCount;
    line_seg Segs[GLYPH_MAX_SEGS];
} glyph;

#define SCHM_GLYPH_H
#endif
