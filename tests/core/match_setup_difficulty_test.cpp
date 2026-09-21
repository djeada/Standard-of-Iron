#include <QObject>
#include <QSettings>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

#include <gtest/gtest.h>
#include <memory>
#include <mutex>

#include "app/core/client_context.h"
#include "app/viewmodels/match_setup_view_model.h"
#include "game/mission/campaign_manager.h"
#include "game/mission/difficulty_profile.h"
#include "utils/resource_utils.h"

namespace {

constexpr char k_mission_file[] = ":/assets/missions/hold_the_sallow_ford.json";

class StubClientHost : public App::Core::ClientHost {
public:
  void ensure_initialized() override {}

  auto lock_frame() -> std::unique_lock<std::recursive_mutex> override {
    return std::unique_lock<std::recursive_mutex>(m_frame_mutex);
  }

  void set_cursor_mode(CursorMode) override {}

private:
  std::recursive_mutex m_frame_mutex;
};

class MatchSetupDifficultyTest : public ::testing::Test {
protected:
  void SetUp() override {
    QSettings::setPath(
        QSettings::IniFormat, QSettings::UserScope, m_settings_dir.path());
    m_context.campaign = &m_campaign;
    m_context.local_owner_id = 1;
    m_setup = std::make_unique<App::ViewModels::MatchSetupViewModel>(m_context, m_host);
  }

  void TearDown() override { m_setup.reset(); }

  [[nodiscard]] static auto mission_path() -> QString {
    return Utils::Resources::resolve_resource_path(QString::fromLatin1(k_mission_file));
  }

