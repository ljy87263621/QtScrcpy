#include "keymapdocument.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QSaveFile>
#include <QFile>
#include <cmath>
#include <limits>

namespace {
bool fail(QString *error, const QString &text)
{
    if (error) *error = text;
    return false;
}
bool validKey(const QJsonValue &value)
{
    if (!value.isString()) return false;
    const auto name = value.toString().toLatin1();
    if (name == "Key_unknown" || name == "NoButton") return false;
    return QMetaEnum::fromType<Qt::Key>().keyToValue(name.constData()) >= 0
        || QMetaEnum::fromType<Qt::MouseButtons>().keyToValue(name.constData()) > 0;
}
bool number(const QJsonValue &value, double low, double high)
{
    return value.isDouble() && std::isfinite(value.toDouble())
        && value.toDouble() >= low && value.toDouble() <= high;
}
bool position(const QJsonValue &value)
{
    const auto object = value.toObject();
    return value.isObject() && number(object["x"], 0, 1) && number(object["y"], 0, 1);
}
QJsonObject point(double x, double y) { return {{"x", x}, {"y", y}}; }
QPointF toPoint(const QJsonValue &value)
{
    const auto object = value.toObject();
    return {object["x"].toDouble(), object["y"].toDouble()};
}
QJsonValue put(QJsonValue root, QStringList path, const QJsonValue &value)
{
    if (path.isEmpty()) return value;
    const auto field = path.takeFirst();
    if (root.isArray()) {
        auto array = root.toArray();
        bool ok = false;
        const int index = field.toInt(&ok);
        if (ok && index >= 0 && index < array.size()) array[index] = put(array[index], path, value);
        return array;
    }
    auto object = root.toObject();
    object[field] = put(object[field], path, value);
    return object;
}
}

class KeymapChange : public QUndoCommand
{
public:
    KeymapChange(KeymapDocument *doc, const QJsonObject &after, const QString &label)
        : QUndoCommand(label), m_doc(doc), m_before(doc->m_root), m_after(after) {}
    void undo() override { m_doc->m_root = m_before; emit m_doc->changed(); }
    void redo() override { m_doc->m_root = m_after; emit m_doc->changed(); }
private:
    KeymapDocument *m_doc;
    QJsonObject m_before, m_after;
};

KeymapDocument::KeymapDocument(QObject *parent) : QObject(parent)
{
    m_root = {{"switchKey", "Key_QuoteLeft"}, {"keyMapNodes", QJsonArray()}};
    connect(&m_undo, &QUndoStack::cleanChanged, this, &KeymapDocument::changed);
}
QByteArray KeymapDocument::json() const { return QJsonDocument(m_root).toJson(QJsonDocument::Indented); }
QStringList KeymapDocument::types()
{
    return {"KMT_CLICK", "KMT_CLICK_TWICE", "KMT_CLICK_MULTI", "KMT_STEER_WHEEL",
            "KMT_DRAG", "KMT_ANDROID_KEY", "KMT_MOUSE_MOVE"};
}
QJsonValue KeymapDocument::valueAt(QJsonValue root, const QStringList &path)
{
    for (const auto &field : path) {
        if (root.isArray()) {
            bool ok = false;
            const int index = field.toInt(&ok);
            if (!ok || index < 0 || index >= root.toArray().size()) return QJsonValue();
            root = root.toArray().at(index);
        } else {
            root = root.toObject().value(field);
        }
    }
    return root;
}

