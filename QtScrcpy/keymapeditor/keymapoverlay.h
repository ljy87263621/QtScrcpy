#ifndef KEYMAPOVERLAY_H
#define KEYMAPOVERLAY_H
#include <QWidget>
#include "keymapdocument.h"

class KeymapOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit KeymapOverlay(KeymapDocument *document, QWidget *parent = nullptr);
    QPointF normalizedAt(const QPointF &point) const;
    void setSelectedNode(int index);
signals:
    void nodeSelected(int index);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void wheelEvent(QWheelEvent *) override;
private:
    KeymapDocument *m_document;
    int m_selected = -2;
    int m_drag = -1;
    QPointF m_preview;
};
#endif
