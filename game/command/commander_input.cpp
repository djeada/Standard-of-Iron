#include "commander_input.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QLatin1String>

#include <array>

namespace Game::Command {
namespace {

struct CommanderActionName {
  CommanderAction action;
  const char* name;
};

constexpr std::array<CommanderActionName, 6> k_commander_action_names{{
    {CommanderAction::EnterControl, "enter_control"},
    {CommanderAction::ExitControl, "exit_control"},
    {CommanderAction::CycleLockOn, "cycle_lock_on"},
    {CommanderAction::ToggleCameraMode, "toggle_camera_mode"},
    {CommanderAction::ToggleWeaponStance, "toggle_weapon_stance"},
    {CommanderAction::ReleaseForRally, "release_for_rally"},
}};

} // namespace

auto to_json(const CommanderInputFrame& frame) -> QJsonObject {
  QJsonObject object;
  object["commander_input"] = static_cast<qint64>(frame.commander);
  object["buttons"] = static_cast<qint64>(frame.buttons);
  object["view_yaw"] = static_cast<double>(frame.view_yaw);
  object["view_pitch"] = static_cast<double>(frame.view_pitch);
  object["sequence"] = static_cast<qint64>(frame.sequence);
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
  if (!object.contains(QLatin1String("commander_input"))) {
    return std::nullopt;
  }

  CommanderInputFrame frame;
  frame.commander = static_cast<Engine::Core::EntityID>(
      object.value(QLatin1String("commander_input")).toVariant().toULongLong());
  frame.buttons = static_cast<std::uint32_t>(
      object.value(QLatin1String("buttons")).toVariant().toUInt());
  frame.view_yaw =
      static_cast<float>(object.value(QLatin1String("view_yaw")).toDouble(0.0));
  frame.view_pitch =
      static_cast<float>(object.value(QLatin1String("view_pitch")).toDouble(0.0));
  frame.sequence = static_cast<std::uint64_t>(
      object.value(QLatin1String("sequence")).toVariant().toULongLong());

  const auto direction = object.value(QLatin1String("dodge"));
  if (direction.isArray()) {
    const QJsonArray values = direction.toArray();
    if (values.size() != 3) {
      return std::nullopt;
    }
    frame.dodge_direction = QVector3D(static_cast<float>(values.at(0).toDouble()),
                                      static_cast<float>(values.at(1).toDouble()),
                                      static_cast<float>(values.at(2).toDouble()));
  }

  const auto aim = object.value(QLatin1String("aim"));
  if (aim.isArray()) {
    const QJsonArray values = aim.toArray();
    if (values.size() != 6) {
      return std::nullopt;
    }
    const auto at = [&values](int index) {
      return static_cast<float>(values.at(index).toDouble());
    };
    frame.aim_origin = QVector3D(at(0), at(1), at(2));
    frame.aim_forward = QVector3D(at(3), at(4), at(5));
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
      CommanderActionRecord record;
      record.action = entry.action;
      record.view_yaw =
          static_cast<float>(object.value(QLatin1String("view_yaw")).toDouble(0.0));
      record.view_pitch =
          static_cast<float>(object.value(QLatin1String("view_pitch")).toDouble(0.0));
      return record;
    }
  }
  return std::nullopt;
}

} // namespace Game::Command
