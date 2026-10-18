#include <QApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QPainter>
#include <QTimer>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <cstdio>
#include "keymapeditor.h"
#include "keymapoverlay.h"

class PreviewSurface : public QWidget
{
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        QLinearGradient gradient(0, 0, width(), height());
        gradient.setColorAt(0, QColor("#15354a")); gradient.setColorAt(1, QColor("#101827"));
        p.fillRect(rect(), gradient);
        p.setPen(QColor(255, 255, 255, 25));
        for (int x = 0; x < width(); x += 60) p.drawLine(x, 0, x, height());
        for (int y = 0; y < height(); y += 60) p.drawLine(0, y, width(), y);
        p.setPen(QColor("#c6dbe8"));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("编辑器测试画布\n（不代表真机投屏）"));
    }
};
int main(int argc, char **argv)
{
    const bool graphicsProbe = argc == 2 && QString::fromLocal8Bit(argv[1]) == "--graphics-probe";
    if (graphicsProbe) QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
    QApplication app(argc, argv);
    if (graphicsProbe) {
        QOpenGLContext context;
        QSurfaceFormat format; format.setVersion(2, 0); context.setFormat(format);
        if (!context.create()) return 5;
        QOffscreenSurface surface; surface.setFormat(context.format()); surface.create();
        if (!surface.isValid() || !context.makeCurrent(&surface)) return 6;
        auto *functions = context.functions(); functions->initializeOpenGLFunctions();
        std::printf("OpenGL: %s\n", functions->glGetString(GL_VERSION));
        context.doneCurrent(); return 0;
    }
    if (argc < 3) return 2;
    QWidget window;
    auto *layout = new QHBoxLayout(&window);
    auto *surface = new PreviewSurface;
    surface->setFixedSize(700, 620); layout->addWidget(surface);
    auto *editor = new KeymapEditor;
    editor->setWindowFlags(Qt::Widget); editor->setFixedSize(430, 780); layout->addWidget(editor);
    QFile file(QString::fromLocal8Bit(argv[1]));
    if (!file.open(QIODevice::ReadOnly)) return 3;
    QString error;
    if (!editor->loadScript(file.readAll(), file.fileName(), &error)) return 4;
    auto *overlay = new KeymapOverlay(editor->document(), surface);
    overlay->setGeometry(surface->rect()); overlay->show();
    QObject::connect(overlay, &KeymapOverlay::nodeSelected, editor, &KeymapEditor::selectNode);
    QObject::connect(editor, &KeymapEditor::selectionChanged, overlay, &KeymapOverlay::setSelectedNode);
    editor->selectNode(0);
    window.show();
    const QString output = QString::fromLocal8Bit(argv[2]);
    QTimer::singleShot(250, &window, [&app, &window, output] { window.grab().save(output); app.quit(); });
    return app.exec();
}
