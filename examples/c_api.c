#include "rl_harfbuzz_textlib/rl_harfbuzz_textlib.h"

#include "raylib.h"

int main(void) {
  static const char kText[] = "Congrats! This window draws HarfBuzz GPU text.";

  InitWindow(800, 450, "rlhb C API example");

  rlhbRenderer *renderer = rlhbCreateRenderer();
  rlhbFont *font = rlhbLoadDefaultFont(renderer);


  rlhbShapeOptions options = rlhbGetDefaultShapeOptions();
  options.fontSize = 20.0f;

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    rlhbDrawText(renderer, font, kText, (Vector2){190.0f, 220.0f}, LIGHTGRAY, &options);

    EndDrawing();
  }

  rlhbUnloadFont(font);
  rlhbDestroyRenderer(renderer);
  CloseWindow();
  return 0;
}