# Raw Pointer to Smart Pointer Migration Tracking

## Overview

This document tracks the remaining raw pointer usage in the Standard of Iron codebase and provides guidance for migrating them to smart pointers where appropriate.

**Current Status:**
- **Member variable raw pointers:** 26 instances
- **System interface raw pointers:** 15+ system implementations
- **Component raw pointers:** 5 types in Unit class
- **Existing smart pointer usage:** 244 instances

**Migration Goal:**
Replace ownership-semantics raw pointers with appropriate smart pointers (`std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr`) while keeping non-owning raw pointers for performance-critical paths and observer patterns.

---

## Categories and Migration Strategy

### 1. Member Variable Raw Pointers (26 instances)

#### 1.1 Qt GUI Objects (Not for Migration)
**Rationale:** Qt objects use parent-child ownership model. Migration unnecessary and potentially harmful.

| File | Line | Pointer | Ownership |
|------|------|---------|-----------|
| `app/core/game_engine.h` | 360 | `QQuickWindow *m_window` | Owned by Qt hierarchy |
| `app/core/language_manager.h` | 29 | `QTranslator *m_translator` | Owned by QCoreApplication |
| `tools/map_editor/editor_window.h` | 24 | `QWidget *m_renderWidget` | Owned by parent widget |

**Action:** ✅ Keep as-is (Qt parent-child ownership)

---

#### 1.2 Service/System Dependencies (Consider Migration)
**Rationale:** These are typically non-owning references to external services. Current usage is safe but could benefit from better lifetime documentation.

| File | Line | Pointer | Current Pattern | Recommended Action |
|------|------|---------|-----------------|-------------------|
| `game/systems/selection_system.h` | 65-66 | `SelectionSystem *m_selection_system`<br>`PickingService *m_picking_service` | Non-owning observer | Document as non-owning; consider raw references |
| `app/core/input_command_handler.h` | 70-76 | `World *m_world`<br>`SelectionController *`<br>`CommandController *`<br>`CursorManager *`<br>`HoverTracker *`<br>`PickingService *`<br>`Camera *` | Constructor-injected dependencies | Keep raw pointers; document as non-owning observers |
| `app/models/selected_units_model.h` | 34 | `GameEngine *m_engine` | Back-reference to parent | Keep raw pointer; document as observer |
| `game/audio/Sound.h` | 29 | `MiniaudioBackend *m_backend` | External service reference | Keep raw pointer; consider std::weak_ptr if backend can be destroyed |

**Action:** 📝 Document ownership semantics clearly in class documentation

**Example Documentation:**
```cpp
class InputCommandHandler {
public:
  /// @param world Non-owning pointer to game world. Must outlive this handler.
  /// @param selection_controller Non-owning pointer. Must outlive this handler.
  InputCommandHandler(Engine::Core::World *world,
                      Game::Systems::SelectionController *selection_controller,
                      /* ... */);
private:
  Engine::Core::World *m_world; ///< Non-owning observer. Not managed by this class.
  // ...
};
```

---

#### 1.3 Renderer Infrastructure (High Priority for Review)
**Rationale:** These pointers often represent state caching and object relationships. Need case-by-case analysis.

##### State Caching (Keep as Raw)
| File | Line | Pointer | Usage | Action |
|------|------|---------|-------|--------|
| `render/gl/backend.h` | 166-167 | `Shader *m_lastBoundShader`<br>`Texture *m_lastBoundTexture` | State cache for OpenGL optimization | ✅ Keep as raw (non-owning cache) |
| `render/scene_renderer.h` | 195 | `Shader *m_current_shader` | Current shader state | ✅ Keep as raw (non-owning) |

