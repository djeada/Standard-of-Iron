#include "render/gl/texture_table.h"

#include <cstring>

#include "render/geom/texture_image.h"
#include "render/gl/texture_upload.h"

namespace Render::GL {

namespace {

auto storage_format(TextureImage::Format format) -> Texture::Format {
  switch (format) {
  case TextureImage::Format::Rgba8:
    return Texture::Format::RGBA;
  case TextureImage::Format::R32F:
    return Texture::Format::R32F;
  case TextureImage::Format::Rgba16F:
    return Texture::Format::RGBA16F;
  }
  return Texture::Format::RGBA;
}

auto storage_filter(TextureImage::Filter filter) -> Texture::Filter {
  return filter == TextureImage::Filter::Nearest ? Texture::Filter::Nearest
                                                 : Texture::Filter::Linear;
}

auto storage_wrap(TextureImage::Wrap wrap) -> Texture::Wrap {
  return wrap == TextureImage::Wrap::Repeat ? Texture::Wrap::Repeat
                                            : Texture::Wrap::ClampToEdge;
}

} // namespace

TextureTable::TextureTable() = default;
TextureTable::~TextureTable() = default;

auto TextureTable::resolve(const TextureImage* image) -> Texture* {
  if (image == nullptr || image->empty()) {
    return nullptr;
  }
  if (m_last == nullptr || m_last_id != image->id()) {
    m_last = &m_entries[image->id()];
    m_last_id = image->id();
  }
  Entry& entry = *m_last;
  if (entry.texture == nullptr || entry.storage_version != image->storage_version()) {
    entry.lifetime = image->lifetime();
    entry.storage_version = image->storage_version();
    entry.version = 0;
    entry.texture = std::make_unique<Texture>();
    if (!entry.texture->create_empty(
            image->width(), image->height(), storage_format(image->format()))) {
      entry.texture.reset();
      return nullptr;
    }
    entry.texture->set_filter(storage_filter(image->filter()),
                              storage_filter(image->filter()));
    entry.texture->set_wrap(storage_wrap(image->wrap()), storage_wrap(image->wrap()));
  }
  if (entry.version != image->version()) {
    upload(*image, entry);
    entry.version = image->version();
  }
  return entry.texture.get();
}

void TextureTable::upload(const TextureImage& image, Entry& entry) {
  entry.texture->bind();
  switch (image.format()) {
  case TextureImage::Format::Rgba8: {
    const TextureImage::Region& changed = image.last_change();
    const bool partial =
        entry.version != 0 && entry.version + 1 == image.version() &&
        !changed.empty() &&
        (changed.width != image.width() || changed.height != image.height());
    if (!partial) {
      upload_bound_texture_rgba8(0, 0, image.width(), image.height(), image.data());
      return;
    }
    const std::size_t texel = TextureImage::bytes_per_texel(image.format());
    const std::size_t row_bytes = static_cast<std::size_t>(changed.width) * texel;
    m_region_scratch.resize(row_bytes * static_cast<std::size_t>(changed.height));
    for (int row = 0; row < changed.height; ++row) {
      const std::size_t source = ((static_cast<std::size_t>(changed.y + row) *
                                   static_cast<std::size_t>(image.width())) +
                                  static_cast<std::size_t>(changed.x)) *
                                 texel;
      std::memcpy(m_region_scratch.data() + (static_cast<std::size_t>(row) * row_bytes),
                  image.data() + source,
                  row_bytes);
    }
    upload_bound_texture_rgba8(
        changed.x, changed.y, changed.width, changed.height, m_region_scratch.data());
    return;
  }
  case TextureImage::Format::R32F:
    upload_bound_texture_float(image.width(),
                               image.height(),
                               FloatTexelLayout::Red,
                               reinterpret_cast<const float*>(image.data()));
    return;
  case TextureImage::Format::Rgba16F:
    upload_bound_texture_float(image.width(),
                               image.height(),
                               FloatTexelLayout::Rgba,
                               reinterpret_cast<const float*>(image.data()));
    return;
  }
}

void TextureTable::collect_retired() {
  m_last = nullptr;
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    if (it->second.lifetime.expired()) {
      it = m_entries.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace Render::GL
