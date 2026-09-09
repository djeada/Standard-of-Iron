#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "render/gl/gl_resource_tracking.h"
#include "render/profiling/asset_counters.h"

namespace {

auto find_repo_root() -> std::filesystem::path {
  auto has_repo_markers = [](const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt") &&
           std::filesystem::exists(path / "render" / "gl" / "buffer.cpp") &&
           std::filesystem::exists(path / "render" / "gl" / "gl_resource_tracking.h");
  };

  auto walk_up = [&](std::filesystem::path path) -> std::filesystem::path {
    while (!path.empty()) {
      if (has_repo_markers(path)) {
        return path;
      }
      const auto parent = path.parent_path();
      if (parent == path) {
        break;
      }
      path = parent;
    }
    return {};
  };

  if (const auto from_file = walk_up(std::filesystem::path(__FILE__).parent_path());
      !from_file.empty()) {
    return from_file;
  }
  return walk_up(std::filesystem::current_path());
}

constexpr std::array k_tracked_calls{"glGenBuffers",
                                     "glCreateBuffers",
                                     "glGenVertexArrays",
                                     "glGenTextures",
                                     "glBufferData",
                                     "glNamedBufferData",
                                     "glBufferSubData",
                                     "glBufferStorage",
                                     "glMapBufferRange",
                                     "glTexImage2D",
                                     "glTexImage3D",
                                     "glTexSubImage2D",
                                     "glTexStorage2D"};

constexpr std::array k_scanned_roots{"render", "ui", "scene", "app"};

constexpr std::size_t k_note_lookahead_lines = 4;

auto is_identifier_character(char value) -> bool {
  return (std::isalnum(static_cast<unsigned char>(value)) != 0) || value == '_';
}

auto call_at(const std::string& line,
             std::size_t position,
             std::string_view name) -> bool {
  if (line.compare(position, name.size(), name) != 0) {
    return false;
  }
  if (position > 0 && is_identifier_character(line[position - 1])) {
    return false;
  }
  const std::size_t after = position + name.size();
  return after < line.size() && line[after] == '(';
}

struct Site {
  std::string file;
  std::size_t line;
  std::string call;
};

auto read_lines(const std::filesystem::path& path) -> std::vector<std::string> {
  std::ifstream stream(path);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(line);
  }
  return lines;
}

auto collect_sites(const std::filesystem::path& root,
                   std::vector<Site>& uncovered) -> std::size_t {
  std::size_t total = 0;
  for (const auto* directory : k_scanned_roots) {
    const auto base = root / directory;
    if (!std::filesystem::is_directory(base)) {
      continue;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(base)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      const auto extension = entry.path().extension().string();
      if (extension != ".cpp" && extension != ".h") {
        continue;
      }
      if (entry.path().filename() == "gl_resource_tracking.h") {
        continue;
      }
      const auto lines = read_lines(entry.path());
      for (std::size_t index = 0; index < lines.size(); ++index) {
        for (const auto* name : k_tracked_calls) {
          const std::string_view call{name};
          for (std::size_t position = 0; position + call.size() < lines[index].size();
               ++position) {
            if (!call_at(lines[index], position, call)) {
              continue;
            }
            ++total;
            std::size_t end = index;
            int depth = 0;
            bool opened = false;
            for (; end < lines.size(); ++end) {
              const std::size_t from = end == index ? position : 0U;
              for (std::size_t probe = from; probe < lines[end].size(); ++probe) {
                if (lines[end][probe] == '(') {
                  ++depth;
                  opened = true;
                } else if (lines[end][probe] == ')') {
                  --depth;
                }
              }
              if (opened && depth <= 0) {
                break;
              }
            }
            const std::size_t last =
                std::min(lines.size(), end + k_note_lookahead_lines + 1);
            bool covered = false;
            for (std::size_t probe = index; probe < last; ++probe) {
              if (lines[probe].find("note_") != std::string::npos) {
                covered = true;
                break;
              }
            }
            if (!covered) {
              uncovered.push_back(
                  {std::filesystem::relative(entry.path(), root).string(),
                   index + 1,
                   std::string{call}});
            }
          }
        }
      }
    }
  }
  return total;
}

TEST(GlCounterCoverage, EveryDirectGlResourceCallReportsToTheAssetCounters) {
  const auto root = find_repo_root();
  ASSERT_FALSE(root.empty()) << "repository root was not found";

  std::vector<Site> uncovered;
  const std::size_t total = collect_sites(root, uncovered);
  EXPECT_GT(total, 100U) << "the scan found suspiciously few GL resource calls";

  std::ostringstream report;
  for (const auto& site : uncovered) {
    report << "\n  " << site.file << ':' << site.line << ": " << site.call;
  }
  EXPECT_TRUE(uncovered.empty())
      << "direct GL resource calls without an adjacent counter note:" << report.str();
}

TEST(GlCounterCoverage, BufferStorageWithoutDataIsNotCountedAsATransfer) {
  auto& counters = Render::Profiling::asset_counters();
  counters.reset();

  Render::GL::note_buffer_storage(4096, false);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlBufferStorageBytes),
            4096U);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlBufferTransferBytes), 0U);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlUploadBytes), 0U);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlBufferOrphans), 1U);

  Render::GL::note_buffer_storage(256, true);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlBufferStorageBytes),
            4352U);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlBufferTransferBytes),
            256U);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlUploadBytes), 256U);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlBufferOrphans), 1U);

  counters.reset();
}

TEST(GlCounterCoverage, TextureAndBufferTransfersShareTheUploadAggregate) {
  auto& counters = Render::Profiling::asset_counters();
  counters.reset();

  Render::GL::note_texture_storage(Render::GL::texture_transfer_bytes(64, 64, 4), true);
  Render::GL::note_buffer_transfer(1024);
  Render::GL::note_mapped_buffer_range(2048);

  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlTextureStorageBytes),
            64U * 64U * 4U);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlTextureTransferBytes),
            64U * 64U * 4U);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlMappedBufferBytes),
            2048U);
  EXPECT_EQ(counters.total(Render::Profiling::AssetCounter::GlUploadBytes),
            (64U * 64U * 4U) + 1024U);

  counters.reset();
}

} // namespace
