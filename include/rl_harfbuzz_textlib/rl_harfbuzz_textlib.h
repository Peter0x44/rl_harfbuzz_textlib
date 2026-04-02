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


// Opaque renderer handle that owns GPU state and the glyph atlas.
typedef struct rlhbRenderer rlhbRenderer;

// Opaque font handle associated with a specific renderer.
typedef struct rlhbFont rlhbFont;

// Opaque shaped text run that can be drawn multiple times.
typedef struct rlhbTextRun rlhbTextRun;

typedef enum rlhbDirection {
  rlhbDirectionAuto = 0, // Let HarfBuzz infer direction from the text.
  rlhbDirectionLtr,      // left-to-right
  rlhbDirectionRtl,      // right-to-left
  rlhbDirectionTtb,      // top-to-bottom
  rlhbDirectionBtt,      // bottom-to-top
} rlhbDirection;

typedef enum rlhbTextAlign {
  rlhbTextAlignLeft = 0,
  rlhbTextAlignCenter,
  rlhbTextAlignRight,
} rlhbTextAlign;

typedef struct rlhbShapeOptions {
  float fontSize;          // Requested draw size in pixels.
  rlhbDirection direction; // Writing direction. Auto usually works for normal UTF-8 text.
  rlhbTextAlign align;     // Horizontal anchoring used when drawing a shaped run.
  const char *language;    // Optional language tag such as "en" or "ar".
  const char *script;      // Optional script tag such as "Latn" or "Arab".
} rlhbShapeOptions;

typedef struct rlhbRunMetrics {
  float width;      // Advance width of the run.
  float ascent;     // Distance above the baseline.
  float descent;    // Distance below the baseline.
  Rectangle bounds; // Bounding box relative to the draw baseline, not a top-left origin.
  int glyphCount;   // Number of shaped glyphs in the run.
} rlhbRunMetrics;

// Global log callback used by the library. Passing NULL restores default TraceLog behavior.
typedef void (*rlhbLogCallback)(int level, const char *message, void *userData);

// Returns default options: 48 px, auto direction, left alignment, and no explicit language or script.
RLHB_API rlhbShapeOptions rlhbGetDefaultShapeOptions(void);

// Sets the global log callback used for warnings and errors. Passing NULL restores the default logger.
RLHB_API void rlhbSetLogCallback(rlhbLogCallback callback, void *userData);

// Creates a renderer. Prefer calling after InitWindow so GPU resources can be created immediately.
RLHB_API rlhbRenderer *rlhbCreateRenderer(void);

// Destroys a renderer and all GPU resources it owns.
RLHB_API void rlhbDestroyRenderer(rlhbRenderer *renderer);

// Returns true once the renderer has initialized its GPU resources successfully.
RLHB_API bool rlhbIsRendererReady(const rlhbRenderer *renderer);

// Returns current glyph atlas usage in KiB.
RLHB_API float rlhbGetAtlasUsageKiB(const rlhbRenderer *renderer);

// Loads a font file and associates it with the renderer that will shape and draw it.
RLHB_API rlhbFont *rlhbLoadFontFromFile(rlhbRenderer *renderer, const char *filePath);

// Loads the bundled fallback font when RLHB_BUNDLE_DEFAULT_FONT was enabled at build time.
RLHB_API rlhbFont *rlhbLoadDefaultFont(rlhbRenderer *renderer);

// Releases a font returned by rlhbLoadFontFromFile or rlhbLoadDefaultFont.
RLHB_API void rlhbUnloadFont(rlhbFont *font);

// Returns how many glyphs have been encoded and cached for this font.
RLHB_API int rlhbGetCachedGlyphCount(const rlhbFont *font);

// Shapes a UTF-8 string using an explicit byte length and returns an owned text run.
RLHB_API bool rlhbShapeTextN(rlhbRenderer *renderer,
                             rlhbFont *font,
                             const char *text,
                             size_t length,
                             const rlhbShapeOptions *options,
                             rlhbTextRun **outRun);

// Shapes a null-terminated UTF-8 string and returns an owned text run.
RLHB_API bool rlhbShapeText(rlhbRenderer *renderer,
                            rlhbFont *font,
                            const char *text,
                            const rlhbShapeOptions *options,
                            rlhbTextRun **outRun);

// Releases a shaped run returned by rlhbShapeTextN or rlhbShapeText.
RLHB_API void rlhbDestroyTextRun(rlhbTextRun *run);

// Returns cached metrics for a shaped run. A NULL run returns zeroed metrics.
RLHB_API rlhbRunMetrics rlhbGetTextRunMetrics(const rlhbTextRun *run);

// Begins an explicit text draw scope. This installs the text shader and related draw state.
// rlhbEndDraw() returns the renderer to a default raylib state, not any previous custom shader mode.
// If you are using BeginShaderMode(), BeginBlendMode(), or other custom draw state, end and restart it yourself around rlhb draws.
RLHB_API bool rlhbBeginDraw(rlhbRenderer *renderer);

// Ends an explicit text draw scope previously started with rlhbBeginDraw().
// This does not restore an arbitrary user shader or other custom OpenGL state.
RLHB_API void rlhbEndDraw(rlhbRenderer *renderer);

// Draws a previously shaped run at the given typographic baseline position.
// baseline.x is the horizontal anchor, adjusted by the run's alignment.
// baseline.y is the vertical baseline the glyphs sit on, not the top edge of the text.
// When no explicit draw scope is active, this creates a temporary one internally.
RLHB_API bool rlhbDrawTextRun(rlhbRenderer *renderer,
                              const rlhbTextRun *run,
                              Vector2 baseline,
                              Color tint);

// Shapes and draws a UTF-8 string with an explicit byte length in one call.
// baseline uses typographic coordinates, not a top-left text box origin.
// When no explicit draw scope is active, this creates a temporary one internally.
RLHB_API bool rlhbDrawTextN(rlhbRenderer *renderer,
                            rlhbFont *font,
                            const char *text,
                            size_t length,
                            Vector2 baseline,
                            Color tint,
                            const rlhbShapeOptions *options);

// Shapes and draws a null-terminated UTF-8 string in one call.
// baseline uses typographic coordinates, not a top-left text box origin.
// When no explicit draw scope is active, this creates a temporary one internally.
RLHB_API bool rlhbDrawText(rlhbRenderer *renderer,
                           rlhbFont *font,
                           const char *text,
                           Vector2 baseline,
                           Color tint,
                           const rlhbShapeOptions *options);

// Shapes a UTF-8 string with an explicit byte length and returns its metrics.
RLHB_API rlhbRunMetrics rlhbMeasureTextN(rlhbRenderer *renderer,
                                         rlhbFont *font,
                                         const char *text,
                                         size_t length,
                                         const rlhbShapeOptions *options);

// Shapes a null-terminated UTF-8 string and returns its metrics.
RLHB_API rlhbRunMetrics rlhbMeasureText(rlhbRenderer *renderer,
                                        rlhbFont *font,
                                        const char *text,
                                        const rlhbShapeOptions *options);

#ifdef __cplusplus
}
#endif

#endif