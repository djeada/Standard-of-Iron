#include <QTemporaryDir>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "animation/bpat/asset_compression.h"
#include "animation/bpat/bpat_reader.h"
#include "tests/render/test_asset_paths.h"

using namespace Render::Creature::Bpat;

namespace {

auto patterned_bytes(std::size_t size) -> std::string {
  std::string bytes(size, '\0');
  for (std::size_t i = 0; i < size; ++i) {
    bytes[i] = static_cast<char>((i * 7U) % 251U);
  }
  return bytes;
}

auto size_on_disk(const std::filesystem::path& path) -> std::uintmax_t {
  return std::filesystem::file_size(path);
}

} // namespace

TEST(AssetCompression, CompressedAssetReadsBackByteForByte) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  std::filesystem::path const path = dir.filePath("asset.bin").toStdString();
  std::string const original = patterned_bytes(1U << 20U);

  std::string error;
  ASSERT_TRUE(write_compressed_asset(path, original, error)) << error;
  EXPECT_LT(size_on_disk(path), original.size() / 4U)
      << "a repetitive cache should shrink";

  std::vector<std::uint8_t> read;
  ASSERT_TRUE(read_asset_file(path.string(), read, error)) << error;
  ASSERT_EQ(read.size(), original.size());
  EXPECT_TRUE(std::equal(
      read.begin(),
      read.end(),
      original.begin(),
      original.end(),
      [](std::uint8_t a, char b) { return a == static_cast<std::uint8_t>(b); }));
}

TEST(AssetCompression, UncompressedAssetsStillLoad) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  std::filesystem::path const path = dir.filePath("raw.bin").toStdString();
  std::string const raw = "BPAT raw bytes baked before compression";
  std::ofstream(path, std::ios::binary) << raw;

  std::vector<std::uint8_t> read;
  std::string error;
  ASSERT_TRUE(read_asset_file(path.string(), read, error)) << error;
  EXPECT_EQ(std::string(read.begin(), read.end()), raw);
}

TEST(AssetCompression, TruncatedFrameIsAnErrorNotACrash) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  std::filesystem::path const path = dir.filePath("asset.bin").toStdString();
  std::string error;
  ASSERT_TRUE(write_compressed_asset(path, patterned_bytes(65536U), error)) << error;
  std::filesystem::resize_file(path, size_on_disk(path) / 2U);

  std::vector<std::uint8_t> read;
  EXPECT_FALSE(read_asset_file(path.string(), read, error));
  EXPECT_FALSE(error.empty());
  EXPECT_TRUE(read.empty());
}

TEST(AssetCompression, MissingFileReportsItsPath) {
  std::vector<std::uint8_t> read;
  std::string error;
  EXPECT_FALSE(read_asset_file("/nonexistent/asset.bpat", read, error));
  EXPECT_NE(error.find("/nonexistent/asset.bpat"), std::string::npos);
}

TEST(AssetCompression, ShippedCreatureCachesAreCompressed) {
  auto const root = TestAssets::find_creature_assets_dir("humanoid.bpat");
  ASSERT_FALSE(root.empty());
  std::ifstream in(std::filesystem::path(root) / "humanoid.bpat", std::ios::binary);
  std::vector<std::uint8_t> head(4U);
  in.read(reinterpret_cast<char*>(head.data()), 4);
  ASSERT_TRUE(in);
  EXPECT_TRUE(is_zstd_frame(head.data(), head.size()))
      << "the baker must write zstd frames; raw caches cost ~200 MB per package";

  auto const blob =
      BpatBlob::from_file((std::filesystem::path(root) / "humanoid.bpat").string());
  EXPECT_TRUE(blob.loaded()) << blob.last_error();
}
