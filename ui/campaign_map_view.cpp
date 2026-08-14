#include "campaign_map_view.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QTextStream>
#include <QVariantMap>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QtGlobal>
#include <QtMath>

#include <cmath>
#include <cstring>
#include <numbers>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../game/util/asset_text.h"
#include "../render/gl/shader.h"
#include "../utils/resource_utils.h"
#include "campaign_map_render_utils.h"

namespace {

auto load_shader_source(const QString& resource_path, QString* out_source) -> bool {
  const QString resolved = Utils::Resources::resolve_resource_path(resource_path);
  QFile file(resolved);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "CampaignMapRenderer: Failed to open shader" << resolved;
    return false;
  }
  QTextStream stream(&file);
  *out_source = Render::GL::Shader::preprocess_source(stream.readAll());
  return true;
}

auto build_mvp_matrix(float width,
                      float height,
                      float yaw_deg,
                      float pitch_deg,
                      float distance,
                      float pan_u,
                      float pan_v) -> QMatrix4x4 {
  const float view_w = qMax(1.0F, width);
  const float view_h = qMax(1.0F, height);
  const float aspect = view_w / view_h;

  QMatrix4x4 projection;
  projection.perspective(45.0F, aspect, 0.1F, 10.0F);

  const float clamped_pan_u = qBound(-0.5F, pan_u, 0.5F);
  const float clamped_pan_v = qBound(-0.5F, pan_v, 0.5F);
  const QVector3D center(0.5F + clamped_pan_u, 0.0F, 0.5F + clamped_pan_v);
  const float yaw_rad = qDegreesToRadians(yaw_deg);
  const float pitch_rad = qDegreesToRadians(pitch_deg);
  const float clamped_distance = qMax(CampaignMapView::k_min_orbit_distance, distance);

  const float cos_pitch = std::cos(pitch_rad);
  const float sin_pitch = std::sin(pitch_rad);
  const float cos_yaw = std::cos(yaw_rad);
  const float sin_yaw = std::sin(yaw_rad);

  QVector3D eye(center.x() + clamped_distance * sin_yaw * cos_pitch,
                center.y() + clamped_distance * sin_pitch,
                center.z() + clamped_distance * cos_yaw * cos_pitch);

  QMatrix4x4 view;
  view.lookAt(eye, center, QVector3D(0.0F, 0.0F, 1.0F));

  QMatrix4x4 model;
  return projection * view * model;
}

auto point_in_triangle(const QVector2D& p,
                       const QVector2D& a,
                       const QVector2D& b,
                       const QVector2D& c) -> bool {
  const QVector2D v0 = c - a;
  const QVector2D v1 = b - a;
  const QVector2D v2 = p - a;

  const float dot00 = QVector2D::dotProduct(v0, v0);
  const float dot01 = QVector2D::dotProduct(v0, v1);
  const float dot02 = QVector2D::dotProduct(v0, v2);
  const float dot11 = QVector2D::dotProduct(v1, v1);
  const float dot12 = QVector2D::dotProduct(v1, v2);

  const float denom = dot00 * dot11 - dot01 * dot01;
  if (qFuzzyIsNull(denom)) {
    return false;
  }
  const float inv_denom = 1.0F / denom;
  const float u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
  const float v = (dot00 * dot12 - dot01 * dot02) * inv_denom;
  return (u >= 0.0F) && (v >= 0.0F) && (u + v <= 1.0F);
}

constexpr int k_outline_steps = 12;
constexpr float k_pick_tolerance_px = 16.0F;
constexpr float k_pick_fallback_tolerance_uv = 0.012F;

auto distance_point_segment(const QVector2D& p,
                            const QVector2D& a,
                            const QVector2D& b) -> float {
  const QVector2D ab = b - a;
  const float len_sq = QVector2D::dotProduct(ab, ab);
  if (len_sq <= 1e-12F) {
    return (p - a).length();
  }
  const float t = qBound(0.0F, QVector2D::dotProduct(p - a, ab) / len_sq, 1.0F);
  return (p - (a + ab * t)).length();
}

auto distance_to_triangle(const QVector2D& p,
                          const QVector2D& a,
                          const QVector2D& b,
                          const QVector2D& c) -> float {
  if (point_in_triangle(p, a, b, c)) {
    return 0.0F;
  }
  return qMin(distance_point_segment(p, a, b),
              qMin(distance_point_segment(p, b, c), distance_point_segment(p, c, a)));
}

auto distance_to_bounds(const QVector2D& p,
                        const QVector2D& lo,
                        const QVector2D& hi) -> float {
  const float dx = qMax(qMax(lo.x() - p.x(), p.x() - hi.x()), 0.0F);
  const float dy = qMax(qMax(lo.y() - p.y(), p.y() - hi.y()), 0.0F);
  return std::sqrt(dx * dx + dy * dy);
}

struct LineSpan {
  int start = 0;
  int count = 0;
};

struct LineLayer {
  GLuint vao = 0;
  GLuint vbo = 0;
  std::vector<LineSpan> spans;
  QVector4D color = QVector4D(1.0F, 1.0F, 1.0F, 1.0F);
  float width = 1.0F;
  bool ready = false;

  bool double_stroke = false;
  QVector4D outer_color = QVector4D(0.12F, 0.10F, 0.08F, 0.95F);
  QVector4D inner_color = QVector4D(0.55F, 0.50F, 0.42F, 0.75F);
  float outer_width_ratio = 1.8F;
  float inner_width_ratio = 1.0F;

  bool clip_to_land = false;
};

struct StrokeMesh {
  GLuint vao = 0;
  GLuint vbo = 0;
  int vertex_count = 0;
  float width = 0.0F;
  bool ready = false;
};

struct PathLine {
  std::vector<QVector2D> points;
  StrokeMesh border;
  StrokeMesh highlight;
  StrokeMesh core;
};

struct PathLayer {
  std::vector<PathLine> lines;
  bool ready = false;
};

struct ProvinceSpan {
  int start = 0;
  int count = 0;
  QVector4D color = QVector4D(0.0F, 0.0F, 0.0F, 0.0F);
  QVector4D base_color = QVector4D(0.0F, 0.0F, 0.0F, 0.0F);
  QString id;
};

struct ProvinceLayer {
  GLuint vao = 0;
  GLuint vbo = 0;
  std::vector<ProvinceSpan> spans;
  bool ready = false;
};

struct MissionBadge {
  GLuint vao = 0;
  GLuint vbo = 0;
  int vertex_count = 0;
  QVector2D position;
  bool ready = false;

  int style = 3;
  QVector4D primary_color = QVector4D(0.75F, 0.18F, 0.12F, 1.0F);
  QVector4D secondary_color = QVector4D(0.95F, 0.85F, 0.45F, 1.0F);
  QVector4D border_color = QVector4D(0.15F, 0.10F, 0.08F, 1.0F);
  QVector4D shadow_color = QVector4D(0.0F, 0.0F, 0.0F, 0.4F);
  float size = 0.025F;
  float border_width = 2.0F;
  bool show_shadow = true;
};

struct CartographicSymbolInstance {
  QVector2D position;
  int symbol_type;
  float size;
  int importance;
};

struct CartographicSymbolLayer {
  GLuint vao = 0;
  GLuint vbo = 0;
  std::vector<CartographicSymbolInstance> symbols;
  std::vector<LineSpan> spans;
  int total_vertices = 0;
  bool ready = false;

  QVector4D fill_color = QVector4D(0.35F, 0.30F, 0.25F, 0.85F);
  QVector4D stroke_color = QVector4D(0.18F, 0.15F, 0.12F, 0.95F);
  QVector4D shadow_color = QVector4D(0.0F, 0.0F, 0.0F, 0.35F);
  float stroke_width = 1.5F;
  bool show_shadow = true;
};

struct TerrainMesh {
  GLuint vao = 0;
  GLuint vbo = 0;
  int vertex_count = 0;
  bool ready = false;

  float height_scale = 0.05F;
  float z_base = 0.0F;
};

struct HillshadeLayer {
  GLuint texture_id = 0;
  int width = 0;
  int height = 0;
  bool ready = false;

  QVector3D light_direction{0.35F, 0.85F, 0.40F};
  float ambient = 0.25F;
  float intensity = 1.0F;
};

struct HeightmapData {
  std::vector<float> samples;
  int width = 0;
  int height = 0;
  float min_m = 0.0F;
  float max_m = 0.0F;
  bool ready = false;
};

struct ProvinceLabel {
  QString text;
  QVector2D position;
  int importance;
  bool is_sea = false;
};

struct LabelLayer {
  GLuint vao = 0;
  GLuint vbo = 0;
  std::vector<ProvinceLabel> labels;
  int total_vertices = 0;
  bool ready = false;

  GLuint font_texture = 0;
};

struct QStringHash {
  std::size_t operator()(const QString& s) const noexcept { return qHash(s); }
};

struct CampaignMapTextureCache {
  static CampaignMapTextureCache& instance() {
    static CampaignMapTextureCache s_instance;
    return s_instance;
  }

  QOpenGLTexture* get_or_load(const QString& resource_path) {

    if (!m_allow_loading) {
      qWarning() << "CampaignMapTextureCache: Attempted to load texture after "
                    "initialization:"
                 << resource_path;
      return nullptr;
    }

    auto it = m_textures.find(resource_path);
    if (it != m_textures.end() && it->second != nullptr) {
      return it->second;
    }

    const QString path = Utils::Resources::resolve_resource_path(resource_path);
    QImage image(path);
    if (image.isNull()) {
      qWarning() << "CampaignMapTextureCache: Failed to load texture" << path;
      return nullptr;
    }

    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    auto* texture = new QOpenGLTexture(rgba);
    texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    m_textures[resource_path] = texture;
    return texture;
  }

  void set_loading_allowed(bool allowed) { m_allow_loading = allowed; }

  void clear() {

    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (ctx != nullptr && ctx->isValid()) {
      for (auto& pair : m_textures) {
        delete pair.second;
      }
    }
    m_textures.clear();
  }

private:
  CampaignMapTextureCache() = default;
  ~CampaignMapTextureCache() { clear(); }
  std::unordered_map<QString, QOpenGLTexture*, QStringHash> m_textures;
  bool m_allow_loading = true;
};

} // namespace

