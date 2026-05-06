#include "prepared_submit.h"

#include "../../submitter.h"
#include "shadow_batch.h"

#include <string_view>
#include <vector>

namespace Render::Creature::Pipeline {
namespace {

void submit_post_body_request(const PostBodyDrawRequest &request,
                              Render::GL::ISubmitter &out) noexcept {
  (void)out;
  switch (request.kind) {
  case PostBodyDrawRequest::Kind::None:
    break;
  }
}

} // namespace

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

  for (const auto &request : prep.post_body_draws) {
    if (request.pass_intent == RenderPassIntent::Shadow) {
      continue;
    }
    submit_post_body_request(request, out);
  }

  flush_shadow_batch(prep.shadow_batch, out);

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
