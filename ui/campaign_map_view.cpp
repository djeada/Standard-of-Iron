#include "campaign_map_view.h"

#include "../utils/resource_utils.h"

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
#include <utility>
#include <vector>

namespace {

auto build_mvp_matrix(float width, float height, float yaw_deg, float pitch_deg,
                      float distance) -> QMatrix4x4 {
  const float view_w = qMax(1.0F, width);
  const float view_h = qMax(1.0F, height);
  const float aspect = view_w / view_h;

  QMatrix4x4 projection;
  projection.perspective(45.0F, aspect, 0.1F, 10.0F);

  const QVector3D center(0.5F, 0.0F, 0.5F);
  const float yaw_rad = qDegreesToRadians(yaw_deg);
  const float pitch_rad = qDegreesToRadians(pitch_deg);
  const float clamped_distance = qMax(1.0F, distance);

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
  QString id;
};

struct ProvinceLayer {
  GLuint vao = 0;
  GLuint vbo = 0;
  std::vector<ProvinceSpan> spans;
  bool ready = false;
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

    glClearColor(0.05F, 0.1F, 0.16F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 mvp;
    compute_mvp(mvp);

    draw_textured_layer(m_waterTexture, m_quadVao, 6, mvp, 1.0F, -0.01F);
    if (m_landVertexCount > 0) {
      draw_textured_layer(m_baseTexture, m_landVao, m_landVertexCount, mvp,
                          1.0F, 0.0F);
    } else {
      draw_textured_layer(m_baseTexture, m_quadVao, 6, mvp, 1.0F, 0.0F);
    }

    glDisable(GL_DEPTH_TEST);
    draw_province_layer(m_provinceLayer, mvp, 0.002F);
    draw_line_layer(m_provinceBorderLayer, mvp, 0.0045F);
    draw_line_layer(m_coastLayer, mvp, 0.004F);
    draw_line_layer(m_riverLayer, mvp, 0.003F);
    draw_line_layer(m_pathLayer, mvp, 0.006F);
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

    m_orbit_yaw = view->orbitYaw();
    m_orbit_pitch = view->orbitPitch();
    m_orbit_distance = view->orbitDistance();
    m_hover_province_id = view->hoverProvinceId();
  }

private:
  QSize m_size;
  bool m_initialized = false;

  QOpenGLShaderProgram m_textureProgram;
  QOpenGLShaderProgram m_lineProgram;

  GLuint m_quadVao = 0;
  GLuint m_quadVbo = 0;

  GLuint m_landVao = 0;
  GLuint m_landVbo = 0;
  int m_landVertexCount = 0;

  QOpenGLTexture *m_baseTexture = nullptr;
  QOpenGLTexture *m_waterTexture = nullptr;

  LineLayer m_coastLayer;
  LineLayer m_riverLayer;
  LineLayer m_pathLayer;
  LineLayer m_provinceBorderLayer;
  ProvinceLayer m_provinceLayer;

  float m_orbit_yaw = 180.0F;
  float m_orbit_pitch = 55.0F;
  float m_orbit_distance = 2.4F;
  QString m_hover_province_id;

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
    m_waterTexture = load_texture(
        QStringLiteral(":/assets/campaign_map/campaign_water.png"));
    m_baseTexture = load_texture(
        QStringLiteral(":/assets/campaign_map/campaign_base_color.png"));
    init_land_mesh();

    init_line_layer(m_coastLayer,
                    QStringLiteral(":/assets/campaign_map/coastlines_uv.json"),
                    QVector4D(0.22F, 0.19F, 0.16F, 1.0F), 1.4F);
    init_line_layer(m_riverLayer,
                    QStringLiteral(":/assets/campaign_map/rivers_uv.json"),
                    QVector4D(0.33F, 0.49F, 0.61F, 0.9F), 1.2F);
    init_line_layer(m_pathLayer,
                    QStringLiteral(":/assets/campaign_map/hannibal_path.json"),
                    QVector4D(0.78F, 0.2F, 0.12F, 0.9F), 2.0F);
    init_province_layer(m_provinceLayer,
                        QStringLiteral(":/assets/campaign_map/provinces.json"));
    init_borders_layer(m_provinceBorderLayer,
                       QStringLiteral(":/assets/campaign_map/provinces.json"),
                       QVector4D(0.18F, 0.16F, 0.14F, 0.85F), 1.6F);

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

