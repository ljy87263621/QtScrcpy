#ifndef KEYMAPDOCUMENT_H
#define KEYMAPDOCUMENT_H
#include <QObject>
#include <QJsonObject>
#include <QPointF>
#include <QStringList>
#include <QUndoStack>

class KeymapDocument : public QObject
{
    Q_OBJECT
public:
    struct Handle {
        QStringList path;
        QPointF position;
        QString label;
        int nodeIndex = 0;
        QString offsetField;
    };
    explicit KeymapDocument(QObject *parent = nullptr);
    QJsonObject root() const { return m_root; }
    QByteArray json() const;
    bool load(const QByteArray &bytes, QString *error);
    bool save(const QString &path, QString *error);
    bool setValue(const QStringList &path, const QJsonValue &value, QString *error);
    bool replace(const QJsonObject &root, const QString &label, QString *error);
    bool addNode(const QString &type, QString *error);
    bool removeNode(int index, QString *error);
    bool moveHandle(const Handle &handle, QPointF pos, QString *error);
    QVector<Handle> handles() const;
    QList<QByteArray> recoverySnapshots(const QByteArray &runtime, const QString &runtimePath, bool editing) const;
    QUndoStack *undoStack() { return &m_undo; }
    bool isDirty() const { return !m_undo.isClean(); }
    static QStringList types();
    static bool validate(const QJsonObject &root, QString *error);
    static QJsonValue valueAt(QJsonValue root, const QStringList &path);
signals:
    void changed();
private:
    friend class KeymapChange;
    QJsonObject m_root;
    QUndoStack m_undo;
};
#endif
