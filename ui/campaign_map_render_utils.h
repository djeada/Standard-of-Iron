#pragma once

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <cmath>
#include <vector>

namespace CampaignMapRender {

// ============================================================================
// Spline Utilities - Catmull-Rom spline implementation for smooth routes
// ============================================================================

/**
 * @brief Evaluates a point on a Catmull-Rom spline segment
 * @param p0, p1, p2, p3 Control points
 * @param t Parameter in range [0, 1]
 * @return Interpolated point on the spline
 */
inline auto catmull_rom(const QVector2D &p0, const QVector2D &p1,
                        const QVector2D &p2, const QVector2D &p3,
                        float t) -> QVector2D {
  const float t2 = t * t;
  const float t3 = t2 * t;

  // Catmull-Rom coefficients with tension 0.5
  const float c0 = -0.5F * t3 + t2 - 0.5F * t;
  const float c1 = 1.5F * t3 - 2.5F * t2 + 1.0F;
  const float c2 = -1.5F * t3 + 2.0F * t2 + 0.5F * t;
  const float c3 = 0.5F * t3 - 0.5F * t2;

  return p0 * c0 + p1 * c1 + p2 * c2 + p3 * c3;
}

/**
 * @brief Evaluates the tangent of a Catmull-Rom spline segment
 * @param p0, p1, p2, p3 Control points
 * @param t Parameter in range [0, 1]
 * @return Tangent vector at the point
 */
inline auto catmull_rom_tangent(const QVector2D &p0, const QVector2D &p1,
                                const QVector2D &p2, const QVector2D &p3,
                                float t) -> QVector2D {
  const float t2 = t * t;

  // Derivative of Catmull-Rom coefficients
  const float c0 = -1.5F * t2 + 2.0F * t - 0.5F;
  const float c1 = 4.5F * t2 - 5.0F * t;
  const float c2 = -4.5F * t2 + 4.0F * t + 0.5F;
  const float c3 = 1.5F * t2 - t;

  return p0 * c0 + p1 * c1 + p2 * c2 + p3 * c3;
}

/**
 * @brief Smooths a polyline using Catmull-Rom spline interpolation
 * @param input Original polyline points
 * @param samples_per_segment Number of interpolated points per segment
 * @return Smoothed polyline
 */
inline auto smooth_catmull_rom(const std::vector<QVector2D> &input,
                               int samples_per_segment = 8)
    -> std::vector<QVector2D> {
  if (input.size() < 2) {
    return input;
  }

  std::vector<QVector2D> result;
  result.reserve(input.size() * static_cast<size_t>(samples_per_segment));

  for (size_t i = 0; i + 1 < input.size(); ++i) {
    // Get four control points with clamping at endpoints
    const QVector2D &p0 = (i == 0) ? input[0] : input[i - 1];
    const QVector2D &p1 = input[i];
    const QVector2D &p2 = input[i + 1];
    const QVector2D &p3 =
        (i + 2 >= input.size()) ? input[input.size() - 1] : input[i + 2];

    for (int s = 0; s < samples_per_segment; ++s) {
      const float t = static_cast<float>(s) / static_cast<float>(samples_per_segment);
      result.push_back(catmull_rom(p0, p1, p2, p3, t));
    }
  }

  // Add final point
  result.push_back(input.back());

  return result;
}

// ============================================================================
// Stroke Mesh Generation - Triangulated ribbon with proper joins and caps
// ============================================================================

/**
 * @brief Miter join style parameters
 */
struct MiterParams {
  float max_miter_ratio = 3.0F;  ///< Maximum miter length as ratio of half-width
  float min_denom = 0.2F;        ///< Minimum denominator to avoid extreme miters
};

/**
 * @brief Stroke cap style
 */
enum class CapStyle { Flat, Round, Square };

/**
 * @brief Stroke join style
 */
enum class JoinStyle { Miter, Round, Bevel };

/**
 * @brief Configuration for stroke mesh generation
 */
struct StrokeMeshConfig {
  float width = 4.0F;
  CapStyle start_cap = CapStyle::Round;
  CapStyle end_cap = CapStyle::Round;
  JoinStyle join_style = JoinStyle::Miter;
  MiterParams miter_params;
  int cap_segments = 6;  ///< Number of segments for round caps
  int join_segments = 4; ///< Number of segments for round joins
};

/**
 * @brief Helper to compute perpendicular vector (90 degrees counterclockwise)
 */
inline auto perp_ccw(const QVector2D &v) -> QVector2D {
  return QVector2D(-v.y(), v.x());
}

/**
 * @brief Safe normalization that returns zero vector for near-zero input
 * @note Returns zero vector for degenerate inputs. Callers should handle
 *       the zero-vector case explicitly if a valid direction is required.
 *       Consider using a default direction when this function returns null.
 */
inline auto safe_normalize(const QVector2D &v, float epsilon = 1e-5F) -> QVector2D {
  const float len = v.length();
  if (len < epsilon) {
    return QVector2D(0.0F, 0.0F);
  }
  return v / len;
}

/**
 * @brief Generates a round cap at the start or end of a stroke
 * @param center Cap center point
 * @param direction Direction the stroke continues from center
 * @param half_width Half of the stroke width
 * @param segments Number of segments for the semicircle
 * @param is_start True for start cap, false for end cap
 * @return Vertices for the cap triangles (as triangle strip)
 */
inline auto generate_round_cap(const QVector2D &center,
                               const QVector2D &direction, float half_width,
                               int segments, bool is_start)
    -> std::vector<QVector2D> {
  std::vector<QVector2D> verts;
  verts.reserve(static_cast<size_t>((segments + 1) * 2));

  const QVector2D perp = perp_ccw(direction) * half_width;
  const float pi = 3.14159265358979F;

  // For start cap, go from -90 to +90 degrees relative to perpendicular
  // For end cap, go from +90 to -90 degrees (or equivalently, opposite direction)
  const float start_angle = is_start ? pi * 0.5F : -pi * 0.5F;
  const float end_angle = is_start ? -pi * 0.5F : pi * 0.5F;

  for (int i = 0; i <= segments; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments);
    const float angle = start_angle + t * (end_angle - start_angle);

    // Rotate perpendicular vector by angle
    const float cos_a = std::cos(angle);
    const float sin_a = std::sin(angle);

    // Offset from center using rotated perpendicular and scaled direction
    const QVector2D offset = perp * cos_a - direction * half_width * sin_a;

    // Add both center and edge vertex for triangle strip
    verts.push_back(center);
    verts.push_back(center + offset);
  }

