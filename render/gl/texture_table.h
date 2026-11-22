#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "render/gl/texture.h"

namespace Render::GL {

class TextureImage;

// The textures one backend keeps for the TextureImages it samples. An image
// holds texels only; the first time a table resolves it, the table makes a
// texture on its own context, and it uploads again whenever the image's
// version moves: only the changed region when it saw the version before, the
// whole image otherwise. Textures of destroyed images go at collect_retired().
class TextureTable {
public:
  TextureTable();
  ~TextureTable();

  TextureTable(const TextureTable&) = delete;
  auto operator=(const TextureTable&) -> TextureTable& = delete;
  TextureTable(TextureTable&&) = delete;
  auto operator=(TextureTable&&) -> TextureTable& = delete;

  // The texture holding the image's current texels, uploaded first if the
  // image changed. Null for a null or empty image, or if the texture could
  // not be created. Uploading binds texture unit 0.
  auto resolve(const TextureImage* image) -> Texture*;

  // Releases the textures of images that no longer exist.
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
  std::vector<std::byte> m_region_scratch;
};

} // namespace Render::GL
