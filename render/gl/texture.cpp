#include "texture.h"
#include <QImage>
#include <QDebug>

namespace Render::GL {

Texture::Texture() {
    // Defer creation until first bind when a context is current
}

Texture::~Texture() {
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
    }
}

bool Texture::loadFromFile(const QString& path) {
    initializeOpenGLFunctions();
    QImage image;
    if (!image.load(path)) {
        qWarning() << "Failed to load texture:" << path;
        return false;
    }
    
    // Convert to RGBA format and flip vertically for OpenGL
    image = image.convertToFormat(QImage::Format_RGBA8888).mirrored();
    
    m_width = image.width();
    m_height = image.height();
    m_format = Format::RGBA;
    
    bind();
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, 
                 GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());
    
    // Set default parameters
    setFilter(Filter::Linear, Filter::Linear);
    setWrap(Wrap::Repeat, Wrap::Repeat);
    
    glGenerateMipmap(GL_TEXTURE_2D);
    
    unbind();
    
    return true;
}

bool Texture::createEmpty(int width, int height, Format format) {
    initializeOpenGLFunctions();
    m_width = width;
    m_height = height;
    m_format = format;
    
    bind();
    
    GLenum glFormat = getGLFormat(format);
    GLenum internalFormat = glFormat;
    GLenum type = GL_UNSIGNED_BYTE;
    
    if (format == Format::Depth) {
        internalFormat = GL_DEPTH_COMPONENT;
        type = GL_FLOAT;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, 
                 glFormat, type, nullptr);
    
    setFilter(Filter::Linear, Filter::Linear);
    setWrap(Wrap::ClampToEdge, Wrap::ClampToEdge);
    
    unbind();
    
    return true;
}

void Texture::bind(int unit) {
    initializeOpenGLFunctions();
    if (!m_texture) {
        glGenTextures(1, &m_texture);
    }
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_texture);
}

void Texture::unbind() {
    initializeOpenGLFunctions();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::setFilter(Filter minFilter, Filter magFilter) {
    bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, getGLFilter(minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, getGLFilter(magFilter));
}

void Texture::setWrap(Wrap sWrap, Wrap tWrap) {
    bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, getGLWrap(sWrap));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, getGLWrap(tWrap));
}

GLenum Texture::getGLFormat(Format format) const {
    switch (format) {
        case Format::RGB: return GL_RGB;
        case Format::RGBA: return GL_RGBA;
        case Format::Depth: return GL_DEPTH_COMPONENT;
    }
    return GL_RGBA;
}

GLenum Texture::getGLFilter(Filter filter) const {
    switch (filter) {
        case Filter::Nearest: return GL_NEAREST;
        case Filter::Linear: return GL_LINEAR;
    }
    return GL_LINEAR;
}

GLenum Texture::getGLWrap(Wrap wrap) const {
    switch (wrap) {
        case Wrap::Repeat: return GL_REPEAT;
        case Wrap::ClampToEdge: return GL_CLAMP_TO_EDGE;
        case Wrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
    }
    return GL_REPEAT;
}

} // namespace Render::GL