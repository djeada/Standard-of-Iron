# Campaign UI/UX Implementation

This document describes the new Campaign Screen UI implementation for Standard of Iron.

## Overview

The Campaign Screen provides an enhanced user interface for selecting and playing campaign missions, with a clear visual progression system and an interactive Mediterranean strategic map.

## Components

### 1. CampaignScreen.qml
The main campaign interface component with the following features:

**Layout:**
- Split pane design: Mission list on left (~45%), Mediterranean map on right (~55%)
- Campaign header with title, description, and back button
- Progress bar showing completed/total missions ratio
- "Continue Campaign" button to auto-select next available mission
- Mission detail panel appears at bottom when mission is selected

**Key Properties:**
- `current_campaign`: The active campaign object
- `campaigns`: List of all available campaigns
- `selected_mission_index`: Currently selected mission index

**Signals:**
- `mission_selected(campaign_id, mission_id)`: Emitted when user starts a mission
- `cancelled()`: Emitted when user backs out to main menu

### 2. MissionListItem.qml
Individual mission list entry component displaying:

**Visual Elements:**
- Mission number badge (circular, colored based on completion status)
- Mission title and description (2-line max)
- Status badge: "✓ Done" (green), "🔒 Locked" (gray), or "Available" (blue)
- Difficulty stars (based on difficulty_modifier field)
- Hover and selection states with color transitions

**Properties:**
- `mission_data`: Mission object with fields:
  - `mission_id`: Unique mission identifier
  - `order_index`: Sequential mission number (0-based)
  - `intro_text`: Brief mission description
  - `unlocked`: Boolean - can the player start this mission?
  - `completed`: Boolean - has player completed this mission?
  - `difficulty_modifier`: Float - difficulty rating (displayed as stars)

### 3. MissionDetailPanel.qml
Expandable panel showing detailed mission information:

**Content:**
- Mission title and full description
- Primary objectives (placeholder for now)
- Stats section (attempts, best time - placeholders)
- "Start Mission" / "Replay Mission" button
- Lock status with tooltip explaining why mission is locked

**Features:**
- Animated height transition when shown/hidden
- Button disabled state for locked missions
- Tooltip shows unlock conditions

### 4. MediterraneanMapPanel.qml
Interactive 3D campaign map component:

**Features:**
- Uses CampaignMapView C++ widget for 3D rendering
- Drag to rotate camera (orbit controls)
- Scroll wheel to zoom in/out
- Province hover detection with tooltips
- Map legend showing faction colors (Rome/Carthage/Neutral)
- Control hints for user interaction

**Province Tooltips:**
- Province name
- Controlling faction
- Positioned near cursor with smart boundary detection

**Camera Controls:**
- Yaw rotation: 360° horizontal orbit
- Pitch rotation: 5° to 85° vertical angle
- Distance: 1.2 to 5.0 zoom range

## Data Flow

### Campaign Loading
```
1. User clicks "Campaign" in main menu
2. GameEngine.load_campaigns() is called
3. SaveLoadService.list_campaigns() loads campaign JSON files
4. Campaign data includes:
   - Campaign metadata (id, title, description)
   - Mission list with order_index, intro_text, difficulty
   - Unlock/completion status (first mission unlocked by default)
5. CampaignScreen displays the campaign list
```

### Mission Selection
```
1. User clicks mission in list
2. selected_mission_index is updated
3. MissionDetailPanel appears with mission info
4. Map camera can be focused on mission's region (future enhancement)
```

### Starting Mission
```
1. User clicks "Start Mission" button
2. mission_selected signal emitted with campaign_id/mission_id
3. Main.qml receives signal
4. GameEngine.start_campaign_mission() loads mission definition
5. Mission begins, HUD becomes visible
```

## Database Schema

Mission unlock/completion state is stored in the save database:

```sql
-- Campaign progress tracking (future implementation)
CREATE TABLE campaign_progress (
    campaign_id TEXT PRIMARY KEY,
    missions_completed INTEGER DEFAULT 0,
    last_mission_id TEXT,
    updated_at TIMESTAMP
);

CREATE TABLE mission_stats (
    campaign_id TEXT,
    mission_id TEXT,
    attempts INTEGER DEFAULT 0,
    completed BOOLEAN DEFAULT FALSE,
    best_time_seconds INTEGER,
    completed_at TIMESTAMP,
    PRIMARY KEY (campaign_id, mission_id)
);
```

Currently, mission state is loaded from campaign JSON and decorated with default unlock logic (first mission unlocked).

## Styling

All components use the Theme singleton for consistent styling:

**Colors:**
- Success: Green tones (#1e4a2c bg, #8fdc9f text)
- Info: Blue tones (#1a3a5a bg, #7ab8e8 text)
- Locked: Gray tones (#1a2a32 bg, #4f6a75 text)
- Warning: Orange tones (#f5a623 text)

**Spacing:**
- Tiny: 4px
- Small: 8px
- Medium: 12px
- Large: 16px
- XLarge: 20px

**Animations:**
- Normal: 160ms for color transitions and layout changes

## Naming Conventions

All code follows snake_case naming:
- Properties: `current_campaign`, `selected_mission_index`
- Functions: `refresh_campaigns()`, `select_next_unlocked_mission()`
- Signals: `mission_selected()`, `start_mission_clicked()`

## Future Enhancements

### Mission Progression System
- [ ] Track completion state in database
- [ ] Unlock subsequent missions when previous completed
- [ ] Store player statistics (attempts, completion time)
- [ ] Award medals/stars for performance

### Map Integration
- [ ] Link missions to specific provinces
- [ ] Highlight mission province on map when selected
- [ ] Auto-pan/zoom camera to mission region
- [ ] Show Hannibal's campaign path progression
- [ ] Animate faction control changes

### Mission Detail Enhancements
- [ ] Load actual objectives from mission definition
- [ ] Display recommended unit compositions
- [ ] Show terrain preview thumbnail
- [ ] Historical context notes
- [ ] Secondary objectives display

### Polish
- [ ] Mission unlock animations
- [ ] Victory celebration effects
- [ ] Campaign completion screen
- [ ] Leaderboards for best times
- [ ] Achievement notifications

## Testing

To test the Campaign UI:

1. Build the project with Qt 5/6
2. Run the application
3. Click "Campaign" in main menu
4. Verify campaign list displays
5. Click "Continue Campaign" to select first mission
6. Verify mission detail panel appears
7. Interact with Mediterranean map (drag, scroll)
8. Hover over provinces to see tooltips
9. Click "Start Mission" to launch
10. Verify mission loads correctly

## Troubleshooting

**Campaign list is empty:**
- Check that campaign JSON files exist in assets/campaigns/
- Verify SaveLoadService.list_campaigns() is called
- Check console for JSON parsing errors

**Map doesn't render:**
- Verify CampaignMapView C++ class is registered
- Check OpenGL context initialization
- Ensure campaign map assets are loaded

**Mission won't start:**
- Verify mission JSON exists in assets/missions/
- Check GameEngine.start_campaign_mission() implementation
- Verify selected_player_id is set correctly

**Theme colors missing:**
- Ensure Theme singleton is registered in main.cpp
- Verify all Theme properties are defined in theme.h
- Check QML imports include StandardOfIron 1.0
