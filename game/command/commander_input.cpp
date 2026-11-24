#include "commander_input.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QLatin1String>

#include <array>
#include <cmath>
#include <limits>

#include "json_u64.h"

namespace Game::Command {
namespace {

struct CommanderActionName {
  CommanderAction action;
  const char* name;
};

constexpr std::array<CommanderActionName, 10> k_commander_action_names{{
    {CommanderAction::EnterControl, "enter_control"},
    {CommanderAction::ExitControl, "exit_control"},
    {CommanderAction::ToggleControl, "toggle_control"},
    {CommanderAction::CycleLockOn, "cycle_lock_on"},
    {CommanderAction::ToggleCameraMode, "toggle_camera_mode"},
    {CommanderAction::ToggleWeaponStance, "toggle_weapon_stance"},
    {CommanderAction::ReleaseForRally, "release_for_rally"},
    {CommanderAction::ToggleLockOn, "toggle_lock_on"},
    {CommanderAction::SwitchLockLeft, "switch_lock_left"},
    {CommanderAction::SwitchLockRight, "switch_lock_right"},
}};

auto finite_float(const QJsonValue& value) -> std::optional<float> {
  const double number = value.toDouble(std::numeric_limits<double>::infinity());
  if (!std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max()) {
    return std::nullopt;
  }
  return static_cast<float>(number);
}

auto read_vector(const QJsonArray& values, int index) -> std::optional<QVector3D> {
  const auto x = finite_float(values.at(index));
  const auto y = finite_float(values.at(index + 1));
  const auto z = finite_float(values.at(index + 2));
  if (!x || !y || !z) {
    return std::nullopt;
  }
  return QVector3D(*x, *y, *z);
}

} // namespace

auto to_json(const CommanderInputFrame& frame) -> QJsonObject {
  QJsonObject object;
  object["commander_input"] = u64_to_json(frame.commander);
  object["buttons"] = u64_to_json(frame.buttons);
  object["view_yaw"] = static_cast<double>(frame.view_yaw);
  object["view_pitch"] = static_cast<double>(frame.view_pitch);
  object["sequence"] = u64_to_json(frame.sequence);
  if (frame.held(CommanderInputFrame::HasDodgeDirection)) {
    QJsonArray direction;
    direction.append(static_cast<double>(frame.dodge_direction.x()));
    direction.append(static_cast<double>(frame.dodge_direction.y()));
    direction.append(static_cast<double>(frame.dodge_direction.z()));
    object["dodge"] = direction;
  }
  if (frame.held(CommanderInputFrame::HasAimRay)) {
    object["aim"] = QJsonArray{static_cast<double>(frame.aim_origin.x()),
                               static_cast<double>(frame.aim_origin.y()),
                               static_cast<double>(frame.aim_origin.z()),
                               static_cast<double>(frame.aim_forward.x()),
                               static_cast<double>(frame.aim_forward.y()),
                               static_cast<double>(frame.aim_forward.z())};
  }
  return object;
}

auto commander_input_from_json(const QJsonObject& object)
    -> std::optional<CommanderInputFrame> {
  const auto commander = u64_from_json(object.value(QLatin1String("commander_input")));
  const auto buttons = u64_from_json(object.value(QLatin1String("buttons")));
  const auto sequence = u64_from_json(object.value(QLatin1String("sequence")));
  const auto yaw = finite_float(object.value(QLatin1String("view_yaw")));
  const auto pitch = finite_float(object.value(QLatin1String("view_pitch")));
  constexpr std::uint64_t all_buttons = (CommanderInputFrame::HasAimRay << 1U) - 1U;
  if (!commander || !buttons || *buttons > all_buttons || !sequence || !yaw || !pitch) {
    return std::nullopt;
  }
  CommanderInputFrame frame{.commander = *commander,
                            .buttons = static_cast<std::uint32_t>(*buttons),
                            .view_yaw = *yaw,
                            .view_pitch = *pitch,
                            .sequence = *sequence};
  if (frame.held(CommanderInputFrame::HasDodgeDirection)) {
    const auto values = object.value(QLatin1String("dodge")).toArray();
    const auto direction = values.size() == 3 ? read_vector(values, 0) : std::nullopt;
    if (!direction) {
      return std::nullopt;
    }
    frame.dodge_direction = *direction;
  }
  if (frame.held(CommanderInputFrame::HasAimRay)) {
    const auto values = object.value(QLatin1String("aim")).toArray();
    if (values.size() != 6) {
      return std::nullopt;
    }
    const auto origin = read_vector(values, 0);
    const auto forward = read_vector(values, 3);
    if (!origin || !forward) {
      return std::nullopt;
    }
    frame.aim_origin = *origin;
    frame.aim_forward = *forward;
  }
  return frame;
}

auto to_json(const CommanderActionRecord& record) -> QJsonObject {
  QJsonObject object;
  for (const auto& entry : k_commander_action_names) {
    if (entry.action == record.action) {
      object["commander_action"] = QLatin1String(entry.name);
      break;
    }
  }
  object["view_yaw"] = static_cast<double>(record.view_yaw);
  object["view_pitch"] = static_cast<double>(record.view_pitch);
  return object;
}

auto commander_action_from_json(const QJsonObject& object)
    -> std::optional<CommanderActionRecord> {
  const QString name = object.value(QLatin1String("commander_action")).toString();
  for (const auto& entry : k_commander_action_names) {
    if (name == QLatin1String(entry.name)) {
      const auto yaw = finite_float(object.value(QLatin1String("view_yaw")));
      const auto pitch = finite_float(object.value(QLatin1String("view_pitch")));
      if (!yaw || !pitch) {
        return std::nullopt;
      }
      return CommanderActionRecord{
          .action = entry.action, .view_yaw = *yaw, .view_pitch = *pitch};
    }
  }
  return std::nullopt;
}

} // namespace Game::Command