bool KeymapDocument::validate(const QJsonObject &root, QString *error)
{
    if (error) error->clear();
    if (!validKey(root["switchKey"])) return fail(error, "switchKey: invalid Qt key or mouse button");
    if (root.contains("keyMapNodes") && !root["keyMapNodes"].isArray())
        return fail(error, "keyMapNodes: expected array");
    if (root.contains("mouseMoveMap")) {
        if (!root["mouseMoveMap"].isObject()) return fail(error, "mouseMoveMap: expected object");
        const auto mouse = root["mouseMoveMap"].toObject();
        if (!position(mouse["startPos"])) return fail(error, "mouseMoveMap.startPos: coordinates must be in [0, 1]");
        bool hasRatio = false;
        for (const auto &field : {"speedRatio", "speedRatioX", "speedRatioY"}) {
            if (mouse.contains(field)) {
                hasRatio = true;
                if (!number(mouse[field], 0.001, std::numeric_limits<double>::max()))
                    return fail(error, QString("mouseMoveMap.%1: expected number >= 0.001").arg(field));
            }
        }
        if (!hasRatio) return fail(error, "mouseMoveMap: sensitivity is required");
        if (mouse.contains("smallEyes")) {
            const auto eyes = mouse["smallEyes"].toObject();
            if (eyes["type"] != "KMT_CLICK" || !validKey(eyes["key"]) || !position(eyes["pos"]) || (eyes.contains("switchMap") && !eyes["switchMap"].isBool()))
                return fail(error, "mouseMoveMap.smallEyes: invalid key, position, or switchMap");
        }
    }
    const auto nodes = root["keyMapNodes"].toArray();
    for (int i = 0; i < nodes.size(); ++i) {
        const auto node = nodes[i].toObject();
        const QString type = node["type"].toString();
        const QString prefix = QString("keyMapNodes[%1] (%2): ").arg(i).arg(type);
        if (!types().contains(type) || type == "KMT_MOUSE_MOVE") return fail(error, prefix + "unsupported type");
        if (type != "KMT_STEER_WHEEL" && !validKey(node["key"])) return fail(error, prefix + "invalid key");
        if (type == "KMT_CLICK" || type == "KMT_CLICK_TWICE") {
            if (!position(node["pos"])) return fail(error, prefix + "pos must be in [0, 1]");
            if (type == "KMT_CLICK" && node.contains("switchMap") && !node["switchMap"].isBool()) return fail(error, prefix + "switchMap must be boolean");
        } else if (type == "KMT_DRAG") {
            if (!position(node["startPos"]) || !position(node["endPos"])) return fail(error, prefix + "invalid drag coordinates");
            if (node.contains("dragSpeed") && !number(node["dragSpeed"], 0, 1)) return fail(error, prefix + "dragSpeed must be in [0, 1]");
            if (node.contains("startDelay") && (!number(node["startDelay"], 0, 2147483647) || std::floor(node["startDelay"].toDouble()) != node["startDelay"].toDouble()))
                return fail(error, prefix + "startDelay must be a nonnegative integer");
        } else if (type == "KMT_CLICK_MULTI") {
            const auto clicks = node["clickNodes"].toArray();
            if (clicks.isEmpty() || clicks.size() > 50) return fail(error, prefix + "requires 1 to 50 click steps");
            double total = 0;
            for (const auto &entry : clicks) {
                const auto click = entry.toObject();
                const auto delay = click["delay"];
                if (!position(click["pos"]) || !number(delay, 0, 2147483647) || std::floor(delay.toDouble()) != delay.toDouble())
                    return fail(error, prefix + "invalid step position or delay");
                total += delay.toDouble() + 20;
            }
            if (total > 2147483647) return fail(error, prefix + "total delay exceeds timer capacity");
        } else if (type == "KMT_STEER_WHEEL") {
            if (!position(node["centerPos"])) return fail(error, prefix + "invalid centerPos");
            for (const auto &direction : {"left", "right", "up", "down"}) {
                if (!validKey(node[QString(direction) + "Key"]) || !number(node[QString(direction) + "Offset"], 0, 1))
                    return fail(error, prefix + "invalid directional key or offset");
            }
        } else if (type == "KMT_ANDROID_KEY") {
            const auto key = node["androidKey"];
            if (!number(key, 0, 2147483647) || std::floor(key.toDouble()) != key.toDouble())
                return fail(error, prefix + "androidKey must be a nonnegative integer");
        }
    }
    return true;
}