  return verts;
}

/**
 * @brief Generates a square cap at the start or end of a stroke
 */
inline auto generate_square_cap(const QVector2D &center,
                                const QVector2D &direction, float half_width,
                                bool is_start) -> std::vector<QVector2D> {
  const QVector2D perp = perp_ccw(direction) * half_width;
  const QVector2D extension = direction * half_width * (is_start ? -1.0F : 1.0F);

  std::vector<QVector2D> verts;
  verts.reserve(4);

  // Add vertices for two triangles forming the square cap
  verts.push_back(center + perp);
  verts.push_back(center - perp);
  verts.push_back(center + perp + extension);
  verts.push_back(center - perp + extension);

  return verts;
}

/**
 * @brief Builds triangulated stroke mesh from polyline
 * @param points Input polyline
 * @param config Stroke configuration
 * @return Triangle strip vertices
 */
inline auto build_stroke_mesh(const std::vector<QVector2D> &points,
                              const StrokeMeshConfig &config)
    -> std::vector<QVector2D> {
  std::vector<QVector2D> result;

  if (points.size() < 2 || config.width <= 0.0F) {
    return result;
  }

  // Remove duplicate points
  std::vector<QVector2D> cleaned;
  cleaned.reserve(points.size());
  for (const auto &pt : points) {
    if (cleaned.empty()) {
      cleaned.push_back(pt);
      continue;
    }
    const QVector2D delta = pt - cleaned.back();
    if (QVector2D::dotProduct(delta, delta) > 1e-10F) {
      cleaned.push_back(pt);
    }
  }

  if (cleaned.size() < 2) {
    return result;
  }

  const float half_width = config.width * 0.5F;
  result.reserve(cleaned.size() * 2 + 32); // Extra for caps

  // Add start cap if round or square
  if (config.start_cap == CapStyle::Round) {
    const QVector2D start_dir = safe_normalize(cleaned[1] - cleaned[0]);
    auto cap_verts = generate_round_cap(cleaned[0], start_dir, half_width,
                                        config.cap_segments, true);
    result.insert(result.end(), cap_verts.begin(), cap_verts.end());
  } else if (config.start_cap == CapStyle::Square) {
    const QVector2D start_dir = safe_normalize(cleaned[1] - cleaned[0]);
    auto cap_verts = generate_square_cap(cleaned[0], start_dir, half_width, true);
    result.insert(result.end(), cap_verts.begin(), cap_verts.end());
  }

  // Generate main stroke body
  for (size_t i = 0; i < cleaned.size(); ++i) {
    QVector2D offset;

    if (i == 0) {
      // First point: use direction to next point
      const QVector2D dir = safe_normalize(cleaned[1] - cleaned[0]);
      offset = perp_ccw(dir) * half_width;
    } else if (i + 1 == cleaned.size()) {
      // Last point: use direction from previous point
      const QVector2D dir = safe_normalize(cleaned[i] - cleaned[i - 1]);
      offset = perp_ccw(dir) * half_width;
    } else {
      // Interior point: compute miter
      QVector2D dir0 = safe_normalize(cleaned[i] - cleaned[i - 1]);
      QVector2D dir1 = safe_normalize(cleaned[i + 1] - cleaned[i]);

      // Handle degenerate cases where direction is null
      if (dir0.isNull() && dir1.isNull()) {
        // Both directions are null - use default horizontal direction
        dir0 = QVector2D(1.0F, 0.0F);
        dir1 = QVector2D(1.0F, 0.0F);
      } else if (dir0.isNull()) {
        dir0 = dir1;
      } else if (dir1.isNull()) {
        dir1 = dir0;
      }

      const QVector2D n0 = perp_ccw(dir0);
      const QVector2D n1 = perp_ccw(dir1);
      QVector2D miter = safe_normalize(n0 + n1);

      if (miter.isNull()) {
        miter = n1;
      }

      // Compute miter length
      float denom = QVector2D::dotProduct(miter, n1);
      if (std::abs(denom) < config.miter_params.min_denom) {
        denom = (denom >= 0.0F) ? config.miter_params.min_denom
                                : -config.miter_params.min_denom;
      }

      float miter_len = half_width / denom;

      // Clamp miter to maximum ratio
      const float max_len = half_width * config.miter_params.max_miter_ratio;
      if (std::abs(miter_len) > max_len) {
        miter_len = (miter_len < 0.0F) ? -max_len : max_len;
      }

      offset = miter * miter_len;
    }

    result.push_back(cleaned[i] + offset);
    result.push_back(cleaned[i] - offset);
  }

  // Add end cap if round or square
  if (config.end_cap == CapStyle::Round) {
    const QVector2D end_dir =
        safe_normalize(cleaned.back() - cleaned[cleaned.size() - 2]);
    auto cap_verts = generate_round_cap(cleaned.back(), end_dir, half_width,
                                        config.cap_segments, false);
    result.insert(result.end(), cap_verts.begin(), cap_verts.end());
  } else if (config.end_cap == CapStyle::Square) {
    const QVector2D end_dir =
        safe_normalize(cleaned.back() - cleaned[cleaned.size() - 2]);
    auto cap_verts = generate_square_cap(cleaned.back(), end_dir, half_width, false);
    result.insert(result.end(), cap_verts.begin(), cap_verts.end());
  }

  return result;
}

