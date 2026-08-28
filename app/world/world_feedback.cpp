#include "app/world/world_feedback.h"

#include <QString>
#include <QVariantMap>

#include <algorithm>
#include <cstdlib>
#include <mutex>

namespace App::Core {

auto feedback_kind_key(FeedbackKind kind) -> const char* {
  switch (kind) {
  case FeedbackKind::Damage:
    return "damage";
  case FeedbackKind::Heal:
    return "heal";
  case FeedbackKind::Resource:
    return "resource";
  case FeedbackKind::Population:
    return "population";
  case FeedbackKind::Count:
    break;
  }
  return "damage";
}

auto feedback_style_key(FeedbackStyle style) -> const char* {
  return style == FeedbackStyle::Burst ? "burst" : "tick";
}

auto WorldFeedbackStore::priority(const WorldFeedbackTick& tick) -> float {
  float score = 0.0F;
  switch (tick.kind) {
  case FeedbackKind::Damage:
    score = static_cast<float>(std::abs(tick.amount)) * 0.01F;
    if (tick.killing_blow) {
      score += 3.0F;
    }
    if (tick.focused) {
      score += 2.0F;
    }
    if (tick.incoming) {
      score += 1.0F;
    }
    if (tick.outgoing) {
      score += 0.5F;
    }
    break;
  case FeedbackKind::Heal:
    score = 1.0F + (static_cast<float>(std::abs(tick.amount)) * 0.01F);
    break;
  case FeedbackKind::Resource:
    score = static_cast<float>(std::abs(tick.amount)) * 0.02F;
    if (tick.paired_resource >= 0) {
      score += 1.0F;
    }
    if (tick.amount < 0) {
      score += 0.5F;
    }
    break;
  case FeedbackKind::Population:
    score = 2.0F;
    break;
  case FeedbackKind::Count:
    break;
  }
  if (tick.style == FeedbackStyle::Burst) {
    score += 4.0F;
  }
  return score;
}

auto WorldFeedbackStore::coalesce_window(FeedbackKind kind) const -> float {
  switch (kind) {
  case FeedbackKind::Damage:
  case FeedbackKind::Heal:
    return m_limits.damage_coalesce_window;
  case FeedbackKind::Resource:
  case FeedbackKind::Population:
    return m_limits.economy_coalesce_window;
  case FeedbackKind::Count:
    break;
  }
  return m_limits.damage_coalesce_window;
}

auto WorldFeedbackStore::merge_into_pending(const WorldFeedbackTick& tick) -> bool {
  if (tick.anchor == Engine::Core::NULL_ENTITY) {
    return false;
  }
  for (auto& pending : m_pending) {
    if (pending.anchor != tick.anchor || pending.kind != tick.kind ||
        pending.resource != tick.resource || pending.style != tick.style) {
      continue;
    }
    if (pending.killing_blow) {
      continue;
    }
    if ((pending.amount < 0) != (tick.amount < 0)) {
      continue;
    }
    pending.amount += tick.amount;
    pending.paired_amount += tick.paired_amount;
    pending.severity = std::min(1.5F, pending.severity + tick.severity);
    pending.x = tick.x;
    pending.y = tick.y;
    pending.z = tick.z;
    pending.incoming = pending.incoming || tick.incoming;
    pending.outgoing = pending.outgoing || tick.outgoing;
    pending.focused = pending.focused || tick.focused;
    pending.killing_blow = pending.killing_blow || tick.killing_blow;
    pending.hits += tick.hits;
    return true;
  }
  return false;
}

auto WorldFeedbackStore::make_room_for(const WorldFeedbackTick& tick) -> bool {
  std::size_t same_kind = 0;
  auto weakest = m_pending.end();
  for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
    if (it->kind != tick.kind) {
      continue;
    }
    ++same_kind;
    if (weakest == m_pending.end() || priority(*it) < priority(*weakest)) {
      weakest = it;
    }
  }
  if (same_kind < m_limits.max_pending_per_kind) {
    return true;
  }
  if (weakest == m_pending.end() || priority(*weakest) >= priority(tick)) {
    return false;
  }
  m_pending.erase(weakest);
  return true;
}

void WorldFeedbackStore::push(WorldFeedbackTick tick) {
  const std::lock_guard<std::mutex> guard(m_mutex);
  tick.age = 0.0F;
  tick.hits = std::max(1, tick.hits);

  if (merge_into_pending(tick)) {
    return;
  }
  if (!make_room_for(tick)) {
    return;
  }

  tick.lane = static_cast<int>(m_sequence % 5U) - 2;
  ++m_sequence;
  m_pending.push_back(tick);
}

void WorldFeedbackStore::update(float dt) {
  if (dt <= 0.0F) {
    return;
  }
  const std::lock_guard<std::mutex> guard(m_mutex);
  for (auto& tick : m_pending) {
    tick.age += dt;
  }
}

auto WorldFeedbackStore::pop_ready() -> std::vector<WorldFeedbackTick> {
  const std::lock_guard<std::mutex> guard(m_mutex);
  std::vector<WorldFeedbackTick> ready;
  std::vector<WorldFeedbackTick> keep;
  keep.reserve(m_pending.size());
  for (auto& tick : m_pending) {
    if (tick.killing_blow || tick.age >= coalesce_window(tick.kind)) {
      ready.push_back(tick);
    } else {
      keep.push_back(tick);
    }
  }
  m_pending.swap(keep);
  std::sort(ready.begin(), ready.end(), [](const auto& a, const auto& b) {
    return priority(a) > priority(b);
  });
  return ready;
}

auto WorldFeedbackStore::pending() const -> std::vector<WorldFeedbackTick> {
  const std::lock_guard<std::mutex> guard(m_mutex);
  return m_pending;
}

void WorldFeedbackStore::clear() {
  const std::lock_guard<std::mutex> guard(m_mutex);
  m_pending.clear();
}

auto WorldFeedbackStore::to_variant(const std::vector<WorldFeedbackTick>& ticks)
    -> QVariantList {
  QVariantList list;
  list.reserve(static_cast<int>(ticks.size()));
  for (const auto& tick : ticks) {
    QVariantMap map;
    map[QStringLiteral("anchor")] = QVariant::fromValue<qulonglong>(tick.anchor);
    map[QStringLiteral("x")] = tick.x;
    map[QStringLiteral("y")] = tick.y;
    map[QStringLiteral("z")] = tick.z;
    map[QStringLiteral("kind")] = QString::fromLatin1(feedback_kind_key(tick.kind));
    map[QStringLiteral("style")] = QString::fromLatin1(feedback_style_key(tick.style));
    map[QStringLiteral("amount")] = tick.amount;
    map[QStringLiteral("severity")] = tick.severity;
    map[QStringLiteral("resource")] = tick.resource;
    map[QStringLiteral("pairedResource")] = tick.paired_resource;
    map[QStringLiteral("pairedAmount")] = tick.paired_amount;
    map[QStringLiteral("killingBlow")] = tick.killing_blow;
    map[QStringLiteral("incoming")] = tick.incoming;
    map[QStringLiteral("outgoing")] = tick.outgoing;
    map[QStringLiteral("focused")] = tick.focused;
    map[QStringLiteral("lane")] = tick.lane;
    map[QStringLiteral("hits")] = tick.hits;
    list.append(map);
  }
  return list;
}

} // namespace App::Core
