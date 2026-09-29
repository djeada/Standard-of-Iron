#pragma once

#include <QJsonObject>

#include <cstdint>
#include <vector>

#include "bird_flock.h"
#include "wildlife_group.h"

namespace Game::Wildlife {

struct WildlifeSaveData {
  bool enabled{false};
  std::uint32_t seed{1U};
  std::uint16_t next_group_id{0U};
  double elapsed{0.0};
  std::vector<bool> released_waves;
  std::vector<GroupState> groups;
  BirdPopulation birds;
};

[[nodiscard]] auto encode_wildlife_state(const WildlifeSaveData& data) -> QJsonObject;

[[nodiscard]] auto
decode_wildlife_state(const QJsonObject& state,
                      const WildlifeSaveData& fallback) -> WildlifeSaveData;

} // namespace Game::Wildlife
