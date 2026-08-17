#include "showcase_pose_manifest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>

#include "rig/pose_fk.h"

namespace Animation {

namespace {

using PoseFk::add;
using PoseFk::identity;
using PoseFk::limb_segments;
using PoseFk::LimbSegments;
using PoseFk::Mat3;
using PoseFk::multiply;
using PoseFk::radians;
using PoseFk::ramp;
using PoseFk::rot_x;
using PoseFk::rot_y;
using PoseFk::rot_z;
using PoseFk::scaled;
using PoseFk::smoothstep;
using PoseFk::sub;
using PoseFk::transform;
using ShowcaseLimbAim = PoseFk::LimbAim;

struct ShowcaseKey {
  float t{0.0F};
  float root_x{0.0F};
  float root_y{0.0F};
  float root_z{0.0F};
  float body_pitch{0.0F};
  float body_roll{0.0F};
  float body_yaw{0.0F};
  float spine_pitch{0.0F};
  float spine_roll{0.0F};
  float spine_yaw{0.0F};
  float head_pitch{0.0F};
  float blade_pitch{0.0F};
  float blade_yaw{0.0F};
  float blade_amount{0.0F};
  ShowcaseLimbAim arm_l{};
  ShowcaseLimbAim arm_r{};
  ShowcaseLimbAim leg_l{};
  ShowcaseLimbAim leg_r{};
};

constexpr std::array<ShowcaseKey, 8> k_jump_keys{{
    {0.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
    {0.16F,
     0.0F,
     0.74F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     20.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {-42.0F, 9.0F, 0.0F, 30.0F},
     {-42.0F, 9.0F, 0.0F, 30.0F},
     {40.0F, 7.0F, 0.0F, 80.0F},
     {40.0F, 7.0F, 0.0F, 80.0F}},
    {0.28F,
     0.0F,
     1.06F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     -6.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {132.0F, 14.0F, 0.0F, 20.0F},
     {132.0F, 14.0F, 0.0F, 20.0F},
     {6.0F, 4.0F, 0.0F, 12.0F},
     {6.0F, 4.0F, 0.0F, 12.0F}},
    {0.44F,
     0.0F,
     1.7F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     -12.0F,
     0.0F,
     0.0F,
     -8.0F,
     0.0F,
     0.0F,
     0.0F,
     {170.0F, 20.0F, 0.0F, 12.0F},
     {170.0F, 20.0F, 0.0F, 12.0F},
     {48.0F, 9.0F, 0.0F, 94.0F},
     {34.0F, 7.0F, 0.0F, 76.0F}},
    {0.58F,
     0.0F,
     1.48F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {130.0F, 18.0F, 0.0F, 24.0F},
     {130.0F, 18.0F, 0.0F, 24.0F},
     {24.0F, 6.0F, 0.0F, 44.0F},
     {18.0F, 6.0F, 0.0F, 38.0F}},
    {0.7F,
     0.0F,
     1.02F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     10.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {52.0F, 17.0F, 0.0F, 32.0F},
     {52.0F, 17.0F, 0.0F, 32.0F},
     {10.0F, 6.0F, 0.0F, 18.0F},
     {10.0F, 6.0F, 0.0F, 18.0F}},
    {0.8F,
     0.0F,
     0.76F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     26.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {62.0F, 19.0F, 0.0F, 48.0F},
     {62.0F, 19.0F, 0.0F, 48.0F},
     {42.0F, 9.0F, 0.0F, 84.0F},
     {42.0F, 9.0F, 0.0F, 84.0F}},
    {1.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
}};

constexpr std::array<ShowcaseKey, 9> k_front_flip_keys{{
    {0.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
    {0.12F,
     0.0F,
     0.74F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     24.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {-48.0F, 10.0F, 0.0F, 26.0F},
     {-48.0F, 10.0F, 0.0F, 26.0F},
     {40.0F, 7.0F, 0.0F, 80.0F},
     {40.0F, 7.0F, 0.0F, 80.0F}},
    {0.24F,
     0.0F,
     1.12F,
     0.0F,
     32.0F,
     0.0F,
     0.0F,
     4.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {150.0F, 13.0F, 0.0F, 16.0F},
     {150.0F, 13.0F, 0.0F, 16.0F},
     {4.0F, 4.0F, 0.0F, 10.0F},
     {4.0F, 4.0F, 0.0F, 10.0F}},
    {0.36F,
     0.0F,
     1.56F,
     0.0F,
     120.0F,
     0.0F,
     0.0F,
     30.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {62.0F, 16.0F, 0.0F, 118.0F},
     {62.0F, 16.0F, 0.0F, 118.0F},
     {100.0F, 10.0F, 0.0F, 134.0F},
     {100.0F, 8.0F, 0.0F, 134.0F}},
    {0.48F,
     0.0F,
     1.72F,
     0.0F,
     206.0F,
     0.0F,
     0.0F,
     38.0F,
     0.0F,
     0.0F,
     14.0F,
     0.0F,
     0.0F,
     0.0F,
     {56.0F, 18.0F, 0.0F, 130.0F},
     {56.0F, 18.0F, 0.0F, 130.0F},
     {114.0F, 12.0F, 0.0F, 146.0F},
     {114.0F, 9.0F, 0.0F, 146.0F}},
    {0.6F,
     0.0F,
     1.52F,
     0.0F,
     288.0F,
     0.0F,
     0.0F,
     24.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {76.0F, 18.0F, 0.0F, 100.0F},
     {76.0F, 18.0F, 0.0F, 100.0F},
     {88.0F, 10.0F, 0.0F, 108.0F},
     {88.0F, 8.0F, 0.0F, 108.0F}},
    {0.72F,
     0.0F,
     1.14F,
     0.0F,
     340.0F,
     0.0F,
     0.0F,
     8.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {34.0F, 22.0F, 0.0F, 48.0F},
     {34.0F, 22.0F, 0.0F, 48.0F},
     {40.0F, 8.0F, 0.0F, 54.0F},
     {40.0F, 7.0F, 0.0F, 54.0F}},
    {0.83F,
     0.0F,
     0.76F,
     0.0F,
     360.0F,
     0.0F,
     0.0F,
     26.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {66.0F, 20.0F, 0.0F, 44.0F},
     {66.0F, 20.0F, 0.0F, 44.0F},
     {42.0F, 9.0F, 0.0F, 84.0F},
     {42.0F, 9.0F, 0.0F, 84.0F}},
    {1.0F,
     0.0F,
     0.975F,
     0.0F,
     360.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
}};

constexpr std::array<ShowcaseKey, 11> k_handstand_keys{{
    {0.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
    {0.09F,
     0.0F,
     0.93F,
     0.1F,
     0.0F,
     0.0F,
     0.0F,
     46.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {104.0F, 12.0F, 0.0F, 22.0F},
     {104.0F, 12.0F, 0.0F, 22.0F},
     {4.0F, 6.0F, 0.0F, 22.0F},
     {30.0F, 6.0F, 0.0F, 44.0F}},
    {0.2F,
     0.0F,
     0.66F,
     0.36F,
     82.0F,
     0.0F,
     0.0F,
     6.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {82.0F, 14.0F, 0.0F, 14.0F},
     {82.0F, 14.0F, 0.0F, 14.0F},
     {-46.0F, 7.0F, 0.0F, 16.0F},
     {44.0F, 7.0F, 0.0F, 62.0F}},
    {0.31F,
     0.0F,
     0.96F,
     0.42F,
     152.0F,
     0.0F,
     0.0F,
     -4.0F,
     0.0F,
     0.0F,
     -14.0F,
     0.0F,
     0.0F,
     0.0F,
     {152.0F, 13.0F, 0.0F, 8.0F},
     {152.0F, 13.0F, 0.0F, 8.0F},
     {-30.0F, 8.0F, 0.0F, 8.0F},
     {22.0F, 8.0F, 0.0F, 16.0F}},
    {0.42F,
     0.0F,
     1.042F,
     0.44F,
     182.0F,
     0.0F,
     0.0F,
     -4.0F,
     0.0F,
     0.0F,
     -18.0F,
     0.0F,
     0.0F,
     0.0F,
     {182.0F, 12.0F, 0.0F, 4.0F},
     {182.0F, 12.0F, 0.0F, 4.0F},
     {-5.0F, 9.0F, 0.0F, 4.0F},
     {5.0F, 9.0F, 0.0F, 4.0F}},
    {0.55F,
     0.0F,
     1.046F,
     0.44F,
     178.0F,
     0.0F,
     0.0F,
     -9.0F,
     0.0F,
     0.0F,
     -18.0F,
     0.0F,
     0.0F,
     0.0F,
     {178.0F, 12.0F, 0.0F, 5.0F},
     {178.0F, 12.0F, 0.0F, 5.0F},
     {-36.0F, 8.0F, 0.0F, 5.0F},
     {34.0F, 8.0F, 0.0F, 5.0F}},
    {0.68F,
     0.0F,
     1.044F,
     0.44F,
     184.0F,
     0.0F,
     0.0F,
     -3.0F,
     0.0F,
     0.0F,
     -16.0F,
     0.0F,
     0.0F,
     0.0F,
     {184.0F, 12.0F, 0.0F, 4.0F},
     {184.0F, 12.0F, 0.0F, 4.0F},
     {-6.0F, 8.0F, 0.0F, 5.0F},
     {6.0F, 8.0F, 0.0F, 5.0F}},
    {0.79F,
     0.0F,
     0.92F,
     0.4F,
     134.0F,
     0.0F,
     0.0F,
     12.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {132.0F, 15.0F, 0.0F, 16.0F},
     {132.0F, 15.0F, 0.0F, 16.0F},
     {104.0F, 7.0F, 0.0F, 44.0F},
     {118.0F, 7.0F, 0.0F, 58.0F}},
    {0.88F,
     0.0F,
     0.85F,
     0.16F,
     62.0F,
     0.0F,
     0.0F,
     28.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {98.0F, 15.0F, 0.0F, 26.0F},
     {98.0F, 15.0F, 0.0F, 26.0F},
     {78.0F, 7.0F, 0.0F, 64.0F},
     {88.0F, 7.0F, 0.0F, 74.0F}},
    {0.94F,
     0.0F,
     0.83F,
     0.06F,
     14.0F,
     0.0F,
     0.0F,
     32.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {54.0F, 14.0F, 0.0F, 34.0F},
     {54.0F, 14.0F, 0.0F, 34.0F},
     {44.0F, 7.0F, 0.0F, 70.0F},
     {48.0F, 7.0F, 0.0F, 76.0F}},
    {1.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
}};

constexpr std::array<ShowcaseKey, 8> k_side_aerial_keys{{
    {0.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
    {0.13F,
     -0.1F,
     0.78F,
     0.0F,
     0.0F,
     10.0F,
     0.0F,
     0.0F,
     -18.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {140.0F, 44.0F, 0.0F, 18.0F},
     {120.0F, 20.0F, 0.0F, 26.0F},
     {32.0F, 20.0F, 0.0F, 70.0F},
     {36.0F, 8.0F, 0.0F, 76.0F}},
    {0.26F,
     -0.52F,
     1.32F,
     0.0F,
     0.0F,
     64.0F,
     0.0F,
     0.0F,
     -8.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {168.0F, 34.0F, 0.0F, 10.0F},
     {166.0F, 22.0F, 0.0F, 12.0F},
     {-30.0F, 30.0F, 0.0F, 14.0F},
     {26.0F, 26.0F, 0.0F, 40.0F}},
    {0.42F,
     -1.1F,
     1.64F,
     0.0F,
     0.0F,
     158.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {174.0F, 30.0F, 0.0F, 6.0F},
     {174.0F, 30.0F, 0.0F, 6.0F},
     {-26.0F, 40.0F, 0.0F, 8.0F},
     {22.0F, 40.0F, 0.0F, 8.0F}},
    {0.58F,
     -1.7F,
     1.56F,
     0.0F,
     0.0F,
     246.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {168.0F, 26.0F, 0.0F, 12.0F},
     {170.0F, 34.0F, 0.0F, 10.0F},
     {18.0F, 34.0F, 0.0F, 22.0F},
     {-14.0F, 34.0F, 0.0F, 16.0F}},
    {0.72F,
     -2.2F,
     1.14F,
     0.0F,
     0.0F,
     316.0F,
     0.0F,
     0.0F,
     6.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {132.0F, 30.0F, 0.0F, 24.0F},
     {138.0F, 32.0F, 0.0F, 22.0F},
     {24.0F, 18.0F, 0.0F, 36.0F},
     {2.0F, 20.0F, 0.0F, 28.0F}},
    {0.84F,
     -2.56F,
     0.78F,
     0.0F,
     0.0F,
     356.0F,
     0.0F,
     0.0F,
     8.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {84.0F, 32.0F, 0.0F, 34.0F},
     {84.0F, 32.0F, 0.0F, 34.0F},
     {42.0F, 12.0F, 0.0F, 84.0F},
     {40.0F, 12.0F, 0.0F, 80.0F}},
    {1.0F,
     -2.62F,
     0.975F,
     0.0F,
     0.0F,
     360.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
}};

constexpr std::array<ShowcaseKey, 9> k_sword_flourish_keys{{
    {0.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     56.0F,
     -22.0F,
     1.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {10.0F, 13.0F, 0.0F, 48.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
    {0.11F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     -6.0F,
     0.0F,
     -30.0F,
     0.0F,
     128.0F,
     -52.0F,
     1.0F,
     {-16.0F, 22.0F, 0.0F, 30.0F},
     {-56.0F, 38.0F, -26.0F, 54.0F},
     {6.0F, 8.0F, 0.0F, 28.0F},
     {26.0F, 8.0F, 0.0F, 44.0F}},
    {0.24F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     12.0F,
     0.0F,
     36.0F,
     0.0F,
     94.0F,
     88.0F,
     1.0F,
     {26.0F, 30.0F, 0.0F, 46.0F},
     {88.0F, 44.0F, 52.0F, 14.0F},
     {2.0F, 10.0F, 0.0F, 26.0F},
     {32.0F, 10.0F, 0.0F, 52.0F}},
    {0.36F,
     0.0F,
     0.995F,
     0.0F,
     0.0F,
     0.0F,
     -30.0F,
     -16.0F,
     0.0F,
     6.0F,
     0.0F,
     178.0F,
     8.0F,
     1.0F,
     {42.0F, 26.0F, 0.0F, 54.0F},
     {166.0F, 16.0F, 0.0F, 20.0F},
     {12.0F, 8.0F, 0.0F, 28.0F},
     {18.0F, 8.0F, 0.0F, 34.0F}},
    {0.5F,
     0.0F,
     0.92F,
     0.0F,
     0.0F,
     0.0F,
     -186.0F,
     26.0F,
     0.0F,
     -18.0F,
     0.0F,
     44.0F,
     -4.0F,
     1.0F,
     {20.0F, 34.0F, 0.0F, 42.0F},
     {44.0F, 20.0F, -12.0F, 16.0F},
     {34.0F, 10.0F, 0.0F, 58.0F},
     {-6.0F, 10.0F, 0.0F, 40.0F}},
    {0.64F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     -324.0F,
     -6.0F,
     0.0F,
     24.0F,
     0.0F,
     134.0F,
     -68.0F,
     1.0F,
     {-8.0F, 28.0F, 0.0F, 36.0F},
     {-44.0F, 42.0F, -38.0F, 46.0F},
     {4.0F, 10.0F, 0.0F, 28.0F},
     {28.0F, 10.0F, 0.0F, 46.0F}},
    {0.77F,
     0.0F,
     0.93F,
     0.0F,
     0.0F,
     0.0F,
     -360.0F,
     28.0F,
     0.0F,
     -6.0F,
     0.0F,
     16.0F,
     14.0F,
     1.0F,
     {36.0F, 24.0F, 0.0F, 58.0F},
     {106.0F, 14.0F, 14.0F, 10.0F},
     {2.0F, 10.0F, 0.0F, 30.0F},
     {42.0F, 10.0F, 0.0F, 64.0F}},
    {0.89F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     -360.0F,
     6.0F,
     0.0F,
     -14.0F,
     0.0F,
     64.0F,
     -28.0F,
     1.0F,
     {8.0F, 20.0F, 0.0F, 36.0F},
     {28.0F, 16.0F, 0.0F, 66.0F},
     {10.0F, 8.0F, 0.0F, 26.0F},
     {20.0F, 8.0F, 0.0F, 38.0F}},
    {1.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     -360.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     56.0F,
     -22.0F,
     1.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {10.0F, 13.0F, 0.0F, 48.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
}};

constexpr std::array<ShowcaseKey, 8> k_spear_throw_keys{{
    {0.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     18.0F,
     4.0F,
     1.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 15.0F, 0.0F, 56.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
    {0.16F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     -8.0F,
     0.0F,
     28.0F,
     0.0F,
     60.0F,
     34.0F,
     1.0F,
     {66.0F, 20.0F, 0.0F, 34.0F},
     {-56.0F, 32.0F, 26.0F, 78.0F},
     {34.0F, 8.0F, 0.0F, 48.0F},
     {-4.0F, 8.0F, 0.0F, 34.0F}},
    {0.34F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     -18.0F,
     0.0F,
     38.0F,
     0.0F,
     76.0F,
     36.0F,
     1.0F,
     {94.0F, 18.0F, 0.0F, 24.0F},
     {-82.0F, 28.0F, 30.0F, 94.0F},
     {52.0F, 8.0F, 0.0F, 52.0F},
     {-16.0F, 8.0F, 0.0F, 26.0F}},
    {0.46F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     4.0F,
     0.0F,
     8.0F,
     0.0F,
     24.0F,
     12.0F,
     1.0F,
     {78.0F, 22.0F, 0.0F, 38.0F},
     {128.0F, 16.0F, 6.0F, 70.0F},
     {40.0F, 8.0F, 0.0F, 54.0F},
     {-10.0F, 8.0F, 0.0F, 30.0F}},
    {0.56F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     24.0F,
     0.0F,
     -14.0F,
     0.0F,
     -62.0F,
     -4.0F,
     1.0F,
     {10.0F, 26.0F, 0.0F, 42.0F},
     {120.0F, 12.0F, -6.0F, 14.0F},
     {14.0F, 8.0F, 0.0F, 40.0F},
     {6.0F, 8.0F, 0.0F, 44.0F}},
    {0.68F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     36.0F,
     0.0F,
     -24.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {-22.0F, 24.0F, 0.0F, 34.0F},
     {62.0F, 10.0F, -14.0F, 24.0F},
     {4.0F, 8.0F, 0.0F, 42.0F},
     {24.0F, 8.0F, 0.0F, 52.0F}},
    {0.82F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     18.0F,
     0.0F,
     -10.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {-12.0F, 18.0F, 0.0F, 28.0F},
     {26.0F, 12.0F, 0.0F, 40.0F},
     {10.0F, 6.0F, 0.0F, 34.0F},
     {20.0F, 6.0F, 0.0F, 40.0F}},
    {1.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
}};

constexpr std::array<ShowcaseKey, 5> k_rest_sit_keys{{
    {0.0F,
     0.0F,
     0.195F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     13.0F,
     0.0F,
     0.0F,
     7.0F,
     95.7F,
     115.2F,
     1.0F,
     {-16.0F, 45.0F, 45.0F, 65.0F},
     {-16.0F, 45.0F, -45.0F, 65.0F},
     {84.0F, -10.0F, -45.0F, 165.0F},
     {84.0F, -10.0F, 45.0F, 165.0F}},
    {0.260F,
     0.0F,
     0.201F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     11.0F,
     0.0F,
     -2.0F,
     5.4F,
     95.7F,
     115.2F,
     1.0F,
     {-16.0F, 45.0F, 45.0F, 65.0F},
     {-16.0F, 45.0F, -45.0F, 65.0F},
     {84.0F, -10.0F, -45.0F, 165.0F},
     {84.0F, -10.0F, 45.0F, 165.0F}},
    {0.480F,
     0.0F,
     0.192F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     14.2F,
     0.0F,
     0.0F,
     8.2F,
     95.7F,
     115.2F,
     1.0F,
     {-16.0F, 45.0F, 45.0F, 65.0F},
     {-16.0F, 45.0F, -45.0F, 65.0F},
     {84.0F, -10.0F, -45.0F, 165.0F},
     {84.0F, -10.0F, 45.0F, 165.0F}},
    {0.740F,
     0.0F,
     0.199F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     11.8F,
     0.0F,
     1.6F,
     6.0F,
     95.7F,
     115.2F,
     1.0F,
     {-16.0F, 45.0F, 45.0F, 65.0F},
     {-16.0F, 45.0F, -45.0F, 65.0F},
     {84.0F, -10.0F, -45.0F, 165.0F},
     {84.0F, -10.0F, 45.0F, 165.0F}},
    {1.0F,
     0.0F,
     0.195F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     13.0F,
     0.0F,
     0.0F,
     7.0F,
     95.7F,
     115.2F,
     1.0F,
     {-16.0F, 45.0F, 45.0F, 65.0F},
     {-16.0F, 45.0F, -45.0F, 65.0F},
     {84.0F, -10.0F, -45.0F, 165.0F},
     {84.0F, -10.0F, 45.0F, 165.0F}},
}};

constexpr std::array<ShowcaseKey, 5> k_rest_sit_knees_keys{{
    {0.0F,
     0.0F,
     0.195F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     13.0F,
     0.0F,
     0.0F,
     7.0F,
     161.8F,
     -126.5F,
     1.0F,
     {32.0F, 0.0F, 5.0F, 115.0F},
     {32.0F, 0.0F, -5.0F, 115.0F},
     {126.0F, -5.0F, -15.0F, 105.0F},
     {126.0F, -5.0F, 15.0F, 105.0F}},
    {0.280F,
     0.0F,
     0.201F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     11.2F,
     0.0F,
     -2.4F,
     5.2F,
     161.8F,
     -126.5F,
     1.0F,
     {32.0F, 0.0F, 5.0F, 115.0F},
     {32.0F, 0.0F, -5.0F, 115.0F},
     {126.0F, -5.0F, -15.0F, 105.0F},
     {126.0F, -5.0F, 15.0F, 105.0F}},
    {0.520F,
     0.0F,
     0.191F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     14.2F,
     0.0F,
     0.4F,
     8.4F,
     161.8F,
     -126.5F,
     1.0F,
     {32.0F, 0.0F, 5.0F, 115.0F},
     {32.0F, 0.0F, -5.0F, 115.0F},
     {126.0F, -5.0F, -15.0F, 105.0F},
     {126.0F, -5.0F, 15.0F, 105.0F}},
    {0.780F,
     0.0F,
     0.199F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     12.0F,
     0.0F,
     2.0F,
     6.0F,
     161.8F,
     -126.5F,
     1.0F,
     {32.0F, 0.0F, 5.0F, 115.0F},
     {32.0F, 0.0F, -5.0F, 115.0F},
     {126.0F, -5.0F, -15.0F, 105.0F},
     {126.0F, -5.0F, 15.0F, 105.0F}},
    {1.0F,
     0.0F,
     0.195F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     13.0F,
     0.0F,
     0.0F,
     7.0F,
     161.8F,
     -126.5F,
     1.0F,
     {32.0F, 0.0F, 5.0F, 115.0F},
     {32.0F, 0.0F, -5.0F, 115.0F},
     {126.0F, -5.0F, -15.0F, 105.0F},
     {126.0F, -5.0F, 15.0F, 105.0F}},
}};

constexpr std::array<ShowcaseKey, 5> k_rest_kneel_keys{{
    {0.0F,
     0.0F,
     0.460F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     20.0F,
     0.0F,
     0.0F,
     8.0F,
     0.0F,
     0.0F,
     0.0F,
     {-8.0F, 15.0F, 50.0F, 85.0F},
     {20.0F, 45.0F, -45.0F, 55.0F},
     {46.0F, 15.0F, 5.0F, 135.0F},
     {88.0F, -5.0F, 10.0F, 55.0F}},
    {0.240F,
     0.0F,
     0.463F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     23.0F,
     0.0F,
     0.0F,
     9.5F,
     0.0F,
     0.0F,
     0.0F,
     {-8.0F, 15.0F, 50.0F, 85.0F},
     {42.0F, 34.0F, -34.0F, 40.0F},
     {46.0F, 15.0F, 5.0F, 135.0F},
     {88.0F, -5.0F, 10.0F, 55.0F}},
    {0.460F,
     0.0F,
     0.456F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     26.0F,
     0.0F,
     0.0F,
     11.0F,
     0.0F,
     0.0F,
     0.0F,
     {-8.0F, 15.0F, 50.0F, 85.0F},
     {64.0F, 22.0F, -22.0F, 22.0F},
     {46.0F, 15.0F, 5.0F, 135.0F},
     {88.0F, -5.0F, 10.0F, 55.0F}},
    {0.7F,
     0.0F,
     0.462F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     23.0F,
     0.0F,
     0.0F,
     9.5F,
     0.0F,
     0.0F,
     0.0F,
     {-8.0F, 15.0F, 50.0F, 85.0F},
     {42.0F, 34.0F, -34.0F, 40.0F},
     {46.0F, 15.0F, 5.0F, 135.0F},
     {88.0F, -5.0F, 10.0F, 55.0F}},
    {1.0F,
     0.0F,
     0.460F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     20.0F,
     0.0F,
     0.0F,
     8.0F,
     0.0F,
     0.0F,
     0.0F,
     {-8.0F, 15.0F, 50.0F, 85.0F},
     {20.0F, 45.0F, -45.0F, 55.0F},
     {46.0F, 15.0F, 5.0F, 135.0F},
     {88.0F, -5.0F, 10.0F, 55.0F}},
}};

constexpr std::array<ShowcaseKey, 5> k_rest_sit_down_keys{{
    {0.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     1.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
    {0.280F,
     0.0F,
     0.760F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     18.0F,
     0.0F,
     0.0F,
     6.0F,
     30.0F,
     40.0F,
     1.0F,
     {22.0F, 14.0F, 10.0F, 44.0F},
     {22.0F, 14.0F, -10.0F, 44.0F},
     {52.0F, 2.0F, -14.0F, 96.0F},
     {52.0F, 2.0F, 14.0F, 96.0F}},
    {0.580F,
     0.0F,
     0.470F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     26.0F,
     0.0F,
     0.0F,
     9.0F,
     62.0F,
     78.0F,
     1.0F,
     {30.0F, 26.0F, 26.0F, 66.0F},
     {30.0F, 26.0F, -26.0F, 66.0F},
     {76.0F, -4.0F, -30.0F, 140.0F},
     {76.0F, -4.0F, 30.0F, 140.0F}},
    {0.820F,
     0.0F,
     0.250F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     19.0F,
     0.0F,
     0.0F,
     9.0F,
     86.0F,
     104.0F,
     1.0F,
     {-4.0F, 40.0F, 40.0F, 66.0F},
     {-4.0F, 40.0F, -40.0F, 66.0F},
     {82.0F, -9.0F, -42.0F, 158.0F},
     {82.0F, -9.0F, 42.0F, 158.0F}},
    {1.0F,
     0.0F,
     0.195F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     13.0F,
     0.0F,
     0.0F,
     7.0F,
     95.7F,
     115.2F,
     1.0F,
     {-16.0F, 45.0F, 45.0F, 65.0F},
     {-16.0F, 45.0F, -45.0F, 65.0F},
     {84.0F, -10.0F, -45.0F, 165.0F},
     {84.0F, -10.0F, 45.0F, 165.0F}},
}};

constexpr std::array<ShowcaseKey, 6> k_rest_sit_knees_down_keys{{
    {0.0F,
     0.0F,
     0.975F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     1.0F,
     {4.0F, 7.0F, 0.0F, 14.0F},
     {4.0F, 7.0F, 0.0F, 14.0F},
     {16.0F, 5.0F, 0.0F, 32.0F},
     {16.0F, 5.0F, 0.0F, 32.0F}},
    {0.120F,
     0.0F,
     0.905F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     10.0F,
     0.0F,
     0.0F,
     3.0F,
     20.0F,
     -16.0F,
     1.0F,
     {10.0F, 7.0F, 2.0F, 30.0F},
     {10.0F, 7.0F, -2.0F, 30.0F},
     {34.0F, 3.0F, -4.0F, 58.0F},
     {34.0F, 3.0F, 4.0F, 58.0F}},
    {0.260F,
     0.0F,
     0.780F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     20.0F,
     0.0F,
     0.0F,
     6.0F,
     44.0F,
     -34.0F,
     1.0F,
     {18.0F, 6.0F, 4.0F, 52.0F},
     {18.0F, 6.0F, -4.0F, 52.0F},
     {58.0F, 0.0F, -8.0F, 80.0F},
     {58.0F, 0.0F, 8.0F, 80.0F}},
    {0.560F,
     0.0F,
     0.5F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     27.0F,
     0.0F,
     0.0F,
     9.0F,
     92.0F,
     -72.0F,
     1.0F,
     {26.0F, 2.0F, 6.0F, 86.0F},
     {26.0F, 2.0F, -6.0F, 86.0F},
     {92.0F, -3.0F, -12.0F, 96.0F},
     {92.0F, -3.0F, 12.0F, 96.0F}},
    {0.820F,
     0.0F,
     0.290F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     18.0F,
     0.0F,
     0.0F,
     8.0F,
     134.0F,
     -106.0F,
     1.0F,
     {30.0F, 0.0F, 5.0F, 108.0F},
     {30.0F, 0.0F, -5.0F, 108.0F},
     {116.0F, -4.0F, -14.0F, 102.0F},
     {116.0F, -4.0F, 14.0F, 102.0F}},
    {1.0F,
     0.0F,
     0.195F,
     0.0F,
     0.0F,
     0.0F,
     0.0F,
     13.0F,
     0.0F,
     0.0F,
     7.0F,
     161.8F,
     -126.5F,
     1.0F,
     {32.0F, 0.0F, 5.0F, 115.0F},
     {32.0F, 0.0F, -5.0F, 115.0F},
     {126.0F, -5.0F, -15.0F, 105.0F},
     {126.0F, -5.0F, 15.0F, 105.0F}},
}};

[[nodiscard]] auto
keys_for(HumanoidShowcaseMove move) noexcept -> std::span<const ShowcaseKey> {
  switch (move) {
  case HumanoidShowcaseMove::Jump:
    return k_jump_keys;
  case HumanoidShowcaseMove::FrontFlip:
    return k_front_flip_keys;
  case HumanoidShowcaseMove::Handstand:
    return k_handstand_keys;
  case HumanoidShowcaseMove::SideAerial:
    return k_side_aerial_keys;
  case HumanoidShowcaseMove::SwordFlourish:
    return k_sword_flourish_keys;
  case HumanoidShowcaseMove::SpearThrow:
    return k_spear_throw_keys;
  case HumanoidShowcaseMove::RestSit:
    return k_rest_sit_keys;
  case HumanoidShowcaseMove::RestSitKnees:
    return k_rest_sit_knees_keys;
  case HumanoidShowcaseMove::RestKneel:
    return k_rest_kneel_keys;
  case HumanoidShowcaseMove::RestSitDown:
    return k_rest_sit_down_keys;
  case HumanoidShowcaseMove::RestSitKneesDown:
    return k_rest_sit_knees_down_keys;
  case HumanoidShowcaseMove::None:
  case HumanoidShowcaseMove::Count:
    break;
  }
  return {};
}

[[nodiscard]] auto blend(float a, float b, float u) noexcept -> float {
  return a + ((b - a) * u);
}

[[nodiscard]] auto blend_aim(const ShowcaseLimbAim& a,
                             const ShowcaseLimbAim& b,
                             float u) noexcept -> ShowcaseLimbAim {
  return {blend(a.pitch, b.pitch, u),
          blend(a.splay, b.splay, u),
          blend(a.yaw, b.yaw, u),
          blend(a.bend, b.bend, u)};
}

[[nodiscard]] auto
blend_key(const ShowcaseKey& a, const ShowcaseKey& b, float u) noexcept -> ShowcaseKey {
  ShowcaseKey out{};
  out.t = blend(a.t, b.t, u);
  out.root_x = blend(a.root_x, b.root_x, u);
  out.root_y = blend(a.root_y, b.root_y, u);
  out.root_z = blend(a.root_z, b.root_z, u);
  out.body_pitch = blend(a.body_pitch, b.body_pitch, u);
  out.body_roll = blend(a.body_roll, b.body_roll, u);
  out.body_yaw = blend(a.body_yaw, b.body_yaw, u);
  out.spine_pitch = blend(a.spine_pitch, b.spine_pitch, u);
  out.spine_roll = blend(a.spine_roll, b.spine_roll, u);
  out.spine_yaw = blend(a.spine_yaw, b.spine_yaw, u);
  out.head_pitch = blend(a.head_pitch, b.head_pitch, u);
  out.blade_pitch = blend(a.blade_pitch, b.blade_pitch, u);
  out.blade_yaw = blend(a.blade_yaw, b.blade_yaw, u);
  out.blade_amount = blend(a.blade_amount, b.blade_amount, u);
  out.arm_l = blend_aim(a.arm_l, b.arm_l, u);
  out.arm_r = blend_aim(a.arm_r, b.arm_r, u);
  out.leg_l = blend_aim(a.leg_l, b.leg_l, u);
  out.leg_r = blend_aim(a.leg_r, b.leg_r, u);
  return out;
}

[[nodiscard]] auto sample_key(std::span<const ShowcaseKey> keys,
                              float phase) noexcept -> ShowcaseKey {
  if (keys.empty()) {
    return {};
  }
  phase = std::clamp(phase, 0.0F, 1.0F);
  if (phase <= keys.front().t) {
    return keys.front();
  }
  for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
    const ShowcaseKey& a = keys[i];
    const ShowcaseKey& b = keys[i + 1];
    if (phase <= b.t) {
      float const span = std::max(1.0e-5F, b.t - a.t);
      return blend_key(a, b, smoothstep((phase - a.t) / span));
    }
  }
  return keys.back();
}

} // namespace

auto humanoid_showcase_move_name(HumanoidShowcaseMove move) noexcept
    -> std::string_view {
  switch (move) {
  case HumanoidShowcaseMove::Jump:
    return "showcase_jump";
  case HumanoidShowcaseMove::FrontFlip:
    return "showcase_front_flip";
  case HumanoidShowcaseMove::Handstand:
    return "showcase_handstand";
  case HumanoidShowcaseMove::SideAerial:
    return "showcase_side_aerial";
  case HumanoidShowcaseMove::SwordFlourish:
    return "showcase_sword_flourish";
  case HumanoidShowcaseMove::SpearThrow:
    return "showcase_spear_throw";
  case HumanoidShowcaseMove::RestSit:
    return "showcase_rest_sit";
  case HumanoidShowcaseMove::RestSitKnees:
    return "showcase_rest_sit_knees";
  case HumanoidShowcaseMove::RestKneel:
    return "showcase_rest_kneel";
  case HumanoidShowcaseMove::RestSitDown:
    return "showcase_rest_sit_down";
  case HumanoidShowcaseMove::RestSitKneesDown:
    return "showcase_rest_sit_knees_down";
  case HumanoidShowcaseMove::None:
  case HumanoidShowcaseMove::Count:
    break;
  }
  return "showcase_none";
}

auto humanoid_showcase_move_from_name(std::string_view name) noexcept
    -> HumanoidShowcaseMove {
  for (std::uint8_t i = 1; i < static_cast<std::uint8_t>(HumanoidShowcaseMove::Count);
       ++i) {
    auto const move = static_cast<HumanoidShowcaseMove>(i);
    std::string_view const full = humanoid_showcase_move_name(move);
    if (name == full || (full.starts_with("showcase_") && name == full.substr(9))) {
      return move;
    }
  }
  return HumanoidShowcaseMove::None;
}

auto humanoid_showcase_move_duration(HumanoidShowcaseMove move) noexcept -> float {
  switch (move) {
  case HumanoidShowcaseMove::Jump:
    return 1.6F;
  case HumanoidShowcaseMove::FrontFlip:
    return 1.7F;
  case HumanoidShowcaseMove::Handstand:
    return 3.4F;
  case HumanoidShowcaseMove::SideAerial:
    return 1.9F;
  case HumanoidShowcaseMove::SwordFlourish:
    return 2.6F;
  case HumanoidShowcaseMove::SpearThrow:
    return 2.2F;
  case HumanoidShowcaseMove::RestSit:
    return 6.0F;
  case HumanoidShowcaseMove::RestSitKnees:
    return 7.0F;
  case HumanoidShowcaseMove::RestKneel:
    return 5.0F;
  case HumanoidShowcaseMove::RestSitDown:
    return 1.6F;
  case HumanoidShowcaseMove::RestSitKneesDown:
    return 1.7F;
  case HumanoidShowcaseMove::None:
  case HumanoidShowcaseMove::Count:
    break;
  }
  return 1.0F;
}

auto humanoid_showcase_move_frames(HumanoidShowcaseMove move) noexcept
    -> std::uint32_t {
  constexpr float k_bake_fps = 30.0F;
  return static_cast<std::uint32_t>(
      std::lround(humanoid_showcase_move_duration(move) * k_bake_fps));
}

auto humanoid_showcase_root_travel(HumanoidShowcaseMove move,
                                   float phase) noexcept -> PoseVec3 {
  phase = std::clamp(phase, 0.0F, 1.0F);
  switch (move) {
  case HumanoidShowcaseMove::FrontFlip:
    return {0.0F, 0.0F, 1.55F * ramp(phase, 0.18F, 0.66F)};
  case HumanoidShowcaseMove::SideAerial:
    return {-2.62F * ramp(phase, 0.10F, 0.78F), 0.0F, 0.0F};
  case HumanoidShowcaseMove::SpearThrow:
    return {0.0F, 0.0F, 0.52F * ramp(phase, 0.30F, 0.34F)};
  case HumanoidShowcaseMove::Jump:
  case HumanoidShowcaseMove::Handstand:
  case HumanoidShowcaseMove::SwordFlourish:
  case HumanoidShowcaseMove::RestSit:
  case HumanoidShowcaseMove::RestSitKnees:
  case HumanoidShowcaseMove::RestKneel:
  case HumanoidShowcaseMove::RestSitDown:
  case HumanoidShowcaseMove::RestSitKneesDown:
  case HumanoidShowcaseMove::None:
  case HumanoidShowcaseMove::Count:
    break;
  }
  return {};
}

auto humanoid_showcase_release_phase(HumanoidShowcaseMove move) noexcept -> float {
  switch (move) {
  case HumanoidShowcaseMove::SpearThrow:
    return 0.58F;
  case HumanoidShowcaseMove::SwordFlourish:
    return 0.44F;
  case HumanoidShowcaseMove::Jump:
  case HumanoidShowcaseMove::FrontFlip:
  case HumanoidShowcaseMove::Handstand:
  case HumanoidShowcaseMove::SideAerial:
  case HumanoidShowcaseMove::RestSit:
  case HumanoidShowcaseMove::RestSitKnees:
  case HumanoidShowcaseMove::RestKneel:
  case HumanoidShowcaseMove::RestSitDown:
  case HumanoidShowcaseMove::RestSitKneesDown:
  case HumanoidShowcaseMove::None:
  case HumanoidShowcaseMove::Count:
    break;
  }
  return -1.0F;
}

auto resolve_humanoid_showcase_pose(const HumanoidShowcasePoseInputs& inputs) noexcept
    -> HumanoidShowcasePoseSample {
  auto const keys = keys_for(inputs.move);
  if (keys.empty()) {
    return {};
  }
  ShowcaseKey const key = sample_key(keys, inputs.phase);
  HumanoidShowcaseRig const& rig = inputs.rig;

  Mat3 const spine =
      multiply(rot_x(radians(key.spine_pitch)),
               multiply(rot_z(radians(key.spine_roll)), rot_y(radians(key.spine_yaw))));

  PoseVec3 const pelvis{0.0F, rig.pelvis_y, 0.0F};
  PoseVec3 const neck = add(pelvis, transform(spine, {0.0F, rig.neck_rise, 0.0F}));
  PoseVec3 const shoulder_center =
      add(pelvis, transform(spine, {0.0F, rig.shoulder_rise, 0.0F}));
  PoseVec3 const shoulder_l =
      add(shoulder_center, transform(spine, {-rig.shoulder_half_width, 0.0F, 0.0F}));
  PoseVec3 const shoulder_r =
      add(shoulder_center, transform(spine, {rig.shoulder_half_width, 0.0F, 0.0F}));
  PoseVec3 const head = add(neck,
                            transform(multiply(spine, rot_x(radians(key.head_pitch))),
                                      {0.0F, rig.head_rise, 0.0F}));

  LimbSegments const arm_l = limb_segments(key.arm_l, -1.0F, 1.0F, spine);
  LimbSegments const arm_r = limb_segments(key.arm_r, 1.0F, 1.0F, spine);
  PoseVec3 const elbow_l = add(shoulder_l, scaled(arm_l.upper, rig.upper_arm_len));
  PoseVec3 const hand_l = add(elbow_l, scaled(arm_l.lower, rig.fore_arm_len));
  PoseVec3 const elbow_r = add(shoulder_r, scaled(arm_r.upper, rig.upper_arm_len));
  PoseVec3 const hand_r = add(elbow_r, scaled(arm_r.lower, rig.fore_arm_len));

  Mat3 const upright = identity();
  PoseVec3 const hip_l = add(pelvis, {-rig.hip_half_width, -rig.hip_drop, 0.0F});
  PoseVec3 const hip_r = add(pelvis, {rig.hip_half_width, -rig.hip_drop, 0.0F});
  LimbSegments const leg_l = limb_segments(key.leg_l, -1.0F, -1.0F, upright);
  LimbSegments const leg_r = limb_segments(key.leg_r, 1.0F, -1.0F, upright);
  PoseVec3 const knee_l = add(hip_l, scaled(leg_l.upper, rig.upper_leg_len));
  PoseVec3 const foot_l = add(knee_l, scaled(leg_l.lower, rig.lower_leg_len));
  PoseVec3 const knee_r = add(hip_r, scaled(leg_r.upper, rig.upper_leg_len));
  PoseVec3 const foot_r = add(knee_r, scaled(leg_r.lower, rig.lower_leg_len));

  Mat3 const body =
      multiply(rot_x(radians(key.body_pitch)),
               multiply(rot_z(radians(key.body_roll)), rot_y(radians(key.body_yaw))));
  PoseVec3 const root{key.root_x, key.root_y, key.root_z};
  float const height = std::max(0.1F, inputs.height_scale);

  auto place = [&](PoseVec3 p) {
    return scaled(add(transform(body, sub(p, pelvis)), root), height);
  };

  HumanoidShowcasePoseSample sample{};
  sample.active = true;
  sample.pelvis = place(pelvis);
  sample.neck_base = place(neck);
  sample.head = place(head);
  sample.shoulder_l = place(shoulder_l);
  sample.shoulder_r = place(shoulder_r);
  sample.elbow_l = place(elbow_l);
  sample.elbow_r = place(elbow_r);
  sample.hand_l = place(hand_l);
  sample.hand_r = place(hand_r);
  sample.knee_l = place(knee_l);
  sample.knee_r = place(knee_r);
  sample.foot_l = place(foot_l);
  sample.foot_r = place(foot_r);

  if (key.blade_amount > 0.01F) {
    PoseVec3 const raw = transform(
        multiply(rot_y(radians(key.blade_yaw)), rot_x(-radians(key.blade_pitch))),
        {0.0F, 1.0F, 0.0F});
    sample.grip_axis_r = transform(body, transform(spine, raw));
    sample.has_grip_axis = true;
  }
  return sample;
}

} // namespace Animation
