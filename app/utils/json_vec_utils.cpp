#include "json_vec_utils.h"

namespace App {
namespace JsonUtils {

QJsonArray vec3ToJsonArray(const QVector3D &vec) {
  QJsonArray arr;
  arr.append(vec.x());
  arr.append(vec.y());
  arr.append(vec.z());
  return arr;
}

QVector3D jsonArrayToVec3(const QJsonValue &value, const QVector3D &fallback) {
  if (!value.isArray()) {
    return fallback;
  }
  const auto arr = value.toArray();
  if (arr.size() < 3) {
    return fallback;
  }
  return QVector3D(static_cast<float>(arr.at(0).toDouble(fallback.x())),
                   static_cast<float>(arr.at(1).toDouble(fallback.y())),
                   static_cast<float>(arr.at(2).toDouble(fallback.z())));
}

} // namespace JsonUtils
} // namespace App