// ============================================================================
// Multi-Pass Stroke Rendering - For inked cartography appearance
// ============================================================================

/**
 * @brief Stroke pass configuration for multi-pass rendering
 */
struct StrokePass {
  QVector4D color;
  float width_multiplier;
  float z_offset;
};

/**
 * @brief Predefined stroke passes for cartographic line styles
 */
namespace CartographicStyles {

/**
 * @brief Inked route style - dark border with golden highlight and red core
 */
inline auto get_inked_route_passes(float base_width, int age_factor = 0)
    -> std::vector<StrokePass> {
  const float fade = 1.0F - static_cast<float>(age_factor) * 0.08F;
  const float fade_clamped = std::max(0.3F, fade);

  return {
      // Outer border (darkest)
      {{0.12F * fade_clamped, 0.09F * fade_clamped, 0.07F * fade_clamped,
        0.65F * fade_clamped},
       1.3F,
       0.000F},
      // Ink border
      {{0.18F * fade_clamped, 0.14F * fade_clamped, 0.10F * fade_clamped,
        0.55F * fade_clamped},
       1.05F,
       0.001F},
      // Golden highlight
      {{0.70F * fade_clamped, 0.58F * fade_clamped, 0.32F * fade_clamped,
        0.65F * fade_clamped},
       0.8F,
       0.002F},
      // Core red
      {{0.62F * fade_clamped, 0.22F * fade_clamped, 0.18F * fade_clamped,
        0.75F * fade_clamped},
       0.6F,
       0.003F}};
}

/**
 * @brief Double-stroke coastline style - dark outer, light inner
 */
inline auto get_coastline_passes(float base_width) -> std::vector<StrokePass> {
  return {
      // Dark outer stroke
      {{0.12F, 0.10F, 0.08F, 0.95F}, 1.8F, 0.000F},
      // Medium transition
      {{0.25F, 0.22F, 0.18F, 0.85F}, 1.4F, 0.001F},
      // Light inner stroke
      {{0.55F, 0.50F, 0.42F, 0.75F}, 1.0F, 0.002F}};
}

/**
 * @brief Province border style - subtle engraved look
 */
inline auto get_border_passes(float base_width) -> std::vector<StrokePass> {
  return {
      // Shadow
      {{0.18F, 0.15F, 0.12F, 0.55F}, 1.6F, 0.000F},
      // Main border
      {{0.32F, 0.28F, 0.24F, 0.70F}, 1.0F, 0.001F}};
}

/**
 * @brief River style - water with subtle banks
 */
inline auto get_river_passes(float base_width) -> std::vector<StrokePass> {
  return {
      // River banks (darker blue-gray)
      {{0.25F, 0.32F, 0.40F, 0.75F}, 1.6F, 0.000F},
      // Main water
      {{0.35F, 0.48F, 0.58F, 0.90F}, 1.0F, 0.001F},
      // Water highlight
      {{0.55F, 0.68F, 0.78F, 0.50F}, 0.4F, 0.002F}};
}

} // namespace CartographicStyles

// ============================================================================
// Terrain Height and Normal Utilities
// ============================================================================

/**
 * @brief Generates a normal from height samples using central differences
 * @param h_left Height sample to the left
 * @param h_right Height sample to the right
 * @param h_down Height sample below
 * @param h_up Height sample above
 * @param scale Scale factor for height difference
 * @return Normalized surface normal
 */
inline auto compute_normal_from_heights(float h_left, float h_right,
                                        float h_down, float h_up,
                                        float scale = 1.0F) -> QVector3D {
  const float dx = (h_right - h_left) * scale;
  const float dz = (h_up - h_down) * scale;
  QVector3D normal(-dx, 2.0F, -dz);
  normal.normalize();
  return normal;
}

/**
 * @brief Simple 2D hash for procedural noise
 */
inline auto hash_2d(float x, float y) -> float {
  const float h = std::sin(x * 12.9898F + y * 78.233F) * 43758.5453123F;
  return h - std::floor(h);
}

/**
 * @brief Smooth noise interpolation (value noise)
 */
inline auto value_noise_2d(float x, float y) -> float {
  const float ix = std::floor(x);
  const float iy = std::floor(y);
  const float fx = x - ix;
  const float fy = y - iy;

  // Smoothstep for interpolation
  const float sx = fx * fx * (3.0F - 2.0F * fx);
  const float sy = fy * fy * (3.0F - 2.0F * fy);

  // Hash corners
  const float c00 = hash_2d(ix, iy);
  const float c10 = hash_2d(ix + 1.0F, iy);
  const float c01 = hash_2d(ix, iy + 1.0F);
  const float c11 = hash_2d(ix + 1.0F, iy + 1.0F);

  // Bilinear interpolation
  const float x0 = c00 * (1.0F - sx) + c10 * sx;
  const float x1 = c01 * (1.0F - sx) + c11 * sx;
  return x0 * (1.0F - sy) + x1 * sy;
}

/**
 * @brief Fractal Brownian Motion noise
 */
inline auto fbm_noise_2d(float x, float y, int octaves = 4,
                         float lacunarity = 2.0F,
                         float persistence = 0.5F) -> float {
  float value = 0.0F;
  float amplitude = 1.0F;
  float frequency = 1.0F;
  float max_value = 0.0F;

  for (int i = 0; i < octaves; ++i) {
    value += amplitude * value_noise_2d(x * frequency, y * frequency);
    max_value += amplitude;
    amplitude *= persistence;
    frequency *= lacunarity;
  }

  return value / max_value;
}

