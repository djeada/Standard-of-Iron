#pragma once

#include "../mesh.h"
#include "../shader.h"
#include <QOpenGLFunctions_3_3_Core>
#include <cstddef>

namespace Render::GL {

class Backend;
class Shader;
struct Vertex;

namespace BackendImpl {

void initialize_cylinder_pipeline(Backend *backend);

void shutdown_cylinder_pipeline(Backend *backend);

void cache_cylinder_uniforms(Backend *backend);

void upload_cylinder_instances(Backend *backend, std::size_t count);

void draw_cylinders(Backend *backend, std::size_t count);

void initialize_fog_pipeline(Backend *backend);

void shutdown_fog_pipeline(Backend *backend);

void cache_fog_uniforms(Backend *backend);

void upload_fog_instances(Backend *backend, std::size_t count);

void draw_fog(Backend *backend, std::size_t count);

} 
} 
