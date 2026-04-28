#!/usr/bin/env python3
"""Convert camelCase identifiers to snake_case in render/ directory."""
import re
import sys
from pathlib import Path

# (camelCase, snake_case)
RENAMES = [
    # Local variables and function parameters
    ("nextRand", "next_rand"),
    ("globalIndex", "global_index"),
    ("rotationDeg", "rotation_deg"),
    ("flipU", "flip_u"),
    ("heightSum", "height_sum"),
    ("heightCount", "height_count"),
    ("heightVarSum", "height_var_sum"),
    ("slopeSum", "slope_sum"),
    ("statCount", "stat_count"),
    ("aoSum", "ao_sum"),
    ("aoCount", "ao_count"),
    ("curvatureSum", "curvature_sum"),
    ("curvatureCount", "curvature_count"),
    ("entrySum", "entry_sum"),
    ("entryPeak", "entry_peak"),
    ("entryCount", "entry_count"),
    ("normalSum", "normal_sum"),
    ("normalUp", "normal_up"),
    ("riverSegments", "river_segments"),
    # Function/method names
    ("drawArms", "draw_arms"),
    ("drawBannerWithTassels", "draw_banner_with_tassels"),
    ("drawBaseFrame", "draw_base_frame"),
    ("drawBowstring", "draw_bowstring"),
    ("drawCarthageOrnaments", "draw_carthage_ornaments"),
    ("drawCarthageRoof", "draw_carthage_roof"),
    ("drawCornerTowers", "draw_corner_towers"),
    ("drawCourtyard", "draw_courtyard"),
    ("drawDecorations", "draw_decorations"),
    ("drawFortressBase", "draw_fortress_base"),
    ("drawFortressWalls", "draw_fortress_walls"),
    ("drawGate", "draw_gate"),
    ("drawPoleWithBanner", "draw_pole_with_banner"),
    ("drawPolygon", "draw_polygon"),
    ("drawRomanOrnaments", "draw_roman_ornaments"),
    ("drawSlide", "draw_slide"),
    ("drawStandards", "draw_standards"),
    ("drawThrowingArm", "draw_throwing_arm"),
    ("drawTorsionBundles", "draw_torsion_bundles"),
    ("drawTorsionMechanism", "draw_torsion_mechanism"),
    ("drawTriggerMechanism", "draw_trigger_mechanism"),
    ("drawWheel", "draw_wheel"),
    ("drawWheels", "draw_wheels"),
    ("drawWindlass", "draw_windlass"),
    ("renderArmGuard", "render_arm_guard"),
    ("renderApronBody", "render_apron_body"),
    ("renderPockets", "render_pockets"),
    ("renderStraps", "render_straps"),
    ("computeKnightExtras", "compute_knight_extras"),
    ("computeSpearmanExtras", "compute_spearman_extras"),
    ("applyMountedKnightLowerBody", "apply_mounted_knight_lower_body"),
    ("calculateDynamicMargin", "calculate_dynamic_margin"),
    ("smoothApproach", "smooth_approach"),
    ("clampOrthoBox", "clamp_ortho_box"),
    ("fromSeed", "from_seed"),
    ("getBridges", "get_bridges"),
    ("getBufferStorageFunction", "get_buffer_storage_function"),
    ("getIndividualsPerUnit", "get_individuals_per_unit"),
    ("getMaxUnitsPerRow", "get_max_units_per_row"),
    ("getRiverSegments", "get_river_segments"),
    ("getSelectionRingGroundOffset", "get_selection_ring_ground_offset"),
    ("getSelectionRingSize", "get_selection_ring_size"),
    ("hasPersistentMapping", "has_persistent_mapping"),
    ("setExplicitFireCamps", "set_explicit_fire_camps"),
    # Member variables
    ("m_riverSegments", "m_river_segments"),
    ("m_heightData", "m_height_data"),
    ("m_terrainTypes", "m_terrain_types"),
    ("m_assetInstanceCount", "m_asset_instance_count"),
    ("m_assetInstances", "m_asset_instances"),
    ("m_assetInstancesDirty", "m_asset_instances_dirty"),
    ("m_assetInstanceBuffer", "m_asset_instance_buffer"),
    ("m_visibleInstances", "m_visible_instances"),
    ("m_visibilityDirty", "m_visibility_dirty"),
    ("fireTexture", "fire_texture"),
    ("clothMesh", "cloth_mesh"),
    ("bannerShader", "banner_shader"),
    ("fogShader", "fog_shader"),
    ("cylinderShader", "cylinder_shader"),
    ("shaderScope", "shader_scope"),
    # Other local variables
    ("auraColor", "aura_color"),
    ("auraRadius", "aura_radius"),
    ("bannerColor", "banner_color"),
    ("bannerCenter", "banner_center"),
    ("bannerHalfSize", "banner_half_size"),
    ("bannerTransform", "banner_transform"),
    ("baseMargin", "base_margin"),
    ("baseTeamColor", "base_team_color"),
    ("baseTeamTrim", "base_team_trim"),
    ("batchSubmitter", "batch_submitter"),
    ("beamWidth", "beam_width"),
    ("bottomL", "bottom_l"),
    ("bottomR", "bottom_r"),
    ("calfBehindGirth", "calf_behind_girth"),
    ("calfBlend", "calf_blend"),
    ("calfDownExtra", "calf_down_extra"),
    ("calfOutOffset", "calf_out_offset"),
    ("calfRelax", "calf_relax"),
    ("calfSurfaceBlend", "calf_surface_blend"),
    ("cameraHeight", "camera_height"),
    ("cellSize", "cell_size"),
    ("earL", "ear_l"),
    ("earR", "ear_r"),
    ("elementOffset", "element_offset"),
    ("endPos", "end_pos"),
    ("flickerAmount", "flicker_amount"),
    ("flickerSpeed", "flicker_speed"),
    ("footBlend", "foot_blend"),
    ("footDownOffset", "foot_down_offset"),
    ("fragPath", "frag_path"),
    ("frameIndex", "frame_index"),
    ("frontOut", "front_out"),
    ("fullShaderMaxDistanceSq", "full_shader_max_distance_sq"),
    ("gfxSettings", "gfx_settings"),
    ("glowStrength", "glow_strength"),
    ("gridColor", "grid_color"),
    ("handleRadius", "handle_radius"),
    ("healColor", "heal_color"),
    ("kneeAlong", "knee_along"),
    ("kneeBlend", "knee_blend"),
    ("kneePlaneLerp", "knee_plane_lerp"),
    ("lineColor", "line_color"),
    ("lowerStart", "lower_start"),
    ("loweringOffset", "lowering_offset"),
    ("majorVersion", "major_version"),
    ("maxLowering", "max_lowering"),
    ("maxVal", "max_val"),
    ("minVal", "min_val"),
    ("minorVersion", "minor_version"),
    ("modeColor", "mode_color"),
    ("newCapacity", "new_capacity"),
    ("outMode", "out_mode"),
    ("outScreen", "out_screen"),
    ("outWorld", "out_world"),
    ("planeIndex", "plane_index"),
    ("poleEnd", "pole_end"),
    ("poleRadius", "pole_radius"),
    ("poleStart", "pole_start"),
    ("rightOut", "right_out"),
    ("scaleY", "scale_y"),
    ("shieldCrossDecal", "shield_cross_decal"),
    ("shieldOutsetFactor", "shield_outset_factor"),
    ("shieldRadius", "shield_radius"),
    ("shieldRaiseSpeed", "shield_raise_speed"),
    ("spearShaftColor", "spear_shaft_color"),
    ("spearheadLength", "spearhead_length"),
    ("startPos", "start_pos"),
    ("stirrupDropScale", "stirrup_drop_scale"),
    ("stirrupForwardBias", "stirrup_forward_bias"),
    ("stirrupHeightBias", "stirrup_height_bias"),
    ("stirrupInsetFactor", "stirrup_inset_factor"),
    ("stirrupOutwardBias", "stirrup_outward_bias"),
    ("swordOutsetFactor", "sword_outset_factor"),
    ("swordWidth", "sword_width"),
    ("targetCenter", "target_center"),
    ("teamTrim", "team_trim"),
    ("teamTrimColor", "team_trim_color"),
    ("thighWrapFactor", "thigh_wrap_factor"),
    ("timberLight", "timber_light"),
    ("topIdx", "top_idx"),
    ("topL", "top_l"),
    ("topR", "top_r"),
    ("trimColor", "trim_color"),
    ("upOut", "up_out"),
    ("upperStart", "upper_start"),
    ("windStrength", "wind_strength"),
    ("woodDark", "wood_dark"),
    ("writeOffset", "write_offset"),
    # Signal/slot/property names
    ("overlayChanged", "overlay_changed"),
    ("overlayText", "overlay_text"),
    ("enabledChanged", "enabled_changed"),
    ("visibleUnitCount", "visible_unit_count"),
    ("screenW", "screen_w"),
    ("screenH", "screen_h"),
    ("battleOptimizer", "battle_optimizer"),
    ("drawCalls", "draw_calls"),
    ("buffersInFlight", "buffers_in_flight"),
    ("useBatching", "use_batching"),
    ("useTexture", "use_texture"),
    ("mapFlags", "map_flags"),
    ("totalMs", "total_ms"),
    ("budgetHeadroomMs", "budget_headroom_ms"),
    ("isPanning", "is_panning"),
    ("isTransparent", "is_transparent"),
    ("isShadowShader", "is_shadow_shader"),
    ("cullEnabled", "cull_enabled"),
    ("blendEnabled", "blend_enabled"),
    ("depthTestEnabled", "depth_test_enabled"),
    ("prevEnable", "prev_enable"),
    ("prevBlendSrc", "prev_blend_src"),
    ("prevBlendDst", "prev_blend_dst"),
    ("armMatrix", "arm_matrix"),
    ("actualBannerColor", "actual_banner_color"),
    ("allowTargetShift", "allow_target_shift"),
    ("bindErr", "bind_err"),
    ("genErr", "gen_err"),
    ("preErr", "pre_err"),
    ("storageFlags", "storage_flags"),
]

