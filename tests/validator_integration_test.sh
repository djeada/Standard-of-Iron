#!/bin/bash
# Integration test for content_validator

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
VALIDATOR="$BUILD_DIR/bin/content_validator"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "Content Validator Integration Tests"
echo "===================================="
echo ""

# Check if validator exists
if [ ! -x "$VALIDATOR" ]; then
    echo -e "${RED}✗ Validator not found at: $VALIDATOR${NC}"
    echo "Please build the project first with: make build"
    exit 1
fi

echo -e "${GREEN}✓ Validator found${NC}"
echo ""

# Test 1: Valid assets directory
echo "Test 1: Validating assets directory..."
if "$VALIDATOR" "$REPO_ROOT/assets"; then
    echo -e "${GREEN}✓ Test 1 passed: Assets validated successfully${NC}"
else
    echo -e "${RED}✗ Test 1 failed: Asset validation failed${NC}"
    exit 1
fi
echo ""

# Test 2: Invalid directory (should fail gracefully)
echo "Test 2: Testing with invalid directory..."
if "$VALIDATOR" "/nonexistent/directory" 2>/dev/null; then
    echo -e "${RED}✗ Test 2 failed: Should have failed with invalid directory${NC}"
    exit 1
else
    echo -e "${GREEN}✓ Test 2 passed: Correctly failed with invalid directory${NC}"
fi
echo ""

# Test 3: Create temporary test missions and campaigns
echo "Test 3: Testing with custom test data..."
TEMP_DIR=$(mktemp -d)
mkdir -p "$TEMP_DIR/missions"
mkdir -p "$TEMP_DIR/campaigns"

# Create a valid test mission
cat > "$TEMP_DIR/missions/test_mission.json" << 'EOF'
{
  "id": "test_mission",
  "title": "Test Mission",
  "summary": "A test mission",
  "map_path": ":/assets/maps/map_forest.json",
  "player_setup": {
    "nation": "roman_republic",
    "faction": "roman",
    "color": "red",
    "starting_units": [],
    "starting_buildings": [],
    "starting_resources": {"gold": 0, "food": 0}
  },
  "ai_setups": [],
  "victory_conditions": [
    {"type": "destroy_all_enemies", "description": "Win"}
  ],
  "defeat_conditions": [],
  "events": []
}
EOF

# Create a valid test campaign
cat > "$TEMP_DIR/campaigns/test_campaign.json" << 'EOF'
{
  "id": "test_campaign",
  "title": "Test Campaign",
  "description": "A test campaign",
  "missions": [
    {
      "mission_id": "test_mission",
      "order_index": 0
    }
  ]
}
EOF

if "$VALIDATOR" "$TEMP_DIR"; then
    echo -e "${GREEN}✓ Test 3 passed: Custom test data validated${NC}"
else
    echo -e "${RED}✗ Test 3 failed: Custom test data validation failed${NC}"
    rm -rf "$TEMP_DIR"
    exit 1
fi
rm -rf "$TEMP_DIR"
echo ""

# Test 4: Invalid JSON (should fail)
echo "Test 4: Testing with invalid JSON..."
TEMP_DIR=$(mktemp -d)
mkdir -p "$TEMP_DIR/missions"

cat > "$TEMP_DIR/missions/invalid.json" << 'EOF'
{
  "id": "invalid"
  "missing": "comma"
}
EOF

if "$VALIDATOR" "$TEMP_DIR" 2>/dev/null; then
    echo -e "${RED}✗ Test 4 failed: Should have failed with invalid JSON${NC}"
    rm -rf "$TEMP_DIR"
    exit 1
else
    echo -e "${GREEN}✓ Test 4 passed: Correctly failed with invalid JSON${NC}"
fi
rm -rf "$TEMP_DIR"
echo ""

# Test 5: Missing required fields
echo "Test 5: Testing with missing required fields..."
TEMP_DIR=$(mktemp -d)
mkdir -p "$TEMP_DIR/missions"

cat > "$TEMP_DIR/missions/incomplete.json" << 'EOF'
{
  "title": "Incomplete Mission"
}
EOF

if "$VALIDATOR" "$TEMP_DIR" 2>/dev/null; then
    echo -e "${RED}✗ Test 5 failed: Should have failed with missing fields${NC}"
    rm -rf "$TEMP_DIR"
    exit 1
else
    echo -e "${GREEN}✓ Test 5 passed: Correctly failed with missing fields${NC}"
fi
rm -rf "$TEMP_DIR"
echo ""

# Test 6: Campaign with non-contiguous order indices
echo "Test 6: Testing campaign with non-contiguous order indices..."
TEMP_DIR=$(mktemp -d)
mkdir -p "$TEMP_DIR/missions"
mkdir -p "$TEMP_DIR/campaigns"

# Create missions
cat > "$TEMP_DIR/missions/mission1.json" << 'EOF'
{
  "id": "mission1",
  "title": "Mission 1",
  "summary": "First mission",
  "map_path": ":/assets/maps/map_forest.json",
  "player_setup": {"nation": "roman_republic", "faction": "roman", "color": "red", "starting_units": [], "starting_buildings": [], "starting_resources": {"gold": 0, "food": 0}},
  "ai_setups": [],
  "victory_conditions": [{"type": "destroy_all_enemies", "description": "Win"}],
  "defeat_conditions": [],
  "events": []
}
EOF

cat > "$TEMP_DIR/missions/mission2.json" << 'EOF'
{
  "id": "mission2",
  "title": "Mission 2",
  "summary": "Second mission",
  "map_path": ":/assets/maps/map_forest.json",
  "player_setup": {"nation": "roman_republic", "faction": "roman", "color": "red", "starting_units": [], "starting_buildings": [], "starting_resources": {"gold": 0, "food": 0}},
  "ai_setups": [],
  "victory_conditions": [{"type": "destroy_all_enemies", "description": "Win"}],
  "defeat_conditions": [],
  "events": []
}
EOF

# Campaign with gaps in order_index (0, 2 - missing 1)
cat > "$TEMP_DIR/campaigns/bad_campaign.json" << 'EOF'
{
  "id": "bad_campaign",
  "title": "Bad Campaign",
  "description": "Campaign with order gaps",
  "missions": [
    {"mission_id": "mission1", "order_index": 0},
    {"mission_id": "mission2", "order_index": 2}
  ]
}
EOF

if "$VALIDATOR" "$TEMP_DIR" 2>/dev/null; then
    echo -e "${RED}✗ Test 6 failed: Should have failed with non-contiguous indices${NC}"
    rm -rf "$TEMP_DIR"
    exit 1
else
    echo -e "${GREEN}✓ Test 6 passed: Correctly failed with non-contiguous indices${NC}"
fi
rm -rf "$TEMP_DIR"
echo ""

echo "===================================="
echo -e "${GREEN}All tests passed!${NC}"
echo ""
