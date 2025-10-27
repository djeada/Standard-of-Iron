#include "json_vec_utils.h"
#include <qjsonarray.h>
#include <qvectornd.h>

namespace App::JsonUtils {

auto vec3ToJsonArray(const QVector3D &vec) -> QJsonArray {
  QJsonArray arr;
  arr.append(vec.x());
  arr.append(vec.y());
  arr.append(vec.z());
  return arr;
}

auto jsonArrayToVec3(const QJsonValue &value,
                     const QVector3D &fallback) -> QVector3D {
  if (!value.isArray()) {
    return fallback;
  }
  const auto arr = value.toArray();
  if (arr.size() < 3) {
    return fallback;
  }
  return {static_cast<float>(arr.at(0).toDouble(fallback.x())),
          static_cast<float>(arr.at(1).toDouble(fallback.y())),
          static_cast<float>(arr.at(2).toDouble(fallback.z()))};
}

} // namespace App::JsonUtils
