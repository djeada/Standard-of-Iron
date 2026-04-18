// Stage 4 — foundation tests for PoseKey + PoseTemplateCache.
//
// These tests lock in the core Stage 4 invariants:
//   1. PoseKey deliberately excludes world transform and owner id — two
//      differently-positioned units with the same animation/variant/frame
//      MUST hit the same cache slot.
//   2. expand_to_world() folds a world matrix onto local-space templates
//      without mutating the cache, so one cached pose can serve N units.
//   3. Eviction is age-based and only fires for entries older than the
//      caller-specified window.

#include "render/draw_part.h"
#include "render/humanoid/pose_key.h"
#include "render/humanoid/pose_template_cache.h"

#include <QMatrix4x4>
#include <gtest/gtest.h>

namespace {

auto make_local_template(std::size_t part_count)
    -> std::vector<Render::GL::DrawPartCmd> {
  std::vector<Render::GL::DrawPartCmd> parts;
  parts.reserve(part_count);
  for (std::size_t i = 0; i < part_count; ++i) {
    Render::GL::DrawPartCmd cmd{};
    cmd.world.setToIdentity();
    // Put each part at a distinct local position so we can verify that
    // the world transform is applied correctly during expansion.
    cmd.world.translate(static_cast<float>(i), 0.0F, 0.0F);
    parts.push_back(cmd);
  }
  return parts;
}

} // namespace

TEST(PoseKey, EqualityIgnoresNothingInItsFields) {
  Render::GL::PoseKey a{42, Render::GL::AnimationState::Walk,
                        Render::GL::PoseStance::Guard, 7};
  Render::GL::PoseKey b = a;
  EXPECT_EQ(a, b);

  b.frame = 8;
  EXPECT_FALSE(a == b);
  b = a;

  b.anim = Render::GL::AnimationState::Run;
  EXPECT_FALSE(a == b);
  b = a;

  b.stance = Render::GL::PoseStance::Ranged;
  EXPECT_FALSE(a == b);
  b = a;

  b.variant_id = 43;
  EXPECT_FALSE(a == b);
}

TEST(PoseKey, HashDistinguishesDifferentKeys) {
  std::hash<Render::GL::PoseKey> h;
  Render::GL::PoseKey k1{1, Render::GL::AnimationState::Idle,
                         Render::GL::PoseStance::Neutral, 0};
  Render::GL::PoseKey k2{2, Render::GL::AnimationState::Idle,
                         Render::GL::PoseStance::Neutral, 0};
  Render::GL::PoseKey k3{1, Render::GL::AnimationState::Walk,
                         Render::GL::PoseStance::Neutral, 0};
  // Can't assert specific hash values, but collisions between these three
  // would defeat the cache's purpose. Not a proof of correctness — a smoke
  // test that the hash at least looks at every field.
  EXPECT_NE(h(k1), h(k2));
  EXPECT_NE(h(k1), h(k3));
}

TEST(PoseTemplateCache, FindReturnsNullptrOnMiss) {
  Render::GL::PoseTemplateCache cache;
  cache.begin_frame(1);

  Render::GL::PoseKey key{0, Render::GL::AnimationState::Idle,
                          Render::GL::PoseStance::Neutral, 0};
  EXPECT_EQ(cache.find(key), nullptr);
  EXPECT_EQ(cache.stats().misses, 1U);
  EXPECT_EQ(cache.stats().hits, 0U);
}

TEST(PoseTemplateCache, InsertThenFindHits) {
  Render::GL::PoseTemplateCache cache;
  cache.begin_frame(1);

  Render::GL::PoseKey key{5, Render::GL::AnimationState::Walk,
                          Render::GL::PoseStance::Neutral, 3};
  cache.insert(key, make_local_template(4));

  auto *entry = cache.find(key);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->parts.size(), 4U);
  EXPECT_EQ(cache.stats().hits, 1U);
  EXPECT_EQ(cache.stats().inserts, 1U);
}

