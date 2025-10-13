

#include "persistent_buffer.h"

PersistentRingBuffer<CylinderInstanceGpu> m_cylinderPersistentBuffer;

void Backend::initializeCylinderPipeline() {

  const std::size_t initialCapacity = 10000;
  if (m_cylinderPersistentBuffer.initialize(initialCapacity, 3)) {
  } else {
    qWarning()
        << "Failed to init persistent buffer, falling back to old method";
  }
}

void Backend::beginFrame() {

  if (m_cylinderPersistentBuffer.isValid()) {
    m_cylinderPersistentBuffer.beginFrame();
  }
}

void Backend::uploadCylinderInstances(std::size_t count) {
  if (count == 0)
    return;

  if (m_cylinderPersistentBuffer.isValid()) {
    if (count > m_cylinderPersistentBuffer.capacity()) {
      qWarning() << "Too many cylinders:" << count
                 << "max:" << m_cylinderPersistentBuffer.capacity();
      count = m_cylinderPersistentBuffer.capacity();
    }

    m_cylinderPersistentBuffer.write(m_cylinderScratch.data(), count);

    glBindBuffer(GL_ARRAY_BUFFER, m_cylinderPersistentBuffer.buffer());

    return;
  }

  if (!m_cylinderInstanceBuffer)
    return;

  glBindBuffer(GL_ARRAY_BUFFER, m_cylinderInstanceBuffer);
  if (count > m_cylinderInstanceCapacity) {
    m_cylinderInstanceCapacity = std::max<std::size_t>(
        count,
        m_cylinderInstanceCapacity ? m_cylinderInstanceCapacity * 2 : count);
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinderInstanceCapacity * sizeof(CylinderInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
    m_cylinderScratch.reserve(m_cylinderInstanceCapacity);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(CylinderInstanceGpu),
                  m_cylinderScratch.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Backend::drawCylinders(std::size_t count) {
  if (!m_cylinderVao || m_cylinderIndexCount == 0 || count == 0)
    return;

  initializeOpenGLFunctions();
  glBindVertexArray(m_cylinderVao);

  glDrawElementsInstanced(GL_TRIANGLES, m_cylinderIndexCount, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));

  glBindVertexArray(0);
}

void Backend::shutdownCylinderPipeline() {

  m_cylinderPersistentBuffer.destroy();
}
