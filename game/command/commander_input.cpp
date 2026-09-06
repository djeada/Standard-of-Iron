#include "commander_input.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QLatin1String>

namespace Game::Command {

auto to_json(const CommanderInputFrame& frame) -> QJsonObject {
  QJsonObject object;
  object["commander_input"] = static_cast<qint64>(frame.commander);
  object["buttons"] = static_cast<qint64>(frame.buttons);
  object["view_yaw"] = static_cast<double>(frame.view_yaw);
  object["sequence"] = static_cast<qint64>(frame.sequence);
  if (frame.held(CommanderInputFrame::HasDodgeDirection)) {
    QJsonArray direction;
    direction.append(static_cast<double>(frame.dodge_direction.x()));
    direction.append(static_cast<double>(frame.dodge_direction.y()));
    direction.append(static_cast<double>(frame.dodge_direction.z()));
    object["dodge"] = direction;
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
  return frame;
}

} // namespace Game::Command
