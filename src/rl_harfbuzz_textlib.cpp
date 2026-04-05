#include "rl_harfbuzz_textlib/rl_harfbuzz_textlib.h"

#include "rlhb_gl_shim.h"

#include "raymath.h"
#include "rlgl.h"

#include <hb-gpu.h>
#include <hb-ot.h>
#include <hb.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if !defined(GRAPHICS_API_OPENGL_33)
#error rl_harfbuzz_textlib currently requires the raylib OpenGL 3.3 backend.
#endif

#if defined(RLHB_HAS_DEFAULT_FONT)
#include "rlhb_default_font_data.h"
#endif

namespace {

constexpr unsigned kAtlasCapacityTexels = 1024u * 1024u;
constexpr unsigned kAtlasTexelBytes = 8u;
constexpr unsigned kInvalidAtlasOffset = std::numeric_limits<unsigned>::max();

struct EncodedGlyphInfo {
  float minX = 0.0f;
  float minY = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;
  float advance = 0.0f;
  bool empty = true;
  unsigned upem = 0;
  unsigned atlasOffset = 0;
};

struct GlyphVertex {
  float x;
  float y;
  float tx;
  float ty;
  float nx;
  float ny;
  float emPerPos;
  float atlasOffset;
};

struct PositionedGlyph {
  unsigned glyphIndex = 0;
  float xOffset = 0.0f;
  float yOffset = 0.0f;
  float xAdvance = 0.0f;
  float yAdvance = 0.0f;
};

static std::string JoinShaderSource(std::initializer_list<const char *> parts) {
  std::string source;
  size_t totalLength = 0;

  for (const char *part : parts) {
    if (part != nullptr) {
      totalLength += std::strlen(part);
    }
  }

  source.reserve(totalLength);

  for (const char *part : parts) {
    if (part != nullptr) {
      source += part;
    }
  }

  return source;
}

static hb_direction_t ToHbDirection(rlhbDirection direction) {
  switch (direction) {
    case rlhbDirectionLtr:
      return HB_DIRECTION_LTR;
    case rlhbDirectionRtl:
      return HB_DIRECTION_RTL;
    case rlhbDirectionTtb:
      return HB_DIRECTION_TTB;
    case rlhbDirectionBtt:
      return HB_DIRECTION_BTT;
    case rlhbDirectionAuto:
    default:
      return HB_DIRECTION_INVALID;
  }
}

static void AppendGlyphVertices(Vector2 pen,
                                float fontSize,
                                const EncodedGlyphInfo &glyph,
                                std::vector<GlyphVertex> *vertices) {
  if (glyph.empty) {
    return;
  }

  const float scale = fontSize / static_cast<float>(glyph.upem);
  GlyphVertex corners[4] = {};

  for (int corner = 0; corner < 4; ++corner) {
    const int cx = (corner >> 1) & 1;
    const int cy = corner & 1;

    const float ex = (1 - cx) * glyph.minX + cx * glyph.maxX;
    const float ey = (1 - cy) * glyph.minY + cy * glyph.maxY;

    corners[corner].x = pen.x + scale * ex;
    corners[corner].y = pen.y - scale * ey;
    corners[corner].tx = ex;
    corners[corner].ty = ey;
    corners[corner].nx = cx ? 1.0f : -1.0f;
    corners[corner].ny = cy ? -1.0f : 1.0f;
    corners[corner].emPerPos = 1.0f / scale;
    corners[corner].atlasOffset = static_cast<float>(glyph.atlasOffset);
  }

  vertices->push_back(corners[0]);
  vertices->push_back(corners[1]);
  vertices->push_back(corners[2]);
  vertices->push_back(corners[1]);
  vertices->push_back(corners[2]);
  vertices->push_back(corners[3]);
}

static void LogMessage(int level, const char *message);
static void LogMessage(int level, const std::string &message);
static void LogError(const char *message);
static void LogError(const std::string &message);
static void LogWarning(const char *message);

}  // namespace

struct rlhbRenderer {
  class GlyphAtlas {
   public:
    bool Init(unsigned capacityTexels) {
      capacity_ = capacityTexels;
      usedTexels_ = 0;

      glGenTextures(1, &texture_);
      glGenBuffers(1, &buffer_);

      if (texture_ == 0 || buffer_ == 0) {
        LogError("Failed to create OpenGL texture-buffer atlas objects.");
        return false;
      }

      glBindBuffer(GL_TEXTURE_BUFFER, buffer_);
      glBufferData(GL_TEXTURE_BUFFER,
                   static_cast<GLsizeiptr>(capacity_) * kAtlasTexelBytes,
                   nullptr,
                   GL_STATIC_DRAW);

      glBindTexture(GL_TEXTURE_BUFFER, texture_);
      glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA16I, buffer_);