    if (!m_textureProgram.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                  kTexVert)) {
      qWarning()
          << "CampaignMapRenderer: Failed to compile texture vertex shader";
      return false;
    }
    if (!m_textureProgram.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                  kTexFrag)) {
      qWarning()
          << "CampaignMapRenderer: Failed to compile texture fragment shader";
      return false;
    }
    if (!m_textureProgram.link()) {
      qWarning() << "CampaignMapRenderer: Failed to link texture shader";
      return false;
    }

    if (!m_lineProgram.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                               kLineVert)) {
      qWarning() << "CampaignMapRenderer: Failed to compile line vertex shader";
      return false;
    }
    if (!m_lineProgram.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                               kLineFrag)) {
      qWarning()
          << "CampaignMapRenderer: Failed to compile line fragment shader";
      return false;
    }
    if (!m_lineProgram.link()) {
      qWarning() << "CampaignMapRenderer: Failed to link line shader";
      return false;
    }

    return true;
  }

  void init_quad() {
    if (m_quadVao != 0) {
      return;
    }

    static const float kQuadVerts[] = {
        0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 1.0F,
    };

    glGenVertexArrays(1, &m_quadVao);
    glGenBuffers(1, &m_quadVbo);

    glBindVertexArray(m_quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
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

    m_landVertexCount = floatCount / 2;
    if (m_landVertexCount == 0) {
      return;
    }

    glGenVertexArrays(1, &m_landVao);
    glGenBuffers(1, &m_landVbo);

    glBindVertexArray(m_landVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_landVbo);
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
        spans.push_back({start, count, color, province_id});
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
    out_mvp = build_mvp_matrix(static_cast<float>(m_size.width()),
                               static_cast<float>(m_size.height()), m_orbit_yaw,
                               m_orbit_pitch, m_orbit_distance);
  }

  void draw_textured_layer(QOpenGLTexture *texture, GLuint vao,
                           int vertex_count, const QMatrix4x4 &mvp, float alpha,
                           float z_offset) {
    if (texture == nullptr || vao == 0 || vertex_count <= 0) {
      return;
    }

    m_textureProgram.bind();
    m_textureProgram.setUniformValue("u_mvp", mvp);
    m_textureProgram.setUniformValue("u_z", z_offset);
    m_textureProgram.setUniformValue("u_alpha", alpha);
    m_textureProgram.setUniformValue("u_tex", 0);

    glActiveTexture(GL_TEXTURE0);
    texture->bind();
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertex_count);
    glBindVertexArray(0);
    texture->release();
    m_textureProgram.release();
  }

  void draw_line_layer(const LineLayer &layer, const QMatrix4x4 &mvp,
                       float z_offset) {
    if (!layer.ready || layer.vao == 0 || layer.spans.empty()) {
      return;
    }

    glLineWidth(layer.width);

    m_lineProgram.bind();
    m_lineProgram.setUniformValue("u_mvp", mvp);
    m_lineProgram.setUniformValue("u_z", z_offset);
    m_lineProgram.setUniformValue("u_color", layer.color);

    glBindVertexArray(layer.vao);
    for (const auto &span : layer.spans) {
      glDrawArrays(GL_LINE_STRIP, span.start, span.count);
    }
    glBindVertexArray(0);
    m_lineProgram.release();
  }

  void draw_province_layer(const ProvinceLayer &layer, const QMatrix4x4 &mvp,
                           float z_offset) {
    if (!layer.ready || layer.vao == 0 || layer.spans.empty()) {
      return;
    }

    m_lineProgram.bind();
    m_lineProgram.setUniformValue("u_mvp", mvp);
    m_lineProgram.setUniformValue("u_z", z_offset);

    glBindVertexArray(layer.vao);
    for (const auto &span : layer.spans) {
      if (span.color.w() <= 0.0F) {
        continue;
      }
      QVector4D color = span.color;
      if (!m_hover_province_id.isEmpty() && span.id == m_hover_province_id) {
        color = QVector4D(
            qMin(1.0F, color.x() + 0.65F), qMin(1.0F, color.y() + 0.65F),
            qMin(1.0F, color.z() + 0.65F), qMin(1.0F, color.w() + 0.75F));
      }
      m_lineProgram.setUniformValue("u_color", color);
      glDrawArrays(GL_TRIANGLES, span.start, span.count);
    }
    glBindVertexArray(0);
    m_lineProgram.release();
  }

  void cleanup() {
    if (QOpenGLContext::currentContext() == nullptr) {
      return;
    }

    if (m_quadVbo != 0) {
      glDeleteBuffers(1, &m_quadVbo);
      m_quadVbo = 0;
    }
    if (m_quadVao != 0) {
      glDeleteVertexArrays(1, &m_quadVao);
      m_quadVao = 0;
    }
    if (m_landVbo != 0) {
      glDeleteBuffers(1, &m_landVbo);
      m_landVbo = 0;
    }
    if (m_landVao != 0) {
      glDeleteVertexArrays(1, &m_landVao);
      m_landVao = 0;
    }
    if (m_coastLayer.vbo != 0) {
      glDeleteBuffers(1, &m_coastLayer.vbo);
      m_coastLayer.vbo = 0;
    }
    if (m_coastLayer.vao != 0) {
      glDeleteVertexArrays(1, &m_coastLayer.vao);
      m_coastLayer.vao = 0;
    }
    if (m_riverLayer.vbo != 0) {
      glDeleteBuffers(1, &m_riverLayer.vbo);
      m_riverLayer.vbo = 0;
    }
    if (m_riverLayer.vao != 0) {
      glDeleteVertexArrays(1, &m_riverLayer.vao);
      m_riverLayer.vao = 0;
    }
    if (m_pathLayer.vbo != 0) {
      glDeleteBuffers(1, &m_pathLayer.vbo);
      m_pathLayer.vbo = 0;
    }
    if (m_pathLayer.vao != 0) {
      glDeleteVertexArrays(1, &m_pathLayer.vao);
      m_pathLayer.vao = 0;
    }
    if (m_provinceBorderLayer.vbo != 0) {
      glDeleteBuffers(1, &m_provinceBorderLayer.vbo);
      m_provinceBorderLayer.vbo = 0;
    }
    if (m_provinceBorderLayer.vao != 0) {
      glDeleteVertexArrays(1, &m_provinceBorderLayer.vao);
      m_provinceBorderLayer.vao = 0;
    }
    if (m_provinceLayer.vbo != 0) {
      glDeleteBuffers(1, &m_provinceLayer.vbo);
      m_provinceLayer.vbo = 0;
    }
    if (m_provinceLayer.vao != 0) {
      glDeleteVertexArrays(1, &m_provinceLayer.vao);
      m_provinceLayer.vao = 0;
    }

    delete m_baseTexture;
    m_baseTexture = nullptr;
    delete m_waterTexture;
    m_waterTexture = nullptr;
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

  emit provinceLabelsChanged();
}

