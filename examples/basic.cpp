#include "rl_harfbuzz_textlib/rl_harfbuzz_textlib.h"

#include "raylib.h"

int main(int argc, char **argv) {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(1280, 720, "rlhb basic example");
  SetTargetFPS(60);

  rlhbRenderer *renderer = rlhbCreateRenderer();
  rlhbFont *font = nullptr;
  rlhbTextRun *headline = nullptr;
  rlhbTextRun *body = nullptr;

  if (argc >= 2) {
    font = rlhbLoadFontFromFile(renderer, argv[1]);
  } else {
    font = rlhbLoadDefaultFont(renderer);
  }

  if (font != nullptr) {
    rlhbShapeOptions headlineOptions = rlhbGetDefaultShapeOptions();
    headlineOptions.fontSize = 64.0f;

    rlhbShapeOptions bodyOptions = rlhbGetDefaultShapeOptions();
    bodyOptions.fontSize = 34.0f;

    rlhbShapeTextNT(renderer,
                    font,
                    "office affine official",
                    &headlineOptions,
                    &headline);
    rlhbShapeTextNT(renderer,
                    font,
                    "Ligatures are shaped once and rendered with HarfBuzz GPU coverage.",
                    &bodyOptions,
                    &body);
  }

  while (!WindowShouldClose()) {
    BeginDrawing();

    ClearBackground(CLITERAL(Color){245, 243, 235, 255});
    DrawText("rl_harfbuzz_textlib", 60, 48, 32, CLITERAL(Color){39, 36, 31, 255});
    DrawText("Pass a font path as argv[1], or let the example use the bundled Amiri fallback.",
             60,
             92,
             20,
             CLITERAL(Color){88, 78, 66, 255});

    if (font == nullptr || headline == nullptr || body == nullptr) {
      DrawText(rlhbGetLastError(renderer), 60, 180, 24, MAROON);
      DrawText("Example override: rlhb_basic_example C:/path/to/YourFont.ttf",
               60,
               214,
               18,
               CLITERAL(Color){88, 78, 66, 255});
    } else {
      rlhbDrawTextRun(renderer, headline, (Vector2){60.0f, 240.0f}, CLITERAL(Color){22, 29, 36, 255});
      rlhbDrawTextRun(renderer, body, (Vector2){60.0f, 320.0f}, CLITERAL(Color){54, 63, 74, 255});

      DrawText(TextFormat("cached glyphs: %i  atlas: %.2f KiB",
                          rlhbGetCachedGlyphCount(font),
                          rlhbGetAtlasUsageKiB(renderer)),
               60,
               390,
               20,
               CLITERAL(Color){88, 78, 66, 255});
    }

    EndDrawing();
  }

  rlhbDestroyTextRun(body);
  rlhbDestroyTextRun(headline);
  rlhbUnloadFont(font);
  rlhbDestroyRenderer(renderer);
  CloseWindow();
  return 0;
}