class CampaignMapRenderer : public QQuickFramebufferObject::Renderer,
                            protected QOpenGLFunctions_3_3_Core {
public:
  CampaignMapRenderer() = default;
  ~CampaignMapRenderer() override { cleanup(); }

  void render() override {
    if (!ensure_initialized()) {
      return;
    }

    glViewport(0, 0, m_size.width(), m_size.height());
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.157F, 0.267F, 0.361F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    QMatrix4x4 mvp;
    compute_mvp(mvp);

    draw_textured_layer(m_water_texture, m_ocean_vao, 6, mvp, 1.0F, -0.01F);
    if (m_terrain_mesh.ready && m_terrain_program.isLinked()) {
      draw_terrain_layer(m_terrain_mesh, mvp);
    } else if (m_land_vertex_count > 0) {
      draw_textured_layer(
          m_base_texture, m_land_vao, m_land_vertex_count, mvp, 1.0F, 0.0F);
    } else {
      draw_textured_layer(m_base_texture, m_quad_vao, 6, mvp, 1.0F, 0.0F);
    }

    glDisable(GL_DEPTH_TEST);
    if (m_show_province_fills) {
      draw_province_layer(m_province_layer, mvp, 0.002F);
    }
    draw_line_layer(m_province_border_layer, mvp, 0.0045F);
    draw_line_layer(m_coast_layer, mvp, 0.004F);
    draw_line_layer(m_river_layer, mvp, 0.003F);
    draw_progressive_path_layers(m_path_layer, mvp, 0.006F);

    draw_symbol_layer(m_symbol_layer, mvp, 0.007F);

    draw_mission_badge(m_mission_badge, mvp, 0.008F);

    draw_label_layer(m_label_layer, mvp, 0.009F);

    if (!m_hover_province_id.isEmpty()) {
      qint64 now = QDateTime::currentMSecsSinceEpoch();
      if (now - m_last_update_time >= 16) {
        m_last_update_time = now;
        update();
      }
    }
  }

  auto
  createFramebufferObject(const QSize& size) -> QOpenGLFramebufferObject* override {
    m_size = size;
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

    fmt.setSamples(4);
    auto* target = new QOpenGLFramebufferObject(size, fmt);
    if (!target->isValid()) {
      delete target;
      fmt.setSamples(0);
      target = new QOpenGLFramebufferObject(size, fmt);
    }
    return target;
  }

  void synchronize(QQuickFramebufferObject* item) override {
    auto* view = dynamic_cast<CampaignMapView*>(item);
    if (view == nullptr) {
      return;
    }

    m_orbit_yaw = view->orbit_yaw();
    m_orbit_pitch = view->orbit_pitch();
    m_orbit_distance = view->orbit_distance();
    m_pan_u = view->pan_u();
    m_pan_v = view->pan_v();
    m_terrain_height_scale = view->terrain_height_scale();
    m_show_province_fills = view->show_province_fills();

    QString new_hover_id = view->hover_province_id();
    if (m_hover_province_id != new_hover_id) {
      m_hover_start_time = QDateTime::currentMSecsSinceEpoch();
      m_hover_province_id = new_hover_id;
    }
    m_selected_province_id = view->selected_province_id();

    m_current_mission = view->current_mission();

    if (m_province_state_version != view->province_state_version() &&
        m_province_layer.ready) {
      apply_province_overrides(view->province_overrides());
      m_province_state_version = view->province_state_version();
    }
  }

private:
  QSize m_size;
  bool m_initialized = false;

  QOpenGLShaderProgram m_texture_program;
  QOpenGLShaderProgram m_line_program;

  QOpenGLShaderProgram m_terrain_program;
  QOpenGLShaderProgram m_province_program;
  QOpenGLShaderProgram m_label_program;

  GLuint m_quad_vao = 0;
  GLuint m_quad_vbo = 0;
  GLuint m_ocean_vao = 0;
  GLuint m_ocean_vbo = 0;

  GLuint m_land_vao = 0;
  GLuint m_land_vbo = 0;
  int m_land_vertex_count = 0;

  QOpenGLTexture* m_base_texture = nullptr;
  QOpenGLTexture* m_water_texture = nullptr;

  LineLayer m_coast_layer;
  LineLayer m_river_layer;
  PathLayer m_path_layer;
  LineLayer m_province_border_layer;
  ProvinceLayer m_province_layer;
  MissionBadge m_mission_badge;
  CartographicSymbolLayer m_symbol_layer;
  TerrainMesh m_terrain_mesh;
  HillshadeLayer m_hillshade;
  LabelLayer m_label_layer;

  float m_orbit_yaw = 185.0F;
  float m_orbit_pitch = 52.0F;
  float m_orbit_distance = 1.35F;
  float m_pan_u = 0.0F;
  float m_pan_v = 0.0F;
  QString m_hover_province_id;
  QString m_selected_province_id;
  int m_province_state_version = 0;
  int m_current_mission = 7;
  float m_terrain_height_scale = 0.15F;
  bool m_show_province_fills = true;
  qint64 m_hover_start_time = 0;
  qint64 m_last_update_time = 0;

  auto ensure_initialized() -> bool {
    if (m_initialized) {
      return true;
    }

    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr || !ctx->isValid()) {
      qWarning() << "CampaignMapRenderer: No valid OpenGL context";
      return false;
    }

    initializeOpenGLFunctions();

    if (!init_shaders()) {
      return false;
    }

    init_quad();

    auto& tex_cache = CampaignMapTextureCache::instance();
    tex_cache.set_loading_allowed(true);
    m_water_texture = tex_cache.get_or_load(
        QStringLiteral(":/assets/campaign_map/campaign_water.png"));
    m_base_texture = tex_cache.get_or_load(
        QStringLiteral(":/assets/campaign_map/campaign_base_color.png"));

    tex_cache.set_loading_allowed(false);
    init_land_mesh();

    init_line_layer(m_coast_layer,
                    QStringLiteral(":/assets/campaign_map/coastlines_uv.json"),
                    QVector4D(0.12F, 0.10F, 0.08F, 0.95F),
                    2.5F);

    m_coast_layer.double_stroke = true;
    m_coast_layer.outer_color = QVector4D(0.12F, 0.10F, 0.08F, 0.95F);
    m_coast_layer.inner_color = QVector4D(0.55F, 0.50F, 0.42F, 0.75F);

    init_line_layer(m_river_layer,
                    QStringLiteral(":/assets/campaign_map/rivers_uv.json"),
                    QVector4D(0.35F, 0.48F, 0.58F, 0.90F),
                    1.5F);

    init_path_layer(m_path_layer,
                    QStringLiteral(":/assets/campaign_map/hannibal_path.json"));
    init_province_layer(m_province_layer,
                        QStringLiteral(":/assets/campaign_map/provinces.json"));

    init_borders_layer(m_province_border_layer,
                       QStringLiteral(":/assets/campaign_map/provinces.json"),
                       QVector4D(0.25F, 0.22F, 0.20F, 0.65F),
                       1.2F);
    m_province_border_layer.clip_to_land = true;

    m_river_layer.clip_to_land = true;

    init_symbol_layer(m_symbol_layer,
                      QStringLiteral(":/assets/campaign_map/provinces.json"));

    init_terrain_mesh(m_terrain_mesh, 128);

    init_hillshade_layer(m_hillshade, 256, 256);

    init_label_layer(m_label_layer,
                     QStringLiteral(":/assets/campaign_map/provinces.json"));

    m_initialized = true;
    return true;
  }

  auto init_shaders() -> bool {
    static const char* k_tex_vert = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;

uniform mat4 u_mvp;
uniform float u_z;

out vec2 v_uv;

void main() {
  vec3 world = vec3(1.0 - a_pos.x, u_z, a_pos.y);
  gl_Position = u_mvp * vec4(world, 1.0);
  v_uv = a_pos;
}
)";

    static const char* k_tex_frag = R"(
#version 330 core
in vec2 v_uv;

uniform sampler2D u_tex;
uniform float u_alpha;

out vec4 fragColor;

void main() {
  vec2 uv = vec2(v_uv.x, 1.0 - v_uv.y);
  vec4 texel = texture(u_tex, uv);
  fragColor = vec4(texel.rgb, texel.a * u_alpha);
}
)";

    static const char* k_line_vert = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;

uniform mat4 u_mvp;
uniform float u_z;

void main() {
  vec3 world = vec3(1.0 - a_pos.x, u_z, a_pos.y);
  gl_Position = u_mvp * vec4(world, 1.0);
}
)";

    static const char* k_line_frag = R"(
#version 330 core
uniform vec4 u_color;

out vec4 fragColor;

