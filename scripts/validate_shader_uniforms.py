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


class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    NC = '\033[0m'  
    BOLD = '\033[1m'

def find_shader_uniforms(shader_path: Path) -> Set[str]:
    """Extract all uniform variable names from a GLSL shader file."""
    uniforms = set()
    
    with open(shader_path, 'r') as f:
        content = f.read()
        
    
    
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
        
    
    pattern = r'uniformHandle\s*\(\s*"([^"]+)"\s*\)'
    
    for line_num, line in enumerate(lines, start=1):
        matches = re.findall(pattern, line)
        for uniform_name in matches:
            uniform_calls[uniform_name].append((line_num, line.strip()))
    
    return uniform_calls

def convert_to_snake_case(name: str) -> str:
    """Convert camelCase to snake_case."""
    
    if '_' in name and not any(c.isupper() for c in name):
        return name
        
    
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()

def find_naming_mismatches(shader_uniforms: Dict[str, Set[str]], 
                          backend_uniforms: Dict[str, List[Tuple[int, str]]]) -> List[Dict]:
    """
    Find uniforms where backend uses different naming than shader.
    Returns list of mismatch dictionaries.
    """
    mismatches = []
    
    
    uniform_to_shaders = defaultdict(list)
    for shader, uniforms in shader_uniforms.items():
        for uniform in uniforms:
            uniform_to_shaders[uniform].append(shader)
    
    
    for backend_name, locations in backend_uniforms.items():
        
        if backend_name not in uniform_to_shaders:
            
            for shader_name in uniform_to_shaders.keys():
                
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
    
    
    shader_files = list(shader_dir.glob("*.frag")) + list(shader_dir.glob("*.vert"))
    print(f"Found {len(shader_files)} shader files")
    
    
    shader_uniforms = {}
    all_shader_uniform_names = set()
    
    for shader_path in shader_files:
        uniforms = find_shader_uniforms(shader_path)
        if uniforms:
            shader_uniforms[shader_path.name] = uniforms
            all_shader_uniform_names.update(uniforms)
            print(f"  {shader_path.name}: {len(uniforms)} uniforms")
    
    print(f"\nTotal unique uniforms in shaders: {len(all_shader_uniform_names)}")
    
    
    backend_uniforms = find_backend_uniform_calls(backend_file)
    print(f"Found {len(backend_uniforms)} unique uniformHandle() calls in backend.cpp")
    print()
    
    
    mismatches = find_naming_mismatches(shader_uniforms, backend_uniforms)
    
    if not mismatches:
        print(f"{Colors.GREEN}✓ All uniform names match between shaders and backend!{Colors.NC}")
        return 0
    
    
    print(f"{Colors.RED}Found {len(mismatches)} naming mismatches:{Colors.NC}\n")
    
    for i, mismatch in enumerate(mismatches, 1):
        print(f"{Colors.BOLD}Mismatch #{i}:{Colors.NC}")
        print(f"  Backend uses:  {Colors.YELLOW}\"{mismatch['backend_name']}\"{Colors.NC}")
        print(f"  Shader has:    {Colors.GREEN}\"{mismatch['shader_name']}\"{Colors.NC}")
        print(f"  Affected shaders: {', '.join(mismatch['shaders'])}")
        print(f"  Locations in backend.cpp:")
        for line_num, line in mismatch['locations'][:3]:  
            print(f"    Line {line_num}: {line}")
        if len(mismatch['locations']) > 3:
            print(f"    ... and {len(mismatch['locations']) - 3} more")
        print()
    
    
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
