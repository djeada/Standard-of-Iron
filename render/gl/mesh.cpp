#include "mesh.h"
#include <QOpenGLFunctions_3_3_Core>

namespace Render::GL {

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    : m_vertices(vertices), m_indices(indices) {
    initializeOpenGLFunctions();
    setupBuffers();
}

Mesh::~Mesh() = default;

void Mesh::setupBuffers() {
    m_vao = std::make_unique<VertexArray>();
    m_vbo = std::make_unique<Buffer>(Buffer::Type::Vertex);
    m_ebo = std::make_unique<Buffer>(Buffer::Type::Index);
    
    m_vao->bind();
    
    // Set vertex data
    m_vbo->setData(m_vertices);
    
    // Set index data
    m_ebo->setData(m_indices);
    
    // Define vertex layout: position (3), normal (3), texCoord (2)
    std::vector<int> layout = {3, 3, 2};
    m_vao->addVertexBuffer(*m_vbo, layout);
    m_vao->setIndexBuffer(*m_ebo);
    
    m_vao->unbind();
}

void Mesh::draw() {
    m_vao->bind();
    
    // Draw elements
    glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, nullptr);
    
    m_vao->unbind();
}

Mesh* createQuadMesh() {
    std::vector<Vertex> vertices = {
        // Position           Normal            TexCoord
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}
    };
    
    std::vector<unsigned int> indices = {
        0, 1, 2,
        2, 3, 0
    };
    
    return new Mesh(vertices, indices);
}

Mesh* createCubeMesh() {
    std::vector<Vertex> vertices = {
        // Front face
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        
        // Back face
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
    };
    
    std::vector<unsigned int> indices = {
        // Front face
        0, 1, 2, 2, 3, 0,
        // Back face
        4, 5, 6, 6, 7, 4,
        // Left face
        4, 0, 3, 3, 5, 4,
        // Right face
        1, 7, 6, 6, 2, 1,
        // Top face
        3, 2, 6, 6, 5, 3,
        // Bottom face
        4, 7, 1, 1, 0, 4
    };
    
    return new Mesh(vertices, indices);
}

Mesh* createPlaneMesh(float width, float height, int subdivisions) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    
    // Generate vertices
    for (int z = 0; z <= subdivisions; ++z) {
        for (int x = 0; x <= subdivisions; ++x) {
            float xPos = (x / static_cast<float>(subdivisions)) * width - halfWidth;
            float zPos = (z / static_cast<float>(subdivisions)) * height - halfHeight;
            
            vertices.push_back({
                {xPos, 0.0f, zPos},
                {0.0f, 1.0f, 0.0f},
                {x / static_cast<float>(subdivisions), z / static_cast<float>(subdivisions)}
            });
        }
    }
    
    // Generate indices
    for (int z = 0; z < subdivisions; ++z) {
        for (int x = 0; x < subdivisions; ++x) {
            int topLeft = z * (subdivisions + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * (subdivisions + 1) + x;
            int bottomRight = bottomLeft + 1;
            
            // First triangle
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            // Second triangle
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    
    return new Mesh(vertices, indices);
}

} // namespace Render::GL