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

  BoneIndex root{kInvalidBone};
  BoneIndex body{kInvalidBone};
  BoneIndex shoulder_fl{kInvalidBone};
  BoneIndex knee_fl{kInvalidBone};
  BoneIndex foot_fl{kInvalidBone};
  BoneIndex shoulder_fr{kInvalidBone};
  BoneIndex knee_fr{kInvalidBone};
  BoneIndex foot_fr{kInvalidBone};
  BoneIndex shoulder_bl{kInvalidBone};
  BoneIndex knee_bl{kInvalidBone};
  BoneIndex foot_bl{kInvalidBone};
  BoneIndex shoulder_br{kInvalidBone};
  BoneIndex knee_br{kInvalidBone};
  BoneIndex foot_br{kInvalidBone};
  BoneIndex neck_top{kInvalidBone};
  BoneIndex head{kInvalidBone};
  BoneIndex appendage_tip{kInvalidBone};

  [[nodiscard]] auto topology() const noexcept -> SkeletonTopology {
    return {std::span<const BoneDef>(bones),
            std::span<const SocketDef>(sockets)};
  }
};

[[nodiscard]] auto
make_topology(const TopologyOptions &options = {}) -> TopologyStorage;

} // namespace Render::Creature::Quadruped
