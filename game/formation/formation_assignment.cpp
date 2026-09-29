#include "formation_assignment.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <unordered_map>

#include "formation_frame.h"

namespace Game::Formation::planning {

namespace {

constexpr float k_inf = std::numeric_limits<float>::max() / 4.0F;

struct HungarianState {
  std::vector<float> u;
  std::vector<float> v;
  std::vector<int> p;
  std::vector<int> way;

  explicit HungarianState(int n)
      : u(static_cast<std::size_t>(n) + 1U, 0.0F)
      , v(static_cast<std::size_t>(n) + 1U, 0.0F)
      , p(static_cast<std::size_t>(n) + 1U, 0)
      , way(static_cast<std::size_t>(n) + 1U, 0) {}
};

struct ColumnSearch {
  std::vector<float> minv;
  std::vector<bool> used;

  explicit ColumnSearch(int n)
      : minv(static_cast<std::size_t>(n) + 1U, k_inf)
      , used(static_cast<std::size_t>(n) + 1U, false) {}
};

auto relax_unused_columns(const std::vector<std::vector<float>>& cost,
                          int n,
                          int j0,
                          HungarianState& state,
                          ColumnSearch& search,
                          int& j1_out) -> float {
  int const i0 = state.p[static_cast<std::size_t>(j0)];
  float delta = k_inf;
  int j1 = 0;
  for (int j = 1; j <= n; ++j) {
    if (search.used[static_cast<std::size_t>(j)]) {
      continue;
    }
    float const cur =
        cost[static_cast<std::size_t>(i0 - 1)][static_cast<std::size_t>(j - 1)] -
        state.u[static_cast<std::size_t>(i0)] - state.v[static_cast<std::size_t>(j)];
    if (cur < search.minv[static_cast<std::size_t>(j)]) {
      search.minv[static_cast<std::size_t>(j)] = cur;
      state.way[static_cast<std::size_t>(j)] = j0;
    }
    if (search.minv[static_cast<std::size_t>(j)] < delta) {
      delta = search.minv[static_cast<std::size_t>(j)];
      j1 = j;
    }
  }
  j1_out = j1;
  return delta;
}

void shift_potentials(int n, float delta, HungarianState& state, ColumnSearch& search) {
  for (int j = 0; j <= n; ++j) {
    if (search.used[static_cast<std::size_t>(j)]) {
      state.u[static_cast<std::size_t>(state.p[static_cast<std::size_t>(j)])] += delta;
      state.v[static_cast<std::size_t>(j)] -= delta;
    } else {
      search.minv[static_cast<std::size_t>(j)] -= delta;
    }
  }
}

void augment_row(const std::vector<std::vector<float>>& cost,
                 int n,
                 int i,
                 HungarianState& state) {
  state.p[0] = i;
  int j0 = 0;
  ColumnSearch search(n);
  do {
    search.used[static_cast<std::size_t>(j0)] = true;
    int j1 = 0;
    float const delta = relax_unused_columns(cost, n, j0, state, search, j1);
    shift_potentials(n, delta, state, search);
    j0 = j1;
  } while (state.p[static_cast<std::size_t>(j0)] != 0);
  do {
    int const j1 = state.way[static_cast<std::size_t>(j0)];
    state.p[static_cast<std::size_t>(j0)] = state.p[static_cast<std::size_t>(j1)];
    j0 = j1;
  } while (j0 != 0);
}

auto bucket_of_kind(const std::vector<std::uint64_t>& kinds,
                    std::size_t first,
                    std::vector<bool>& done) -> std::vector<std::size_t> {
  std::vector<std::size_t> bucket;
  for (std::size_t i = first; i < kinds.size(); ++i) {
    if (!done[i] && kinds[i] == kinds[first]) {
      bucket.push_back(i);
      done[i] = true;
    }
  }
  return bucket;
}

} // namespace

auto min_cost_assignment(const std::vector<std::vector<float>>& cost)
    -> std::vector<int> {
  auto const n = static_cast<int>(cost.size());
  HungarianState state(n);
  for (int i = 1; i <= n; ++i) {
    augment_row(cost, n, i, state);
  }
  std::vector<int> row_to_column(static_cast<std::size_t>(n), -1);
  for (int j = 1; j <= n; ++j) {
    if (state.p[static_cast<std::size_t>(j)] > 0) {
      row_to_column[static_cast<std::size_t>(state.p[static_cast<std::size_t>(j)] -
                                             1)] = j - 1;
    }
  }
  return row_to_column;
}

void assign_nearest_troops(std::vector<FormationSlot>& slot_list,
                           std::vector<int>& slot_files,
                           const NearestAssignmentInput& input) {
  auto const count = slot_list.size();
  std::vector<bool> done(count, false);
  std::vector<EntityID> occupants(count, 0U);
  std::vector<int> files(count, 0);
  for (std::size_t first = 0; first < count; ++first) {
    if (done[first]) {
      continue;
    }
    auto const bucket = bucket_of_kind(input.slot_kind, first, done);
    std::vector<std::vector<float>> cost(bucket.size(),
                                         std::vector<float>(bucket.size(), 0.0F));
    for (std::size_t r = 0; r < bucket.size(); ++r) {
      QVector3D const start = input.slot_start[bucket[r]];
      for (std::size_t c = 0; c < bucket.size(); ++c) {
        QVector3D const rotated =
            rotate_offset(slot_list[bucket[c]].local_offset, input.facing);
        QVector3D const target(
            input.anchor.x() + rotated.x(), 0.0F, input.anchor.z() + rotated.z());
        cost[r][c] =
            QVector3D(target.x() - start.x(), 0.0F, target.z() - start.z()).length();
      }
    }
    auto const chosen = min_cost_assignment(cost);
    for (std::size_t r = 0; r < bucket.size(); ++r) {
      auto const column = chosen[r] >= 0 ? static_cast<std::size_t>(chosen[r]) : r;
      occupants[bucket[column]] = slot_list[bucket[r]].occupant;
      files[bucket[column]] = slot_files[bucket[r]];
    }
  }
  for (std::size_t i = 0; i < count; ++i) {
    slot_list[i].occupant = occupants[i];
    slot_files[i] = files[i];
  }
}

namespace {

using ReferenceMap = std::unordered_map<EntityID, const FormationSlot*>;

auto funnel_before(const QVector3D& a, const QVector3D& b) -> bool {
  if (std::abs(a.z() - b.z()) > 0.01F) {
    return a.z() > b.z();
  }
  if (std::abs(std::abs(a.x()) - std::abs(b.x())) > 0.01F) {
    return std::abs(a.x()) < std::abs(b.x());
  }
  return a.x() < b.x();
}

auto size_key(const FormationSlot& slot) -> std::uint64_t {
  return (static_cast<std::uint64_t>(slot.role) << 1U) |
         static_cast<std::uint64_t>(slot.heavy);
}

auto references_every_occupant(const std::vector<FormationSlot>& slot_list,
                               const ReferenceMap& reference_of) -> bool {
  return std::all_of(
      slot_list.begin(), slot_list.end(), [&](const FormationSlot& slot) {
        return reference_of.count(slot.occupant) != 0U;
      });
}

void seat_bucket_in_funnel_order(const std::vector<FormationSlot>& slot_list,
                                 std::size_t first,
                                 std::vector<bool>& done,
                                 ReferenceMap& reference_of,
                                 std::vector<EntityID>& occupants) {
  auto const key = size_key(slot_list[first]);
  std::vector<std::size_t> positions;
  std::vector<EntityID> troops;
  for (std::size_t i = first; i < slot_list.size(); ++i) {
    if (!done[i] && size_key(slot_list[i]) == key) {
      done[i] = true;
      positions.push_back(i);
      troops.push_back(slot_list[i].occupant);
    }
  }
  std::stable_sort(
      positions.begin(), positions.end(), [&](std::size_t a, std::size_t b) {
        return funnel_before(slot_list[a].local_offset, slot_list[b].local_offset);
      });
  std::stable_sort(troops.begin(), troops.end(), [&](EntityID a, EntityID b) {
    return funnel_before(reference_of[a]->local_offset, reference_of[b]->local_offset);
  });
  for (std::size_t k = 0; k < positions.size(); ++k) {
    occupants[positions[k]] = troops[k];
  }
}

auto previous_slot_of_each_occupant(const std::vector<FormationSlot>& previous_slots,
                                    std::size_t slot_count)
    -> std::unordered_map<EntityID, std::size_t> {
  std::unordered_map<EntityID, std::size_t> desired;
  for (const auto& old_slot : previous_slots) {
    if (old_slot.occupant == 0U || old_slot.id < 0) {
      continue;
    }
    auto const index = static_cast<std::size_t>(old_slot.id);
    if (index < slot_count) {
      desired.emplace(old_slot.occupant, index);
    }
  }
  return desired;
}

} // namespace

void fold_onto_reference(std::vector<FormationSlot>& slot_list,
                         const std::vector<FormationSlot>& reference) {
  ReferenceMap reference_of;
  for (const auto& slot : reference) {
    if (slot.occupant != 0U) {
      reference_of.emplace(slot.occupant, &slot);
    }
  }
  if (!references_every_occupant(slot_list, reference_of)) {
    return;
  }

  auto const count = slot_list.size();
  std::vector<bool> done(count, false);
  std::vector<EntityID> occupants(count, 0U);
  for (std::size_t first = 0; first < count; ++first) {
    if (!done[first]) {
      seat_bucket_in_funnel_order(slot_list, first, done, reference_of, occupants);
    }
  }
  for (std::size_t i = 0; i < count; ++i) {
    slot_list[i].occupant = occupants[i];
    slot_list[i].id = reference_of[occupants[i]]->id;
  }
}

void keep_previous_occupants(std::vector<FormationSlot>& slot_list,
                             const std::vector<std::uint64_t>& slot_keys,
                             const std::vector<FormationSlot>& previous_slots) {
  auto const slot_count = slot_list.size();
  auto const desired = previous_slot_of_each_occupant(previous_slots, slot_count);
  std::vector<EntityID> occupants(slot_count, 0U);
  std::vector<bool> bucket_done(slot_count, false);
  for (std::size_t first = 0; first < slot_count; ++first) {
    if (bucket_done[first]) {
      continue;
    }
    auto const bucket = bucket_of_kind(slot_keys, first, bucket_done);
    std::vector<EntityID> pending;
    for (auto const i : bucket) {
      auto const entity = slot_list[i].occupant;
      auto const wanted = desired.find(entity);
      bool const in_bucket = wanted != desired.end() && wanted->second < slot_count &&
                             slot_keys[wanted->second] == slot_keys[first];
      if (in_bucket && occupants[wanted->second] == 0U) {
        occupants[wanted->second] = entity;
      } else {
        pending.push_back(entity);
      }
    }
    std::size_t next = 0;
    for (auto const i : bucket) {
      if (occupants[i] == 0U && next < pending.size()) {
        occupants[i] = pending[next++];
      }
    }
  }
  for (std::size_t i = 0; i < slot_count; ++i) {
    slot_list[i].occupant = occupants[i];
  }
}

} // namespace Game::Formation::planning