TEST(PoseTemplateCache, SameKeyDifferentWorldTransformsShareCache) {
  // Stage 4's whole point: a unit that only translated must not cause a
  // new cache entry.
  Render::GL::PoseTemplateCache cache;
  cache.begin_frame(1);

  Render::GL::PoseKey key{0, Render::GL::AnimationState::Idle,
                          Render::GL::PoseStance::Neutral, 0};
  cache.insert(key, make_local_template(3));

  auto const initial_size = cache.size();

  QMatrix4x4 unit_a;
  unit_a.translate(10.0F, 0.0F, 0.0F);
  QMatrix4x4 unit_b;
  unit_b.translate(-5.0F, 0.0F, 12.0F);

  auto *tpl = cache.find(key);
  ASSERT_NE(tpl, nullptr);

  std::vector<Render::GL::DrawPartCmd> out_a;
  std::vector<Render::GL::DrawPartCmd> out_b;
  Render::GL::PoseTemplateCache::expand_to_world(*tpl, unit_a, out_a);
  Render::GL::PoseTemplateCache::expand_to_world(*tpl, unit_b, out_b);

  EXPECT_EQ(cache.size(), initial_size);
  EXPECT_EQ(cache.stats().inserts, 1U);
  ASSERT_EQ(out_a.size(), 3U);
  ASSERT_EQ(out_b.size(), 3U);

  // First part's local world is identity * translate(0,0,0) → origin.
  // After unit_a's translation, the part's world-origin column must match.
  QVector3D const a_origin =
      out_a[0].world.map(QVector3D(0.0F, 0.0F, 0.0F));
  QVector3D const b_origin =
      out_b[0].world.map(QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_FLOAT_EQ(a_origin.x(), 10.0F);
  EXPECT_FLOAT_EQ(b_origin.x(), -5.0F);
  EXPECT_FLOAT_EQ(b_origin.z(), 12.0F);

  // Second part was locally at (1,0,0) — after world translate (10,0,0) it
  // must end up at (11, 0, 0).
  QVector3D const a_second =
      out_a[1].world.map(QVector3D(0.0F, 0.0F, 0.0F));
  EXPECT_FLOAT_EQ(a_second.x(), 11.0F);
}

TEST(PoseTemplateCache, EvictOlderThanSkipsFreshEntries) {
  Render::GL::PoseTemplateCache cache;

  Render::GL::PoseKey key_old{1, Render::GL::AnimationState::Idle,
                              Render::GL::PoseStance::Neutral, 0};
  Render::GL::PoseKey key_fresh{2, Render::GL::AnimationState::Idle,
                                Render::GL::PoseStance::Neutral, 0};

  cache.begin_frame(1);
  cache.insert(key_old, make_local_template(1));

  cache.begin_frame(100);
  cache.insert(key_fresh, make_local_template(1));

  // max age = 10 → old (frame 1) should be evicted, fresh (frame 100)
  // retained.
  cache.evict_older_than(10);

  EXPECT_EQ(cache.find(key_old), nullptr);
  EXPECT_NE(cache.find(key_fresh), nullptr);
  EXPECT_GE(cache.stats().evictions, 1U);
}

TEST(PoseTemplateCache, EvictNoopWhenFrameBelowMaxAge) {
  // Guards the early-exit path: m_current_frame <= max_age_frames cannot
  // underflow, even though frame indices are unsigned.
  Render::GL::PoseTemplateCache cache;
  cache.begin_frame(3);

  Render::GL::PoseKey key{1, Render::GL::AnimationState::Idle,
                          Render::GL::PoseStance::Neutral, 0};
  cache.insert(key, make_local_template(1));

  cache.evict_older_than(100); // way larger than current frame
  EXPECT_NE(cache.find(key), nullptr);
  EXPECT_EQ(cache.stats().evictions, 0U);
}
