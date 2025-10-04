#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace Render { namespace Geom {

/**
 * Utility for rendering flag markers at world positions.
 * Used for rally points, patrol waypoints, and other location markers.
 */
class Flag {
public:
    /**
     * Render a flag at the specified world position.
     * 
     * @param worldX X coordinate in world space
     * @param worldY Y coordinate in world space (height)
     * @param worldZ Z coordinate in world space
     * @param color Color of the flag pennant
     * @param poleColor Color of the pole (defaults to dark wood)
     * @param scale Overall scale multiplier (default 1.0)
     * @return Array of matrices: [pole, pennant, finial]
     */
    struct FlagMatrices {
        QMatrix4x4 pole;
        QMatrix4x4 pennant;
        QMatrix4x4 finial;
        QVector3D pennantColor;
        QVector3D poleColor;
    };
    
    static FlagMatrices create(
        float worldX, 
        float worldZ, 
        const QVector3D& flagColor = QVector3D(1.0f, 0.9f, 0.2f),
        const QVector3D& poleColor = QVector3D(0.3f, 0.2f, 0.1f),
        float scale = 1.0f
    );
};

} } // namespace Render::Geom