##### Object References (Review Ownership)
| File | Line | Pointer | Current Usage | Recommended Action |
|------|------|---------|---------------|-------------------|
| `render/scene_renderer.h` | 174 | `Camera *m_camera` | Set via `set_camera()` | Keep as raw; document as non-owning |
| `render/scene_renderer.h` | 177 | `DrawQueue *m_active_queue` | Points to owned or external queue | Review if owner should be `unique_ptr` |
| `render/submitter.h` | 199-200 | `DrawQueue *m_queue`<br>`Shader *m_shader` | Constructor-injected | Keep raw; document as non-owning |
| `render/submitter.h` | 298-299 | `ISubmitter *m_fallback`<br>`PrimitiveBatcher *m_batcher` | Delegation pattern | Keep raw; document as non-owning |
| `render/gl/backend.h` | 163-164 | `Shader *m_basicShader`<br>`Shader *m_gridShader` | Resource ownership | ⚠️ **HIGH PRIORITY:** Consider `std::unique_ptr` |

##### BarracksFlagRenderer (Review Ownership)
| File | Line | Pointer | Usage | Recommended Action |
|------|------|---------|-------|-------------------|
| `render/entity/barracks_flag_renderer.h` | 72-75 | `QueueSubmitter *m_queue`<br>`Shader *m_previousQueueShader`<br>`Renderer *m_renderer`<br>`Shader *m_previousRendererShader` | State saving pattern | Keep raw for state cache; consider RAII guard object |

**Action:** 🔍 Review each renderer class individually

##### Persistent Buffer (Low Level OpenGL)
| File | Line | Pointer | Usage | Action |
|------|------|---------|-------|--------|
| `render/gl/persistent_buffer.h` | 182 | `void *m_mappedPtr` | OpenGL mapped buffer pointer | ✅ Keep as `void*` (OpenGL API requirement) |

---

#### 1.4 World Pointers (Design Pattern - Keep)
**Rationale:** The ECS architecture passes `World*` throughout the system. These are non-owning references to the central game world.

| File | Line | Pointer | Pattern | Action |
|------|------|---------|---------|--------|
| `game/audio/AudioEventHandler.h` | 45 | `Engine::Core::World *m_world` | Observer reference | ✅ Keep as raw; document as non-owning |
| `game/units/unit.h` | 66 | `Engine::Core::World *m_world` | Back-reference to world | ✅ Keep as raw; document as non-owning |
| `game/systems/victory_service.h` | 80 | `Engine::Core::World *m_worldPtr` | Service reference | ✅ Keep as raw; document as non-owning |

**Design Rationale:**
- World is owned by the game engine
- Systems and entities hold non-owning observers
- Lifetime guarantees: World outlives all systems and entities
- Performance: Avoid reference counting overhead in hot paths

---

### 2. System Interface Raw Pointers (Design Pattern - Keep)

The `System::update(World *world, float delta_time)` interface is used consistently across 15+ system implementations.

**Files Affected:**
- `game/core/system.h` (base interface)
- `game/systems/catapult_attack_system.h`
- `game/systems/production_system.h`
- `game/systems/arrow_system.h`
- `game/systems/cleanup_system.h`
- `game/systems/ballista_attack_system.h`
- `game/systems/projectile_system.h`
- `game/systems/patrol_system.h`
- `game/systems/ai_system.h`
- `game/systems/healing_beam_system.h`
- `game/systems/combat_system.h`
- `game/systems/healing_system.h`
- `game/systems/movement_system.h`
- `game/systems/selection_system.h`
- `game/systems/terrain_alignment_system.h`
- `game/systems/capture_system.h`

**Rationale for Raw Pointers:**
1. **Performance:** Systems are called every frame. No reference counting overhead.
2. **Clear semantics:** `World*` is never null and always valid during system updates.
3. **Established pattern:** Consistent across entire ECS architecture.
4. **No ownership:** Systems never own the World.

**Action:** ✅ Keep as raw pointers; document in `System` base class

**Recommended Documentation:**
```cpp
namespace Engine::Core {

class System {
public:
  /// Update system state for the current frame.
  /// @param world Non-owning pointer to game world. Never null. Guaranteed to outlive the update call.
  /// @param delta_time Time elapsed since last update in seconds.
  virtual void update(World *world, float delta_time) = 0;
};

} // namespace Engine::Core
```

---

### 3. Component Raw Pointers in Unit Class (Design Pattern - Keep)

**File:** `game/units/unit.h` lines 70-74