void main() {
  fragColor = u_color;
}
)";

    if (!m_texture_program.addShaderFromSourceCode(QOpenGLShader::Vertex, k_tex_vert)) {
      qWarning() << "CampaignMapRenderer: Failed to compile texture vertex shader";
      return false;
    }
    if (!m_texture_program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                   k_tex_frag)) {
      qWarning() << "CampaignMapRenderer: Failed to compile texture fragment shader";
      return false;
    }
    if (!m_texture_program.link()) {
      qWarning() << "CampaignMapRenderer: Failed to link texture shader";
      return false;
    }

    if (!m_line_program.addShaderFromSourceCode(QOpenGLShader::Vertex, k_line_vert)) {
      qWarning() << "CampaignMapRenderer: Failed to compile line vertex shader";
      return false;
    }
    if (!m_line_program.addShaderFromSourceCode(QOpenGLShader::Fragment, k_line_frag)) {
      qWarning() << "CampaignMapRenderer: Failed to compile line fragment shader";
      return false;
    }
    if (!m_line_program.link()) {
      qWarning() << "CampaignMapRenderer: Failed to link line shader";
      return false;
    }

    init_asset_shader(m_terrain_program, QStringLiteral("campaign_terrain"));
    init_asset_shader(m_province_program, QStringLiteral("campaign_province"));
    return true;
  }

  void init_asset_shader(QOpenGLShaderProgram& program, const QString& name) {
    const QString vert_path = QStringLiteral(":/assets/shaders/%1.vert").arg(name);
    const QString frag_path = QStringLiteral(":/assets/shaders/%1.frag").arg(name);

    QString vert_source;
    QString frag_source;
    if (!load_shader_source(vert_path, &vert_source) ||
        !load_shader_source(frag_path, &frag_source)) {
      program.removeAllShaders();
      return;
    }

    if (!program.addShaderFromSourceCode(QOpenGLShader::Vertex, vert_source)) {
      qWarning() << "CampaignMapRenderer: Failed to compile vertex" << vert_path;
      program.removeAllShaders();
      return;
    }
    if (!program.addShaderFromSourceCode(QOpenGLShader::Fragment, frag_source)) {
      qWarning() << "CampaignMapRenderer: Failed to compile fragment" << frag_path;
      program.removeAllShaders();
      return;
    }
    if (!program.link()) {
      qWarning() << "CampaignMapRenderer: Failed to link shader" << name;
      program.removeAllShaders();
    }
  }

  void build_quad(GLuint& vao, GLuint& vbo, float min_uv, float max_uv) {
    const float verts[] = {
        min_uv,
        min_uv,
        max_uv,
        min_uv,
        max_uv,
        max_uv,
        min_uv,
        min_uv,
        max_uv,
        max_uv,
        min_uv,
        max_uv,
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
  }

  void init_quad() {
    if (m_quad_vao != 0) {
      return;
    }

    build_quad(m_quad_vao, m_quad_vbo, 0.0F, 1.0F);

    build_quad(m_ocean_vao, m_ocean_vbo, -2.0F, 3.0F);
  }

  void init_land_mesh() {
    const QString path = Utils::Resources::resolve_resource_path(
        QStringLiteral(":/assets/campaign_map/land_mesh.bin"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "CampaignMapRenderer: Failed to open land mesh" << path;
      return;
    }

    const QByteArray data = file.readAll();
    if (data.isEmpty() || (data.size() % sizeof(float) != 0)) {
      qWarning() << "CampaignMapRenderer: Land mesh is empty or invalid";
      return;
    }

    const int float_count = data.size() / static_cast<int>(sizeof(float));
    if (float_count % 2 != 0) {
      qWarning() << "CampaignMapRenderer: Land mesh float count is odd";
      return;
    }

    std::vector<float> verts(static_cast<size_t>(float_count));
    memcpy(verts.data(), data.constData(), static_cast<size_t>(data.size()));

    m_land_vertex_count = float_count / 2;
    if (m_land_vertex_count == 0) {
      return;
    }

    glGenVertexArrays(1, &m_land_vao);
    glGenBuffers(1, &m_land_vbo);

    glBindVertexArray(m_land_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_land_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.size()),
                 verts.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);
  }

  void init_line_layer(LineLayer& layer,
                       const QString& resource_path,
                       const QVector4D& color,
                       float width) {
    const QString path = Utils::Resources::resolve_resource_path(resource_path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "CampaignMapRenderer: Failed to open line layer" << path;
      return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
      qWarning() << "CampaignMapRenderer: Line layer JSON invalid" << path;
      return;
    }

    const QJsonArray lines = doc.object().value("lines").toArray();
    std::vector<float> verts;
    verts.reserve(lines.size() * 8);

    std::vector<LineSpan> spans;
    int cursor = 0;

    for (const auto& line_val : lines) {
      const QJsonArray line = line_val.toArray();
      if (line.isEmpty()) {
        continue;
      }

      const int start = cursor;
      int count = 0;
      for (const auto& pt_val : line) {
        const QJsonArray pt = pt_val.toArray();
        if (pt.size() < 2) {
          continue;
        }
        verts.push_back(static_cast<float>(pt.at(0).toDouble()));
        verts.push_back(static_cast<float>(pt.at(1).toDouble()));
        ++count;
        ++cursor;
      }

      if (count >= 2) {
        spans.push_back({start, count});
      }
    }

    if (verts.empty() || spans.empty()) {
      return;
    }

    glGenVertexArrays(1, &layer.vao);
    glGenBuffers(1, &layer.vbo);

    glBindVertexArray(layer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, layer.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);

    layer.color = color;
    layer.width = width;
    layer.spans = std::move(spans);
    layer.ready = true;
  }

  void init_province_layer(ProvinceLayer& layer, const QString& resource_path) {
    const QString path = Utils::Resources::resolve_resource_path(resource_path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "CampaignMapRenderer: Failed to open provinces" << path;
      return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
      qWarning() << "CampaignMapRenderer: Provinces JSON invalid" << path;
      return;
    }

    const QJsonArray provinces = doc.object().value("provinces").toArray();
    if (provinces.isEmpty()) {
      return;
    }

    std::vector<float> verts;
    std::vector<ProvinceSpan> spans;
    int cursor = 0;

    for (const auto& prov_val : provinces) {
      const QJsonObject prov = prov_val.toObject();
      const QString province_id = prov.value("id").toString();
      const QJsonArray tri = prov.value("triangles").toArray();
      if (tri.isEmpty()) {
        continue;
      }

      const int start = cursor;
      int count = 0;
      for (const auto& pt_val : tri) {
        const QJsonArray pt = pt_val.toArray();
        if (pt.size() < 2) {
          continue;
        }
        verts.push_back(static_cast<float>(pt.at(0).toDouble()));
        verts.push_back(static_cast<float>(pt.at(1).toDouble()));
        ++count;
        ++cursor;
      }

      if (count >= 3) {
        QVector4D color(0.0F, 0.0F, 0.0F, 0.0F);
        const QJsonArray color_arr = prov.value("color").toArray();
        if (color_arr.size() >= 4) {
          color = QVector4D(static_cast<float>(color_arr.at(0).toDouble()),
                            static_cast<float>(color_arr.at(1).toDouble()),
                            static_cast<float>(color_arr.at(2).toDouble()),
                            static_cast<float>(color_arr.at(3).toDouble()));
        }
        spans.push_back({start, count, color, color, province_id});
      }
    }

    if (verts.empty() || spans.empty()) {
      return;
    }

    glGenVertexArrays(1, &layer.vao);
    glGenBuffers(1, &layer.vbo);

    glBindVertexArray(layer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, layer.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);

    layer.spans = std::move(spans);
    layer.ready = true;
  }

  void init_borders_layer(LineLayer& layer,
                          const QString& resource_path,
                          const QVector4D& color,
                          float width) {
    const QString path = Utils::Resources::resolve_resource_path(resource_path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "CampaignMapRenderer: Failed to open borders" << path;
      return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
      qWarning() << "CampaignMapRenderer: Borders JSON invalid" << path;
      return;
    }

    const QJsonArray lines = doc.object().value("borders").toArray();
    std::vector<float> verts;
    verts.reserve(lines.size() * 8);

    std::vector<LineSpan> spans;
    int cursor = 0;

    for (const auto& line_val : lines) {
      const QJsonArray line = line_val.toArray();
      if (line.isEmpty()) {
        continue;
      }

      const int start = cursor;
      int count = 0;
      for (const auto& pt_val : line) {
        const QJsonArray pt = pt_val.toArray();
        if (pt.size() < 2) {
          continue;
        }
        verts.push_back(static_cast<float>(pt.at(0).toDouble()));
        verts.push_back(static_cast<float>(pt.at(1).toDouble()));
        ++count;
        ++cursor;
      }

      if (count >= 2) {
        spans.push_back({start, count});
      }
    }

    if (verts.empty() || spans.empty()) {
      return;
    }

    glGenVertexArrays(1, &layer.vao);
    glGenBuffers(1, &layer.vbo);

    glBindVertexArray(layer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, layer.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);

    layer.color = color;
    layer.width = width;
    layer.spans = std::move(spans);
    layer.ready = true;
  }

  void init_path_layer(PathLayer& layer, const QString& resource_path) {
    const QString path = Utils::Resources::resolve_resource_path(resource_path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "CampaignMapRenderer: Failed to open path layer" << path;
      return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
      qWarning() << "CampaignMapRenderer: Path layer JSON invalid" << path;
      return;
    }

    const QJsonArray lines = doc.object().value("lines").toArray();
    for (const auto& line_val : lines) {
      const QJsonArray line = line_val.toArray();
      if (line.isEmpty()) {
        continue;
      }

      std::vector<QVector2D> raw_points;
      raw_points.reserve(static_cast<size_t>(line.size()));
      for (const auto& pt_val : line) {
        const QJsonArray pt = pt_val.toArray();
        if (pt.size() < 2) {
          continue;
        }
        raw_points.emplace_back(static_cast<float>(pt.at(0).toDouble()),
                                static_cast<float>(pt.at(1).toDouble()));
      }

      if (raw_points.size() < 2) {
        continue;
      }

      PathLine entry;

      constexpr int k_spline_samples_per_segment = 8;
      entry.points = CampaignMapRender::smooth_catmull_rom(
          raw_points, k_spline_samples_per_segment);

      if (entry.points.size() >= 2) {
        layer.lines.push_back(std::move(entry));
      }
    }

    layer.ready = !layer.lines.empty();
  }

  void init_symbol_layer(CartographicSymbolLayer& layer, const QString& resource_path) {
    const QString path = Utils::Resources::resolve_resource_path(resource_path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "CampaignMapRenderer: Failed to open symbols source" << path;
      return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
      qWarning() << "CampaignMapRenderer: Symbols JSON invalid" << path;
      return;
    }

    const QJsonArray provinces = doc.object().value("provinces").toArray();
    for (const auto& prov_val : provinces) {
      const QJsonObject prov = prov_val.toObject();
      const QJsonArray cities = prov.value("cities").toArray();

      for (const auto& city_val : cities) {
        const QJsonObject city = city_val.toObject();
        const QJsonArray uv = city.value("uv").toArray();
        if (uv.size() < 2) {
          continue;
        }

        CartographicSymbolInstance symbol;
        symbol.position = QVector2D(static_cast<float>(uv.at(0).toDouble()),
                                    static_cast<float>(uv.at(1).toDouble()));

        const QString name = city.value("name").toString().toLower();
        const bool is_port = name.contains("port") || name.contains("harbor") ||
                             city.value("is_port").toBool();
        const bool is_capital = city.value("is_capital").toBool();

        if (is_port) {
          symbol.symbol_type = 2;
          symbol.importance = 2;
        } else if (is_capital) {
          symbol.symbol_type = 1;
          symbol.importance = 3;
        } else {
          symbol.symbol_type = 1;
          symbol.importance = 1;
        }

        symbol.size = 0.012F + 0.004F * static_cast<float>(symbol.importance);
        layer.symbols.push_back(symbol);
      }

      const QString terrain = prov.value("terrain_type").toString().toLower();
      if (terrain.contains("mountain") || terrain.contains("alpine")) {
        const QJsonArray label_uv = prov.value("label_uv").toArray();
        if (label_uv.size() >= 2) {
          CartographicSymbolInstance mountain;
          mountain.position = QVector2D(static_cast<float>(label_uv.at(0).toDouble()),
                                        static_cast<float>(label_uv.at(1).toDouble()));
          mountain.symbol_type = 0;
          mountain.importance = 2;
          mountain.size = 0.018F;
          layer.symbols.push_back(mountain);
        }
      }
    }

    if (!layer.symbols.empty()) {
      build_symbol_geometry(layer);
    }
  }

  void build_symbol_geometry(CartographicSymbolLayer& layer) {
    std::vector<float> verts;

    verts.reserve(layer.symbols.size() * 20 * 4);
    layer.spans.clear();

    for (const auto& symbol : layer.symbols) {
      std::vector<QVector2D> outline;

      switch (symbol.symbol_type) {
      case 0:
        outline = CampaignMapRender::generate_mountain_icon(
            QVector2D(0.0F, 0.0F), 1.0F, symbol.importance);
        break;
      case 1:
        outline = CampaignMapRender::generate_city_marker(
            QVector2D(0.0F, 0.0F), 1.0F, symbol.importance);
        break;
      case 2:
        outline = CampaignMapRender::generate_anchor_icon(QVector2D(0.0F, 0.0F), 1.0F);
        break;
      default:
        outline =
            CampaignMapRender::generate_city_marker(QVector2D(0.0F, 0.0F), 1.0F, 1);
        break;
      }

      if (outline.size() < 2) {
        continue;
      }

      const int start = static_cast<int>(verts.size() / 4);
      const int count = static_cast<int>(outline.size());

      for (const auto& pt : outline) {
        float world_x = symbol.position.x() + pt.x() * symbol.size;
        float world_y = symbol.position.y() + pt.y() * symbol.size;
        verts.push_back(world_x);
        verts.push_back(world_y);
        verts.push_back(pt.x());
        verts.push_back(static_cast<float>(symbol.symbol_type));
      }

      layer.spans.push_back({start, count});
    }

    if (verts.empty() || layer.spans.empty()) {
      layer.ready = false;
      return;
    }

    if (layer.vao == 0) {
      glGenVertexArrays(1, &layer.vao);
      glGenBuffers(1, &layer.vbo);
    }

    glBindVertexArray(layer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, layer.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);

    layer.total_vertices = static_cast<int>(verts.size() / 4);
    layer.ready = true;
  }

  void draw_symbol_layer(const CartographicSymbolLayer& layer,
                         const QMatrix4x4& mvp,
                         float z_offset) {
    if (!layer.ready || layer.vao == 0 || layer.spans.empty()) {
      return;
    }

    m_line_program.bind();
    m_line_program.setUniformValue("u_mvp", mvp);

    if (layer.show_shadow) {
      m_line_program.setUniformValue("u_z", z_offset);
      m_line_program.setUniformValue("u_color", layer.shadow_color);
      glBindVertexArray(layer.vao);
      for (const auto& span : layer.spans) {
        glDrawArrays(GL_LINE_STRIP, span.start, span.count);
      }
      glBindVertexArray(0);
    }

    m_line_program.setUniformValue("u_z", z_offset + 0.001F);
    m_line_program.setUniformValue("u_color", layer.fill_color);
    glBindVertexArray(layer.vao);
    for (const auto& span : layer.spans) {
      glDrawArrays(GL_LINE_STRIP, span.start, span.count);
    }
    glBindVertexArray(0);

    m_line_program.setUniformValue("u_z", z_offset + 0.002F);
    m_line_program.setUniformValue("u_color", layer.stroke_color);
    glBindVertexArray(layer.vao);
    for (const auto& span : layer.spans) {
      glDrawArrays(GL_LINE_STRIP, span.start, span.count);
    }
    glBindVertexArray(0);

    m_line_program.release();
  }

  void init_terrain_mesh(TerrainMesh& mesh, int resolution) {
    constexpr float k_height_scale = 1.0F;
    std::vector<float> vertices;
    HeightmapData heightmap = load_heightmap_data(
        QStringLiteral(":/assets/campaign_map/terrain_height.png"),
        QStringLiteral(":/assets/campaign_map/terrain_height.json"));

    if (heightmap.ready) {
      mesh.height_scale = k_height_scale;
      const int grid = qMax(resolution, qMax(heightmap.width, heightmap.height) / 4);
      if (grid > 1) {
        const float step = 1.0F / static_cast<float>(grid - 1);
        const float sample_dist =
            1.0F / static_cast<float>(qMax(heightmap.width, heightmap.height));

        float land_max = 0.0F;
        for (float h : heightmap.samples) {
          if (h > land_max) {
            land_max = h;
          }
        }
        const float land_scale = (land_max > 1e-6F) ? (1.0F / land_max) : 1.0F;
        const float edge_margin = 0.03F;

        auto sample_height_raw = [&](float u, float v) -> float {
          const float cu = qBound(0.0F, u, 1.0F);
          const float cv = qBound(0.0F, v, 1.0F);
          const float x = cu * static_cast<float>(heightmap.width - 1);
          const float y = (1.0F - cv) * static_cast<float>(heightmap.height - 1);

          const int x0 = static_cast<int>(std::floor(x));
          const int y0 = static_cast<int>(std::floor(y));
          const int x1 = qMin(x0 + 1, heightmap.width - 1);
          const int y1 = qMin(y0 + 1, heightmap.height - 1);

          const float fx = x - static_cast<float>(x0);
          const float fy = y - static_cast<float>(y0);

          const float h00 =
              heightmap.samples[static_cast<size_t>(y0 * heightmap.width + x0)];
          const float h10 =
              heightmap.samples[static_cast<size_t>(y0 * heightmap.width + x1)];
          const float h01 =
              heightmap.samples[static_cast<size_t>(y1 * heightmap.width + x0)];
          const float h11 =
              heightmap.samples[static_cast<size_t>(y1 * heightmap.width + x1)];

          const float hx0 = h00 + (h10 - h00) * fx;
          const float hx1 = h01 + (h11 - h01) * fx;
          return hx0 + (hx1 - hx0) * fy;
        };

        auto sample_height = [&](float u, float v) -> float {
          const float h = sample_height_raw(u, v);
          float height = (h >= 0.0F) ? (h * land_scale) : 0.0F;

          const float edge_dist = qMin(qMin(u, 1.0F - u), qMin(v, 1.0F - v));
          if (edge_dist < edge_margin) {
            height *= qMax(0.0F, edge_dist / edge_margin);
          }
          return height;
        };

        auto sample_normal = [&](float u, float v) -> QVector3D {
          const float h_left = sample_height(u - sample_dist, v);
          const float h_right = sample_height(u + sample_dist, v);
          const float h_down = sample_height(u, v - sample_dist);
          const float h_up = sample_height(u, v + sample_dist);

          const float dx =
              (h_right - h_left) / (2.0F * sample_dist) * mesh.height_scale;
          const float dz = (h_up - h_down) / (2.0F * sample_dist) * mesh.height_scale;

          QVector3D normal(-dx, 1.0F, -dz);
          normal.normalize();
          return normal;
        };

        vertices.reserve(static_cast<size_t>(grid - 1) * static_cast<size_t>(grid - 1) *
                         6 * 8);

        auto add_vertex = [&](float u, float v, float h, const QVector3D& n) {
          vertices.push_back(u);
          vertices.push_back(v);
          vertices.push_back(u);
          vertices.push_back(v);
          vertices.push_back(h);
          vertices.push_back(n.x());
          vertices.push_back(n.y());
          vertices.push_back(n.z());
        };

        for (int y = 0; y < grid - 1; ++y) {
          for (int x = 0; x < grid - 1; ++x) {
            const float u0 = static_cast<float>(x) * step;
            const float v0 = static_cast<float>(y) * step;
            const float u1 = static_cast<float>(x + 1) * step;
            const float v1 = static_cast<float>(y + 1) * step;

            const float h00 = sample_height(u0, v0);
            const float h10 = sample_height(u1, v0);
            const float h01 = sample_height(u0, v1);
            const float h11 = sample_height(u1, v1);

            const QVector3D n00 = sample_normal(u0, v0);
            const QVector3D n10 = sample_normal(u1, v0);
            const QVector3D n01 = sample_normal(u0, v1);
            const QVector3D n11 = sample_normal(u1, v1);

            add_vertex(u0, v0, h00, n00);
            add_vertex(u1, v0, h10, n10);
            add_vertex(u0, v1, h01, n01);

            add_vertex(u1, v0, h10, n10);
            add_vertex(u1, v1, h11, n11);
            add_vertex(u0, v1, h01, n01);
          }
        }
      }
    }

    if (vertices.empty()) {
      mesh.height_scale = k_height_scale;
      vertices = CampaignMapRender::generate_terrain_mesh(resolution, 1.0F);
    }

    if (vertices.empty()) {
      mesh.ready = false;
      return;
    }

    if (mesh.vao == 0) {
      glGenVertexArrays(1, &mesh.vao);
      glGenBuffers(1, &mesh.vbo);
    }

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    const int stride = 8 * sizeof(float);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(4 * sizeof(float)));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(5 * sizeof(float)));

    glBindVertexArray(0);

    mesh.vertex_count = static_cast<int>(vertices.size() / 8);
    mesh.ready = true;
  }

  void init_hillshade_layer(HillshadeLayer& layer, int width, int height) {
    CampaignMapRender::HillshadeConfig config;
    config.light_direction = layer.light_direction;
    config.ambient = layer.ambient;
    config.intensity = layer.intensity;

    std::vector<unsigned char> pixels =
        CampaignMapRender::generate_hillshade_texture(width, height, config);

    if (pixels.empty()) {
      layer.ready = false;
      return;
    }

    if (layer.texture_id == 0) {
      glGenTextures(1, &layer.texture_id);
    }

    glBindTexture(GL_TEXTURE_2D, layer.texture_id);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 width,
                 height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    layer.width = width;
    layer.height = height;
    layer.ready = true;
  }

  auto load_heightmap_data(const QString& image_path,
                           const QString& meta_path) -> HeightmapData {
    HeightmapData data;

    const QString resolved_image = Utils::Resources::resolve_resource_path(image_path);
    QImage image(resolved_image);
    if (image.isNull()) {
      qWarning() << "CampaignMapRenderer: Failed to load heightmap" << resolved_image;
      return data;
    }

    if (image.format() != QImage::Format_Grayscale16) {
      image = image.convertToFormat(QImage::Format_Grayscale16);
    }

    if (image.isNull()) {
      qWarning() << "CampaignMapRenderer: Heightmap conversion failed"
                 << resolved_image;
      return data;
    }

    float min_m = -6000.0F;
    float max_m = 4000.0F;

    const QString resolved_meta = Utils::Resources::resolve_resource_path(meta_path);
    QFile meta_file(resolved_meta);
    if (meta_file.open(QIODevice::ReadOnly)) {
      const QJsonDocument doc = QJsonDocument::fromJson(meta_file.readAll());
      if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        if (obj.contains("min_m")) {
          min_m = static_cast<float>(obj.value("min_m").toDouble());
        }
        if (obj.contains("max_m")) {
          max_m = static_cast<float>(obj.value("max_m").toDouble());
        }
      }
    }

    if (!(min_m < max_m)) {
      qWarning() << "CampaignMapRenderer: Heightmap metadata invalid" << resolved_meta;
      return data;
    }

    const int width = image.width();
    const int height = image.height();
    if (width <= 1 || height <= 1) {
      qWarning() << "CampaignMapRenderer: Heightmap dimensions invalid" << width
                 << height;
      return data;
    }

    const float range = max_m - min_m;
    const float max_abs = qMax(std::abs(min_m), std::abs(max_m));
    if (max_abs <= 0.0F) {
      return data;
    }

    data.samples.resize(static_cast<size_t>(width * height));
    for (int y = 0; y < height; ++y) {
      const auto* row = reinterpret_cast<const quint16*>(image.constScanLine(y));
      for (int x = 0; x < width; ++x) {
        const float norm = static_cast<float>(row[x]) / 65535.0F;
        const float height_m = min_m + norm * range;
        data.samples[static_cast<size_t>(y * width + x)] = height_m / max_abs;
      }
    }

    data.width = width;
    data.height = height;
    data.min_m = min_m;
    data.max_m = max_m;
    data.ready = true;
    return data;
  }

  void init_label_layer(LabelLayer& layer, const QString& resource_path) {
    const QString path = Utils::Resources::resolve_resource_path(resource_path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "CampaignMapRenderer: Failed to open labels source" << path;
      return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
      qWarning() << "CampaignMapRenderer: Labels JSON invalid" << path;
      return;
    }

    const QJsonArray provinces = doc.object().value("provinces").toArray();
    for (const auto& prov_val : provinces) {
      const QJsonObject prov = prov_val.toObject();
      const QString name = prov.value("name").toString();
      const QJsonArray label_uv = prov.value("label_uv").toArray();

      if (name.isEmpty() || label_uv.size() < 2) {
        continue;
      }

      ProvinceLabel label;
      label.text = Game::Util::tr_asset(Game::Util::k_campaign_map_context, name);
      label.position = QVector2D(static_cast<float>(label_uv.at(0).toDouble()),
                                 static_cast<float>(label_uv.at(1).toDouble()));

      const QString id = prov.value("id").toString().toLower();
      if (id.contains("core") || id.contains("capital")) {
        label.importance = 3;
      } else if (id.contains("major") || prov.value("cities").toArray().size() > 2) {
        label.importance = 2;
      } else {
        label.importance = 1;
      }

      label.is_sea =
          id.contains("sea") || id.contains("mare") || id.contains("mediterranean");

      layer.labels.push_back(label);
    }

    build_label_geometry(layer);
  }

  void build_label_geometry(LabelLayer& layer) {
    std::vector<float> verts;

    for (const auto& label : layer.labels) {

      CampaignMapRender::LabelStyle style;
      if (label.is_sea) {
        style = CampaignMapRender::LabelStyles::sea_label();
      } else if (label.importance >= 3) {
        style = CampaignMapRender::LabelStyles::region_label();
      } else if (label.importance >= 2) {
        style = CampaignMapRender::LabelStyles::province_label();
      } else {
        style = CampaignMapRender::LabelStyles::city_label();
      }

      std::vector<float> label_verts = CampaignMapRender::generate_label_quads(
          label.position, label.text.toStdString(), style);

      verts.insert(verts.end(), label_verts.begin(), label_verts.end());
    }

    if (verts.empty()) {
      layer.ready = false;
      return;
    }

    if (layer.vao == 0) {
      glGenVertexArrays(1, &layer.vao);
      glGenBuffers(1, &layer.vbo);
    }

    glBindVertexArray(layer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, layer.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(),
                 GL_STATIC_DRAW);

    const int stride = 6 * sizeof(float);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(4 * sizeof(float)));

    glBindVertexArray(0);

    layer.total_vertices = static_cast<int>(verts.size() / 6);
    layer.ready = true;
  }

  void
  draw_label_layer(const LabelLayer& layer, const QMatrix4x4& mvp, float z_offset) {
    if (!layer.ready || layer.vao == 0 || layer.total_vertices <= 0) {
      return;
    }
    if (layer.font_texture == 0) {

      return;
    }

    m_line_program.bind();
    m_line_program.setUniformValue("u_mvp", mvp);
    m_line_program.setUniformValue("u_z", z_offset);

    m_line_program.setUniformValue("u_color", QVector4D(0.25F, 0.20F, 0.15F, 0.85F));

    glBindVertexArray(layer.vao);
    glDrawArrays(GL_TRIANGLES, 0, layer.total_vertices);
    glBindVertexArray(0);

    m_line_program.release();
  }

  auto load_texture(const QString& resource_path) -> QOpenGLTexture* {
    const QString path = Utils::Resources::resolve_resource_path(resource_path);
    QImage image(path);
    if (image.isNull()) {
      qWarning() << "CampaignMapRenderer: Failed to load texture" << path;
      return nullptr;
    }

    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    auto* texture = new QOpenGLTexture(rgba);
    texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    return texture;
  }

  void compute_mvp(QMatrix4x4& out_mvp) const {
    out_mvp = build_mvp_matrix(static_cast<float>(m_size.width()),
                               static_cast<float>(m_size.height()),
                               m_orbit_yaw,
                               m_orbit_pitch,
                               m_orbit_distance,
                               m_pan_u,
                               m_pan_v);
  }

  void draw_textured_layer(QOpenGLTexture* texture,
                           GLuint vao,
                           int vertex_count,
                           const QMatrix4x4& mvp,
                           float alpha,
                           float z_offset) {
    if (texture == nullptr || vao == 0 || vertex_count <= 0) {
      return;
    }

    m_texture_program.bind();
    m_texture_program.setUniformValue("u_mvp", mvp);
    m_texture_program.setUniformValue("u_z", z_offset);
    m_texture_program.setUniformValue("u_alpha", alpha);
    m_texture_program.setUniformValue("u_tex", 0);

    glActiveTexture(GL_TEXTURE0);
    texture->bind();
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count);
    glBindVertexArray(0);
    texture->release();
    m_texture_program.release();
  }

  void draw_terrain_layer(const TerrainMesh& mesh, const QMatrix4x4& mvp) {
    if (!mesh.ready || mesh.vao == 0 || mesh.vertex_count <= 0 ||
        m_base_texture == nullptr || !m_terrain_program.isLinked()) {
      return;
    }

    m_terrain_program.bind();
    m_terrain_program.setUniformValue("u_mvp", mvp);
    m_terrain_program.setUniformValue("u_height_scale",
                                      mesh.height_scale * m_terrain_height_scale);
    m_terrain_program.setUniformValue("u_z_base", mesh.z_base);

    m_terrain_program.setUniformValue("u_light_direction",
                                      QVector3D(-0.45F, 0.78F, -0.44F));

    m_terrain_program.setUniformValue("u_ambient_strength", 0.62F);
    m_terrain_program.setUniformValue("u_hillshade_strength", 0.5F);
    m_terrain_program.setUniformValue("u_ao_strength", 0.12F);
    m_terrain_program.setUniformValue("u_use_hillshade", true);
    m_terrain_program.setUniformValue("u_use_parchment", true);
    m_terrain_program.setUniformValue("u_use_lighting", false);
    m_terrain_program.setUniformValue("u_water_deep_color",
                                      QVector3D(0.10F, 0.22F, 0.30F));
    m_terrain_program.setUniformValue("u_water_shallow_color",
                                      QVector3D(0.26F, 0.42F, 0.52F));

    m_terrain_program.setUniformValue("u_lowland_tint", QVector3D(1.0F, 0.99F, 0.97F));
    m_terrain_program.setUniformValue("u_highland_tint",
                                      QVector3D(0.99F, 0.94F, 0.83F));
    m_terrain_program.setUniformValue("u_mountain_tint",
                                      QVector3D(0.93F, 0.82F, 0.66F));

    m_terrain_program.setUniformValue("u_elevation_scale", 1.6F);

    m_terrain_program.setUniformValue("u_base_texture", 0);
    glActiveTexture(GL_TEXTURE0);
    m_base_texture->bind();

    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertex_count);
    glBindVertexArray(0);

    m_base_texture->release();
    m_terrain_program.release();
  }

  void draw_line_layer(const LineLayer& layer, const QMatrix4x4& mvp, float z_offset) {
    if (!layer.ready || layer.vao == 0 || layer.spans.empty()) {
      return;
    }

    const bool clipped = layer.clip_to_land && m_province_program.isLinked() &&
                         m_base_texture != nullptr;
    QOpenGLShaderProgram& program = clipped ? m_province_program : m_line_program;

    program.bind();
    program.setUniformValue("u_mvp", mvp);
    if (clipped) {
      program.setUniformValue("u_base_texture", 0);
      program.setUniformValue("u_use_parchment", false);
      glActiveTexture(GL_TEXTURE0);
      m_base_texture->bind();
    }

    if (layer.double_stroke) {

      glLineWidth(layer.width * layer.outer_width_ratio);
      program.setUniformValue("u_z", z_offset);
      program.setUniformValue("u_color", layer.outer_color);

      glBindVertexArray(layer.vao);
      for (const auto& span : layer.spans) {
        glDrawArrays(GL_LINE_STRIP, span.start, span.count);
      }

      glLineWidth(layer.width * layer.inner_width_ratio);
      program.setUniformValue("u_z", z_offset + 0.001F);
      program.setUniformValue("u_color", layer.inner_color);

      for (const auto& span : layer.spans) {
        glDrawArrays(GL_LINE_STRIP, span.start, span.count);
      }
      glBindVertexArray(0);
    } else {

      glLineWidth(layer.width);
      program.setUniformValue("u_z", z_offset);
      program.setUniformValue("u_color", layer.color);

      glBindVertexArray(layer.vao);
      for (const auto& span : layer.spans) {
        glDrawArrays(GL_LINE_STRIP, span.start, span.count);
      }
      glBindVertexArray(0);
    }

    if (clipped) {
      m_base_texture->release();
    }
    program.release();
  }

  static auto safe_normalized(const QVector2D& v) -> QVector2D {
    const float len = v.length();
    if (len < 1e-5F) {
      return QVector2D(0.0F, 0.0F);
    }
    return v / len;
  }

  static auto perp(const QVector2D& v) -> QVector2D { return QVector2D(-v.y(), v.x()); }

  void build_path_strip(const std::vector<QVector2D>& input,
                        float half_width,
                        std::vector<QVector2D>& out) const {
    out.clear();
    if (input.size() < 2 || half_width <= 0.0F) {
      return;
    }

    std::vector<QVector2D> points;
    points.reserve(input.size());
    for (const auto& pt : input) {
      if (points.empty()) {
        points.push_back(pt);
        continue;
      }
      const QVector2D delta = pt - points.back();
      if (QVector2D::dotProduct(delta, delta) > 1e-10F) {
        points.push_back(pt);
      }
    }
    if (points.size() < 2) {
      return;
    }

    out.reserve(points.size() * 2);

    for (size_t i = 0; i < points.size(); ++i) {
      QVector2D offset;
      if (i == 0) {
        const QVector2D dir = safe_normalized(points[1] - points[0]);
        offset = perp(dir) * half_width;
      } else if (i + 1 == points.size()) {
        const QVector2D dir = safe_normalized(points[i] - points[i - 1]);
        offset = perp(dir) * half_width;
      } else {
        QVector2D dir0 = safe_normalized(points[i] - points[i - 1]);
        QVector2D dir1 = safe_normalized(points[i + 1] - points[i]);
        if (dir0.isNull()) {
          dir0 = dir1;
        }
        if (dir1.isNull()) {
          dir1 = dir0;
        }

        const QVector2D n0 = perp(dir0);
        const QVector2D n1 = perp(dir1);
        QVector2D miter = safe_normalized(n0 + n1);
        if (miter.isNull()) {
          miter = n1;
        }

        float denom = QVector2D::dotProduct(miter, n1);
        if (std::abs(denom) < 0.2F) {
          denom = (denom >= 0.0F) ? 0.2F : -0.2F;
        }
        float miter_len = half_width / denom;
        const float max_miter = half_width * 3.0F;
        if (std::abs(miter_len) > max_miter) {
          miter_len = (miter_len < 0.0F) ? -max_miter : max_miter;
        }

        offset = miter * miter_len;
      }

      out.push_back(points[i] + offset);
      out.push_back(points[i] - offset);
    }
  }

  void update_path_mesh(StrokeMesh& mesh,
                        const std::vector<QVector2D>& points,
                        float width) {
    if (points.size() < 2) {
      mesh.ready = false;
      mesh.vertex_count = 0;
      return;
    }

    if (mesh.ready && qFuzzyCompare(mesh.width, width)) {
      return;
    }

    CampaignMapRender::StrokeMeshConfig config;
    config.width = width;
    config.start_cap = CampaignMapRender::CapStyle::Round;
    config.end_cap = CampaignMapRender::CapStyle::Round;
    config.join_style = CampaignMapRender::JoinStyle::Miter;
    config.cap_segments = 6;
    config.miter_params.max_miter_ratio = 3.0F;

    std::vector<QVector2D> strip = CampaignMapRender::build_stroke_mesh(points, config);

    if (strip.size() < 4) {
      mesh.ready = false;
      mesh.vertex_count = 0;
      return;
    }

    std::vector<float> verts;
    verts.reserve(strip.size() * 2);
    for (const auto& v : strip) {
      verts.push_back(v.x());
      verts.push_back(v.y());
    }

    if (mesh.vao == 0) {
      glGenVertexArrays(1, &mesh.vao);
      glGenBuffers(1, &mesh.vbo);
    }

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(),
                 GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glBindVertexArray(0);

    mesh.vertex_count = static_cast<int>(strip.size());
    mesh.width = width;
    mesh.ready = true;
  }

  auto uv_width_for_pixels(float px) const -> float {
    const float viewport_h = qMax(1.0F, static_cast<float>(m_size.height()));
    const float fov_rad = qDegreesToRadians(45.0F * 0.5F);
    const float view_h = 2.0F * m_orbit_distance * std::tan(fov_rad);
    const float uv_width = px * (view_h / viewport_h);
    return qMax(0.0005F, uv_width);
  }

  void draw_path_mesh(const StrokeMesh& mesh) {
    if (!mesh.ready || mesh.vao == 0 || mesh.vertex_count <= 0) {
      return;
    }
    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, mesh.vertex_count);
    glBindVertexArray(0);
  }

  void draw_progressive_path_layers(PathLayer& layer,
                                    const QMatrix4x4& mvp,
                                    float z_offset) {
    if (!layer.ready || layer.lines.empty()) {
      return;
    }

    const int max_mission =
        qBound(0, m_current_mission, static_cast<int>(layer.lines.size()) - 1);

    m_line_program.bind();
    m_line_program.setUniformValue("u_mvp", mvp);

    for (int i = 0; i <= max_mission; ++i) {
      if (i >= static_cast<int>(layer.lines.size())) {
        break;
      }

      auto& line = layer.lines[static_cast<size_t>(i)];

      const int age = max_mission - i;
      auto passes =
          CampaignMapRender::CartographicStyles::get_inked_route_passes(1.0F, age);

      if (!passes.empty()) {
        const auto& border_pass = passes[0];

        float border_width_px = 3.6F * border_pass.width_multiplier;
        if (i != max_mission) {
          border_width_px *= 0.85F;
        }

        m_line_program.setUniformValue("u_z", z_offset + border_pass.z_offset);
        update_path_mesh(
            line.border, line.points, uv_width_for_pixels(border_width_px));
        m_line_program.setUniformValue("u_color", border_pass.color);
        draw_path_mesh(line.border);
      }
    }

    for (int i = 0; i <= max_mission; ++i) {
      if (i >= static_cast<int>(layer.lines.size())) {
        break;
      }

      auto& line = layer.lines[static_cast<size_t>(i)];

      const int age = max_mission - i;
      auto passes =
          CampaignMapRender::CartographicStyles::get_inked_route_passes(1.0F, age);

      if (passes.size() > 2) {
        const auto& highlight_pass = passes[2];
        float highlight_width_px = 2.5F * highlight_pass.width_multiplier;
        if (i != max_mission) {
          highlight_width_px *= 0.8F;
        }

        m_line_program.setUniformValue("u_z", z_offset + highlight_pass.z_offset);
        update_path_mesh(
            line.highlight, line.points, uv_width_for_pixels(highlight_width_px));
        m_line_program.setUniformValue("u_color", highlight_pass.color);
        draw_path_mesh(line.highlight);
      }
    }

    for (int i = 0; i <= max_mission; ++i) {
      if (i >= static_cast<int>(layer.lines.size())) {
        break;
      }

      auto& line = layer.lines[static_cast<size_t>(i)];

      const int age = max_mission - i;
      auto passes =
          CampaignMapRender::CartographicStyles::get_inked_route_passes(1.0F, age);

      if (passes.size() > 3) {
        const auto& core_pass = passes[3];
        float core_width_px = 1.9F * core_pass.width_multiplier;
        if (i != max_mission) {
          core_width_px *= 0.75F;
        }

        m_line_program.setUniformValue("u_z", z_offset + core_pass.z_offset);
        update_path_mesh(line.core, line.points, uv_width_for_pixels(core_width_px));
        m_line_program.setUniformValue("u_color", core_pass.color);
        draw_path_mesh(line.core);
      }
    }

    m_line_program.release();
  }

  void update_mission_badge_position(MissionBadge& badge,
                                     const PathLayer& path_layer,
                                     int mission_index) {
    if (!path_layer.ready || path_layer.lines.empty()) {
      badge.ready = false;
      return;
    }

    const int clamped_mission =
        qBound(0, mission_index, static_cast<int>(path_layer.lines.size()) - 1);
    const auto& line = path_layer.lines[static_cast<size_t>(clamped_mission)];

    if (line.points.empty()) {
      badge.ready = false;
      return;
    }

    badge.position = line.points.back();
    badge.ready = true;
  }

  void build_badge_geometry(MissionBadge& badge) {
    if (!badge.ready) {
      return;
    }

    std::vector<QVector2D> outline =
        CampaignMapRender::generate_shield_badge(QVector2D(0.0F, 0.0F), 1.0F, 16);

    if (outline.size() < 3) {
      badge.ready = false;
      return;
    }

    std::vector<float> verts;
    verts.reserve((outline.size() + 1) * 4);

    verts.push_back(badge.position.x());
    verts.push_back(badge.position.y());
    verts.push_back(0.0F);
    verts.push_back(0.0F);

    for (const auto& pt : outline) {
      verts.push_back(badge.position.x() + pt.x() * badge.size);
      verts.push_back(badge.position.y() + pt.y() * badge.size);
      verts.push_back(pt.x());
      verts.push_back(pt.y());
    }

    if (badge.vao == 0) {
      glGenVertexArrays(1, &badge.vao);
      glGenBuffers(1, &badge.vbo);
    }

    glBindVertexArray(badge.vao);
    glBindBuffer(GL_ARRAY_BUFFER, badge.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(),
                 GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);

    badge.vertex_count = static_cast<int>(outline.size() + 1);
  }

  void draw_mission_badge(MissionBadge& badge, const QMatrix4x4& mvp, float z_offset) {
    if (!badge.ready) {
      return;
    }

    update_mission_badge_position(badge, m_path_layer, m_current_mission);
    build_badge_geometry(badge);

    if (badge.vao == 0 || badge.vertex_count < 3) {
      return;
    }

    m_line_program.bind();
    m_line_program.setUniformValue("u_mvp", mvp);

    if (badge.show_shadow) {
      m_line_program.setUniformValue("u_z", z_offset);
      m_line_program.setUniformValue("u_color", badge.shadow_color);
      glBindVertexArray(badge.vao);
      glDrawArrays(GL_TRIANGLE_FAN, 0, badge.vertex_count);
      glBindVertexArray(0);
    }

    m_line_program.setUniformValue("u_z", z_offset + 0.001F);
    m_line_program.setUniformValue("u_color", badge.border_color);
    glBindVertexArray(badge.vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, badge.vertex_count);
    glBindVertexArray(0);

    m_line_program.setUniformValue("u_z", z_offset + 0.002F);
    m_line_program.setUniformValue("u_color", badge.primary_color);
    glBindVertexArray(badge.vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, badge.vertex_count);
    glBindVertexArray(0);

    m_line_program.release();
  }

  void apply_province_overrides(
      const QHash<QString, CampaignMapView::ProvinceVisual>& overrides) {
    if (!m_province_layer.ready || m_province_layer.spans.empty()) {
      return;
    }

    for (auto& span : m_province_layer.spans) {
      const auto it = overrides.find(span.id);
      if (it != overrides.end() && it->has_color) {
        span.color = it->color;
      } else {
        span.color = span.base_color;
      }
    }
  }

  void draw_province_layer(const ProvinceLayer& layer,
                           const QMatrix4x4& mvp,
                           float z_offset) {
    if (!layer.ready || layer.vao == 0 || layer.spans.empty()) {
      return;
    }

    const bool use_province_program =
        m_province_program.isLinked() && m_base_texture != nullptr;
    QOpenGLShaderProgram& program =
        use_province_program ? m_province_program : m_line_program;

    program.bind();
    program.setUniformValue("u_mvp", mvp);
    program.setUniformValue("u_z", z_offset);
    if (use_province_program) {
      program.setUniformValue("u_base_texture", 0);
      program.setUniformValue("u_parchment_strength", 0.35F);
      program.setUniformValue("u_parchment_scale", 6.0F);
      program.setUniformValue("u_use_parchment", true);
      glActiveTexture(GL_TEXTURE0);
      m_base_texture->bind();
    }

    if (use_province_program) {
      program.setUniformValue("u_ndc_offset", QVector2D(0.0F, 0.0F));
    }

    glBindVertexArray(layer.vao);
    const ProvinceSpan* selected_span = nullptr;
    const ProvinceSpan* hovered_span = nullptr;
    for (const auto& span : layer.spans) {
      if (!m_hover_province_id.isEmpty() && span.id == m_hover_province_id) {
        hovered_span = &span;
        continue;
      }
      if (!m_selected_province_id.isEmpty() && span.id == m_selected_province_id) {
        selected_span = &span;
        continue;
      }
      if (span.color.w() <= 0.0F) {
        continue;
      }
      program.setUniformValue("u_color", province_fill_color(span));
      glDrawArrays(GL_TRIANGLES, span.start, span.count);
    }

    if (selected_span != nullptr) {
      draw_focused_province(program, *selected_span, use_province_program, false);
    }
    if (hovered_span != nullptr) {
      draw_focused_province(program, *hovered_span, use_province_program, true);
    }

    glBindVertexArray(0);
    if (use_province_program) {
      m_base_texture->release();
    }
    program.release();
  }

  [[nodiscard]] auto province_fill_color(const ProvinceSpan& span) const -> QVector4D {
    QVector4D color = span.color;

    const float parchment_tint = 0.92F;
    color.setX(color.x() * parchment_tint);
    color.setY(color.y() * parchment_tint);
    color.setZ(color.z() * parchment_tint * 0.98F);
    if (m_terrain_mesh.ready && m_terrain_height_scale > 0.01F) {
      const float fade = 1.0F / (1.0F + 3.0F * m_terrain_height_scale);
      color.setW(color.w() * fade);
    }
    return color;
  }

  void draw_focused_province(QOpenGLShaderProgram& program,
                             const ProvinceSpan& span,
                             bool can_outline,
                             bool hovered) {
    QVector4D color = province_fill_color(span);

    const QVector3D lamp(0.99F, 0.84F, 0.42F);
    const auto wash = [&color, &lamp](float amount, float min_alpha) {
      color.setX(color.x() + (lamp.x() - color.x()) * amount);
      color.setY(color.y() + (lamp.y() - color.y()) * amount);
      color.setZ(color.z() + (lamp.z() - color.z()) * amount);
      color.setW(qMin(1.0F, qMax(color.w(), min_alpha)));
    };

    float pulse = 0.0F;
    if (hovered) {
      const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_hover_start_time;
      const float pulse_cycle = 1400.0F;
      pulse = 0.5F + 0.5F * std::sin(static_cast<float>(elapsed) * 2.0F *
                                     std::numbers::pi_v<float> / pulse_cycle);
      wash(0.72F, 0.44F + 0.08F * pulse);
    } else {
      wash(0.38F, 0.30F);
    }

    if (can_outline) {
      const float radius_px = hovered ? 3.2F + 0.6F * pulse : 2.4F;
      const QVector4D outline = hovered ? QVector4D(1.0F, 0.88F, 0.46F, 0.98F)
                                        : QVector4D(0.99F, 0.66F, 0.20F, 0.95F);
      draw_province_outline(program, span, radius_px, outline);
    }

    program.setUniformValue("u_color", color);
    glDrawArrays(GL_TRIANGLES, span.start, span.count);
  }

  void draw_province_outline(QOpenGLShaderProgram& program,
                             const ProvinceSpan& span,
                             float radius_px,
                             const QVector4D& color) {
    const float width_px = static_cast<float>(qMax(1, m_size.width()));
    const float height_px = static_cast<float>(qMax(1, m_size.height()));

    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    glClear(GL_STENCIL_BUFFER_BIT);

    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    program.setUniformValue("u_ndc_offset", QVector2D(0.0F, 0.0F));
    program.setUniformValue("u_color", color);
    glDrawArrays(GL_TRIANGLES, span.start, span.count);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    for (int i = 0; i < k_outline_steps; ++i) {
      const float angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(i) /
                          static_cast<float>(k_outline_steps);
      const QVector2D offset(2.0F * radius_px * std::cos(angle) / width_px,
                             2.0F * radius_px * std::sin(angle) / height_px);
      program.setUniformValue("u_ndc_offset", offset);
      glDrawArrays(GL_TRIANGLES, span.start, span.count);
    }

    program.setUniformValue("u_ndc_offset", QVector2D(0.0F, 0.0F));
    glDisable(GL_STENCIL_TEST);
  }

  void cleanup() {
    if (QOpenGLContext::currentContext() == nullptr) {
      return;
    }

    if (m_quad_vbo != 0) {
      glDeleteBuffers(1, &m_quad_vbo);
      m_quad_vbo = 0;
    }
    if (m_ocean_vao != 0) {
      glDeleteVertexArrays(1, &m_ocean_vao);
      m_ocean_vao = 0;
    }
    if (m_ocean_vbo != 0) {
      glDeleteBuffers(1, &m_ocean_vbo);
      m_ocean_vbo = 0;
    }
    if (m_quad_vao != 0) {
      glDeleteVertexArrays(1, &m_quad_vao);
      m_quad_vao = 0;
    }
    if (m_land_vbo != 0) {
      glDeleteBuffers(1, &m_land_vbo);
      m_land_vbo = 0;
    }
    if (m_land_vao != 0) {
      glDeleteVertexArrays(1, &m_land_vao);
      m_land_vao = 0;
    }
    if (m_coast_layer.vbo != 0) {
      glDeleteBuffers(1, &m_coast_layer.vbo);
      m_coast_layer.vbo = 0;
    }
    if (m_coast_layer.vao != 0) {
      glDeleteVertexArrays(1, &m_coast_layer.vao);
      m_coast_layer.vao = 0;
    }
    if (m_river_layer.vbo != 0) {
      glDeleteBuffers(1, &m_river_layer.vbo);
      m_river_layer.vbo = 0;
    }
    if (m_river_layer.vao != 0) {
      glDeleteVertexArrays(1, &m_river_layer.vao);
      m_river_layer.vao = 0;
    }
    for (auto& line : m_path_layer.lines) {
      if (line.border.vbo != 0) {
        glDeleteBuffers(1, &line.border.vbo);
        line.border.vbo = 0;
      }
      if (line.border.vao != 0) {
        glDeleteVertexArrays(1, &line.border.vao);
        line.border.vao = 0;
      }
      if (line.highlight.vbo != 0) {
        glDeleteBuffers(1, &line.highlight.vbo);
        line.highlight.vbo = 0;
      }
      if (line.highlight.vao != 0) {
        glDeleteVertexArrays(1, &line.highlight.vao);
        line.highlight.vao = 0;
      }
      if (line.core.vbo != 0) {
        glDeleteBuffers(1, &line.core.vbo);
        line.core.vbo = 0;
      }
      if (line.core.vao != 0) {
        glDeleteVertexArrays(1, &line.core.vao);
        line.core.vao = 0;
      }
    }
    m_path_layer.lines.clear();
    if (m_province_border_layer.vbo != 0) {
      glDeleteBuffers(1, &m_province_border_layer.vbo);
      m_province_border_layer.vbo = 0;
    }
    if (m_province_border_layer.vao != 0) {
      glDeleteVertexArrays(1, &m_province_border_layer.vao);
      m_province_border_layer.vao = 0;
    }
    if (m_province_layer.vbo != 0) {
      glDeleteBuffers(1, &m_province_layer.vbo);
      m_province_layer.vbo = 0;
    }
    if (m_province_layer.vao != 0) {
      glDeleteVertexArrays(1, &m_province_layer.vao);
      m_province_layer.vao = 0;
    }

    if (m_mission_badge.vbo != 0) {
      glDeleteBuffers(1, &m_mission_badge.vbo);
      m_mission_badge.vbo = 0;
    }
    if (m_mission_badge.vao != 0) {
      glDeleteVertexArrays(1, &m_mission_badge.vao);
      m_mission_badge.vao = 0;
    }
    m_mission_badge.ready = false;

    if (m_symbol_layer.vbo != 0) {
      glDeleteBuffers(1, &m_symbol_layer.vbo);
      m_symbol_layer.vbo = 0;
    }
    if (m_symbol_layer.vao != 0) {
      glDeleteVertexArrays(1, &m_symbol_layer.vao);
      m_symbol_layer.vao = 0;
    }
    m_symbol_layer.symbols.clear();
    m_symbol_layer.ready = false;

    if (m_terrain_mesh.vbo != 0) {
      glDeleteBuffers(1, &m_terrain_mesh.vbo);
      m_terrain_mesh.vbo = 0;
    }
    if (m_terrain_mesh.vao != 0) {
      glDeleteVertexArrays(1, &m_terrain_mesh.vao);
      m_terrain_mesh.vao = 0;
    }
    m_terrain_mesh.ready = false;

    if (m_hillshade.texture_id != 0) {
      glDeleteTextures(1, &m_hillshade.texture_id);
      m_hillshade.texture_id = 0;
    }
    m_hillshade.ready = false;

    if (m_label_layer.vbo != 0) {
      glDeleteBuffers(1, &m_label_layer.vbo);
      m_label_layer.vbo = 0;
    }
    if (m_label_layer.vao != 0) {
      glDeleteVertexArrays(1, &m_label_layer.vao);
      m_label_layer.vao = 0;
    }
    if (m_label_layer.font_texture != 0) {
      glDeleteTextures(1, &m_label_layer.font_texture);
      m_label_layer.font_texture = 0;
    }
    m_label_layer.labels.clear();
    m_label_layer.ready = false;

    m_base_texture = nullptr;
    m_water_texture = nullptr;
  }
};