QVariantList CampaignMapView::provinceLabels() {
  load_province_labels();
  return m_province_labels;
}

QString CampaignMapView::provinceAtScreen(float x, float y) {
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

  const QMatrix4x4 mvp =
      build_mvp_matrix(w, h, m_orbit_yaw, m_orbit_pitch, m_orbit_distance);
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

QVariantMap CampaignMapView::provinceInfoAtScreen(float x, float y) {
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

  const QMatrix4x4 mvp =
      build_mvp_matrix(w, h, m_orbit_yaw, m_orbit_pitch, m_orbit_distance);
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

QPointF CampaignMapView::screenPosForUv(float u, float v) {
  const float w = static_cast<float>(width());
  const float h = static_cast<float>(height());
  if (w <= 0.0F || h <= 0.0F) {
    return {};
  }

  const float clamped_u = qBound(0.0F, u, 1.0F);
  const float clamped_v = qBound(0.0F, v, 1.0F);

  const QMatrix4x4 mvp =
      build_mvp_matrix(w, h, m_orbit_yaw, m_orbit_pitch, m_orbit_distance);
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

void CampaignMapView::setOrbitYaw(float yaw) {
  if (qFuzzyCompare(m_orbit_yaw, yaw)) {
    return;
  }
  m_orbit_yaw = yaw;
  emit orbitYawChanged();
  update();
}

void CampaignMapView::setOrbitPitch(float pitch) {
  const float clamped = qBound(5.0F, pitch, 90.0F);
  if (qFuzzyCompare(m_orbit_pitch, clamped)) {
    return;
  }
  m_orbit_pitch = clamped;
  emit orbitPitchChanged();
  update();
}

void CampaignMapView::setOrbitDistance(float distance) {
  const float clamped = qBound(1.2F, distance, 5.0F);
  if (qFuzzyCompare(m_orbit_distance, clamped)) {
    return;
  }
  m_orbit_distance = clamped;
  emit orbitDistanceChanged();
  update();
}

void CampaignMapView::setHoverProvinceId(const QString &province_id) {
  if (m_hover_province_id == province_id) {
    return;
  }
  m_hover_province_id = province_id;
  emit hoverProvinceIdChanged();
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
