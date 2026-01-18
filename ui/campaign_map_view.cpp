#include "campaign_map_view.h"

#include "../utils/resource_utils.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
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
#include <QVariantMap>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QtGlobal>
#include <QtMath>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

auto build_mvp_matrix(float width, float height, float yaw_deg, float pitch_deg,
                      float distance, float pan_u, float pan_v) -> QMatrix4x4 {
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
  const float clamped_distance =
      qMax(CampaignMapView::k_min_orbit_distance, distance);

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

auto point_in_triangle(const QVector2D &p, const QVector2D &a,
                       const QVector2D &b, const QVector2D &c) -> bool {
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

struct QStringHash {
  std::size_t operator()(const QString &s) const noexcept { return qHash(s); }
};

struct CampaignMapTextureCache {
  static CampaignMapTextureCache &instance() {
    static CampaignMapTextureCache s_instance;
    return s_instance;
  }

  QOpenGLTexture *get_or_load(const QString &resource_path) {

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

    const QString path = Utils::Resources::resolveResourcePath(resource_path);
    QImage image(path);
    if (image.isNull()) {
      qWarning() << "CampaignMapTextureCache: Failed to load texture" << path;
      return nullptr;
    }

    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    auto *texture = new QOpenGLTexture(rgba);
    texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    m_textures[resource_path] = texture;
    return texture;
  }

  void set_loading_allowed(bool allowed) { m_allow_loading = allowed; }

  void clear() {

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx != nullptr && ctx->isValid()) {
      for (auto &pair : m_textures) {
        delete pair.second;
      }
    }
    m_textures.clear();
  }

private:
  CampaignMapTextureCache() = default;
  ~CampaignMapTextureCache() { clear(); }
  std::unordered_map<QString, QOpenGLTexture *, QStringHash> m_textures;
  bool m_allow_loading = true;
};

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
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 mvp;
    compute_mvp(mvp);

    draw_textured_layer(m_water_texture, m_quad_vao, 6, mvp, 1.0F, -0.01F);
    if (m_land_vertex_count > 0) {
      draw_textured_layer(m_base_texture, m_land_vao, m_land_vertex_count, mvp,
                          1.0F, 0.0F);
    } else {
      draw_textured_layer(m_base_texture, m_quad_vao, 6, mvp, 1.0F, 0.0F);
    }

    glDisable(GL_DEPTH_TEST);
    draw_province_layer(m_province_layer, mvp, 0.002F);
    draw_line_layer(m_province_border_layer, mvp, 0.0045F);
    draw_line_layer(m_coast_layer, mvp, 0.004F);
    draw_line_layer(m_river_layer, mvp, 0.003F);
    draw_progressive_path_layers(m_path_layer, mvp, 0.006F);

    if (!m_hover_province_id.isEmpty()) {
      qint64 now = QDateTime::currentMSecsSinceEpoch();
      if (now - m_last_update_time >= 16) {
        m_last_update_time = now;
        update();
      }
    }
  }

  auto createFramebufferObject(const QSize &size)
      -> QOpenGLFramebufferObject * override {
    m_size = size;
    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::Depth);
    fmt.setSamples(0);
    return new QOpenGLFramebufferObject(size, fmt);
  }

  void synchronize(QQuickFramebufferObject *item) override {
    auto *view = dynamic_cast<CampaignMapView *>(item);
    if (view == nullptr) {
      return;
    }

    m_orbit_yaw = view->orbit_yaw();
    m_orbit_pitch = view->orbit_pitch();
    m_orbit_distance = view->orbit_distance();
    m_pan_u = view->pan_u();
    m_pan_v = view->pan_v();

    QString new_hover_id = view->hover_province_id();
    if (m_hover_province_id != new_hover_id) {
      m_hover_start_time = QDateTime::currentMSecsSinceEpoch();
      m_hover_province_id = new_hover_id;
    }

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

  GLuint m_quad_vao = 0;
  GLuint m_quad_vbo = 0;

  GLuint m_land_vao = 0;
  GLuint m_land_vbo = 0;
  int m_land_vertex_count = 0;

  QOpenGLTexture *m_base_texture = nullptr;
  QOpenGLTexture *m_water_texture = nullptr;

  LineLayer m_coast_layer;
  LineLayer m_river_layer;
  LineLayer m_path_layer;
  LineLayer m_province_border_layer;
  ProvinceLayer m_province_layer;

  float m_orbit_yaw = 180.0F;
  float m_orbit_pitch = 55.0F;
  float m_orbit_distance = 2.4F;
  float m_pan_u = 0.0F;
  float m_pan_v = 0.0F;
  QString m_hover_province_id;
  int m_province_state_version = 0;
  int m_current_mission = 7;
  qint64 m_hover_start_time = 0;
  qint64 m_last_update_time = 0;

  auto ensure_initialized() -> bool {
    if (m_initialized) {
      return true;
    }

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr || !ctx->isValid()) {
      qWarning() << "CampaignMapRenderer: No valid OpenGL context";
      return false;
    }

    initializeOpenGLFunctions();

    if (!init_shaders()) {
      return false;
    }

    init_quad();

    auto &tex_cache = CampaignMapTextureCache::instance();
    tex_cache.set_loading_allowed(true);
    m_water_texture = tex_cache.get_or_load(
        QStringLiteral(":/assets/campaign_map/campaign_water.png"));
    m_base_texture = tex_cache.get_or_load(
        QStringLiteral(":/assets/campaign_map/campaign_base_color.png"));

    tex_cache.set_loading_allowed(false);
    init_land_mesh();

    init_line_layer(m_coast_layer,
                    QStringLiteral(":/assets/campaign_map/coastlines_uv.json"),
                    QVector4D(0.15F, 0.13F, 0.11F, 1.0F), 2.0F);

    init_line_layer(m_river_layer,
                    QStringLiteral(":/assets/campaign_map/rivers_uv.json"),
                    QVector4D(0.35F, 0.45F, 0.55F, 0.85F), 1.5F);

    init_line_layer(m_path_layer,
                    QStringLiteral(":/assets/campaign_map/hannibal_path.json"),
                    QVector4D(0.78F, 0.2F, 0.12F, 0.9F), 2.0F);
    init_province_layer(m_province_layer,
                        QStringLiteral(":/assets/campaign_map/provinces.json"));

    init_borders_layer(m_province_border_layer,
                       QStringLiteral(":/assets/campaign_map/provinces.json"),
                       QVector4D(0.25F, 0.22F, 0.20F, 0.65F), 1.2F);

    m_initialized = true;
    return true;
  }

  auto init_shaders() -> bool {
    static const char *kTexVert = R"(
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

    static const char *kTexFrag = R"(
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

    static const char *kLineVert = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;

uniform mat4 u_mvp;
uniform float u_z;

void main() {
  vec3 world = vec3(1.0 - a_pos.x, u_z, a_pos.y);
  gl_Position = u_mvp * vec4(world, 1.0);
}
)";

    static const char *kLineFrag = R"(
