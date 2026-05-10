#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QVector3D>

namespace App::JsonUtils {

auto vec3_to_json_array(const QVector3D &vec) -> QJsonArray;

auto json_array_to_vec3(const QJsonValue &value,
                     const QVector3D &fallback) -> QVector3D;

} // namespace App::JsonUtils
