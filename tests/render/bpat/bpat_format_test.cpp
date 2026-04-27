#include "render/creature/bpat/bpat_format.h"
#include "render/creature/bpat/bpat_reader.h"
#include "render/creature/bpat/bpat_writer.h"

#include <gtest/gtest.h>

#include <QMatrix4x4>
#include <QVector3D>
#include <cstring>
#include <sstream>
#include <vector>

using namespace Render::Creature::Bpat;

namespace {

QMatrix4x4 fingerprint_matrix(int seed) {
  QMatrix4x4 m;
  m.setToIdentity();
  m.translate(
      QVector3D(static_cast<float>(seed), 2.0F * seed, -3.0F * seed + 1.0F));
  m.rotate(static_cast<float>(seed % 90), 0.0F, 1.0F, 0.0F);
  return m;
}

std::vector<std::uint8_t> serialize(const BpatWriter &w) {
  std::stringstream ss(std::ios::out | std::ios::binary | std::ios::in);
  EXPECT_TRUE(w.write(ss));
  std::string s = ss.str();
  return std::vector<std::uint8_t>(s.begin(), s.end());
}

} // namespace

TEST(BpatFormat, HeaderAndEntrySizesAreFrozen) {
  EXPECT_EQ(sizeof(BpatHeader), 64U);
  EXPECT_EQ(sizeof(BpatClipEntry), 32U);
  EXPECT_EQ(sizeof(BpatSocketEntry), 32U);
  EXPECT_EQ(kMagic[0], 'B');
  EXPECT_EQ(kMagic[3], 'T');
}

TEST(BpatWriter, RejectsEmptyClipList) {
  BpatWriter w(kSpeciesHumanoid, 20U);
  std::stringstream ss;
  EXPECT_FALSE(w.write(ss));
}

TEST(BpatWriter, WriteAndReadbackRoundTripsClipsAndPalettes) {
  constexpr std::uint32_t bone_count = 20U;
  BpatWriter w(kSpeciesHumanoid, bone_count);

  // Two clips of different lengths.
  ClipDescriptor idle{"idle", 4U, 30.0F, true};
  ClipDescriptor walk{"walk", 6U, 30.0F, true};

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
  EXPECT_EQ(blob.species_id(), kSpeciesHumanoid);
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

  // Spot check palette values, including row-major ordering.
  for (std::uint32_t f = 0; f < idle.frame_count; ++f) {
    for (std::uint32_t b = 0; b < bone_count; ++b) {
      auto span = blob.palette_matrix(f, b);
      ASSERT_EQ(span.size(), 16U);
      auto const &expected = idle_palettes[(f * bone_count) + b];
      for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
          EXPECT_FLOAT_EQ(span[(row * 4) + col], expected(row, col))
              << "frame " << f << " bone " << b << " r" << row << " c" << col;
        }
      }
    }
  }
}

TEST(BpatWriter, BakedSocketsRoundTrip) {
  constexpr std::uint32_t bone_count = 20U;
  BpatWriter w(kSpeciesHumanoid, bone_count);
  w.add_socket({"hand_r", 13U, QVector3D(0.1F, -0.2F, 0.0F)});
  w.add_socket({"head", 5U, QVector3D(0.0F, 0.05F, 0.0F)});

  ClipDescriptor c{"idle", 2U, 30.0F, true};
  w.add_clip(c);

  std::vector<QMatrix4x4> palettes(bone_count * c.frame_count);
  for (auto &m : palettes)
    m.setToIdentity();
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
  auto const &expected = sockets[0];
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_FLOAT_EQ(sm[(row * 4) + col], expected(row, col));
    }
  }
}

TEST(BpatReader, RejectsBadMagic) {
  std::vector<std::uint8_t> bytes(sizeof(BpatHeader), 0U);
  bytes[0] = 'X';
  auto blob = BpatBlob::from_bytes(std::move(bytes));
  EXPECT_FALSE(blob.loaded());
}

TEST(BpatReader, RejectsTruncatedFile) {
  BpatWriter w(kSpeciesHumanoid, 20U);
  ClipDescriptor c{"idle", 1U, 30.0F, true};
  w.add_clip(c);
  std::vector<QMatrix4x4> palettes(20U);
  for (auto &m : palettes)
    m.setToIdentity();
  w.append_clip_palettes(palettes);
  auto bytes = serialize(w);
  bytes.resize(bytes.size() / 2U);
  auto blob = BpatBlob::from_bytes(std::move(bytes));
  EXPECT_FALSE(blob.loaded());
}

TEST(BpatReader, RejectsBadVersion) {
  BpatWriter w(kSpeciesHumanoid, 20U);
  ClipDescriptor c{"idle", 1U, 30.0F, true};
  w.add_clip(c);
  std::vector<QMatrix4x4> palettes(20U);
  for (auto &m : palettes)
    m.setToIdentity();
  w.append_clip_palettes(palettes);
  auto bytes = serialize(w);
  // Bump version field at byte offset 4.
  bytes[4] = 99U;
  auto blob = BpatBlob::from_bytes(std::move(bytes));
  EXPECT_FALSE(blob.loaded());
}
