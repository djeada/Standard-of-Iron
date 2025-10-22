#pragma once

#include "mesh.h"
#include <memory>

namespace Render::GL {

Mesh *getUnitCylinder(int radialSegments = 32);

Mesh *getUnitSphere(int latSegments = 16, int lonSegments = 32);

Mesh *getUnitCone(int radialSegments = 32);

Mesh *getUnitCapsule(int radialSegments = 32, int heightSegments = 1);

Mesh *getUnitTorso(int radialSegments = 32, int heightSegments = 8);

} 
