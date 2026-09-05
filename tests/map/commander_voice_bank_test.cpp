#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>

#include <gtest/gtest.h>
#include <map>
#include <set>

#include "game/map/commander_message_grammar.h"
#include "game/map/mission_definition.h"
#include "game/map/mission_loader.h"
#include "game/mission/commander_voice_bank.h"
#include "game/units/commander_catalog.h"

namespace {

using Game::Mission::CommanderMessageTrigger;
using Game::Mission::CommanderRelationship;

auto asset_dir_path(const QString& relative_path) -> QString {
  return QDir(QCoreApplication::applicationDirPath())
      .absoluteFilePath(QStringLiteral("../../assets/%1").arg(relative_path));
}

auto load_shipped_library() -> Game::Mission::CommanderVoiceLibrary {
  QString error;
  auto library = Game::Mission::CommanderVoiceLibrary::load_from_directory(
      asset_dir_path(QStringLiteral("data/commanders/voices")), &error);
  EXPECT_TRUE(error.isEmpty()) << error.toStdString();
  return library;
}

struct RequiredBeat {
  CommanderMessageTrigger trigger;
  Game::Mission::CommanderMessageRole subject;
  Game::Mission::CommanderMessageRole actor;
  int min_variants;
  const char* what;
};

using Role = Game::Mission::CommanderMessageRole;

const std::vector<RequiredBeat> k_enemy_beats{
    {CommanderMessageTrigger::MissionStart, Role::Unset, Role::Unset, 2, "opening"},
    {CommanderMessageTrigger::MissionVictory,
     Role::Unset,
     Role::Unset,
     2,
     "player wins"},
    {CommanderMessageTrigger::MissionDefeat,
     Role::Unset,
     Role::Unset,
     2,
     "player loses"},
    {CommanderMessageTrigger::FirstContact,
     Role::Self,
     Role::Player,
     1,
     "first contact"},
    {CommanderMessageTrigger::AttackLaunched,
     Role::Player,
     Role::Self,
     3,
     "attacks player"},
    {CommanderMessageTrigger::UnderAttack,
     Role::Self,
     Role::Player,
     2,
     "player hits camp"},
    {CommanderMessageTrigger::StructureCaptured,
     Role::Self,
     Role::Player,
     2,
     "loses camp"},
    {CommanderMessageTrigger::StructureCaptured,
     Role::Player,
     Role::Self,
     2,
     "takes camp"},
    {CommanderMessageTrigger::HeavyLosses,
     Role::Player,
     Role::Self,
     2,
     "player bleeds"},
    {CommanderMessageTrigger::HeavyLosses, Role::Self, Role::Unset, 1, "own losses"},
    {CommanderMessageTrigger::WaveIncoming, Role::Self, Role::Unset, 2, "wave"},
    {CommanderMessageTrigger::CommanderDefeated,
     Role::Player,
     Role::Self,
     1,
     "kills player commander"},
    {CommanderMessageTrigger::CommanderDefeated,
     Role::Self,
     Role::Unset,
     1,
     "own death"},
    {CommanderMessageTrigger::NearDefeat, Role::Self, Role::Unset, 1, "near defeat"},
};

const std::vector<RequiredBeat> k_ally_beats{
    {CommanderMessageTrigger::MissionStart, Role::Unset, Role::Unset, 1, "opening"},
    {CommanderMessageTrigger::MissionVictory, Role::Unset, Role::Unset, 1, "victory"},
    {CommanderMessageTrigger::MissionDefeat, Role::Unset, Role::Unset, 1, "defeat"},
    {CommanderMessageTrigger::UnderAttack, Role::Self, Role::Unset, 2, "needs help"},
    {CommanderMessageTrigger::HeavyLosses, Role::Self, Role::Unset, 1, "bleeding"},
    {CommanderMessageTrigger::NearDefeat, Role::Self, Role::Unset, 1, "near defeat"},
    {CommanderMessageTrigger::OwnerEliminated,
     Role::Self,
     Role::Unset,
     1,
     "eliminated"},
    {CommanderMessageTrigger::AttackLaunched, Role::Unset, Role::Self, 2, "attacks"},
    {CommanderMessageTrigger::StructureCaptured,
     Role::Unset,
     Role::Self,
     1,
     "captured"},
    {CommanderMessageTrigger::CommanderDefeated,
     Role::Player,
     Role::Unset,
     1,
     "player commander fell"},
};

auto effective_role(Game::Mission::CommanderMessageRole role, bool is_local) -> Role {
  return is_local ? Role::Player : role;
}

auto covers(const Game::Mission::CommanderVoiceBank& bank,
            CommanderRelationship relationship,
            const RequiredBeat& beat) -> bool {
  for (const auto& line : bank.lines) {
    if (line.relationship != relationship || line.trigger != beat.trigger) {
      continue;
    }
    const auto subject =
        effective_role(line.condition.subject_role, line.condition.owner_is_local);
    const auto actor =
        effective_role(line.condition.actor_role, line.condition.by_owner_is_local);
    if (subject != beat.subject || actor != beat.actor) {
      continue;
    }
    if (line.variants.size() >= beat.min_variants) {
      return true;
    }
  }
  return false;
}

TEST(CommanderVoiceBankTest, ParsesVariantsAndTheTextShorthand) {
  const auto doc = QJsonDocument::fromJson(R"({
    "commander": "roman_veteran_consul",
    "chatter_per_match": 7,
    "lines": [
      { "id": "a", "relationship": "enemy", "pose": "dismissive", "priority": 40, "once": false,
        "trigger": { "type": "attack_launched", "actor": "self", "subject": "player", "cooldown": 90, "delay": 1.5 },
        "variants": ["One.", "Two."] },
      { "id": "b", "relationship": "ally", "trigger": { "type": "match_start" }, "text": "Hello." }
    ]
  })");
  QString error;
  const auto bank =
      Game::Mission::CommanderVoiceLibrary::parse_bank(doc.object(), &error);
  ASSERT_TRUE(bank.has_value()) << error.toStdString();
  EXPECT_EQ(bank->commander_id, QStringLiteral("roman_veteran_consul"));
  EXPECT_EQ(bank->chatter_per_match, 7);
  ASSERT_EQ(bank->lines.size(), 2U);