// ============================================================================
// Cartographic Color Utilities
// ============================================================================

/**
 * @brief Generates hillshade value from normal and light direction
 * @param normal Surface normal
 * @param light_dir Normalized light direction (towards light)
 * @param ambient Ambient light contribution
 * @return Shading value in [0, 1]
 */
inline auto compute_hillshade(const QVector3D &normal, const QVector3D &light_dir,
                              float ambient = 0.3F) -> float {
  const float ndotl = QVector3D::dotProduct(normal, light_dir);
  return ambient + (1.0F - ambient) * std::max(0.0F, ndotl);
}

/**
 * @brief Converts elevation to bathymetry/elevation tint
 * @param elevation Normalized elevation value
 * @param is_water True if point is underwater
 * @return Tint color to multiply with base
 */
inline auto elevation_to_tint(float elevation, bool is_water) -> QVector4D {
  if (is_water) {
    // Bathymetry: deeper = darker blue
    const float depth_factor = 1.0F - std::min(1.0F, std::abs(elevation) * 2.0F);
    return QVector4D(0.6F * depth_factor + 0.2F, 0.7F * depth_factor + 0.2F,
                     0.85F * depth_factor + 0.15F, 1.0F);
  }

  // Land elevation tinting
  if (elevation < 0.2F) {
    // Lowlands: slight green tint
    return QVector4D(0.95F, 1.0F, 0.92F, 1.0F);
  }
  if (elevation < 0.5F) {
    // Hills: neutral to slight brown
    const float t = (elevation - 0.2F) / 0.3F;
    return QVector4D(1.0F, 0.98F - t * 0.05F, 0.95F - t * 0.08F, 1.0F);
  }
  // Mountains: brown to gray
  const float t = (elevation - 0.5F) / 0.5F;
  return QVector4D(0.95F - t * 0.1F, 0.88F - t * 0.15F, 0.82F - t * 0.12F, 1.0F);
}

/**
 * @brief Parchment texture pattern for province fills
 * @param u, v Texture coordinates
 * @param scale Pattern scale
 * @return Texture intensity for multiply blending
 */
inline auto parchment_pattern(float u, float v, float scale = 8.0F) -> float {
  // Multi-frequency noise for organic parchment look
  const float n1 = fbm_noise_2d(u * scale, v * scale, 3, 2.0F, 0.5F);
  const float n2 = fbm_noise_2d(u * scale * 2.5F + 100.0F,
                                v * scale * 2.5F + 100.0F, 2, 2.0F, 0.4F);

  // Combine and map to parchment-like range
  const float combined = n1 * 0.6F + n2 * 0.4F;
  return 0.85F + combined * 0.15F; // Results in [0.85, 1.0]
}

// ============================================================================
// Camera Defaults for Cinematic View
// ============================================================================

namespace CinematicCameraDefaults {

// Primary view - showcases the Mediterranean with relief visible
inline constexpr float k_default_yaw = 185.0F;      // Slight northwest tilt
inline constexpr float k_default_pitch = 52.0F;     // More oblique for depth
inline constexpr float k_default_distance = 1.35F;  // Closer for detail

// Region focus positions (UV coordinates)
struct RegionFocus {
  float u;
  float v;
  float distance;
  float pitch;
  float yaw;
};

// Key regions for cinematic transitions
inline constexpr RegionFocus k_focus_carthage = {0.35F, 0.55F, 1.0F, 48.0F, 200.0F};
inline constexpr RegionFocus k_focus_rome = {0.55F, 0.35F, 0.9F, 50.0F, 175.0F};
inline constexpr RegionFocus k_focus_spain = {0.18F, 0.42F, 1.1F, 45.0F, 195.0F};
inline constexpr RegionFocus k_focus_alps = {0.52F, 0.28F, 0.85F, 55.0F, 180.0F};
inline constexpr RegionFocus k_focus_sicily = {0.58F, 0.48F, 0.75F, 52.0F, 185.0F};

} // namespace CinematicCameraDefaults

// ============================================================================
// Mission Marker Badge Configuration
// ============================================================================

/**
 * @brief Badge style for mission markers
 */
enum class BadgeStyle {
  Standard,    ///< Military standard/flag
  Seal,        ///< Wax seal style
  Banner,      ///< Hanging banner
  Shield,      ///< Heraldic shield
  Medallion    ///< Circular medallion
};

/**
 * @brief Configuration for mission marker badges
 */
struct MissionBadgeConfig {
  BadgeStyle style = BadgeStyle::Standard;
  QVector4D primary_color{0.75F, 0.18F, 0.12F, 1.0F};    // Deep red
  QVector4D secondary_color{0.95F, 0.85F, 0.45F, 1.0F};  // Gold accent
  QVector4D border_color{0.15F, 0.10F, 0.08F, 1.0F};     // Dark border
  float size = 24.0F;
  float border_width = 2.0F;
  bool show_shadow = true;
  float shadow_offset = 2.0F;
  float shadow_opacity = 0.4F;
};

/**
 * @brief Generates vertices for a shield-shaped badge
 */
