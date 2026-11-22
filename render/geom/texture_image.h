#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {

// A 2D image a draw command samples: terrain heights and fields, the ground
// cover mask, the visibility and fog masks. It holds texels only, no GL
// object. A domain renderer owns and fills it; each backend keeps its own
// texture for it in a TextureTable, keyed by id(), uploads again whenever
// version() moves (only the changed region when it can), and drops the
// texture once the image is destroyed.
class TextureImage {
public:
  enum class Format : std::uint8_t {
    Rgba8,  // 4 x uint8 per texel
    R32F,   // 1 x float per texel
    Rgba16F // 4 x float per texel, stored at half precision on the GPU
  };
  enum class Filter : std::uint8_t {
    Nearest,
    Linear
  };
  enum class Wrap : std::uint8_t {
    ClampToEdge,
    Repeat
  };

  struct Region {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    [[nodiscard]] auto empty() const noexcept -> bool {
      return width <= 0 || height <= 0;
    }
  };

  TextureImage();
  ~TextureImage();

  TextureImage(const TextureImage&) = delete;
  auto operator=(const TextureImage&) -> TextureImage& = delete;
  TextureImage(TextureImage&&) = delete;
  auto operator=(TextureImage&&) -> TextureImage& = delete;

  // Sets the size and format and zeroes the texels. Backends make new storage.
  void reset(int width,
             int height,
             Format format,
             Filter filter = Filter::Linear,
             Wrap wrap = Wrap::ClampToEdge);

  // Replaces every texel; `texels` holds width x height texels of the format.
  void assign(const void* texels);
  // Replaces the texels of `region`; `texels` holds region.width x
  // region.height texels, packed row by row.
  void update_region(const Region& region, const void* texels);

  [[nodiscard]] auto width() const noexcept -> int { return m_width; }
  [[nodiscard]] auto height() const noexcept -> int { return m_height; }
  [[nodiscard]] auto format() const noexcept -> Format { return m_format; }
  [[nodiscard]] auto filter() const noexcept -> Filter { return m_filter; }
  [[nodiscard]] auto wrap() const noexcept -> Wrap { return m_wrap; }
  [[nodiscard]] auto data() const noexcept -> const std::byte* {
    return m_texels.data();
  }
  [[nodiscard]] auto empty() const noexcept -> bool {
    return m_width <= 0 || m_height <= 0;
  }
  [[nodiscard]] static auto bytes_per_texel(Format format) noexcept -> std::size_t;

  [[nodiscard]] auto id() const noexcept -> std::uint64_t { return m_id; }
  // Moves on every change of texels or storage.
  [[nodiscard]] auto version() const noexcept -> std::uint64_t { return m_version; }
  // Moves when the size, format, filter or wrap change.
  [[nodiscard]] auto storage_version() const noexcept -> std::uint64_t {
    return m_storage_version;
  }
  // What the latest version() changed; the whole image after reset() or
  // assign().
  [[nodiscard]] auto last_change() const noexcept -> const Region& {
    return m_last_change;
  }
  [[nodiscard]] auto lifetime() const noexcept -> std::weak_ptr<const void> {
    return m_lifetime;
  }

private:
  std::vector<std::byte> m_texels;
  int m_width = 0;
  int m_height = 0;
  Format m_format = Format::Rgba8;
  Filter m_filter = Filter::Linear;
  Wrap m_wrap = Wrap::ClampToEdge;
  Region m_last_change;
  std::uint64_t m_id;
  std::uint64_t m_version = 0;
  std::uint64_t m_storage_version = 0;
  std::shared_ptr<const int> m_lifetime;
};

} // namespace Render::GL
