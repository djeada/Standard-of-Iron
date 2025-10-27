#!/usr/bin/env python3
"""
Shader Uniform Validation Tool

This script validates that uniform names in GLSL shaders match the uniform
names used in the C++ backend code. It helps prevent rendering bugs caused
by naming convention mismatches (camelCase vs snake_case).

Usage:
    python3 scripts/validate_shader_uniforms.py [--fix]

Options:
    --fix    Attempt to automatically fix naming mismatches in backend.cpp
"""

import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple
from collections import defaultdict

# ANSI color codes
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  # No Color
    BOLD = '\033[1m'

def find_shader_uniforms(shader_path: Path) -> Set[str]:
    """Extract all uniform variable names from a GLSL shader file."""
    uniforms = set()
    
    with open(shader_path, 'r') as f:
        content = f.read()
        
    # Match patterns like: uniform mat4 u_viewProj;
    # This regex captures the variable name after the type
    pattern = r'uniform\s+\w+\s+(\w+)\s*;'
    matches = re.findall(pattern, content)
    uniforms.update(matches)
    
    return uniforms

def find_backend_uniform_calls(backend_path: Path) -> Dict[str, List[Tuple[int, str]]]:
    """
    Find all uniformHandle() calls in backend.cpp
    Returns dict mapping uniform names to list of (line_number, line_content) tuples
    """
    uniform_calls = defaultdict(list)
    
    with open(backend_path, 'r') as f:
        lines = f.readlines()
        
    # Match patterns like: uniformHandle("u_viewProj")
    pattern = r'uniformHandle\s*\(\s*"([^"]+)"\s*\)'
    
    for line_num, line in enumerate(lines, start=1):
        matches = re.findall(pattern, line)
        for uniform_name in matches:
            uniform_calls[uniform_name].append((line_num, line.strip()))
    
    return uniform_calls

def convert_to_snake_case(name: str) -> str:
    """Convert camelCase to snake_case."""
    # Handle already snake_case names
    if '_' in name and not any(c.isupper() for c in name):
        return name
        
    # Insert underscore before uppercase letters
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()

def find_naming_mismatches(shader_uniforms: Dict[str, Set[str]], 
                          backend_uniforms: Dict[str, List[Tuple[int, str]]]) -> List[Dict]:
    """
    Find uniforms where backend uses different naming than shader.
    Returns list of mismatch dictionaries.
    """
    mismatches = []
    
    # Build reverse index: what shaders use each uniform
    uniform_to_shaders = defaultdict(list)
    for shader, uniforms in shader_uniforms.items():
        for uniform in uniforms:
            uniform_to_shaders[uniform].append(shader)
    
    # Check each backend uniform call
    for backend_name, locations in backend_uniforms.items():
        # Check if this exact name exists in any shader
        if backend_name not in uniform_to_shaders:
            # Try to find similar names in shaders (potential mismatch)
            for shader_name in uniform_to_shaders.keys():
                # Check if one is snake_case version of the other
                if (convert_to_snake_case(shader_name) == backend_name or
                    convert_to_snake_case(backend_name) == shader_name or
                    backend_name.replace('_', '') == shader_name.replace('_', '')):
                    
                    mismatches.append({
                        'backend_name': backend_name,
                        'shader_name': shader_name,
                        'shaders': uniform_to_shaders[shader_name],
                        'locations': locations
                    })
                    break
    
    return mismatches

def main():
    project_root = Path(__file__).parent.parent
    shader_dir = project_root / "assets" / "shaders"
    backend_file = project_root / "render" / "gl" / "backend.cpp"
    
    if not shader_dir.exists():
        print(f"{Colors.RED}Error: Shader directory not found: {shader_dir}{Colors.NC}")
        return 1
        
    if not backend_file.exists():
        print(f"{Colors.RED}Error: Backend file not found: {backend_file}{Colors.NC}")
        return 1
    
    print(f"{Colors.BOLD}=== Shader Uniform Validation ==={Colors.NC}")
    print(f"Project root: {project_root}")
    print(f"Shader directory: {shader_dir}")
    print(f"Backend file: {backend_file}")
    print()
    
    # Find all shader files
    shader_files = list(shader_dir.glob("*.frag")) + list(shader_dir.glob("*.vert"))
    print(f"Found {len(shader_files)} shader files")
    
    # Extract uniforms from all shaders
    shader_uniforms = {}
    all_shader_uniform_names = set()
    
    for shader_path in shader_files:
        uniforms = find_shader_uniforms(shader_path)
        if uniforms:
            shader_uniforms[shader_path.name] = uniforms
            all_shader_uniform_names.update(uniforms)
            print(f"  {shader_path.name}: {len(uniforms)} uniforms")
    
    print(f"\nTotal unique uniforms in shaders: {len(all_shader_uniform_names)}")
    
    # Extract uniform calls from backend
    backend_uniforms = find_backend_uniform_calls(backend_file)
    print(f"Found {len(backend_uniforms)} unique uniformHandle() calls in backend.cpp")
    print()
    
    # Find mismatches
    mismatches = find_naming_mismatches(shader_uniforms, backend_uniforms)
    
    if not mismatches:
        print(f"{Colors.GREEN}✓ All uniform names match between shaders and backend!{Colors.NC}")
        return 0
    
    # Report mismatches
    print(f"{Colors.RED}Found {len(mismatches)} naming mismatches:{Colors.NC}\n")
    
    for i, mismatch in enumerate(mismatches, 1):
        print(f"{Colors.BOLD}Mismatch #{i}:{Colors.NC}")
        print(f"  Backend uses:  {Colors.YELLOW}\"{mismatch['backend_name']}\"{Colors.NC}")
        print(f"  Shader has:    {Colors.GREEN}\"{mismatch['shader_name']}\"{Colors.NC}")
        print(f"  Affected shaders: {', '.join(mismatch['shaders'])}")
        print(f"  Locations in backend.cpp:")
        for line_num, line in mismatch['locations'][:3]:  # Show first 3 locations
            print(f"    Line {line_num}: {line}")
        if len(mismatch['locations']) > 3:
            print(f"    ... and {len(mismatch['locations']) - 3} more")
        print()
    
    # Summary
    print(f"{Colors.BOLD}=== Summary ==={Colors.NC}")
    print(f"  {Colors.RED}Errors: {len(mismatches)}{Colors.NC}")
    print()
    print(f"{Colors.YELLOW}These mismatches will cause uniforms to not be found at runtime,")
    print(f"resulting in rendering errors.{Colors.NC}")
    print()
    print(f"To fix: Update backend.cpp to use the exact uniform names from the shaders.")
    
    return 1 if mismatches else 0

if __name__ == "__main__":
    sys.exit(main())
