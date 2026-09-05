#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

#include "game/map/mission_definition.h"

namespace Game::Mission {

struct CommanderVoiceLine {
  QString id;
  CommanderRelationship relationship = CommanderRelationship::Enemy;
  QString pose;
  QString voice_cue;
  CommanderMessageTrigger trigger = CommanderMessageTrigger::MissionStart;
  CommanderMessageCondition condition;
  float delay = 0.0F;
  float duration = k_default_commander_message_seconds;
  int priority = 0;
  bool once = true;
  QStringList variants;
};

struct CommanderVoiceBank {
  QString commander_id;
  int chatter_per_match = k_default_commander_chatter_per_match;
  std::vector<CommanderVoiceLine> lines;
};

inline constexpr const char* k_commander_voices_relative_dir =
    "assets/data/commanders/voices";

class CommanderVoiceLibrary {
public:
  static auto parse_bank(const QJsonObject& root,
                         QString* error) -> std::optional<CommanderVoiceBank>;

  static auto load_from_file(const QString& path,
                             QString* error) -> std::optional<CommanderVoiceBank>;

  static auto load_from_directory(const QString& directory,
                                  QString* error) -> CommanderVoiceLibrary;

  static auto load_default(QString* error) -> CommanderVoiceLibrary;

  static auto default_directory() -> QString;

  void add(CommanderVoiceBank bank);

  [[nodiscard]] auto
  bank_for(const QString& commander_id) const -> const CommanderVoiceBank*;
  [[nodiscard]] auto banks() const -> const std::vector<CommanderVoiceBank>& {
    return m_banks;
  }
  [[nodiscard]] auto empty() const -> bool { return m_banks.empty(); }

private:
  std::vector<CommanderVoiceBank> m_banks;
};

[[nodiscard]] auto commander_voice_rule_id(int owner_id,
                                           const QString& line_id,
                                           int variant_index) -> QString;

[[nodiscard]] auto
expand_commander_voice_line(const CommanderVoiceLine& line,
                            const QString& commander_id,
                            int owner_id) -> std::vector<CommanderMessage>;

} // namespace Game::Mission
