#pragma once

#include "../../entity/registry.h"
#include "creature_pipeline.h"
#include "creature_render_state.h"

#include <cstddef>
#include <deque>
#include <span>
#include <vector>

namespace Render::GL {
class ISubmitter;
}

namespace Render::Creature::Pipeline {

class PreparedCreatureSubmitBatch {
public:
  void clear() noexcept;
  void reserve(std::size_t count);

  void add(PreparedCreatureRenderRow row);
  void add_with_legacy_context(PreparedCreatureRenderRow row,
                               const Render::GL::DrawContext &legacy_ctx);

  [[nodiscard]] auto empty() const noexcept -> bool;
  [[nodiscard]] auto size() const noexcept -> std::size_t;
  [[nodiscard]] auto
  rows() const noexcept -> std::span<const PreparedCreatureRenderRow>;

  [[nodiscard]] auto
  submit(Render::GL::ISubmitter &out) const noexcept -> SubmitStats;

private:
  std::vector<PreparedCreatureRenderRow> rows_{};
  std::deque<Render::GL::DrawContext> legacy_contexts_{};
};

[[nodiscard]] auto
submit_prepared_creatures(std::span<const PreparedCreatureRenderRow> rows,
                          Render::GL::ISubmitter &out) noexcept -> SubmitStats;

} // namespace Render::Creature::Pipeline
