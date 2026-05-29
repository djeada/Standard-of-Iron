#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include "../../decoration_gpu.h"
#include "../../draw_queue.h"
#include "../../geom/mode_indicator.h"
#include "../../geom/selection_disc.h"
#include "../../geom/selection_ring.h"
#include "../../graphics_settings.h"
#include "../../material.h"
#include "../../primitive_batch.h"
#include "../../rain_gpu.h"
#include "../backend.h"
#include "../buffer.h"
#include "../mesh.h"
#include "../render_constants.h"
#include "../resources.h"
#include "../shader.h"
#include "../state_scopes.h"
#include "../texture.h"
#include "../uniform_helpers.h"
#include "../vertex_attrib_layout.h"
#include "banner_pipeline.h"
#include "character_pipeline.h"
#include "combat_dust_pipeline.h"
#include "cylinder_pipeline.h"
#include "effects_pipeline.h"
#include "healer_aura_pipeline.h"
#include "healing_beam_pipeline.h"
#include "mesh_instancing_pipeline.h"
#include "mode_indicator_pipeline.h"
#include "primitive_batch_pipeline.h"
#include "rain_pipeline.h"
#include "rigged_character_pipeline.h"
#include "terrain_pipeline.h"
#include "vegetation_pipeline.h"
#include "water_pipeline.h"

// TODO(phase1): Move direct shader uniform/state setup behind pipeline draw methods
// once each command path has a mechanically verifiable contract.