#version 330 core
uniform vec4 u_color;

out vec4 fragColor;

void main() {
  fragColor = u_color;
}
)";

    if (!m_texture_program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                   kTexVert)) {
      qWarning()
          << "CampaignMapRenderer: Failed to compile texture vertex shader";
      return false;
    }
    if (!m_texture_program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                   kTexFrag)) {
      qWarning()
          << "CampaignMapRenderer: Failed to compile texture fragment shader";
      return false;
    }
    if (!m_texture_program.link()) {
      qWarning() << "CampaignMapRenderer: Failed to link texture shader";
      return false;
    }

    if (!m_line_program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                kLineVert)) {
      qWarning() << "CampaignMapRenderer: Failed to compile line vertex shader";
      return false;
    }
    if (!m_line_program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                kLineFrag)) {
      qWarning()
          << "CampaignMapRenderer: Failed to compile line fragment shader";
      return false;
    }
    if (!m_line_program.link()) {
      qWarning() << "CampaignMapRenderer: Failed to link line shader";
      return false;
    }

    return true;
  }

  void init_quad() {
    if (m_quad_vao != 0) {
      return;
    }

    static const float kQuadVerts[] = {
        0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 1.0F,
    };

    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);

    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glBindVertexArray(0);
  }

  void init_land_mesh() {
    const QString path = Utils::Resources::resolveResourcePath(
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

    const int floatCount = data.size() / static_cast<int>(sizeof(float));
    if (floatCount % 2 != 0) {
      qWarning() << "CampaignMapRenderer: Land mesh float count is odd";
      return;
    }

    std::vector<float> verts(static_cast<size_t>(floatCount));
    memcpy(verts.data(), data.constData(), static_cast<size_t>(data.size()));

    m_land_vertex_count = floatCount / 2;
    if (m_land_vertex_count == 0) {
      return;
    }

    glGenVertexArrays(1, &m_land_vao);
    glGenBuffers(1, &m_land_vbo);

    glBindVertexArray(m_land_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_land_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size()),
                 verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glBindVertexArray(0);
  }

  void init_line_layer(LineLayer &layer, const QString &resource_path,
                       const QVector4D &color, float width) {
    const QString path = Utils::Resources::resolveResourcePath(resource_path);
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

    for (const auto &line_val : lines) {
      const QJsonArray line = line_val.toArray();
      if (line.isEmpty()) {
        continue;
      }

      const int start = cursor;
      int count = 0;
      for (const auto &pt_val : line) {
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
                 verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glBindVertexArray(0);

    layer.color = color;
    layer.width = width;
    layer.spans = std::move(spans);
    layer.ready = true;
  }

  void init_province_layer(ProvinceLayer &layer, const QString &resource_path) {
    const QString path = Utils::Resources::resolveResourcePath(resource_path);
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

    for (const auto &prov_val : provinces) {
      const QJsonObject prov = prov_val.toObject();
      const QString province_id = prov.value("id").toString();
      const QJsonArray tri = prov.value("triangles").toArray();
      if (tri.isEmpty()) {
        continue;
      }

      const int start = cursor;
      int count = 0;
      for (const auto &pt_val : tri) {
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
                 verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glBindVertexArray(0);

    layer.spans = std::move(spans);
    layer.ready = true;
  }

  void init_borders_layer(LineLayer &layer, const QString &resource_path,
                          const QVector4D &color, float width) {
    const QString path = Utils::Resources::resolveResourcePath(resource_path);
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

    for (const auto &line_val : lines) {
      const QJsonArray line = line_val.toArray();
      if (line.isEmpty()) {
        continue;
      }

      const int start = cursor;
      int count = 0;
      for (const auto &pt_val : line) {
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
                 verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glBindVertexArray(0);

    layer.color = color;
    layer.width = width;
    layer.spans = std::move(spans);
    layer.ready = true;
  }

  auto load_texture(const QString &resource_path) -> QOpenGLTexture * {
    const QString path = Utils::Resources::resolveResourcePath(resource_path);
    QImage image(path);
    if (image.isNull()) {
      qWarning() << "CampaignMapRenderer: Failed to load texture" << path;
      return nullptr;
    }

    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    auto *texture = new QOpenGLTexture(rgba);
    texture->setWrapMode(QOpenGLTexture::ClampToEdge);
    texture->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    return texture;
  }

  void compute_mvp(QMatrix4x4 &out_mvp) const {
    out_mvp = build_mvp_matrix(
        static_cast<float>(m_size.width()), static_cast<float>(m_size.height()),
        m_orbit_yaw, m_orbit_pitch, m_orbit_distance, m_pan_u, m_pan_v);
  }

  void draw_textured_layer(QOpenGLTexture *texture, GLuint vao,
                           int vertex_count, const QMatrix4x4 &mvp, float alpha,
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

  void draw_line_layer(const LineLayer &layer, const QMatrix4x4 &mvp,
                       float z_offset) {
    if (!layer.ready || layer.vao == 0 || layer.spans.empty()) {
      return;
    }

    glLineWidth(layer.width);

    m_line_program.bind();
    m_line_program.setUniformValue("u_mvp", mvp);
    m_line_program.setUniformValue("u_z", z_offset);
    m_line_program.setUniformValue("u_color", layer.color);

    glBindVertexArray(layer.vao);
    for (const auto &span : layer.spans) {
      glDrawArrays(GL_LINE_STRIP, span.start, span.count);
    }
    glBindVertexArray(0);
    m_line_program.release();
  }

  void draw_progressive_path_layers(const LineLayer &layer,
                                    const QMatrix4x4 &mvp, float z_offset) {
    if (!layer.ready || layer.vao == 0 || layer.spans.empty()) {
      return;
    }

    const int max_mission =
        qBound(0, m_current_mission, static_cast<int>(layer.spans.size()) - 1);

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    m_line_program.bind();
    m_line_program.setUniformValue("u_mvp", mvp);
    m_line_program.setUniformValue("u_z", z_offset);

    glBindVertexArray(layer.vao);

    for (int i = 0; i <= max_mission; ++i) {
      if (i >= static_cast<int>(layer.spans.size())) {
        break;
      }

      const auto &span = layer.spans[static_cast<size_t>(i)];

      QVector4D border_color;
      float border_width;

      if (i == max_mission) {

        border_color = QVector4D(0.15F, 0.08F, 0.05F, 0.85F);
        border_width = 18.0F;
      } else if (i == max_mission - 1) {

        border_color = QVector4D(0.15F, 0.08F, 0.05F, 0.70F);
        border_width = 16.0F;
      } else {

        const float age_factor = 1.0F - (max_mission - i) * 0.08F;
        border_color = QVector4D(0.15F * age_factor, 0.08F * age_factor,
                                 0.05F * age_factor, 0.55F * age_factor);
        border_width = 14.0F;
      }

      glLineWidth(border_width);
      m_line_program.setUniformValue("u_color", border_color);
      glDrawArrays(GL_LINE_STRIP, span.start, span.count);
    }

    for (int i = 0; i <= max_mission; ++i) {
      if (i >= static_cast<int>(layer.spans.size())) {
        break;
      }

      const auto &span = layer.spans[static_cast<size_t>(i)];

      QVector4D highlight_color;
      float highlight_width;

      if (i == max_mission) {

        highlight_color = QVector4D(0.95F, 0.75F, 0.35F, 0.90F);
        highlight_width = 12.0F;
      } else if (i == max_mission - 1) {

        highlight_color = QVector4D(0.85F, 0.65F, 0.30F, 0.80F);
        highlight_width = 10.0F;
      } else {

        const float age_factor = 1.0F - (max_mission - i) * 0.08F;
        highlight_color = QVector4D(0.70F * age_factor, 0.50F * age_factor,
                                    0.25F * age_factor, 0.65F * age_factor);
        highlight_width = 8.5F;
      }

      glLineWidth(highlight_width);
      m_line_program.setUniformValue("u_color", highlight_color);
      glDrawArrays(GL_LINE_STRIP, span.start, span.count);
    }

    for (int i = 0; i <= max_mission; ++i) {
      if (i >= static_cast<int>(layer.spans.size())) {
        break;
      }

      const auto &span = layer.spans[static_cast<size_t>(i)];

      QVector4D color;
      float width;

      if (i == max_mission) {

        color = QVector4D(0.80F, 0.15F, 0.10F, 1.0F);
        width = 7.0F;
      } else if (i == max_mission - 1) {

        color = QVector4D(0.70F, 0.15F, 0.10F, 0.95F);
        width = 6.0F;
      } else {

        const float age_factor = 1.0F - (max_mission - i) * 0.08F;
        color = QVector4D(0.55F * age_factor, 0.12F * age_factor,
                          0.08F * age_factor, 0.85F * age_factor);
        width = 5.0F;
      }

      glLineWidth(width);
      m_line_program.setUniformValue("u_color", color);
      glDrawArrays(GL_LINE_STRIP, span.start, span.count);
    }

    glBindVertexArray(0);
    m_line_program.release();

    glDisable(GL_LINE_SMOOTH);
  }

  void apply_province_overrides(
      const QHash<QString, CampaignMapView::ProvinceVisual> &overrides) {
    if (!m_province_layer.ready || m_province_layer.spans.empty()) {
      return;
    }

    for (auto &span : m_province_layer.spans) {
      const auto it = overrides.find(span.id);
      if (it != overrides.end() && it->has_color) {
        span.color = it->color;
      } else {
        span.color = span.base_color;
      }
    }
  }

  void draw_province_layer(const ProvinceLayer &layer, const QMatrix4x4 &mvp,
                           float z_offset) {
    if (!layer.ready || layer.vao == 0 || layer.spans.empty()) {
      return;
    }

    m_line_program.bind();
    m_line_program.setUniformValue("u_mvp", mvp);
    m_line_program.setUniformValue("u_z", z_offset);

    glBindVertexArray(layer.vao);
    for (const auto &span : layer.spans) {
      if (span.color.w() <= 0.0F) {
        continue;
      }
      QVector4D color = span.color;
      if (!m_hover_province_id.isEmpty() && span.id == m_hover_province_id) {

        qint64 elapsed =
            QDateTime::currentMSecsSinceEpoch() - m_hover_start_time;
        float pulse_cycle = 1200.0F;
        float pulse =
            0.5F + 0.5F * std::sin(elapsed * 2.0F * M_PI / pulse_cycle);

        float brightness_boost = 0.3F + 0.15F * pulse;
        color = QVector4D(qMin(1.0F, color.x() + brightness_boost),
                          qMin(1.0F, color.y() + brightness_boost),
                          qMin(1.0F, color.z() + brightness_boost),
                          qMin(1.0F, color.w() + 0.2F));
      }
      m_line_program.setUniformValue("u_color", color);
      glDrawArrays(GL_TRIANGLES, span.start, span.count);
    }
    glBindVertexArray(0);
    m_line_program.release();
  }

  void cleanup() {
    if (QOpenGLContext::currentContext() == nullptr) {
      return;
    }

    if (m_quad_vbo != 0) {
      glDeleteBuffers(1, &m_quad_vbo);
      m_quad_vbo = 0;
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
    if (m_path_layer.vbo != 0) {
      glDeleteBuffers(1, &m_path_layer.vbo);
      m_path_layer.vbo = 0;
    }
    if (m_path_layer.vao != 0) {
      glDeleteVertexArrays(1, &m_path_layer.vao);
      m_path_layer.vao = 0;
    }
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

    m_base_texture = nullptr;
    m_water_texture = nullptr;
  }
};

} // namespace

CampaignMapView::CampaignMapView() {
  setMirrorVertically(true);

  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    qWarning() << "CampaignMapView: No OpenGL context available";
    qWarning()
        << "CampaignMapView: 3D rendering will not work in software mode";
    qWarning()
        << "CampaignMapView: Try running without QT_QUICK_BACKEND=software";
  }
}

void CampaignMapView::load_provinces_for_hit_test() {
  if (m_provinces_loaded) {
    return;
  }

  m_provinces_loaded = true;
  m_provinces.clear();

  const QString path = Utils::Resources::resolveResourcePath(
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
  for (const auto &prov_val : provinces) {
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

    for (const auto &pt_val : tri) {
      const QJsonArray pt = pt_val.toArray();
      if (pt.size() < 2) {
        continue;
      }
      province.triangles.emplace_back(static_cast<float>(pt.at(0).toDouble()),
                                      static_cast<float>(pt.at(1).toDouble()));
    }

    if (province.triangles.size() >= 3) {
      m_provinces.push_back(std::move(province));
    }
  }

  if (!m_province_overrides.isEmpty()) {
    for (auto &province : m_provinces) {
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

  const QString path = Utils::Resources::resolveResourcePath(
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
  for (const auto &prov_val : provinces) {
    const QJsonObject prov = prov_val.toObject();
    QVariantMap entry;
    entry.insert(QStringLiteral("id"), prov.value("id").toString());
    entry.insert(QStringLiteral("name"), prov.value("name").toString());
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
    for (const auto &city_val : cities) {
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
      city_entry.insert(QStringLiteral("uv"), uv_list);
      city_list.push_back(city_entry);
    }
    entry.insert(QStringLiteral("cities"), city_list);

    m_province_labels.push_back(entry);
  }

  if (!m_province_overrides.isEmpty()) {
    QVariantList updated;
    updated.reserve(m_province_labels.size());
    for (const auto &entry_val : m_province_labels) {
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

void CampaignMapView::apply_province_state(const QVariantList &states) {
  QHash<QString, ProvinceVisual> next_overrides;
  next_overrides.reserve(states.size());

  for (const auto &state_val : states) {
    const QVariantMap state = state_val.toMap();
    const QString id = state.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
      continue;
    }

    ProvinceVisual visual;
    visual.owner = state.value(QStringLiteral("owner")).toString();

    const QVariantList color_list =
        state.value(QStringLiteral("color")).toList();
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
    for (auto &province : m_provinces) {
      const auto it = m_province_overrides.find(province.id);
      if (it != m_province_overrides.end() && !it->owner.isEmpty()) {
        province.owner = it->owner;
      }
    }
  }

  if (m_province_labels_loaded) {
    QVariantList updated;
    updated.reserve(m_province_labels.size());
    for (const auto &entry_val : m_province_labels) {
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

QString CampaignMapView::province_at_screen(float x, float y) {
  load_provinces_for_hit_test();
  if (m_provinces.empty()) {
    return {};
  }

  const float w = static_cast<float>(width());
  const float h = static_cast<float>(height());
  if (w <= 0.0F || h <= 0.0F) {
    return {};
  }

  const float ndc_x = (2.0F * x / w) - 1.0F;
  const float ndc_y = 1.0F - (2.0F * y / h);

  const QMatrix4x4 mvp = build_mvp_matrix(w, h, m_orbit_yaw, m_orbit_pitch,
                                          m_orbit_distance, m_pan_u, m_pan_v);
  bool inverted = false;
  const QMatrix4x4 inv = mvp.inverted(&inverted);
  if (!inverted) {
    return {};
  }

  QVector4D near_p = inv * QVector4D(ndc_x, ndc_y, -1.0F, 1.0F);
  QVector4D far_p = inv * QVector4D(ndc_x, ndc_y, 1.0F, 1.0F);
  if (qFuzzyIsNull(near_p.w()) || qFuzzyIsNull(far_p.w())) {
    return {};
  }
  QVector3D near_v = QVector3D(near_p.x(), near_p.y(), near_p.z()) / near_p.w();
  QVector3D far_v = QVector3D(far_p.x(), far_p.y(), far_p.z()) / far_p.w();

  const QVector3D dir = far_v - near_v;
  if (qFuzzyIsNull(dir.y())) {
    return {};
  }

  const float t = -near_v.y() / dir.y();
  if (t < 0.0F) {
    return {};
  }

  const QVector3D hit = near_v + dir * t;
  const float u = 1.0F - hit.x();
  const float v = hit.z();
  if (u < 0.0F || u > 1.0F || v < 0.0F || v > 1.0F) {
    return {};
  }

  const QVector2D p(u, v);
  for (const auto &province : m_provinces) {
    const auto &triangles = province.triangles;
    for (size_t i = 0; i + 2 < triangles.size(); i += 3) {
      if (point_in_triangle(p, triangles[i], triangles[i + 1],
                            triangles[i + 2])) {
        return province.id;
      }
    }
  }

  return {};
}

QVariantMap CampaignMapView::province_info_at_screen(float x, float y) {
  load_provinces_for_hit_test();
  QVariantMap info;
  if (m_provinces.empty()) {
    return info;
  }

  const float w = static_cast<float>(width());
  const float h = static_cast<float>(height());
  if (w <= 0.0F || h <= 0.0F) {
    return info;
  }

  const float ndc_x = (2.0F * x / w) - 1.0F;
  const float ndc_y = 1.0F - (2.0F * y / h);

  const QMatrix4x4 mvp = build_mvp_matrix(w, h, m_orbit_yaw, m_orbit_pitch,
                                          m_orbit_distance, m_pan_u, m_pan_v);
  bool inverted = false;
  const QMatrix4x4 inv = mvp.inverted(&inverted);
  if (!inverted) {
    return info;
  }

  QVector4D near_p = inv * QVector4D(ndc_x, ndc_y, -1.0F, 1.0F);
  QVector4D far_p = inv * QVector4D(ndc_x, ndc_y, 1.0F, 1.0F);
  if (qFuzzyIsNull(near_p.w()) || qFuzzyIsNull(far_p.w())) {
    return info;
  }
  QVector3D near_v = QVector3D(near_p.x(), near_p.y(), near_p.z()) / near_p.w();
  QVector3D far_v = QVector3D(far_p.x(), far_p.y(), far_p.z()) / far_p.w();

  const QVector3D dir = far_v - near_v;
  if (qFuzzyIsNull(dir.y())) {
    return info;
  }

  const float t = -near_v.y() / dir.y();
  if (t < 0.0F) {
    return info;
  }

  const QVector3D hit = near_v + dir * t;
  const float u = 1.0F - hit.x();
  const float v = hit.z();
  if (u < 0.0F || u > 1.0F || v < 0.0F || v > 1.0F) {
    return info;
  }

  const QVector2D p(u, v);
  for (const auto &province : m_provinces) {
    const auto &triangles = province.triangles;
    for (size_t i = 0; i + 2 < triangles.size(); i += 3) {
      if (point_in_triangle(p, triangles[i], triangles[i + 1],
                            triangles[i + 2])) {
        info.insert(QStringLiteral("id"), province.id);
        info.insert(QStringLiteral("name"), province.name);
        info.insert(QStringLiteral("owner"), province.owner);
        return info;
      }
    }
  }

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

  const QMatrix4x4 mvp = build_mvp_matrix(w, h, m_orbit_yaw, m_orbit_pitch,
                                          m_orbit_distance, m_pan_u, m_pan_v);
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

  const QString path = Utils::Resources::resolveResourcePath(
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
  for (const auto &line_val : lines) {
    const QJsonArray line = line_val.toArray();
    std::vector<QVector2D> path;
    path.reserve(static_cast<size_t>(line.size()));

    for (const auto &pt_val : line) {
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

  const int mission_idx = qBound(0, m_current_mission,
                                 static_cast<int>(m_hannibal_paths.size()) - 1);
  const auto &path = m_hannibal_paths[static_cast<size_t>(mission_idx)];

  if (path.empty()) {
    return {};
  }

  const QVector2D &endpoint = path.back();
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
  const float clamped =
      qBound(k_min_orbit_distance, distance, k_max_orbit_distance);
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

void CampaignMapView::set_hover_province_id(const QString &province_id) {
  if (m_hover_province_id == province_id) {
    return;
  }
  m_hover_province_id = province_id;
  emit hover_province_id_changed();
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

auto CampaignMapView::createRenderer() const -> Renderer * {
  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical()
        << "CampaignMapView::createRenderer() - No valid OpenGL context";
    qCritical()
        << "Running in software rendering mode - map view not available";
    return nullptr;
  }

  return new CampaignMapRenderer();
}