      glBindTexture(GL_TEXTURE_BUFFER, 0);
      glBindBuffer(GL_TEXTURE_BUFFER, 0);
      return true;
    }

    void Shutdown() {
      if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
      }
      if (buffer_ != 0) {
        glDeleteBuffers(1, &buffer_);
      }
      texture_ = 0;
      buffer_ = 0;
      capacity_ = 0;
      usedTexels_ = 0;
    }

    unsigned Allocate(const char *data, unsigned lengthBytes) {
      const unsigned lengthTexels = lengthBytes / kAtlasTexelBytes;
      if ((lengthBytes % kAtlasTexelBytes) != 0) {
        LogError("HarfBuzz GPU encoded glyph data was not aligned to atlas texels.");
        return kInvalidAtlasOffset;
      }

      if (usedTexels_ + lengthTexels > capacity_) {
        LogError("Glyph atlas capacity exceeded. Atlas growth policy is not implemented yet.");
        return kInvalidAtlasOffset;
      }

      const unsigned offset = usedTexels_;
      usedTexels_ += lengthTexels;

      glBindBuffer(GL_TEXTURE_BUFFER, buffer_);
      glBufferSubData(GL_TEXTURE_BUFFER,
                      static_cast<GLintptr>(offset) * kAtlasTexelBytes,
                      lengthBytes,
                      data);
      glBindBuffer(GL_TEXTURE_BUFFER, 0);

      return offset;
    }

    void Bind(GLint atlasLocation) const {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_BUFFER, texture_);
      glUniform1i(atlasLocation, 0);
    }

    float UsedKiB() const {
      return static_cast<float>(usedTexels_) * (static_cast<float>(kAtlasTexelBytes) / 1024.0f);
    }

   private:
    GLuint texture_ = 0;
    GLuint buffer_ = 0;
    unsigned capacity_ = 0;
    unsigned usedTexels_ = 0;
  } atlas;

  GLuint program = 0;
  GLuint vao = 0;
  GLuint vbo = 0;
  GLint locMvp = -1;
  GLint locViewport = -1;
  GLint locGamma = -1;
  GLint locForeground = -1;
  GLint locStemDarkening = -1;
  GLint locDebug = -1;
  GLint locAtlas = -1;
  bool ready = false;
  bool debug = false;
  bool stemDarkening = true;
  float gamma = 1.0f / 2.2f;
  unsigned drawScopeDepth = 0;
  int drawScopeViewportWidth = 0;
  int drawScopeViewportHeight = 0;
  size_t vertexCapacity = 4096;
  std::vector<GlyphVertex> scratchVertices;
};

struct rlhbFont {
  rlhbRenderer *renderer = nullptr;
  std::filesystem::path fontPath;
  hb_face_t *face = nullptr;
  hb_font_t *font = nullptr;
  hb_gpu_draw_t *draw = nullptr;
  unsigned upem = 0;
  std::vector<unsigned char> ownedFontData;
  std::unordered_map<unsigned, EncodedGlyphInfo> glyphCache;
};

struct rlhbTextRun {
  rlhbFont *font = nullptr;
  float fontSize = 0.0f;
  rlhbTextAlign align = rlhbTextAlignLeft;
  std::vector<PositionedGlyph> glyphs;
  rlhbRunMetrics metrics = {};
};

