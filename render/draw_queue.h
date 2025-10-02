#pragma once
#include <vector>
#include <QMatrix4x4>
#include <QVector3D>
namespace Render::GL { class Mesh; class Texture; }
namespace Render::GL {
struct DrawItem { Mesh* mesh{}; Texture* texture{}; QMatrix4x4 model; QVector3D color{1,1,1}; };
class DrawQueue {
public:
    void clear() { m_items.clear(); }
    void submit(const DrawItem& it) { m_items.push_back(it); }
    const std::vector<DrawItem>& items() const { return m_items; }
private:
    std::vector<DrawItem> m_items;
};
}
#pragma once

#include <vector>
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL { class Mesh; class Texture; }

namespace Render {

struct DrawItem {
    Render::GL::Mesh* mesh = nullptr;
    Render::GL::Texture* texture = nullptr;
    QMatrix4x4 model;
    QVector3D color{1,1,1};
};

class DrawQueue {
public:
    void clear() { items.clear(); }
    void add(const DrawItem& it) { items.push_back(it); }
    bool empty() const { return items.empty(); }
    const std::vector<DrawItem>& data() const { return items; }
    std::vector<DrawItem>& data() { return items; }
private:
    std::vector<DrawItem> items;
};

} // namespace Render
