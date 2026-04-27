

#pragma once

#include "clip.h"
#include "state_machine.h"

#include <cstddef>
#include <vector>

namespace Render::Animation {

template <typename T> class ChannelEvaluator {
public:
  ChannelEvaluator(std::vector<Clip<T>> clips, const StateMachine *machine)
      : m_clips(std::move(clips)), m_machine(machine) {}

  [[nodiscard]] auto sample(float time,
                            WrapMode wrap = WrapMode::Loop) const -> T {
    if (m_machine == nullptr) {
      return T{};
    }
    StateId const cur = m_machine->current();
    T cur_val = sample_clip(cur, time, wrap);

    if (!m_machine->is_blending() ||
        m_machine->previous() == m_machine->current()) {
      return cur_val;
    }

    StateId const prev = m_machine->previous();
    T const prev_val = sample_clip(prev, time, wrap);
    return lerp(prev_val, cur_val, m_machine->weight());
  }

  [[nodiscard]] auto clip_count() const noexcept -> std::size_t {
    return m_clips.size();
  }

private:
  [[nodiscard]] auto sample_clip(StateId id, float time,
                                 WrapMode wrap) const -> T {
    if (id >= m_clips.size()) {
      return T{};
    }
    return evaluate(m_clips[id], time, wrap);
  }

  std::vector<Clip<T>> m_clips;
  const StateMachine *m_machine{nullptr};
};

} // namespace Render::Animation