namespace {

static void DefaultLogCallback(int level, const char *message, void *userData) {
  (void)userData;

  if (message == nullptr || message[0] == '\0') {
    return;
  }

  TraceLog(level, "rlhb: %s", message);
}

struct LogState {
  rlhbLogCallback callback = DefaultLogCallback;
  void *userData = nullptr;
};

static LogState gLogState = {};

static void LogMessage(int level, const char *message) {
  if (message == nullptr || message[0] == '\0') {
    return;
  }

  if (gLogState.callback != nullptr) {
    gLogState.callback(level, message, gLogState.userData);
  }
}

static void LogMessage(int level, const std::string &message) {
  LogMessage(level, message.c_str());
}

static void LogError(const char *message) {
  LogMessage(LOG_ERROR, message);
}

static void LogError(const std::string &message) {
  LogMessage(LOG_ERROR, message);
}

static void LogWarning(const char *message) {
  LogMessage(LOG_WARNING, message);
}

static bool EnsureVertexBufferCapacity(rlhbRenderer *renderer, size_t requiredVertices) {
  if (requiredVertices <= renderer->vertexCapacity && renderer->vao != 0 && renderer->vbo != 0) {
    return true;
  }

  while (renderer->vertexCapacity < requiredVertices) {
    renderer->vertexCapacity *= 2;
  }

  if (renderer->vbo != 0) {
    rlUnloadVertexBuffer(renderer->vbo);
    renderer->vbo = 0;
  }
  if (renderer->vao == 0) {
    renderer->vao = rlLoadVertexArray();
  }
  renderer->vbo = rlLoadVertexBuffer(nullptr,
                                     static_cast<int>(renderer->vertexCapacity * sizeof(GlyphVertex)),
                                     true);

  if (renderer->vao == 0 || renderer->vbo == 0) {
    LogError("Failed to create rlgl vertex resources for glyph rendering.");
    return false;
  }

  rlEnableVertexArray(renderer->vao);
  rlEnableVertexBuffer(renderer->vbo);

  const GLsizei stride = static_cast<GLsizei>(sizeof(GlyphVertex));
  const int positionLoc = rlGetLocationAttrib(renderer->program, "a_position");
  const int texcoordLoc = rlGetLocationAttrib(renderer->program, "a_texcoord");
  const int normalLoc = rlGetLocationAttrib(renderer->program, "a_normal");
  const int emPerPosLoc = rlGetLocationAttrib(renderer->program, "a_emPerPos");
  const int glyphLoc = rlGetLocationAttrib(renderer->program, "a_glyphLoc");

  rlEnableVertexAttribute(positionLoc);
  rlSetVertexAttribute(positionLoc, 2, RL_FLOAT, false, stride, offsetof(GlyphVertex, x));

  rlEnableVertexAttribute(texcoordLoc);
  rlSetVertexAttribute(texcoordLoc, 2, RL_FLOAT, false, stride, offsetof(GlyphVertex, tx));

  rlEnableVertexAttribute(normalLoc);
  rlSetVertexAttribute(normalLoc, 2, RL_FLOAT, false, stride, offsetof(GlyphVertex, nx));

  rlEnableVertexAttribute(emPerPosLoc);
  rlSetVertexAttribute(emPerPosLoc, 1, RL_FLOAT, false, stride, offsetof(GlyphVertex, emPerPos));

  rlEnableVertexAttribute(glyphLoc);
  rlSetVertexAttribute(glyphLoc, 1, RL_FLOAT, false, stride, offsetof(GlyphVertex, atlasOffset));

  rlDisableVertexBuffer();
  rlDisableVertexArray();
  return true;
}

static bool InitRenderer(rlhbRenderer *renderer) {
  static const char *kVertexMain = R"glsl(
uniform mat4 u_matViewProjection;
uniform vec2 u_viewport;

in vec2 a_position;
in vec2 a_texcoord;
in vec2 a_normal;
in float a_emPerPos;
in float a_glyphLoc;

out vec2 v_texcoord;
flat out uint v_glyphLoc;

void main ()
{
    vec2 pos = a_position;
    vec2 tex = a_texcoord;
    vec4 jac = vec4(a_emPerPos, 0.0, 0.0, -a_emPerPos);

    hb_gpu_dilate(pos, tex, a_normal, jac, u_matViewProjection, u_viewport);

    gl_Position = u_matViewProjection * vec4(pos, 0.0, 1.0);
    v_texcoord = tex;
    v_glyphLoc = uint(a_glyphLoc + 0.5);
}
)glsl";

  static const char *kFragmentMain = R"glsl(
uniform float u_gamma;
uniform float u_debug;
uniform float u_stem_darkening;
uniform vec4 u_foreground;

in vec2 v_texcoord;
flat in uint v_glyphLoc;

out vec4 fragColor;

void main ()
{
    float coverage = hb_gpu_render(v_texcoord, v_glyphLoc);

    if (u_stem_darkening > 0.0)
    {
        coverage = hb_gpu_darken(
            coverage,
            dot(u_foreground.rgb, vec3(1.0 / 3.0)),
            hb_gpu_ppem(v_texcoord, v_glyphLoc)
        );
    }

    if (u_gamma != 1.0) coverage = pow(coverage, u_gamma);

    if (u_debug > 0.0)
    {
        ivec2 counts = _hb_gpu_curve_counts(v_texcoord, v_glyphLoc);
        float r = clamp(float(counts.x) / 8.0, 0.0, 1.0);
        float g = clamp(float(counts.y) / 8.0, 0.0, 1.0);
        fragColor = vec4(r, g, coverage, max(max(r, g), coverage));
        return;
    }

    fragColor = vec4(u_foreground.rgb, u_foreground.a * coverage);
}
)glsl";

  if (!IsWindowReady()) {
    LogError("Create the renderer after InitWindow so an OpenGL context is available.");
    return false;
  }

  const std::string vertexSource = JoinShaderSource({
      "#version 330\n",
      hb_gpu_shader_vertex_source(HB_GPU_SHADER_LANG_GLSL),
      kVertexMain,
  });
  const std::string fragmentSource = JoinShaderSource({
      "#version 330\n",
      hb_gpu_shader_fragment_source(HB_GPU_SHADER_LANG_GLSL),
      kFragmentMain,
  });

  renderer->program = rlLoadShaderCode(vertexSource.c_str(), fragmentSource.c_str());
  if (renderer->program == 0) {
    LogError("Failed to compile HarfBuzz GPU shaders.");
    return false;
  }

  renderer->locMvp = rlGetLocationUniform(renderer->program, "u_matViewProjection");
  renderer->locViewport = rlGetLocationUniform(renderer->program, "u_viewport");
  renderer->locGamma = rlGetLocationUniform(renderer->program, "u_gamma");
  renderer->locForeground = rlGetLocationUniform(renderer->program, "u_foreground");
  renderer->locStemDarkening = rlGetLocationUniform(renderer->program, "u_stem_darkening");
  renderer->locDebug = rlGetLocationUniform(renderer->program, "u_debug");
  renderer->locAtlas = rlGetLocationUniform(renderer->program, "hb_gpu_atlas");

  if (!renderer->atlas.Init(kAtlasCapacityTexels)) {
    return false;
  }
  if (!EnsureVertexBufferCapacity(renderer, renderer->vertexCapacity)) {
    return false;
  }

  renderer->ready = true;
  return true;
}

