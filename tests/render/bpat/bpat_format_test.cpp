#include <QMatrix4x4>
#include <QVector3D>

#include <cstring>
#include <gtest/gtest.h>
#include <sstream>
#include <vector>

#include "animation/bpat/bpat_format.h"
#include "animation/bpat/bpat_playback.h"
#include "animation/bpat/bpat_reader.h"
#include "animation/bpat/bpat_writer.h"

using namespace Render::Creature::Bpat;

namespace {

QMatrix4x4 fingerprint_matrix(int seed) {
  QMatrix4x4 m;
  m.setToIdentity();
  m.translate(QVector3D(static_cast<float>(seed), 2.0F * seed, -3.0F * seed + 1.0F));
  m.rotate(static_cast<float>(seed % 90), 0.0F, 1.0F, 0.0F);
  return m;
}

std::vector<std::uint8_t> serialize(const BpatWriter& w) {
  std::stringstream ss(std::ios::out | std::ios::binary | std::ios::in);
  EXPECT_TRUE(w.write(ss));
  std::string s = ss.str();
  return std::vector<std::uint8_t>(s.begin(), s.end());
}

} // namespace

TEST(BpatFormat, HeaderAndEntrySizesAreFrozen) {
  EXPECT_EQ(sizeof(BpatHeader), 64U);
  EXPECT_EQ(sizeof(BpatHeaderExtV3), 32U);
  EXPECT_EQ(sizeof(BpatClipEntry), 48U);
  EXPECT_EQ(sizeof(BpatSocketEntry), 32U);
  EXPECT_EQ(sizeof(BpatFrameContact), 8U);
  EXPECT_EQ(k_version, 3U);
  EXPECT_EQ(k_magic[0], 'B');
  EXPECT_EQ(k_magic[3], 'T');
}

TEST(BpatWriter, RejectsEmptyClipList) {
  BpatWriter const w(k_species_humanoid, 20U);
  std::stringstream ss;
  EXPECT_FALSE(w.write(ss));
}

TEST(BpatWriter, WriteAndReadbackRoundTripsClipsAndPalettes) {
  constexpr std::uint32_t bone_count = 20U;
  BpatWriter w(k_species_humanoid, bone_count);

  ClipDescriptor const idle{"idle", 4U, 30.0F, true};
  ClipDescriptor const walk{"walk", 6U, 30.0F, true};

  w.add_clip(idle);
  std::vector<QMatrix4x4> idle_palettes(bone_count * idle.frame_count);
  for (std::size_t i = 0; i < idle_palettes.size(); ++i) {
    idle_palettes[i] = fingerprint_matrix(static_cast<int>(i));
  }
  w.append_clip_palettes(idle_palettes);

  w.add_clip(walk);
  std::vector<QMatrix4x4> walk_palettes(bone_count * walk.frame_count);
  for (std::size_t i = 0; i < walk_palettes.size(); ++i) {
    walk_palettes[i] = fingerprint_matrix(static_cast<int>(i + 1000));
  }
  w.append_clip_palettes(walk_palettes);

  auto bytes = serialize(w);
  ASSERT_GT(bytes.size(), sizeof(BpatHeader));

  auto blob = BpatBlob::from_bytes(std::move(bytes));
  ASSERT_TRUE(blob.loaded()) << blob.last_error();
  EXPECT_EQ(blob.species_id(), k_species_humanoid);
  EXPECT_EQ(blob.bone_count(), bone_count);
  EXPECT_EQ(blob.clip_count(), 2U);
  EXPECT_EQ(blob.frame_total(), idle.frame_count + walk.frame_count);

  auto idle_view = blob.clip(0);
  EXPECT_EQ(idle_view.name, "idle");
  EXPECT_EQ(idle_view.frame_count, idle.frame_count);
  EXPECT_EQ(idle_view.frame_offset, 0U);
  EXPECT_EQ(idle_view.fps, 30.0F);
  EXPECT_TRUE(idle_view.loops);

  auto walk_view = blob.clip(1);
  EXPECT_EQ(walk_view.name, "walk");
  EXPECT_EQ(walk_view.frame_offset, idle.frame_count);

  for (std::uint32_t f = 0; f < idle.frame_count; ++f) {
    for (std::uint32_t b = 0; b < bone_count; ++b) {
      auto span = blob.palette_matrix(f, b);
      ASSERT_EQ(span.size(), 16U);
      auto const& expected = idle_palettes[(f * bone_count) + b];
      for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
          EXPECT_FLOAT_EQ(span[(row * 4) + col], expected(row, col))
              << "frame " << f << " bone " << b << " r" << row << " c" << col;
        }
      }
    }
  }
}

