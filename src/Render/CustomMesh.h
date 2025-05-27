#pragma once

#include "raymath.h"

#include <array>
#include <numbers>

namespace CustomMesh {
namespace {
typedef std::pair<uint16_t, uint16_t> Edge;

uint16_t getOrRegisterEdgeNewVertexIndex(uint16_t edgeA,
                                         uint16_t edgeB,
                                         std::array<Edge, 30>& edges,
                                         std::array<Vector3, 42>& vertices,
                                         size_t& registeredEdges)
{
    auto end = edges.begin() + registeredEdges;
    auto it = std::find_if(edges.begin(), end, [=](const Edge& edge) {
        return (edgeA == edge.first && edgeB == edge.second) ||
               (edgeA == edge.second && edgeB == edge.first);
    });
    if (it == end) {
        *it = {edgeA, edgeB};
        Vector3 vertexA = vertices[edgeA];
        Vector3 vertexB = vertices[edgeB];
        Vector3 newVertex = Vector3Normalize(vertexA + vertexB);
        vertices[12 + registeredEdges] = {newVertex};
        ++registeredEdges;
    }
    return (short)(12 + std ::distance(edges.begin(), it));
}
} // namespace

void IcosahedronMesh(std::array<Vector3, 12>& vertices, std::array<uint16_t, 20 * 3>& triangles)
{
    constexpr float zero = 0.0;
    constexpr float one = 1.0;
    constexpr float phi = std::numbers::phi_v<float>;

    vertices[0] = Vector3{-one, -phi, zero};
    vertices[1] = Vector3{one, -phi, zero};
    vertices[2] = Vector3{one, phi, zero};
    vertices[3] = Vector3{-one, phi, zero};

    vertices[4] = Vector3{zero, -one, -phi};
    vertices[5] = Vector3{zero, one, -phi};
    vertices[6] = Vector3{zero, one, phi};
    vertices[7] = Vector3{zero, -one, phi};

    vertices[8] = Vector3{-phi, zero, -one};
    vertices[9] = Vector3{phi, zero, -one};
    vertices[10] = Vector3{phi, zero, one};
    vertices[11] = Vector3{-phi, zero, one};

    const float normalizer = sqrt(one / (one + (phi * phi)));
    for (auto& vertex : vertices) {
        vertex *= normalizer;
    }

    // Triangles with vertex 0
    triangles[0 * 3 + 0] = 0;
    triangles[0 * 3 + 1] = 1;
    triangles[0 * 3 + 2] = 4;

    triangles[1 * 3 + 1] = 0;
    triangles[1 * 3 + 2] = 7;
    triangles[1 * 3 + 0] = 1;

    triangles[2 * 3 + 0] = 0;
    triangles[2 * 3 + 1] = 11;
    triangles[2 * 3 + 2] = 7;

    triangles[3 * 3 + 0] = 0;
    triangles[3 * 3 + 1] = 8;
    triangles[3 * 3 + 2] = 11;

    triangles[4 * 3 + 0] = 0;
    triangles[4 * 3 + 1] = 4;
    triangles[4 * 3 + 2] = 8;

    // Triangles with vertex 1
    triangles[5 * 3 + 0] = 1;
    triangles[5 * 3 + 1] = 7;
    triangles[5 * 3 + 2] = 10;

    triangles[6 * 3 + 0] = 1;
    triangles[6 * 3 + 1] = 9;
    triangles[6 * 3 + 2] = 4;

    triangles[7 * 3 + 0] = 1;
    triangles[7 * 3 + 1] = 10;
    triangles[7 * 3 + 2] = 9;

    // triangles with vertex 2
    triangles[8 * 3 + 0] = 2;
    triangles[8 * 3 + 1] = 6;
    triangles[8 * 3 + 2] = 3;

    triangles[9 * 3 + 1] = 2;
    triangles[9 * 3 + 2] = 10;
    triangles[9 * 3 + 0] = 6;

    triangles[10 * 3 + 0] = 2;
    triangles[10 * 3 + 1] = 9;
    triangles[10 * 3 + 2] = 10;

    triangles[11 * 3 + 0] = 2;
    triangles[11 * 3 + 1] = 5;
    triangles[11 * 3 + 2] = 9;

    triangles[12 * 3 + 0] = 2;
    triangles[12 * 3 + 1] = 3;
    triangles[12 * 3 + 2] = 5;

    // Triangles with vertex 3
    triangles[13 * 3 + 0] = 3;
    triangles[13 * 3 + 1] = 6;
    triangles[13 * 3 + 2] = 11;

    triangles[14 * 3 + 0] = 3;
    triangles[14 * 3 + 1] = 11;
    triangles[14 * 3 + 2] = 8;

    triangles[15 * 3 + 0] = 3;
    triangles[15 * 3 + 1] = 8;
    triangles[15 * 3 + 2] = 5;

    // Triangles with vertex 4
    triangles[16 * 3 + 0] = 4;
    triangles[16 * 3 + 1] = 5;
    triangles[16 * 3 + 2] = 8;

    triangles[17 * 3 + 0] = 4;
    triangles[17 * 3 + 1] = 9;
    triangles[17 * 3 + 2] = 5;

    // Triangles with vertex 5 - All taken

    // Triangles with vertex 6
    triangles[18 * 3 + 0] = 6;
    triangles[18 * 3 + 1] = 10;
    triangles[18 * 3 + 2] = 7;

    triangles[19 * 3 + 0] = 6;
    triangles[19 * 3 + 1] = 7;
    triangles[19 * 3 + 2] = 11;
};

void SphereMesh(std::array<Vector3, 42>& vertices, std::array<uint16_t, 20 * 4 * 3>& triangles)
{
    std::array<Vector3, 12> icosVertices;
    std::array<uint16_t, 20 * 3> icosTriangles;
    IcosahedronMesh(icosVertices, icosTriangles);

    std::memcpy(&vertices, &icosVertices, sizeof(icosVertices));

    std::array<Edge, 30> edges;
    for (Edge& edge : edges) {
        edge = {0, 0};
    }

    size_t registeredEdges = 0;

    for (size_t tri = 0; tri < 20; ++tri) {
        size_t baseIndex = tri * 3;
        uint16_t vertIndex1 = icosTriangles[baseIndex + 0];
        uint16_t vertIndex2 = icosTriangles[baseIndex + 1];
        uint16_t vertIndex3 = icosTriangles[baseIndex + 2];

        uint16_t newVertexIndex1 =
        getOrRegisterEdgeNewVertexIndex(vertIndex1, vertIndex2, edges, vertices, registeredEdges);
        uint16_t newVertexIndex2 =
        getOrRegisterEdgeNewVertexIndex(vertIndex2, vertIndex3, edges, vertices, registeredEdges);
        uint16_t newVertexIndex3 =
        getOrRegisterEdgeNewVertexIndex(vertIndex3, vertIndex1, edges, vertices, registeredEdges);

        size_t triIndex = baseIndex * 4;
        triangles[triIndex + 0] = vertIndex1;
        triangles[triIndex + 1] = newVertexIndex3;
        triangles[triIndex + 2] = newVertexIndex1;

        triangles[triIndex + 3] = vertIndex2;
        triangles[triIndex + 4] = newVertexIndex1;
        triangles[triIndex + 5] = newVertexIndex2;

        triangles[triIndex + 6] = vertIndex3;
        triangles[triIndex + 7] = newVertexIndex2;
        triangles[triIndex + 8] = newVertexIndex3;

        triangles[triIndex + 9] = newVertexIndex1;
        triangles[triIndex + 10] = newVertexIndex3;
        triangles[triIndex + 11] = newVertexIndex2;
    }
};
} // namespace CustomMesh