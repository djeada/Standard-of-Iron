// Stage 13 — IRenderBackend implementation that routes to the
// SoftwareRasterizer. This is the runtime consumer for
// ShaderQuality::None. It translates a DrawQueue (specifically the
// MeshCmd + DrawPartCmd variants, which together describe every
// visible world entity) into ColoredTriangle primitives and renders
// to an internal QImage surface. Effects, particles and batched
// vegetation are deliberately skipped — this tier targets "looks
// acceptable, never blocks gameplay".

#pragma once

#include "draw_queue.h"
#include "frame_budget.h"
#include "i_render_backend.h"
#include "software/software_rasterizer.h"
#include <QImage>
#include <QString>

namespace Render::GL {

class SoftwareBackend : public IRenderBackend {
public:
  void initialize() override {}
  void begin_frame() override { m_rasterizer.clear(); }
  void execute(const DrawQueue &queue, const Camera &cam) override;

  void set_viewport(int w, int h) override {
    auto settings = m_rasterizer.settings();
    settings.width = w;
    settings.height = h;
    m_rasterizer = Render::Software::SoftwareRasterizer(settings);
  }
  void set_clear_color(float r, float g, float b, float a) override {
    auto settings = m_rasterizer.settings();
    settings.clear_color =
        QColor::fromRgbF(r, g, b, a);
    m_rasterizer = Render::Software::SoftwareRasterizer(settings);
  }
  void set_animation_time(float /*time*/) noexcept override {}
  void set_frame_budget(const Render::FrameBudgetConfig & /*config*/) override {
  }

  [[nodiscard]] auto resources() const -> ResourceManager * override {
    return nullptr;
  }
  [[nodiscard]] auto shader(const QString & /*name*/) const
      -> Shader * override {
    return nullptr;
  }
  [[nodiscard]] auto supports_shaders() const -> bool override { return false; }
  [[nodiscard]] auto shader_quality() const -> ShaderQuality override {
    return ShaderQuality::None;
  }
  [[nodiscard]] auto frame_tracker() const
      -> const Render::FrameTimeTracker * override {
    return nullptr;
  }

  void set_settings(const Render::Software::RasterSettings &settings) {
    m_rasterizer = Render::Software::SoftwareRasterizer(settings);
  }

  [[nodiscard]] auto last_frame() const -> const QImage & { return m_image; }
  [[nodiscard]] auto rasterizer() -> Render::Software::SoftwareRasterizer & {
    return m_rasterizer;
  }

private:
  Render::Software::SoftwareRasterizer m_rasterizer;
  QImage m_image;
};

} // namespace Render::GL
