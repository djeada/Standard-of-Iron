#include "promo_spec.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace Arena::Promo {
namespace {

constexpr float k_min_slow_motion = 1.0F / 60.0F;

constexpr float k_world_edge_margin = 4.0F;

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
  focus.dead_zone = std::max(
      0.0F,
      static_cast<float>(
          object.value(QStringLiteral("dead_zone")).toDouble(focus.dead_zone)));
  focus.spring = object.value(QStringLiteral("spring")).toBool(false);
  focus.lead_seconds =
      std::max(0.0F,
               static_cast<float>(
                   object.value(QStringLiteral("lead")).toDouble(focus.lead_seconds)));

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

template <typename Value>
auto hermite(const Value& p0,
             const Value& m0,
             const Value& p1,
             const Value& m1,
             float span,
             float u) -> Value {
  const float u2 = u * u;
  const float u3 = u2 * u;
  const float h00 = (2.0F * u3) - (3.0F * u2) + 1.0F;
  const float h10 = u3 - (2.0F * u2) + u;
  const float h01 = (-2.0F * u3) + (3.0F * u2);
  const float h11 = u3 - u2;
  return (p0 * h00) + (m0 * (h10 * span)) + (p1 * h01) + (m1 * (h11 * span));
}

template <typename Value, typename Sample>
auto spline_at(std::size_t count,
               const Sample& sample,
               const std::vector<float>& times,
               float shot_time,
               Ends ends) -> Value {
  if (count == 1U || shot_time <= times.front()) {
    return sample(0U);
  }
  if (shot_time >= times.back()) {
    return sample(count - 1U);
  }
  auto tangent = [&](std::size_t index) -> Value {
    if (index == 0U) {
      if (ends == Ends::Ease) {
        return sample(0U) * 0.0F;
      }
      return (sample(1U) - sample(0U)) / std::max(1e-4F, times[1] - times[0]);
    }
    if (index == count - 1U) {
      if (ends == Ends::Ease) {
        return sample(0U) * 0.0F;
      }
      return (sample(index) - sample(index - 1U)) /
             std::max(1e-4F, times[index] - times[index - 1U]);
    }
    return (sample(index + 1U) - sample(index - 1U)) /
           std::max(1e-4F, times[index + 1U] - times[index - 1U]);
  };
  std::size_t segment = 1U;
  while (segment < count - 1U && shot_time > times[segment]) {
    ++segment;
  }
  const float span = std::max(1e-4F, times[segment] - times[segment - 1U]);
  const float u = std::clamp((shot_time - times[segment - 1U]) / span, 0.0F, 1.0F);
  return hermite(sample(segment - 1U),
                 tangent(segment - 1U),
                 sample(segment),
                 tangent(segment),
                 span,
                 u);
}

auto smooth_noise(float t, int seed) -> float {
  const float a = 1.0F + (0.37F * static_cast<float>(seed % 7));
  const float b = 2.31F + (0.21F * static_cast<float>(seed % 5));
  const float c = 4.73F + (0.13F * static_cast<float>(seed % 3));
  return (0.58F * std::sin((t * a) + (1.7F * static_cast<float>(seed)))) +
         (0.29F * std::sin((t * b) + (0.3F * static_cast<float>(seed)))) +
         (0.13F * std::sin((t * c) + (2.9F * static_cast<float>(seed))));
}

} // namespace

auto evaluate_spline(const std::vector<CameraKey>& keys,
                     float shot_time,
                     Ends ends) -> Pose {
  Pose pose;
  if (keys.empty()) {
    return pose;
  }
  std::vector<float> times;
  times.reserve(keys.size());
  std::vector<float> yaws;
  yaws.reserve(keys.size());
  for (const CameraKey& key : keys) {
    times.push_back(key.time);
    yaws.push_back(yaws.empty() ? key.yaw
                                : yaws.back() + shorter_arc(yaws.back(), key.yaw));
  }
  const std::size_t count = keys.size();
  auto channel = [&](auto field) {
    return spline_at<float>(
        count, [&](std::size_t index) { return field(index); }, times, shot_time, ends);
  };
  pose.distance =
      std::max(0.2F, channel([&](std::size_t index) { return keys[index].distance; }));
  pose.pitch = channel([&](std::size_t index) { return keys[index].pitch; });
  pose.yaw = channel([&](std::size_t index) { return yaws[index]; });
  pose.fov = channel([&](std::size_t index) { return keys[index].fov; });
  pose.roll = channel([&](std::size_t index) { return keys[index].roll; });
  pose.height = channel([&](std::size_t index) { return keys[index].height; });
  return pose;
}