  QTemporaryDir m_settings_dir;
  StubClientHost m_host;
  CampaignManager m_campaign;
  App::Core::ClientContext m_context;
  std::unique_ptr<App::ViewModels::MatchSetupViewModel> m_setup;
};

TEST_F(MatchSetupDifficultyTest, ThePresetListIsTheOneTheBalanceContractDescribes) {
  const QVariantList presets = m_setup->difficulty_presets();
  ASSERT_EQ(presets.size(), Game::Mission::k_difficulty_preset_count);

  const QStringList ids{"easy", "normal", "hard", "very_hard"};
  for (int i = 0; i < presets.size(); ++i) {
    const QVariantMap preset = presets.at(i).toMap();
    EXPECT_EQ(preset.value("id").toString(), ids.at(i));
    EXPECT_GT(preset.value("resource_multiplier").toFloat(), 0.0F);
    EXPECT_GT(preset.value("unit_multiplier").toFloat(), 0.0F);
    EXPECT_GT(preset.value("wave_multiplier").toFloat(), 0.0F);
  }

  const QVariantMap hard = m_setup->difficulty_preset(QStringLiteral("hard"));
  EXPECT_FLOAT_EQ(hard.value("resource_multiplier").toFloat(), 1.5F);
  EXPECT_FLOAT_EQ(hard.value("wave_multiplier").toFloat(), 1.5F);
  EXPECT_FALSE(hard.value("is_baseline").toBool());
  EXPECT_TRUE(m_setup->difficulty_preset(QStringLiteral("normal"))
                  .value("is_baseline")
                  .toBool());
}

TEST_F(MatchSetupDifficultyTest, AFreshProfileStartsOnNormal) {
  EXPECT_EQ(m_setup->preferred_difficulty(), QStringLiteral("normal"));
  EXPECT_EQ(m_setup->normalize_difficulty(QStringLiteral("brutal")),
            QStringLiteral("very_hard"));
  EXPECT_EQ(m_setup->normalize_difficulty(QStringLiteral("nonsense")),
            QStringLiteral("normal"));
}

TEST_F(MatchSetupDifficultyTest, TheChosenPresetIsRememberedForTheNextLaunch) {
  int remembered = 0;
  QObject::connect(m_setup.get(),
                   &App::ViewModels::MatchSetupViewModel::preferred_difficulty_changed,
                   m_setup.get(),
                   [&remembered]() { ++remembered; });
  m_setup->set_preferred_difficulty(QStringLiteral("brutal"));
  EXPECT_EQ(remembered, 1);
  EXPECT_EQ(m_setup->preferred_difficulty(), QStringLiteral("very_hard"));

  m_setup->set_preferred_difficulty(QStringLiteral("very_hard"));
  EXPECT_EQ(remembered, 1) << "choosing the same preset again is not a change";

  App::ViewModels::MatchSetupViewModel reopened(m_context, m_host);
  EXPECT_EQ(reopened.preferred_difficulty(), QStringLiteral("very_hard"))
      << "the preset survives a relaunch of the menu";
}

TEST_F(MatchSetupDifficultyTest, AMissionLaunchCarriesThePresetIntoTheMatch) {
  int launched = 0;
  QObject::connect(m_setup.get(),
                   &App::ViewModels::MatchSetupViewModel::launch_requested,
                   m_setup.get(),
                   [&launched](const App::Core::MatchLaunch&) { ++launched; });

  m_setup->start_mission_file(mission_path(), QStringLiteral("hard"));
  ASSERT_EQ(launched, 1);
  EXPECT_EQ(m_campaign.current_mission_context().difficulty, QStringLiteral("hard"));
  EXPECT_EQ(m_setup->active_difficulty(), QStringLiteral("hard"));
  EXPECT_EQ(m_setup->preferred_difficulty(), QStringLiteral("hard"))
      << "starting a mission remembers what was chosen";
}

TEST_F(MatchSetupDifficultyTest, AnOmittedPresetFallsBackToTheRememberedOne) {
  m_setup->set_preferred_difficulty(QStringLiteral("very_hard"));
  m_setup->start_mission_file(mission_path());
  EXPECT_EQ(m_campaign.current_mission_context().difficulty,
            QStringLiteral("very_hard"));
}

TEST_F(MatchSetupDifficultyTest, FightingTheSameBattleAgainKeepsItsPreset) {
  m_setup->start_mission_file(mission_path(), QStringLiteral("easy"));
  ASSERT_TRUE(m_setup->can_restart());

  m_setup->set_preferred_difficulty(QStringLiteral("very_hard"));
  ASSERT_TRUE(m_setup->restart_current_match());
  EXPECT_EQ(m_campaign.current_mission_context().difficulty, QStringLiteral("easy"))
      << "a replay reruns the battle that was fought, not the one now selected";
}

TEST_F(MatchSetupDifficultyTest, ASkirmishCarriesAPresetPerOpponentSlot) {
  QVariantList configs;
  QVariantMap human;
  human.insert("player_id", 1);
  human.insert("isHuman", true);
  human.insert("difficulty", QString());
  configs.append(human);
  QVariantMap first_cpu;
  first_cpu.insert("player_id", 2);
  first_cpu.insert("isHuman", false);
  first_cpu.insert("difficulty", QStringLiteral("hard"));
  configs.append(first_cpu);
  QVariantMap second_cpu;
  second_cpu.insert("player_id", 3);
  second_cpu.insert("isHuman", false);
  second_cpu.insert("difficulty", QStringLiteral("brutal"));
  configs.append(second_cpu);

  int launched = 0;
  QObject::connect(m_setup.get(),
                   &App::ViewModels::MatchSetupViewModel::launch_requested,
                   m_setup.get(),
                   [&launched](const App::Core::MatchLaunch&) { ++launched; });
  m_setup->start_skirmish(QStringLiteral("assets/maps/map_forest.json"), configs);
  ASSERT_EQ(launched, 1);

  Game::Mission::MatchDifficulty difficulty;
  for (const QVariant& entry : configs) {
    const QVariantMap config = entry.toMap();
    if (config.value("isHuman").toBool() ||
        config.value("difficulty").toString().isEmpty()) {
      continue;
    }
    difficulty.set_owner(config.value("player_id").toInt(),
                         config.value("difficulty").toString());
  }
  EXPECT_EQ(difficulty.id_for(1), QStringLiteral("normal"));
  EXPECT_EQ(difficulty.id_for(2), QStringLiteral("hard"));
  EXPECT_EQ(difficulty.id_for(3), QStringLiteral("very_hard"));
}

} // namespace