def rename_in_file(path: Path, renames: list, dry_run: bool = False) -> int:
    """Apply renames to a file. Returns number of changes made."""
    content = path.read_text(encoding='utf-8')
    original = content
    
    for old, new in renames:
        # Use word boundaries: \b works for word chars -> non-word chars transitions
        pattern = r'\b' + re.escape(old) + r'\b'
        content = re.sub(pattern, new, content)
    
    if content != original:
        if not dry_run:
            path.write_text(content, encoding='utf-8')
        # Count changes
        changes = 0
        for old, new in renames:
            pattern = r'\b' + re.escape(old) + r'\b'
            changes += len(re.findall(pattern, original))
        return changes
    return 0

def main():
    base = Path("/home/runner/work/Standard-of-Iron/Standard-of-Iron")
    dirs = [base / "render", base / "tests" / "render"]
    
    total_files = 0
    total_changes = 0
    
    for d in dirs:
        if not d.exists():
            print(f"Directory not found: {d}")
            continue
        for ext in ("*.h", "*.cpp"):
            for path in sorted(d.rglob(ext)):
                n = rename_in_file(path, RENAMES)
                if n > 0:
                    total_files += 1
                    total_changes += n
                    print(f"  {path.relative_to(base)}: {n} replacements")
    
    print(f"\nTotal: {total_changes} replacements in {total_files} files")

if __name__ == "__main__":
    main()
