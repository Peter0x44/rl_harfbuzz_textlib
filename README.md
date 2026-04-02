# rl_harfbuzz_textlib

Reusable HarfBuzz 14 GPU text rendering for raylib.

## Current status

This repository now contains the initial reusable library skeleton:

- C API with rlhbPascalCase naming
- Optional thin C++ wrapper
- Single main implementation file for the core library
- HarfBuzz GPU atlas, glyph cache, shaping, and draw path extracted into reusable code
- Null-terminated and pointer-plus-length text APIs
- Optional bundled default font fallback
- Simple CMake integration for local/subproject use
- Small Latin and Arabic examples

## Build

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Dependency behavior

When building this project itself, dependency resolution works like this:

- reuse an existing compatible raylib target if one already exists
- try `find_package(... CONFIG)` for raylib
- fetch raylib if it is still missing and fetching is enabled
- build HarfBuzz internally from the upstream `harfbuzz-world.cc` amalgamated source

By default the project fetches a pinned HarfBuzz source snapshot and compiles the core plus GPU support into `rl_harfbuzz_textlib` itself. If you want to supply your own HarfBuzz source tree instead, set `RLHB_HARFBUZZ_SOURCE_DIR`.

## Default Font

By default, the library embeds `Amiri-Regular.ttf` and exposes it through the C API and the thin C++ wrapper.

- C API: `rlhbLoadDefaultFont()`
- C++ API: `renderer.loadDefaultFont()`

This is a convenience fallback so the examples can run without requiring the caller to provide a TTF path. It is a reasonable Arabic and Latin fallback, but it is not a universal font fallback system.

The bundled font asset and its license live in:

- `assets/fonts/Amiri-Regular.ttf`
- `assets/fonts/LICENSE.Amiri.txt`

The embedded byte array is pre-generated and committed in:

- `src/rlhb_default_font_data.h`

If you do not want the library to embed a fallback font, configure with `-DRLHB_BUNDLE_DEFAULT_FONT=OFF`.

## Logging

By default, the library logs through raylib's `TraceLog`.

This behavior can be overriden using:
- C API: `rlhbSetLogCallback()`
- C++ API: `rlhb::setLogCallback()`

Passing `nullptr` resets logging back to the default `TraceLog` behavior.

## Render State

The convenience draw helpers change rlgl state. In particular they flush raylib's active batch, install the library shader, and use their own blend and texture bindings.

For simple 2D usage that is fine. For explicit control, use a text draw scope:

- `rlhbBeginDraw()` / `rlhbEndDraw()` in C
- `renderer.beginDraw()` / `renderer.endDraw()` in C++

That makes the state ownership explicit and lets you batch several text draws under one setup cost.

Important limitation: ending the text draw scope returns to default raylib state. It does not restore an arbitrary previous custom shader, custom viewport, or other user-managed OpenGL state.

In particular, do not expect this sequence to preserve your custom shader mode:

```c
BeginShaderMode(myShader);
rlhbDrawText(...);
EndShaderMode();
```

The text draw installs its own shader and ends in default raylib shader state. If you want to mix custom shader mode with rlhb drawing, manage that scope yourself:

1. End your custom shader mode before calling `rlhbBeginDraw()` or any `rlhbDrawText*()` helper.
2. Draw the text.
3. Begin your custom shader mode again afterward if you still need it.

If your goal is to apply a custom effect to the text itself, render the text to a `RenderTexture` first and then draw that texture under your own shader.

## API Overview

Typical usage looks like this:

1. Call `InitWindow()` first.
2. Create an `rlhbRenderer`.
3. Load a font from disk using `rlhbLoadFontFromFile()` or call `rlhbLoadDefaultFont()`.
4. Start from `rlhbGetDefaultShapeOptions()` and override only what you need.
5. Shape once and draw many times with `rlhbShapeText*()` plus `rlhbDrawTextRun()`, or use `rlhbDrawText*()` for one-shot calls.
6. If you are issuing multiple text draws in a row, wrapping them in `rlhbBeginDraw()` / `rlhbEndDraw()` will improve performance.

### Shape options

- `fontSize` is the requested draw size in pixels.
- `direction` controls directional shaping. `rlhbDirectionAuto` usually works for normal UTF-8 text.
- `align` controls how `baseline.x` anchors a shaped run when it is drawn.
- `language` is optional extra shaping context, usually a language tag such as `en` or `ar`.
- `script` is optional extra shaping context, usually a four-letter script tag such as `Latn` or `Arab`.

In most cases you can leave `language` and `script` as null and let HarfBuzz infer them. They are exposed so callers can force shaping context for short text, mixed-script content, or fonts with language-specific substitutions.

### Baseline positioning

The draw APIs use a typographic baseline, not a top-left origin.

- `baseline.x` is the horizontal anchor point.
- `baseline.y` is the vertical line the glyphs sit on.
- `align` affects how the run is anchored horizontally around `baseline.x`.
- `ascent`, `descent`, and `bounds` in `rlhbRunMetrics` are all relative to that baseline.

If you want top-left style placement, shape or measure first and then place the baseline at `topY + ascent`.

### Minimal C example

```c
InitWindow(1280, 720, "rlhb example");

rlhbRenderer *renderer = rlhbCreateRenderer();
rlhbFont *font = rlhbLoadDefaultFont(renderer);

rlhbShapeOptions options = rlhbGetDefaultShapeOptions();
options.fontSize = 32.0f;

rlhbDrawText(renderer,
             font,
             "مرحبا world",
             (Vector2){40.0f, 120.0f},
             BLACK,
             &options);
```

If you draw the same text repeatedly unchanged, prefer `rlhbShapeTextN()` or `rlhbShapeText()` once and then reuse the resulting `rlhbTextRun` with `rlhbDrawTextRun()`.

If you are drawing multiple text runs in sequence, wrap them in `rlhbBeginDraw()` and `rlhbEndDraw()` so the library only installs its shader and related draw state once.

## Examples

There are now four example entry points, all as flat files under `examples/`:

- `rlhb_basic_example` and `rlhb_arabic_example` use the C API from C++ translation units
- `rlhb_c_api_example` is a pure C translation unit using only the public C header
- `rlhb_cpp_api_example` uses the thin C++ wrapper API

All examples can run without arguments by using the bundled default font, and still accept a font path override on the command line.

```powershell
./build/rlhb_basic_example
./build/rlhb_arabic_example
./build/rlhb_c_api_example
./build/rlhb_cpp_api_example

# optional override
./build/rlhb_basic_example C:/path/to/YourFont.ttf
./build/rlhb_arabic_example C:/path/to/Amiri-Regular.ttf
./build/rlhb_c_api_example C:/path/to/YourFont.ttf
./build/rlhb_cpp_api_example C:/path/to/YourFont.ttf
```

## CMake consumption

### As a subproject

```cmake
add_subdirectory(rl_harfbuzz_textlib)
target_link_libraries(my_app PRIVATE rl_harfbuzz_textlib::rl_harfbuzz_textlib)
```