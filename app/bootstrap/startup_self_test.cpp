#include "app/bootstrap/startup_self_test.h"

#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QString>

#include <array>

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

} // namespace App::Bootstrap
