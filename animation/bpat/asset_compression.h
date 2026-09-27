#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Render::Creature::Bpat {

inline constexpr int k_asset_compression_level = 19;

[[nodiscard]] auto is_zstd_frame(const std::uint8_t* data, std::size_t size) -> bool;

auto read_asset_file(const std::string& path,
                     std::vector<std::uint8_t>& out,
                     std::string& error) -> bool;

auto decode_asset_bytes(std::vector<std::uint8_t> bytes,
                        std::vector<std::uint8_t>& out,
                        std::string& error) -> bool;

auto write_compressed_asset(const std::filesystem::path& path,
                            std::string_view bytes,
                            std::string& error,
                            int level = k_asset_compression_level) -> bool;

} // namespace Render::Creature::Bpat
