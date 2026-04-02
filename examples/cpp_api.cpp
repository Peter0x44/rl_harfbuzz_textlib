#include "rl_harfbuzz_textlib/rl_harfbuzz_textlib.hpp"

#include "raylib.h"

int main() {
  const char *kText = "Congrats! This window draws HarfBuzz GPU text.";

  InitWindow(800, 450, "rlhb C++ API example");

  rlhb::Renderer renderer;
  rlhb::Font font = renderer.loadDefaultFont();

  if (!font) {
    CloseWindow();
    return 1;
  }

  rlhbShapeOptions options = rlhbGetDefaultShapeOptions();
  options.fontSize = 20.0f;

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    (void)renderer.drawText(kText, font, Vector2{190.0f, 220.0f}, LIGHTGRAY, options);

    EndDrawing();
  }

  CloseWindow();
  return 0;
}