  const auto& a = bank->lines[0];
  EXPECT_EQ(a.relationship, CommanderRelationship::Enemy);
  EXPECT_EQ(a.trigger, CommanderMessageTrigger::AttackLaunched);
  EXPECT_EQ(a.condition.actor_role, Role::Self);
  EXPECT_TRUE(a.condition.owner_is_local)
      << "\"player\" keeps its mission-file meaning";
  EXPECT_FLOAT_EQ(a.condition.cooldown, 90.0F);
  EXPECT_FLOAT_EQ(a.delay, 1.5F);
  EXPECT_FALSE(a.once);
  EXPECT_EQ(a.variants, (QStringList{QStringLiteral("One."), QStringLiteral("Two.")}));

  const auto& b = bank->lines[1];
  EXPECT_EQ(b.relationship, CommanderRelationship::Ally);
  EXPECT_EQ(b.trigger, CommanderMessageTrigger::MissionStart);
  EXPECT_EQ(b.variants, QStringList{QStringLiteral("Hello.")});
}

TEST(CommanderVoiceBankTest, RejectsALineWithoutARelationshipOrWithAnUnknownTrigger) {
  QString error;
  auto missing_relationship = Game::Mission::CommanderVoiceLibrary::parse_bank(
      QJsonDocument::fromJson(
          R"({"commander":"x","lines":[{"id":"a","trigger":{"type":"match_start"},"text":"t"}]})")
          .object(),
      &error);
  EXPECT_FALSE(missing_relationship.has_value());
  EXPECT_FALSE(error.isEmpty());

  auto unknown_trigger = Game::Mission::CommanderVoiceLibrary::parse_bank(
      QJsonDocument::fromJson(
          R"({"commander":"x","lines":[{"id":"a","relationship":"enemy","trigger":{"type":"sneeze"},"text":"t"}]})")
          .object(),
      &error);
  EXPECT_FALSE(unknown_trigger.has_value());
}

TEST(CommanderVoiceBankTest, ExpandsVariantsIntoOwnerScopedRules) {
  Game::Mission::CommanderVoiceLine line;
  line.id = QStringLiteral("scipio.enemy.attack_launched");
  line.variants = {QStringLiteral("One."), QStringLiteral("Two.")};
  line.priority = 40;
  line.trigger = CommanderMessageTrigger::AttackLaunched;
  line.condition.cooldown = 90.0F;

  const auto rules = Game::Mission::expand_commander_voice_line(
      line, QStringLiteral("roman_veteran_consul"), 3);
  ASSERT_EQ(rules.size(), 2U);
  EXPECT_EQ(rules[0].id, QStringLiteral("3:scipio.enemy.attack_launched.1"));
  EXPECT_EQ(rules[1].id, QStringLiteral("3:scipio.enemy.attack_launched.2"));
  EXPECT_EQ(rules[1].text, QStringLiteral("Two."));
  EXPECT_EQ(rules[0].speaker, QStringLiteral("roman_veteran_consul"));
  EXPECT_EQ(rules[0].trigger, CommanderMessageTrigger::AttackLaunched);
  EXPECT_FLOAT_EQ(rules[0].condition.cooldown, 90.0F);
  EXPECT_EQ(rules[0].priority, 40);
}

