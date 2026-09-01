#include "resource_json.h"

#include <QJsonObject>
#include <QJsonValue>

namespace Game::Systems {

auto read_resource_overlay(const QJsonObject& obj) -> ResourceOverlay {
  ResourceOverlay overlay;
  for (ResourceType const type : k_all_resource_types) {
    const auto value = obj.value(QLatin1String(resource_type_key(type)));
    if (value.isUndefined() || value.isNull()) {
      continue;
    }
    overlay.set(type, value.toInt(0));
  }
  return overlay;
}

} // namespace Game::Systems
