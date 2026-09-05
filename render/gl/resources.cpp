#include "resources.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "gl/mesh.h"
#include "gl/texture.h"
#include "platform_gl.h"
#include "render_constants.h"

namespace Render::GL {

using namespace Render::GL::Geometry;
using namespace Render::GL::RGBA;

namespace {

constexpr int k_material_detail_size = 256;

constexpr int k_wear_volume_size = 32;

constexpr int k_material_detail_cells[4] = {4, 16, 64, 128};

auto detail_hash(int x, int y, int band) -> float {
  auto mix32 = [](std::uint32_t value) {
    value ^= value >> HashXorShift::k_xor_shift_amount_17;
    value *= 0xED5AD4BBU;
    value ^= value >> 11U;
    value *= 0xAC4C1B51U;
    value ^= value >> HashXorShift::k_xor_shift_amount_13;
    return value;
  };
  const auto seed = static_cast<std::uint32_t>(x) * 0x9E3779B1U +
                    static_cast<std::uint32_t>(y) * 0x85EBCA77U +
                    static_cast<std::uint32_t>(band) * 0xC2B2AE3DU;
  return static_cast<float>(mix32(seed) >> 8U) / 16777215.0F;
}

auto detail_band(float u, float v, int cells, int band) -> float {
  const float x = u * static_cast<float>(cells);
  const float y = v * static_cast<float>(cells);
  const auto ix = static_cast<int>(std::floor(x));
  const auto iy = static_cast<int>(std::floor(y));
  const float fx = x - static_cast<float>(ix);
  const float fy = y - static_cast<float>(iy);
  const float sx = fx * fx * (3.0F - 2.0F * fx);
  const float sy = fy * fy * (3.0F - 2.0F * fy);
  auto wrap = [cells](int value) {
    return ((value % cells) + cells) % cells;
  };
  const int x0 = wrap(ix);
  const int y0 = wrap(iy);
  const int x1 = wrap(ix + 1);
  const int y1 = wrap(iy + 1);
  const float a = detail_hash(x0, y0, band);
  const float b = detail_hash(x1, y0, band);
  const float c = detail_hash(x0, y1, band);
  const float d = detail_hash(x1, y1, band);
  const float top = a + (b - a) * sx;
  const float bottom = c + (d - c) * sx;
  return top + (bottom - top) * sy;
}

} // namespace

auto ResourceManager::initialize() -> bool {
  initializeOpenGLFunctions();

  m_quad_mesh = create_quad_mesh();
  m_ground_mesh = create_plane_mesh(1.0F, 1.0F, ground_plane_subdivisions);
  m_unit_mesh = create_cube_mesh();

  m_white_texture = std::make_unique<Texture>();
  m_white_texture->create_empty(1, 1, Texture::Format::RGBA);
  unsigned char white_pixel[4] = {max_value, max_value, max_value, max_value};
  m_white_texture->bind();
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);

  std::vector<unsigned char> detail(
      static_cast<std::size_t>(k_material_detail_size) *
          static_cast<std::size_t>(k_material_detail_size) * 4U,
      0);
  for (int y = 0; y < k_material_detail_size; ++y) {
    const float v =
        (static_cast<float>(y) + 0.5F) / static_cast<float>(k_material_detail_size);
    for (int x = 0; x < k_material_detail_size; ++x) {
      const float u =
          (static_cast<float>(x) + 0.5F) / static_cast<float>(k_material_detail_size);
      const std::size_t slot = ((static_cast<std::size_t>(y) *
                                 static_cast<std::size_t>(k_material_detail_size)) +
                                static_cast<std::size_t>(x)) *
                               4U;
      for (int band = 0; band < 4; ++band) {
        const float value = detail_band(u, v, k_material_detail_cells[band], band);
        detail[slot + static_cast<std::size_t>(band)] = static_cast<unsigned char>(
            std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
      }
    }
  }

  m_material_detail_texture = std::make_unique<Texture>();
  m_material_detail_texture->create_empty(
      k_material_detail_size, k_material_detail_size, Texture::Format::RGBA);
  m_material_detail_texture->bind();
  glTexSubImage2D(GL_TEXTURE_2D,
                  0,
                  0,
                  0,
                  k_material_detail_size,
                  k_material_detail_size,
                  GL_RGBA,
                  GL_UNSIGNED_BYTE,
                  detail.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  m_material_detail_texture->set_filter(Texture::Filter::Linear,
                                        Texture::Filter::Linear);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  m_material_detail_texture->set_wrap(Texture::Wrap::Repeat, Texture::Wrap::Repeat);
  m_material_detail_texture->unbind();

  std::vector<unsigned char> wear(static_cast<std::size_t>(k_wear_volume_size) *
                                      static_cast<std::size_t>(k_wear_volume_size) *
                                      static_cast<std::size_t>(k_wear_volume_size) * 4U,
                                  0);
  for (int z = 0; z < k_wear_volume_size; ++z) {
    for (int y = 0; y < k_wear_volume_size; ++y) {
      for (int x = 0; x < k_wear_volume_size; ++x) {
        const std::size_t slot = ((((static_cast<std::size_t>(z) *
                                     static_cast<std::size_t>(k_wear_volume_size)) +
                                    static_cast<std::size_t>(y)) *
                                   static_cast<std::size_t>(k_wear_volume_size)) +
                                  static_cast<std::size_t>(x)) *
                                 4U;
        for (int band = 0; band < 4; ++band) {
          const float value = detail_hash(x + z * k_wear_volume_size, y, band + 8);
          wear[slot + static_cast<std::size_t>(band)] = static_cast<unsigned char>(
              std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
        }
      }
    }
  }

  glGenTextures(1, &m_wear_volume);
  glBindTexture(GL_TEXTURE_3D, m_wear_volume);
  glTexImage3D(GL_TEXTURE_3D,
               0,
               GL_RGBA8,
               k_wear_volume_size,
               k_wear_volume_size,
               k_wear_volume_size,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               wear.data());

  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
  glBindTexture(GL_TEXTURE_3D, 0);
  return true;
}

} // namespace Render::GL
