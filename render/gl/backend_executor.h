#pragma once

#include "../draw_queue.h"
#include "camera.h"
#include <qmatrix4x4.h>

namespace Render::GL {

class Backend;

namespace BackendExecutor {

void execute_cylinder_batch(Backend *backend, const DrawQueue &queue,
                            std::size_t &i, const QMatrix4x4 &view_proj);

void execute_fog_batch(Backend *backend, const DrawQueue &queue, std::size_t &i,
                      const QMatrix4x4 &view_proj);

void execute_grass_batch(Backend *backend, const DrawQueue &queue,
                        std::size_t &i, const QMatrix4x4 &view_proj);

void execute_stone_batch(Backend *backend, const DrawQueue &queue,
                        std::size_t &i, const QMatrix4x4 &view_proj);

void execute_plant_batch(Backend *backend, const DrawQueue &queue,
                        std::size_t &i, const QMatrix4x4 &view_proj);

void execute_pine_batch(Backend *backend, const DrawQueue &queue, std::size_t &i,
                       const QMatrix4x4 &view_proj);

void execute_olive_batch(Backend *backend, const DrawQueue &queue,
                        std::size_t &i, const QMatrix4x4 &view_proj);

void execute_firecamp_batch(Backend *backend, const DrawQueue &queue,
                           std::size_t &i, const Camera &cam,
                           const QMatrix4x4 &view_proj);

void execute_rain_batch(Backend *backend, const DrawQueue &queue, std::size_t &i,
                       const Camera &cam);

void execute_terrain_chunk(Backend *backend, const DrawQueue &queue,
                          std::size_t &i, const QMatrix4x4 &view_proj);

void execute_mesh_cmd(Backend *backend, const DrawQueue &queue, std::size_t &i,
                     const Camera &cam, const QMatrix4x4 &view_proj);

void execute_grid_cmd(Backend *backend, const DrawQueue &queue, std::size_t &i);

void execute_selection_ring(Backend *backend, const DrawQueue &queue,
                           std::size_t &i, const QMatrix4x4 &view_proj);

void execute_selection_smoke(Backend *backend, const DrawQueue &queue,
                            std::size_t &i, const QMatrix4x4 &view_proj);

void execute_primitive_batch(Backend *backend, const DrawQueue &queue,
                            std::size_t &i, const QMatrix4x4 &view_proj);

void execute_healing_beam(Backend *backend, const DrawQueue &queue,
                         std::size_t &i, const QMatrix4x4 &view_proj);

void execute_healer_aura(Backend *backend, const DrawQueue &queue,
                        std::size_t &i, const QMatrix4x4 &view_proj);

void execute_combat_dust(Backend *backend, const DrawQueue &queue,
                        std::size_t &i, const QMatrix4x4 &view_proj);

void execute_building_flame(Backend *backend, const DrawQueue &queue,
                           std::size_t &i, const QMatrix4x4 &view_proj);

void execute_stone_impact(Backend *backend, const DrawQueue &queue,
                         std::size_t &i, const QMatrix4x4 &view_proj);

void execute_mode_indicator(Backend *backend, const DrawQueue &queue,
                           std::size_t &i, const QMatrix4x4 &view_proj);

} // namespace BackendExecutor
} // namespace Render::GL
