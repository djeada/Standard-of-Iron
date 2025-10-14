# Progressive Map Loading

## Overview

The map loading system has been designed to provide a responsive user experience by loading maps incrementally rather than all at once. This prevents the UI from blocking while map JSON files are being read from disk.

## Architecture

### Backend (C++)

#### MapCatalog Class
- **Type**: `QObject` with signals/slots support
- **Location**: `game/map/map_catalog.h` and `game/map/map_catalog.cpp`

**Key Methods**:
- `loadMapsAsync()`: Initiates progressive loading of all maps
- `loadNextMap()`: Private method that loads one map and schedules the next
- `loadSingleMap(path)`: Parses a single JSON map file and returns its metadata

**Signals**:
- `mapLoaded(QVariantMap)`: Emitted each time a map is successfully loaded
- `loadingChanged(bool)`: Emitted when loading state changes
- `allMapsLoaded()`: Emitted when all maps have been processed

**Loading Flow**:
1. `loadMapsAsync()` is called
2. Scans `assets/maps/` directory for `*.json` files
3. Loads each file with a 10ms delay between loads using `QTimer::singleShot`
4. Emits `mapLoaded` for each successful load
5. Emits `allMapsLoaded` when complete

#### GameEngine Integration
- **Location**: `app/game_engine.h` and `app/game_engine.cpp`

**Properties**:
- `availableMaps`: Returns the currently loaded maps (grows as maps load)
- `mapsLoading`: Boolean indicating if maps are still being loaded

**Methods**:
- `startLoadingMaps()`: Initiates the progressive loading process

**Signal Handling**:
- Connects to `MapCatalog::mapLoaded` to append maps to `m_availableMaps`
- Connects to `MapCatalog::loadingChanged` to update `m_mapsLoading`
- Emits `availableMapsChanged()` to notify QML when new maps are available

### Frontend (QML)

#### MapSelect Component
- **Location**: `ui/qml/MapSelect.qml`

**Loading States**:

1. **Initial Loading** (`mapsLoading && list.count === 0`)
   - Shows spinning loader icon (⟳)
   - Displays "Loading maps..." message
   - Visible in both the map list and right panel

2. **Progressive Population**
   - Maps appear in the list as they load
   - User can select and interact with loaded maps immediately
   - Remaining maps continue loading in background

3. **Map Selection Skeleton** (`!selectedMapData && list.currentIndex >= 0`)
   - Shows skeleton placeholders for title and description
   - Indicates "Loading map details..." when map is selected but data pending
   - Smooth fade animation while waiting

**UI Components**:
- `loadingIndicator`: Central spinner shown when no maps loaded yet
- `loadingSkeleton`: Placeholder shown when map selected but data not ready
- Map list spinner: Shows in list area during initial load

## Benefits

### User Experience
- ✓ **Immediate Feedback**: User sees loading progress immediately
- ✓ **Progressive Interaction**: Top maps can be selected while bottom maps load
- ✓ **Visual Clarity**: Clear indicators show when data is still loading
- ✓ **No Freezing**: UI remains responsive throughout the loading process

### Performance
- ✓ **Non-blocking**: UI thread stays responsive with 10ms delays between loads
- ✓ **Lazy Display**: Maps appear as soon as they're ready
- ✓ **Graceful Degradation**: Failed map loads are silently skipped

## Error Handling

- **Invalid JSON**: Maps that fail to parse are skipped (empty QVariantMap)
- **Missing Files**: Non-existent thumbnails default to empty string
- **Empty Directory**: Shows "No maps available" after loading completes
- **Corrupt Files**: Silently ignored, won't break the loading process

## Future Enhancements

Potential improvements for even better UX:

1. **Lazy Loading**: Only load visible maps, defer off-screen maps
2. **Priority Loading**: Load recently used or favorited maps first
3. **Background Caching**: Cache parsed map metadata to speed up subsequent loads
4. **Error Notifications**: Show inline warnings for maps that failed to load
5. **Loading Progress**: Display "X of Y maps loaded" counter
6. **Cancellation**: Allow user to cancel loading if needed

## Testing

To test the progressive loading:

1. Add several map JSON files to `assets/maps/`
2. Launch the application and navigate to Map Select
3. Observe:
   - Initial spinner appears
   - Maps appear one-by-one in the list
   - Selecting early maps works before all maps load
   - Right panel shows skeleton when map selected but data pending
   - No UI freezing or blocking during load

## Technical Notes

- Uses Qt's `QTimer::singleShot` for non-blocking delayed execution
- Each map load takes ~10ms of wall time plus file I/O time
- Signal/slot mechanism ensures thread-safe communication
- QML property bindings automatically update UI when signals fire
