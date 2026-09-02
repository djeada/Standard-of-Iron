#pragma once

#include <QPainter>
#include <QPointF>

#include <functional>
#include <vector>

#include "app/world/world_feedback.h"
#include "game/core/event_manager.h"

namespace Engine::Core {
class World;
}

class ArenaFeedback {
public:
  using Projector = std::function<bool(float x, float y, float z, QPointF& out)>;

  ArenaFeedback();

  void set_world(Engine::Core::World* world) { m_world = world; }

  void advance(float dt);

  void draw(QPainter& painter, const Projector& project, float ui_scale = 1.0F) const;

  void clear();

  [[nodiscard]] auto live_count() const -> int {
    return static_cast<int>(m_floaters.size());
  }

private:
  struct Floater {
    App::Core::WorldFeedbackTick tick;
    float age = 0.0F;
    float life = 0.85F;
  };

  void collect_ready();

  Engine::Core::World* m_world = nullptr;
  App::Core::WorldFeedbackStore m_store;
  std::vector<Floater> m_floaters;

  Engine::Core::ScopedEventSubscription<Engine::Core::CombatHitEvent>
      m_hit_subscription;
  Engine::Core::ScopedEventSubscription<Engine::Core::WorldFeedbackEvent>
      m_world_feedback_subscription;
};
