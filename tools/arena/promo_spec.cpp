#include "promo_spec.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>
#include <cmath>

namespace Arena::Promo {
namespace {

auto parse_vector(const QJsonValue& value, const QVector3D& fallback) -> QVector3D {
  if (!value.isArray()) {
    return fallback;
  }
  const QJsonArray array = value.toArray();
  if (array.size() < 3) {
    return fallback;
  }
  return {static_cast<float>(array.at(0).toDouble()),
          static_cast<float>(array.at(1).toDouble()),
          static_cast<float>(array.at(2).toDouble())};
}

auto parse_ease(const QString& name) -> Ease {
  const QString normalized = name.trimmed().toLower();
  if (normalized == QStringLiteral("linear")) {
    return Ease::Linear;
  }
  if (normalized == QStringLiteral("in")) {
    return Ease::EaseIn;
  }
  if (normalized == QStringLiteral("out")) {
    return Ease::EaseOut;
  }
  return Ease::Smooth;
}

auto parse_focus(const QJsonObject& object, QString* error) -> std::optional<Focus> {
  Focus focus;
  const QString mode =
      object.value(QStringLiteral("mode")).toString().trimmed().toLower();
  if (mode.isEmpty() || mode == QStringLiteral("all")) {
    focus.mode = FocusMode::AllUnits;
  } else if (mode == QStringLiteral("point")) {
    focus.mode = FocusMode::Point;
  } else if (mode == QStringLiteral("group")) {
    focus.mode = FocusMode::Group;
  } else if (mode == QStringLiteral("group_pair")) {
    focus.mode = FocusMode::GroupPair;
  } else {
    if (error != nullptr) {
      *error = QStringLiteral("unknown focus mode '%1'").arg(mode);
    }
    return std::nullopt;
  }

  focus.point = parse_vector(object.value(QStringLiteral("point")), {});
  focus.group = object.value(QStringLiteral("group")).toString();
  focus.second_group = object.value(QStringLiteral("second_group")).toString();
  focus.offset = parse_vector(object.value(QStringLiteral("offset")), {});
  focus.smoothing = static_cast<float>(
      object.value(QStringLiteral("smoothing")).toDouble(focus.smoothing));

  if (focus.mode == FocusMode::Group && focus.group.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("focus mode 'group' needs a 'group' name");
    }
    return std::nullopt;
  }
  if (focus.mode == FocusMode::GroupPair &&
      (focus.group.isEmpty() || focus.second_group.isEmpty())) {
    if (error != nullptr) {
      *error =
          QStringLiteral("focus mode 'group_pair' needs 'group' and 'second_group'");
    }
    return std::nullopt;
  }
  return focus;
}

auto ease_value(Ease ease, float t) -> float {
  const float clamped = std::clamp(t, 0.0F, 1.0F);
  switch (ease) {
  case Ease::Linear:
    return clamped;
  case Ease::EaseIn:
    return clamped * clamped * clamped;
  case Ease::EaseOut: {
    const float inverted = 1.0F - clamped;
    return 1.0F - (inverted * inverted * inverted);
  }
  case Ease::Smooth:
    break;
  }
  return clamped * clamped * (3.0F - (2.0F * clamped));
}

auto hash_noise(int seed) -> float {
  auto value = static_cast<std::uint32_t>(seed) * 747796405U + 2891336453U;
  value = ((value >> ((value >> 28U) + 4U)) ^ value) * 277803737U;
  value = (value >> 22U) ^ value;
  return (static_cast<float>(value & 0xFFFFU) / 32767.5F) - 1.0F;
}

} // namespace

auto evaluate(const std::vector<CameraKey>& keys, float shot_time) -> Pose {
  Pose pose;
  if (keys.empty()) {
    return pose;
  }

  auto to_pose = [](const CameraKey& key) {
    Pose result;
    result.distance = key.distance;
    result.pitch = key.pitch;
    result.yaw = key.yaw;
    result.fov = key.fov;
    result.roll = key.roll;
    result.height = key.height;
    return result;
  };

  if (shot_time <= keys.front().time || keys.size() == 1U) {
    return to_pose(keys.front());
  }
  if (shot_time >= keys.back().time) {
    return to_pose(keys.back());
  }

  for (std::size_t index = 1; index < keys.size(); ++index) {
    const CameraKey& next = keys[index];
    if (shot_time > next.time) {
      continue;
    }
    const CameraKey& previous = keys[index - 1U];
    const float span = next.time - previous.time;
    const float raw = span > 0.0F ? (shot_time - previous.time) / span : 1.0F;
    const float blend = ease_value(next.ease, raw);
    Pose result;
    result.distance = std::lerp(previous.distance, next.distance, blend);
    result.pitch = std::lerp(previous.pitch, next.pitch, blend);
    result.yaw = std::lerp(previous.yaw, next.yaw, blend);
    result.fov = std::lerp(previous.fov, next.fov, blend);
    result.roll = std::lerp(previous.roll, next.roll, blend);
    result.height = std::lerp(previous.height, next.height, blend);
    return result;
  }
  return to_pose(keys.back());
}

auto shake_offset(int frame_index, float amount) -> QVector3D {
  if (amount <= 0.0F) {
    return {};
  }
  return {hash_noise(frame_index * 3) * amount,
          hash_noise((frame_index * 3) + 1) * amount * 0.5F,
          hash_noise((frame_index * 3) + 2) * amount};
}