TEST(BpatWriter, ClipFlagsAndVariantTableRoundTrip) {
  constexpr std::uint32_t bone_count = 2U;
  BpatWriter w(k_species_humanoid, bone_count);
  struct Clip {
    const char* name;
    std::uint16_t family;
    std::uint8_t ordinal;
  };
  for (Clip const& clip : {Clip{"idle", 0U, 0U},
                           Clip{"idle_squat", 0U, 1U},
                           Clip{"walk", 2U, 0U},
                           Clip{"riding_idle", 3U, 0U},
                           Clip{"showcase_jump", 4U, 0U}}) {
    ClipDescriptor desc{clip.name, 1U, 30.0F, true};
    desc.variant_family = clip.family;
    desc.variant_ordinal = clip.ordinal;
    w.add_clip(desc);
    std::vector<QMatrix4x4> const palettes(bone_count);
    w.append_clip_palettes(palettes);
  }

  auto blob = BpatBlob::from_bytes(serialize(w));
  ASSERT_TRUE(blob.loaded()) << blob.last_error();
  EXPECT_TRUE(blob.clip_supplies_ground_contact(0));
  EXPECT_TRUE(blob.clip_supplies_ground_contact(2));
  EXPECT_FALSE(blob.clip_supplies_ground_contact(3));
  EXPECT_FALSE(blob.clip_supplies_ground_contact(4));
  EXPECT_EQ(blob.clip_variant_family(1), 0U);
  EXPECT_EQ(blob.clip_variant_ordinal(1), 1U);
  EXPECT_TRUE(blob.clip_is_variant_of(0, 1, 1U));
  EXPECT_FALSE(blob.clip_is_variant_of(0, 2, 2U));
  EXPECT_FALSE(blob.clip_is_variant_of(0, 1, 2U));
  EXPECT_TRUE(blob.frame_contacts().empty());
}

TEST(BpatWriter, ContactTableRoundTripsPerGlobalFrame) {
  constexpr std::uint32_t bone_count = 2U;
  BpatWriter w(k_species_humanoid, bone_count);

  ClipDescriptor const idle{"idle", 3U, 30.0F, true};
  w.add_clip(idle);
  std::vector<QMatrix4x4> const idle_palettes(bone_count * idle.frame_count);
  w.append_clip_palettes(idle_palettes);
  std::vector<BpatFrameContact> const idle_contacts{
      {0.0F, 0.02F}, {-0.01F, 0.03F}, {0.015F, 0.025F}};
  w.append_clip_contacts(idle_contacts);

  ClipDescriptor const walk{"walk", 2U, 30.0F, true};
  w.add_clip(walk);
  std::vector<QMatrix4x4> const walk_palettes(bone_count * walk.frame_count);
  w.append_clip_palettes(walk_palettes);
  std::vector<BpatFrameContact> const walk_contacts{{0.1F, 0.2F}, {-0.2F, 0.4F}};
  w.append_clip_contacts(walk_contacts);

  auto blob = BpatBlob::from_bytes(serialize(w));
  ASSERT_TRUE(blob.loaded()) << blob.last_error();
  auto const contacts = blob.frame_contacts();
  ASSERT_EQ(contacts.size(), 5U);
  EXPECT_FLOAT_EQ(contacts[1].sole_y, -0.01F);
  EXPECT_FLOAT_EQ(contacts[1].foot_y, 0.03F);
  EXPECT_FLOAT_EQ(contacts[3].sole_y, 0.1F);
  EXPECT_FLOAT_EQ(contacts[4].foot_y, 0.4F);
}

