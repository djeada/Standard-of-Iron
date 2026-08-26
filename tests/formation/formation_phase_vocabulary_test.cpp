#include <QString>

#include <array>
#include <gtest/gtest.h>
#include <set>
#include <string>

#include "game/formation/army_formation_types.h"

namespace {

using Game::Formation::FormationPhase;

constexpr std::array<const char*, 6> k_phases_the_hud_knows = {
    "reforming", "formed", "disrupted", "opening", "traversing", "arrived"};

constexpr std::array<FormationPhase, 6> k_all_phases = {FormationPhase::Reforming,
                                                        FormationPhase::Formed,
                                                        FormationPhase::Disrupted,
                                                        FormationPhase::Opening,
                                                        FormationPhase::Traversing,
                                                        FormationPhase::Arrived};

} // namespace

TEST(FormationPhaseVocabularyTest, EveryPhaseHasAStringTheHudRecognises) {
  std::set<std::string> known(k_phases_the_hud_knows.begin(),
                              k_phases_the_hud_knows.end());

  for (const FormationPhase phase : k_all_phases) {
    const std::string reported = Game::Formation::phase_to_string(phase);
    EXPECT_TRUE(known.contains(reported))
        << "the simulation reports phase '" << reported
        << "' which FormationStatusBadge.qml does not label";
  }
}

TEST(FormationPhaseVocabularyTest, NoTwoPhasesShareAString) {
  std::set<std::string> seen;
  for (const FormationPhase phase : k_all_phases) {
    const std::string reported = Game::Formation::phase_to_string(phase);
    EXPECT_TRUE(seen.insert(reported).second)
        << "two phases both report '" << reported << "'";
  }
  EXPECT_EQ(seen.size(), k_phases_the_hud_knows.size())
      << "the phase vocabulary changed; update FormationStatusBadge.qml too";
}

TEST(FormationPhaseVocabularyTest, EveryPhaseHasADisplayName) {
  for (const FormationPhase phase : k_all_phases) {
    EXPECT_FALSE(Game::Formation::phase_display_name(phase).isEmpty())
        << "phase " << static_cast<int>(phase) << " has no display name";
  }
}
