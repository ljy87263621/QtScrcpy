#include "keymapeditor.h"
#include "keycapturebutton.h"
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
class PropertyDelegate : public QStyledItemDelegate
{
public:
    explicit PropertyDelegate(QObject *parent) : QStyledItemDelegate(parent) {}
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        return index.column() == 1 ? QStyledItemDelegate::createEditor(parent, option, index) : nullptr;
    }
};
QString typeName(const QString &type)
{
    static const QMap<QString, QString> names{
        {"KMT_CLICK", QStringLiteral("单击")}, {"KMT_CLICK_TWICE", QStringLiteral("按下/松开双击")},
        {"KMT_CLICK_MULTI", QStringLiteral("一键多点")}, {"KMT_STEER_WHEEL", QStringLiteral("方向轮盘")},
        {"KMT_DRAG", QStringLiteral("拖动/滑动")}, {"KMT_ANDROID_KEY", QStringLiteral("Android 按键")},
        {"KMT_MOUSE_MOVE", QStringLiteral("鼠标视角")}};
    return names.value(type, type);
}
QString fieldName(const QString &field)
{
    static const QMap<QString, QString> names{
        {"key", QStringLiteral("按键")}, {"switchKey", QStringLiteral("模式切换键")},
        {"pos", QStringLiteral("位置")}, {"startPos", QStringLiteral("起点")}, {"endPos", QStringLiteral("终点")},
        {"centerPos", QStringLiteral("轮盘中心")}, {"switchMap", QStringLiteral("释放鼠标")},
        {"dragSpeed", QStringLiteral("滑动速度 (0–1)")}, {"startDelay", QStringLiteral("起始延迟 (ms)")},
        {"delay", QStringLiteral("延迟 (ms)")}, {"comment", QStringLiteral("说明")},
        {"clickNodes", QStringLiteral("点击序列")}, {"smallEyes", QStringLiteral("小眼睛")},
        {"androidKey", QStringLiteral("Android 键码")}};
    return names.value(field, field);
}
bool keyField(const QString &field) { return field == "key" || field.endsWith("Key"); }
}