TEST(BpatReader, ClipIndexFindsNamesAndSurvivesBlobMove) {
  constexpr std::uint32_t bone_count = 2U;
  BpatWriter w(k_species_humanoid, bone_count);

  for (ClipDescriptor const& clip : {ClipDescriptor{"idle", 1U, 30.0F, true},
                                     ClipDescriptor{"walk", 1U, 30.0F, true},
                                     ClipDescriptor{"idle", 1U, 24.0F, false}}) {
    w.add_clip(clip);
    std::vector<QMatrix4x4> palettes(bone_count * clip.frame_count);
    for (auto& m : palettes) {
      m.setToIdentity();
    }
    w.append_clip_palettes(palettes);
  }

  BpatBlob blob = BpatBlob::from_bytes(serialize(w));
  ASSERT_TRUE(blob.loaded()) << blob.last_error();

  BpatBlob const moved = std::move(blob);
  EXPECT_EQ(moved.clip_index("idle"), 0U);
  EXPECT_EQ(moved.clip_index("walk"), 1U);
  EXPECT_EQ(moved.clip_index("missing"), BpatBlob::k_invalid_clip_index);
}

TEST(BpatWriter, BakedSocketsRoundTrip) {
  constexpr std::uint32_t bone_count = 20U;
  BpatWriter w(k_species_humanoid, bone_count);
  w.add_socket({"hand_r", 13U, QVector3D(0.1F, -0.2F, 0.0F)});
  w.add_socket({"head", 5U, QVector3D(0.0F, 0.05F, 0.0F)});

  ClipDescriptor const c{"idle", 2U, 30.0F, true};
  w.add_clip(c);

  std::vector<QMatrix4x4> palettes(bone_count * c.frame_count);
  for (auto& m : palettes) {
    m.setToIdentity();
  }
  w.append_clip_palettes(palettes);

  std::vector<QMatrix4x4> sockets(2U * c.frame_count);
  for (std::size_t i = 0; i < sockets.size(); ++i) {
    sockets[i] = fingerprint_matrix(static_cast<int>(i + 7));
  }
  w.append_clip_socket_transforms(sockets);

  auto bytes = serialize(w);
  auto blob = BpatBlob::from_bytes(std::move(bytes));
  ASSERT_TRUE(blob.loaded()) << blob.last_error();
  ASSERT_EQ(blob.socket_count(), 2U);

  auto s0 = blob.socket(0);
  EXPECT_EQ(s0.name, "hand_r");
  EXPECT_EQ(s0.anchor_bone, 13U);
  EXPECT_FLOAT_EQ(s0.local_offset[0], 0.1F);

  auto sm = blob.socket_matrix(0U, 0U);
  ASSERT_EQ(sm.size(), 12U);
  auto const& expected = sockets[0];
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_FLOAT_EQ(sm[(row * 4) + col], expected(row, col));
    }
  }
}

