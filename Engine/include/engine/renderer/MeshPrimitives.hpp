#pragma once

#include "engine/core/Base.hpp"
#include "engine/renderer/VertexArray.hpp"

namespace Leon {

    /**
     * @brief Factory for generating standard 3D/2D procedural geometric mesh primitives.
     */
    class FMeshPrimitives {
    public:
        /**
         * @brief Creates an indexed 3D cube vertex array (24 vertices, 36 indices).
         * @param InSize Length of each side of the cube (centered at origin).
         */
        static TRef<FVertexArray> CreateCube(float InSize = 1.0f);

        /**
         * @brief Creates an indexed 2D quad / rectangle on the XY plane (4 vertices, 6 indices).
         * @param InWidth Width along the X axis.
         * @param InHeight Height along the Y axis.
         */
        static TRef<FVertexArray> CreateQuad(float InWidth = 1.0f, float InHeight = 1.0f);

        /**
         * @brief Creates an indexed 3D UV Sphere.
         * @param InRadius Radius of the sphere.
         * @param InSegments Number of longitudinal subdivisions (longitude).
         * @param InRings Number of latitudinal subdivisions (latitude).
         */
        static TRef<FVertexArray> CreateSphere(float InRadius = 0.5f, unsigned int InSegments = 32,
                                               unsigned int InRings = 16);

        /**
         * @brief Creates an indexed 3D ground plane grid on the XZ plane.
         * @param InWidth Width along X axis.
         * @param InDepth Depth along Z axis.
         * @param InSubdivisionsX Number of grid segments on X.
         * @param InSubdivisionsZ Number of grid segments on Z.
         */
        static TRef<FVertexArray> CreatePlane(float InWidth = 10.0f, float InDepth = 10.0f,
                                              unsigned int InSubdivisionsX = 10, unsigned int InSubdivisionsZ = 10);

        /**
         * @brief Creates an indexed 3D cylinder or cone with end caps.
         * @param InBottomRadius Radius at the bottom cap.
         * @param InTopRadius Radius at the top cap (0 for cone).
         * @param InHeight Total height along the Y axis.
         * @param InSegments Number of radial subdivisions around the cylinder.
         * @param InbCaps Whether to generate top and bottom caps.
         */
        static TRef<FVertexArray> CreateCylinder(float InBottomRadius = 0.5f, float InTopRadius = 0.5f,
                                                 float InHeight = 1.0f, unsigned int InSegments = 32,
                                                 bool InbCaps = true);
    };

} // namespace Leon
