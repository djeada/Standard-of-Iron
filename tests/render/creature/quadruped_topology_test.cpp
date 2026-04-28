#include "render/creature/quadruped/skeleton_factory.h"

#include <gtest/gtest.h>

namespace {

using namespace Render::Creature;
using namespace Render::Creature::Quadruped;

TEST(QuadrupedTopologyTest, DefaultTopologyValidatesAndExposesNamedBones) {
  TopologyOptions options;
  options.include_appendage_tip = true;
  options.appendage_tip_name = "TrunkTip";

  auto storage = make_topology(options);
  auto const topology = storage.topology();

  EXPECT_TRUE(validate_topology(topology));
  EXPECT_EQ(find_bone(topology, "Root"), storage.root);
  EXPECT_EQ(find_bone(topology, "Body"), storage.body);
  EXPECT_EQ(find_bone(topology, "FootBR"), storage.foot_br);
  EXPECT_EQ(find_bone(topology, "NeckTop"), storage.neck_top);
  EXPECT_EQ(find_bone(topology, "Head"), storage.head);
  EXPECT_EQ(find_bone(topology, "TrunkTip"), storage.appendage_tip);
  ASSERT_LT(static_cast<std::size_t>(storage.appendage_tip),
            storage.bones.size());
  EXPECT_EQ(storage.bones[storage.appendage_tip].parent, storage.head);
}

TEST(QuadrupedTopologyTest, OptionalHeadChainCanBeRemoved) {
  TopologyOptions options;
  options.include_neck_top = false;
  options.include_head = false;
  options.include_appendage_tip = false;

  auto storage = make_topology(options);
  auto const topology = storage.topology();

  EXPECT_TRUE(validate_topology(topology));
  EXPECT_EQ(storage.neck_top, kInvalidBone);
  EXPECT_EQ(storage.head, kInvalidBone);
  EXPECT_EQ(storage.appendage_tip, kInvalidBone);
  EXPECT_EQ(find_bone(topology, "NeckTop"), kInvalidBone);
  EXPECT_EQ(find_bone(topology, "Head"), kInvalidBone);
  EXPECT_EQ(storage.bones.size(), 14U);
}

} // namespace
