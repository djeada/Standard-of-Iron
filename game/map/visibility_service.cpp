#include "visibility_service.h"

#include "../core/component.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../systems/owner_registry.h"
#include <algorithm>
#include <cmath>

namespace Game::Map {

namespace {
constexpr float kDefaultVisionRange = 12.0f;

bool inBoundsStatic(int x, int z, int width, int height) {
  return x >= 0 && x < width && z >= 0 && z < height;
}

int indexStatic(int x, int z, int width) { return z * width + x; }

int worldToGridStatic(float worldCoord, float half, float tileSize) {
  float gridCoord = worldCoord / tileSize + half;
  return static_cast<int>(std::floor(gridCoord + 0.5f));
}
} // namespace

VisibilityService &VisibilityService::instance() {
  static VisibilityService s_instance;
  return s_instance;
}

void VisibilityService::initialize(int width, int height, float tileSize) {
  std::unique_lock lock(m_cellsMutex);
  m_width = std::max(1, width);
  m_height = std::max(1, height);
  m_tileSize = std::max(0.0001f, tileSize);
  m_halfWidth = m_width * 0.5f - 0.5f;
  m_halfHeight = m_height * 0.5f - 0.5f;
  const int count = m_width * m_height;
  m_cells.assign(count, static_cast<std::uint8_t>(VisibilityState::Unseen));
  m_version.store(1, std::memory_order_release);
  m_generation.store(0, std::memory_order_release);
  m_initialized = true;
}

void VisibilityService::reset() {
  if (!m_initialized) {
    return;
  }
  std::unique_lock lock(m_cellsMutex);
  std::fill(m_cells.begin(), m_cells.end(),
            static_cast<std::uint8_t>(VisibilityState::Unseen));
  m_version.fetch_add(1, std::memory_order_release);
}

bool VisibilityService::update(Engine::Core::World &world, int playerId) {
  if (!m_initialized) {
    return false;
  }

  bool integrated = integrateCompletedJob();

  if (!m_jobActive.load(std::memory_order_acquire)) {
    auto sources = gatherVisionSources(world, playerId);
    auto payload = composeJobPayload(sources);
    startAsyncJob(std::move(payload));
  }

  return integrated;
}

void VisibilityService::computeImmediate(Engine::Core::World &world,
                                         int playerId) {
  if (!m_initialized) {
    return;
  }

  auto sources = gatherVisionSources(world, playerId);
  auto payload = composeJobPayload(sources);
  auto result = executeJob(std::move(payload));

  if (result.changed) {
    std::unique_lock lock(m_cellsMutex);
    m_cells = std::move(result.cells);
    m_version.fetch_add(1, std::memory_order_release);
  }
}

std::vector<VisibilityService::VisionSource>
VisibilityService::gatherVisionSources(Engine::Core::World &world,
                                       int playerId) const {
  std::vector<VisionSource> sources;
  auto entities = world.getEntitiesWith<Engine::Core::TransformComponent>();
  const float rangePadding = m_tileSize * 0.5f;

  auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();

  for (auto *entity : entities) {
    auto *transform = entity->getComponent<Engine::Core::TransformComponent>();
    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!transform || !unit) {
      continue;
    }

    if (Game::Core::isNeutralOwner(unit->ownerId)) {
      continue;
    }

    if (unit->ownerId != playerId &&
        !ownerRegistry.areAllies(playerId, unit->ownerId)) {
      continue;
    }

    if (unit->health <= 0) {
      continue;
    }

    const float visionRange = std::max(unit->visionRange, kDefaultVisionRange);
    const int centerX = worldToGrid(transform->position.x, m_halfWidth);
    const int centerZ = worldToGrid(transform->position.z, m_halfHeight);
    if (!inBounds(centerX, centerZ)) {
      continue;
    }

    const int cellRadius =
        std::max(1, static_cast<int>(std::ceil(visionRange / m_tileSize)));
    const float expandedRangeSq =
        (visionRange + rangePadding) * (visionRange + rangePadding);

    sources.push_back({centerX, centerZ, cellRadius, expandedRangeSq});
  }

  return sources;
}

VisibilityService::JobPayload VisibilityService::composeJobPayload(
    const std::vector<VisionSource> &sources) const {
  std::shared_lock lock(m_cellsMutex);
  const auto gen = const_cast<std::atomic<std::uint64_t> &>(m_generation)
                       .fetch_add(1, std::memory_order_relaxed);
  return JobPayload{m_width, m_height, m_tileSize, m_cells, sources, gen};
}