auto load(const QString& path, QString* error) -> std::optional<Spec> {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error != nullptr) {
      *error = QStringLiteral("could not open promo spec '%1'").arg(path);
    }
    return std::nullopt;
  }

  QJsonParseError parse_error{};
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
  if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
    if (error != nullptr) {
      *error = QStringLiteral("invalid promo spec '%1': %2")
                   .arg(path, parse_error.errorString());
    }
    return std::nullopt;
  }

  const QJsonObject root = document.object();
  Spec spec;
  spec.id = root.value(QStringLiteral("id")).toString();
  spec.title = root.value(QStringLiteral("title")).toString();
  spec.width = root.value(QStringLiteral("width")).toInt(spec.width);
  spec.height = root.value(QStringLiteral("height")).toInt(spec.height);
  spec.fps = root.value(QStringLiteral("fps")).toInt(spec.fps);
  spec.supersample = root.value(QStringLiteral("supersample")).toInt(spec.supersample);

  if (spec.id.trimmed().isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("promo spec needs an 'id'");
    }
    return std::nullopt;
  }
  if (spec.width < 16 || spec.height < 16 || spec.width > 7680 || spec.height > 7680) {
    if (error != nullptr) {
      *error = QStringLiteral("promo spec resolution %1x%2 is out of range")
                   .arg(spec.width)
                   .arg(spec.height);
    }
    return std::nullopt;
  }

  if ((spec.width % 2) != 0 || (spec.height % 2) != 0) {
    if (error != nullptr) {
      *error = QStringLiteral("promo spec resolution must be even in both axes");
    }
    return std::nullopt;
  }
  if (spec.fps < 1 || spec.fps > 240) {
    if (error != nullptr) {
      *error = QStringLiteral("promo spec fps %1 is out of range").arg(spec.fps);
    }
    return std::nullopt;
  }
  spec.supersample = std::clamp(spec.supersample, 1, 4);

  const QJsonArray shots = root.value(QStringLiteral("shots")).toArray();
  if (shots.isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("promo spec '%1' declares no shots").arg(spec.id);
    }
    return std::nullopt;
  }

  for (const QJsonValue& shot_value : shots) {
    const QJsonObject shot_object = shot_value.toObject();
    Shot shot;
    shot.name = shot_object.value(QStringLiteral("name")).toString();
    shot.scenario = shot_object.value(QStringLiteral("scenario")).toString().trimmed();
    shot.seed = shot_object.value(QStringLiteral("seed")).toInt(shot.seed);
    shot.start_seconds = static_cast<float>(
        shot_object.value(QStringLiteral("start")).toDouble(shot.start_seconds));
    shot.duration_seconds = static_cast<float>(
        shot_object.value(QStringLiteral("duration")).toDouble(shot.duration_seconds));
    shot.slow_motion = static_cast<float>(
        shot_object.value(QStringLiteral("slow_motion")).toDouble(shot.slow_motion));
    shot.shake = static_cast<float>(
        shot_object.value(QStringLiteral("shake")).toDouble(shot.shake));

    if (shot.scenario.isEmpty()) {
      if (error != nullptr) {
        *error = QStringLiteral("shot '%1' has no scenario").arg(shot.name);
      }
      return std::nullopt;
    }
    if (shot.name.trimmed().isEmpty()) {
      shot.name = QStringLiteral("shot_%1").arg(
          spec.shots.size() + 1U, 2, 10, QLatin1Char('0'));
    }
    if (shot.duration_seconds <= 0.0F || shot.start_seconds < 0.0F) {
      if (error != nullptr) {
        *error =
            QStringLiteral("shot '%1' has an invalid start or duration").arg(shot.name);
      }
      return std::nullopt;
    }
    shot.slow_motion = std::clamp(shot.slow_motion, 0.05F, 8.0F);

    QString focus_error;
    auto focus = parse_focus(shot_object.value(QStringLiteral("focus")).toObject(),
                             &focus_error);
    if (!focus.has_value()) {
      if (error != nullptr) {
        *error = QStringLiteral("shot '%1': %2").arg(shot.name, focus_error);
      }
      return std::nullopt;
    }
    shot.focus = *focus;

    const QJsonArray keys = shot_object.value(QStringLiteral("camera")).toArray();
    if (keys.isEmpty()) {
      if (error != nullptr) {
        *error = QStringLiteral("shot '%1' has no camera keyframes").arg(shot.name);
      }
      return std::nullopt;
    }
    for (const QJsonValue& key_value : keys) {
      const QJsonObject key_object = key_value.toObject();
      CameraKey key;
      key.time =
          static_cast<float>(key_object.value(QStringLiteral("time")).toDouble(0.0));
      key.distance = static_cast<float>(
          key_object.value(QStringLiteral("distance")).toDouble(key.distance));
      key.pitch = static_cast<float>(
          key_object.value(QStringLiteral("pitch")).toDouble(key.pitch));
      key.yaw =
          static_cast<float>(key_object.value(QStringLiteral("yaw")).toDouble(key.yaw));
      key.fov =
          static_cast<float>(key_object.value(QStringLiteral("fov")).toDouble(key.fov));
      key.roll = static_cast<float>(
          key_object.value(QStringLiteral("roll")).toDouble(key.roll));
      key.height = static_cast<float>(
          key_object.value(QStringLiteral("height")).toDouble(key.height));
      key.ease = parse_ease(key_object.value(QStringLiteral("ease")).toString());
      if (key.distance <= 0.05F || key.fov < 5.0F || key.fov > 120.0F) {
        if (error != nullptr) {
          *error = QStringLiteral("shot '%1' has an out-of-range camera keyframe")
                       .arg(shot.name);
        }
        return std::nullopt;
      }
      shot.keys.push_back(key);
    }
    std::stable_sort(
        shot.keys.begin(),
        shot.keys.end(),
        [](const CameraKey& lhs, const CameraKey& rhs) { return lhs.time < rhs.time; });
    spec.shots.push_back(std::move(shot));
  }

  return spec;
}

} // namespace Arena::Promo