CampaignMapView::CampaignMapView() {
  setMirrorVertically(true);
}

void CampaignMapView::load_provinces_for_hit_test() {
  if (m_provinces_loaded) {
    return;
  }

  m_provinces_loaded = true;
  m_provinces.clear();

  const QString path = Utils::Resources::resolve_resource_path(
      QStringLiteral(":/assets/campaign_map/provinces.json"));
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    return;
  }

  const QJsonArray provinces = doc.object().value("provinces").toArray();
  for (const auto& prov_val : provinces) {
    const QJsonObject prov = prov_val.toObject();
    const QJsonArray tri = prov.value("triangles").toArray();
    if (tri.isEmpty()) {
      continue;
    }

    ProvinceHit province;
    province.id = prov.value("id").toString();
    province.name = prov.value("name").toString();
    province.owner = prov.value("owner").toString();
    province.triangles.reserve(static_cast<size_t>(tri.size()));

    for (const auto& pt_val : tri) {
      const QJsonArray pt = pt_val.toArray();
      if (pt.size() < 2) {
        continue;
      }
      province.triangles.emplace_back(static_cast<float>(pt.at(0).toDouble()),
                                      static_cast<float>(pt.at(1).toDouble()));
    }

    if (province.triangles.size() >= 3) {
      QVector2D lo = province.triangles.front();
      QVector2D hi = province.triangles.front();
      for (const auto& pt : province.triangles) {
        lo.setX(qMin(lo.x(), pt.x()));
        lo.setY(qMin(lo.y(), pt.y()));
        hi.setX(qMax(hi.x(), pt.x()));
        hi.setY(qMax(hi.y(), pt.y()));
      }
      province.bounds_min = lo;
      province.bounds_max = hi;

      float area = 0.0F;
      for (size_t i = 0; i + 2 < province.triangles.size(); i += 3) {
        const QVector2D& a = province.triangles[i];
        const QVector2D& b = province.triangles[i + 1];
        const QVector2D& c = province.triangles[i + 2];
        area += std::abs((b.x() - a.x()) * (c.y() - a.y()) -
                         (c.x() - a.x()) * (b.y() - a.y())) *
                0.5F;
      }
      province.area = area;

      m_provinces.push_back(std::move(province));
    }
  }

  if (!m_province_overrides.isEmpty()) {
    for (auto& province : m_provinces) {
      const auto it = m_province_overrides.find(province.id);
      if (it != m_province_overrides.end() && !it->owner.isEmpty()) {
        province.owner = it->owner;
      }
    }
  }
}

