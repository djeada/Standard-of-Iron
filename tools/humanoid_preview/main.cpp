

#include <QGuiApplication>
#include <QImage>
#include <QMatrix4x4>
#include <QPainter>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include "animation/bpat/bpat_reader.h"
#include "animation/rig/humanoid_proportions.h"
#include "render/software/software_rasterizer.h"

namespace {

using Render::Creature::Bpat::BpatBlob;
using Render::Software::ColoredTriangle;
using Render::Software::RasterSettings;
using Render::Software::SoftwareRasterizer;

enum class Bone : std::size_t {
  Root = 0,
  Pelvis,
  Spine,
  Chest,
  Neck,
  Head,
  ShoulderL,
  UpperArmL,
  ForearmL,
  HandL,
  ShoulderR,
  UpperArmR,
  ForearmR,
  HandR,
  HipL,
  KneeL,
  FootL,
  HipR,
  KneeR,
  FootR,
  Count
};

constexpr std::size_t k_bone_count = static_cast<std::size_t>(Bone::Count);

struct Segment {
  Bone from;
  Bone to;
  float radius_from;
  float radius_to;
  QVector3D color;
};

using HP = Render::GL::HumanProportions;

const std::array<Segment, 15> k_segments{{
    {Bone::Pelvis, Bone::Spine, 0.105F, 0.100F, {0.30F, 0.34F, 0.48F}},
    {Bone::Spine, Bone::Chest, 0.100F, 0.112F, {0.30F, 0.34F, 0.48F}},
    {Bone::Chest, Bone::Neck, 0.112F, 0.048F, {0.32F, 0.36F, 0.50F}},
    {Bone::Neck, Bone::Head, 0.048F, 0.055F, {0.72F, 0.58F, 0.46F}},
    {Bone::Chest, Bone::ShoulderL, 0.070F, 0.058F, {0.26F, 0.30F, 0.44F}},
    {Bone::Chest, Bone::ShoulderR, 0.070F, 0.058F, {0.26F, 0.30F, 0.44F}},
    {Bone::UpperArmL, Bone::ForearmL, 0.052F, 0.040F, {0.70F, 0.56F, 0.44F}},
    {Bone::ForearmL, Bone::HandL, 0.040F, 0.030F, {0.74F, 0.60F, 0.48F}},
    {Bone::UpperArmR, Bone::ForearmR, 0.052F, 0.040F, {0.86F, 0.62F, 0.42F}},
    {Bone::ForearmR, Bone::HandR, 0.040F, 0.030F, {0.90F, 0.66F, 0.46F}},
    {Bone::HipL, Bone::KneeL, 0.072F, 0.052F, {0.24F, 0.28F, 0.42F}},
    {Bone::KneeL, Bone::FootL, 0.052F, 0.036F, {0.26F, 0.30F, 0.44F}},
    {Bone::HipR, Bone::KneeR, 0.072F, 0.052F, {0.34F, 0.26F, 0.30F}},
    {Bone::KneeR, Bone::FootR, 0.052F, 0.036F, {0.36F, 0.28F, 0.32F}},
    {Bone::Head, Bone::Head, 0.0F, 0.0F, {0.0F, 0.0F, 0.0F}},
}};

struct BoneLengthRule {
  Bone from;
  Bone to;
  float bind_length;
  const char* label;
};

const std::array<BoneLengthRule, 8> k_length_rules{{
    {Bone::UpperArmL, Bone::ForearmL, HP::UPPER_ARM_LEN, "upper_arm_l"},
    {Bone::ForearmL, Bone::HandL, HP::FORE_ARM_LEN, "forearm_l"},
    {Bone::UpperArmR, Bone::ForearmR, HP::UPPER_ARM_LEN, "upper_arm_r"},
    {Bone::ForearmR, Bone::HandR, HP::FORE_ARM_LEN, "forearm_r"},
    {Bone::HipL, Bone::KneeL, HP::UPPER_LEG_LEN, "thigh_l"},
    {Bone::KneeL, Bone::FootL, HP::LOWER_LEG_LEN, "shin_l"},
    {Bone::HipR, Bone::KneeR, HP::UPPER_LEG_LEN, "thigh_r"},
    {Bone::KneeR, Bone::FootR, HP::LOWER_LEG_LEN, "shin_r"},
}};

auto bone_origin(std::span<const QMatrix4x4> palette, Bone bone) -> QVector3D {
  auto const index = static_cast<std::size_t>(bone);
  if (index >= palette.size()) {
    return {};
  }
  return palette[index].column(3).toVector3D();
}

void append_prism(std::vector<ColoredTriangle>& out,
                  const QVector3D& a,
                  const QVector3D& b,
                  float radius_a,
                  float radius_b,
                  const QVector3D& color) {
  QVector3D axis = b - a;
  float const length = axis.length();
  if (length < 1.0e-5F) {
    return;
  }
  axis /= length;
  QVector3D reference(0.0F, 1.0F, 0.0F);
  if (std::abs(QVector3D::dotProduct(reference, axis)) > 0.94F) {
    reference = QVector3D(1.0F, 0.0F, 0.0F);
  }
  QVector3D const side = QVector3D::crossProduct(reference, axis).normalized();
  QVector3D const up = QVector3D::crossProduct(axis, side).normalized();

  constexpr int k_slices = 10;
  std::array<QVector3D, k_slices> ring_a{};
  std::array<QVector3D, k_slices> ring_b{};
  for (int i = 0; i < k_slices; ++i) {
    float const angle = 2.0F * std::numbers::pi_v<float> * static_cast<float>(i) /
                        static_cast<float>(k_slices);
    QVector3D const dir = side * std::cos(angle) + up * std::sin(angle);
    ring_a[i] = a + dir * radius_a;
    ring_b[i] = b + dir * radius_b;
  }
  for (int i = 0; i < k_slices; ++i) {
    int const j = (i + 1) % k_slices;
    out.push_back({ring_a[i], ring_b[i], ring_b[j], color, 1.0F});
    out.push_back({ring_a[i], ring_b[j], ring_a[j], color, 1.0F});
  }
  for (int i = 1; i + 1 < k_slices; ++i) {
    out.push_back({ring_b[0], ring_b[i], ring_b[i + 1], color, 1.0F});
    out.push_back({ring_a[0], ring_a[i + 1], ring_a[i], color, 1.0F});
  }
}

void append_sphere(std::vector<ColoredTriangle>& out,
                   const QVector3D& center,
                   float radius,
                   const QVector3D& color) {
  constexpr int k_stacks = 8;
  constexpr int k_slices = 12;
  auto point = [&](int stack, int slice) {
    float const phi = std::numbers::pi_v<float> * static_cast<float>(stack) /
                      static_cast<float>(k_stacks);
    float const theta = 2.0F * std::numbers::pi_v<float> * static_cast<float>(slice) /
                        static_cast<float>(k_slices);
    return center + QVector3D(radius * std::sin(phi) * std::cos(theta),
                              radius * std::cos(phi),
                              radius * std::sin(phi) * std::sin(theta));
  };
  for (int stack = 0; stack < k_stacks; ++stack) {
    for (int slice = 0; slice < k_slices; ++slice) {
      QVector3D const p00 = point(stack, slice);
      QVector3D const p01 = point(stack, slice + 1);
      QVector3D const p10 = point(stack + 1, slice);
      QVector3D const p11 = point(stack + 1, slice + 1);
      out.push_back({p00, p10, p11, color, 1.0F});
      out.push_back({p00, p11, p01, color, 1.0F});
    }
  }
}

void append_ground(std::vector<ColoredTriangle>& out) {
  constexpr float k_half = 2.4F;
  constexpr int k_cells = 12;
  constexpr float k_step = (2.0F * k_half) / static_cast<float>(k_cells);
  for (int ix = 0; ix < k_cells; ++ix) {
    for (int iz = 0; iz < k_cells; ++iz) {
      float const x0 = -k_half + (k_step * static_cast<float>(ix));
      float const z0 = -k_half + (k_step * static_cast<float>(iz));
      float const x1 = x0 + k_step;
      float const z1 = z0 + k_step;
      bool const light = ((ix + iz) % 2) == 0;
      QVector3D const color =
          light ? QVector3D(0.30F, 0.33F, 0.27F) : QVector3D(0.25F, 0.28F, 0.23F);
      out.push_back({{x0, 0.0F, z0}, {x1, 0.0F, z1}, {x1, 0.0F, z0}, color, 1.0F});
      out.push_back({{x0, 0.0F, z0}, {x0, 0.0F, z1}, {x1, 0.0F, z1}, color, 1.0F});
    }
  }
}

struct FrameMetrics {
  float phase{0.0F};
  float foot_l_z{0.0F};
  float foot_r_z{0.0F};
  float hand_l_z{0.0F};
  float hand_r_z{0.0F};
  float pelvis_y{0.0F};
  float pelvis_x{0.0F};
  float step_width{0.0F};
  float shoulder_l_z{0.0F};
  float max_bone_stretch{0.0F};
  const char* worst_bone{"-"};
  float arm_reach_l{0.0F};
  float arm_reach_r{0.0F};
  float lowest_foot_y{0.0F};
  float torso_up_y{1.0F};
  QVector3D hand_r_axis{};
};

auto measure(std::span<const QMatrix4x4> palette, float phase) -> FrameMetrics {
  FrameMetrics metrics{};
  metrics.phase = phase;
  metrics.foot_l_z = bone_origin(palette, Bone::FootL).z();
  metrics.foot_r_z = bone_origin(palette, Bone::FootR).z();
  metrics.hand_l_z = bone_origin(palette, Bone::HandL).z();
  metrics.hand_r_z = bone_origin(palette, Bone::HandR).z();
  metrics.pelvis_y = bone_origin(palette, Bone::Pelvis).y();
  metrics.pelvis_x = bone_origin(palette, Bone::Pelvis).x();
  metrics.shoulder_l_z = bone_origin(palette, Bone::ShoulderL).z();
  metrics.step_width =
      bone_origin(palette, Bone::FootR).x() - bone_origin(palette, Bone::FootL).x();
  for (auto const& rule : k_length_rules) {
    float const length =
        (bone_origin(palette, rule.to) - bone_origin(palette, rule.from)).length();
    float const stretch = length / std::max(1.0e-4F, rule.bind_length);
    if (stretch > metrics.max_bone_stretch) {
      metrics.max_bone_stretch = stretch;
      metrics.worst_bone = rule.label;
    }
  }
  metrics.arm_reach_l =
      (bone_origin(palette, Bone::HandL) - bone_origin(palette, Bone::ShoulderL))
          .length();
  metrics.arm_reach_r =
      (bone_origin(palette, Bone::HandR) - bone_origin(palette, Bone::ShoulderR))
          .length();
  metrics.lowest_foot_y = std::min(bone_origin(palette, Bone::FootL).y(),
                                   bone_origin(palette, Bone::FootR).y());
  QVector3D const torso =
      bone_origin(palette, Bone::Neck) - bone_origin(palette, Bone::Pelvis);
  metrics.torso_up_y = torso.lengthSquared() > 1.0e-8F ? torso.normalized().y() : 1.0F;
  auto const hand_index = static_cast<std::size_t>(Bone::HandR);
  if (hand_index < palette.size()) {
    metrics.hand_r_axis = palette[hand_index].column(1).toVector3D();
  }
  return metrics;
}

struct StrideSummary {
  float foot_travel{0.0F};
  float planted_travel_per_cycle{0.0F};
  float hand_swing{0.0F};
  float pelvis_bob{0.0F};
  float pelvis_sway{0.0F};
  float step_width{0.0F};
  float shoulder_twist{0.0F};
  float foot_lift{0.0F};
};

auto summarize_stride(const BpatBlob& blob,
                      const Render::Creature::Bpat::ClipView& clip) -> StrideSummary {
  std::vector<FrameMetrics> frames;
  std::vector<float> foot_l_y;
  frames.reserve(clip.frame_count);
  foot_l_y.reserve(clip.frame_count);
  for (std::uint32_t f = 0; f < clip.frame_count; ++f) {
    auto const palette = blob.frame_palette_view(clip.frame_offset + f);
    frames.push_back(
        measure(palette, static_cast<float>(f) / static_cast<float>(clip.frame_count)));
    foot_l_y.push_back(bone_origin(palette, Bone::FootL).y());
  }
  if (frames.size() < 4U) {
    return {};
  }

  auto range = [&](auto&& pick) {
    float lo = std::numeric_limits<float>::max();
    float hi = std::numeric_limits<float>::lowest();
    for (auto const& m : frames) {
      float const value = pick(m);
      lo = std::min(lo, value);
      hi = std::max(hi, value);
    }
    return hi - lo;
  };

  StrideSummary summary{};
  summary.foot_travel = range([](const FrameMetrics& m) { return m.foot_l_z; });
  summary.hand_swing = range([](const FrameMetrics& m) { return m.hand_l_z; });
  summary.pelvis_bob = range([](const FrameMetrics& m) { return m.pelvis_y; });
  summary.pelvis_sway = range([](const FrameMetrics& m) { return m.pelvis_x; });
  for (auto const& m : frames) {
    summary.step_width += m.step_width;
  }
  summary.step_width /= static_cast<float>(frames.size());
  summary.shoulder_twist = range([](const FrameMetrics& m) { return m.shoulder_l_z; });

  float const lowest = *std::min_element(foot_l_y.begin(), foot_l_y.end());
  float const highest = *std::max_element(foot_l_y.begin(), foot_l_y.end());
  summary.foot_lift = highest - lowest;

  std::vector<float> slopes;
  auto const count = static_cast<std::uint32_t>(frames.size());
  float const d_phase = 1.0F / static_cast<float>(count);
  for (std::uint32_t f = 0; f < count; ++f) {
    std::uint32_t const next = (f + 1U) % count;
    if (foot_l_y[f] > lowest + 0.015F || foot_l_y[next] > lowest + 0.015F) {
      continue;
    }
    float const delta = frames[next].foot_l_z - frames[f].foot_l_z;
    if (delta < 0.0F) {
      slopes.push_back(-delta / d_phase);
    }
  }
  if (!slopes.empty()) {
    std::sort(slopes.begin(), slopes.end());
    summary.planted_travel_per_cycle = slopes[slopes.size() / 2U];
  }
  return summary;
}

auto view_matrix(std::string_view view, int width, int height) -> QMatrix4x4 {
  QVector3D eye(4.1F, 1.25F, 0.0F);
  if (view == "front") {
    eye = QVector3D(0.0F, 1.25F, 4.1F);
  } else if (view == "iso") {
    eye = QVector3D(3.0F, 2.10F, 3.0F);
  } else if (view == "top") {
    eye = QVector3D(0.02F, 4.2F, 0.02F);
  }
  QMatrix4x4 projection;
  projection.perspective(
      34.0F, static_cast<float>(width) / static_cast<float>(height), 0.1F, 40.0F);
  QMatrix4x4 look;
  look.lookAt(eye, QVector3D(0.0F, 0.95F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F));
  return projection * look;
}

auto socket_index(const BpatBlob& blob, std::string_view name) -> std::uint32_t {
  for (std::uint32_t i = 0; i < blob.socket_count(); ++i) {
    if (blob.socket(i).name == name) {
      return i;
    }
  }
  return 0xFFFFFFFFU;
}

auto socket_origin(const BpatBlob& blob,
                   std::uint32_t global_frame,
                   std::uint32_t index) -> QVector3D {
  if (index == 0xFFFFFFFFU) {
    return {};
  }
  auto const values = blob.socket_matrix(global_frame, index);
  if (values.size() < 12U) {
    return {};
  }
  return {values[3], values[7], values[11]};
}

void render_frame(const BpatBlob& blob,
                  std::uint32_t global_frame,
                  std::string_view view,
                  std::string_view weapon,
                  int width,
                  int height,
                  QImage& out) {
  auto const palette = blob.frame_palette_view(global_frame);
  std::vector<ColoredTriangle> triangles;
  triangles.reserve(2048);
  append_ground(triangles);

  for (auto const& segment : k_segments) {
    if (segment.from == segment.to) {
      continue;
    }
    append_prism(triangles,
                 bone_origin(palette, segment.from),
                 bone_origin(palette, segment.to),
                 segment.radius_from,
                 segment.radius_to,
                 segment.color);
  }
  append_sphere(
      triangles, bone_origin(palette, Bone::Head), 0.115F, {0.80F, 0.64F, 0.52F});

  auto draw_foot = [&](Bone bone, const QVector3D& color) {
    auto const index = static_cast<std::size_t>(bone);
    if (index >= palette.size()) {
      return;
    }
    QVector3D const ankle = palette[index].column(3).toVector3D();
    QVector3D const forward = palette[index].column(2).toVector3D();
    QVector3D const up = palette[index].column(1).toVector3D();
    QVector3D const heel = ankle - (forward * 0.055F) - (up * 0.010F);
    QVector3D const toe = ankle + (forward * 0.170F) - (up * 0.018F);
    append_prism(triangles, heel, toe, 0.036F, 0.028F, color);
  };
  draw_foot(Bone::FootL, {0.20F, 0.16F, 0.14F});
  draw_foot(Bone::FootR, {0.26F, 0.18F, 0.16F});

  auto draw_weapon = [&](std::string_view base_name,
                         std::string_view tip_name,
                         float radius,
                         const QVector3D& color) {
    QVector3D const base =
        socket_origin(blob, global_frame, socket_index(blob, base_name));
    QVector3D const tip =
        socket_origin(blob, global_frame, socket_index(blob, tip_name));
    if ((tip - base).lengthSquared() < 1.0e-6F) {
      return;
    }
    append_prism(triangles, base, tip, radius, radius * 0.55F, color);
  };
  if (weapon == "sword") {
    draw_weapon(
        "sword_blade_base_r", "sword_blade_tip_r", 0.028F, {0.78F, 0.80F, 0.86F});
  } else if (weapon == "spear") {
    draw_weapon(
        "spear_shaft_base_r", "spear_head_tip_r", 0.022F, {0.55F, 0.42F, 0.26F});
  } else if (weapon == "bone") {

    auto const hand_index = static_cast<std::size_t>(Bone::HandR);
    if (hand_index < palette.size()) {
      QVector3D const origin = palette[hand_index].column(3).toVector3D();
      QVector3D const axis = palette[hand_index].column(1).toVector3D();
      append_prism(triangles,
                   origin,
                   origin + axis * 0.92F,
                   0.028F,
                   0.014F,
                   {0.78F, 0.80F, 0.86F});
    }
  }

  RasterSettings settings;
  settings.width = width;
  settings.height = height;
  settings.clear_color = QColor(24, 27, 33);
  settings.backface_cull = false;
  SoftwareRasterizer rasterizer(settings);
  rasterizer.set_view_projection(view_matrix(view, width, height));
  for (auto const& tri : triangles) {
    rasterizer.submit(tri);
  }
  out = rasterizer.render();
}

auto usage() -> int {
  std::cout
      << "usage:\n"
         "  humanoid_preview --bpat <file.bpat> --list\n"
         "  humanoid_preview --bpat <file.bpat> --clip <name> [--frames N]\n"
         "                   [--view side|front|iso|top] [--weapon sword|spear|none]\n"
         "                   [--out strip.png] [--report]\n"
         "\n"
         "Renders baked humanoid clips as a phase strip so walk, run and weapon\n"
         "swings can be reviewed frame by frame, and reports per-frame bone\n"
         "stretch so a pose that pulls a limb past its bind length is caught.\n";
  return 2;
}

} // namespace

