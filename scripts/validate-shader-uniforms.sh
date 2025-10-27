#!/bin/bash
# Script to validate shader uniform naming consistency
# Helps catch camelCase vs snake_case mismatches between shaders and C++ code

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Shader Uniform Validation ==="
echo "Checking for naming inconsistencies between shaders and backend code..."
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

ERRORS=0
WARNINGS=0

# Common uniform naming patterns that should be consistent
# Format: "camelCase|snake_case|shader_pattern"
declare -a UNIFORM_PATTERNS=(
    "viewProj|view_proj|u_viewProj"
    "lightDir|light_dir|u_lightDir"
    "tileSize|tile_size|u_tileSize"
    "detailNoiseScale|detail_noiseScale|u_detailNoiseScale"
    "cameraRight|camera_right|u_cameraRight"
    "cameraForward|camera_forward|u_cameraForward"
)

echo "Checking shader files in assets/shaders/..."

# Find all fragment and vertex shaders
SHADER_FILES=$(find "$PROJECT_ROOT/assets/shaders" -type f \( -name "*.frag" -o -name "*.vert" \))

for shader_file in $SHADER_FILES; do
    shader_name=$(basename "$shader_file")
    
    # Extract uniform declarations from shader
    uniforms=$(grep -oP 'uniform\s+\w+\s+\K\w+' "$shader_file" 2>/dev/null || true)
    
    if [ -z "$uniforms" ]; then
        continue
    fi
    
    echo "Checking: $shader_name"
    
    # Check each uniform against common patterns
    while IFS= read -r uniform; do
        # Check if this uniform follows a known pattern
        for pattern in "${UNIFORM_PATTERNS[@]}"; do
            IFS='|' read -r camel snake shader_expected <<< "$pattern"
            
            # If uniform contains the pattern
            if [[ "$uniform" == *"$camel"* ]] || [[ "$uniform" == *"$snake"* ]]; then
                # Check if it matches the expected shader pattern
                if [[ "$uniform" != "$shader_expected" ]]; then
                    echo -e "  ${YELLOW}WARNING${NC}: Uniform '$uniform' might not match expected pattern '$shader_expected'"
                    ((WARNINGS++))
                fi
                
                # Now check if backend.cpp is using the correct name
                if grep -q "uniformHandle(\"$uniform\")" "$PROJECT_ROOT/render/gl/backend.cpp" 2>/dev/null; then
                    echo -e "  ${GREEN}✓${NC} Found correct usage: uniformHandle(\"$uniform\")"
                else
                    # Check if backend is using wrong naming convention
                    if [[ "$uniform" == "$shader_expected" ]]; then
                        # Check for common wrong patterns
                        if [[ "$shader_expected" == "u_viewProj" ]] && grep -q "uniformHandle(\"u_view_proj\")" "$PROJECT_ROOT/render/gl/backend.cpp"; then
                            echo -e "  ${RED}ERROR${NC}: backend.cpp uses 'u_view_proj' but shader has 'u_viewProj'"
                            ((ERRORS++))
                        elif [[ "$shader_expected" == "u_lightDir" ]] && grep -q "uniformHandle(\"u_light_dir\")" "$PROJECT_ROOT/render/gl/backend.cpp"; then
                            echo -e "  ${RED}ERROR${NC}: backend.cpp uses 'u_light_dir' but shader has 'u_lightDir'"
                            ((ERRORS++))
                        elif [[ "$shader_expected" == "u_tileSize" ]] && grep -q "uniformHandle(\"u_tile_size\")" "$PROJECT_ROOT/render/gl/backend.cpp"; then
                            echo -e "  ${RED}ERROR${NC}: backend.cpp uses 'u_tile_size' but shader has 'u_tileSize'"
                            ((ERRORS++))
                        elif [[ "$shader_expected" == "u_cameraRight" ]] && grep -q "uniformHandle(\"u_camera_right\")" "$PROJECT_ROOT/render/gl/backend.cpp"; then
                            echo -e "  ${RED}ERROR${NC}: backend.cpp uses 'u_camera_right' but shader has 'u_cameraRight'"
                            ((ERRORS++))
                        fi
                    fi
                fi
            fi
        done
    done <<< "$uniforms"
done

echo ""
echo "=== Validation Summary ==="
echo -e "Errors: ${RED}$ERRORS${NC}"
echo -e "Warnings: ${YELLOW}$WARNINGS${NC}"

if [ $ERRORS -gt 0 ]; then
    echo -e "${RED}Validation FAILED - please fix the errors above${NC}"
    exit 1
elif [ $WARNINGS -gt 0 ]; then
    echo -e "${YELLOW}Validation passed with warnings${NC}"
    exit 0
else
    echo -e "${GREEN}Validation PASSED - all uniforms look correct${NC}"
    exit 0
fi