void CampaignMapView::load_province_labels() {
  if (m_province_labels_loaded) {
    return;
  }

  m_province_labels_loaded = true;
  m_province_labels.clear();

  const QString path = Utils::Resources::resolve_resource_path(
      QStringLiteral(":/assets/campaign_map/provinces.json"));
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    return;
  }

  const QJsonArray provinces = doc.object().value("provinces").toArray();
  for (const auto& prov_val : provinces) {
    const QJsonObject prov = prov_val.toObject();
    QVariantMap entry;
    const QString province_name = prov.value("name").toString();
    entry.insert(QStringLiteral("id"), prov.value("id").toString());
    entry.insert(QStringLiteral("name"), province_name);
    entry.insert(
        QStringLiteral("display_name"),
        Game::Util::tr_asset(Game::Util::k_campaign_map_context, province_name));
    entry.insert(QStringLiteral("owner"), prov.value("owner").toString());

    const QJsonArray label_uv = prov.value("label_uv").toArray();
    if (label_uv.size() >= 2) {
      QVariantList label_list;
      label_list.reserve(2);
      label_list.push_back(label_uv.at(0).toDouble());
      label_list.push_back(label_uv.at(1).toDouble());
      entry.insert(QStringLiteral("label_uv"), label_list);
    }

    const QJsonArray cities = prov.value("cities").toArray();
    QVariantList city_list;
    city_list.reserve(cities.size());
    for (const auto& city_val : cities) {
      const QJsonObject city = city_val.toObject();
      const QString name = city.value("name").toString();
      const QJsonArray uv = city.value("uv").toArray();
      if (name.isEmpty() || uv.size() < 2) {
        continue;
      }
      QVariantList uv_list;
      uv_list.reserve(2);
      uv_list.push_back(uv.at(0).toDouble());
      uv_list.push_back(uv.at(1).toDouble());

      QVariantMap city_entry;
      city_entry.insert(QStringLiteral("name"), name);
      city_entry.insert(QStringLiteral("display_name"),
                        Game::Util::tr_asset(Game::Util::k_campaign_map_context, name));
      city_entry.insert(QStringLiteral("uv"), uv_list);
      city_list.push_back(city_entry);
    }
    entry.insert(QStringLiteral("cities"), city_list);

    m_province_labels.push_back(entry);
  }

  if (!m_province_overrides.isEmpty()) {
    QVariantList updated;
    updated.reserve(m_province_labels.size());
    for (const auto& entry_val : m_province_labels) {
      QVariantMap entry = entry_val.toMap();
      const QString id = entry.value(QStringLiteral("id")).toString();
      const auto it = m_province_overrides.find(id);
      if (it != m_province_overrides.end() && !it->owner.isEmpty()) {
        entry.insert(QStringLiteral("owner"), it->owner);
      }
      updated.push_back(entry);
    }
    m_province_labels = std::move(updated);
  }

  emit province_labels_changed();
}