bool KeymapDocument::load(const QByteArray &bytes, QString *error)
{
    QJsonParseError parse;
    const auto doc = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError) return fail(error, parse.errorString());
    if (!doc.isObject()) return fail(error, "Expected a JSON object");
    if (!validate(doc.object(), error)) return false;
    m_root = doc.object();
    m_undo.clear();
    emit changed();
    return true;
}
bool KeymapDocument::save(const QString &path, QString *error)
{
    if (!validate(m_root, error)) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    const auto bytes = json();
    if (file.write(bytes) != bytes.size() || !file.commit()) return fail(error, file.errorString());
    m_undo.setClean();
    emit changed();
    return true;
}
bool KeymapDocument::replace(const QJsonObject &root, const QString &label, QString *error)
{
    if (!validate(root, error)) return false;
    if (m_root != root) m_undo.push(new KeymapChange(this, root, label));
    return true;
}
bool KeymapDocument::setValue(const QStringList &path, const QJsonValue &value, QString *error)
{
    if (path.isEmpty()) return fail(error, "Property path is empty");
    return replace(put(m_root, path, value).toObject(), tr("Edit %1").arg(path.join('.')), error);
}
bool KeymapDocument::addNode(const QString &type, QString *error)
{
    if (!types().contains(type)) return fail(error, "Unsupported mapping type");
    auto root = m_root;
    if (type == "KMT_MOUSE_MOVE") {
        if (root.contains("mouseMoveMap")) return fail(error, tr("Only one mouse movement map is supported."));
        root["mouseMoveMap"] = QJsonObject{{"startPos", point(0.6, 0.5)}, {"speedRatioX", 0.01}, {"speedRatioY", 0.01},
            {"smallEyes", QJsonObject{{"type", "KMT_CLICK"}, {"key", "Key_Alt"}, {"pos", point(0.8, 0.2)}, {"switchMap", false}}}};
    } else {
        auto nodes = root["keyMapNodes"].toArray();
        const auto key = QString("Key_%1").arg(QChar('A' + (nodes.size() % 26)));
        QJsonObject node{{"type", type}, {"key", key}};
        if (type == "KMT_CLICK" || type == "KMT_CLICK_TWICE") {
            node["pos"] = point(0.5, 0.5);
            if (type == "KMT_CLICK") node["switchMap"] = false;
        } else if (type == "KMT_DRAG") {
            node["startPos"] = point(0.4, 0.5); node["endPos"] = point(0.7, 0.5);
            node["dragSpeed"] = 1.0; node["startDelay"] = 0;
        } else if (type == "KMT_CLICK_MULTI") {
            node["clickNodes"] = QJsonArray{QJsonObject{{"pos", point(0.4, 0.5)}, {"delay", 100}},
                QJsonObject{{"pos", point(0.6, 0.5)}, {"delay", 100}}};
        } else if (type == "KMT_STEER_WHEEL") {
            node.remove("key"); node["centerPos"] = point(0.2, 0.7);
            node["leftKey"] = "Key_A"; node["rightKey"] = "Key_D";
            node["upKey"] = "Key_W"; node["downKey"] = "Key_S";
            for (const auto &d : {"leftOffset", "rightOffset", "upOffset", "downOffset"}) node[d] = 0.1;
        } else if (type == "KMT_ANDROID_KEY") {
            node["androidKey"] = 4;
        }
        nodes.append(node); root["keyMapNodes"] = nodes;
    }
    return replace(root, tr("Add %1").arg(type), error);
}
bool KeymapDocument::removeNode(int index, QString *error)
{
    auto root = m_root;
    if (index == -1 && root.contains("mouseMoveMap")) root.remove("mouseMoveMap");
    else {
        auto nodes = root["keyMapNodes"].toArray();
        if (index < 0 || index >= nodes.size()) return fail(error, "Select a mapping to delete");
        nodes.removeAt(index); root["keyMapNodes"] = nodes;
    }
    return replace(root, tr("Delete mapping"), error);
}
QVector<KeymapDocument::Handle> KeymapDocument::handles() const
{
    QVector<Handle> result;
    const auto add = [&](const QStringList &path, const QString &label, int index) {
        const auto value = valueAt(m_root, path);
        if (position(value)) { Handle h; h.path = path; h.position = toPoint(value); h.label = label; h.nodeIndex = index; result.append(h); }
    };
    if (m_root.contains("mouseMoveMap")) {
        add({"mouseMoveMap", "startPos"}, tr("Mouse"), -1);
        add({"mouseMoveMap", "smallEyes", "pos"}, tr("Look"), -1);
    }
    const auto nodes = m_root["keyMapNodes"].toArray();
    for (int i = 0; i < nodes.size(); ++i) {
        const auto node = nodes[i].toObject();
        const QStringList base{"keyMapNodes", QString::number(i)};
        const auto key = node["key"].toString();
        const auto type = node["type"].toString();
        if (type == "KMT_CLICK" || type == "KMT_CLICK_TWICE") add(base + QStringList{"pos"}, key, i);
        if (type == "KMT_DRAG") {
            add(base + QStringList{"startPos"}, key + tr(" start"), i);
            add(base + QStringList{"endPos"}, key + tr(" end"), i);
        }
        if (type == "KMT_CLICK_MULTI") {
            const auto clicks = node["clickNodes"].toArray();
            for (int j = 0; j < clicks.size(); ++j)
                add(base + QStringList{"clickNodes", QString::number(j), "pos"}, key + QString(" #%1").arg(j + 1), i);
        }
        if (type == "KMT_STEER_WHEEL") {
            add(base + QStringList{"centerPos"}, tr("WASD"), i);
            const auto center = toPoint(node["centerPos"]);
            for (const auto &direction : {"left", "right", "up", "down"}) {
                Handle h; h.path = base; h.offsetField = QString(direction) + "Offset"; h.nodeIndex = i;
                h.label = node[QString(direction) + "Key"].toString();
                const double offset = node[h.offsetField].toDouble();
                h.position = center + QPointF(QString(direction) == "left" ? -offset : QString(direction) == "right" ? offset : 0,
                    QString(direction) == "up" ? -offset : QString(direction) == "down" ? offset : 0);
                result.append(h);
            }
        }
    }
    return result;
}
bool KeymapDocument::moveHandle(const Handle &handle, QPointF pos, QString *error)
{
    pos.setX(qBound(0.0, pos.x(), 1.0)); pos.setY(qBound(0.0, pos.y(), 1.0));
    if (!handle.offsetField.isEmpty()) {
        const auto center = toPoint(valueAt(m_root, handle.path).toObject()["centerPos"]);
        double offset = 0;
        if (handle.offsetField == "leftOffset") offset = center.x() - pos.x();
        if (handle.offsetField == "rightOffset") offset = pos.x() - center.x();
        if (handle.offsetField == "upOffset") offset = center.y() - pos.y();
        if (handle.offsetField == "downOffset") offset = pos.y() - center.y();
        return setValue(handle.path + QStringList{handle.offsetField}, qBound(0.0, offset, 1.0), error);
    }
    auto value = valueAt(m_root, handle.path).toObject();
    value["x"] = pos.x(); value["y"] = pos.y();
    return setValue(handle.path, value, error);
}

QList<QByteArray> KeymapDocument::recoverySnapshots(const QByteArray &runtime, const QString &runtimePath, bool editing) const
{
    QList<QByteArray> result;
    if (editing && isDirty()) result.append(json());
    if (runtime.isEmpty()) return result;
    const auto running = QJsonDocument::fromJson(runtime);
    QFile file(runtimePath);
    const bool saved = file.open(QIODevice::ReadOnly) && QJsonDocument::fromJson(file.readAll()) == running;
    if (!saved && (result.isEmpty() || QJsonDocument::fromJson(result.first()) != running)) result.append(runtime);
    return result;
}
