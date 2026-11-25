#include "app/models/graphics_settings_proxy.h"

#include <QDebug>

#include "app/core/user_settings.h"
#include "render/frame/graphics_settings.h"

namespace App::Models {

GraphicsSettingsProxy::GraphicsSettingsProxy(QObject* parent)
    : QObject(parent) {
  const auto& graphics = Render::GraphicsSettings::instance();
  m_seen_generation = graphics.generation();
  constexpr int k_poll_interval_ms = 500;
  m_poll.setInterval(k_poll_interval_ms);
  connect(
      &m_poll, &QTimer::timeout, this, &GraphicsSettingsProxy::poll_render_settings);
  m_poll.start();
}

void GraphicsSettingsProxy::poll_render_settings() {
  const auto& graphics = Render::GraphicsSettings::instance();
  const std::uint32_t generation = graphics.generation();
  if (generation != m_seen_generation) {
    m_seen_generation = generation;
    emit quality_level_changed();
  }
}

int GraphicsSettingsProxy::quality_level() const {
  return static_cast<int>(Render::GraphicsSettings::instance().quality());
}

void GraphicsSettingsProxy::set_quality_level(int level) {
  if (level < 0 || level > 3) {
    qWarning() << "Ignoring invalid graphics quality level:" << level;
    return;
  }

  auto newQuality = static_cast<Render::GraphicsQuality>(level);
  // Picking the preset the game chose automatically still counts as the
  // player's choice: it stops the game from changing it again.
  if (newQuality != Render::GraphicsSettings::instance().quality() ||
      !Render::GraphicsSettings::instance().quality_chosen_by_user()) {
    Render::GraphicsSettings::instance().set_quality(newQuality);
    App::Core::UserSettings::save_graphics_quality_level(level);
    m_seen_generation = Render::GraphicsSettings::instance().generation();
    emit quality_level_changed();
  }
}

QString GraphicsSettingsProxy::quality_name() const {
  switch (Render::GraphicsSettings::instance().quality()) {
  case Render::GraphicsQuality::Low:
    return tr("Low");
  case Render::GraphicsQuality::Medium:
    return tr("Medium");
  case Render::GraphicsQuality::High:
    return tr("High");
  case Render::GraphicsQuality::Ultra:
    return tr("Ultra");
  }
  return tr("Medium");
}

QStringList GraphicsSettingsProxy::quality_options() const {
  return {tr("Low"), tr("Medium"), tr("High"), tr("Ultra")};
}

void GraphicsSettingsProxy::set_quality_by_name(const QString& name) {
  if (name == tr("Low")) {
    set_quality_level(0);
  } else if (name == tr("Medium")) {
    set_quality_level(1);
  } else if (name == tr("High")) {
    set_quality_level(2);
  } else if (name == tr("Ultra")) {
    set_quality_level(3);
  }
}

QString GraphicsSettingsProxy::get_quality_description() const {
  switch (Render::GraphicsSettings::instance().quality()) {
  case Render::GraphicsQuality::Low:
    return tr("Playable on weak hardware: no cast shadows or post-processing, "
              "sparse grass, reduced detail beyond a few metres.");
  case Render::GraphicsQuality::Medium:
    return tr("Small cast shadows, lighter shaders, reduced detail at distance. "
              "For integrated graphics.");
  case Render::GraphicsQuality::High:
    return tr("Everything as designed: full detail everywhere, full shadows and "
              "post-processing. Recommended.");
  case Render::GraphicsQuality::Ultra:
    return tr("High plus contact-hardening shadows, foliage translucency, extra "
              "terrain detail and 8x anti-aliasing. Needs a strong GPU.");
  }
  return {};
}

} // namespace App::Models
