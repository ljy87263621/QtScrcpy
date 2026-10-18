#ifndef KEYCAPTUREBUTTON_H
#define KEYCAPTUREBUTTON_H
#include <QPushButton>
class KeyCaptureButton : public QPushButton
{
    Q_OBJECT
public:
    explicit KeyCaptureButton(const QString &key, QWidget *parent = nullptr);
signals:
    void captured(const QString &key);
protected:
    bool event(QEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
private:
    void finish(const QString &key);
    QString m_key;
    bool m_capturing = false;
};
#endif
