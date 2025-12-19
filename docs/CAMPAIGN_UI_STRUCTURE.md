# Campaign UI Structure

## Component Hierarchy

```
CampaignScreen.qml (Main Container)
│
├── Header Section
│   ├── Campaign Title & Description
│   └── Back Button
│
├── Progress Bar Section
│   ├── Progress Indicator (Completed / Total)
│   └── Continue Campaign Button
│
├── Main Content (Split Pane)
│   │
│   ├── Left Pane (45% width) - Mission List
│   │   └── ScrollView
│   │       └── ListView
│   │           └── MissionListItem.qml (Repeater)
│   │               ├── Mission Number Badge
│   │               ├── Mission Title
│   │               ├── Status Badge (Locked/Available/Done)
│   │               ├── Description Text
│   │               └── Difficulty Stars
│   │
│   └── Right Pane (55% width) - Interactive Map
│       └── MediterraneanMapPanel.qml
│           ├── CampaignMapView (C++ 3D Renderer)
│           ├── Province Hover Tooltip
│           ├── Map Legend (Rome/Carthage/Neutral)
│           └── Control Hints
│
└── Mission Detail Panel (Bottom, Expandable)
    └── MissionDetailPanel.qml
        ├── Mission Title & Description
        ├── Objectives Section
        ├── Stats Section
        └── Start/Replay Button
```

## Data Flow Diagram

```
User Action → UI Component → Signal → Main.qml → GameEngine
                                                      ↓
Campaign JSON Files ←── SaveLoadService ←──────────────┘
       ↓
   Campaign Data
       ↓
CampaignScreen ← campaigns property
       ↓
Mission List Items (with unlock/completion state)
```

## State Management

```
Campaign State:
├── campaigns[]
│   ├── id
│   ├── title
│   ├── description
│   ├── unlocked (always true for now)
│   ├── completed (always false for now)
│   └── missions[]
│       ├── mission_id
│       ├── order_index
│       ├── intro_text
│       ├── outro_text
│       ├── difficulty_modifier
│       ├── unlocked (order_index == 0 ? true : false)
│       └── completed (false by default)
│
└── selected_mission_index (tracked by UI)
```

## User Interaction Flow

### Viewing Campaigns
```
Main Menu
   ↓ (Click "Campaign")
Campaign Screen
   ↓ (Loads campaigns)
Display Campaign List
   ↓ (Auto-select first campaign)
Show Mission List + Mediterranean Map
```

### Starting a Mission
```
Mission List Item
   ↓ (Click mission)
selected_mission_index updated
   ↓
Mission Detail Panel appears
   ↓ (Click "Start Mission")
mission_selected(campaign_id, mission_id) signal
   ↓
Main.qml receives signal
   ↓
GameEngine.start_campaign_mission()
   ↓
Mission loads and game begins
```

### Continuing Campaign
```
Campaign Screen
   ↓ (Click "Continue Campaign")
select_next_unlocked_mission() called
   ↓
Finds first unlocked incomplete mission
   ↓
selected_mission_index updated
   ↓
Mission Detail Panel shows selected mission
```

## Mission Status States

```
┌─────────────────────────────────────────┐
│ Mission Status State Machine             │
├─────────────────────────────────────────┤
│                                          │
│  [Locked] ──────► [Available] ──────► [Completed]
│     │                 │                    │
│     │                 │                    │
│  🔒 Gray         ⓘ Blue             ✓ Green
│  Cannot start     Can start         Can replay
│                                          │
│                                          │
│                                     (Stays selectable)
└─────────────────────────────────────────┘

Current Logic:
- order_index == 0 → Available
- order_index > 0 → Locked
- completed → Completed (not yet persisted)

Future Logic:
- Previous mission completed → Available
- Mission completed → Completed (persisted in DB)
```

## Map Interaction

```
Camera Controls:
├── Drag (Left Mouse Button)
│   ├── Horizontal movement → Yaw rotation (0° - 360°)
│   └── Vertical movement → Pitch rotation (5° - 85°)
│
├── Scroll Wheel
│   └── Zoom → Distance adjustment (1.2 - 5.0)
│
└── Hover
    └── Province detection → Show tooltip with:
        ├── Province name
        ├── Controlling faction
        └── Historical note (future)

Province Colors:
├── Rome: Red (#d01f1a)
├── Carthage: Orange (#cc8f47)
└── Neutral: Gray (#3a3a3a)
```

## Theme Colors Reference

```
Status Colors:
├── Success (Completed)
│   ├── Background: #1e4a2c
│   ├── Border: #2d6b3f
│   └── Text: #8fdc9f
│
├── Info (Available)
│   ├── Background: #1a3a5a
│   ├── Border: #2a5a8a
│   └── Text: #7ab8e8
│
├── Disabled (Locked)
│   ├── Background: #1a2a32
│   ├── Border: #0f2b34
│   └── Text: #4f6a75
│
└── Warning (Difficulty)
    └── Text: #f5a623

Interactive States:
├── Hover: #184c7a
├── Selected: #1f8bf5
└── Accent: #9fd9ff
```

## Performance Considerations

### Optimization Strategies
1. **Mission List**
   - Uses ListView for efficient rendering
   - Only visible items are rendered
   - Smooth scrolling with native scroll bars

2. **Mediterranean Map**
   - C++ CampaignMapView for hardware-accelerated 3D
   - Cached province geometry
   - Batched rendering for provinces
   - Efficient ray-casting for hover detection

3. **Animations**
   - Color transitions: 160ms
   - Layout changes: smooth Behavior animations
   - No heavy computations in binding expressions

4. **Memory**
   - Campaign data loaded once on screen visibility
   - Province data cached in CampaignMapView
   - Minimal QML object creation

## File Size Summary

```
New QML Components:
- CampaignScreen.qml:          ~12 KB
- MissionListItem.qml:         ~6 KB
- MissionDetailPanel.qml:      ~4 KB
- MediterraneanMapPanel.qml:   ~6 KB
Total QML:                     ~28 KB

Modified C++ Files:
- save_storage.cpp:            +50 lines (mission unlock fields)
- theme.h:                     +14 lines (new color properties)
- CMakeLists.txt:              +4 lines (QML file registration)

Documentation:
- docs/CAMPAIGN_UI.md:         ~7 KB
- docs/CAMPAIGN_UI_STRUCTURE.md: ~6 KB
Total Docs:                    ~13 KB

Total Impact:                  ~41 KB new content
```
