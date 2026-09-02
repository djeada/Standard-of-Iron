#include "promo_spec.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <utility>

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
  } else if (mode == QStringLiteral("battle")) {
    focus.mode = FocusMode::Battle;
  } else if (mode == QStringLiteral("army")) {
    focus.mode = FocusMode::Army;
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
  focus.owner = object.value(QStringLiteral("owner")).toInt(focus.owner);
  focus.engagement_radius =
      static_cast<float>(object.value(QStringLiteral("engagement_radius"))
                             .toDouble(focus.engagement_radius));
  focus.home_radius = static_cast<float>(
      object.value(QStringLiteral("home_radius")).toDouble(focus.home_radius));

  if (focus.mode == FocusMode::Army && focus.owner <= 0) {
    if (error != nullptr) {
      *error = QStringLiteral("focus mode 'army' needs an 'owner' id");
    }
    return std::nullopt;
  }

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

auto shorter_arc(float from, float to) -> float {
  float delta = std::fmod(to - from, 360.0F);
  if (delta > 180.0F) {
    delta -= 360.0F;
  } else if (delta < -180.0F) {
    delta += 360.0F;
  }
  return delta;
}

auto lerp_yaw(float from, float to, float blend) -> float {
  return from + (shorter_arc(from, to) * blend);
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
    result.yaw = lerp_yaw(previous.yaw, next.yaw, blend);
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

namespace {

[[nodiscard]] auto windows_overlap(const Shot& lhs, const Shot& rhs) -> bool {
  const float lhs_end = lhs.start_seconds + lhs.duration_seconds;
  const float rhs_end = rhs.start_seconds + rhs.duration_seconds;
  return lhs.start_seconds < rhs_end && rhs.start_seconds < lhs_end;
}

} // namespace

auto plan_passes(const Spec& spec) -> std::vector<CapturePass> {
  std::vector<CapturePass> passes;
  for (std::size_t index = 0; index < spec.shots.size(); ++index) {
    const Shot& shot = spec.shots[index];
    CapturePass* home = nullptr;
    for (CapturePass& pass : passes) {
      if (pass.scenario != shot.scenario || pass.seed != shot.seed) {
        continue;
      }
      const bool clashes =
          std::any_of(pass.shots.begin(), pass.shots.end(), [&](std::size_t other) {
            return windows_overlap(spec.shots[other], shot);
          });
      if (!clashes) {
        home = &pass;
        break;
      }
    }
    if (home == nullptr) {
      passes.push_back(CapturePass{shot.scenario, shot.seed, {}});
      home = &passes.back();
    }
    home->shots.push_back(index);
  }
  for (CapturePass& pass : passes) {
    std::stable_sort(
        pass.shots.begin(), pass.shots.end(), [&](std::size_t lhs, std::size_t rhs) {
          return spec.shots[lhs].start_seconds < spec.shots[rhs].start_seconds;
        });
  }
  return passes;
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
  spec.audio = root.value(QStringLiteral("audio")).toBool(spec.audio);
  spec.music_track =
      root.value(QStringLiteral("music_track")).toString(spec.music_track).trimmed();
  spec.report_sound_decided = root.value(QStringLiteral("report_sound_decided"))
                                  .toString(spec.report_sound_decided)
                                  .trimmed();
  spec.report_sound_undecided = root.value(QStringLiteral("report_sound_undecided"))
                                    .toString(spec.report_sound_undecided)
                                    .trimmed();
  spec.report_sound_volume =
      std::clamp(static_cast<float>(root.value(QStringLiteral("report_sound_volume"))
                                        .toDouble(spec.report_sound_volume)),
                 0.0F,
                 1.0F);
  spec.music_volume = std::clamp(
      static_cast<float>(
          root.value(QStringLiteral("music_volume")).toDouble(spec.music_volume)),
      0.0F,
      1.0F);
  spec.gameplay_ui = root.value(QStringLiteral("gameplay_ui")).toBool(spec.gameplay_ui);
  spec.gameplay_ui_all_owners = root.value(QStringLiteral("gameplay_ui_all_owners"))
                                    .toBool(spec.gameplay_ui_all_owners);

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

  for (const QJsonValue shot_value : shots) {
    const QJsonObject shot_object = shot_value.toObject();
    Shot shot;
    shot.gameplay_ui = spec.gameplay_ui;
    shot.gameplay_ui_all_owners = spec.gameplay_ui_all_owners;
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
    shot.gameplay_camera = shot_object.value(QStringLiteral("gameplay_camera"))
                               .toBool(shot.gameplay_camera);
    shot.flame_card =
        shot_object.value(QStringLiteral("flame_card")).toBool(shot.flame_card);
    shot.flame_speed = static_cast<float>(
        shot_object.value(QStringLiteral("flame_speed")).toDouble(shot.flame_speed));
    shot.flame_intensity =
        static_cast<float>(shot_object.value(QStringLiteral("flame_intensity"))
                               .toDouble(shot.flame_intensity));
    shot.rpg_hud = shot_object.value(QStringLiteral("rpg_hud")).toBool(shot.rpg_hud);
    shot.gameplay_ui =
        shot_object.value(QStringLiteral("gameplay_ui")).toBool(shot.gameplay_ui);
    shot.gameplay_ui_all_owners =
        shot_object.value(QStringLiteral("gameplay_ui_all_owners"))
            .toBool(shot.gameplay_ui_all_owners);
    shot.report_card_seconds =
        std::clamp(static_cast<float>(shot_object.value(QStringLiteral("report_card"))
                                          .toDouble(shot.report_card_seconds)),
                   0.0F,
                   30.0F);

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

    if (shot.gameplay_camera || shot.flame_card) {

      spec.shots.push_back(std::move(shot));
      continue;
    }

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
    for (const QJsonValue key_value : keys) {
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

  if (auto const breaches = motion_violations(spec); !breaches.empty()) {
    if (error != nullptr) {
      QStringList lines;
      lines.reserve(static_cast<int>(breaches.size()));
      for (const QString& breach : breaches) {
        lines.push_back(breach);
      }
      *error = QStringLiteral("promo spec '%1' has camera work that is unwatchable "
                              "on a phone:\n  %2")
                   .arg(spec.id, lines.join(QStringLiteral("\n  ")));
    }
    return std::nullopt;
  }

  return spec;
}

auto motion_violations(const Spec& spec,
                       const MotionLimits& limits) -> std::vector<QString> {
  std::vector<QString> breaches;
  float total_clip_seconds = 0.0F;
  int measured_shots = 0;

  auto report = [&breaches](const QString& shot, const QString& what) {
    breaches.push_back(QStringLiteral("%1: %2").arg(shot, what));
  };

  for (const Shot& shot : spec.shots) {
    if (shot.flame_card) {
      continue;
    }
    float const clip_seconds = shot.duration_seconds * shot.slow_motion;
    total_clip_seconds += clip_seconds;
    ++measured_shots;

    if (clip_seconds + 1e-3F < limits.minimum_clip_seconds) {
      report(shot.name,
             QStringLiteral("is on screen for %1 s, under the %2 s a viewer needs to "
                            "read a frame")
                 .arg(clip_seconds, 0, 'f', 2)
                 .arg(limits.minimum_clip_seconds, 0, 'f', 2));
    }
    if (shot.shake > limits.shake + 1e-4F) {
      report(shot.name,
             QStringLiteral("shakes at %1, over the %2 ceiling")
                 .arg(shot.shake, 0, 'f', 3)
                 .arg(limits.shake, 0, 'f', 3));
    }
    if (shot.gameplay_camera) {
      continue;
    }

    for (const CameraKey& key : shot.keys) {
      if (std::abs(key.roll) > limits.roll_magnitude_degrees + 1e-4F) {
        report(shot.name,
               QStringLiteral("rolls the horizon %1 degrees, over the %2 ceiling")
                   .arg(std::abs(key.roll), 0, 'f', 1)
                   .arg(limits.roll_magnitude_degrees, 0, 'f', 1));
        break;
      }
    }

    for (std::size_t index = 1; index < shot.keys.size(); ++index) {
      const CameraKey& from = shot.keys[index - 1];
      const CameraKey& to = shot.keys[index];
      float const span = std::max(0.001F, to.time - from.time);

      auto rate = [&](const QString& what, float delta, float limit) {
        float const measured = std::abs(delta) / span;
        if (measured > limit + 1e-3F) {
          report(shot.name,
                 QStringLiteral("swings %1 at %2 deg/s, over the %3 deg/s ceiling")
                     .arg(what)
                     .arg(measured, 0, 'f', 1)
                     .arg(limit, 0, 'f', 1));
        }
      };

      rate(QStringLiteral("yaw"),
           shorter_arc(from.yaw, to.yaw),
           limits.yaw_degrees_per_second);
      rate(QStringLiteral("pitch"),
           to.pitch - from.pitch,
           limits.pitch_degrees_per_second);
      rate(QStringLiteral("fov"), to.fov - from.fov, limits.fov_degrees_per_second);
      rate(QStringLiteral("roll"), to.roll - from.roll, limits.roll_degrees_per_second);
    }
  }

  if (measured_shots > 0) {
    float const mean = total_clip_seconds / static_cast<float>(measured_shots);
    if (mean + 1e-3F < limits.mean_clip_seconds) {
      breaches.push_back(
          QStringLiteral("the cut averages %1 s a shot, under the %2 s that keeps a "
                         "reel from reading as strobing")
              .arg(mean, 0, 'f', 2)
              .arg(limits.mean_clip_seconds, 0, 'f', 2));
    }
  }

  return breaches;
}

} // namespace Arena::Promo