```cpp
Engine::Core::TransformComponent *m_t = nullptr;
Engine::Core::RenderableComponent *m_r = nullptr;
Engine::Core::UnitComponent *m_u = nullptr;
Engine::Core::MovementComponent *m_mv = nullptr;
Engine::Core::AttackComponent *m_atk = nullptr;
```

**Rationale:**
- Components are owned by the World's component arrays
- Units cache pointers for performance (avoid repeated lookups)
- Pointers are invalidated when entity is destroyed
- This is a standard ECS cache pattern

**Action:** ✅ Keep as raw pointers; document as cached observers

**Recommended Documentation:**
```cpp
protected:
  /// Cached non-owning pointers to entity components.
  /// These are owned by the World's component arrays.
  /// Pointers are populated by ensureCoreComponents() and may be null if component doesn't exist.
  /// @warning These become invalid when the entity is destroyed.
  Engine::Core::TransformComponent *m_t = nullptr;
  Engine::Core::RenderableComponent *m_r = nullptr;
  Engine::Core::UnitComponent *m_u = nullptr;
  Engine::Core::MovementComponent *m_mv = nullptr;
  Engine::Core::AttackComponent *m_atk = nullptr;
```

---

## ✅ Completed Investigations (Previously High Priority)

All unclear ownership cases have been investigated and resolved. No smart pointer migrations are needed.

### ✅ RESOLVED: Shader Ownership in Backend

**File:** `render/gl/backend.h`

```cpp
Shader *m_basicShader = nullptr;
Shader *m_gridShader = nullptr;
```

**Status:** Non-owning cache pointers. No migration needed.

**Investigation Results:**
- Shaders are retrieved from `ShaderCache` via `m_shaderCache->get("basic")` and `m_shaderCache->get("grid")`
- `ShaderCache` owns shaders using `std::unique_ptr<Shader>` in internal maps
- `Backend` and pipeline classes store raw pointers for fast access
- Lifetime: ShaderCache outlives Backend, guaranteeing pointer validity

**Actual Pattern:**
```cpp
// shader_cache.h
class ShaderCache {
  std::unordered_map<QString, std::unique_ptr<Shader>> m_named;
  auto get(const QString &name) const -> Shader * {
    auto it = m_named.find(name);
    return (it != m_named.end()) ? it->second.get() : nullptr;
  }
};

// backend.cpp
void Backend::initialize() {
  m_basicShader = m_shaderCache->get(QStringLiteral("basic"));
  m_gridShader = m_shaderCache->get(QStringLiteral("grid"));
}
```

**Conclusion:** This is correct usage. Raw pointers serve as non-owning cache for performance. ShaderCache is the single owner.

---

### ✅ RESOLVED: DrawQueue Ownership

**File:** `render/scene_renderer.h`

```cpp
DrawQueue m_queues[2];
DrawQueue *m_active_queue = nullptr;
```

**Status:** Non-owning pointer to owned array element. No migration needed.

**Investigation Results:**
- Renderer owns `DrawQueue m_queues[2]` as direct members
- `m_active_queue` points to either `&m_queues[0]` or `&m_queues[1]`
- This is a double-buffering pattern for render commands
- Lifetime: `m_active_queue` always points to valid memory owned by Renderer

**Actual Pattern:**
```cpp
// scene_renderer.cpp
Renderer::Renderer() { 
  m_active_queue = &m_queues[m_fill_queue_index]; 
}

void Renderer::begin_frame() {
  m_active_queue = &m_queues[m_fill_queue_index];
  m_active_queue->clear();
}
```

**Conclusion:** This is correct usage. Raw pointer for internal state switching between owned buffers.

---

## Best Practices Going Forward

### When to Use Smart Pointers

#### Use `std::unique_ptr` when:
- ✅ Class owns the pointed-to object exclusively
- ✅ Object lifetime matches the owner's lifetime
- ✅ Clear single-owner semantics

**Example:**
```cpp
class Renderer {
  std::unique_ptr<ResourceManager> m_resources; // Renderer owns resources
};
```

