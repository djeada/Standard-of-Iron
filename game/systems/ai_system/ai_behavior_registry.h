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
  AIBehaviorRegistry &operator=(const AIBehaviorRegistry &) = delete;

  void registerBehavior(std::unique_ptr<AIBehavior> behavior) {
    m_behaviors.push_back(std::move(behavior));

    std::sort(m_behaviors.begin(), m_behaviors.end(),
              [](const std::unique_ptr<AIBehavior> &a,
                 const std::unique_ptr<AIBehavior> &b) {
                return a->getPriority() > b->getPriority();
              });
  }

  void forEach(std::function<void(AIBehavior &)> func) {
    for (auto &behavior : m_behaviors) {
      func(*behavior);
    }
  }

  void forEach(std::function<void(const AIBehavior &)> func) const {
    for (const auto &behavior : m_behaviors) {
      func(*behavior);
    }
  }

  size_t size() const { return m_behaviors.size(); }

  bool empty() const { return m_behaviors.empty(); }

  void clear() { m_behaviors.clear(); }

private:
  std::vector<std::unique_ptr<AIBehavior>> m_behaviors;
};

} // namespace Game::Systems::AI
