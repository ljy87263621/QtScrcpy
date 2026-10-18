#ifndef KEYMAPEDITOR_H
#define KEYMAPEDITOR_H
#include <QWidget>
#include "keymapdocument.h"
class QListWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QComboBox;
class KeymapEditor : public QWidget
{
    Q_OBJECT
public:
    explicit KeymapEditor(QWidget *parent = nullptr);
    KeymapDocument *document() { return &m_document; }
    bool loadScript(const QByteArray &bytes, const QString &path, QString *error);
    QString filePath() const { return m_path; }
    void selectNode(int index);
signals:
    void selectionChanged(int index);
    void applied(const QByteArray &script, const QString &path);
    void canceled();
    void saved(const QString &path);
protected:
    void closeEvent(QCloseEvent *) override;
private:
    void refresh();
    void refreshProperties();
    void addProperties(QTreeWidgetItem *parent, const QJsonValue &value, const QStringList &path);
    void propertyChanged(QTreeWidgetItem *item, int column);
    bool save(bool saveAs);
    bool mayDiscard();
    void report(const QString &error);
    QStringList selectedPath() const;
    KeymapDocument m_document;
    QString m_path;
    int m_selected = -2;
    bool m_refreshing = false;
    bool m_refreshPending = false;
    QListWidget *m_list = nullptr;
    QTreeWidget *m_properties = nullptr;
    QLabel *m_status = nullptr;
    QComboBox *m_type = nullptr;
};
#endif