static void ShutdownRenderer(rlhbRenderer *renderer) {
  if (renderer->vbo != 0) {
    rlUnloadVertexBuffer(renderer->vbo);
  }
  if (renderer->vao != 0) {
    rlUnloadVertexArray(renderer->vao);
  }
  if (renderer->program != 0) {
    rlUnloadShaderProgram(renderer->program);
  }

  renderer->vbo = 0;
  renderer->vao = 0;
  renderer->program = 0;
  renderer->atlas.Shutdown();
  renderer->scratchVertices.clear();
  renderer->ready = false;
}

static bool EnsureRendererReady(rlhbRenderer *renderer) {
  if (renderer == nullptr) {
    return false;
  }
  if (renderer->ready) {
    return true;
  }
  return InitRenderer(renderer);
}

static void UnloadFont(rlhbFont *font) {
  if (font == nullptr) {
    return;
  }
  if (font->draw != nullptr) {
    hb_gpu_draw_destroy(font->draw);
  }
  if (font->font != nullptr) {
    hb_font_destroy(font->font);
  }
  if (font->face != nullptr) {
    hb_face_destroy(font->face);
  }
  font->draw = nullptr;
  font->font = nullptr;
  font->face = nullptr;
  font->upem = 0;
  font->renderer = nullptr;
  font->fontPath.clear();
  font->ownedFontData.clear();
  font->glyphCache.clear();
}

static bool LoadFontFromBytesCopy(rlhbRenderer *renderer,
                                  rlhbFont *font,
                                  const void *fontData,
                                  size_t fontDataSize,
                                  const char *sourceName) {
  if (renderer == nullptr || font == nullptr || fontData == nullptr || fontDataSize == 0) {
    LogError("Valid font bytes are required.");
    return false;
  }

  if (!EnsureRendererReady(renderer)) {
    return false;
  }

  if (fontDataSize > static_cast<size_t>(std::numeric_limits<unsigned int>::max())) {
    LogError("Font data is too large for HarfBuzz blob creation.");
    return false;
  }

  UnloadFont(font);

  const unsigned char *sourceBytes = static_cast<const unsigned char *>(fontData);
  font->ownedFontData.assign(sourceBytes, sourceBytes + fontDataSize);

  hb_blob_t *blob = hb_blob_create(reinterpret_cast<const char *>(font->ownedFontData.data()),
                                   static_cast<unsigned int>(font->ownedFontData.size()),
                                   HB_MEMORY_MODE_READONLY,
                                   nullptr,
                                   nullptr);
  font->face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);

  if (font->face == nullptr) {
    LogError("Failed to create a HarfBuzz face from the font data.");
    UnloadFont(font);
    return false;
  }

  font->font = hb_font_create(font->face);
  hb_ot_font_set_funcs(font->font);
  font->upem = hb_face_get_upem(font->face);
  hb_font_set_scale(font->font, static_cast<int>(font->upem), static_cast<int>(font->upem));

  font->draw = hb_gpu_draw_create_or_fail();
  if (font->draw == nullptr) {
    LogError("Failed to create the HarfBuzz GPU glyph encoder.");
    UnloadFont(font);
    return false;
  }

  font->renderer = renderer;
  font->fontPath = sourceName != nullptr ? std::filesystem::path(sourceName) : std::filesystem::path();
  return true;
}

