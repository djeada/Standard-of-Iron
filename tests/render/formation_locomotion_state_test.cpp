#include <gtest/gtest.h>
#include <vector>

#include "render/entity/formation_instance_layout.h"

namespace {

using Render::Entity::FormationInstance;
using Render::Entity::FormationLayoutCache;
using Render::Entity::FormationLayoutRequest;
using Render::Entity::resolve_formation_instances;

auto request_for(float blend_ratio, int total_count = 12) -> FormationLayoutRequest {
  FormationLayoutRequest request;
  request.rows = 3;
  request.cols = 4;
  request.total_count = total_count;
  request.seed = 4242U;
  request.layout_version = 7;
  request.formation.individuals_per_unit = total_count;
  request.formation.max_per_row = 4;
  request.formation.spacing = 1.1F;
  request.blend_ratio = blend_ratio;
  return request;
}

void prime(FormationLayoutCache& cache, const FormationLayoutRequest& request) {
  std::vector<FormationInstance> scratch;
  (void)resolve_formation_instances(&cache, scratch, request);
}

} // namespace

TEST(FormationLocomotionStateTest, ABlendingLayoutKeepsItsSoldiersGait) {
  FormationLayoutCache cache;
  prime(cache, request_for(0.25F));

  std::vector<FormationInstance> scratch;

  auto const result = resolve_formation_instances(&cache, scratch, request_for(0.31F));

  EXPECT_FALSE(result.preserve_state_prefix)
      << "the cached instance geometry is stale once the blend has moved";
  EXPECT_TRUE(result.preserve_locomotion_state)
      << "a soldier mid-stride is the same soldier at a different blend ratio";
}

TEST(FormationLocomotionStateTest, AReshapedLayoutKeepsItsSoldiersGait) {
  FormationLayoutCache cache;
  prime(cache, request_for(0.0F));

  auto reshaped = request_for(0.0F);
  reshaped.rows = 4;
  reshaped.cols = 3;
  std::vector<FormationInstance> scratch;
  auto const result = resolve_formation_instances(&cache, scratch, reshaped);

  EXPECT_FALSE(result.preserve_state_prefix);
  EXPECT_TRUE(result.preserve_locomotion_state)
      << "changing rank and file moves a man, it does not replace him";
}

TEST(FormationLocomotionStateTest, ADifferentUnitDoesNotInheritAGait) {
  FormationLayoutCache cache;
  prime(cache, request_for(0.0F));

  auto other = request_for(0.0F);
  other.seed = 99U;
  std::vector<FormationInstance> scratch;
  EXPECT_FALSE(
      resolve_formation_instances(&cache, scratch, other).preserve_locomotion_state)
      << "a different seed is a different set of men";

  auto resized = request_for(0.0F, 9);
  resized.formation.individuals_per_unit = 9;
  EXPECT_FALSE(
      resolve_formation_instances(&cache, scratch, resized).preserve_locomotion_state)
      << "a roster of a different size cannot map onto the cached one";
}