KeymapEditor::KeymapEditor(QWidget *parent) : QWidget(parent, Qt::Tool)
{
    setWindowTitle(QStringLiteral("Keymap 可视化编辑"));
    setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    resize(430, 740);
    setMinimumSize(370, 480);
    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(QStringLiteral("Keymap 可视化编辑"), this);
    QFont font = title->font(); font.setPointSize(font.pointSize() + 3); font.setBold(true); title->setFont(font);
    layout->addWidget(title);
    auto *hint = new QLabel(QStringLiteral("拖动画面上的标记调整位置。编辑期间设备输入暂停。"), this);
    hint->setWordWrap(true); layout->addWidget(hint);
    auto *files = new QHBoxLayout;
    const auto button = [this](const QString &text, QBoxLayout *row, const std::function<void()> &action) {
        auto *b = new QPushButton(text, this); row->addWidget(b); connect(b, &QPushButton::clicked, this, action); return b;
    };
    button(QStringLiteral("新建"), files, [this] {
        if (!mayDiscard()) return;
        QString error; loadScript("{\"switchKey\":\"Key_QuoteLeft\",\"keyMapNodes\":[]}", QString(), &error);
    });
    button(QStringLiteral("导入…"), files, [this] {
        const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("导入按键映射"), m_path, "JSON (*.json)");
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) { report(file.errorString()); return; }
        const auto bytes = file.readAll();
        KeymapDocument candidate; QString error;
        if (!candidate.load(bytes, &error)) { report(error); return; }
        if (mayDiscard()) loadScript(bytes, path, &error);
    });
    button(QStringLiteral("保存"), files, [this] { save(false); });
    button(QStringLiteral("另存为…"), files, [this] { save(true); });
    layout->addLayout(files);
    auto *history = new QHBoxLayout;
    auto *undo = new QPushButton(QStringLiteral("撤销"), this);
    auto *redo = new QPushButton(QStringLiteral("重做"), this);
    history->addWidget(undo); history->addWidget(redo);
    connect(undo, &QPushButton::clicked, m_document.undoStack(), &QUndoStack::undo);
    connect(redo, &QPushButton::clicked, m_document.undoStack(), &QUndoStack::redo);
    connect(m_document.undoStack(), &QUndoStack::canUndoChanged, undo, &QPushButton::setEnabled);
    connect(m_document.undoStack(), &QUndoStack::canRedoChanged, redo, &QPushButton::setEnabled);
    undo->setEnabled(false); redo->setEnabled(false);
    auto *undoShortcut = new QShortcut(QKeySequence::Undo, this);
    auto *redoShortcut = new QShortcut(QKeySequence::Redo, this);
    connect(undoShortcut, &QShortcut::activated, m_document.undoStack(), &QUndoStack::undo);
    connect(redoShortcut, &QShortcut::activated, m_document.undoStack(), &QUndoStack::redo);
    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, [this] { save(false); });
    layout->addLayout(history);
    m_list = new QListWidget(this); m_list->setObjectName("mappingList");
    m_list->setMaximumHeight(190); layout->addWidget(m_list, 1);
    auto *create = new QHBoxLayout;
    m_type = new QComboBox(this);
    for (const auto &type : KeymapDocument::types()) m_type->addItem(typeName(type), type);
    create->addWidget(m_type, 1);
    button(QStringLiteral("添加"), create, [this] {
        QString error;
        if (!m_document.addNode(m_type->currentData().toString(), &error)) { report(error); return; }
        m_selected = m_type->currentData() == "KMT_MOUSE_MOVE" ? -1 : m_document.root()["keyMapNodes"].toArray().size() - 1;
    })->setObjectName("addMappingButton");
    button(QStringLiteral("删除"), create, [this] {
        QString error;
        if (!m_document.removeNode(m_selected, &error)) report(error);
        else m_selected = -2;
    });
    layout->addLayout(create);
    m_properties = new QTreeWidget(this); m_properties->setObjectName("mappingProperties");
    m_properties->setItemDelegate(new PropertyDelegate(m_properties));
    m_properties->setColumnCount(2); m_properties->setHeaderLabels({QStringLiteral("属性"), QStringLiteral("值")});
    m_properties->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_properties->header()->setStretchLastSection(true);
    m_properties->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    layout->addWidget(m_properties, 3);
    connect(m_properties, &QTreeWidget::itemChanged, this, &KeymapEditor::propertyChanged);
    auto *steps = new QHBoxLayout;
    button(QStringLiteral("添加点击点"), steps, [this] {
        const auto path = selectedPath();
        auto node = KeymapDocument::valueAt(m_document.root(), path).toObject();
        if (node["type"] != "KMT_CLICK_MULTI") { report(QStringLiteral("请先选择“一键多点”映射。")); return; }
        auto clicks = node["clickNodes"].toArray();
        clicks.append(QJsonObject{{"delay", 100}, {"pos", QJsonObject{{"x", 0.5}, {"y", 0.5}}}});
        QString error;
        if (!m_document.setValue(path + QStringList{"clickNodes"}, clicks, &error)) report(error);
    });
    button(QStringLiteral("删除点击点"), steps, [this] {
        auto *item = m_properties->currentItem();
        const auto path = item ? item->data(0, Qt::UserRole).toStringList() : QStringList();
        if (path.size() < 4 || path[2] != "clickNodes") { report(QStringLiteral("请在属性中选择要删除的点击序列项。")); return; }
        auto array = KeymapDocument::valueAt(m_document.root(), path.mid(0, 3)).toArray();
        array.removeAt(path[3].toInt()); QString error;
        if (!m_document.setValue(path.mid(0, 3), array, &error)) report(error);
    });
    button(QStringLiteral("高级 JSON…"), steps, [this] {
        QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("编辑所选映射 JSON")); dialog.resize(600, 560);
        auto *box = new QVBoxLayout(&dialog);
        auto *text = new QPlainTextEdit(&dialog); box->addWidget(text);
        const auto path = selectedPath();
        text->setPlainText(QString::fromUtf8(QJsonDocument(KeymapDocument::valueAt(m_document.root(), path).toObject()).toJson()));
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog); box->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, &dialog, text, path] {
            QJsonParseError parse; const auto json = QJsonDocument::fromJson(text->toPlainText().toUtf8(), &parse);
            if (parse.error != QJsonParseError::NoError || !json.isObject()) { report(parse.errorString()); return; }
            QString error;
            const bool ok = path.isEmpty() ? m_document.replace(json.object(), tr("Edit JSON"), &error)
                : m_document.setValue(path, json.object(), &error);
            if (ok) dialog.accept(); else report(error);
        });
        dialog.exec();
    });
    layout->addLayout(steps);
    m_status = new QLabel(this); m_status->setWordWrap(true); m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_status);
    auto *actions = new QHBoxLayout;
    button(QStringLiteral("取消编辑"), actions, [this] { close(); });
    auto *apply = button(QStringLiteral("应用并恢复控制"), actions, [this] {
        QString error;
        if (!KeymapDocument::validate(m_document.root(), &error)) { report(error); return; }
        emit applied(m_document.json(), m_path);
    });
    apply->setObjectName("applyMappingButton"); apply->setDefault(true);
    layout->addLayout(actions);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_refreshing || row < 0) return;
        m_selected = m_list->item(row)->data(Qt::UserRole).toInt();
        refreshProperties(); emit selectionChanged(m_selected);
    });
    connect(&m_document, &KeymapDocument::changed, this, [this] {
        if (m_refreshPending) return;
        m_refreshPending = true;
        QTimer::singleShot(0, this, [this] { m_refreshPending = false; refresh(); });
    });
    refresh();
}