void VisibilityService::startAsyncJob(JobPayload &&payload) {
  m_jobActive.store(true, std::memory_order_release);
  m_pendingJob = std::async(std::launch::async, executeJob, std::move(payload));
}

bool VisibilityService::integrateCompletedJob() {
  if (!m_jobActive.load(std::memory_order_acquire)) {
    return false;
  }

  if (m_pendingJob.wait_for(std::chrono::seconds(0)) !=
      std::future_status::ready) {
    return false;
  }

  auto result = m_pendingJob.get();
  m_jobActive.store(false, std::memory_order_release);

  if (result.changed) {
    std::unique_lock lock(m_cellsMutex);
    m_cells = std::move(result.cells);
    m_version.fetch_add(1, std::memory_order_release);
    return true;
  }

  return false;
}

VisibilityService::JobResult VisibilityService::executeJob(JobPayload payload) {
  const int count = payload.width * payload.height;
  std::vector<std::uint8_t> currentVisible(count, 0);

  const float halfWidth = payload.width * 0.5f - 0.5f;
  const float halfHeight = payload.height * 0.5f - 0.5f;

  for (const auto &src : payload.sources) {
    for (int dz = -src.cellRadius; dz <= src.cellRadius; ++dz) {
      const int gz = src.centerZ + dz;
      if (!inBoundsStatic(src.centerX, gz, payload.width, payload.height)) {
        continue;
      }
      const float worldDz = dz * payload.tileSize;
      for (int dx = -src.cellRadius; dx <= src.cellRadius; ++dx) {
        const int gx = src.centerX + dx;
        if (!inBoundsStatic(gx, gz, payload.width, payload.height)) {
          continue;
        }
        const float worldDx = dx * payload.tileSize;
        const float distSq = worldDx * worldDx + worldDz * worldDz;
        if (distSq <= src.expandedRangeSq) {
          const int idx = indexStatic(gx, gz, payload.width);
          currentVisible[idx] = 1;
        }
      }
    }
  }

  bool changed = false;
  for (int idx = 0; idx < count; ++idx) {
    const std::uint8_t nowVisible = currentVisible[idx];

    if (nowVisible) {
      if (payload.cells[idx] !=
          static_cast<std::uint8_t>(VisibilityState::Visible)) {
        payload.cells[idx] =
            static_cast<std::uint8_t>(VisibilityState::Visible);
        changed = true;
      }
    } else {
      if (payload.cells[idx] ==
          static_cast<std::uint8_t>(VisibilityState::Visible)) {
        payload.cells[idx] =
            static_cast<std::uint8_t>(VisibilityState::Explored);
        changed = true;
      }
    }
  }

  return JobResult{std::move(payload.cells), payload.generation, changed};
}

VisibilityState VisibilityService::stateAt(int gridX, int gridZ) const {
  if (!m_initialized || !inBounds(gridX, gridZ)) {
    return VisibilityState::Visible;
  }
  std::shared_lock lock(m_cellsMutex);
  return static_cast<VisibilityState>(m_cells[index(gridX, gridZ)]);
}

bool VisibilityService::isVisibleWorld(float worldX, float worldZ) const {
  if (!m_initialized) {
    return true;
  }
  const int gx = worldToGrid(worldX, m_halfWidth);
  const int gz = worldToGrid(worldZ, m_halfHeight);
  if (!inBounds(gx, gz)) {
    return false;
  }
  std::shared_lock lock(m_cellsMutex);
  return m_cells[index(gx, gz)] ==
         static_cast<std::uint8_t>(VisibilityState::Visible);
}

bool VisibilityService::isExploredWorld(float worldX, float worldZ) const {
  if (!m_initialized) {
    return true;
  }
  const int gx = worldToGrid(worldX, m_halfWidth);
  const int gz = worldToGrid(worldZ, m_halfHeight);
  if (!inBounds(gx, gz)) {
    return false;
  }
  std::shared_lock lock(m_cellsMutex);
  const auto state = m_cells[index(gx, gz)];
  return state == static_cast<std::uint8_t>(VisibilityState::Visible) ||
         state == static_cast<std::uint8_t>(VisibilityState::Explored);
}

std::vector<std::uint8_t> VisibilityService::snapshotCells() const {
  std::shared_lock lock(m_cellsMutex);
  return m_cells;
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
