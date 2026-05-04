#include "render/draw_queue.h"
#include "render/gl/backend.h"
#include "render/gl/camera.h"
#include "render/gl/shader.h"
#include "render/humanoid/humanoid_spec.h"
#include "render/rigged_mesh_cache.h"

#include <QByteArray>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QVector3D>
#include <gtest/gtest.h>

#include <optional>

namespace {

using Render::Creature::CreatureLOD;
using Render::GL::Backend;
using Render::GL::Camera;
using Render::GL::DrawQueue;
using Render::GL::EffectBatchCmd;
using Render::GL::MeshCmd;
using Render::GL::RiggedCreatureCmd;
using Render::GL::RiggedMeshCache;
using Render::GL::ShaderBindAuditEntry;

class ScopedEnvVar {
public:
  ScopedEnvVar(const char *name, const char *value) : m_name(name) {
    if (qEnvironmentVariableIsSet(name)) {
      m_old = qgetenv(name);
    }
    qputenv(name, value);
  }

  ~ScopedEnvVar() {
    if (m_old.has_value()) {
      qputenv(m_name, *m_old);
    } else {
      qunsetenv(m_name);
    }
  }

private:
  const char *m_name;
  std::optional<QByteArray> m_old;
};

class ScopedShaderAudit {
public:
  ScopedShaderAudit() : m_was_enabled(Render::GL::shader_bind_audit_enabled()) {
    Render::GL::reset_shader_bind_audit();
    Render::GL::set_shader_bind_audit_enabled(true);
  }

  ~ScopedShaderAudit() {
    Render::GL::set_shader_bind_audit_enabled(m_was_enabled);
    Render::GL::reset_shader_bind_audit();
  }

private:
  bool m_was_enabled;
};

class ScopedOffscreenGlContext {
public:
  ScopedOffscreenGlContext() {
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);

    m_surface.setFormat(format);
    m_surface.create();

    m_context.setFormat(format);
    if (!m_context.create()) {
      return;
    }
    m_valid = m_context.makeCurrent(&m_surface);
  }

  ~ScopedOffscreenGlContext() {
    if (m_valid) {
      m_context.doneCurrent();
    }
  }

  [[nodiscard]] bool valid() const noexcept { return m_valid; }

private:
  QOffscreenSurface m_surface;
  QOpenGLContext m_context;
  bool m_valid{false};
};

[[nodiscard]] auto make_camera() -> Camera {
  Camera cam;
  cam.set_perspective(60.0F, 4.0F / 3.0F, 0.1F, 100.0F);
  cam.look_at(QVector3D(0.0F, 5.0F, 8.0F), QVector3D(0.0F, 0.5F, 0.0F),
              QVector3D(0.0F, 1.0F, 0.0F));
  return cam;
}

void append_humanoid_rigged_pair(RiggedMeshCache &cache, DrawQueue &queue) {
  auto const &spec = Render::Humanoid::humanoid_creature_spec();
  auto const bind = Render::Humanoid::humanoid_bind_palette();
  auto const *entry = cache.get_or_bake(spec, CreatureLOD::Full, bind);
  ASSERT_NE(entry, nullptr);
  ASSERT_NE(entry->mesh, nullptr);

  RiggedCreatureCmd cmd{};
  cmd.mesh = entry->mesh.get();
  cmd.bone_palette = bind.data();
  cmd.bone_count = static_cast<std::uint32_t>(bind.size());
  cmd.role_color_count = 0;
  cmd.alpha = 1.0F;

  RiggedCreatureCmd second = cmd;
  second.world.translate(0.8F, 0.0F, 0.0F);

  queue.submit(cmd);
  queue.submit(second);
}

void append_banner_draw(Backend &backend, DrawQueue &queue) {
  MeshCmd banner{};
  banner.mesh = backend.banner_mesh();
  banner.shader = backend.banner_shader();
  banner.color = QVector3D(0.75F, 0.10F, 0.10F);
  banner.alpha = 1.0F;
  banner.model.translate(-1.5F, 0.0F, 0.0F);
  ASSERT_NE(banner.mesh, nullptr);
  ASSERT_NE(banner.shader, nullptr);
  queue.submit(banner);
}

void append_combat_dust(DrawQueue &queue) {
  EffectBatchCmd dust{};
  dust.kind = EffectBatchCmd::Kind::CombatDust;
  dust.position = QVector3D(0.0F, 0.1F, 0.5F);
  dust.color = QVector3D(0.45F, 0.35F, 0.25F);
  dust.radius = 0.9F;
  dust.intensity = 1.0F;
  dust.time = 0.25F;
  queue.submit(dust);
}

[[nodiscard]] auto
contains_shader(const std::vector<ShaderBindAuditEntry> &entries,
                const QString &name) -> bool {
  for (const auto &entry : entries) {
    if (entry.name == name) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] auto contains_legacy_troop_shader_name(
    const std::vector<ShaderBindAuditEntry> &entries) -> bool {
  for (const auto &entry : entries) {
    if (entry.name.contains(QStringLiteral("swordsman")) ||
        entry.name.contains(QStringLiteral("spearman")) ||
        entry.name.contains(QStringLiteral("archer")) ||
        entry.name.contains(QStringLiteral("builder")) ||
        entry.name.contains(QStringLiteral("healer")) ||
        entry.name.contains(QStringLiteral("horse_"))) {
      return true;
    }
  }
  return false;
}

TEST(ShaderAuditIntegration, RepresentativeBattleQueueUsesCanonicalShaders) {
  ScopedOffscreenGlContext gl_context;
  if (!gl_context.valid()) {
    GTEST_SKIP() << "OpenGL offscreen context unavailable";
  }

  ScopedEnvVar enable_instancing("SOI_RENDER_ENABLE_RIGGED_INSTANCING", "1");
  ScopedShaderAudit audit;

  auto camera = make_camera();
  std::vector<ShaderBindAuditEntry> entries;
  {
    Backend backend;
    backend.initialize();
    backend.set_viewport(256, 192);
    backend.begin_frame();

    DrawQueue queue;
    RiggedMeshCache cache;
    append_humanoid_rigged_pair(cache, queue);
    append_banner_draw(backend, queue);
    append_combat_dust(queue);
    queue.sort_for_batching();

    backend.execute(queue, camera);
    entries = Render::GL::shader_bind_audit_snapshot();
  }

  ASSERT_FALSE(entries.empty());
  EXPECT_TRUE(contains_shader(entries, QStringLiteral("banner")));
  EXPECT_TRUE(contains_shader(entries, QStringLiteral("combat_dust")));
  EXPECT_TRUE(
      contains_shader(entries, QStringLiteral("character_skinned")) ||
      contains_shader(entries, QStringLiteral("character_skinned_instanced")));
  EXPECT_FALSE(contains_legacy_troop_shader_name(entries));

  for (const auto &entry : entries) {
    EXPECT_NE(Render::GL::classify_shader_for_audit(entry.name),
              QStringLiteral("unused/dead-or-dynamic"))
        << entry.name.toStdString();
  }
}

} // namespace