#### Use `std::shared_ptr` when:
- ✅ Multiple owners need to share object lifetime
- ✅ Lifetime extends beyond any single owner
- ✅ Resource caching across multiple systems

**Example:**
```cpp
class TextureCache {
  std::map<QString, std::shared_ptr<Texture>> m_cache;
};
```

#### Use `std::weak_ptr` when:
- ✅ Observer pattern (non-owning reference to `shared_ptr`)
- ✅ Breaking circular dependencies
- ✅ Optional reference that may be null

**Example:**
```cpp
class Entity {
  std::weak_ptr<Texture> m_texture; // May be destroyed by cache
};
```

#### Use raw pointers when:
- ✅ Non-owning observer (guaranteed valid lifetime)
- ✅ Performance-critical hot path
- ✅ Interface parameter (caller guarantees lifetime)
- ✅ Qt parent-child ownership model
- ✅ Component caching in ECS
- ✅ OpenGL API requirements (`void*`)

---

### Documentation Requirements

When keeping raw pointers, **always document**:
1. **Ownership:** Who owns this pointer?
2. **Lifetime:** What guarantees its validity?
3. **Nullability:** Can it be null?

**Example:**
```cpp
/// @param world Non-owning pointer to game world. Must outlive this object. Never null.
/// @param camera Non-owning pointer to camera. May be null if not set.
void initialize(Engine::Core::World *world, Camera *camera);
```

---

## Migration Checklist

- [x] Survey all raw pointer usage
- [x] Categorize by usage pattern
- [x] Identify ownership semantics
- [x] Investigate Backend shader ownership (✅ Non-owning cache)
- [x] Investigate DrawQueue ownership (✅ Internal state pointer)
- [ ] Document all non-owning raw pointers with doxygen comments
- [ ] Add doxygen comments to System interface
- [ ] Add doxygen comments to Unit component pointers
- [ ] Add doxygen comments to InputCommandHandler dependencies
- [ ] Add doxygen comments to renderer infrastructure pointers
- [ ] Create example patterns document
- [ ] Update CONTRIBUTING.md with pointer usage guidelines

**Next Steps:** Focus on documentation rather than migration, as all raw pointers are correctly used.

---

## Summary Statistics

| Category | Count | Smart Pointer Candidates | Keep as Raw | Qt Managed | Investigated |
|----------|-------|-------------------------|-------------|------------|--------------|
| Member variables | 26 | 0 ✅ | 23 | 3 | Yes ✅ |
| System interfaces | 15+ | 0 | 15+ | 0 | N/A |
| Component pointers | 5 | 0 | 5 | 0 | N/A |
| **Total** | **46+** | **0** | **43+** | **3** | **Complete** |

**Existing Smart Pointer Usage:** 244 instances

**✅ Investigation Complete:** All unclear ownership cases have been investigated and resolved.

**Conclusion:** The codebase already uses smart pointers appropriately for ownership. All remaining raw pointers are intentional non-owning observers, caches, or internal state pointers, which is the correct pattern for this ECS architecture and renderer design. **No migrations needed.**

### Key Findings

1. **Shader Pointers:** Non-owning cache pointers. Owned by `ShaderCache` with `std::unique_ptr`.
2. **DrawQueue Pointer:** Internal state pointer to owned array element (double-buffering pattern).
3. **World Pointers:** Non-owning observers in ECS architecture. World outlives all systems.
4. **Component Pointers:** Cached non-owning pointers in ECS pattern. Components owned by World.
5. **Service Dependencies:** Non-owning references injected via constructors. Clear lifetime contracts.
6. **Qt Objects:** Managed by Qt parent-child ownership model.
7. **OpenGL Pointers:** Required by OpenGL API (`void*` for mapped buffers).

All raw pointer usage follows C++ Core Guidelines and established patterns.

---

## References

- [C++ Core Guidelines: Resource Management](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-resource)
- [C++ Core Guidelines: F.7 - Don't pass smart pointers unless you intend to transfer ownership](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rf-smart)
- [C++ Core Guidelines: R.3 - A raw pointer (a T*) is non-owning](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-ptr)