QVariantList CampaignMapView::province_labels() {
  load_province_labels();
  return m_province_labels;
}

void CampaignMapView::apply_province_state(const QVariantList& states) {
  QHash<QString, ProvinceVisual> next_overrides;
  next_overrides.reserve(states.size());

  for (const auto& state_val : states) {
    const QVariantMap state = state_val.toMap();
    const QString id = state.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
      continue;
    }

    ProvinceVisual visual;
    visual.owner = state.value(QStringLiteral("owner")).toString();

    const QVariantList color_list = state.value(QStringLiteral("color")).toList();
    if (color_list.size() >= 4) {
      visual.color = QVector4D(static_cast<float>(color_list.at(0).toDouble()),
                               static_cast<float>(color_list.at(1).toDouble()),
                               static_cast<float>(color_list.at(2).toDouble()),
                               static_cast<float>(color_list.at(3).toDouble()));
      visual.has_color = true;
    }

    next_overrides.insert(id, visual);
  }

  m_province_overrides = std::move(next_overrides);
  ++m_province_state_version;

  if (m_provinces_loaded) {
    for (auto& province : m_provinces) {
      const auto it = m_province_overrides.find(province.id);
      if (it != m_province_overrides.end() && !it->owner.isEmpty()) {
        province.owner = it->owner;
      }
    }
  }

  if (m_province_labels_loaded) {
    QVariantList updated;
    updated.reserve(m_province_labels.size());
    for (const auto& entry_val : m_province_labels) {
      QVariantMap entry = entry_val.toMap();
      const QString id = entry.value(QStringLiteral("id")).toString();
      const auto it = m_province_overrides.find(id);
      if (it != m_province_overrides.end() && !it->owner.isEmpty()) {
        entry.insert(QStringLiteral("owner"), it->owner);
      }
      updated.push_back(entry);
    }
    m_province_labels = std::move(updated);
    emit province_labels_changed();
  }

  update();
}

