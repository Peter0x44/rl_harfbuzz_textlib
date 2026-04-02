#include "rl_harfbuzz_textlib/rl_harfbuzz_textlib.hpp"

#include "raylib.h"

#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr int kWindowWidth = 1480;
constexpr int kWindowHeight = 920;
constexpr float kMinFontSize = 10.0f;
constexpr float kMaxFontSize = 180.0f;
constexpr float kMinZoomFactor = 0.01f;
constexpr float kZoomStep = 1.15f;
constexpr size_t kMaxTextBytes = 192;
constexpr double kBackspaceInitialDelay = 0.40;
constexpr double kBackspaceRepeatInterval = 0.035;

struct SamplePreset {
  const char *label;
  const char *text;
};

constexpr SamplePreset kPresets[] = {
    {
        "Ligatures",
        "ffi fi fl office",
    },
    {
        "Arabic",
        u8"هذا نص عربي للاختبار",
    },
    {
        "Emoji",
        u8"😀☠️❤️",
    },
};

struct DemoState {
  std::string text;
  float fontSize = 42.0f;
  float zoomFactor = 1.0f;
  Vector2 stagePan = {0.0f, 0.0f};
  rlhbDirection direction = rlhbDirectionAuto;
  rlhbTextAlign align = rlhbTextAlignCenter;
  const char *language = nullptr;
  const char *script = nullptr;
  const char *presetLabel = nullptr;
  std::string status;
  bool dirty = true;
};

std::string FormatString(const char *format, ...) {
  char buffer[512] = {};

  va_list args;
  va_start(args, format);
  const int written = std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (written <= 0) {
    return {};
  }

  const size_t length = std::min(static_cast<size_t>(written), sizeof(buffer) - 1u);
  return std::string(buffer, length);
}

size_t Utf8SequenceLength(unsigned char leadByte) {
  if ((leadByte & 0x80u) == 0u) {
    return 1;
  }
  if ((leadByte & 0xE0u) == 0xC0u) {
    return 2;
  }
  if ((leadByte & 0xF0u) == 0xE0u) {
    return 3;
  }
  if ((leadByte & 0xF8u) == 0xF0u) {
    return 4;
  }
  return 1;
}

