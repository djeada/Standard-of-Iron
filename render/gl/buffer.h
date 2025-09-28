#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <vector>

namespace Render::GL {

class Buffer : protected QOpenGLFunctions_3_3_Core {
public:
    enum class Type {
        Vertex,
        Index,
        Uniform
    };
    
    enum class Usage {
        Static,
        Dynamic,
        Stream
    };

    Buffer(Type type);
    ~Buffer();

    void bind();
    void unbind();
    
    void setData(const void* data, size_t size, Usage usage = Usage::Static);
    
    template<typename T>
    void setData(const std::vector<T>& data, Usage usage = Usage::Static) {
        setData(data.data(), data.size() * sizeof(T), usage);
    }

private:
    GLuint m_buffer = 0;
    Type m_type;
    GLenum getGLType() const;
    GLenum getGLUsage(Usage usage) const;
};

class VertexArray : protected QOpenGLFunctions_3_3_Core {
public:
    VertexArray();
    ~VertexArray();

    void bind();
    void unbind();
    
    void addVertexBuffer(Buffer& buffer, const std::vector<int>& layout);
    void setIndexBuffer(Buffer& buffer);

private:
    GLuint m_vao = 0;
    int m_currentAttribIndex = 0;
};

} // namespace Render::GL