bool KeymapEditor::loadScript(const QByteArray &bytes, const QString &path, QString *error)
{
    if (!m_document.load(bytes, error)) return false;
    m_path = path; m_selected = -2;
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)
        || QJsonDocument::fromJson(file.readAll()).object() != m_document.root()) m_document.undoStack()->resetClean();
    refresh();
    return true;
}
QStringList KeymapEditor::selectedPath() const
{
    if (m_selected == -1) return {"mouseMoveMap"};
    if (m_selected >= 0) return {"keyMapNodes", QString::number(m_selected)};
    return {};
}
void KeymapEditor::selectNode(int index)
{
    for (int i = 0; i < m_list->count(); ++i)
        if (m_list->item(i)->data(Qt::UserRole).toInt() == index) { m_list->setCurrentRow(i); return; }
}
void KeymapEditor::refresh()
{
    m_refreshing = true;
    m_list->clear();
    const auto add = [this](const QString &name, int index) {
        auto *item = new QListWidgetItem(name, m_list); item->setData(Qt::UserRole, index);
    };
    add(QStringLiteral("全局 · 模式切换键"), -2);
    if (m_document.root().contains("mouseMoveMap")) add(QStringLiteral("鼠标视角 · 小眼睛"), -1);
    const auto nodes = m_document.root()["keyMapNodes"].toArray();
    for (int i = 0; i < nodes.size(); ++i) {
        const auto node = nodes[i].toObject();
        add(QString("%1  %2  %3").arg(i + 1).arg(typeName(node["type"].toString()), node["key"].toString(node["comment"].toString())), i);
    }
    if (m_selected >= nodes.size() || (m_selected == -1 && !m_document.root().contains("mouseMoveMap"))) m_selected = -2;
    selectNode(m_selected);
    m_refreshing = false;
    refreshProperties();
    emit selectionChanged(m_selected);
    m_status->setText((m_path.isEmpty() ? QStringLiteral("未命名配置") : QDir::toNativeSeparators(m_path))
        + (m_document.isDirty() ? QStringLiteral("\n● 有未保存的更改；应用不会自动保存。") : QStringLiteral("\n已保存")));
    setWindowTitle(QStringLiteral("Keymap 可视化编辑") + (m_document.isDirty() ? " *" : ""));
}
void KeymapEditor::refreshProperties()
{
    const QSignalBlocker blocker(m_properties);
    m_properties->clear();
    auto object = KeymapDocument::valueAt(m_document.root(), selectedPath()).toObject();
    if (m_selected == -2) { object.remove("keyMapNodes"); object.remove("mouseMoveMap"); }
    addProperties(nullptr, object, selectedPath());
    m_properties->expandAll();
}
void KeymapEditor::addProperties(QTreeWidgetItem *parent, const QJsonValue &value, const QStringList &path)
{
    QStringList fields;
    if (value.isObject()) fields = value.toObject().keys();
    else for (int i = 0; i < value.toArray().size(); ++i) fields.append(QString::number(i));
    for (const auto &field : fields) {
        const auto childPath = path + QStringList{field};
        const auto childValue = value.isArray() ? value.toArray().at(field.toInt()) : value.toObject()[field];
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_properties);
        item->setText(0, fieldName(field)); item->setToolTip(0, childPath.join('.'));
        item->setData(0, Qt::UserRole, childPath);
        item->setData(0, Qt::UserRole + 1, int(childValue.type()));
        if (childValue.isObject() || childValue.isArray()) { addProperties(item, childValue, childPath); continue; }
        if (childValue.isBool()) {
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(1, childValue.toBool() ? Qt::Checked : Qt::Unchecked);
        } else if (childValue.isString() && keyField(field)) {
            auto *capture = new KeyCaptureButton(childValue.toString(), m_properties);
            m_properties->setItemWidget(item, 1, capture);
            connect(capture, &KeyCaptureButton::captured, this, [this, childPath](const QString &key) {
                QString error; if (!m_document.setValue(childPath, key, &error)) report(error);
            });
        } else {
            item->setText(1, childValue.isString() ? childValue.toString() : QString::number(childValue.toDouble(), 'g', 12));
            if (!childValue.isNull() && field != "type") item->setFlags(item->flags() | Qt::ItemIsEditable);
        }
    }
}
void KeymapEditor::propertyChanged(QTreeWidgetItem *item, int column)
{
    if (column != 1) return;
    const auto path = item->data(0, Qt::UserRole).toStringList();
    const auto type = static_cast<QJsonValue::Type>(item->data(0, Qt::UserRole + 1).toInt());
    QJsonValue value;
    if (type == QJsonValue::Bool) value = item->checkState(1) == Qt::Checked;
    else if (type == QJsonValue::Double) {
        bool ok = false; value = item->text(1).toDouble(&ok);
        if (!ok) { report(QStringLiteral("请输入有效数字。")); QTimer::singleShot(0, this, &KeymapEditor::refreshProperties); return; }
    } else value = item->text(1);
    QString error;
    if (!m_document.setValue(path, value, &error)) { report(error); QTimer::singleShot(0, this, &KeymapEditor::refreshProperties); }
}
bool KeymapEditor::save(bool saveAs)
{
    QString path = m_path;
    if (saveAs || path.isEmpty()) {
        auto directory = QString::fromLocal8Bit(qgetenv("QTSCRCPY_KEYMAP_PATH"));
        if (directory.isEmpty()) directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/keymap";
        QDir().mkpath(directory);
        path = QFileDialog::getSaveFileName(this, QStringLiteral("保存按键映射"), path.isEmpty() ? directory + "/custom.json" : path, "JSON (*.json)");
        if (path.isEmpty()) return false;
        if (!path.endsWith(".json", Qt::CaseInsensitive)) path += ".json";
    }
    QString error;
    if (!m_document.save(path, &error)) { report(error); return false; }
    m_path = path; refresh(); emit saved(path); return true;
}
bool KeymapEditor::mayDiscard()
{
    if (!m_document.isDirty()) return true;
    const auto result = QMessageBox::question(this, QStringLiteral("未保存的配置"), QStringLiteral("配置尚未保存，是否保存更改？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    return result == QMessageBox::Discard || (result == QMessageBox::Save && save(false));
}
void KeymapEditor::closeEvent(QCloseEvent *event)
{
    if (!mayDiscard()) { event->ignore(); return; }
    event->accept(); emit canceled();
}
void KeymapEditor::report(const QString &error) { QMessageBox::warning(this, QStringLiteral("按键映射"), error); }
