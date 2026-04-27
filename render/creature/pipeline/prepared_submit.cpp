#include "prepared_submit.h"

#include "../../submitter.h"

#include <string_view>
#include <vector>

namespace Render::Creature::Pipeline {

auto submit_preparation(CreaturePreparationResult &prep,
                        Render::GL::ISubmitter &out) noexcept -> SubmitStats {
  thread_local CreaturePipeline pipeline;
  thread_local std::vector<Render::Creature::CreatureRenderRequest>
      visible_requests;
  visible_requests.clear();

  auto const requests = prep.bodies.requests();
  visible_requests.reserve(requests.size());

  for (const auto &request : requests) {
    if (request.pass != RenderPassIntent::Shadow) {
      visible_requests.push_back(request);
    }
  }

  SubmitStats stats{};
  if (!visible_requests.empty()) {
    stats = pipeline.submit_requests(visible_requests, out);
  }

  for (auto &request : prep.post_body_draws) {
    if (request.pass_intent == RenderPassIntent::Shadow || !request.draw) {
      continue;
    }
    request.draw(out);
  }
  return stats;
}

void PreparedCreatureSubmitBatch::clear() noexcept { rows_.clear(); }

void PreparedCreatureSubmitBatch::reserve(std::size_t count) {
  rows_.reserve(count);
}

void PreparedCreatureSubmitBatch::add(PreparedCreatureRenderRow row) {
  rows_.push_back(std::move(row));
}

auto PreparedCreatureSubmitBatch::empty() const noexcept -> bool {
  return rows_.empty();
}

auto PreparedCreatureSubmitBatch::size() const noexcept -> std::size_t {
  return rows_.size();
}

auto PreparedCreatureSubmitBatch::rows() const noexcept
    -> std::span<const PreparedCreatureRenderRow> {
  return {rows_.data(), rows_.size()};
}

} // namespace Render::Creature::Pipeline
