#pragma once

#include <cstdint>

namespace Render::GL {

namespace HorseDimensionRange {

constexpr float k_body_length_min = 1.12F;
constexpr float k_body_length_max = 1.28F;
constexpr float k_body_width_min = 0.23F;
constexpr float k_body_width_max = 0.29F;
constexpr float k_body_height_min = 0.50F;
constexpr float k_body_height_max = 0.60F;

constexpr float k_neck_length_min = 0.54F;
constexpr float k_neck_length_max = 0.66F;
constexpr float k_neck_rise_min = 0.42F;
constexpr float k_neck_rise_max = 0.50F;

constexpr float k_head_length_min = 0.36F;
constexpr float k_head_length_max = 0.46F;
constexpr float k_head_width_min = 0.16F;
constexpr float k_head_width_max = 0.20F;
constexpr float k_head_height_min = 0.22F;
constexpr float k_head_height_max = 0.28F;
constexpr float k_muzzle_length_min = 0.16F;
constexpr float k_muzzle_length_max = 0.20F;

constexpr float k_leg_length_min = 1.14F;
constexpr float k_leg_length_max = 1.28F;
constexpr float k_hoof_height_min = 0.095F;
constexpr float k_hoof_height_max = 0.115F;

constexpr float k_tail_length_min = 0.55F;
constexpr float k_tail_length_max = 0.72F;

constexpr float k_saddle_thickness_min = 0.035F;
constexpr float k_saddle_thickness_max = 0.045F;
constexpr float k_seat_forward_offset_min = 0.010F;
constexpr float k_seat_forward_offset_max = 0.035F;
constexpr float k_stirrup_out_scale_min = 0.75F;
constexpr float k_stirrup_out_scale_max = 0.88F;
constexpr float k_stirrup_drop_min = 0.28F;
constexpr float k_stirrup_drop_max = 0.32F;

constexpr float k_idle_bob_amplitude_min = 0.004F;
constexpr float k_idle_bob_amplitude_max = 0.007F;
constexpr float k_move_bob_amplitude_min = 0.024F;
constexpr float k_move_bob_amplitude_max = 0.032F;

constexpr float k_leg_segment_ratio_upper = 0.59F;
constexpr float k_leg_segment_ratio_middle = 0.30F;
constexpr float k_leg_segment_ratio_lower = 0.12F;
constexpr float k_shoulder_barrel_offset_scale = 0.05F;
constexpr float k_shoulder_barrel_offset_base = 0.05F;
constexpr float k_saddle_height_body_scale = 0.55F;

constexpr uint32_t k_salt_body_length = 0x12U;
constexpr uint32_t k_salt_body_width = 0x34U;
constexpr uint32_t k_salt_body_height = 0x56U;
constexpr uint32_t k_salt_neck_length = 0x9AU;
constexpr uint32_t k_salt_neck_rise = 0xBCU;
constexpr uint32_t k_salt_head_length = 0xDEU;
constexpr uint32_t k_salt_head_width = 0xF1U;
constexpr uint32_t k_salt_head_height = 0x1357U;
constexpr uint32_t k_salt_muzzle_length = 0x2468U;
constexpr uint32_t k_salt_leg_length = 0x369CU;
constexpr uint32_t k_salt_hoof_height = 0x48AEU;
constexpr uint32_t k_salt_tail_length = 0x5ABCU;
constexpr uint32_t k_salt_saddle_thickness = 0x6CDEU;
constexpr uint32_t k_salt_seat_forward_offset = 0x7531U;
constexpr uint32_t k_salt_stirrup_out = 0x8642U;
constexpr uint32_t k_salt_stirrup_drop = 0x9753U;
constexpr uint32_t k_salt_idle_bob = 0xA864U;
constexpr uint32_t k_salt_move_bob = 0xB975U;

} // namespace HorseDimensionRange

namespace HorseVariantConstants {

constexpr float k_gray_coat_threshold = 0.18F;
constexpr float k_bay_coat_threshold = 0.38F;
constexpr float k_chestnut_coat_threshold = 0.65F;
constexpr float k_black_coat_threshold = 0.85F;

constexpr float k_gray_coat_r = 0.70F;
constexpr float k_gray_coat_g = 0.68F;
constexpr float k_gray_coat_b = 0.63F;
constexpr float k_bay_coat_r = 0.40F;
constexpr float k_bay_coat_g = 0.30F;
constexpr float k_bay_coat_b = 0.22F;
constexpr float k_chestnut_coat_r = 0.28F;
constexpr float k_chestnut_coat_g = 0.22F;
constexpr float k_chestnut_coat_b = 0.19F;
constexpr float k_black_coat_r = 0.18F;
constexpr float k_black_coat_g = 0.15F;
constexpr float k_black_coat_b = 0.13F;
constexpr float k_dun_coat_r = 0.48F;
constexpr float k_dun_coat_g = 0.42F;
constexpr float k_dun_coat_b = 0.39F;

constexpr float k_blaze_chance_threshold = 0.82F;
constexpr float k_blaze_color_r = 0.92F;
constexpr float k_blaze_color_g = 0.92F;
constexpr float k_blaze_color_b = 0.90F;
constexpr float k_blaze_blend_factor = 0.25F;

constexpr float k_mane_blend_min = 0.55F;
constexpr float k_mane_blend_max = 0.85F;
constexpr float k_mane_base_r = 0.10F;
constexpr float k_mane_base_g = 0.09F;
constexpr float k_mane_base_b = 0.08F;
constexpr float k_tail_blend_factor = 0.35F;

constexpr float k_muzzle_blend_factor = 0.65F;
constexpr float k_muzzle_base_r = 0.18F;
constexpr float k_muzzle_base_g = 0.14F;
constexpr float k_muzzle_base_b = 0.12F;
constexpr float k_hoof_dark_r = 0.16F;
constexpr float k_hoof_dark_g = 0.14F;
constexpr float k_hoof_dark_b = 0.12F;
constexpr float k_hoof_light_r = 0.40F;
constexpr float k_hoof_light_g = 0.35F;
constexpr float k_hoof_light_b = 0.32F;
constexpr float k_hoof_blend_min = 0.15F;
constexpr float k_hoof_blend_max = 0.65F;

constexpr float k_leather_tone_min = 0.78F;
constexpr float k_leather_tone_max = 0.96F;
constexpr float k_tack_tone_min = 0.58F;
constexpr float k_tack_tone_max = 0.78F;
constexpr float k_special_tack_threshold = 0.90F;
constexpr float k_special_tack_r = 0.18F;
constexpr float k_special_tack_g = 0.19F;
constexpr float k_special_tack_b = 0.22F;
constexpr float k_special_tack_blend = 0.25F;

constexpr float k_blanket_tint_min = 0.92F;
constexpr float k_blanket_tint_max = 1.05F;

constexpr float k_ear_inner_base_r = 0.45F;
constexpr float k_ear_inner_base_g = 0.35F;
constexpr float k_ear_inner_base_b = 0.32F;
constexpr float k_ear_inner_blend_factor = 0.30F;

constexpr uint32_t k_salt_coat_hue = 0x23456U;
constexpr uint32_t k_salt_blaze_chance = 0x1122U;
constexpr uint32_t k_salt_mane_blend = 0x3344U;
constexpr uint32_t k_salt_hoof_blend = 0x5566U;
constexpr uint32_t k_salt_leather_tone = 0x7788U;
constexpr uint32_t k_salt_tack_tone = 0x88AAU;
constexpr uint32_t k_salt_blanket_tint = 0x99B0U;

} // namespace HorseVariantConstants

namespace HorseGaitConstants {

constexpr float k_cycle_time_min = 0.60F;
constexpr float k_cycle_time_max = 0.72F;
constexpr float k_front_leg_phase_min = 0.08F;
constexpr float k_front_leg_phase_max = 0.16F;
constexpr float k_diagonal_lead_min = 0.44F;
constexpr float k_diagonal_lead_max = 0.54F;
constexpr float k_stride_swing_min = 0.26F;
constexpr float k_stride_swing_max = 0.32F;
constexpr float k_stride_lift_min = 0.10F;
constexpr float k_stride_lift_max = 0.14F;

constexpr uint32_t k_salt_cycle_time = 0xAA12U;
constexpr uint32_t k_salt_front_leg_phase = 0xBB34U;
constexpr uint32_t k_salt_diagonal_lead = 0xCC56U;
constexpr uint32_t k_salt_stride_swing = 0xDD78U;
constexpr uint32_t k_salt_stride_lift = 0xEE9AU;

} // namespace HorseGaitConstants

} // namespace Render::GL
