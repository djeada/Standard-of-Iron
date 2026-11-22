#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "render/frame/pass/frame_pass_runner.h"
#include "render/scene/scene_renderer.h"
#include "render/scene/world_frame.h"

namespace {

auto names_of(const Render::Pass::FramePassRunner& runner) -> std::vector<std::string> {
  std::vector<std::string> names;
  for (std::size_t i = 0; i < runner.size(); ++i) {
    names.emplace_back(runner.pass_name(i));
  }
  return names;
}

} // namespace

TEST(WorldFrameOrder, AFrameIsTerrainThenWorldThenOverlays) {
  Render::GL::Renderer renderer;
  Render::Pass::FramePassRunner frame;
  Render::Pass::FramePassRunner world;
  Render::GL::build_world_frame_passes(renderer, frame, world);

  EXPECT_EQ(names_of(frame),
            (std::vector<std::string>{"terrain", "world", "overlays"}));
}

TEST(WorldFrameOrder, TheWorldStageFlushesPrimitivesBeforeConstructionPreviews) {
  Render::GL::Renderer renderer;
  Render::Pass::FramePassRunner frame;
  Render::Pass::FramePassRunner world;
  Render::GL::build_world_frame_passes(renderer, frame, world);

  EXPECT_EQ(names_of(world),
            (std::vector<std::string>{"primitive_flush", "construction_previews"}));
}
