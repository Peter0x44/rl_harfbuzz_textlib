# rl_harfbuzz_textlib

HarfBuzz GPU text rendering for raylib.

`rl_harfbuzz_textlib` shapes and renders UTF-8 text with HarfBuzz. It is aimed at projects that want support for ligatures, Arabic and RTL shaping, as well as GPU font rasterization to make arbitrary-size font drawing crispy and beautiful.

## What Is Here

- C API in `include/rl_harfbuzz_textlib/rl_harfbuzz_textlib.h`
- Thin C++ wrapper in `include/rl_harfbuzz_textlib/rl_harfbuzz_textlib.hpp`
- Core implementation in `src/rl_harfbuzz_textlib.cpp`
- Bundled default font `DejaVuSans.ttf`
- Three examples: a C API example, C++ API example, and an interactive demo

## Current Scope

- raylib with the OpenGL 3.3 backend
- local or subproject CMake usage
- HarfBuzz build from `harfbuzz-world.cc`

## Build

Requirements:

- CMake 4.0 or newer
- A C++17-capable compiler
- raylib available in your environment, or `RLHB_FETCH_RAYLIB=ON`

Typical build for this repository:

```powershell
cmake -S . -B build -DRLHB_BUILD_EXAMPLES=ON -DRLHB_FETCH_RAYLIB=ON
cmake --build build
```

If your environment already provides `raylib` or `raylib::raylib`, leave `RLHB_FETCH_RAYLIB` off.

CMake options:

- `RLHB_BUILD_EXAMPLES=ON|OFF` builds the example programs. Default: `OFF`
- `RLHB_FETCH_RAYLIB=ON|OFF` fetches raylib if it is not already available. Default: `OFF`
- `RLHB_BUNDLE_DEFAULT_FONT=ON|OFF` embeds the bundled fallback font. Default: `ON`
- `RLHB_HARFBUZZ_SOURCE_DIR=/path/to/harfbuzz` uses your own HarfBuzz source tree instead of the pinned download

## Using From CMake

This project is set up for subproject use:

```cmake
add_subdirectory(rl_harfbuzz_textlib)
target_link_libraries(my_app PRIVATE rl_harfbuzz_textlib::rl_harfbuzz_textlib)
```

The library vendors HarfBuzz internally and links against raylib.

## API At A Glance

Core objects:

- `rlhbRenderer`: owns GPU state and the glyph atlas
- `rlhbFont`: a font associated with a renderer
- `rlhbTextRun`: a reusable shaped run

Main C functions:

- `rlhbCreateRenderer()` / `rlhbDestroyRenderer()`
- `rlhbLoadFontFromFile()` / `rlhbLoadDefaultFont()` / `rlhbUnloadFont()`
- `rlhbGetDefaultShapeOptions()`
- `rlhbDrawText()` / `rlhbDrawTextN()`
- `rlhbShapeText()` / `rlhbShapeTextN()` / `rlhbDrawTextRun()`
- `rlhbMeasureText()` / `rlhbMeasureTextN()`
- `rlhbBeginDraw()` / `rlhbEndDraw()`

Main C++ wrapper types:

- `rlhb::Renderer`
- `rlhb::Font`
- `rlhb::TextRun`

One-shot draw example in C:

```c
InitWindow(800, 450, "rlhb example");

rlhbRenderer *renderer = rlhbCreateRenderer();
rlhbFont *font = rlhbLoadDefaultFont(renderer);

rlhbShapeOptions options = rlhbGetDefaultShapeOptions();
options.fontSize = 32.0f;
options.align = rlhbTextAlignCenter;

BeginDrawing();
ClearBackground(RAYWHITE);
rlhbDrawText(renderer,
             font,
             "Hello world",
             (Vector2){400.0f, 220.0f},
             DARKGRAY,
             &options);
EndDrawing();
```

If the text stays the same across frames, shape it once with `rlhbShapeText*()` and reuse the result with `rlhbDrawTextRun()`.

## Begin And End Draw

The one-shot draw helpers work on their own, but if you are drawing several text items in one frame, you should prefer to begin and end an explicit draw scope:

```c
BeginDrawing();
ClearBackground(RAYWHITE);

rlhbBeginDraw(renderer);
rlhbDrawText(renderer, font, "First", (Vector2){40.0f, 120.0f}, DARKGRAY, &options);
rlhbDrawText(renderer, font, "Second", (Vector2){40.0f, 170.0f}, DARKGRAY, &options);
rlhbEndDraw(renderer);

EndDrawing();
```


The C++ wrapper exposes this as `renderer.beginDraw()` and `renderer.endDraw()`.

If the call is not wrapped in rlhbBeginDraw and rlhbEndDraw, it will set up and tear down the state for every single call.

Ending the scope returns the renderer to a default raylib state when it ends. It does not restore any previously active custom shader or other user-managed OpenGL state.

## Shaping And Positioning

- Text input is UTF-8
- `fontSize` is in pixels
- `direction` defaults to `rlhbDirectionAuto`
- `align` controls how `baseline.x` anchors the run
- Draw positions use a typographic baseline, not a top-left origin
- `rlhbRunMetrics` exposes width, ascent, descent, bounds, and glyph count

If you want top-left style layout, measure first and place the baseline at `top + ascent`.

## Default Font

When `RLHB_BUNDLE_DEFAULT_FONT=ON`, the library embeds `DejaVuSans.ttf` and exposes it through rlhbLoadDefaultFont.

Relevant files:

- `assets/fonts/DejaVuSans.ttf`
- `assets/fonts/LICENSE.DejaVu.txt`
- `src/rlhb_default_font_data.h`

## Logging

By default the library logs through raylib's `TraceLog`.

You can override that with:

- C API: `rlhbSetLogCallback()`
- C++ API: `rlhb::setLogCallback()`

Passing `nullptr` restores the default logger.

## Examples

The repository currently ships three examples:

- `rlhb_c_api_example`: a tiny C example in the spirit of raylib's `core_basic_window.c`
- `rlhb_cpp_api_example`: the same kind of minimal example using the C++ wrapper
- `rlhb_type_lab_example`: an interactive demo for typing, shaping, zooming, panning, and trying different fonts


Type lab controls:

- Type to replace the sample text
- Click preset, direction, and alignment chips
- `Backspace` deletes one UTF-8 codepoint
- `Ctrl+V` pastes text from the clipboard
- Mouse wheel zooms
- Left mouse drag pans
- Drop a font file onto the window to switch fonts
- `Up` and `Down` change font size
- `F4` cycles direction, `F5` resets the view, and `F6` cycles alignment


## Limitations

Current limitations include:

- no full bidi run segmentation or reordering for mixed-direction text
- no paragraph layout or line wrapping
- no automatic font fallback chain
- fixed-size glyph atlas that cannot expand at runtime
- OpenGL 3.3 backend only (untested on web)

These will be addressed in future.

## AI use disclosure

This code was written using GPT-5.4 and then reviewed and tested by @Peter0x44. If that offends your moral framework, this library is not for you.

## Credits
 
| Project | Link | Notes |
| --- | --- | --- |
| @raysan5 | [raysan5/raylib](https://github.com/raysan5/raylib) | Raylib library |
| @JeffM2501 | [raylib-extras/rTextLib](https://github.com/raylib-extras/rTextLib) | rTextLib API design inspiration |
| @behdad | [harfbuzz/harfbuzz](https://github.com/harfbuzz/harfbuzz) | HarfBuzz maintainer |