std::string EncodeUtf8(int codepoint) {
  std::string encoded;
  if (codepoint <= 0x7F) {
    encoded.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FF) {
    encoded.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
    encoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0xFFFF) {
    encoded.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
    encoded.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    encoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0x10FFFF) {
    encoded.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
    encoded.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    encoded.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    encoded.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  return encoded;
}

void AppendUtf8Clamped(std::string *text, std::string_view fragment) {
  if (text == nullptr || fragment.empty() || text->size() >= kMaxTextBytes) {
    return;
  }

  size_t offset = 0;
  while (offset < fragment.size() && text->size() < kMaxTextBytes) {
    const size_t codepointBytes = Utf8SequenceLength(static_cast<unsigned char>(fragment[offset]));
    if (offset + codepointBytes > fragment.size()) {
      break;
    }
    if (text->size() + codepointBytes > kMaxTextBytes) {
      break;
    }
    text->append(fragment.substr(offset, codepointBytes));
    offset += codepointBytes;
  }
}

void PopUtf8Back(std::string *text) {
  if (text == nullptr || text->empty()) {
    return;
  }

  size_t newSize = text->size() - 1;
  while (newSize > 0 && (static_cast<unsigned char>((*text)[newSize]) & 0xC0u) == 0x80u) {
    --newSize;
  }
  text->resize(newSize);
}

bool ControlHeld() {
  return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
}

bool DrawUiText(const rlhb::Renderer &renderer,
                rlhb::Font &font,
                std::string_view text,
                Vector2 baseline,
                float fontSize,
                Color tint,
                rlhbTextAlign align = rlhbTextAlignLeft,
                rlhbDirection direction = rlhbDirectionAuto,
                const char *language = nullptr,
                const char *script = nullptr) {
  rlhbShapeOptions options = rlhbGetDefaultShapeOptions();
  options.fontSize = fontSize;
  options.align = align;
  options.direction = direction;
  options.language = language;
  options.script = script;
  return renderer.drawText(text, font, baseline, tint, options);
}

rlhbRunMetrics MeasureUiText(const rlhb::Renderer &renderer,
                             rlhb::Font &font,
                             std::string_view text,
                             float fontSize,
                             rlhbTextAlign align = rlhbTextAlignLeft,
                             rlhbDirection direction = rlhbDirectionAuto,
                             const char *language = nullptr,
                             const char *script = nullptr) {
  rlhbShapeOptions options = rlhbGetDefaultShapeOptions();
  options.fontSize = fontSize;
  options.align = align;
  options.direction = direction;
  options.language = language;
  options.script = script;
  return rlhbMeasureTextN(renderer.get(), font.get(), text.data(), text.size(), &options);
}

const char *DirectionLabel(rlhbDirection direction) {
  switch (direction) {
    case rlhbDirectionAuto:
      return "Auto";
    case rlhbDirectionLtr:
      return "LTR";
    case rlhbDirectionRtl:
      return "RTL";
    case rlhbDirectionTtb:
      return "TTB";
    case rlhbDirectionBtt:
      return "BTT";
    default:
      return "Unknown";
  }
}

const char *AlignLabel(rlhbTextAlign align) {
  switch (align) {
    case rlhbTextAlignLeft:
      return "Left";
    case rlhbTextAlignCenter:
      return "Center";
    case rlhbTextAlignRight:
      return "Right";
    default:
      return "Unknown";
  }
}

void SetDirection(DemoState *state, rlhbDirection direction) {
  if (state == nullptr) {
    return;
  }

  state->direction = direction;
  switch (direction) {
    case rlhbDirectionLtr:
      state->language = "en";
      state->script = "Latn";
      break;
    case rlhbDirectionRtl:
      state->language = "ar";
      state->script = "Arab";
      break;
    case rlhbDirectionAuto:
    default:
      state->language = nullptr;
      state->script = nullptr;
      break;
  }

  state->dirty = true;
}

void SetAlign(DemoState *state, rlhbTextAlign align) {
  if (state == nullptr) {
    return;
  }

  state->align = align;
  state->dirty = true;
}

void CycleDirection(DemoState *state) {
  if (state == nullptr) {
    return;
  }

  switch (state->direction) {
    case rlhbDirectionAuto:
      SetDirection(state, rlhbDirectionLtr);
      break;
    case rlhbDirectionLtr:
      SetDirection(state, rlhbDirectionRtl);
      break;
    case rlhbDirectionRtl:
      SetDirection(state, rlhbDirectionAuto);
      break;
    default:
      SetDirection(state, rlhbDirectionAuto);
      break;
  }
}

void CycleAlign(DemoState *state) {
  if (state == nullptr) {
    return;
  }

  switch (state->align) {
    case rlhbTextAlignLeft:
      SetAlign(state, rlhbTextAlignCenter);
      break;
    case rlhbTextAlignCenter:
      SetAlign(state, rlhbTextAlignRight);
      break;
    case rlhbTextAlignRight:
    default:
      SetAlign(state, rlhbTextAlignLeft);
      break;
  }
}

void ApplyPreset(DemoState *state, int presetIndex) {
  if (state == nullptr || presetIndex < 0 || presetIndex >= static_cast<int>(std::size(kPresets))) {
    return;
  }

  const SamplePreset &preset = kPresets[presetIndex];
  state->text = preset.text;
  state->presetLabel = preset.label;
  state->status.clear();
  state->dirty = true;
}

void MarkCustomText(DemoState *state) {
  if (state == nullptr) {
    return;
  }

  state->dirty = true;
}

void ResetDemoState(DemoState *state) {
  if (state == nullptr) {
    return;
  }

  *state = DemoState();
}

Rectangle MakeRect(float x, float y, float w, float h) {
  return Rectangle{x, y, w, h};
}

float StageAnchorX(const Rectangle &panel) {
  return panel.x + panel.width * 0.5f;
}

float LeftXFromMetrics(float anchorX, float width, rlhbTextAlign align) {
  if (align == rlhbTextAlignCenter) {
    return anchorX - width * 0.5f;
  }
  if (align == rlhbTextAlignRight) {
    return anchorX - width;
  }
  return anchorX;
}

void DrawGrid(const Rectangle &panel, float spacing, Color tint) {
  const int columns = static_cast<int>(panel.width / spacing);
  const int rows = static_cast<int>(panel.height / spacing);

  for (int column = 1; column < columns; ++column) {
    const float x = panel.x + column * spacing;
    DrawLineV(Vector2{x, panel.y}, Vector2{x, panel.y + panel.height}, tint);
  }

  for (int row = 1; row < rows; ++row) {
    const float y = panel.y + row * spacing;
    DrawLineV(Vector2{panel.x, y}, Vector2{panel.x + panel.width, y}, tint);
  }
}

void DrawPanelChrome(const Rectangle &panel, Color fill, Color border) {
  DrawRectangleRounded(panel, 0.045f, 16, fill);
  DrawRectangleRoundedLinesEx(panel, 0.045f, 16, 2.0f, border);
}

bool DrawChoiceChip(const rlhb::Renderer &renderer,
                    rlhb::Font &font,
                    Rectangle rect,
                    std::string_view label,
                    bool active,
                    bool hovered,
                    Color panelFill,
                    Color border,
                    Color accent,
                    Color textColor) {
  const Color fill = active ? Fade(accent, 0.16f) : (hovered ? Fade(border, 0.12f) : Fade(border, 0.06f));
  const Color outline = active ? accent : Fade(border, hovered ? 0.96f : 0.86f);
  const Color labelColor = active ? accent : textColor;

  DrawRectangleRounded(rect, 0.28f, 12, fill);
  DrawRectangleRoundedLinesEx(rect, 0.28f, 12, 2.0f, outline);
  return DrawUiText(renderer,
                    font,
                    label,
                    Vector2{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.68f},
                    18.0f,
                    labelColor,
                    rlhbTextAlignCenter);
}

}  // namespace