TEST(CommanderVoiceBankTest, TriggerNamesRoundTrip) {
  for (int raw = 0; raw <= static_cast<int>(CommanderMessageTrigger::WaveCleared);
       ++raw) {
    const auto trigger = static_cast<CommanderMessageTrigger>(raw);
    CommanderMessageTrigger parsed{};
    ASSERT_TRUE(Game::Mission::parse_commander_message_trigger(
        Game::Mission::commander_message_trigger_name(trigger), parsed));
    EXPECT_EQ(parsed, trigger);
  }
  CommanderMessageTrigger alias{};
  ASSERT_TRUE(Game::Mission::parse_commander_message_trigger(
      QStringLiteral("match_start"), alias));
  EXPECT_EQ(alias, CommanderMessageTrigger::MissionStart);
}

TEST(CommanderVoiceAssetTest, EveryCatalogueCommanderHasAVoiceBank) {
  const auto library = load_shipped_library();
  for (const auto& definition : Game::Units::all_commander_definitions()) {
    const QString id = QString::fromStdString(definition.id);
    EXPECT_NE(library.bank_for(id), nullptr)
        << id.toStdString() << " has no voice bank";
  }
  for (const auto& bank : library.banks()) {
    bool catalogued = false;
    for (const auto& definition : Game::Units::all_commander_definitions()) {
      catalogued =
          catalogued || QString::fromStdString(definition.id) == bank.commander_id;
    }
    EXPECT_TRUE(catalogued) << bank.commander_id.toStdString()
                            << " is a bank for nobody in the catalogue";
  }
}

TEST(CommanderVoiceAssetTest, EveryVoiceBankCoversTheRequiredBeats) {
  const auto library = load_shipped_library();
  ASSERT_FALSE(library.empty());
  for (const auto& bank : library.banks()) {
    for (const auto& beat : k_enemy_beats) {
      EXPECT_TRUE(covers(bank, CommanderRelationship::Enemy, beat))
          << bank.commander_id.toStdString()
          << " as enemy has no line for: " << beat.what;
    }
    for (const auto& beat : k_ally_beats) {
      EXPECT_TRUE(covers(bank, CommanderRelationship::Ally, beat))
          << bank.commander_id.toStdString()
          << " as ally has no line for: " << beat.what;
    }
  }
}

TEST(CommanderVoiceAssetTest, VoiceLineIdsAreGloballyUniqueAndRepeatsHaveCooldowns) {
  const auto library = load_shipped_library();
  std::set<QString> ids;
  for (const auto& bank : library.banks()) {
    EXPECT_GT(bank.chatter_per_match, 0) << bank.commander_id.toStdString();
    for (const auto& line : bank.lines) {
      EXPECT_TRUE(ids.insert(line.id).second)
          << line.id.toStdString() << " is defined twice";
      EXPECT_FALSE(line.variants.isEmpty()) << line.id.toStdString();
      if (!line.once) {
        EXPECT_GT(line.condition.cooldown, 0.0F)
            << line.id.toStdString() << " repeats without a cooldown";
      }
      EXPECT_TRUE(line.pose == QStringLiteral("dismissive") ||
                  line.pose == QStringLiteral("cynical"))
          << line.id.toStdString()
          << " uses a pose that is not baked: " << line.pose.toStdString();
    }
  }
}

TEST(CommanderVoiceAssetTest, ChatterFitsTheReadingBudget) {
  const auto library = load_shipped_library();
  for (const auto& bank : library.banks()) {
    for (const auto& line : bank.lines) {
      const bool bookend =
          line.trigger == CommanderMessageTrigger::MissionStart ||
          Game::Mission::commander_message_trigger_is_outcome(line.trigger);
      for (const QString& variant : line.variants) {
        const float needed = Game::Mission::legible_commander_message_seconds(
            static_cast<int>(variant.length()), 0.0F);
        if (!bookend) {
          EXPECT_LT(variant.length(), 260)
              << line.id.toStdString() << " is too long to read in the panel";
          EXPECT_LT(needed, Game::Mission::k_commander_message_max_seconds)
              << line.id.toStdString();
        }
        EXPECT_GE(variant.length(), 20) << line.id.toStdString() << " is a grunt";
      }
    }
  }
}

TEST(CommanderVoiceAssetTest, MissionMutedLinesNameRealBankLines) {
  const auto library = load_shipped_library();
  QDir const missions_dir(asset_dir_path(QStringLiteral("missions")));
  int muted = 0;
  for (const QString& file_name :
       missions_dir.entryList({"*.json"}, QDir::Files, QDir::Name)) {
    Game::Mission::MissionDefinition mission;
    QString error;
    ASSERT_TRUE(Game::Mission::MissionLoader::load_from_json_file(
        missions_dir.absoluteFilePath(file_name), mission, &error))
        << file_name.toStdString() << ": " << error.toStdString();
    for (const QString& line_id : mission.commander_voices.muted_lines) {
      ++muted;
      bool found = false;
      for (const auto& bank : library.banks()) {
        for (const auto& line : bank.lines) {
          found = found || line.id == line_id;
        }
      }
      EXPECT_TRUE(found) << file_name.toStdString() << " mutes '"
                         << line_id.toStdString() << "', which no bank defines";
    }
  }
  EXPECT_GE(muted, 0);
}

} // namespace