static bool LoadFont(rlhbRenderer *renderer, rlhbFont *font, const char *filePath) {
  if (renderer == nullptr || font == nullptr || filePath == nullptr || filePath[0] == '\0') {
    LogError("A valid font file path is required.");
    return false;
  }

  int fileSize = 0;
  unsigned char *fileData = LoadFileData(filePath, &fileSize);
  if (fileData == nullptr || fileSize <= 0) {
    LogError(std::string("Failed to load font file: ") + filePath);
    return false;
  }

  const bool loaded = LoadFontFromBytesCopy(renderer,
                                            font,
                                            fileData,
                                            static_cast<size_t>(fileSize),
                                            filePath);
  UnloadFileData(fileData);
  return loaded;
}

static const EncodedGlyphInfo *GetGlyph(rlhbFont *font, unsigned glyphIndex) {
  auto found = font->glyphCache.find(glyphIndex);
  if (found != font->glyphCache.end()) {
    return &found->second;
  }

  EncodedGlyphInfo info = {};

  hb_gpu_draw_reset(font->draw);
  hb_gpu_draw_glyph(font->draw, font->font, glyphIndex);

  hb_blob_t *encoded = hb_gpu_draw_encode(font->draw);
  if (encoded == nullptr) {
    LogError("Failed to encode a glyph with the HarfBuzz GPU API.");
    return nullptr;
  }

  hb_glyph_extents_t extents = {};
  hb_gpu_draw_get_extents(font->draw, &extents);

  info.minX = static_cast<float>(extents.x_bearing);
  info.maxX = static_cast<float>(extents.x_bearing + extents.width);
  info.maxY = static_cast<float>(extents.y_bearing);
  info.minY = static_cast<float>(extents.y_bearing + extents.height);
  info.advance = static_cast<float>(hb_font_get_glyph_h_advance(font->font, glyphIndex));
  info.upem = font->upem;
  info.empty = hb_blob_get_length(encoded) == 0;

  if (!info.empty) {
    info.atlasOffset = font->renderer->atlas.Allocate(hb_blob_get_data(encoded, nullptr),
                                                      hb_blob_get_length(encoded));
    if (info.atlasOffset == kInvalidAtlasOffset) {
      hb_gpu_draw_recycle_blob(font->draw, encoded);
      return nullptr;
    }
  }

  hb_gpu_draw_recycle_blob(font->draw, encoded);

  auto inserted = font->glyphCache.emplace(glyphIndex, info);
  return &inserted.first->second;
}

static bool ShapeTextImpl(rlhbRenderer *renderer,
                          rlhbFont *font,
                          const char *text,
                          size_t length,
                          const rlhbShapeOptions *options,
                          std::unique_ptr<rlhbTextRun> *outRun) {
  if (renderer == nullptr || font == nullptr || outRun == nullptr) {
    LogError("Renderer, font, and output run are required.");
    return false;
  }

  if (text == nullptr && length != 0) {
    LogError("Text data is null but the supplied length is non-zero.");
    return false;
  }

  if (!EnsureRendererReady(renderer)) {
    return false;
  }

  if (font->renderer != renderer) {
    LogError("The font belongs to a different renderer instance.");
    return false;
  }

  if (length > static_cast<size_t>(std::numeric_limits<int>::max())) {
    LogError("Text length exceeds HarfBuzz's supported UTF-8 input range.");
    return false;
  }

  rlhbShapeOptions resolved = rlhbGetDefaultShapeOptions();
  if (options != nullptr) {
    resolved = *options;
    if (resolved.fontSize <= 0.0f) {
      resolved.fontSize = 48.0f;
    }
  }

  std::unique_ptr<rlhbTextRun> run(new rlhbTextRun());
  run->font = font;
  run->fontSize = resolved.fontSize;
  run->align = resolved.align;

  hb_buffer_t *buffer = hb_buffer_create();
  if (buffer == nullptr) {
    LogError("Failed to create a HarfBuzz buffer.");
    return false;
  }

  hb_buffer_add_utf8(buffer, text != nullptr ? text : "", static_cast<int>(length), 0, static_cast<int>(length));
  hb_buffer_guess_segment_properties(buffer);

  const hb_direction_t direction = ToHbDirection(resolved.direction);
  if (direction != HB_DIRECTION_INVALID) {
    hb_buffer_set_direction(buffer, direction);
  }
  if (resolved.script != nullptr && resolved.script[0] != '\0') {
    hb_buffer_set_script(buffer, hb_script_from_string(resolved.script, -1));
  }
  if (resolved.language != nullptr && resolved.language[0] != '\0') {
    hb_buffer_set_language(buffer, hb_language_from_string(resolved.language, -1));
  }

  hb_shape(font->font, buffer, nullptr, 0);

  unsigned glyphCount = 0;
  hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &glyphCount);
  hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, nullptr);

  run->glyphs.reserve(glyphCount);

  const float scale = resolved.fontSize / static_cast<float>(font->upem);
  float cursorX = 0.0f;
  float cursorY = 0.0f;
  bool haveBounds = false;
  float minX = 0.0f;
  float minY = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;

  for (unsigned index = 0; index < glyphCount; ++index) {
    PositionedGlyph positioned = {};
    positioned.glyphIndex = infos[index].codepoint;
    positioned.xOffset = static_cast<float>(positions[index].x_offset);
    positioned.yOffset = static_cast<float>(positions[index].y_offset);
    positioned.xAdvance = static_cast<float>(positions[index].x_advance);
    positioned.yAdvance = static_cast<float>(positions[index].y_advance);
    run->glyphs.push_back(positioned);

    const EncodedGlyphInfo *glyph = GetGlyph(font, positioned.glyphIndex);
    if (glyph == nullptr) {
      hb_buffer_destroy(buffer);
      return false;
    }

    if (!glyph->empty) {
      const float left = scale * (cursorX + positioned.xOffset + glyph->minX);
      const float right = scale * (cursorX + positioned.xOffset + glyph->maxX);
      const float top = -scale * (cursorY + positioned.yOffset + glyph->maxY);
      const float bottom = -scale * (cursorY + positioned.yOffset + glyph->minY);

      if (!haveBounds) {
        minX = left;
        maxX = right;
        minY = top;
        maxY = bottom;
        haveBounds = true;
      } else {
        minX = std::min(minX, left);
        maxX = std::max(maxX, right);
        minY = std::min(minY, top);
        maxY = std::max(maxY, bottom);
      }
    }

    cursorX += positioned.xAdvance;
    cursorY += positioned.yAdvance;
  }

  hb_buffer_destroy(buffer);

  run->metrics.width = scale * cursorX;
  run->metrics.glyphCount = static_cast<int>(run->glyphs.size());

  if (haveBounds) {
    run->metrics.bounds = Rectangle{
        minX,
        minY,
        maxX - minX,
        maxY - minY,
    };
    run->metrics.ascent = std::max(0.0f, -minY);
    run->metrics.descent = std::max(0.0f, maxY);
  } else {
    run->metrics.bounds = Rectangle{0.0f, 0.0f, 0.0f, 0.0f};
    run->metrics.ascent = 0.0f;
    run->metrics.descent = 0.0f;
  }

  *outRun = std::move(run);
  return true;
}

