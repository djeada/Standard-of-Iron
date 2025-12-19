#include "campaign_map_view.h"

#include "../utils/resource_utils.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMatrix4x4>
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

class CampaignMapRenderer : public QQuickFramebufferObject::Renderer,
                            protected QOpenGLFunctions_3_3_Core {
public:
  CampaignMapRenderer() = default;
  ~CampaignMapRenderer() override { cleanup(); }

  void render() override {
    if (!ensureInitialized()) {
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
    computeMvp(mvp);

    drawTexturedLayer(m_waterTexture, m_quadVao, 6, mvp, 1.0F, -0.01F);
    if (m_landVertexCount > 0) {
      drawTexturedLayer(m_baseTexture, m_landVao, m_landVertexCount, mvp, 1.0F,
                        0.0F);
    } else {
      drawTexturedLayer(m_baseTexture, m_quadVao, 6, mvp, 1.0F, 0.0F);
    }

    glDisable(GL_DEPTH_TEST);
    drawLineLayer(m_coastLayer, mvp, 0.004F);
    drawLineLayer(m_riverLayer, mvp, 0.003F);
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

    m_orbitYaw = view->orbitYaw();
    m_orbitPitch = view->orbitPitch();
    m_orbitDistance = view->orbitDistance();
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

  float m_orbitYaw = 35.0F;
  float m_orbitPitch = 35.0F;
  float m_orbitDistance = 2.2F;

  auto ensureInitialized() -> bool {
    if (m_initialized) {
      return true;
    }

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx == nullptr || !ctx->isValid()) {
      qWarning() << "CampaignMapRenderer: No valid OpenGL context";
      return false;
    }

    initializeOpenGLFunctions();

    if (!initShaders()) {
      return false;
    }

    initQuad();
    m_waterTexture = loadTexture(QStringLiteral(":/assets/campaign_map/campaign_water.png"));
    m_baseTexture = loadTexture(QStringLiteral(":/assets/campaign_map/campaign_base_color.png"));
    initLandMesh();

    initLineLayer(m_coastLayer,
                  QStringLiteral(":/assets/campaign_map/coastlines_uv.json"),
                  QVector4D(0.22F, 0.19F, 0.16F, 1.0F), 1.4F);
    initLineLayer(m_riverLayer,
                  QStringLiteral(":/assets/campaign_map/rivers_uv.json"),
                  QVector4D(0.33F, 0.49F, 0.61F, 0.9F), 1.2F);

    m_initialized = true;
    return true;
  }

  auto initShaders() -> bool {
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
      qWarning() << "CampaignMapRenderer: Failed to compile texture vertex shader";
      return false;
    }
    if (!m_textureProgram.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                  kTexFrag)) {
      qWarning() << "CampaignMapRenderer: Failed to compile texture fragment shader";
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
      qWarning() << "CampaignMapRenderer: Failed to compile line fragment shader";
      return false;
    }
    if (!m_lineProgram.link()) {
      qWarning() << "CampaignMapRenderer: Failed to link line shader";
      return false;
    }

    return true;
  }

  void initQuad() {
    if (m_quadVao != 0) {
      return;
    }

    static const float kQuadVerts[] = {
        0.0F, 0.0F, 1.0F, 0.0F, 1.0F, 1.0F,
        0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 1.0F,
    };

    glGenVertexArrays(1, &m_quadVao);
    glGenBuffers(1, &m_quadVbo);

    glBindVertexArray(m_quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glBindVertexArray(0);
  }

  void initLandMesh() {
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

  void initLineLayer(LineLayer &layer, const QString &resource_path,
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

  auto loadTexture(const QString &resource_path) -> QOpenGLTexture * {
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

  void computeMvp(QMatrix4x4 &out_mvp) const {
    const float view_w = qMax(1, m_size.width());
    const float view_h = qMax(1, m_size.height());
    const float aspect = view_w / view_h;

    QMatrix4x4 projection;
    projection.perspective(45.0F, aspect, 0.1F, 10.0F);

    const QVector3D center(0.5F, 0.0F, 0.5F);
    const float yaw_rad = qDegreesToRadians(m_orbitYaw);
    const float pitch_rad = qDegreesToRadians(m_orbitPitch);
    const float distance = qMax(1.0F, m_orbitDistance);

    const float cos_pitch = std::cos(pitch_rad);
    const float sin_pitch = std::sin(pitch_rad);
    const float cos_yaw = std::cos(yaw_rad);
    const float sin_yaw = std::sin(yaw_rad);

    QVector3D eye(center.x() + distance * sin_yaw * cos_pitch,
                  center.y() + distance * sin_pitch,
                  center.z() + distance * cos_yaw * cos_pitch);

    QMatrix4x4 view;
    view.lookAt(eye, center, QVector3D(0.0F, 0.0F, 1.0F));

    QMatrix4x4 model;
    out_mvp = projection * view * model;
  }

  void drawTexturedLayer(QOpenGLTexture *texture, GLuint vao, int vertex_count,
                         const QMatrix4x4 &mvp, float alpha, float z_offset) {
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

  void drawLineLayer(const LineLayer &layer, const QMatrix4x4 &mvp,
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
    qWarning() << "CampaignMapView: 3D rendering will not work in software mode";
    qWarning() << "CampaignMapView: Try running without QT_QUICK_BACKEND=software";
  }
}

void CampaignMapView::setOrbitYaw(float yaw) {
  if (qFuzzyCompare(m_orbitYaw, yaw)) {
    return;
  }
  m_orbitYaw = yaw;
  emit orbitYawChanged();
  update();
}

void CampaignMapView::setOrbitPitch(float pitch) {
  const float clamped = qBound(5.0F, pitch, 85.0F);
  if (qFuzzyCompare(m_orbitPitch, clamped)) {
    return;
  }
  m_orbitPitch = clamped;
  emit orbitPitchChanged();
  update();
}

void CampaignMapView::setOrbitDistance(float distance) {
  const float clamped = qBound(1.2F, distance, 5.0F);
  if (qFuzzyCompare(m_orbitDistance, clamped)) {
    return;
  }
  m_orbitDistance = clamped;
  emit orbitDistanceChanged();
  update();
}

auto CampaignMapView::createRenderer() const -> Renderer * {
  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical() << "CampaignMapView::createRenderer() - No valid OpenGL context";
    qCritical() << "Running in software rendering mode - map view not available";
    return nullptr;
  }

  return new CampaignMapRenderer();
}
