#include "visibility_service.h"

#include "../core/component.h"
#include "../core/world.h"
#include <algorithm>
#include <cmath>

namespace Game::Map {

namespace {
constexpr float kDefaultVisionRange = 12.0f;
}

VisibilityService &VisibilityService::instance() {
  static VisibilityService s_instance;
  return s_instance;
}

void VisibilityService::initialize(int width, int height, float tileSize) {
  m_width = std::max(1, width);
  m_height = std::max(1, height);
  m_tileSize = std::max(0.0001f, tileSize);
  m_halfWidth = m_width * 0.5f - 0.5f;
  m_halfHeight = m_height * 0.5f - 0.5f;
  const int count = m_width * m_height;
  m_cells.assign(count, static_cast<std::uint8_t>(VisibilityState::Unseen));
  m_currentVisible.assign(count, 0);
  m_version++;
  m_initialized = true;
}

void VisibilityService::reset() {
  if (!m_initialized)
    return;
  std::fill(m_cells.begin(), m_cells.end(),
            static_cast<std::uint8_t>(VisibilityState::Unseen));
  std::fill(m_currentVisible.begin(), m_currentVisible.end(), 0);
  m_version++;
}

void VisibilityService::update(Engine::Core::World &world, int playerId) {
  if (!m_initialized)
    return;

  std::fill(m_currentVisible.begin(), m_currentVisible.end(), 0);

  auto entities = world.getEntitiesWith<Engine::Core::TransformComponent>();
  const float rangePadding = m_tileSize * 0.5f;

  for (auto *entity : entities) {
    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!transform || !unit)
      continue;
    if (unit->ownerId != playerId)
      continue;
    if (unit->health <= 0)
      continue;

    const float visionRange = std::max(unit->visionRange, kDefaultVisionRange);

    const int centerX = worldToGrid(transform->position.x, m_halfWidth);
    const int centerZ = worldToGrid(transform->position.z, m_halfHeight);
    if (!inBounds(centerX, centerZ))
      continue;
    const int cellRadius =
        std::max(1, static_cast<int>(std::ceil(visionRange / m_tileSize)));

    for (int dz = -cellRadius; dz <= cellRadius; ++dz) {
      const int gz = centerZ + dz;
      if (!inBounds(centerX, gz))
        continue;
      const float worldDz = dz * m_tileSize;
      for (int dx = -cellRadius; dx <= cellRadius; ++dx) {
        const int gx = centerX + dx;
        if (!inBounds(gx, gz))
          continue;
        const float worldDx = dx * m_tileSize;
        const float distSq = worldDx * worldDx + worldDz * worldDz;
        if (distSq <=
            (visionRange + rangePadding) * (visionRange + rangePadding)) {
          const int idx = index(gx, gz);
          m_currentVisible[idx] = 1;
        }
      }
    }
  }

  bool changed = false;
  for (int idx = 0; idx < static_cast<int>(m_cells.size()); ++idx) {
    const std::uint8_t nowVisible = m_currentVisible[idx];

    if (nowVisible) {
      if (m_cells[idx] != static_cast<std::uint8_t>(VisibilityState::Visible)) {
        m_cells[idx] = static_cast<std::uint8_t>(VisibilityState::Visible);
        changed = true;
      }
    } else {
      if (m_cells[idx] == static_cast<std::uint8_t>(VisibilityState::Visible)) {
        m_cells[idx] = static_cast<std::uint8_t>(VisibilityState::Explored);
        changed = true;
      }
    }
  }

  if (changed)
    ++m_version;
}

VisibilityState VisibilityService::stateAt(int gridX, int gridZ) const {
  if (!m_initialized || !inBounds(gridX, gridZ))
    return VisibilityState::Visible;
  return static_cast<VisibilityState>(m_cells[index(gridX, gridZ)]);
}

bool VisibilityService::isVisibleWorld(float worldX, float worldZ) const {
  if (!m_initialized)
    return true;
  const int gx = worldToGrid(worldX, m_halfWidth);
  const int gz = worldToGrid(worldZ, m_halfHeight);
  if (!inBounds(gx, gz))
    return false;
  return m_cells[index(gx, gz)] ==
         static_cast<std::uint8_t>(VisibilityState::Visible);
}

bool VisibilityService::isExploredWorld(float worldX, float worldZ) const {
  if (!m_initialized)
    return true;
  const int gx = worldToGrid(worldX, m_halfWidth);
  const int gz = worldToGrid(worldZ, m_halfHeight);
  if (!inBounds(gx, gz))
    return false;
  const auto state = m_cells[index(gx, gz)];
  return state == static_cast<std::uint8_t>(VisibilityState::Visible) ||
         state == static_cast<std::uint8_t>(VisibilityState::Explored);
}

bool VisibilityService::inBounds(int x, int z) const {
  return x >= 0 && x < m_width && z >= 0 && z < m_height;
}

int VisibilityService::index(int x, int z) const { return z * m_width + x; }

int VisibilityService::worldToGrid(float worldCoord, float half) const {
  float gridCoord = worldCoord / m_tileSize + half;
  return static_cast<int>(std::floor(gridCoord + 0.5f));
}

} // namespace Game::Map
