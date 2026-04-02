#include "rl_harfbuzz_textlib/rl_harfbuzz_textlib.h"

#include "raylib.h"

#include <string.h>

int main(int argc, char **argv) {
  static const char *headlineText = "Pure C API example";
  static const char *bodyText = "This line is drawn with rlhbDrawTextN using pointer plus length.";

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(1280, 720, "rlhb C API example");
  SetTargetFPS(60);

  rlhbRenderer *renderer = rlhbCreateRenderer();
  rlhbFont *font = NULL;
  rlhbTextRun *headline = NULL;

  rlhbShapeOptions headlineOptions = rlhbGetDefaultShapeOptions();
  headlineOptions.fontSize = 60.0f;

  rlhbShapeOptions bodyOptions = rlhbGetDefaultShapeOptions();
  bodyOptions.fontSize = 32.0f;

  if (argc >= 2) {
    font = rlhbLoadFontFromFile(renderer, argv[1]);
  } else {
    font = rlhbLoadDefaultFont(renderer);
  }

  if (font != NULL) {
    rlhbShapeTextNT(renderer, font, headlineText, &headlineOptions, &headline);
  }

  while (!WindowShouldClose()) {
    BeginDrawing();

    ClearBackground(CLITERAL(Color){245, 243, 235, 255});
    DrawText("rlhb C API example", 60, 48, 32, CLITERAL(Color){39, 36, 31, 255});
    DrawText("This is a real C translation unit using only the public C header. No argv uses bundled Amiri.",
             60,
             92,
             20,
             CLITERAL(Color){88, 78, 66, 255});

    if (font == NULL || headline == NULL) {
      DrawText("Setup failed. Check logs.", 60, 180, 24, MAROON);
      DrawText("Example override: rlhb_c_api_example C:/path/to/YourFont.ttf",
               60,
               214,
               18,
               CLITERAL(Color){88, 78, 66, 255});
    } else {
      rlhbDrawTextRun(renderer, headline, (Vector2){60.0f, 250.0f}, CLITERAL(Color){22, 29, 36, 255});
      rlhbDrawTextN(renderer,
                    font,
                    bodyText,
                    strlen(bodyText),
                    (Vector2){60.0f, 330.0f},
                    CLITERAL(Color){54, 63, 74, 255},
                    &bodyOptions);

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

  rlhbDestroyTextRun(headline);
  rlhbUnloadFont(font);
  rlhbDestroyRenderer(renderer);
  CloseWindow();
  return 0;
}