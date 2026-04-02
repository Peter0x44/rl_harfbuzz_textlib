#ifndef RL_HARFBUZZ_TEXTLIB_RL_HARFBUZZ_TEXTLIB_H
#define RL_HARFBUZZ_TEXTLIB_RL_HARFBUZZ_TEXTLIB_H

#include <stddef.h>
#include <stdbool.h>

#include "raylib.h"

#if defined(_WIN32)
  #if defined(RLHB_BUILD_SHARED)
    #define RLHB_API __declspec(dllexport)
  #elif defined(RLHB_USE_SHARED)
    #define RLHB_API __declspec(dllimport)
  #else
    #define RLHB_API
  #endif
#elif defined(__GNUC__) && defined(RLHB_BUILD_SHARED)
  #define RLHB_API __attribute__((visibility("default")))
#else
  #define RLHB_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rlhbRenderer rlhbRenderer;
typedef struct rlhbFont rlhbFont;
typedef struct rlhbTextRun rlhbTextRun;

typedef enum rlhbDirection {
  rlhbDirectionAuto = 0,
  rlhbDirectionLtr,
  rlhbDirectionRtl,
  rlhbDirectionTtb,
  rlhbDirectionBtt,
} rlhbDirection;

typedef enum rlhbTextAlign {
  rlhbTextAlignLeft = 0,
  rlhbTextAlignCenter,
  rlhbTextAlignRight,
} rlhbTextAlign;

typedef struct rlhbShapeOptions {
  float fontSize;
  rlhbDirection direction;
  rlhbTextAlign align;
  const char *language;
  const char *script;
} rlhbShapeOptions;

typedef struct rlhbRunMetrics {
  float width;
  float ascent;
  float descent;
  Rectangle bounds;
  int glyphCount;
} rlhbRunMetrics;

RLHB_API rlhbShapeOptions rlhbGetDefaultShapeOptions(void);

RLHB_API rlhbRenderer *rlhbCreateRenderer(void);
RLHB_API void rlhbDestroyRenderer(rlhbRenderer *renderer);
RLHB_API bool rlhbIsRendererReady(const rlhbRenderer *renderer);
RLHB_API const char *rlhbGetLastError(const rlhbRenderer *renderer);
RLHB_API float rlhbGetAtlasUsageKiB(const rlhbRenderer *renderer);

RLHB_API rlhbFont *rlhbLoadFontFromFile(rlhbRenderer *renderer, const char *filePath);
RLHB_API rlhbFont *rlhbLoadDefaultFont(rlhbRenderer *renderer);
RLHB_API void rlhbUnloadFont(rlhbFont *font);
RLHB_API int rlhbGetCachedGlyphCount(const rlhbFont *font);

RLHB_API bool rlhbShapeTextN(rlhbRenderer *renderer,
                             rlhbFont *font,
                             const char *text,
                             size_t length,
                             const rlhbShapeOptions *options,
                             rlhbTextRun **outRun);
RLHB_API bool rlhbShapeTextNT(rlhbRenderer *renderer,
                              rlhbFont *font,
                              const char *text,
                              const rlhbShapeOptions *options,
                              rlhbTextRun **outRun);

RLHB_API void rlhbDestroyTextRun(rlhbTextRun *run);
RLHB_API rlhbRunMetrics rlhbGetTextRunMetrics(const rlhbTextRun *run);
RLHB_API bool rlhbDrawTextRun(rlhbRenderer *renderer,
                              const rlhbTextRun *run,
                              Vector2 baseline,
                              Color tint);

RLHB_API bool rlhbDrawTextN(rlhbRenderer *renderer,
                            rlhbFont *font,
                            const char *text,
                            size_t length,
                            Vector2 baseline,
                            Color tint,
                            const rlhbShapeOptions *options);
RLHB_API bool rlhbDrawTextNT(rlhbRenderer *renderer,
                             rlhbFont *font,
                             const char *text,
                             Vector2 baseline,
                             Color tint,
                             const rlhbShapeOptions *options);

RLHB_API rlhbRunMetrics rlhbMeasureTextN(rlhbRenderer *renderer,
                                         rlhbFont *font,
                                         const char *text,
                                         size_t length,
                                         const rlhbShapeOptions *options);
RLHB_API rlhbRunMetrics rlhbMeasureTextNT(rlhbRenderer *renderer,
                                          rlhbFont *font,
                                          const char *text,
                                          const rlhbShapeOptions *options);

#ifdef __cplusplus
}
#endif

#endif