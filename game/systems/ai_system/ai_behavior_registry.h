#pragma once

#include "ai_behavior.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace Game::Systems::AI {

class AIBehaviorRegistry {
public:
  AIBehaviorRegistry() = default;
  ~AIBehaviorRegistry() = default;

  AIBehaviorRegistry(const AIBehaviorRegistry &) = delete;
  auto operator=(const AIBehaviorRegistry &) -> AIBehaviorRegistry & = delete;

  void register_behavior(std::unique_ptr<AIBehavior> behavior) {
    m_behaviors.push_back(std::move(behavior));

    std::sort(m_behaviors.begin(), m_behaviors.end(),
              [](const std::unique_ptr<AIBehavior> &a,
                 const std::unique_ptr<AIBehavior> &b) {
                return a->get_priority() > b->get_priority();
              });
  }

  void for_each(const std::function<void(AIBehavior &)> &func) {
    for (auto &behavior : m_behaviors) {
      func(*behavior);
    }
  }

  void for_each(const std::function<void(const AIBehavior &)> &func) const {
    for (const auto &behavior : m_behaviors) {
      func(*behavior);
    }
  }

  [[nodiscard]] auto size() const -> size_t { return m_behaviors.size(); }

  [[nodiscard]] auto empty() const -> bool { return m_behaviors.empty(); }

  void clear() { m_behaviors.clear(); }

private:
  std::vector<std::unique_ptr<AIBehavior>> m_behaviors;
};

} // namespace Game::Systems::AI