inline auto generate_shield_badge(const QVector2D &center, float size,
                                  int segments = 16)
    -> std::vector<QVector2D> {
  std::vector<QVector2D> verts;
  verts.reserve(static_cast<size_t>(segments * 2 + 4));

  const float w = size * 0.5F;
  const float h = size * 0.6F;
  const float pi = 3.14159265358979F;

  // Top edge (flat or slightly curved)
  verts.push_back(center + QVector2D(-w, -h * 0.4F));
  verts.push_back(center + QVector2D(w, -h * 0.4F));

  // Right edge curving down
  for (int i = 0; i <= segments / 2; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments / 2);
    const float angle = pi * 0.5F * t;
    const float x = w * std::cos(angle);
    const float y = -h * 0.4F + h * 0.9F * std::sin(angle) +
                    h * 0.5F * t * t; // Pointed bottom
    verts.push_back(center + QVector2D(x, y));
  }

  // Bottom point
  verts.push_back(center + QVector2D(0.0F, h * 0.6F));

  // Left edge curving up
  for (int i = segments / 2; i >= 0; --i) {
    const float t = static_cast<float>(i) / static_cast<float>(segments / 2);
    const float angle = pi * 0.5F * t;
    const float x = -w * std::cos(angle);
    const float y = -h * 0.4F + h * 0.9F * std::sin(angle) + h * 0.5F * t * t;
    verts.push_back(center + QVector2D(x, y));
  }

  return verts;
}

/**
 * @brief Generates vertices for a banner-shaped badge
 */
inline auto generate_banner_badge(const QVector2D &center, float size,
                                  int segments = 12)
    -> std::vector<QVector2D> {
  std::vector<QVector2D> verts;
  const float w = size * 0.4F;
  const float h = size * 0.7F;

  // Simple rectangular banner with pointed bottom
  verts.push_back(center + QVector2D(-w, -h * 0.5F));  // Top left
  verts.push_back(center + QVector2D(w, -h * 0.5F));   // Top right
  verts.push_back(center + QVector2D(w, h * 0.3F));    // Bottom right
  verts.push_back(center + QVector2D(0.0F, h * 0.5F)); // Point
  verts.push_back(center + QVector2D(-w, h * 0.3F));   // Bottom left

  return verts;
}

/**
 * @brief Generates vertices for a medallion badge
 */
inline auto generate_medallion_badge(const QVector2D &center, float size,
                                     int segments = 24)
    -> std::vector<QVector2D> {
  std::vector<QVector2D> verts;
  verts.reserve(static_cast<size_t>(segments + 1));

  const float radius = size * 0.5F;
  const float pi = 3.14159265358979F;

  for (int i = 0; i <= segments; ++i) {
    const float angle = 2.0F * pi * static_cast<float>(i) / static_cast<float>(segments);
    verts.push_back(center +
                    QVector2D(radius * std::cos(angle), radius * std::sin(angle)));
  }

  return verts;
}

// ============================================================================
// Cartographic Symbol Generation
// ============================================================================

/**
 * @brief Symbol type for map features
 */
enum class CartographicSymbol {
  Mountain,
  City,
  Port,
  Fort,
  Temple
};

/**
 * @brief Generates mountain icon vertices (triangular peaks)
 * @param center Icon center position
 * @param size Icon size
 * @param peaks Number of mountain peaks (1-3)
 * @return Triangle vertices for the mountain icon
 */
inline auto generate_mountain_icon(const QVector2D &center, float size,
                                   int peaks = 2) -> std::vector<QVector2D> {
  std::vector<QVector2D> verts;
  const float h = size * 0.5F;
  const float w = size * 0.3F;

  if (peaks == 1) {
    // Single peak
    verts.push_back(center + QVector2D(-w, h * 0.3F));
    verts.push_back(center + QVector2D(0.0F, -h * 0.5F));
    verts.push_back(center + QVector2D(w, h * 0.3F));
  } else if (peaks == 2) {
    // Double peak
    verts.push_back(center + QVector2D(-w * 1.5F, h * 0.3F));
    verts.push_back(center + QVector2D(-w * 0.5F, -h * 0.4F));
    verts.push_back(center + QVector2D(0.0F, h * 0.1F));
    verts.push_back(center + QVector2D(w * 0.5F, -h * 0.5F));
    verts.push_back(center + QVector2D(w * 1.5F, h * 0.3F));
  } else {
    // Triple peak
    verts.push_back(center + QVector2D(-w * 2.0F, h * 0.3F));
    verts.push_back(center + QVector2D(-w, -h * 0.35F));
    verts.push_back(center + QVector2D(-w * 0.3F, h * 0.1F));
    verts.push_back(center + QVector2D(0.0F, -h * 0.5F));
    verts.push_back(center + QVector2D(w * 0.3F, h * 0.0F));
    verts.push_back(center + QVector2D(w, -h * 0.4F));
    verts.push_back(center + QVector2D(w * 2.0F, h * 0.3F));
  }

  return verts;
}

/**
 * @brief Generates city marker vertices (building silhouette)
 * @param center Icon center position
 * @param size Icon size
 * @param importance City importance (affects complexity)
 * @return Triangle vertices for the city marker
 */
inline auto generate_city_marker(const QVector2D &center, float size,
                                 int importance = 1) -> std::vector<QVector2D> {
  std::vector<QVector2D> verts;
  const float h = size * 0.5F;
  const float w = size * 0.2F;

  // Base rectangle
  verts.push_back(center + QVector2D(-w * 2.0F, h * 0.3F));
  verts.push_back(center + QVector2D(w * 2.0F, h * 0.3F));

  if (importance >= 2) {
    // Add towers for major cities
    verts.push_back(center + QVector2D(w * 2.0F, -h * 0.2F));
    verts.push_back(center + QVector2D(w * 1.5F, -h * 0.2F));
    verts.push_back(center + QVector2D(w * 1.5F, -h * 0.5F));
    verts.push_back(center + QVector2D(w * 0.5F, -h * 0.5F));
    verts.push_back(center + QVector2D(w * 0.5F, -h * 0.3F));
    verts.push_back(center + QVector2D(-w * 0.5F, -h * 0.3F));
    verts.push_back(center + QVector2D(-w * 0.5F, -h * 0.6F));
    verts.push_back(center + QVector2D(-w * 1.5F, -h * 0.6F));
    verts.push_back(center + QVector2D(-w * 1.5F, -h * 0.2F));
    verts.push_back(center + QVector2D(-w * 2.0F, -h * 0.2F));
  } else {
    // Simple building for minor cities
    verts.push_back(center + QVector2D(w * 2.0F, -h * 0.1F));
    verts.push_back(center + QVector2D(w, -h * 0.1F));
    verts.push_back(center + QVector2D(w, -h * 0.4F));
    verts.push_back(center + QVector2D(-w, -h * 0.4F));
    verts.push_back(center + QVector2D(-w, -h * 0.1F));
    verts.push_back(center + QVector2D(-w * 2.0F, -h * 0.1F));
  }

  return verts;
}

