#pragma once

namespace Render::GL::VertexAttrib {
inline constexpr int Position = 0;
inline constexpr int Normal = 1;
inline constexpr int TexCoord = 2;
inline constexpr int InstancePosition = 3;
inline constexpr int InstanceScale = 4;
inline constexpr int InstanceColor = 5;
inline constexpr int InstanceAlpha = 6;
inline constexpr int InstanceTint = 7;
} // namespace Render::GL::VertexAttrib

namespace Render::GL::ComponentCount {
inline constexpr int Vec2 = 2;
inline constexpr int Vec3 = 3;
inline constexpr int Vec4 = 4;
} // namespace Render::GL::ComponentCount

namespace Render::GL::BufferCapacity {
inline constexpr int DefaultCylinderInstances = 256;
inline constexpr int DefaultFogInstances = 512;
inline constexpr int BuffersInFlight = 3;
inline constexpr int ShaderInfoLogSize = 512;
} // namespace Render::GL::BufferCapacity

namespace Render::GL::Geometry {
inline constexpr int QuadVertexCount = 4;
inline constexpr int QuadIndexCount = 6;
inline constexpr int GrassBladeVertexCount = 6;
inline constexpr int CubeVertexCount = 24;
inline constexpr int CubeIndexCount = 36;
inline constexpr int PlantCrossQuadVertexCount = 16;
inline constexpr int PlantCrossQuadIndexCount = 24;
inline constexpr int PineTreeSegments = 6;
inline constexpr int GroundPlaneSubdivisions = 64;
inline constexpr int DefaultCapsuleSegments = 8;
inline constexpr int DefaultTorsoHeightSegments = 8;
inline constexpr int DefaultChunkSize = 16;
inline constexpr int MinLengthSteps = 8;
inline constexpr int MinLengthSegments = 8;
} // namespace Render::GL::Geometry

namespace Render::GL::Growth {
inline constexpr int CapacityMultiplier = 2;
}

namespace Render::GL::ColorIndex {
inline constexpr int Red = 0;
inline constexpr int Green = 1;
inline constexpr int Blue = 2;
inline constexpr int Alpha = 3;
} // namespace Render::GL::ColorIndex

namespace Render::GL::FrustumPlane {
inline constexpr int MatrixSize = 16;
inline constexpr int Index0 = 0;
inline constexpr int Index1 = 1;
inline constexpr int Index2 = 2;
inline constexpr int Index3 = 3;
inline constexpr int Index4 = 4;
inline constexpr int Index5 = 5;
inline constexpr int Index6 = 6;
inline constexpr int Index7 = 7;
inline constexpr int Index8 = 8;
inline constexpr int Index9 = 9;
inline constexpr int Index10 = 10;
inline constexpr int Index11 = 11;
} // namespace Render::GL::FrustumPlane

namespace Render::GL::RGBA {
inline constexpr unsigned char MaxValue = 255;
inline constexpr unsigned char MinValue = 0;
} // namespace Render::GL::RGBA

namespace Render::GL::BitShift {
inline constexpr int Shift8 = 8;
inline constexpr int Shift16 = 16;
inline constexpr int Shift32 = 32;
inline constexpr unsigned int Mask24Bit = 0xFFFFFF;
inline constexpr float Mask24BitFloat = 16777215.0F;
inline constexpr unsigned int Mask24BitHex = 0x01000000U;
} // namespace Render::GL::BitShift

namespace Render::GL::HashXorShift {
inline constexpr int Shift5 = 5;
inline constexpr int Shift13 = 13;
inline constexpr int Shift17 = 17;
inline constexpr uint32_t GoldenRatioHash = 0x9E3779B9U;
} // namespace Render::GL::HashXorShift