int main(int argc, char **argv) {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(kWindowWidth, kWindowHeight, "rlhb type lab");
  SetTargetFPS(60);

  rlhb::Renderer renderer;
  rlhb::Font font;
  rlhb::TextRun liveRun;

  if (argc >= 2) {
    font = renderer.loadFont(argv[1]);
  } else {
    font = renderer.loadDefaultFont();
  }

  if (!font) {
    CloseWindow();
    return 1;
  }

  DemoState state;
  double nextBackspaceRepeatTime = 0.0;
  bool draggingView = false;

  while (!WindowShouldClose()) {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    const Rectangle headerPanel = MakeRect(30.0f, 26.0f, static_cast<float>(screenWidth - 60), 150.0f);
    const Rectangle footerPanel = MakeRect(30.0f, static_cast<float>(screenHeight - 98), static_cast<float>(screenWidth - 60), 68.0f);
    const Rectangle stagePanel = MakeRect(30.0f,
                                          194.0f,
                                          static_cast<float>(screenWidth - 60),
                                          footerPanel.y - 18.0f - 194.0f);

    float chipX = headerPanel.x + 44.0f;
    const float chipY = headerPanel.y + 102.0f;
    const float chipH = 34.0f;
    const float chipGap = 10.0f;
    const float groupGap = 26.0f;

    const Rectangle presetLigaturesRect = MakeRect(chipX, chipY, 108.0f, chipH);
    chipX += presetLigaturesRect.width + chipGap;
    const Rectangle presetArabicRect = MakeRect(chipX, chipY, 92.0f, chipH);
    chipX += presetArabicRect.width + chipGap;
    const Rectangle presetEmojiRect = MakeRect(chipX, chipY, 78.0f, chipH);
    chipX += presetEmojiRect.width + groupGap;
    const Rectangle directionAutoRect = MakeRect(chipX, chipY, 72.0f, chipH);
    chipX += directionAutoRect.width + chipGap;
    const Rectangle directionLtrRect = MakeRect(chipX, chipY, 62.0f, chipH);
    chipX += directionLtrRect.width + chipGap;
    const Rectangle directionRtlRect = MakeRect(chipX, chipY, 62.0f, chipH);
    chipX += directionRtlRect.width + groupGap;
    const Rectangle alignLeftRect = MakeRect(chipX, chipY, 72.0f, chipH);
    chipX += alignLeftRect.width + chipGap;
    const Rectangle alignCenterRect = MakeRect(chipX, chipY, 90.0f, chipH);
    chipX += alignCenterRect.width + chipGap;
    const Rectangle alignRightRect = MakeRect(chipX, chipY, 78.0f, chipH);

    const Vector2 mouse = GetMousePosition();
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      state.zoomFactor *= std::pow(kZoomStep, wheel);
      if (state.zoomFactor < kMinZoomFactor) {
        state.zoomFactor = kMinZoomFactor;
      }
      state.dirty = true;
    }

    if (IsKeyPressed(KEY_UP)) {
      state.fontSize = std::clamp(state.fontSize + 2.0f, kMinFontSize, kMaxFontSize);
      state.dirty = true;
    }
    if (IsKeyPressed(KEY_DOWN)) {
      state.fontSize = std::clamp(state.fontSize - 2.0f, kMinFontSize, kMaxFontSize);
      state.dirty = true;
    }
    if (IsKeyPressed(KEY_PAGE_UP)) {
      state.zoomFactor *= kZoomStep;
      state.dirty = true;
    }
    if (IsKeyPressed(KEY_PAGE_DOWN)) {
      state.zoomFactor /= kZoomStep;
      if (state.zoomFactor < kMinZoomFactor) {
        state.zoomFactor = kMinZoomFactor;
      }
      state.dirty = true;
    }
    if (IsKeyPressed(KEY_F6)) {
      CycleAlign(&state);
    }
    if (IsKeyPressed(KEY_F4)) {
      CycleDirection(&state);
    }
    if (IsKeyPressed(KEY_F5)) {
      ResetDemoState(&state);
      nextBackspaceRepeatTime = 0.0;
      draggingView = false;
    }
    if (IsKeyPressed(KEY_DELETE)) {
      state.text.clear();
      MarkCustomText(&state);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      if (CheckCollisionPointRec(mouse, presetLigaturesRect)) {
        ApplyPreset(&state, 0);
      } else if (CheckCollisionPointRec(mouse, presetArabicRect)) {
        ApplyPreset(&state, 1);
      } else if (CheckCollisionPointRec(mouse, presetEmojiRect)) {
        ApplyPreset(&state, 2);
      } else if (CheckCollisionPointRec(mouse, directionAutoRect)) {
        SetDirection(&state, rlhbDirectionAuto);
      } else if (CheckCollisionPointRec(mouse, directionLtrRect)) {
        SetDirection(&state, rlhbDirectionLtr);
      } else if (CheckCollisionPointRec(mouse, directionRtlRect)) {
        SetDirection(&state, rlhbDirectionRtl);
      } else if (CheckCollisionPointRec(mouse, alignLeftRect)) {
        SetAlign(&state, rlhbTextAlignLeft);
      } else if (CheckCollisionPointRec(mouse, alignCenterRect)) {
        SetAlign(&state, rlhbTextAlignCenter);
      } else if (CheckCollisionPointRec(mouse, alignRightRect)) {
        SetAlign(&state, rlhbTextAlignRight);
      } else if (CheckCollisionPointRec(mouse, stagePanel)) {
        draggingView = true;
      }
    }

    if (draggingView && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      const Vector2 delta = GetMouseDelta();
      state.stagePan.x += delta.x;
      state.stagePan.y += delta.y;
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      draggingView = false;
    }

    int codepoint = GetCharPressed();
    while (codepoint > 0) {
      if (codepoint >= 32 && codepoint != 127) {
        const std::string encoded = EncodeUtf8(codepoint);
        if (!encoded.empty() && state.text.size() + encoded.size() <= kMaxTextBytes) {
          state.text += encoded;
          MarkCustomText(&state);
        }
      }
      codepoint = GetCharPressed();
    }

    const double now = GetTime();
    if (IsKeyPressed(KEY_BACKSPACE) && !state.text.empty()) {
      PopUtf8Back(&state.text);
      MarkCustomText(&state);
      nextBackspaceRepeatTime = now + kBackspaceInitialDelay;
    } else if (IsKeyDown(KEY_BACKSPACE) && !state.text.empty() && now >= nextBackspaceRepeatTime && nextBackspaceRepeatTime > 0.0) {
      PopUtf8Back(&state.text);
      MarkCustomText(&state);
      nextBackspaceRepeatTime = now + kBackspaceRepeatInterval;
    }

    if (IsKeyReleased(KEY_BACKSPACE) || state.text.empty()) {
      nextBackspaceRepeatTime = 0.0;
    }

    if (ControlHeld() && IsKeyPressed(KEY_V)) {
      const char *clipboard = GetClipboardText();
      if (clipboard != nullptr && clipboard[0] != '\0') {
        AppendUtf8Clamped(&state.text, clipboard);
        MarkCustomText(&state);
      }
    }

    std::string droppedFontError;
    if (IsFileDropped()) {
      FilePathList droppedFiles = LoadDroppedFiles();
      bool loadedDroppedFont = false;

      for (unsigned int fileIndex = 0; fileIndex < droppedFiles.count; ++fileIndex) {
        const char *droppedPath = droppedFiles.paths[fileIndex];
        if (droppedPath == nullptr || droppedPath[0] == '\0') {
          continue;
        }

        rlhb::Font droppedFont = renderer.loadFont(droppedPath);
        if (droppedFont) {
          font = std::move(droppedFont);
          state.dirty = true;
          loadedDroppedFont = true;
          break;
        }

        if (droppedFontError.empty()) {
          droppedFontError = FormatString("Font load failed for %s. Check logs.", GetFileName(droppedPath));
        }
      }

      UnloadDroppedFiles(droppedFiles);

      if (!loadedDroppedFont && droppedFontError.empty()) {
        droppedFontError = "Dropped file could not be loaded as a font. Check logs.";
      }
    }

    if (state.dirty) {
      liveRun.reset();
      state.status.clear();
      const float stageFontSize = std::max(state.fontSize * state.zoomFactor, 0.1f);

      if (!font) {
        state.status = "Font load failed. Check logs.";
      } else if (!state.text.empty()) {
        rlhbShapeOptions liveOptions = rlhbGetDefaultShapeOptions();
        liveOptions.fontSize = stageFontSize;
        liveOptions.direction = state.direction;
        liveOptions.align = state.align;
        liveOptions.language = state.language;
        liveOptions.script = state.script;

        liveRun = renderer.shapeText(state.text, font, liveOptions);

        if (!liveRun) {
          state.status = "Shaping failed. Check logs.";
        }
      }

      state.dirty = false;
    }

    if (!droppedFontError.empty() && state.status.empty()) {
      state.status = droppedFontError;
    }

    const Color backgroundTop = CLITERAL(Color){22, 27, 34, 255};
    const Color backgroundBottom = CLITERAL(Color){206, 196, 183, 255};
    const Color panelFill = CLITERAL(Color){248, 244, 238, 238};
    const Color panelBorder = CLITERAL(Color){74, 86, 96, 255};
    const Color accent = CLITERAL(Color){174, 41, 41, 255};
    const Color accentGlow = CLITERAL(Color){224, 96, 96, 255};
    const Color markerRing = CLITERAL(Color){34, 112, 56, 255};
    const Color markerFill = CLITERAL(Color){95, 184, 116, 255};
    const Color markerCore = CLITERAL(Color){25, 85, 42, 255};
    const Color grid = Fade(CLITERAL(Color){77, 89, 103, 255}, 0.10f);
    const Color bodyColor = CLITERAL(Color){24, 30, 37, 255};
    const Color helperColor = CLITERAL(Color){68, 64, 57, 255};

    const float stageBaselineY = stagePanel.y + stagePanel.height * 0.57f + state.stagePan.y;
    const float stageAnchorX = StageAnchorX(stagePanel) + state.stagePan.x;
    const float stageFontSize = std::max(state.fontSize * state.zoomFactor, 0.1f);
    const float baselineThickness = 2.0f * state.zoomFactor;
    const float markerRingRadius = 5.0f * state.zoomFactor;
    const float markerOuterRadius = 3.9f * state.zoomFactor;
    const float markerInnerRadius = 1.8f * state.zoomFactor;

    const std::string emptyPromptText = "Type some text.";
    const bool hasUserText = !state.text.empty();

    rlhbRunMetrics liveMetrics = {};
    if (liveRun) {
      liveMetrics = liveRun.metrics();
    }

    const rlhbRunMetrics stageMetrics = hasUserText
                                          ? liveMetrics
                                          : MeasureUiText(renderer,
                                                          font,
                                                          emptyPromptText,
                                                          stageFontSize,
                                                          state.align,
                                                          state.direction,
                                                          state.language,
                                                          state.script);
    const float stageLeftX = LeftXFromMetrics(stageAnchorX, stageMetrics.width, state.align);
    const float baselineStartX = stageLeftX + std::min(0.0f, stageMetrics.bounds.x);
    const float baselineEndX = std::max(stageLeftX + stageMetrics.width,
                                        stageLeftX + stageMetrics.bounds.x + stageMetrics.bounds.width);

    const std::string titleText = "rlhb Type Lab";
    const std::string subtitleText = "Type freely, click chips, drag the stage, drop a font file, and zoom with the wheel.";
    const char *presetReadout = state.presetLabel != nullptr ? state.presetLabel : "None";
    const std::string headerReadoutText = FormatString("size %.0f px   zoom x%.2f   pan %.0f, %.0f",
                                                       state.fontSize,
                                                       state.zoomFactor,
                                                       state.stagePan.x,
                                                       state.stagePan.y);
    const std::string footerStatsText = FormatString("preset %s   glyphs %i   cached %i   atlas %.2f KiB",
                             presetReadout,
                             liveMetrics.glyphCount,
                             font.cachedGlyphCount(),
                             renderer.atlasUsageKiB());
    const std::string footerHintText =
      "wheel zooms   drag pans   drop font file   ctrl+v paste   f5 resets defaults";

    BeginDrawing();

    DrawRectangleGradientV(0, 0, screenWidth, screenHeight, backgroundTop, backgroundBottom);

    DrawPanelChrome(headerPanel, panelFill, panelBorder);
    DrawPanelChrome(stagePanel, panelFill, panelBorder);
    DrawPanelChrome(footerPanel, Fade(CLITERAL(Color){247, 240, 230, 255}, 0.95f), panelBorder);

    DrawGrid(stagePanel, 36.0f, grid);
    if (baselineEndX > baselineStartX) {
      DrawLineEx(Vector2{baselineStartX, stageBaselineY},
                 Vector2{baselineEndX, stageBaselineY},
                 baselineThickness,
                 Fade(accentGlow, 0.95f));
    }

    bool drawFailed = false;
    if (renderer.beginDraw()) {
      drawFailed = !DrawUiText(renderer,
                               font,
                               titleText,
                               Vector2{54.0f, 74.0f},
                               40.0f,
                               bodyColor) || drawFailed;
      drawFailed = !DrawUiText(renderer,
                               font,
                               subtitleText,
                               Vector2{56.0f, 104.0f},
                               19.0f,
                               helperColor) || drawFailed;
      drawFailed = !DrawUiText(renderer,
                               font,
                               headerReadoutText,
                               Vector2{headerPanel.x + headerPanel.width - 42.0f, 74.0f},
                               22.0f,
                               helperColor,
                               rlhbTextAlignRight) || drawFailed;

      if (hasUserText && liveRun) {
        drawFailed = !renderer.drawText(liveRun, Vector2{stageAnchorX, stageBaselineY}, bodyColor) || drawFailed;
      } else {
        drawFailed = !DrawUiText(renderer,
                                 font,
                                 emptyPromptText,
                                 Vector2{stageAnchorX, stageBaselineY},
                                 stageFontSize,
                                 helperColor,
                                 state.align,
                                 state.direction,
                                 state.language,
                                 state.script) || drawFailed;
      }

      drawFailed = !DrawChoiceChip(renderer,
                                   font,
                                   presetLigaturesRect,
                                   "Ligatures",
                                   state.presetLabel == kPresets[0].label,
                                   CheckCollisionPointRec(mouse, presetLigaturesRect),
                                   panelFill,
                                   panelBorder,
                                   accent,
                                   bodyColor) || drawFailed;
      drawFailed = !DrawChoiceChip(renderer,
                                   font,
                                   presetArabicRect,
                                   "Arabic",
                                   state.presetLabel == kPresets[1].label,
                                   CheckCollisionPointRec(mouse, presetArabicRect),
                                   panelFill,
                                   panelBorder,
                                   accent,
                                   bodyColor) || drawFailed;
      drawFailed = !DrawChoiceChip(renderer,
                                   font,
                                   presetEmojiRect,
                                   "Emoji",
                                   state.presetLabel == kPresets[2].label,
                                   CheckCollisionPointRec(mouse, presetEmojiRect),
                                   panelFill,
                                   panelBorder,
                                   accent,
                                   bodyColor) || drawFailed;
      drawFailed = !DrawChoiceChip(renderer,
                                   font,
                                   directionAutoRect,
                                   "Auto",
                                   state.direction == rlhbDirectionAuto,
                                   CheckCollisionPointRec(mouse, directionAutoRect),
                                   panelFill,
                                   panelBorder,
                                   accent,
                                   bodyColor) || drawFailed;
      drawFailed = !DrawChoiceChip(renderer,
                                   font,
                                   directionLtrRect,
                                   "LTR",
                                   state.direction == rlhbDirectionLtr,
                                   CheckCollisionPointRec(mouse, directionLtrRect),
                                   panelFill,
                                   panelBorder,
                                   accent,
                                   bodyColor) || drawFailed;
      drawFailed = !DrawChoiceChip(renderer,
                                   font,
                                   directionRtlRect,
                                   "RTL",
                                   state.direction == rlhbDirectionRtl,
                                   CheckCollisionPointRec(mouse, directionRtlRect),
                                   panelFill,
                                   panelBorder,
                                   accent,
                                   bodyColor) || drawFailed;
      drawFailed = !DrawChoiceChip(renderer,
                                   font,
                                   alignLeftRect,
                                   "Left",
                                   state.align == rlhbTextAlignLeft,
                                   CheckCollisionPointRec(mouse, alignLeftRect),
                                   panelFill,
                                   panelBorder,
                                   accent,
                                   bodyColor) || drawFailed;
      drawFailed = !DrawChoiceChip(renderer,
                                   font,
                                   alignCenterRect,
                                   "Center",
                                   state.align == rlhbTextAlignCenter,
                                   CheckCollisionPointRec(mouse, alignCenterRect),
                                   panelFill,
                                   panelBorder,
                                   accent,
                                   bodyColor) || drawFailed;
      drawFailed = !DrawChoiceChip(renderer,
                                   font,
                                   alignRightRect,
                                   "Right",
                                   state.align == rlhbTextAlignRight,
                                   CheckCollisionPointRec(mouse, alignRightRect),
                                   panelFill,
                                   panelBorder,
                                   accent,
                                   bodyColor) || drawFailed;

      drawFailed = !DrawUiText(renderer,
                               font,
                               footerStatsText,
                               Vector2{footerPanel.x + 22.0f, footerPanel.y + 28.0f},
                               18.0f,
                               helperColor) || drawFailed;
      drawFailed = !DrawUiText(renderer,
                               font,
                               footerHintText,
                               Vector2{footerPanel.x + 22.0f, footerPanel.y + 51.0f},
                               17.0f,
                               helperColor) || drawFailed;

      if (!state.status.empty()) {
        drawFailed = !DrawUiText(renderer,
                                 font,
                                 state.status,
                                 Vector2{54.0f, 126.0f},
                                 18.0f,
                                 MAROON) || drawFailed;
      }
      if (drawFailed) {
        DrawUiText(renderer,
                   font,
                   "Draw failed. Check logs.",
                   Vector2{54.0f, 126.0f},
                   18.0f,
                   MAROON);
      }

      renderer.endDraw();
    }

    if (liveRun) {
      const float liveLeftX = LeftXFromMetrics(stageAnchorX, liveMetrics.width, state.align);
      const Rectangle liveBounds = Rectangle{
          liveLeftX + liveMetrics.bounds.x,
          stageBaselineY + liveMetrics.bounds.y,
          liveMetrics.bounds.width,
          liveMetrics.bounds.height,
      };
      DrawRectangleLinesEx(liveBounds, 2.0f, Fade(CLITERAL(Color){67, 98, 117, 255}, 0.75f));

      if ((static_cast<int>(std::floor(GetTime() * 2.0)) % 2) == 0) {
        const float caretX = liveLeftX + liveMetrics.width + 2.0f;
        DrawLineEx(Vector2{caretX, stageBaselineY - liveMetrics.ascent},
                   Vector2{caretX, stageBaselineY + liveMetrics.descent},
                   2.5f,
                   accent);
      }
    }

    DrawCircleLinesV(Vector2{stageAnchorX, stageBaselineY}, markerRingRadius, markerRing);
    DrawCircleV(Vector2{stageAnchorX, stageBaselineY}, markerOuterRadius, markerFill);
    DrawCircleV(Vector2{stageAnchorX, stageBaselineY}, markerInnerRadius, markerCore);

    EndDrawing();
  }

  CloseWindow();
  return 0;
}