auto evaluate_free(const std::vector<FreeKey>& keys,
                   float shot_time,
                   Ends ends) -> FreePose {
  FreePose pose;
  if (keys.empty()) {
    return pose;
  }
  std::vector<float> times;
  times.reserve(keys.size());
  for (const FreeKey& key : keys) {
    times.push_back(key.time);
  }
  const std::size_t count = keys.size();
  pose.eye = spline_at<QVector3D>(
      count,
      [&](std::size_t index) { return keys[index].eye; },
      times,
      shot_time,
      ends);
  pose.look = spline_at<QVector3D>(
      count,
      [&](std::size_t index) { return keys[index].look; },
      times,
      shot_time,
      ends);
  pose.fov = spline_at<float>(
      count,
      [&](std::size_t index) { return keys[index].fov; },
      times,
      shot_time,
      ends);
  pose.roll = spline_at<float>(
      count,
      [&](std::size_t index) { return keys[index].roll; },
      times,
      shot_time,
      ends);
  return pose;
}

auto handheld_wobble(const Handheld& handheld,
                     const std::vector<Jolt>& jolts,
                     float shot_time) -> Wobble {
  Wobble wobble;
  if (handheld.degrees > 0.0F) {
    const float t = shot_time * handheld.frequency * 2.0F * std::numbers::pi_v<float>;
    wobble.yaw = handheld.degrees * smooth_noise(t, handheld.seed);
    wobble.pitch =
        handheld.degrees * 0.7F * smooth_noise(t * 1.13F, handheld.seed + 11);
    wobble.roll =
        handheld.degrees * 0.45F * smooth_noise(t * 0.87F, handheld.seed + 23);
  }
  for (const Jolt& jolt : jolts) {
    const float since = shot_time - jolt.at;
    if (since < 0.0F) {
      continue;
    }
    const float envelope =
        jolt.degrees * std::exp(-since / std::max(0.02F, jolt.decay));
    if (envelope < 1e-3F) {
      continue;
    }
    const float t = since * 2.0F * std::numbers::pi_v<float>;
    wobble.yaw += envelope * 0.6F * std::sin(t * 9.0F + 0.4F);
    wobble.pitch += envelope * std::sin(t * 13.0F);
    wobble.roll += envelope * 0.35F * std::sin(t * 7.0F + 1.1F);
  }
  return wobble;
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
  spec.record_music =
      root.value(QStringLiteral("record_music")).toBool(spec.record_music);
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
  spec.reel_loudness_lufs =
      std::clamp(static_cast<float>(root.value(QStringLiteral("reel_loudness_lufs"))
                                        .toDouble(spec.reel_loudness_lufs)),
                 -70.0F,
                 0.0F);
  spec.music_volume = std::clamp(
      static_cast<float>(
          root.value(QStringLiteral("music_volume")).toDouble(spec.music_volume)),
      0.0F,
      1.0F);
  if (const QJsonValue limits_value = root.value(QStringLiteral("motion_limits"));
      limits_value.isObject()) {
    const QJsonObject limits = limits_value.toObject();
    auto read_rate = [&limits](const char* key, float& field) {
      field = std::max(
          0.0F, static_cast<float>(limits.value(QLatin1String(key)).toDouble(field)));
    };
    read_rate("yaw_degrees_per_second", spec.motion_limits.yaw_degrees_per_second);
    read_rate("pitch_degrees_per_second", spec.motion_limits.pitch_degrees_per_second);
    read_rate("fov_degrees_per_second", spec.motion_limits.fov_degrees_per_second);
    read_rate("roll_degrees_per_second", spec.motion_limits.roll_degrees_per_second);
    read_rate("roll_magnitude_degrees", spec.motion_limits.roll_magnitude_degrees);
    read_rate("minimum_clip_seconds", spec.motion_limits.minimum_clip_seconds);
    read_rate("mean_clip_seconds", spec.motion_limits.mean_clip_seconds);
  }
  spec.gameplay_ui = root.value(QStringLiteral("gameplay_ui")).toBool(spec.gameplay_ui);
  spec.gameplay_ui_all_owners = root.value(QStringLiteral("gameplay_ui_all_owners"))
                                    .toBool(spec.gameplay_ui_all_owners);
  spec.casting_overlay =
      root.value(QStringLiteral("casting_overlay")).toBool(spec.casting_overlay);
  spec.require_decision =
      root.value(QStringLiteral("require_decision")).toBool(spec.require_decision);
  spec.forbid_world_edge =
      root.value(QStringLiteral("forbid_world_edge")).toBool(spec.forbid_world_edge);

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
    if (shot_object.contains(QStringLiteral("clip"))) {

      continue;
    }
    Shot shot;
    shot.gameplay_ui = spec.gameplay_ui;
    shot.gameplay_ui_all_owners = spec.gameplay_ui_all_owners;
    shot.casting_overlay = spec.casting_overlay;
    shot.name = shot_object.value(QStringLiteral("name")).toString();
    shot.scenario = shot_object.value(QStringLiteral("scenario")).toString().trimmed();
    shot.seed = shot_object.value(QStringLiteral("seed")).toInt(shot.seed);
    shot.start_seconds = static_cast<float>(
        shot_object.value(QStringLiteral("start")).toDouble(shot.start_seconds));
    shot.duration_seconds = static_cast<float>(
        shot_object.value(QStringLiteral("duration")).toDouble(shot.duration_seconds));
    shot.slow_motion = static_cast<float>(
        shot_object.value(QStringLiteral("slow_motion")).toDouble(shot.slow_motion));
    if (const QJsonValue time_lapse = shot_object.value(QStringLiteral("time_lapse"));
        time_lapse.isDouble()) {
      if (shot_object.contains(QStringLiteral("slow_motion"))) {
        if (error != nullptr) {
          *error = QStringLiteral("shot '%1' sets both slow_motion and time_lapse; "
                                  "they are the same knob")
                       .arg(shot.name);
        }
        return std::nullopt;
      }
      const float factor = static_cast<float>(time_lapse.toDouble(1.0));
      if (factor < 1.0F) {
        if (error != nullptr) {
          *error = QStringLiteral("shot '%1': time_lapse must be at least 1 (use "
                                  "slow_motion to slow a shot down)")
                       .arg(shot.name);
        }
        return std::nullopt;
      }
      shot.slow_motion = 1.0F / factor;
    }
    shot.casting_overlay = shot_object.value(QStringLiteral("casting_overlay"))
                               .toBool(shot.casting_overlay);
    if (const QJsonValue start_on = shot_object.value(QStringLiteral("start_on"));
        start_on.isObject()) {
      const QJsonObject trigger = start_on.toObject();
      StartOn resolved;
      resolved.event = trigger.value(QStringLiteral("event")).toString().trimmed();
      resolved.side = trigger.value(QStringLiteral("side")).toString().trimmed();
      resolved.offset_seconds =
          static_cast<float>(trigger.value(QStringLiteral("offset")).toDouble(0.0));
      if (!known_start_event(resolved.event)) {
        if (error != nullptr) {
          *error = QStringLiteral("shot '%1': unknown start_on event '%2' (one of "
                                  "%3, %4, %5, %6)")
                       .arg(shot.name,
                            resolved.event,
                            QLatin1String(k_event_first_wave),
                            QLatin1String(k_event_first_contact),
                            QLatin1String(k_event_first_building_lost),
                            QLatin1String(k_event_decision));
        }
        return std::nullopt;
      }
      if (shot_object.contains(QStringLiteral("start"))) {
        if (error != nullptr) {
          *error =
              QStringLiteral("shot '%1' sets both start and start_on").arg(shot.name);
        }
        return std::nullopt;
      }
      shot.start_on = resolved;
    }
    shot.shake = static_cast<float>(
        shot_object.value(QStringLiteral("shake")).toDouble(shot.shake));
    shot.gameplay_camera = shot_object.value(QStringLiteral("gameplay_camera"))
                               .toBool(shot.gameplay_camera);
    shot.stabilize_seconds = std::max(
        0.0F,
        static_cast<float>(shot_object.value(QStringLiteral("stabilize_seconds"))
                               .toDouble(shot.stabilize_seconds)));
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

    if (const QJsonValue lighting = shot_object.value(QStringLiteral("lighting"));
        lighting.isObject()) {
      const QJsonObject light = lighting.toObject();
      auto number = [&light](const char* key) -> std::optional<float> {
        const QJsonValue value = light.value(QLatin1String(key));
        if (!value.isDouble()) {
          return std::nullopt;
        }
        return static_cast<float>(value.toDouble());
      };
      auto colour = [&light](const char* key) -> std::optional<QVector3D> {
        const QJsonValue value = light.value(QLatin1String(key));
        if (!value.isArray() || value.toArray().size() < 3) {
          return std::nullopt;
        }
        return parse_vector(value, {});
      };
      shot.lighting.hour = number("hour");
      shot.lighting.sun_azimuth = number("sun_azimuth");
      shot.lighting.sun_elevation = number("sun_elevation");
      shot.lighting.sun_scale = number("sun_scale");
      shot.lighting.sun_color = colour("sun_color");
      shot.lighting.ambient_scale = number("ambient_scale");
      shot.lighting.sky_color = colour("sky_color");
      shot.lighting.fog_color = colour("fog_color");
      shot.lighting.fog_density = number("fog_density");
      shot.lighting.exposure = number("exposure");
      shot.lighting.shadow_strength = number("shadow_strength");
      shot.lighting.shadow_softness = number("shadow_softness");
    }
    {
      const QString rig = shot_object.value(QStringLiteral("rig")).toString().toLower();
      shot.rig = rig == QStringLiteral("free") ? Rig::Free : Rig::Orbit;
      const QString interp =
          shot_object.value(QStringLiteral("interp")).toString().toLower();
      shot.interp = (interp == QStringLiteral("spline") || shot.rig == Rig::Free)
                        ? Interp::Spline
                        : Interp::Keys;
      shot.ends = shot_object.value(QStringLiteral("ends")).toString().toLower() ==
                          QStringLiteral("ease")
                      ? Ends::Ease
                      : Ends::Moving;
      auto space = [&shot_object](const char* key) {
        return shot_object.value(QLatin1String(key)).toString().toLower() ==
                       QStringLiteral("world")
                   ? Space::World
                   : Space::Focus;
      };
      shot.eye_space = space("eye_space");
      shot.look_space = space("look_space");
      shot.terrain_relative =
          shot_object.value(QStringLiteral("terrain_relative")).toBool(true);
      shot.near_plane =
          static_cast<float>(shot_object.value(QStringLiteral("near")).toDouble(0.0));
      shot.ground_clearance = static_cast<float>(
          shot_object.value(QStringLiteral("ground_clearance")).toDouble(-1.0));
      if (const QJsonValue handheld = shot_object.value(QStringLiteral("handheld"));
          handheld.isObject()) {
        const QJsonObject hand = handheld.toObject();
        shot.handheld.degrees =
            static_cast<float>(hand.value(QStringLiteral("degrees")).toDouble(0.0));
        shot.handheld.frequency =
            static_cast<float>(hand.value(QStringLiteral("frequency")).toDouble(0.35));
        shot.handheld.seed = hand.value(QStringLiteral("seed")).toInt(7);
      }
      for (const QJsonValue jolt_value :
           shot_object.value(QStringLiteral("jolts")).toArray()) {
        const QJsonObject jolt_object = jolt_value.toObject();
        Jolt jolt;
        jolt.at =
            static_cast<float>(jolt_object.value(QStringLiteral("at")).toDouble(0.0));
        jolt.degrees = static_cast<float>(
            jolt_object.value(QStringLiteral("degrees")).toDouble(0.4));
        jolt.decay = static_cast<float>(
            jolt_object.value(QStringLiteral("decay")).toDouble(0.35));
        shot.jolts.push_back(jolt);
      }
    }

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
    shot.slow_motion = std::clamp(shot.slow_motion, k_min_slow_motion, 8.0F);

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
    if (shot.rig == Rig::Free && !keys.isEmpty()) {
      for (const QJsonValue key_value : keys) {
        const QJsonObject key_object = key_value.toObject();
        FreeKey key;
        key.time =
            static_cast<float>(key_object.value(QStringLiteral("time")).toDouble(0.0));
        key.eye = parse_vector(key_object.value(QStringLiteral("eye")), {});
        key.look = parse_vector(key_object.value(QStringLiteral("look")), {});
        key.fov = static_cast<float>(
            key_object.value(QStringLiteral("fov")).toDouble(key.fov));
        key.roll = static_cast<float>(
            key_object.value(QStringLiteral("roll")).toDouble(key.roll));
        if (key.fov < 5.0F || key.fov > 120.0F ||
            (key.eye - key.look).length() < 0.05F) {
          if (error != nullptr) {
            *error = QStringLiteral("shot '%1' has an out-of-range free camera key")
                         .arg(shot.name);
          }
          return std::nullopt;
        }
        shot.free_keys.push_back(key);
      }
      std::stable_sort(
          shot.free_keys.begin(),
          shot.free_keys.end(),
          [](const FreeKey& lhs, const FreeKey& rhs) { return lhs.time < rhs.time; });
      spec.shots.push_back(std::move(shot));
      continue;
    }
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

  if (auto const breaches = motion_violations(spec, spec.motion_limits);
      !breaches.empty()) {
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

      float const span = std::max(0.001F, (to.time - from.time) * shot.slow_motion);

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

namespace Arena::Promo {

auto known_start_event(const QString& event) -> bool {
  return event == QLatin1String(k_event_first_wave) ||
         event == QLatin1String(k_event_first_contact) ||
         event == QLatin1String(k_event_first_building_lost) ||
         event == QLatin1String(k_event_decision);
}

auto uses_start_events(const Spec& spec) -> bool {
  return std::any_of(spec.shots.begin(), spec.shots.end(), [](const Shot& shot) {
    return shot.start_on.has_value();
  });
}

auto view_ground_footprint(const Pose& pose,
                           const QVector3D& focus,
                           float aspect,
                           float ground_y) -> GroundFootprint {

  const float pitch = qDegreesToRadians(pose.pitch);
  const float yaw = qDegreesToRadians(pose.yaw);
  const float horizontal = pose.distance * std::cos(pitch);
  const QVector3D target = focus + QVector3D(0.0F, pose.height, 0.0F);
  const QVector3D position = target + QVector3D(std::sin(yaw) * horizontal,
                                                pose.distance * std::sin(pitch),
                                                std::cos(yaw) * horizontal);
  const QVector3D forward = (target - position).normalized();
  QVector3D right = QVector3D::crossProduct(forward, QVector3D(0.0F, 1.0F, 0.0F));
  if (right.lengthSquared() < 1e-6F) {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  }
  right.normalize();
  const QVector3D up = QVector3D::crossProduct(right, forward).normalized();

  const float tan_vertical = std::tan(qDegreesToRadians(pose.fov) * 0.5F);
  const float tan_horizontal = tan_vertical * std::max(0.01F, aspect);

  GroundFootprint footprint;
  for (const float sx : {-1.0F, 1.0F}) {
    for (const float sy : {-1.0F, 1.0F}) {
      const QVector3D ray =
          forward + (right * (sx * tan_horizontal)) + (up * (sy * tan_vertical));
      if (ray.y() >= -1e-4F) {
        footprint.horizon_visible = true;
        continue;
      }
      const float t = (ground_y - position.y()) / ray.y();
      if (t <= 0.0F) {
        footprint.horizon_visible = true;
        continue;
      }
      const QVector3D hit = position + (ray * t);
      footprint.max_abs_extent = std::max(
          footprint.max_abs_extent, std::max(std::abs(hit.x()), std::abs(hit.z())));
    }
  }
  return footprint;
}

auto frames_world_edge(const GroundFootprint& footprint,
                       float floor_half_extent) -> bool {
  return footprint.horizon_visible ||
         footprint.max_abs_extent > floor_half_extent + k_world_edge_margin;
}

auto world_edge_violations(const Spec& spec,
                           float floor_half_extent) -> std::vector<QString> {
  std::vector<QString> breaches;
  const float aspect =
      static_cast<float>(spec.width) / static_cast<float>(std::max(1, spec.height));
  for (const Shot& shot : spec.shots) {
    if (shot.flame_card || shot.gameplay_camera ||
        shot.focus.mode != FocusMode::Point || shot.keys.empty()) {
      continue;
    }
    std::vector<float> sample_times;
    for (const CameraKey& key : shot.keys) {
      sample_times.push_back(key.time);
    }
    sample_times.push_back(shot.duration_seconds);
    for (const float time : sample_times) {
      const Pose pose = evaluate(shot.keys, time);
      const GroundFootprint footprint = view_ground_footprint(
          pose, shot.focus.point + shot.focus.offset, aspect, 0.0F);
      if (!frames_world_edge(footprint, floor_half_extent)) {
        continue;
      }
      breaches.push_back(
          footprint.horizon_visible
              ? QStringLiteral("%1: at %2 s the camera sees over the horizon (pitch "
                               "%3 with fov %4)")
                    .arg(shot.name)
                    .arg(time, 0, 'f', 1)
                    .arg(pose.pitch, 0, 'f', 1)
                    .arg(pose.fov, 0, 'f', 1)
              : QStringLiteral("%1: at %2 s the frame reaches %3 m from the centre, "
                               "past the %4 m arena floor")
                    .arg(shot.name)
                    .arg(time, 0, 'f', 1)
                    .arg(footprint.max_abs_extent, 0, 'f', 1)
                    .arg(floor_half_extent, 0, 'f', 1));
      break;
    }
  }
  return breaches;
}

} // namespace Arena::Promo
