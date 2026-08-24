#include "app/world/combat_feedback.h"

#include <QVariantMap>

#include <algorithm>
#include <mutex>

namespace App::Core {

auto CombatFeedbackStore::priority(const CombatHitFeedback& hit) -> float {
  float score = static_cast<float>(hit.damage) * 0.01F;
  if (hit.killing_blow) {
    score += 3.0F;
  }
  if (hit.focused) {
    score += 2.0F;
  }
  if (hit.incoming) {
    score += 1.0F;
  }
  if (hit.outgoing) {
    score += 0.5F;
  }
  return score;
}

void CombatFeedbackStore::push(CombatHitFeedback hit) {
  const std::lock_guard<std::mutex> guard(m_mutex);
  hit.age = 0.0F;
  hit.hits = std::max(1, hit.hits);

  if (hit.target != Engine::Core::NULL_ENTITY) {
    for (auto& pending : m_pending) {
      if (pending.target != hit.target || pending.killing_blow) {
        continue;
      }
      pending.damage += hit.damage;
      pending.damage_ratio = std::min(1.5F, pending.damage_ratio + hit.damage_ratio);
      pending.x = hit.x;
      pending.y = hit.y;
      pending.z = hit.z;
      pending.incoming = pending.incoming || hit.incoming;
      pending.outgoing = pending.outgoing || hit.outgoing;
      pending.focused = pending.focused || hit.focused;
      pending.killing_blow = pending.killing_blow || hit.killing_blow;
      pending.hits += hit.hits;
      return;
    }
  }

  hit.lane = static_cast<int>(m_sequence % 5U) - 2;
  ++m_sequence;

  if (m_pending.size() >= m_limits.max_pending) {
    auto weakest = std::min_element(
        m_pending.begin(), m_pending.end(), [](const auto& a, const auto& b) {
          return priority(a) < priority(b);
        });
    if (weakest == m_pending.end() || priority(*weakest) >= priority(hit)) {
      return;
    }
    m_pending.erase(weakest);
  }
  m_pending.push_back(hit);
}

void CombatFeedbackStore::update(float dt) {
  if (dt <= 0.0F) {
    return;
  }
  const std::lock_guard<std::mutex> guard(m_mutex);
  for (auto& hit : m_pending) {
    hit.age += dt;
  }
}

auto CombatFeedbackStore::pop_ready() -> std::vector<CombatHitFeedback> {
  const std::lock_guard<std::mutex> guard(m_mutex);
  std::vector<CombatHitFeedback> ready;
  std::vector<CombatHitFeedback> keep;
  keep.reserve(m_pending.size());
  for (auto& hit : m_pending) {
    if (hit.killing_blow || hit.age >= m_limits.coalesce_window) {
      ready.push_back(hit);
    } else {
      keep.push_back(hit);
    }
  }
  m_pending.swap(keep);
  std::sort(ready.begin(), ready.end(), [](const auto& a, const auto& b) {
    return priority(a) > priority(b);
  });
  return ready;
}

auto CombatFeedbackStore::pending() const -> std::vector<CombatHitFeedback> {
  const std::lock_guard<std::mutex> guard(m_mutex);
  return m_pending;
}

void CombatFeedbackStore::clear() {
  const std::lock_guard<std::mutex> guard(m_mutex);
  m_pending.clear();
}

auto CombatFeedbackStore::to_variant(const std::vector<CombatHitFeedback>& hits)
    -> QVariantList {
  QVariantList list;
  list.reserve(static_cast<int>(hits.size()));
  for (const auto& hit : hits) {
    QVariantMap map;
    map[QStringLiteral("target")] = QVariant::fromValue<qulonglong>(hit.target);
    map[QStringLiteral("x")] = hit.x;
    map[QStringLiteral("y")] = hit.y;
    map[QStringLiteral("z")] = hit.z;
    map[QStringLiteral("damage")] = hit.damage;
    map[QStringLiteral("damageRatio")] = hit.damage_ratio;
    map[QStringLiteral("killingBlow")] = hit.killing_blow;
    map[QStringLiteral("incoming")] = hit.incoming;
    map[QStringLiteral("outgoing")] = hit.outgoing;
    map[QStringLiteral("focused")] = hit.focused;
    map[QStringLiteral("lane")] = hit.lane;
    map[QStringLiteral("hits")] = hit.hits;
    list.append(map);
  }
  return list;
}

} // namespace App::Core
