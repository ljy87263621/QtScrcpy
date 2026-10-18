#include "keymapoverlay.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QLineF>

KeymapOverlay::KeymapOverlay(KeymapDocument *doc, QWidget *parent) : QWidget(parent), m_document(doc)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_NoSystemBackground);
    setCursor(Qt::CrossCursor);
    connect(doc, &KeymapDocument::changed, this, [this] { m_drag = -1; update(); });
    setToolTip(tr("Drag a marker to move it. Select mappings and edit properties in the editor."));
}
QPointF KeymapOverlay::normalizedAt(const QPointF &point) const
{
    return {qBound(0.0, point.x() / qMax(1, width()), 1.0),
            qBound(0.0, point.y() / qMax(1, height()), 1.0)};
}
void KeymapOverlay::setSelectedNode(int index) { m_selected = index; update(); }
void KeymapOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(12, 20, 35, 35));
    const auto handles = m_document->handles();
    QMap<int, QPointF> previous;
    for (int i = 0; i < handles.size(); ++i) {
        const auto &h = handles[i];
        const auto normalized = i == m_drag ? m_preview : h.position;
        const QPointF pt(normalized.x() * width(), normalized.y() * height());
        const bool selected = h.nodeIndex == m_selected;
        const QColor color = selected ? QColor(255, 199, 76) : QColor(92, 220, 255);
        p.setPen(QPen(color, selected ? 2.5 : 1.5));
        if (previous.contains(h.nodeIndex)) {
            const auto prev = previous[h.nodeIndex];
            p.drawLine(prev, pt);
        }
        if (h.offsetField.isEmpty() || !previous.contains(h.nodeIndex)) previous[h.nodeIndex] = pt;
        p.setBrush(QColor(15, 23, 42, 210));
        p.drawEllipse(pt, 12, 12);
        p.drawLine(pt + QPointF(-5, 0), pt + QPointF(5, 0));
        p.drawLine(pt + QPointF(0, -5), pt + QPointF(0, 5));
        auto label = h.label; label.remove("Key_");
        const int labelWidth = qMin(qMax(42, p.fontMetrics().horizontalAdvance(label) + 14), qMax(42, width() - 8));
        const qreal x = qBound(2.0, pt.x() - labelWidth / 2.0, qMax(2.0, width() - labelWidth - 2.0));
        const qreal y = qBound(2.0, pt.y() - 36.0, qMax(2.0, height() - 24.0));
        const QRectF textRect(x, y, labelWidth, 22);
        p.drawRoundedRect(textRect, 5, 5);
        p.drawText(textRect, Qt::AlignCenter, p.fontMetrics().elidedText(label, Qt::ElideRight, labelWidth - 10));
    }
    p.setPen(QColor(255, 255, 255));
    p.fillRect(QRect(0, 0, width(), 24), QColor(15, 23, 42, 205));
    p.drawText(QRect(8, 0, width() - 16, 24), Qt::AlignVCenter, tr("KEYMAP EDIT • Device input paused"));
}
void KeymapOverlay::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    setFocus();
    if (event->button() != Qt::LeftButton) return;
    const auto handles = m_document->handles();
    qreal best = 24;
    for (int i = 0; i < handles.size(); ++i) {
        const QPointF pt(handles[i].position.x() * width(), handles[i].position.y() * height());
        const qreal distance = QLineF(pt, event->pos()).length();
        if (distance < best) { best = distance; m_drag = i; }
    }
    if (m_drag >= 0) {
        m_preview = handles[m_drag].position;
        m_selected = handles[m_drag].nodeIndex;
        emit nodeSelected(m_selected);
        update();
    }
}
void KeymapOverlay::mouseMoveEvent(QMouseEvent *event)
{
    event->accept();
    if (m_drag < 0) return;
    m_preview = normalizedAt(event->pos());
    update();
}
void KeymapOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();
    if (event->button() != Qt::LeftButton || m_drag < 0) return;
    const auto handles = m_document->handles();
    const int index = m_drag;
    m_drag = -1;
    if (index < handles.size()) {
        QString error;
        m_document->moveHandle(handles[index], normalizedAt(event->pos()), &error);
    }
    update();
}
void KeymapOverlay::keyPressEvent(QKeyEvent *event)
{
    event->accept();
    if (event->matches(QKeySequence::Undo)) m_document->undoStack()->undo();
    else if (event->matches(QKeySequence::Redo)) m_document->undoStack()->redo();
}
void KeymapOverlay::keyReleaseEvent(QKeyEvent *event) { event->accept(); }
void KeymapOverlay::wheelEvent(QWheelEvent *event) { event->accept(); }
