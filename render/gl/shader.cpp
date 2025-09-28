#include "shader.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

namespace Render::GL {

Shader::Shader() {
    initializeOpenGLFunctions();
}

Shader::~Shader() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
    }
}

bool Shader::loadFromFiles(const QString& vertexPath, const QString& fragmentPath) {
    QFile vertexFile(vertexPath);
    QFile fragmentFile(fragmentPath);
    
    if (!vertexFile.open(QIODevice::ReadOnly) || !fragmentFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open shader files";
        return false;
    }
    
    QTextStream vertexStream(&vertexFile);
    QTextStream fragmentStream(&fragmentFile);
    
    QString vertexSource = vertexStream.readAll();
    QString fragmentSource = fragmentStream.readAll();
    
    return loadFromSource(vertexSource, fragmentSource);
}

bool Shader::loadFromSource(const QString& vertexSource, const QString& fragmentSource) {
    GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);
    
    if (vertexShader == 0 || fragmentShader == 0) {
        return false;
    }
    
    bool success = linkProgram(vertexShader, fragmentShader);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    
    return success;
}

void Shader::use() {
    glUseProgram(m_program);
}

void Shader::release() {
    glUseProgram(0);
}

void Shader::setUniform(const QString& name, float value) {
    GLint location = glGetUniformLocation(m_program, name.toUtf8().constData());
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void Shader::setUniform(const QString& name, const QVector3D& value) {
    GLint location = glGetUniformLocation(m_program, name.toUtf8().constData());
    if (location != -1) {
        glUniform3f(location, value.x(), value.y(), value.z());
    }
}

void Shader::setUniform(const QString& name, const QMatrix4x4& value) {
    GLint location = glGetUniformLocation(m_program, name.toUtf8().constData());
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, value.constData());
    }
}

void Shader::setUniform(const QString& name, int value) {
    GLint location = glGetUniformLocation(m_program, name.toUtf8().constData());
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void Shader::setUniform(const QString& name, bool value) {
    setUniform(name, static_cast<int>(value));
}

GLuint Shader::compileShader(const QString& source, GLenum type) {
    GLuint shader = glCreateShader(type);
    
    QByteArray sourceBytes = source.toUtf8();
    const char* sourcePtr = sourceBytes.constData();
    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        qWarning() << "Shader compilation failed:" << infoLog;
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

bool Shader::linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    glLinkProgram(m_program);
    
    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(m_program, 512, nullptr, infoLog);
        qWarning() << "Shader linking failed:" << infoLog;
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }
    
    return true;
}

} // namespace Render::GL