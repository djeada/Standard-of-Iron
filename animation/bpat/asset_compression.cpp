#include "asset_compression.h"

#include <array>
#include <fstream>
#include <iterator>
#include <utility>
#include <zstd.h>

namespace Render::Creature::Bpat {

namespace {

// ZSTD_MAGICNUMBER, little-endian on disk.
constexpr std::array<std::uint8_t, 4> k_zstd_magic = {0x28, 0xB5, 0x2F, 0xFD};

// No baked cache comes near this; a larger declared size means a corrupt or
// hostile header, not an asset.
constexpr unsigned long long k_max_decoded_bytes = 1ULL << 31U;

} // namespace

auto is_zstd_frame(const std::uint8_t* data, std::size_t size) -> bool {
  if (data == nullptr || size < k_zstd_magic.size()) {
    return false;
  }
  for (std::size_t i = 0; i < k_zstd_magic.size(); ++i) {
    if (data[i] != k_zstd_magic[i]) {
      return false;
    }
  }
  return true;
}

auto decode_asset_bytes(std::vector<std::uint8_t> bytes,
                        std::vector<std::uint8_t>& out,
                        std::string& error) -> bool {
  if (!is_zstd_frame(bytes.data(), bytes.size())) {
    out = std::move(bytes);
    return true;
  }

  unsigned long long const decoded_size =
      ZSTD_getFrameContentSize(bytes.data(), bytes.size());
  if (decoded_size == ZSTD_CONTENTSIZE_ERROR) {
    error = "corrupt zstd frame header";
    return false;
  }
  if (decoded_size == ZSTD_CONTENTSIZE_UNKNOWN) {
    error = "zstd frame without a content size";
    return false;
  }
  if (decoded_size > k_max_decoded_bytes) {
    error = "zstd frame declares an implausible size";
    return false;
  }

  out.resize(static_cast<std::size_t>(decoded_size));
  std::size_t const written =
      ZSTD_decompress(out.data(), out.size(), bytes.data(), bytes.size());
  if (ZSTD_isError(written) != 0U) {
    error = std::string("zstd decompression failed: ") + ZSTD_getErrorName(written);
    out.clear();
    return false;
  }
  if (written != out.size()) {
    error = "zstd frame decoded to fewer bytes than it declared";
    out.clear();
    return false;
  }
  return true;
}

auto read_asset_file(const std::string& path,
                     std::vector<std::uint8_t>& out,
                     std::string& error) -> bool {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    error = "failed to open " + path;
    return false;
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
  if (!decode_asset_bytes(std::move(bytes), out, error)) {
    error = path + ": " + error;
    return false;
  }
  return true;
}

auto write_compressed_asset(const std::filesystem::path& path,
                            std::string_view bytes,
                            std::string& error,
                            int level) -> bool {
  std::vector<char> frame(ZSTD_compressBound(bytes.size()));
  std::size_t const size =
      ZSTD_compress(frame.data(), frame.size(), bytes.data(), bytes.size(), level);
  if (ZSTD_isError(size) != 0U) {
    error = std::string("zstd compression failed: ") + ZSTD_getErrorName(size);
    return false;
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "cannot open " + path.string() + " for writing";
    return false;
  }
  out.write(frame.data(), static_cast<std::streamsize>(size));
  out.flush();
  if (!out) {
    error = "write failed for " + path.string();
    return false;
  }
  return true;
}

} // namespace Render::Creature::Bpat
