#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Render::MeshPack {

inline constexpr std::uint32_t k_version = 1U;

struct Range {
  std::uint32_t first_index{0};
  std::uint32_t index_count{0};
};

template <typename Vertex>
struct Entry {
  static_assert(std::is_trivially_copyable_v<Vertex>);
  std::string name;
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<Range> ranges;
  std::array<float, 4> bounds{};
};

namespace Detail {

struct EntryHeader {
  std::uint32_t name_bytes;
  std::uint32_t vertex_count;
  std::uint32_t index_count;
  std::uint32_t range_count;
  std::array<float, 4> bounds;
};

template <typename T>
void append(std::string& out, const T& value) {
  out.append(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
void append(std::string& out, const std::vector<T>& values) {
  out.append(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(T));
}

class Reader {
public:
  Reader(const std::uint8_t* data, std::size_t size)
      : m_data(data)
      , m_size(size) {}

  template <typename T>
  auto read(T& value) -> bool {
    return read_bytes(&value, sizeof(T));
  }

  template <typename T>
  auto read(std::vector<T>& values, std::size_t count) -> bool {
    if (count > (m_size - m_offset) / sizeof(T)) {
      return false;
    }
    values.resize(count);
    return read_bytes(values.data(), count * sizeof(T));
  }

  auto read(std::string& value, std::size_t count) -> bool {
    if (count > m_size - m_offset) {
      return false;
    }
    value.assign(reinterpret_cast<const char*>(m_data + m_offset), count);
    m_offset += count;
    return true;
  }

private:
  auto read_bytes(void* out, std::size_t count) -> bool {
    if (count > m_size - m_offset) {
      return false;
    }
    std::memcpy(out, m_data + m_offset, count);
    m_offset += count;
    return true;
  }

  const std::uint8_t* m_data;
  std::size_t m_size;
  std::size_t m_offset{0};
};

} // namespace Detail

template <typename Vertex>
[[nodiscard]] auto serialize(std::uint32_t magic,
                             const std::vector<Entry<Vertex>>& entries) -> std::string {
  std::string out;
  Detail::append(out, magic);
  Detail::append(out, k_version);
  Detail::append(out, static_cast<std::uint32_t>(sizeof(Vertex)));
  Detail::append(out, static_cast<std::uint32_t>(entries.size()));
  for (const Entry<Vertex>& entry : entries) {
    Detail::append(
        out,
        Detail::EntryHeader{
            .name_bytes = static_cast<std::uint32_t>(entry.name.size()),
            .vertex_count = static_cast<std::uint32_t>(entry.vertices.size()),
            .index_count = static_cast<std::uint32_t>(entry.indices.size()),
            .range_count = static_cast<std::uint32_t>(entry.ranges.size()),
            .bounds = entry.bounds,
        });
    out.append(entry.name);
    Detail::append(out, entry.vertices);
    Detail::append(out, entry.indices);
    Detail::append(out, entry.ranges);
  }
  return out;
}

template <typename Vertex>
auto parse(std::uint32_t magic,
           const std::uint8_t* data,
           std::size_t size,
           std::vector<Entry<Vertex>>& out,
           std::string& error) -> bool {
  Detail::Reader reader(data, size);
  std::uint32_t read_magic = 0;
  std::uint32_t version = 0;
  std::uint32_t vertex_size = 0;
  std::uint32_t count = 0;
  if (!reader.read(read_magic) || read_magic != magic) {
    error = "not a mesh pack of this family";
    return false;
  }
  if (!reader.read(version) || version != k_version || !reader.read(vertex_size) ||
      vertex_size != sizeof(Vertex) || !reader.read(count)) {
    error = "mesh pack version or vertex layout does not match this build";
    return false;
  }
  out.clear();
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    Detail::EntryHeader header{};
    Entry<Vertex> entry;
    if (!reader.read(header) || !reader.read(entry.name, header.name_bytes) ||
        !reader.read(entry.vertices, header.vertex_count) ||
        !reader.read(entry.indices, header.index_count) ||
        !reader.read(entry.ranges, header.range_count)) {
      error = "truncated mesh pack entry";
      return false;
    }
    for (std::uint32_t const index : entry.indices) {
      if (index >= header.vertex_count) {
        error = "mesh pack index out of range in " + entry.name;
        return false;
      }
    }
    for (const Range& range : entry.ranges) {
      if (range.first_index + range.index_count > header.index_count) {
        error = "mesh pack range out of range in " + entry.name;
        return false;
      }
    }
    entry.bounds = header.bounds;
    out.push_back(std::move(entry));
  }
  return true;
}

} // namespace Render::MeshPack