static bool BuildVertices(rlhbRenderer *renderer, const rlhbTextRun *run, Vector2 baseline) {
  if (renderer == nullptr || run == nullptr || run->font == nullptr) {
    LogError("A valid renderer and shaped run are required for drawing.");
    return false;
  }

  if (!EnsureRendererReady(renderer)) {
    return false;
  }

  if (run->font->renderer != renderer) {
    LogError("The shaped run belongs to a different renderer instance.");
    return false;
  }

  renderer->scratchVertices.clear();
  renderer->scratchVertices.reserve(run->glyphs.size() * 6u);

  if (!EnsureVertexBufferCapacity(renderer, run->glyphs.size() * 6u)) {
    return false;
  }

  float anchorX = baseline.x;
  if (run->align == rlhbTextAlignCenter) {
    anchorX -= run->metrics.width * 0.5f;
  } else if (run->align == rlhbTextAlignRight) {
    anchorX -= run->metrics.width;
  }

  const float scale = run->fontSize / static_cast<float>(run->font->upem);
  Vector2 cursor = {anchorX, baseline.y};

  for (const PositionedGlyph &positioned : run->glyphs) {
    const EncodedGlyphInfo *glyph = GetGlyph(run->font, positioned.glyphIndex);
    if (glyph == nullptr) {
      return false;
    }

    Vector2 pen = cursor;
    pen.x += scale * positioned.xOffset;
    pen.y -= scale * positioned.yOffset;

    AppendGlyphVertices(pen, run->fontSize, *glyph, &renderer->scratchVertices);

    cursor.x += scale * positioned.xAdvance;
    cursor.y -= scale * positioned.yAdvance;
  }

  return true;
}