auto main(int argc, char** argv) -> int {
  QGuiApplication app(argc, argv);

  std::string bpat_path = "assets/creatures/humanoid_sword.bpat";
  std::string clip_name;
  std::string view = "side";
  std::string weapon;
  std::string out_path;
  int frames = 10;
  bool list_clips = false;
  bool report = false;

  for (int i = 1; i < argc; ++i) {
    std::string_view const arg = argv[i];
    auto next = [&]() -> std::string {
      return (i + 1 < argc) ? std::string(argv[++i]) : std::string();
    };
    if (arg == "--bpat") {
      bpat_path = next();
    } else if (arg == "--clip") {
      clip_name = next();
    } else if (arg == "--view") {
      view = next();
    } else if (arg == "--weapon") {
      weapon = next();
    } else if (arg == "--out") {
      out_path = next();
    } else if (arg == "--frames") {
      frames = std::max(1, std::atoi(next().c_str()));
    } else if (arg == "--list") {
      list_clips = true;
    } else if (arg == "--report") {
      report = true;
    } else {
      return usage();
    }
  }

  BpatBlob const blob = BpatBlob::from_file(bpat_path);
  if (!blob.loaded()) {
    std::cerr << "failed to load " << bpat_path << ": " << blob.last_error() << "\n";
    return 1;
  }

  if (list_clips || clip_name.empty()) {
    std::cout << bpat_path << ": species " << blob.species_id() << ", "
              << blob.bone_count() << " bones, " << blob.clip_count() << " clips\n";
    for (std::uint32_t i = 0; i < blob.clip_count(); ++i) {
      auto const clip = blob.clip(i);
      std::cout << "  " << clip.name << "  frames=" << clip.frame_count
                << " fps=" << clip.fps << (clip.loops ? " loop" : "") << "\n";
    }
    return list_clips ? 0 : usage();
  }

  if (weapon.empty()) {
    weapon = (clip_name.find("spear") != std::string::npos ||
              clip_name.find("bow") != std::string::npos)
                 ? "spear"
                 : "sword";
  }

  std::uint32_t const index = blob.clip_index(clip_name);
  if (index == BpatBlob::k_invalid_clip_index) {
    std::cerr << "clip not found: " << clip_name << " (try --list)\n";
    return 1;
  }
  auto const clip = blob.clip(index);
  frames = std::min<int>(frames, static_cast<int>(clip.frame_count));

  constexpr int k_tile_w = 340;
  constexpr int k_tile_h = 460;
  int const columns = std::min(frames, 6);
  int const rows = (frames + columns - 1) / columns;
  QImage strip(columns * k_tile_w, rows * k_tile_h, QImage::Format_RGB32);
  strip.fill(QColor(16, 18, 22));
  QPainter painter(&strip);
  painter.setPen(QColor(210, 216, 226));

  std::vector<FrameMetrics> metrics;
  metrics.reserve(static_cast<std::size_t>(frames));

  for (int i = 0; i < frames; ++i) {
    float const phase =
        (frames > 1) ? static_cast<float>(i) / static_cast<float>(frames - 1) : 0.0F;
    auto local_frame = static_cast<std::uint32_t>(
        std::lround(phase * static_cast<float>(clip.frame_count - 1U)));
    local_frame = std::min(local_frame, clip.frame_count - 1U);
    std::uint32_t const global_frame = clip.frame_offset + local_frame;

    QImage tile;
    render_frame(blob, global_frame, view, weapon, k_tile_w, k_tile_h, tile);
    painter.drawImage((i % columns) * k_tile_w, (i / columns) * k_tile_h, tile);
    painter.drawText(
        (i % columns) * k_tile_w + 8,
        (i / columns) * k_tile_h + 18,
        QString::asprintf("%.2f  f%u", static_cast<double>(phase), local_frame));

    metrics.push_back(measure(blob.frame_palette_view(global_frame), phase));
  }
  painter.end();

  if (out_path.empty()) {
    out_path = clip_name + "_" + view + ".png";
  }
  std::filesystem::path const out_fs(out_path);
  if (out_fs.has_parent_path()) {
    std::filesystem::create_directories(out_fs.parent_path());
  }
  if (!strip.save(QString::fromStdString(out_path))) {
    std::cerr << "failed to write " << out_path << "\n";
    return 1;
  }
  std::cout << "wrote " << out_path << " (" << frames << " frames of " << clip_name
            << ")\n";

  if (report) {
    std::cout << "phase\tstretch\tworst\t\treach_l\treach_r\tfoot_y\tfoot_l_z\tfoot_r_"
                 "z\tup_y\tblade_axis\n";
    float worst_stretch = 0.0F;
    for (auto const& m : metrics) {
      std::cout
          << QString::asprintf(
                 "%.2f\t%.2f\t%-12s\t%.2f\t%.2f\t%+.3f\t%+.3f\t\t%+.3f\t\t%.2f\t(%+."
                 "2f,%+.2f,%+.2f)",
                 static_cast<double>(m.phase),
                 static_cast<double>(m.max_bone_stretch),
                 m.worst_bone,
                 static_cast<double>(m.arm_reach_l),
                 static_cast<double>(m.arm_reach_r),
                 static_cast<double>(m.lowest_foot_y),
                 static_cast<double>(m.foot_l_z),
                 static_cast<double>(m.foot_r_z),
                 static_cast<double>(m.torso_up_y),
                 static_cast<double>(m.hand_r_axis.x()),
                 static_cast<double>(m.hand_r_axis.y()),
                 static_cast<double>(m.hand_r_axis.z()))
                 .toStdString()
          << "\n";
      worst_stretch = std::max(worst_stretch, m.max_bone_stretch);
    }
    std::cout << "worst bone stretch: " << worst_stretch << "x bind length\n";

    auto const stride = summarize_stride(blob, clip);
    std::cout << QString::asprintf(
                     "stride: foot travel %.3f m, contact retreat %.3f m/cycle, "
                     "foot lift %.3f m, hand swing %.3f m, pelvis bob %.3f m, "
                     "pelvis sway %.3f m, shoulder twist %.3f m, step width %.3f m\n",
                     static_cast<double>(stride.foot_travel),
                     static_cast<double>(stride.planted_travel_per_cycle),
                     static_cast<double>(stride.foot_lift),
                     static_cast<double>(stride.hand_swing),
                     static_cast<double>(stride.pelvis_bob),
                     static_cast<double>(stride.pelvis_sway),
                     static_cast<double>(stride.shoulder_twist),
                     static_cast<double>(stride.step_width))
                     .toStdString();
    for (float const cycle_time : {0.70F, 0.85F, 1.00F}) {
      std::cout << QString::asprintf(
                       "  at cycle_time %.2fs this clip is skate-free at %.2f m/s\n",
                       static_cast<double>(cycle_time),
                       static_cast<double>(stride.planted_travel_per_cycle /
                                           cycle_time))
                       .toStdString();
    }
  }

  return 0;
}
