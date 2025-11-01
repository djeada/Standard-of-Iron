#!/usr/bin/env python3
"""
Validation script for nation-troop shader configurations.
Ensures all 12 combinations have proper vertex and fragment shaders.
"""

import os
import re
import sys

# Configuration
SHADER_DIR = "assets/shaders"
STYLE_DIR = "render/entity/nations"

NATIONS = {
    "kingdom": "kingdom_of_iron",
    "roman": "roman_republic",
    "carthage": "carthage"
}

TROOPS = ["archer", "swordsman", "spearman", "horse_swordsman"]

def check_shader_files():
    """Check if all required shader files exist."""
    missing = []
    
    for troop in TROOPS:
        for nation_short, nation_full in NATIONS.items():
            shader_name = f"{troop}_{nation_full}"
            vert_file = os.path.join(SHADER_DIR, f"{shader_name}.vert")
            frag_file = os.path.join(SHADER_DIR, f"{shader_name}.frag")
            
            if not os.path.exists(vert_file):
                missing.append(f"Vertex shader: {vert_file}")
            if not os.path.exists(frag_file):
                missing.append(f"Fragment shader: {frag_file}")
    
    return missing

def check_style_configs():
    """Check if style configurations match shader files."""
    issues = []
    
    for troop in TROOPS:
        for nation_short, nation_full in NATIONS.items():
            expected_shader = f"{troop}_{nation_full}"
            style_file = os.path.join(STYLE_DIR, nation_short, f"{troop}_style.cpp")
            
            if not os.path.exists(style_file):
                issues.append(f"Missing style file: {style_file}")
                continue
            
            with open(style_file, 'r') as f:
                content = f.read()
            
            # Look for shader_id assignment
            shader_id_pattern = r'shader_id\s*=\s*"([^"]+)"'
            matches = re.findall(shader_id_pattern, content)
            
            if not matches:
                issues.append(f"{nation_short}/{troop}: No shader_id found in {style_file}")
            elif matches[0] != expected_shader:
                issues.append(
                    f"{nation_short}/{troop}: shader_id is '{matches[0]}' "
                    f"but should be '{expected_shader}'"
                )
    
    return issues

def main():
    """Run all validations and report results."""
    print("=" * 80)
    print("Nation-Troop Shader Configuration Validation")
    print("=" * 80)
    print()
    
    # Check shader files
    print("Checking shader files...")
    missing_shaders = check_shader_files()
    
    if missing_shaders:
        print("✗ Missing shader files:")
        for item in missing_shaders:
            print(f"  - {item}")
    else:
        print("✓ All shader files present")
    
    print()
    
    # Check style configurations
    print("Checking style configurations...")
    config_issues = check_style_configs()
    
    if config_issues:
        print("✗ Configuration issues:")
        for item in config_issues:
            print(f"  - {item}")
    else:
        print("✓ All style configurations valid")
    
    print()
    print("=" * 80)
    
    # Summary
    total_issues = len(missing_shaders) + len(config_issues)
    if total_issues == 0:
        print("✓ VALIDATION PASSED")
        print()
        print(f"All {len(TROOPS) * len(NATIONS)} nation-troop combinations are properly configured.")
        return 0
    else:
        print("✗ VALIDATION FAILED")
        print()
        print(f"Found {total_issues} issue(s) that need to be fixed.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
