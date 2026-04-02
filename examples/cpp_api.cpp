#include "rl_harfbuzz_textlib/rl_harfbuzz_textlib.hpp"

#include "raylib.h"

#include <string_view>

int main(int argc, char **argv) {
  constexpr std::string_view kTitle = "C++ wrapper example";
  constexpr std::string_view kBody = "This line uses the thin RAII wrapper and std::string_view overloads.";

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(1280, 720, "rlhb C++ API example");
  SetTargetFPS(60);

  rlhb::Renderer renderer;
  rlhb::Font font;
  rlhb::TextRun titleRun;

  rlhbShapeOptions titleOptions = rlhbGetDefaultShapeOptions();
  titleOptions.fontSize = 60.0f;

  rlhbShapeOptions bodyOptions = rlhbGetDefaultShapeOptions();
  bodyOptions.fontSize = 32.0f;

  if (argc >= 2) {
    font = renderer.loadFont(argv[1]);
  } else {
    font = renderer.loadDefaultFont();
  }

  if (font) {
    titleRun = renderer.shapeText(kTitle, font, titleOptions);
  }

  while (!WindowShouldClose()) {
    BeginDrawing();

    ClearBackground(CLITERAL(Color){245, 243, 235, 255});
    DrawText("rlhb C++ wrapper example", 60, 48, 32, CLITERAL(Color){39, 36, 31, 255});
    DrawText("This uses rlhb::Renderer, rlhb::Font, and rlhb::TextRun. No argv uses bundled Amiri.",
             60,
             92,
             20,
             CLITERAL(Color){88, 78, 66, 255});

    if (!font || !titleRun) {
      DrawText(renderer.lastError(), 60, 180, 24, MAROON);
      DrawText("Example override: rlhb_cpp_api_example C:/path/to/YourFont.ttf",
               60,
               214,
               18,
               CLITERAL(Color){88, 78, 66, 255});
    } else {
      const bool drewTitle = renderer.drawText(titleRun,
                                               (Vector2){60.0f, 250.0f},
                                               CLITERAL(Color){22, 29, 36, 255});
      const bool drewBody = renderer.drawText(kBody,
                                              font,
                                              (Vector2){60.0f, 330.0f},
                                              CLITERAL(Color){54, 63, 74, 255},
                                              bodyOptions);

      DrawText(TextFormat("cached glyphs: %i  atlas: %.2f KiB",
                          font.cachedGlyphCount(),
                          renderer.atlasUsageKiB()),
               60,
               390,
               20,
               CLITERAL(Color){88, 78, 66, 255});

      if (!drewTitle || !drewBody) {
        DrawText(renderer.lastError(), 60, 430, 20, MAROON);
      }
    }

    EndDrawing();
  }

  CloseWindow();
  return 0;
}