TEST(BpatWriter, AuthoredClipMarkersRoundTrip) {
  constexpr std::uint32_t bone_count = 4U;
  BpatWriter w(k_species_humanoid, bone_count);

  ClipDescriptor attack{"attack_sword_a", 2U, 30.0F, false};
  attack.marker_anticipation_start = 0.10F;
  attack.marker_weapon_release = 0.54F;
  attack.marker_contact = 0.58F;
  attack.marker_recover_unlocked = 0.72F;
  attack.marker_exit_safe = 0.90F;
  w.add_clip(attack);
  std::vector<QMatrix4x4> attack_palettes(bone_count * attack.frame_count);
  for (auto& m : attack_palettes) {
    m.setToIdentity();
  }
  w.append_clip_palettes(attack_palettes);

  ClipDescriptor const idle{"idle", 2U, 30.0F, true};
  w.add_clip(idle);
  std::vector<QMatrix4x4> idle_palettes(bone_count * idle.frame_count);
  for (auto& m : idle_palettes) {
    m.setToIdentity();
  }
  w.append_clip_palettes(idle_palettes);

  auto blob = BpatBlob::from_bytes(serialize(w));
  ASSERT_TRUE(blob.loaded()) << blob.last_error();

  auto const attack_view = blob.clip(0);
  EXPECT_FLOAT_EQ(attack_view.marker_anticipation_start, 0.10F);
  EXPECT_FLOAT_EQ(attack_view.marker_weapon_release, 0.54F);
  EXPECT_FLOAT_EQ(attack_view.marker_contact, 0.58F);
  EXPECT_FLOAT_EQ(attack_view.marker_recover_unlocked, 0.72F);
  EXPECT_FLOAT_EQ(attack_view.marker_exit_safe, 0.90F);

  auto const idle_view = blob.clip(1);
  EXPECT_FLOAT_EQ(idle_view.marker_anticipation_start, -1.0F);
  EXPECT_FLOAT_EQ(idle_view.marker_contact, -1.0F);
}

TEST(BpatReader, RejectsBadMagic) {
  std::vector<std::uint8_t> bytes(sizeof(BpatHeader), 0U);
  bytes[0] = 'X';
  auto blob = BpatBlob::from_bytes(std::move(bytes));
  EXPECT_FALSE(blob.loaded());
}

TEST(BpatReader, RejectsTruncatedFile) {
  BpatWriter w(k_species_humanoid, 20U);
  ClipDescriptor const c{"idle", 1U, 30.0F, true};
  w.add_clip(c);
  std::vector<QMatrix4x4> palettes(20U);
  for (auto& m : palettes) {
    m.setToIdentity();
  }
  w.append_clip_palettes(palettes);
  auto bytes = serialize(w);
  bytes.resize(bytes.size() / 2U);
  auto blob = BpatBlob::from_bytes(std::move(bytes));
  EXPECT_FALSE(blob.loaded());
}

TEST(BpatReader, RejectsBadVersion) {
  BpatWriter w(k_species_humanoid, 20U);
  ClipDescriptor const c{"idle", 1U, 30.0F, true};
  w.add_clip(c);
  std::vector<QMatrix4x4> palettes(20U);
  for (auto& m : palettes) {
    m.setToIdentity();
  }
  w.append_clip_palettes(palettes);
  auto bytes = serialize(w);

  bytes[4] = 99U;
  auto blob = BpatBlob::from_bytes(std::move(bytes));
  EXPECT_FALSE(blob.loaded());
}

TEST(BpatPlayback, ResolvesAdjacentFrameAndLerpForSmoothRiggedPlayback) {
  BpatWriter w(k_species_humanoid, 1U);
  ClipDescriptor const looping{"idle", 4U, 30.0F, true};
  w.add_clip(looping);
  std::vector<QMatrix4x4> palettes(looping.frame_count);
  for (auto& m : palettes) {
    m.setToIdentity();
  }
  w.append_clip_palettes(palettes);

  auto bytes = serialize(w);
  auto blob = BpatBlob::from_bytes(std::move(bytes));
  ASSERT_TRUE(blob.loaded()) << blob.last_error();

  auto const playback =
      Render::Creature::Pipeline::resolve_bpat_playback(&blob, 0U, 0.375F);
  ASSERT_TRUE(playback.valid());
  EXPECT_EQ(playback.frame_in_clip, 1U);
  EXPECT_EQ(playback.next_frame_in_clip, 2U);
  EXPECT_FLOAT_EQ(playback.frame_lerp, 0.5F);

  auto const wrap = Render::Creature::Pipeline::resolve_bpat_playback(&blob, 0U, 0.95F);
  ASSERT_TRUE(wrap.valid());
  EXPECT_EQ(wrap.frame_in_clip, 3U);
  EXPECT_EQ(wrap.next_frame_in_clip, 0U);
  EXPECT_GT(wrap.frame_lerp, 0.0F);
}
