#pragma once

#include "../../entity/registry.h"
#include "creature_pipeline.h"
#include "creature_render_graph.h"
#include "creature_render_state.h"

#include <cstddef>
#include <span>
#include <vector>

namespace Render::GL {
class ISubmitter;
}

namespace Render::Creature::Pipeline {

void submit_preparation(CreaturePreparationResult &prep,
                        Render::GL::ISubmitter &out) noexcept;

class PreparedCreatureSubmitBatch {
public:
  void clear() noexcept;
  void reserve(std::size_t count);

  void add(PreparedCreatureRenderRow row);

  [[nodiscard]] auto empty() const noexcept -> bool;
  [[nodiscard]] auto size() const noexcept -> std::size_t;
  [[nodiscard]] auto
  rows() const noexcept -> std::span<const PreparedCreatureRenderRow>;

  [[nodiscard]] auto
  submit(Render::GL::ISubmitter &out) const noexcept -> SubmitStats;

private:
  std::vector<PreparedCreatureRenderRow> rows_{};
};

} // namespace Render::Creature::Pipeline
