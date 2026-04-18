#include "frame_pass_runner.h"

namespace Render::Pass {

void FramePassRunner::add(std::unique_ptr<IFramePass> pass) {
  if (pass == nullptr) {
    return;
  }
  Slot s;
  s.ref = pass.get();
  s.owned = std::move(pass);
  m_slots.push_back(std::move(s));
}

void FramePassRunner::add_non_owning(IFramePass &pass) {
  Slot s;
  s.ref = &pass;
  m_slots.push_back(std::move(s));
}

void FramePassRunner::execute(FrameContext &ctx) {
  for (auto &slot : m_slots) {
    if (slot.ref != nullptr) {
      slot.ref->execute(ctx);
    }
  }
}

auto FramePassRunner::size() const noexcept -> std::size_t {
  return m_slots.size();
}

auto FramePassRunner::pass_name(std::size_t i) const noexcept
    -> std::string_view {
  if (i >= m_slots.size() || m_slots[i].ref == nullptr) {
    return {};
  }
  return m_slots[i].ref->name();
}

void FramePassRunner::clear() noexcept { m_slots.clear(); }

} // namespace Render::Pass
