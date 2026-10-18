#include "keycapturebutton.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMetaEnum>
#include <QFocusEvent>
KeyCaptureButton::KeyCaptureButton(const QString &key, QWidget *parent) : QPushButton(key, parent), m_key(key)
{
    setFocusPolicy(Qt::StrongFocus);
    setToolTip(tr("Click, then press one keyboard key or mouse button. Escape cancels. Modifiers are separate keys."));
    connect(this, &QPushButton::clicked, this, [this] {
        m_capturing = true; setText(tr("Press key / mouse…")); setFocus();
    });
}
bool KeyCaptureButton::event(QEvent *event)
{
    if (m_capturing && event->type() == QEvent::ShortcutOverride) { event->accept(); return true; }
    return QPushButton::event(event);
}
void KeyCaptureButton::keyPressEvent(QKeyEvent *event)
{
    if (!m_capturing) { QPushButton::keyPressEvent(event); return; }
    event->accept();
    if (event->isAutoRepeat()) return;
    if (event->key() == Qt::Key_Escape) { finish(QString()); return; }
    const char *name = QMetaEnum::fromType<Qt::Key>().valueToKey(event->key());
    if (name && event->key() != Qt::Key_unknown) finish(QString::fromLatin1(name));
}
void KeyCaptureButton::mousePressEvent(QMouseEvent *event)
{
    if (!m_capturing) { QPushButton::mousePressEvent(event); return; }
    event->accept();
    const char *name = QMetaEnum::fromType<Qt::MouseButtons>().valueToKey(event->button());
    if (name) finish(QString::fromLatin1(name));
}
void KeyCaptureButton::focusOutEvent(QFocusEvent *event)
{
    if (m_capturing) finish(QString());
    QPushButton::focusOutEvent(event);
}
void KeyCaptureButton::finish(const QString &key)
{
    m_capturing = false;
    if (!key.isEmpty()) m_key = key;
    setText(m_key);
    if (!key.isEmpty()) emit captured(key);
}
