#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QVector3D>

namespace App::JsonUtils {

auto vec3ToJsonArray(const QVector3D &vec) -> QJsonArray;

auto jsonArrayToVec3(const QJsonValue &value,
                     const QVector3D &fallback) -> QVector3D;

} 