static bool BeginDrawScope(rlhbRenderer *renderer) {
  if (renderer == nullptr) {
    LogError("A valid renderer is required to begin text drawing.");
    return false;
  }

  if (!EnsureRendererReady(renderer)) {
    return false;
  }

  if (renderer->drawScopeDepth > 0) {
    renderer->drawScopeDepth += 1;
    return true;
  }

  renderer->drawScopeViewportWidth = GetScreenWidth();
  renderer->drawScopeViewportHeight = GetScreenHeight();

  const Matrix mvp = MatrixOrtho(0.0,
                                 static_cast<double>(renderer->drawScopeViewportWidth),
                                 static_cast<double>(renderer->drawScopeViewportHeight),
                                 0.0,
                                 -1.0,
                                 1.0);

  rlDrawRenderBatchActive();
  rlViewport(0, 0, renderer->drawScopeViewportWidth, renderer->drawScopeViewportHeight);
  rlDisableDepthTest();
  rlDisableBackfaceCulling();
  rlDisableScissorTest();
  rlEnableColorBlend();
  rlSetBlendMode(BLEND_ALPHA);
  rlEnableShader(renderer->program);
  rlSetUniformMatrix(renderer->locMvp, mvp);

  const float viewport[2] = {
      static_cast<float>(renderer->drawScopeViewportWidth),
      static_cast<float>(renderer->drawScopeViewportHeight),
  };
  const float debugValue = renderer->debug ? 1.0f : 0.0f;
  const float stemDarkening = renderer->stemDarkening ? 1.0f : 0.0f;

  rlSetUniform(renderer->locViewport, viewport, RL_SHADER_UNIFORM_VEC2, 1);
  rlSetUniform(renderer->locGamma, &renderer->gamma, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(renderer->locDebug, &debugValue, RL_SHADER_UNIFORM_FLOAT, 1);
  rlSetUniform(renderer->locStemDarkening, &stemDarkening, RL_SHADER_UNIFORM_FLOAT, 1);

  renderer->drawScopeDepth = 1;
  return true;
}

static void EndDrawScope(rlhbRenderer *renderer) {
  if (renderer == nullptr || renderer->drawScopeDepth == 0) {
    return;
  }

  renderer->drawScopeDepth -= 1;
  if (renderer->drawScopeDepth != 0) {
    return;
  }

  rlDisableVertexArray();
  rlDisableVertexBuffer();
  rlDisableShader();
  rlSetBlendMode(BLEND_ALPHA);
  glBindTexture(GL_TEXTURE_BUFFER, 0);

  renderer->drawScopeViewportWidth = 0;
  renderer->drawScopeViewportHeight = 0;
}

static bool RenderVertices(rlhbRenderer *renderer, Color tint) {
  if (renderer == nullptr || renderer->drawScopeDepth == 0) {
    LogError("Text drawing requires an active draw scope.");
    return false;
  }

  if (renderer->scratchVertices.empty()) {
    return true;
  }

  const float foreground[4] = {
      tint.r / 255.0f,
      tint.g / 255.0f,
      tint.b / 255.0f,
      tint.a / 255.0f,
  };

  rlSetUniform(renderer->locForeground, foreground, RL_SHADER_UNIFORM_VEC4, 1);

  renderer->atlas.Bind(renderer->locAtlas);
  rlEnableVertexArray(renderer->vao);
  rlUpdateVertexBuffer(renderer->vbo,
                       renderer->scratchVertices.data(),
                       static_cast<int>(renderer->scratchVertices.size() * sizeof(GlyphVertex)),
                       0);
  rlDrawVertexArray(0, static_cast<int>(renderer->scratchVertices.size()));
  rlDisableVertexArray();
  rlDisableVertexBuffer();
  glBindTexture(GL_TEXTURE_BUFFER, 0);
  return true;
}

}  // namespace

