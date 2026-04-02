#ifndef RL_HARFBUZZ_TEXTLIB_RL_HARFBUZZ_TEXTLIB_HPP
#define RL_HARFBUZZ_TEXTLIB_RL_HARFBUZZ_TEXTLIB_HPP

#include <string_view>
#include <utility>

#include "rl_harfbuzz_textlib/rl_harfbuzz_textlib.h"

namespace rlhb {

using LogCallback = ::rlhbLogCallback;

inline void setLogCallback(LogCallback callback, void *userData = nullptr) noexcept {
  rlhbSetLogCallback(callback, userData);
}

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

  void reset() noexcept {
    if (handle_ != nullptr) {
      rlhbDestroyTextRun(handle_);
      handle_ = nullptr;
    }
  }

  [[nodiscard]] ::rlhbRunMetrics metrics() const noexcept {
    return rlhbGetTextRunMetrics(handle_);
  }

  [[nodiscard]] ::rlhbTextRun *get() const noexcept { return handle_; }
  [[nodiscard]] explicit operator bool() const noexcept { return handle_ != nullptr; }

 private:
  ::rlhbTextRun *handle_ = nullptr;
};

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

  void reset() noexcept {
    if (handle_ != nullptr) {
      rlhbUnloadFont(handle_);
      handle_ = nullptr;
    }
  }

  [[nodiscard]] int cachedGlyphCount() const noexcept {
    return rlhbGetCachedGlyphCount(handle_);
  }

  [[nodiscard]] ::rlhbFont *get() const noexcept { return handle_; }
  [[nodiscard]] explicit operator bool() const noexcept { return handle_ != nullptr; }

 private:
  ::rlhbFont *handle_ = nullptr;
};

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

  void reset() noexcept {
    if (handle_ != nullptr) {
      rlhbDestroyRenderer(handle_);
      handle_ = nullptr;
    }
  }

  [[nodiscard]] bool ready() const noexcept { return rlhbIsRendererReady(handle_); }
  [[nodiscard]] float atlasUsageKiB() const noexcept { return rlhbGetAtlasUsageKiB(handle_); }

  [[nodiscard]] Font loadFont(const char *filePath) const {
    return Font(rlhbLoadFontFromFile(handle_, filePath));
  }

  [[nodiscard]] Font loadDefaultFont() const {
    return Font(rlhbLoadDefaultFont(handle_));
  }

  [[nodiscard]] TextRun shapeText(std::string_view text,
                                  Font &font,
                                  const ::rlhbShapeOptions &options) const {
    ::rlhbTextRun *run = nullptr;
    if (!rlhbShapeTextN(handle_, font.get(), text.data(), text.size(), &options, &run)) {
      return TextRun();
    }
    return TextRun(run);
  }

  [[nodiscard]] bool drawText(std::string_view text,
                              Font &font,
                              Vector2 baseline,
                              Color tint,
                              const ::rlhbShapeOptions &options) const {
    return rlhbDrawTextN(handle_, font.get(), text.data(), text.size(), baseline, tint, &options);
  }

  [[nodiscard]] bool drawText(const TextRun &run, Vector2 baseline, Color tint) const {
    return rlhbDrawTextRun(handle_, run.get(), baseline, tint);
  }

  [[nodiscard]] ::rlhbRenderer *get() const noexcept { return handle_; }
  [[nodiscard]] explicit operator bool() const noexcept { return handle_ != nullptr; }

 private:
  ::rlhbRenderer *handle_ = nullptr;
};

}  // namespace rlhb

#endif