/**
 * @brief Generates anchor icon for ports
 * @param center Icon center position
 * @param size Icon size
 * @return Vertices for anchor icon (as line segments)
 */
inline auto generate_anchor_icon(const QVector2D &center,
                                 float size) -> std::vector<QVector2D> {
  std::vector<QVector2D> verts;
  const float h = size * 0.5F;
  const float w = size * 0.4F;

  // Vertical shaft
  verts.push_back(center + QVector2D(0.0F, -h * 0.6F));
  verts.push_back(center + QVector2D(0.0F, h * 0.4F));

  // Ring at top
  const float ring_r = size * 0.12F;
  const int ring_segs = 8;
  const float pi = 3.14159265358979F;
  const QVector2D ring_center = center + QVector2D(0.0F, -h * 0.6F - ring_r);
  for (int i = 0; i <= ring_segs; ++i) {
    const float angle =
        2.0F * pi * static_cast<float>(i) / static_cast<float>(ring_segs);
    verts.push_back(ring_center + QVector2D(ring_r * std::cos(angle),
                                            ring_r * std::sin(angle)));
  }

  // Crossbar
  verts.push_back(center + QVector2D(-w * 0.6F, -h * 0.2F));
  verts.push_back(center + QVector2D(w * 0.6F, -h * 0.2F));

  // Curved bottom (flukes)
  verts.push_back(center + QVector2D(-w, h * 0.1F));
  verts.push_back(center + QVector2D(0.0F, h * 0.4F));
  verts.push_back(center + QVector2D(w, h * 0.1F));

  return verts;
}

// ============================================================================
// 3D Terrain Heightmap Generation
// ============================================================================

/**
 * @brief Mediterranean region elevation data configuration
 * Used to generate procedural heightmap based on known geographic features
 */
struct MediterraneanTerrainConfig {
  // Mountain ranges (normalized UV coordinates)
  static constexpr float alps_u_min = 0.48F;
  static constexpr float alps_u_max = 0.58F;
  static constexpr float alps_v_min = 0.22F;
  static constexpr float alps_v_max = 0.32F;
  static constexpr float alps_height = 0.85F;

  static constexpr float pyrenees_u_min = 0.20F;
  static constexpr float pyrenees_u_max = 0.32F;
  static constexpr float pyrenees_v_min = 0.30F;
  static constexpr float pyrenees_v_max = 0.38F;
  static constexpr float pyrenees_height = 0.65F;

  static constexpr float apennines_u_min = 0.52F;
  static constexpr float apennines_u_max = 0.62F;
  static constexpr float apennines_v_min = 0.35F;
  static constexpr float apennines_v_max = 0.55F;
  static constexpr float apennines_height = 0.55F;

  static constexpr float atlas_u_min = 0.30F;
  static constexpr float atlas_u_max = 0.55F;
  static constexpr float atlas_v_min = 0.62F;
  static constexpr float atlas_v_max = 0.72F;
  static constexpr float atlas_height = 0.60F;

  // Sea level definition (negative = water)
  static constexpr float sea_level = 0.0F;
  static constexpr float max_depth = -0.35F;
};

/**
 * @brief Helper function to compute mountain range contribution to height
 * @param u, v Current UV coordinates
 * @param u_min, u_max, v_min, v_max Range bounds
 * @param peak_height Maximum height for this range
 * @return Height contribution from this mountain range
 */
inline auto compute_mountain_contribution(float u, float v,
                                          float u_min, float u_max,
                                          float v_min, float v_max,
                                          float peak_height) -> float {
  if (u < u_min || u > u_max || v < v_min || v > v_max) {
    return 0.0F;
  }

  float dist_u = 1.0F - 2.0F * std::abs(u - (u_min + u_max) * 0.5F) /
                                (u_max - u_min);
  float dist_v = 1.0F - 2.0F * std::abs(v - (v_min + v_max) * 0.5F) /
                                (v_max - v_min);
  float falloff = dist_u * dist_v;
  falloff = falloff * falloff;
  return peak_height * falloff;
}

/**
 * @brief Generates terrain height at a given UV coordinate
 * Uses procedural noise shaped by Mediterranean geographic features
 */
inline auto generate_terrain_height(float u, float v) -> float {
  using Config = MediterraneanTerrainConfig;

  // Start with base continental height
  float height = 0.05F;

  // Add contributions from each mountain range
  height += compute_mountain_contribution(u, v,
      Config::alps_u_min, Config::alps_u_max,
      Config::alps_v_min, Config::alps_v_max, Config::alps_height);

  height += compute_mountain_contribution(u, v,
      Config::pyrenees_u_min, Config::pyrenees_u_max,
      Config::pyrenees_v_min, Config::pyrenees_v_max, Config::pyrenees_height);

  height += compute_mountain_contribution(u, v,
      Config::apennines_u_min, Config::apennines_u_max,
      Config::apennines_v_min, Config::apennines_v_max, Config::apennines_height);

  height += compute_mountain_contribution(u, v,
      Config::atlas_u_min, Config::atlas_u_max,
      Config::atlas_v_min, Config::atlas_v_max, Config::atlas_height);

  // Add procedural noise for natural variation
  float noise = fbm_noise_2d(u * 8.0F, v * 8.0F, 4, 2.0F, 0.5F);
  height += (noise - 0.5F) * 0.15F;

  return height;
}

