#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "render/gl/texture.h"

namespace Render::GL {

class TextureImage;

class TextureTable {
public:
  TextureTable();
  ~TextureTable();

  TextureTable(const TextureTable&) = delete;
  auto operator=(const TextureTable&) -> TextureTable& = delete;
  TextureTable(TextureTable&&) = delete;
  auto operator=(TextureTable&&) -> TextureTable& = delete;

  auto resolve(const TextureImage* image) -> Texture*;

  void collect_retired();

  [[nodiscard]] auto size() const noexcept -> std::size_t { return m_entries.size(); }

private:
  struct Entry {
    std::weak_ptr<const void> lifetime;
    std::unique_ptr<Texture> texture;
    std::uint64_t storage_version = 0;
    std::uint64_t version = 0;
  };

  void upload(const TextureImage& image, Entry& entry);

  std::unordered_map<std::uint64_t, Entry> m_entries;

  std::uint64_t m_last_id = 0;
  Entry* m_last = nullptr;
  std::vector<std::byte> m_region_scratch;
};

} // namespace Render::GL
