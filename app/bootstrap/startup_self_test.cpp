#include "app/bootstrap/startup_self_test.h"

#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QString>

#include <array>

#include "app/audio/audio_resource_loader.h"
#include "render/graphics_settings.h"
#include "render/horse/horse_source_asset.h"

namespace App::Bootstrap {

auto validate_release_campaign_map_resources() -> bool {
  constexpr std::array<const char*, 7> resources{
      ":/assets/campaign_map/campaign_base_color.png",
      ":/assets/campaign_map/campaign_water.png",
      ":/assets/campaign_map/coastlines_uv.json",
      ":/assets/campaign_map/rivers_uv.json",
      ":/assets/campaign_map/land_mesh.bin",
      ":/assets/campaign_map/provinces.json",
      ":/assets/campaign_map/terrain_height.png",
  };

  for (const char* path : resources) {
    QFile file(QString::fromLatin1(path));
    if (!file.open(QIODevice::ReadOnly) || file.size() < 32) {
      qCritical() << "SOI_CAMPAIGN_MAP_SELF_TEST: FAIL - missing or empty" << path;
      return false;
    }
  }
  qInfo() << "SOI_CAMPAIGN_MAP_SELF_TEST: PASS - all campaign map resources are "
             "embedded";
  return true;
}

auto check_audio_manifest() -> int {
  const QStringList missing_audio = AudioResourceLoader::missing_asset_ids();
  if (!missing_audio.isEmpty()) {
    qCritical() << "SOI_AUDIO_SELF_TEST: FAIL -" << missing_audio.size()
                << "manifest entries have no file on disk, first:"
                << missing_audio.first();
    return 11;
  }
  qInfo() << "SOI_AUDIO_SELF_TEST: PASS - every audio manifest entry resolves";
  return -1;
}

auto check_release_defaults() -> int {
  if (Render::GraphicsSettings::instance().quality() !=
      Render::k_default_graphics_quality) {
    qCritical() << "SOI_GRAPHICS_DEFAULT_SELF_TEST: FAIL - fresh profile is not "
                   "the default preset";
    return 14;
  }
  qInfo() << "SOI_GRAPHICS_DEFAULT_SELF_TEST: PASS - fresh profile uses the "
             "default preset";
  if (!App::Bootstrap::validate_release_campaign_map_resources()) {
    return 15;
  }
  const auto& horse_status = Render::Horse::horse_source_asset_status();
  if (!horse_status.loaded) {
    qCritical() << "SOI_CREATURE_ASSET_SELF_TEST: FAIL - horse asset:"
                << horse_status.error.c_str();
    return 16;
  }
  qInfo() << "SOI_CREATURE_ASSET_SELF_TEST: PASS - packaged horse asset loaded";
  return -1;
}

} // namespace App::Bootstrap
