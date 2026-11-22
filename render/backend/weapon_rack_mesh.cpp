#include "render/backend/weapon_rack_mesh.h"

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

#include "render/backend/prop_mesh_builder.h"
#include "render/backend/weapon_rack_mesh_internal.h"
#include "render/backend/weapon_rack_parts.h"

namespace Render::GL::BackendPipelines {

using namespace weapon_rack_mesh_detail;
namespace M = WeaponRackMaterial;
using V3 = QVector3D;
using namespace WeaponRackParts;

auto build_weapon_rack_mesh() -> WeaponRackMeshData {
  WeaponRackMeshData mesh;
  RackBuilder rack(mesh);
  int part = 0;

  for (const PropBoxPart& p : k_weapon_rack_boxes) {
    rack.box(V3(p.lo.x, p.lo.y, p.lo.z),
             V3(p.hi.x, p.hi.y, p.hi.z),
             M::k_oak,
             seed_of(part++));
  }
  for (const PropBeamPart& p : k_weapon_rack_beams) {
    rack.beam(V3(p.a.x, p.a.y, p.a.z),
              V3(p.b.x, p.b.y, p.b.z),
              p.half_width,
              p.half_depth,
              M::k_oak,
              seed_of(part++));
  }
  for (const PropTaperPart& p : k_weapon_rack_tapers) {
    rack.taper(p, M::k_oak, seed_of(part++));
  }

  for (float const sx : {-1.0F, 1.0F}) {
    float const x = sx * k_upright_x;
    for (float const y : {1.425F, 0.100F}) {
      rack.box(V3(x - 0.057F, y, -0.067F),
               V3(x + 0.057F, y + 0.048F, 0.047F),
               M::k_iron,
               seed_of(part++));
    }
    rack.box(V3(x - 0.057F, 0.955F, -0.067F),
             V3(x + 0.057F, 0.975F, 0.047F),
             M::k_iron,
             seed_of(part++));
    rack.nail(x, 1.510F, k_top_cap_front_z, seed_of(part++));
    rack.nail(x, 1.552F, k_top_cap_front_z, seed_of(part++));
    rack.nail(x, 1.020F, 0.040F, seed_of(part++));
    rack.nail(x, 0.240F, 0.255F, seed_of(part++));
  }

  for (float const x : {-0.545F, -0.395F, -0.175F, 0.125F, 0.310F, 0.505F, 0.665F}) {
    rack.peg(V3(x, 1.534F, 0.050F), V3(x, 1.548F, 0.140F), seed_of(part++));
  }

  auto shaft_top = [](float x) {
    return V3(x, k_top_cap_y, 0.085F);
  };
  auto shaft_foot = [](float x) {
    return V3(x, k_tray_top_y, 0.310F);
  };

  rack.hasta(shaft_foot(-0.600F), shaft_top(-0.612F), 2.05F, seed_of(part++));
  rack.pilum(shaft_foot(-0.470F), shaft_top(-0.462F), 1.98F, seed_of(part++));
  rack.hasta(shaft_foot(-0.330F), shaft_top(-0.296F), 1.92F, seed_of(part++));
  rack.pilum(shaft_foot(0.060F), shaft_top(0.048F), 1.98F, seed_of(part++));
  rack.hasta(shaft_foot(0.190F), shaft_top(0.222F), 2.08F, seed_of(part++));
  rack.hasta(shaft_foot(0.580F), shaft_top(0.598F), 1.96F, seed_of(part++));

  {
    V3 const tip(0.400F, 0.045F, 0.325F);
    V3 const guard(0.430F, 0.800F, 0.125F);
    float const seed = seed_of(part++);
    rack.gladius_blade(guard, tip, V3(1.0F, 0.0F, 0.0F), seed);
    rack.gladius_hilt(guard, guard - tip, V3(1.0F, 0.0F, 0.0F), seed);
  }

  {
    V3 const top(-0.060F, 0.900F, 0.100F);
    V3 const bottom(-0.085F, 0.300F, 0.115F);
    V3 const down = unit(bottom - top, V3(0.0F, -1.0F, 0.0F));
    float const length = (bottom - top).length();
    float const seed = seed_of(part++);
    V3 const flat(1.0F, 0.0F, 0.0F);
    rack.lathe(top,
               down,
               {{0.000F, 0.060F, 0.021F},
                {0.200F, 0.056F, 0.020F},
                {0.420F, 0.058F, 0.019F},
                {length - 0.090F, 0.042F, 0.016F},
                {length - 0.020F, 0.020F, 0.012F},
                {length, 0.000F, 0.000F}},
               12,
               false,
               flat,
               M::k_leather,
               seed,
               true);
    for (float const h : {0.000F, 0.150F, 0.290F}) {
      rack.lathe(top + down * h,
                 down,
                 {{0.000F, 0.064F, 0.025F},
                  {0.006F, 0.066F, 0.026F},
                  {0.030F, 0.066F, 0.026F},
                  {0.036F, 0.064F, 0.025F}},
                 12,
                 false,
                 flat,
                 M::k_bronze,
                 seed + 0.07F * h);
    }
    rack.lathe(top + down * (length - 0.130F),
               down,
               {{0.000F, 0.050F, 0.022F},
                {0.012F, 0.052F, 0.023F},
                {0.110F, 0.032F, 0.018F},
                {0.132F, 0.018F, 0.016F},
                {0.140F, 0.000F, 0.000F}},
               12,
               false,
               flat,
               M::k_bronze,
               seed + 0.19F);
    for (float const h : {0.020F, 0.170F}) {
      for (float const sx : {-1.0F, 1.0F}) {
        V3 const ring = top + down * h + flat * (sx * 0.070F);
        rack.tube(ring - down * 0.012F,
                  ring + down * 0.012F,
                  0.0085F,
                  0.0085F,
                  8,
                  M::k_bronze,
                  seed + 0.3F);
      }
    }
    rack.gladius_hilt(top, -down, flat, seed + 0.5F);

    V3 const peg_tip(-0.175F, 1.046F, 0.120F);
    rack.peg(V3(-0.175F, 1.032F, 0.030F), peg_tip, seed_of(part++));
    std::array<Ring, 4> const upper{
        {{top + down * 0.02F + flat * -0.078F, 0.013F, 0.0025F},
         {V3(-0.155F, 0.960F, 0.112F), 0.013F, 0.0025F},
         {V3(-0.172F, 1.040F, 0.112F), 0.013F, 0.0025F},
         {V3(-0.178F, 1.064F, 0.090F), 0.013F, 0.0025F}}};
    rack.sweep(upper, 6, false, V3(0.0F, 0.0F, 1.0F), M::k_leather, seed + 0.61F);
    std::array<Ring, 4> const lower{
        {{top + down * 0.17F + flat * 0.078F, 0.012F, 0.0025F},
         {V3(-0.020F, 0.880F, 0.145F), 0.012F, 0.0025F},
         {V3(-0.130F, 1.010F, 0.140F), 0.012F, 0.0025F},
         {V3(-0.168F, 1.062F, 0.100F), 0.012F, 0.0025F}}};
    rack.sweep(lower, 6, false, V3(0.0F, 0.0F, 1.0F), M::k_leather, seed + 0.67F);
  }

  {
    V3 const base(-0.740F, 1.598F, -0.010F);
    V3 const up = unit(V3(0.06F, 1.0F, 0.04F), V3(0.0F, 1.0F, 0.0F));
    float const seed = seed_of(part++);
    rack.lathe(base,
               up,
               {{0.000F, 0.112F, 0.120F},
                {0.008F, 0.116F, 0.124F},
                {0.014F, 0.104F, 0.112F},
                {0.030F, 0.100F, 0.107F},
                {0.080F, 0.097F, 0.104F},
                {0.120F, 0.085F, 0.091F},
                {0.155F, 0.062F, 0.067F},
                {0.178F, 0.036F, 0.039F},
                {0.190F, 0.018F, 0.019F},
                {0.194F, 0.014F, 0.014F},
                {0.202F, 0.018F, 0.018F},
                {0.216F, 0.018F, 0.018F},
                {0.228F, 0.010F, 0.010F},
                {0.234F, 0.000F, 0.000F}},
               18,
               false,
               V3(1.0F, 0.0F, 0.0F),
               M::k_bronze,
               seed);
    for (float const sz : {-1.0F, 1.0F}) {
      V3 const hinge = base + V3(0.005F, 0.012F, sz * 0.100F);
      rack.beam(hinge,
                hinge + V3(0.018F, -0.110F, sz * -0.006F),
                0.0045F,
                0.036F,
                M::k_bronze,
                seed + 0.2F);
    }
  }

  rack.scutum(shield_frame(V3(-0.500F, 0.513F, 0.346F), 0.170F, 0.20F),
              0.50F,
              0.62F,
              0.50F,
              0.020F,
              0.12F + 0.3F * seed_of(part++));
  rack.parma(shield_frame(V3(0.550F, 0.300F, 0.400F), 0.349F, -0.26F),
             0.290F,
             0.040F,
             0.018F,
             0.62F + 0.3F * seed_of(part++));

  {
    float const seed = seed_of(part++);
    std::vector<Ring> limbs;
    constexpr int k_steps = 24;
    for (int i = 0; i <= k_steps; ++i) {
      float const t = -1.0F + 2.0F * static_cast<float>(i) / k_steps;
      float const at = std::abs(t);
      float const recurve = std::clamp((at - 0.78F) / 0.22F, 0.0F, 1.0F);
      float const x = 0.300F + 0.190F * (1.0F - t * t) +
                      0.060F * recurve * recurve * (3.0F - 2.0F * recurve);
      float const r = 0.024F - 0.013F * std::pow(at, 1.3F);
      limbs.push_back({V3(x, 0.950F + 0.660F * t, -0.150F), r * 1.35F, r * 0.78F});
    }
    rack.sweep(limbs, 10, false, V3(0.0F, 0.0F, 1.0F), M::k_yew, seed);
    V3 const grip = limbs[k_steps / 2].center;
    rack.tube(grip - V3(0.0F, 0.070F, 0.0F),
              grip + V3(0.0F, 0.070F, 0.0F),
              0.0265F,
              0.0265F,
              10,
              M::k_leather,
              seed + 0.3F,
              V3(0.0F, 0.0F, 1.0F));
    V3 const nock_low = limbs.front().center;
    V3 const nock_high = limbs.back().center;
    rack.tube(nock_low, nock_high, 0.0035F, 0.0035F, 5, M::k_linen, seed + 0.5F);
    rack.peg(V3(0.360F, 1.534F, -0.075F), V3(0.360F, 1.548F, -0.175F), seed_of(part++));
  }

  {
    float const seed = seed_of(part++);
    V3 const foot(0.620F, 0.520F, -0.130F);
    V3 const mouth(0.650F, 1.180F, -0.136F);
    V3 const axis = unit(mouth - foot, V3(0.0F, 1.0F, 0.0F));
    float const length = (mouth - foot).length();
    rack.lathe(foot,
               axis,
               {{0.000F, 0.000F, 0.000F},
                {0.004F, 0.036F, 0.030F},
                {0.030F, 0.048F, 0.040F},
                {length * 0.5F, 0.052F, 0.043F},
                {length, 0.056F, 0.047F}},
               12,
               false,
               V3(1.0F, 0.0F, 0.0F),
               M::k_leather,
               seed);
    for (float const h : {0.050F, length - 0.030F}) {
      rack.lathe(foot + axis * h,
                 axis,
                 {{0.000F, 0.054F, 0.045F}, {0.026F, 0.058F, 0.049F}},
                 12,
                 false,
                 V3(1.0F, 0.0F, 0.0F),
                 M::k_bronze,
                 seed + 0.1F);
    }
    std::array<V3, 6> const arrows{{V3(-0.020F, 0.0F, -0.014F),
                                    V3(0.018F, 0.0F, -0.018F),
                                    V3(0.000F, 0.0F, 0.012F),
                                    V3(-0.026F, 0.0F, 0.016F),
                                    V3(0.026F, 0.0F, 0.010F),
                                    V3(0.004F, 0.0F, -0.004F)}};
    int arrow = 0;
    for (const V3& offset : arrows) {
      float const lean = 0.010F * static_cast<float>(arrow % 3 - 1);
      V3 const low = mouth + offset - axis * 0.220F;
      V3 const high = mouth + offset * 1.5F + V3(lean, 0.0F, 0.0F) +
                      axis * (0.150F + 0.012F * static_cast<float>(arrow % 2));
      V3 const shaft = unit(high - low, axis);
      float const arrow_seed = seed + 0.07F * static_cast<float>(arrow);
      rack.tube(low, high, 0.0048F, 0.0048F, 5, M::k_ash, arrow_seed);
      rack.lathe(high,
                 shaft,
                 {{0.000F, 0.0055F, 0.0055F},
                  {0.012F, 0.0050F, 0.0050F},
                  {0.014F, 0.0F, 0.0F}},
                 5,
                 false,
                 V3(1.0F, 0.0F, 0.0F),
                 M::k_bone,
                 arrow_seed);
      V3 const vane_dir = unit(V3::crossProduct(shaft, offset + V3(0.001F, 0.0F, 0.0F)),
                               V3(1.0F, 0.0F, 0.0F));
      for (float const s : {-1.0F, 1.0F}) {
        V3 const root = high - shaft * 0.105F + vane_dir * (s * 0.010F);
        rack.beam(root,
                  root + shaft * 0.085F + vane_dir * (s * 0.004F),
                  0.0012F,
                  0.0080F,
                  M::k_feather,
                  arrow_seed + 0.5F);
      }
      ++arrow;
    }
    rack.peg(V3(0.620F, 1.534F, -0.075F), V3(0.620F, 1.548F, -0.160F), seed_of(part++));
    std::array<Ring, 5> const strap{
        {{mouth + V3(-0.050F, -0.020F, 0.0F), 0.013F, 0.0025F},
         {V3(0.590F, 1.340F, -0.152F), 0.013F, 0.0025F},
         {V3(0.618F, 1.562F, -0.150F), 0.013F, 0.0025F},
         {V3(0.650F, 1.340F, -0.156F), 0.013F, 0.0025F},
         {mouth + V3(0.052F, -0.020F, 0.0F), 0.013F, 0.0025F}}};
    rack.sweep(strap, 6, false, V3(0.0F, 0.0F, 1.0F), M::k_leather, seed + 0.8F);
  }

  mesh.surface.resize(mesh.vertices.size());
  return mesh;
}

} // namespace Render::GL::BackendPipelines
