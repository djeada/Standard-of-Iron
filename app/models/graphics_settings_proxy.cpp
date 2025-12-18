#include "graphics_settings_proxy.h"

#include "../../render/graphics_settings.h"

namespace App::Models {

GraphicsSettingsProxy::GraphicsSettingsProxy(QObject *parent)
    : QObject(parent) {}

int GraphicsSettingsProxy::quality_level() const {
  return static_cast<int>(Render::GraphicsSettings::instance().quality());
}

void GraphicsSettingsProxy::set_quality_level(int level) {
  if (level < 0 || level > 3) {
    return;
  }

  auto newQuality = static_cast<Render::GraphicsQuality>(level);
  if (newQuality != Render::GraphicsSettings::instance().quality()) {
    Render::GraphicsSettings::instance().setQuality(newQuality);
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

void GraphicsSettingsProxy::set_quality_by_name(const QString &name) {
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
    return tr(
        "Maximum performance. Aggressive LOD, reduced detail at distance.");
  case Render::GraphicsQuality::Medium:
    return tr(
        "Balanced performance and quality. Recommended for most systems.");
  case Render::GraphicsQuality::High:
    return tr("Higher quality. More detail visible at distance. Requires "
              "better hardware.");
  case Render::GraphicsQuality::Ultra:
    return tr(
        "Maximum quality. Full detail always. Best hardware recommended.");
  }
  return QString();
}

} 
