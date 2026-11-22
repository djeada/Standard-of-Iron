#include "render/geom/texture_image.h"

#include <algorithm>
#include <atomic>
#include <cstring>

namespace Render::GL {

namespace {

auto next_texture_image_id() -> std::uint64_t {
  static std::atomic<std::uint64_t> next{1};
  return next.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

TextureImage::TextureImage()
    : m_id(next_texture_image_id())
    , m_lifetime(std::make_shared<const int>(0)) {
}

TextureImage::~TextureImage() = default;

auto TextureImage::bytes_per_texel(Format format) noexcept -> std::size_t {
  switch (format) {
  case Format::Rgba8:
    return 4U;
  case Format::R32F:
    return sizeof(float);
  case Format::Rgba16F:
    return 4U * sizeof(float);
  }
  return 4U;
}

void TextureImage::reset(
    int width, int height, Format format, Filter filter, Wrap wrap) {
  m_width = std::max(width, 0);
  m_height = std::max(height, 0);
  m_format = format;
  m_filter = filter;
  m_wrap = wrap;
  m_texels.assign(static_cast<std::size_t>(m_width) *
                      static_cast<std::size_t>(m_height) * bytes_per_texel(format),
                  std::byte{0});
  m_last_change = Region{0, 0, m_width, m_height};
  ++m_storage_version;
  ++m_version;
}

void TextureImage::assign(const void* texels) {
  if (!m_texels.empty() && texels != nullptr) {
    std::memcpy(m_texels.data(), texels, m_texels.size());
  }
  m_last_change = Region{0, 0, m_width, m_height};
  ++m_version;
}

void TextureImage::update_region(const Region& region, const void* texels) {
  if (region.empty() || texels == nullptr || region.x < 0 || region.y < 0 ||
      region.x + region.width > m_width || region.y + region.height > m_height) {
    return;
  }
  const std::size_t texel = bytes_per_texel(m_format);
  const std::size_t row_bytes = static_cast<std::size_t>(region.width) * texel;
  const auto* source = static_cast<const std::byte*>(texels);
  for (int row = 0; row < region.height; ++row) {
    const std::size_t offset = ((static_cast<std::size_t>(region.y + row) *
                                 static_cast<std::size_t>(m_width)) +
                                static_cast<std::size_t>(region.x)) *
                               texel;
    std::memcpy(m_texels.data() + offset,
                source + (static_cast<std::size_t>(row) * row_bytes),
                row_bytes);
  }
  m_last_change = region;
  ++m_version;
}

} // namespace Render::GL
