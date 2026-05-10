#pragma once

#include "../skeleton.h"

#include <deque>
#include <string>
#include <string_view>
#include <vector>

namespace Render::Creature::Quadruped {

struct TopologyOptions {
  bool include_body{true};
  bool include_neck_top{true};
  bool include_head{true};
  bool include_appendage_tip{false};
  std::string_view appendage_tip_name{"AppendageTip"};
};

struct TopologyStorage {
  std::deque<std::string> owned_names{};
  std::vector<BoneDef> bones{};
  std::vector<SocketDef> sockets{};

  BoneIndex root{k_invalid_bone};
  BoneIndex body{k_invalid_bone};
  BoneIndex shoulder_fl{k_invalid_bone};
  BoneIndex knee_fl{k_invalid_bone};
  BoneIndex foot_fl{k_invalid_bone};
  BoneIndex shoulder_fr{k_invalid_bone};
  BoneIndex knee_fr{k_invalid_bone};
  BoneIndex foot_fr{k_invalid_bone};
  BoneIndex shoulder_bl{k_invalid_bone};
  BoneIndex knee_bl{k_invalid_bone};
  BoneIndex foot_bl{k_invalid_bone};
  BoneIndex shoulder_br{k_invalid_bone};
  BoneIndex knee_br{k_invalid_bone};
  BoneIndex foot_br{k_invalid_bone};
  BoneIndex neck_top{k_invalid_bone};
  BoneIndex head{k_invalid_bone};
  BoneIndex appendage_tip{k_invalid_bone};

  [[nodiscard]] auto topology() const noexcept -> SkeletonTopology {
    return {std::span<const BoneDef>(bones),
            std::span<const SocketDef>(sockets)};
  }
};

[[nodiscard]] auto
make_topology(const TopologyOptions &options = {}) -> TopologyStorage;

} // namespace Render::Creature::Quadruped
