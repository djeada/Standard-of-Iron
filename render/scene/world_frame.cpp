#include "render/scene/world_frame.h"

#include <memory>
#include <string_view>
#include <utility>

#include "render/frame/pass/frame_context.h"
#include "render/frame/pass/frame_pass_runner.h"
#include "render/frame/pass/i_pass.h"
#include "render/scene/construction_preview_pass.h"
#include "render/scene/primitive_flush_pass.h"
#include "render/scene/scene_renderer.h"

namespace Render::GL {

namespace {

class StagePass final : public Render::Pass::IFramePass {
public:
  StagePass(std::string_view name, std::function<void(Render::Pass::FrameContext&)> run)
      : m_name(name)
      , m_run(std::move(run)) {}

  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return m_name;
  }
  void execute(Render::Pass::FrameContext& ctx) override { m_run(ctx); }

private:
  std::string_view m_name;
  std::function<void(Render::Pass::FrameContext&)> m_run;
};

} // namespace

void build_world_frame_passes(Renderer& renderer,
                              Render::Pass::FramePassRunner& frame,
                              Render::Pass::FramePassRunner& world) {
  Renderer* const r = &renderer;

  frame.clear();
  frame.add(std::make_unique<StagePass>(
      "terrain", [r](Render::Pass::FrameContext&) { r->run_frame_source_terrain(); }));
  frame.add(std::make_unique<StagePass>(
      "world", [r](Render::Pass::FrameContext& ctx) { r->render_world(ctx.world); }));
  frame.add(std::make_unique<StagePass>("overlays", [r](Render::Pass::FrameContext&) {
    r->run_frame_source_overlays();
  }));

  world.clear();
  world.add(std::make_unique<Render::Pass::PrimitiveFlushPass>());
  world.add(std::make_unique<Render::Pass::ConstructionPreviewPass>());
}

} // namespace Render::GL
