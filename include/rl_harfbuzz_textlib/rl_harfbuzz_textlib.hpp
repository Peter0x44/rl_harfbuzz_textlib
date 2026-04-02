#ifndef RL_HARFBUZZ_TEXTLIB_RL_HARFBUZZ_TEXTLIB_HPP
#define RL_HARFBUZZ_TEXTLIB_RL_HARFBUZZ_TEXTLIB_HPP

#include <string_view>
#include <utility>

#include "rl_harfbuzz_textlib/rl_harfbuzz_textlib.h"

namespace rlhb {

// C++ alias for the global C log callback type.
using LogCallback = ::rlhbLogCallback;

// Sets the global library log callback. Passing nullptr restores default TraceLog behavior.
inline void setLogCallback(LogCallback callback, void *userData = nullptr) noexcept {
  rlhbSetLogCallback(callback, userData);
}

// Move-only owner for a shaped text run.
class TextRun {
 public:
  TextRun() = default;
  explicit TextRun(::rlhbTextRun *handle) noexcept : handle_(handle) {}

  TextRun(const TextRun &) = delete;
  TextRun &operator=(const TextRun &) = delete;

  TextRun(TextRun &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

  TextRun &operator=(TextRun &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  ~TextRun() { reset(); }

  // Releases the owned shaped run, if any.
  void reset() noexcept {
    if (handle_ != nullptr) {
      rlhbDestroyTextRun(handle_);
      handle_ = nullptr;
    }
  }

  // Returns cached metrics for the shaped run.
  ::rlhbRunMetrics metrics() const noexcept {
    return rlhbGetTextRunMetrics(handle_);
  }

  ::rlhbTextRun *get() const noexcept { return handle_; }
  explicit operator bool() const noexcept { return handle_ != nullptr; }

 private:
  ::rlhbTextRun *handle_ = nullptr;
};

// Move-only owner for a font loaded through the C API.
class Font {
 public:
  Font() = default;
  explicit Font(::rlhbFont *handle) noexcept : handle_(handle) {}

  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;

  Font(Font &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

  Font &operator=(Font &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  ~Font() { reset(); }

  // Releases the owned font, if any.
  void reset() noexcept {
    if (handle_ != nullptr) {
      rlhbUnloadFont(handle_);
      handle_ = nullptr;
    }
  }

  // Returns how many glyphs are currently cached for this font.
  int cachedGlyphCount() const noexcept {
    return rlhbGetCachedGlyphCount(handle_);
  }

  ::rlhbFont *get() const noexcept { return handle_; }
  explicit operator bool() const noexcept { return handle_ != nullptr; }

 private:
  ::rlhbFont *handle_ = nullptr;
};

// Move-only owner for renderer state and convenience wrappers around the C API.
class Renderer {
 public:
  Renderer() : handle_(rlhbCreateRenderer()) {}

  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;

  Renderer(Renderer &&other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

  Renderer &operator=(Renderer &&other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  ~Renderer() { reset(); }

  // Releases the owned renderer, if any.
  void reset() noexcept {
    if (handle_ != nullptr) {
      rlhbDestroyRenderer(handle_);
      handle_ = nullptr;
    }
  }

  // Returns true once GPU resources have been initialized.
  bool ready() const noexcept { return rlhbIsRendererReady(handle_); }

  // Returns current glyph atlas usage in KiB.
  float atlasUsageKiB() const noexcept { return rlhbGetAtlasUsageKiB(handle_); }

  // Loads a font file and returns a move-only Font wrapper.
  Font loadFont(const char *filePath) const {
    return Font(rlhbLoadFontFromFile(handle_, filePath));
  }

  // Loads the bundled default font and returns a move-only Font wrapper.
  Font loadDefaultFont() const {
    return Font(rlhbLoadDefaultFont(handle_));
  }

  // Shapes UTF-8 text once and returns a reusable TextRun.
  TextRun shapeText(std::string_view text,
                    Font &font,
                    const ::rlhbShapeOptions &options) const {
    ::rlhbTextRun *run = nullptr;
    if (!rlhbShapeTextN(handle_, font.get(), text.data(), text.size(), &options, &run)) {
      return TextRun();
    }
    return TextRun(run);
  }

  // Begins an explicit text draw scope. endDraw() returns to default raylib-style state.
  // Note: Will overwrite any currently active shader state, and flush raylib's batch system.
  bool beginDraw() const {
    return rlhbBeginDraw(handle_);
  }

  // Ends an explicit text draw scope started with beginDraw().
  void endDraw() const noexcept {
    rlhbEndDraw(handle_);
  }

  // Shapes and draws UTF-8 text in one call.
  // baseline is a typographic baseline, not a top-left text box origin.
  bool drawText(std::string_view text,
                Font &font,
                Vector2 baseline,
                Color tint,
                const ::rlhbShapeOptions &options) const {
    return rlhbDrawTextN(handle_, font.get(), text.data(), text.size(), baseline, tint, &options);
  }

  // Draws a previously shaped run using a typographic baseline position.
  bool drawText(const TextRun &run, Vector2 baseline, Color tint) const {
    return rlhbDrawTextRun(handle_, run.get(), baseline, tint);
  }

  ::rlhbRenderer *get() const noexcept { return handle_; }
  explicit operator bool() const noexcept { return handle_ != nullptr; }

 private:
  ::rlhbRenderer *handle_ = nullptr;
};

}  // namespace rlhb

#endif