auto CampaignMapView::uv_at_screen(float x, float y, QVector2D* out_uv) const -> bool {
  const float w = static_cast<float>(width());
  const float h = static_cast<float>(height());
  if (w <= 0.0F || h <= 0.0F) {
    return false;
  }

  const float ndc_x = (2.0F * x / w) - 1.0F;
  const float ndc_y = 1.0F - (2.0F * y / h);

  const QMatrix4x4 mvp = build_mvp_matrix(
      w, h, m_orbit_yaw, m_orbit_pitch, m_orbit_distance, m_pan_u, m_pan_v);
  bool inverted = false;
  const QMatrix4x4 inv = mvp.inverted(&inverted);
  if (!inverted) {
    return false;
  }

  const QVector4D near_p = inv * QVector4D(ndc_x, ndc_y, -1.0F, 1.0F);
  const QVector4D far_p = inv * QVector4D(ndc_x, ndc_y, 1.0F, 1.0F);
  if (qFuzzyIsNull(near_p.w()) || qFuzzyIsNull(far_p.w())) {
    return false;
  }
  const QVector3D near_v = QVector3D(near_p.x(), near_p.y(), near_p.z()) / near_p.w();
  const QVector3D far_v = QVector3D(far_p.x(), far_p.y(), far_p.z()) / far_p.w();

  const QVector3D dir = far_v - near_v;
  if (qFuzzyIsNull(dir.y())) {
    return false;
  }

  const float t = -near_v.y() / dir.y();
  if (t < 0.0F) {
    return false;
  }

  const QVector3D hit = near_v + dir * t;
  *out_uv = QVector2D(1.0F - hit.x(), hit.z());
  return true;
}

