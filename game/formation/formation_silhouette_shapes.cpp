#include "formation_silhouette_shapes.h"

#include <algorithm>
#include <cmath>

namespace Game::Formation::planning {

namespace {

constexpr float k_crescent_bend = 0.85F;
constexpr float k_horn_turn_degrees = 28.0F;
constexpr float k_horn_reach_ratio = 1.35F;
constexpr float k_crescent_turn_share = 0.6F;

struct Post {
  float x;
  float z;
  float facing;
};

auto widest_body(const std::vector<std::size_t>& indices,
                 const std::vector<float>& half_width,
                 const std::vector<float>& half_depth) -> float {
  float body = 0.0F;
  for (auto const index : indices) {
    body = std::max(body, std::max(half_width[index], half_depth[index]));
  }
  return body;
}

auto perimeter_posts(int count, float pitch) -> std::vector<Post> {
  int const across = std::max(2, (count + 3) / 4 + 1);
  float const half_side = pitch * static_cast<float>(across - 1) * 0.5F;

  std::vector<Post> posts;
  for (int i = 0; i < across; ++i) {
    float const t = -half_side + pitch * static_cast<float>(i);
    posts.push_back({t, half_side, 0.0F});
    posts.push_back({t, -half_side, 180.0F});
  }
  for (int i = 1; i + 1 < across; ++i) {
    float const t = -half_side + pitch * static_cast<float>(i);
    posts.push_back({-half_side, t, -90.0F});
    posts.push_back({half_side, t, 90.0F});
  }
  std::stable_sort(posts.begin(), posts.end(), [](const Post& a, const Post& b) {
    if (std::abs(a.z - b.z) > 0.01F) {
      return a.z > b.z;
    }
    return std::abs(a.x) < std::abs(b.x);
  });
  return posts;
}

void place_perimeter(std::vector<FormationSlot>& slot_list,
                     const std::vector<std::size_t>& perimeter,
                     const std::vector<float>& half_width,
                     const std::vector<float>& half_depth,
                     float lateral_gap) {
  float const body = widest_body(perimeter, half_width, half_depth);
  float const pitch = 2.0F * body + lateral_gap;
  auto const posts = perimeter_posts(static_cast<int>(perimeter.size()), pitch);
  for (std::size_t k = 0; k < perimeter.size(); ++k) {
    auto& slot = slot_list[perimeter[k]];
    const Post& post = posts[k];
    slot.local_offset = QVector3D(post.x, 0.0F, post.z);
    slot.local_facing = post.facing;
    slot.rank = post.z > 0.0F ? 0 : 1;
    slot.file = static_cast<int>(k);
  }
}

void place_inner_block(std::vector<FormationSlot>& slot_list,
                       const std::vector<std::size_t>& inner,
                       const std::vector<float>& half_width,
                       const std::vector<float>& half_depth,
                       float pitch_gap) {
  int const inner_files = std::max(
      1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(inner.size())))));
  float const inner_body = widest_body(inner, half_width, half_depth);
  float const inner_pitch = 2.0F * inner_body + pitch_gap;
  int const inner_rows =
      (static_cast<int>(inner.size()) + inner_files - 1) / std::max(1, inner_files);
  for (std::size_t k = 0; k < inner.size(); ++k) {
    int const row = static_cast<int>(k) / inner_files;
    int const file = static_cast<int>(k) % inner_files;
    auto& slot = slot_list[inner[k]];
    slot.local_offset = QVector3D(
        (static_cast<float>(file) - static_cast<float>(inner_files - 1) * 0.5F) *
            inner_pitch,
        0.0F,
        (static_cast<float>(inner_rows - 1) * 0.5F - static_cast<float>(row)) *
            inner_pitch);
    slot.local_facing = 0.0F;
    slot.rank = 2 + row;
    slot.file = file;
  }
}

} // namespace

void place_hollow_square(std::vector<FormationSlot>& slot_list,
                         const HollowSquareMembers& members,
                         const std::vector<float>& half_width,
                         const std::vector<float>& half_depth,
                         float lateral_gap,
                         float rank_gap) {
  place_perimeter(slot_list, members.perimeter, half_width, half_depth, lateral_gap);
  place_inner_block(slot_list,
                    members.inner,
                    half_width,
                    half_depth,
                    std::min(lateral_gap, rank_gap));
}

void bend_into_crescent(std::vector<FormationSlot>& slot_list,
                        const std::vector<std::size_t>& left_wing,
                        const std::vector<std::size_t>& right_wing,
                        float front_half_width) {
  float const half = std::max(1.0F, front_half_width);
  float const bend = half * k_crescent_bend;
  auto is_horn = [&](std::size_t index) {
    return std::find(left_wing.begin(), left_wing.end(), index) != left_wing.end() ||
           std::find(right_wing.begin(), right_wing.end(), index) != right_wing.end();
  };
  for (std::size_t index = 0; index < slot_list.size(); ++index) {
    auto& slot = slot_list[index];
    float const x = slot.local_offset.x();
    if (is_horn(index)) {
      slot.local_offset.setZ(slot.local_offset.z() + bend * k_horn_reach_ratio);
      slot.local_facing = x < 0.0F ? k_horn_turn_degrees : -k_horn_turn_degrees;
      continue;
    }
    float const t = std::clamp(x / half, -1.0F, 1.0F);
    slot.local_offset.setZ(slot.local_offset.z() + bend * t * t);
    slot.local_facing = -t * k_horn_turn_degrees * k_crescent_turn_share;
  }
}

} // namespace Game::Formation::planning
