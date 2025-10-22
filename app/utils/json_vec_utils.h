#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QVector3D>

namespace App {
namespace JsonUtils {

QJsonArray vec3ToJsonArray(const QVector3D &vec);

QVector3D jsonArrayToVec3(const QJsonValue &value, const QVector3D &fallback);

} 
} 
