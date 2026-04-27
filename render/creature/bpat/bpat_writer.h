#pragma once

#include "bpat_format.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Render::Creature::Bpat {

struct ClipDescriptor {
  std::string name;
  std::uint32_t frame_count{0U};
  float fps{30.0F};
  bool loops{true};
};

struct SocketDescriptor {
  std::string name;
  std::uint32_t anchor_bone{0U};
  QVector3D local_offset{};
};

class BpatWriter {
public:
  BpatWriter(std::uint32_t species_id, std::uint32_t bone_count) noexcept;

  void add_clip(ClipDescriptor desc);
  void add_socket(SocketDescriptor desc);

  void append_clip_palettes(std::span<const QMatrix4x4> palette_frames);

  void append_clip_socket_transforms(std::span<const QMatrix4x4> socket_frames);

  [[nodiscard]] auto write(std::ostream &out) const -> bool;

  [[nodiscard]] auto species_id() const noexcept -> std::uint32_t {
    return m_species_id;
  }
  [[nodiscard]] auto bone_count() const noexcept -> std::uint32_t {
    return m_bone_count;
  }
  [[nodiscard]] auto frame_total() const noexcept -> std::uint32_t {
    return m_frame_total;
  }

private:
  struct PendingClip {
    ClipDescriptor desc;
    std::uint32_t frame_offset{0U};
    bool palette_appended{false};
    bool sockets_appended{false};
  };

  std::uint32_t m_species_id;
  std::uint32_t m_bone_count;
  std::uint32_t m_frame_total{0U};
  std::vector<PendingClip> m_clips{};
  std::vector<SocketDescriptor> m_sockets{};
  std::vector<float> m_palette_floats{};
  std::vector<float> m_socket_floats{};
};

} // namespace Render::Creature::Bpat
