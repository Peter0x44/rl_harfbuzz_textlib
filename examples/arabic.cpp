#include "rl_harfbuzz_textlib/rl_harfbuzz_textlib.h"

#include "raylib.h"

int main(int argc, char **argv) {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(1280, 760, "rlhb arabic example");
  SetTargetFPS(60);

  rlhbRenderer *renderer = rlhbCreateRenderer();
  rlhbFont *font = nullptr;
  rlhbTextRun *arabic = nullptr;
  rlhbTextRun *mixed = nullptr;

  if (argc >= 2) {
    font = rlhbLoadFontFromFile(renderer, argv[1]);
  } else {
    font = rlhbLoadDefaultFont(renderer);
  }

  if (font != nullptr) {
    rlhbShapeOptions arabicOptions = rlhbGetDefaultShapeOptions();
    arabicOptions.fontSize = 72.0f;
    arabicOptions.direction = rlhbDirectionRtl;
    arabicOptions.align = rlhbTextAlignRight;
    arabicOptions.language = "ar";
    arabicOptions.script = "Arab";

    rlhbShapeOptions mixedOptions = arabicOptions;
    mixedOptions.fontSize = 40.0f;

    rlhbShapeTextNT(renderer,
                    font,
                    u8"السلام عليكم ورحمة الله وبركاته",
                    &arabicOptions,
                    &arabic);
    rlhbShapeTextNT(renderer,
                    font,
                    u8"هذا سطر عربي مع mixed Latin text 123 داخل نفس التشغيل.",
                    &mixedOptions,
                    &mixed);
  }

  while (!WindowShouldClose()) {
    BeginDrawing();

    ClearBackground(CLITERAL(Color){242, 236, 228, 255});
    DrawText("Arabic shaping through HarfBuzz GPU", 60, 48, 32, CLITERAL(Color){39, 36, 31, 255});
    DrawText("No argv uses bundled Amiri. Pass argv[1] to override with another Arabic-capable font.",
             60,
             92,
             20,
             CLITERAL(Color){88, 78, 66, 255});

    if (font == nullptr || arabic == nullptr || mixed == nullptr) {
      DrawText("Setup failed. Check logs.", 60, 180, 24, MAROON);
      DrawText("Example override: rlhb_arabic_example C:/path/to/Amiri-Regular.ttf",
               60,
               214,
               18,
               CLITERAL(Color){88, 78, 66, 255});
    } else {
      const float rightEdge = static_cast<float>(GetScreenWidth() - 80);
      rlhbDrawTextRun(renderer, arabic, (Vector2){rightEdge, 250.0f}, CLITERAL(Color){24, 29, 36, 255});
      rlhbDrawTextRun(renderer, mixed, (Vector2){rightEdge, 360.0f}, CLITERAL(Color){52, 63, 74, 255});

      DrawText(TextFormat("cached glyphs: %i  atlas: %.2f KiB",
                          rlhbGetCachedGlyphCount(font),
                          rlhbGetAtlasUsageKiB(renderer)),
               60,
               460,
               20,
               CLITERAL(Color){88, 78, 66, 255});
    }

    EndDrawing();
  }

  rlhbDestroyTextRun(mixed);
  rlhbDestroyTextRun(arabic);
  rlhbUnloadFont(font);
  rlhbDestroyRenderer(renderer);
  CloseWindow();
  return 0;
}