auto CampaignMapView::pick_province(float x, float y) -> const ProvinceHit* {
  load_provinces_for_hit_test();
  if (m_provinces.empty()) {
    return nullptr;
  }

  QVector2D p;
  if (!uv_at_screen(x, y, &p)) {
    return nullptr;
  }

  const ProvinceHit* best = nullptr;
  for (const auto& province : m_provinces) {
    if (distance_to_bounds(p, province.bounds_min, province.bounds_max) > 0.0F) {
      continue;
    }
    const auto& triangles = province.triangles;
    for (size_t i = 0; i + 2 < triangles.size(); i += 3) {
      if (point_in_triangle(p, triangles[i], triangles[i + 1], triangles[i + 2])) {

        if (best == nullptr || province.area < best->area) {
          best = &province;
        }
        break;
      }
    }
  }
  if (best != nullptr) {
    return best;
  }

  QVector2D nudged;
  float tolerance = k_pick_fallback_tolerance_uv;
  if (uv_at_screen(x + k_pick_tolerance_px, y, &nudged)) {
    tolerance = (nudged - p).length();
  }
  if (tolerance <= 0.0F) {
    return nullptr;
  }

  float best_distance = tolerance;
  for (const auto& province : m_provinces) {
    if (distance_to_bounds(p, province.bounds_min, province.bounds_max) >
        best_distance) {
      continue;
    }
    const auto& triangles = province.triangles;
    for (size_t i = 0; i + 2 < triangles.size(); i += 3) {
      const float distance =
          distance_to_triangle(p, triangles[i], triangles[i + 1], triangles[i + 2]);
      if (distance < best_distance) {
        best_distance = distance;
        best = &province;
      }
    }
  }

  return best;
}

QString CampaignMapView::province_at_screen(float x, float y) {
  const ProvinceHit* province = pick_province(x, y);
  return province != nullptr ? province->id : QString();
}

QVariantMap CampaignMapView::province_info_at_screen(float x, float y) {
  QVariantMap info;
  const ProvinceHit* province = pick_province(x, y);
  if (province == nullptr) {
    return info;
  }
  info.insert(QStringLiteral("id"), province->id);
  info.insert(QStringLiteral("name"),
              Game::Util::tr_asset(Game::Util::k_campaign_map_context, province->name));
  info.insert(QStringLiteral("owner"), province->owner);
  return info;
}

QPointF CampaignMapView::screen_pos_for_uv(float u, float v) {
  const float w = static_cast<float>(width());
  const float h = static_cast<float>(height());
  if (w <= 0.0F || h <= 0.0F) {
    return {};
  }

  const float clamped_u = qBound(0.0F, u, 1.0F);
  const float clamped_v = qBound(0.0F, v, 1.0F);

  const QMatrix4x4 mvp = build_mvp_matrix(
      w, h, m_orbit_yaw, m_orbit_pitch, m_orbit_distance, m_pan_u, m_pan_v);
  const QVector4D world(1.0F - clamped_u, 0.0F, clamped_v, 1.0F);
  const QVector4D clip = mvp * world;
  if (qFuzzyIsNull(clip.w())) {
    return {};
  }

  const float ndc_x = clip.x() / clip.w();
  const float ndc_y = clip.y() / clip.w();
  const float screen_x = (ndc_x + 1.0F) * 0.5F * w;
  const float screen_y = (1.0F - (ndc_y + 1.0F) * 0.5F) * h;
  return QPointF(screen_x, screen_y);
}

void CampaignMapView::load_hannibal_paths() {
  if (m_hannibal_paths_loaded) {
    return;
  }

  m_hannibal_paths_loaded = true;
  m_hannibal_paths.clear();

  const QString path = Utils::Resources::resolve_resource_path(
      QStringLiteral(":/assets/campaign_map/hannibal_path.json"));
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "Failed to load Hannibal paths from" << path;
    return;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) {
    return;
  }

  const QJsonArray lines = doc.object().value("lines").toArray();
  for (const auto& line_val : lines) {
    const QJsonArray line = line_val.toArray();
    std::vector<QVector2D> path;
    path.reserve(static_cast<size_t>(line.size()));

    for (const auto& pt_val : line) {
      const QJsonArray pt = pt_val.toArray();
      if (pt.size() < 2) {
        continue;
      }
      path.emplace_back(static_cast<float>(pt.at(0).toDouble()),
                        static_cast<float>(pt.at(1).toDouble()));
    }

    if (!path.empty()) {
      m_hannibal_paths.push_back(std::move(path));
    }
  }
}

QPointF CampaignMapView::hannibal_icon_position() {
  load_hannibal_paths();

  if (m_hannibal_paths.empty()) {
    return {};
  }

  const int mission_idx =
      qBound(0, m_current_mission, static_cast<int>(m_hannibal_paths.size()) - 1);
  const auto& path = m_hannibal_paths[static_cast<size_t>(mission_idx)];

  if (path.empty()) {
    return {};
  }

  const QVector2D& endpoint = path.back();
  return screen_pos_for_uv(endpoint.x(), endpoint.y());
}

void CampaignMapView::set_orbit_yaw(float yaw) {
  if (qFuzzyCompare(m_orbit_yaw, yaw)) {
    return;
  }
  m_orbit_yaw = yaw;
  emit orbit_yaw_changed();
  update();
}

void CampaignMapView::set_orbit_pitch(float pitch) {
  const float clamped = qBound(5.0F, pitch, 90.0F);
  if (qFuzzyCompare(m_orbit_pitch, clamped)) {
    return;
  }
  m_orbit_pitch = clamped;
  emit orbit_pitch_changed();
  update();
}

void CampaignMapView::set_orbit_distance(float distance) {
  const float clamped = qBound(k_min_orbit_distance, distance, k_max_orbit_distance);
  if (qFuzzyCompare(m_orbit_distance, clamped)) {
    return;
  }
  m_orbit_distance = clamped;
  emit orbit_distance_changed();
  update();
}

void CampaignMapView::set_pan_u(float pan) {
  const float clamped = qBound(-0.5F, pan, 0.5F);
  if (qFuzzyCompare(m_pan_u, clamped)) {
    return;
  }
  m_pan_u = clamped;
  emit pan_u_changed();
  update();
}

void CampaignMapView::set_pan_v(float pan) {
  const float clamped = qBound(-0.5F, pan, 0.5F);
  if (qFuzzyCompare(m_pan_v, clamped)) {
    return;
  }
  m_pan_v = clamped;
  emit pan_v_changed();
  update();
}

void CampaignMapView::set_hover_province_id(const QString& province_id) {
  if (m_hover_province_id == province_id) {
    return;
  }
  m_hover_province_id = province_id;
  emit hover_province_id_changed();
  update();
}

void CampaignMapView::set_selected_province_id(const QString& province_id) {
  if (m_selected_province_id == province_id) {
    return;
  }
  m_selected_province_id = province_id;
  emit selected_province_id_changed();
  update();
}

void CampaignMapView::set_current_mission(int mission) {
  const int clamped = qBound(0, mission, 7);
  if (m_current_mission == clamped) {
    return;
  }
  m_current_mission = clamped;
  emit current_mission_changed();
  update();
}

void CampaignMapView::set_terrain_height_scale(float scale) {
  if (qFuzzyCompare(m_terrain_height_scale, scale)) {
    return;
  }
  m_terrain_height_scale = scale;
  emit terrain_height_scale_changed();
  update();
}

void CampaignMapView::set_show_province_fills(bool show) {
  if (m_show_province_fills == show) {
    return;
  }
  m_show_province_fills = show;
  emit show_province_fills_changed();
  update();
}

auto CampaignMapView::createRenderer() const -> Renderer* {
  QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical() << "CampaignMapView::createRenderer() - No valid OpenGL context";
    qCritical() << "Running in software rendering mode - map view not available";
    return nullptr;
  }

  return new CampaignMapRenderer();
}