/**
 * @brief Computes surface normal from height samples using central differences
 */
inline auto compute_terrain_normal(float u, float v, float sample_dist = 0.01F) -> QVector3D {
  float h_left = generate_terrain_height(u - sample_dist, v);
  float h_right = generate_terrain_height(u + sample_dist, v);
  float h_down = generate_terrain_height(u, v - sample_dist);
  float h_up = generate_terrain_height(u, v + sample_dist);

  float dx = (h_right - h_left) / (2.0F * sample_dist);
  float dz = (h_up - h_down) / (2.0F * sample_dist);

  QVector3D normal(-dx, 1.0F, -dz);
  normal.normalize();
  return normal;
}

/**
 * @brief Generates a complete terrain mesh with height displacement
 * @param resolution Grid resolution (vertices per side)
 * @param height_scale Scale factor for height values
 * @return Vector of floats: [pos.x, pos.y, uv.u, uv.v, height, normal.x, normal.y, normal.z]
 */
inline auto generate_terrain_mesh(int resolution = 64, float height_scale = 0.05F)
    -> std::vector<float> {
  std::vector<float> vertices;
  const int vertex_floats = 8; // pos(2) + uv(2) + height(1) + normal(3)
  vertices.reserve(static_cast<size_t>(resolution * resolution * 6 * vertex_floats));

  const float step = 1.0F / static_cast<float>(resolution - 1);

  for (int y = 0; y < resolution - 1; ++y) {
    for (int x = 0; x < resolution - 1; ++x) {
      float u0 = static_cast<float>(x) * step;
      float v0 = static_cast<float>(y) * step;
      float u1 = static_cast<float>(x + 1) * step;
      float v1 = static_cast<float>(y + 1) * step;

      // Heights at corners
      float h00 = generate_terrain_height(u0, v0) * height_scale;
      float h10 = generate_terrain_height(u1, v0) * height_scale;
      float h01 = generate_terrain_height(u0, v1) * height_scale;
      float h11 = generate_terrain_height(u1, v1) * height_scale;

      // Normals at corners
      QVector3D n00 = compute_terrain_normal(u0, v0);
      QVector3D n10 = compute_terrain_normal(u1, v0);
      QVector3D n01 = compute_terrain_normal(u0, v1);
      QVector3D n11 = compute_terrain_normal(u1, v1);

      // Triangle 1: (0,0), (1,0), (0,1)
      auto add_vertex = [&](float u, float v, float h, const QVector3D &n) {
        vertices.push_back(u);
        vertices.push_back(v);
        vertices.push_back(u);
        vertices.push_back(v);
        vertices.push_back(h);
        vertices.push_back(n.x());
        vertices.push_back(n.y());
        vertices.push_back(n.z());
      };

      add_vertex(u0, v0, h00, n00);
      add_vertex(u1, v0, h10, n10);
      add_vertex(u0, v1, h01, n01);

      // Triangle 2: (1,0), (1,1), (0,1)
      add_vertex(u1, v0, h10, n10);
      add_vertex(u1, v1, h11, n11);
      add_vertex(u0, v1, h01, n01);
    }
  }

  return vertices;
}

// ============================================================================
// Hillshade Texture Generation
// ============================================================================

/**
 * @brief Configuration for hillshade generation
 */
struct HillshadeConfig {
  QVector3D light_direction{0.35F, 0.85F, 0.40F};  // NW light (azimuth ~315°, altitude ~45°)
  float ambient = 0.25F;
  float intensity = 1.0F;
  float z_factor = 2.5F;  // Vertical exaggeration
};

/**
 * @brief Generates a hillshade value at a UV coordinate
 * @return Hillshade intensity in [0, 1]
 */
inline auto compute_hillshade_at(float u, float v, const HillshadeConfig &config) -> float {
  QVector3D normal = compute_terrain_normal(u, v, 0.005F);

  // Exaggerate vertical component for more dramatic relief
  normal.setY(normal.y() * config.z_factor);
  normal.normalize();

  QVector3D light = config.light_direction.normalized();
  float shade = QVector3D::dotProduct(normal, light);
  shade = config.ambient + (1.0F - config.ambient) * std::max(0.0F, shade);
  return std::min(1.0F, shade * config.intensity);
}

/**
 * @brief Generates a complete hillshade texture as RGBA bytes
 * @param width Texture width in pixels
 * @param height Texture height in pixels
 * @return Vector of RGBA bytes
 */
inline auto generate_hillshade_texture(int width, int height,
                                       const HillshadeConfig &config = HillshadeConfig{})
    -> std::vector<unsigned char> {
  std::vector<unsigned char> pixels;
  pixels.reserve(static_cast<size_t>(width * height * 4));

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float u = static_cast<float>(x) / static_cast<float>(width - 1);
      float v = static_cast<float>(y) / static_cast<float>(height - 1);

      float shade = compute_hillshade_at(u, v, config);
      auto byte_val = static_cast<unsigned char>(shade * 255.0F);

      // Store as grayscale in RGBA format
      pixels.push_back(byte_val);  // R
      pixels.push_back(byte_val);  // G
      pixels.push_back(byte_val);  // B
      pixels.push_back(255);       // A
    }
  }

  return pixels;
}

// ============================================================================
// Typography / SDF Text Rendering
// ============================================================================

/**
 * @brief Glyph metrics for SDF font rendering
 */
