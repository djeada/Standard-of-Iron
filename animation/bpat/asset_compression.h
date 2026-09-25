#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Render::Creature::Bpat {

// The baked creature caches (.bpat, .bpsm, .bprm) ship as zstd frames: ~240 MB
// of vertex and pose data compresses to ~27 MB and decompresses in about a
// tenth of a second. The formats themselves are unchanged; compression wraps
// the whole file, and a file that is not a zstd frame is read as it is, so
// caches baked before compression still load.

// Level the baker writes at. Higher levels only cost bake time; decompression
// speed does not depend on the level.
inline constexpr int k_asset_compression_level = 19;

// True when the bytes begin with a zstd frame header.
[[nodiscard]] auto is_zstd_frame(const std::uint8_t* data, std::size_t size) -> bool;

// Reads a baked asset, decompressing it if it is a zstd frame. On failure
// returns false and describes the problem in `error`.
auto read_asset_file(const std::string& path,
                     std::vector<std::uint8_t>& out,
                     std::string& error) -> bool;

// Decompresses a zstd frame, or copies bytes that are not one.
auto decode_asset_bytes(std::vector<std::uint8_t> bytes,
                        std::vector<std::uint8_t>& out,
                        std::string& error) -> bool;

// Writes `bytes` to `path` as one zstd frame.
auto write_compressed_asset(const std::filesystem::path& path,
                            std::string_view bytes,
                            std::string& error,
                            int level = k_asset_compression_level) -> bool;

} // namespace Render::Creature::Bpat
