#pragma once

#include <cstdint>

namespace Render::GL {

namespace ElephantDimensionRange {

constexpr float k_body_length_min = 0.7333333F;
constexpr float k_body_length_max = 0.8666667F;
constexpr float k_body_width_min = 0.30F;
constexpr float k_body_width_max = 0.3666667F;
constexpr float k_body_height_min = 0.40F;
constexpr float k_body_height_max = 0.50F;

constexpr float k_neck_length_min = 0.175F;
constexpr float k_neck_length_max = 0.25F;
constexpr float k_neck_width_min = 0.225F;
constexpr float k_neck_width_max = 0.275F;

constexpr float k_head_length_min = 0.275F;
constexpr float k_head_length_max = 0.35F;
constexpr float k_head_width_min = 0.25F;
constexpr float k_head_width_max = 0.325F;
constexpr float k_head_height_min = 0.275F;
constexpr float k_head_height_max = 0.35F;

constexpr float k_trunk_length_min = 0.80F;
constexpr float k_trunk_length_max = 1.00F;
constexpr float k_trunk_base_radius_min = 0.09F;
constexpr float k_trunk_base_radius_max = 0.12F;
constexpr float k_trunk_tip_radius_min = 0.02F;
constexpr float k_trunk_tip_radius_max = 0.035F;

constexpr float k_ear_width_min = 0.35F;
constexpr float k_ear_width_max = 0.45F;
constexpr float k_ear_height_min = 0.40F;
constexpr float k_ear_height_max = 0.50F;
constexpr float k_ear_thickness_min = 0.012F;
constexpr float k_ear_thickness_max = 0.022F;

constexpr float k_leg_length_min = 0.70F;
constexpr float k_leg_length_max = 0.85F;
constexpr float k_leg_radius_min = 0.09F;
constexpr float k_leg_radius_max = 0.125F;
constexpr float k_foot_radius_min = 0.11F;
constexpr float k_foot_radius_max = 0.15F;

constexpr float k_tail_length_min = 0.35F;
constexpr float k_tail_length_max = 0.475F;

constexpr float k_tusk_length_min = 0.25F;
constexpr float k_tusk_length_max = 0.425F;
constexpr float k_tusk_radius_min = 0.02F;
constexpr float k_tusk_radius_max = 0.035F;

constexpr float k_howdah_width_min = 0.40F;
constexpr float k_howdah_width_max = 0.50F;
constexpr float k_howdah_length_min = 0.50F;
constexpr float k_howdah_length_max = 0.65F;
constexpr float k_howdah_height_min = 0.20F;
constexpr float k_howdah_height_max = 0.275F;

constexpr float k_idle_bob_amplitude_min = 0.004F;
constexpr float k_idle_bob_amplitude_max = 0.0075F;
constexpr float k_move_bob_amplitude_min = 0.0175F;
constexpr float k_move_bob_amplitude_max = 0.0275F;

constexpr uint32_t k_salt_body_length = 0x12U;
constexpr uint32_t k_salt_body_width = 0x34U;
constexpr uint32_t k_salt_body_height = 0x56U;
constexpr uint32_t k_salt_neck_length = 0x78U;
constexpr uint32_t k_salt_neck_width = 0x9AU;
constexpr uint32_t k_salt_head_length = 0xBCU;
constexpr uint32_t k_salt_head_width = 0xDEU;
constexpr uint32_t k_salt_head_height = 0xF0U;
constexpr uint32_t k_salt_trunk_length = 0x123U;
constexpr uint32_t k_salt_trunk_base_radius = 0x234U;
constexpr uint32_t k_salt_trunk_tip_radius = 0x345U;
constexpr uint32_t k_salt_ear_width = 0x456U;
constexpr uint32_t k_salt_ear_height = 0x567U;
constexpr uint32_t k_salt_ear_thickness = 0x678U;
constexpr uint32_t k_salt_leg_length = 0x789U;
constexpr uint32_t k_salt_leg_radius = 0x89AU;
constexpr uint32_t k_salt_foot_radius = 0x9ABU;
constexpr uint32_t k_salt_tail_length = 0xABCU;
constexpr uint32_t k_salt_tusk_length = 0xBCDU;
constexpr uint32_t k_salt_tusk_radius = 0xCDEU;
constexpr uint32_t k_salt_howdah_width = 0xDEFU;
constexpr uint32_t k_salt_howdah_length = 0xEF0U;
constexpr uint32_t k_salt_howdah_height = 0xF01U;
constexpr uint32_t k_salt_idle_bob = 0x102U;
constexpr uint32_t k_salt_move_bob = 0x213U;

} // namespace ElephantDimensionRange

namespace ElephantVariantConstants {

constexpr float k_skin_base_r = 0.65F;
constexpr float k_skin_base_g = 0.61F;
constexpr float k_skin_base_b = 0.58F;

constexpr float k_skin_variation_min = 0.85F;
constexpr float k_skin_variation_max = 1.15F;

constexpr float k_highlight_blend = 0.15F;
constexpr float k_shadow_blend = 0.20F;

constexpr float k_ear_inner_r = 0.55F;
constexpr float k_ear_inner_g = 0.45F;
constexpr float k_ear_inner_b = 0.42F;

constexpr float k_tusk_r = 1.0F;
constexpr float k_tusk_g = 1.0F;
constexpr float k_tusk_b = 1.0F;

constexpr float k_toenail_r = 0.35F;
constexpr float k_toenail_g = 0.32F;
constexpr float k_toenail_b = 0.28F;

constexpr float k_wood_r = 0.45F;
constexpr float k_wood_g = 0.32F;
constexpr float k_wood_b = 0.22F;

constexpr uint32_t k_salt_skin_variation = 0x324U;
constexpr uint32_t k_salt_highlight = 0x435U;
constexpr uint32_t k_salt_shadow = 0x546U;

} // namespace ElephantVariantConstants

namespace ElephantGaitConstants {

constexpr float k_cycle_time_min = 2.20F;
constexpr float k_cycle_time_max = 2.80F;
constexpr float k_front_leg_phase_min = 0.0F;
constexpr float k_front_leg_phase_max = 0.10F;
constexpr float k_diagonal_lead_min = 0.48F;
constexpr float k_diagonal_lead_max = 0.52F;

constexpr float k_stride_swing_min = 0.55F;
constexpr float k_stride_swing_max = 0.75F;

constexpr float k_stride_lift_min = 0.18F;
constexpr float k_stride_lift_max = 0.26F;

constexpr uint32_t k_salt_cycle_time = 0x657U;
constexpr uint32_t k_salt_front_leg_phase = 0x768U;
constexpr uint32_t k_salt_diagonal_lead = 0x879U;
constexpr uint32_t k_salt_stride_swing = 0x98AU;
constexpr uint32_t k_salt_stride_lift = 0xA9BU;

} // namespace ElephantGaitConstants

} // namespace Render::GL