extern "C" {

rlhbShapeOptions rlhbGetDefaultShapeOptions(void) {
  rlhbShapeOptions options = {};
  options.fontSize = 48.0f;
  options.direction = rlhbDirectionAuto;
  options.align = rlhbTextAlignLeft;
  options.language = nullptr;
  options.script = nullptr;
  return options;
}

void rlhbSetLogCallback(rlhbLogCallback callback, void *userData) {
  if (callback == nullptr) {
    gLogState.callback = DefaultLogCallback;
    gLogState.userData = nullptr;
    return;
  }

  gLogState.callback = callback;
  gLogState.userData = userData;
}

rlhbRenderer *rlhbCreateRenderer(void) {
  std::unique_ptr<rlhbRenderer> renderer(new rlhbRenderer());
  if (IsWindowReady()) {
    InitRenderer(renderer.get());
  } else {
    LogWarning("Renderer created before InitWindow. The first text operation will retry initialization.");
  }
  return renderer.release();
}

void rlhbDestroyRenderer(rlhbRenderer *renderer) {
  if (renderer == nullptr) {
    return;
  }
  ShutdownRenderer(renderer);
  delete renderer;
}

bool rlhbIsRendererReady(const rlhbRenderer *renderer) {
  return renderer != nullptr && renderer->ready;
}

float rlhbGetAtlasUsageKiB(const rlhbRenderer *renderer) {
  if (renderer == nullptr) {
    return 0.0f;
  }
  return renderer->atlas.UsedKiB();
}

rlhbFont *rlhbLoadFontFromFile(rlhbRenderer *renderer, const char *filePath) {
  std::unique_ptr<rlhbFont> font(new rlhbFont());
  if (!LoadFont(renderer, font.get(), filePath)) {
    return nullptr;
  }
  return font.release();
}

rlhbFont *rlhbLoadDefaultFont(rlhbRenderer *renderer) {
#if defined(RLHB_HAS_DEFAULT_FONT)
  std::unique_ptr<rlhbFont> font(new rlhbFont());
  if (!LoadFontFromBytesCopy(renderer,
                             font.get(),
                             rlhb_embedded::rlhbDefaultFontData,
                             rlhb_embedded::rlhbDefaultFontData_len,
                             rlhb_embedded::rlhbDefaultFontName)) {
    return nullptr;
  }
  return font.release();
#else
  (void)renderer;
  LogWarning("This build was configured without a bundled default font.");
  return nullptr;
#endif
}

void rlhbUnloadFont(rlhbFont *font) {
  if (font == nullptr) {
    return;
  }
  UnloadFont(font);
  delete font;
}

int rlhbGetCachedGlyphCount(const rlhbFont *font) {
  if (font == nullptr) {
    return 0;
  }
  return static_cast<int>(font->glyphCache.size());
}

bool rlhbShapeTextN(rlhbRenderer *renderer,
                    rlhbFont *font,
                    const char *text,
                    size_t length,
                    const rlhbShapeOptions *options,
                    rlhbTextRun **outRun) {
  if (outRun == nullptr) {
    (void)renderer;
    LogError("An output run pointer is required.");
    return false;
  }

  std::unique_ptr<rlhbTextRun> run;
  if (!ShapeTextImpl(renderer, font, text, length, options, &run)) {
    *outRun = nullptr;
    return false;
  }

  *outRun = run.release();
  return true;
}

bool rlhbShapeText(rlhbRenderer *renderer,
                   rlhbFont *font,
                   const char *text,
                   const rlhbShapeOptions *options,
                   rlhbTextRun **outRun) {
  if (text == nullptr) {
    return rlhbShapeTextN(renderer, font, nullptr, 0, options, outRun);
  }
  return rlhbShapeTextN(renderer, font, text, std::strlen(text), options, outRun);
}

void rlhbDestroyTextRun(rlhbTextRun *run) {
  delete run;
}

rlhbRunMetrics rlhbGetTextRunMetrics(const rlhbTextRun *run) {
  if (run == nullptr) {
    rlhbRunMetrics metrics = {};
    return metrics;
  }
  return run->metrics;
}

bool rlhbBeginDraw(rlhbRenderer *renderer) {
  return BeginDrawScope(renderer);
}

void rlhbEndDraw(rlhbRenderer *renderer) {
  EndDrawScope(renderer);
}

bool rlhbDrawTextRun(rlhbRenderer *renderer,
                     const rlhbTextRun *run,
                     Vector2 baseline,
                     Color tint) {
  if (!BuildVertices(renderer, run, baseline)) {
    return false;
  }

  bool ownsScope = false;
  if (renderer != nullptr && renderer->drawScopeDepth == 0) {
    if (!BeginDrawScope(renderer)) {
      return false;
    }
    ownsScope = true;
  }

  const bool rendered = RenderVertices(renderer, tint);

  if (ownsScope) {
    EndDrawScope(renderer);
  }

  return rendered;
}

bool rlhbDrawTextN(rlhbRenderer *renderer,
                   rlhbFont *font,
                   const char *text,
                   size_t length,
                   Vector2 baseline,
                   Color tint,
                   const rlhbShapeOptions *options) {
  std::unique_ptr<rlhbTextRun> run;
  if (!ShapeTextImpl(renderer, font, text, length, options, &run)) {
    return false;
  }
  return rlhbDrawTextRun(renderer, run.get(), baseline, tint);
}

bool rlhbDrawText(rlhbRenderer *renderer,
                  rlhbFont *font,
                  const char *text,
                  Vector2 baseline,
                  Color tint,
                  const rlhbShapeOptions *options) {
  if (text == nullptr) {
    return rlhbDrawTextN(renderer, font, nullptr, 0, baseline, tint, options);
  }
  return rlhbDrawTextN(renderer, font, text, std::strlen(text), baseline, tint, options);
}

rlhbRunMetrics rlhbMeasureTextN(rlhbRenderer *renderer,
                                rlhbFont *font,
                                const char *text,
                                size_t length,
                                const rlhbShapeOptions *options) {
  std::unique_ptr<rlhbTextRun> run;
  rlhbRunMetrics metrics = {};
  if (!ShapeTextImpl(renderer, font, text, length, options, &run)) {
    return metrics;
  }
  return run->metrics;
}

rlhbRunMetrics rlhbMeasureText(rlhbRenderer *renderer,
                               rlhbFont *font,
                               const char *text,
                               const rlhbShapeOptions *options) {
  if (text == nullptr) {
    return rlhbMeasureTextN(renderer, font, nullptr, 0, options);
  }
  return rlhbMeasureTextN(renderer, font, text, std::strlen(text), options);
}

}  // extern "C"