struct GlyphMetrics {
  float advance;     // Horizontal advance
  float bearing_x;   // Horizontal bearing
  float bearing_y;   // Vertical bearing
  float width;       // Glyph width
  float height;      // Glyph height
  float uv_x;        // Texture U coordinate
  float uv_y;        // Texture V coordinate
  float uv_w;        // Texture width
  float uv_h;        // Texture height
};

/**
 * @brief Configuration for cartographic label styling
 */
struct LabelStyle {
  float font_size = 14.0F;
  QVector4D fill_color{0.18F, 0.14F, 0.10F, 1.0F};      // Dark brown/sepia
  QVector4D stroke_color{0.95F, 0.92F, 0.88F, 0.85F};   // Light cream outline
  float stroke_width = 1.5F;
  bool use_small_caps = true;
  float letter_spacing = 0.05F;  // As fraction of font size
  float line_height = 1.2F;
};

/**
 * @brief Province label styling presets
 */
namespace LabelStyles {

inline auto province_label() -> LabelStyle {
  return {
      .font_size = 12.0F,
      .fill_color = QVector4D(0.25F, 0.20F, 0.15F, 0.95F),
      .stroke_color = QVector4D(0.98F, 0.96F, 0.92F, 0.75F),
      .stroke_width = 1.2F,
      .use_small_caps = true,
      .letter_spacing = 0.08F,
      .line_height = 1.15F};
}

inline auto city_label() -> LabelStyle {
  return {
      .font_size = 10.0F,
      .fill_color = QVector4D(0.30F, 0.25F, 0.18F, 0.90F),
      .stroke_color = QVector4D(0.98F, 0.96F, 0.92F, 0.70F),
      .stroke_width = 1.0F,
      .use_small_caps = false,
      .letter_spacing = 0.03F,
      .line_height = 1.1F};
}

inline auto region_label() -> LabelStyle {
  return {
      .font_size = 16.0F,
      .fill_color = QVector4D(0.20F, 0.16F, 0.12F, 1.0F),
      .stroke_color = QVector4D(0.95F, 0.92F, 0.88F, 0.80F),
      .stroke_width = 2.0F,
      .use_small_caps = true,
      .letter_spacing = 0.12F,
      .line_height = 1.25F};
}

inline auto sea_label() -> LabelStyle {
  return {
      .font_size = 11.0F,
      .fill_color = QVector4D(0.25F, 0.38F, 0.50F, 0.85F),  // Blue-gray for water
      .stroke_color = QVector4D(0.92F, 0.95F, 0.98F, 0.65F),
      .stroke_width = 1.0F,
      .use_small_caps = true,
      .letter_spacing = 0.15F,
      .line_height = 1.2F};
}

} // namespace LabelStyles

/**
 * @brief Generates quad vertices for a text label
 * @param position Label anchor position (UV coordinates)
 * @param text Label text
 * @param style Label styling
 * @param base_font_size Reference font size for scaling
 * @return Vertex data for rendering quads
 */
inline auto generate_label_quads(const QVector2D &position, const std::string &text,
                                 const LabelStyle &style,
                                 float base_font_size = 12.0F) -> std::vector<float> {
  std::vector<float> vertices;
  if (text.empty()) {
    return vertices;
  }

  // Calculate scale factor based on style font size vs base
  const float scale = style.font_size / base_font_size;
  const float char_width = 0.006F * scale;  // Approximate character width in UV space
  const float char_height = 0.012F * scale;
  const float spacing = char_width * style.letter_spacing;

  // Calculate total text width for centering
  const float total_width = static_cast<float>(text.length()) * (char_width + spacing);
  float x_offset = -total_width * 0.5F;

  // Generate quad vertices for each character
  // Format: pos.x, pos.y, uv.x, uv.y, local.x, local.y
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c == ' ') {
      x_offset += char_width * 0.5F;
      continue;
    }

    // Character UV in atlas (simplified: assume uniform grid)
    float atlas_u = static_cast<float>(c % 16) / 16.0F;
    float atlas_v = static_cast<float>(c / 16) / 16.0F;
    float atlas_w = 1.0F / 16.0F;
    float atlas_h = 1.0F / 16.0F;

    // Quad corners
    float x0 = position.x() + x_offset;
    float y0 = position.y() - char_height * 0.5F;
    float x1 = x0 + char_width;
    float y1 = y0 + char_height;

    // Triangle 1
    vertices.insert(vertices.end(), {x0, y0, atlas_u, atlas_v, 0.0F, 0.0F});
    vertices.insert(vertices.end(), {x1, y0, atlas_u + atlas_w, atlas_v, 1.0F, 0.0F});
    vertices.insert(vertices.end(), {x0, y1, atlas_u, atlas_v + atlas_h, 0.0F, 1.0F});

    // Triangle 2
    vertices.insert(vertices.end(), {x1, y0, atlas_u + atlas_w, atlas_v, 1.0F, 0.0F});
    vertices.insert(vertices.end(), {x1, y1, atlas_u + atlas_w, atlas_v + atlas_h, 1.0F, 1.0F});
    vertices.insert(vertices.end(), {x0, y1, atlas_u, atlas_v + atlas_h, 0.0F, 1.0F});

    x_offset += char_width + spacing;
  }

  return vertices;
}

/**
 * @brief Calculates zoom-consistent label scale
 * @param viewport_height Viewport height in pixels
 * @param camera_distance Camera distance from target
 * @param base_size Base size in screen pixels
 * @return Scale factor to apply to labels
 */
inline auto compute_label_scale(float viewport_height, float camera_distance,
                                float base_size = 12.0F) -> float {
  // Convert screen pixels to UV space, accounting for camera distance
  const float fov_rad = 0.7854F;  // 45 degrees
  const float view_height = 2.0F * camera_distance * std::tan(fov_rad * 0.5F);
  const float px_to_uv = view_height / viewport_height;
  return base_size * px_to_uv;
}

} // namespace CampaignMapRender
