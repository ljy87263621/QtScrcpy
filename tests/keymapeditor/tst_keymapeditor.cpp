#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include "keymapdocument.h"
#include "keymapoverlay.h"
#include "keycapturebutton.h"
#include "keymapeditor.h"
#include <QTreeWidget>
#include <QListWidget>
#include "keymap.h"

class KeymapEditorTest : public QObject
{
    Q_OBJECT
private slots:
    void recoverySeparatesAppliedAndCanceledDrafts()
    {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const auto path = temp.filePath("map.json");
        KeymapDocument doc; QString error;
        QVERIFY(doc.addNode("KMT_CLICK", &error));
        QVERIFY(doc.save(path, &error));
        const auto saved = doc.json();
        QVERIFY(doc.setValue({"keyMapNodes","0","key"}, "Key_B", &error));
        const auto applied = doc.json();
        QCOMPARE(doc.recoverySnapshots(saved, path, false).size(), 0); // canceled draft
        QCOMPARE(doc.recoverySnapshots(applied, path, false), QList<QByteArray>{applied});
        QVERIFY(doc.setValue({"keyMapNodes","0","key"}, "Key_C", &error));
        const auto snapshots = doc.recoverySnapshots(applied, path, true);
        QCOMPARE(snapshots.size(), 2); // active draft and previous applied version
        QCOMPARE(snapshots[0], doc.json());
        QCOMPARE(snapshots[1], applied);
        QVERIFY(doc.save(path, &error));
        QCOMPARE(doc.recoverySnapshots(doc.json(), path, false).size(), 0);
    }
    void newMappingsAcceptedByRuntime()
    {
        for (const auto &type : KeymapDocument::types()) {
            KeymapDocument doc; QString error;
            QVERIFY(doc.addNode(type, &error));
            KeyMap runtime; runtime.loadKeyMap(QString::fromUtf8(doc.json()));
            if (type == "KMT_MOUSE_MOVE") QVERIFY(runtime.isValidMouseMoveMap());
            else QVERIFY2(runtime.getKeyMapNodeKey(Qt::Key_A).type != KeyMap::KMT_INVALID, qPrintable(type));
        }
    }
    void editorDocumentAndProperties()
    {
        KeymapEditor editor;
        QString error;
        QVERIFY(editor.loadScript(R"({"switchKey":"Key_QuoteLeft","keyMapNodes":[]})", QString(), &error));
        QVERIFY(editor.document()->addNode("KMT_CLICK", &error));
        editor.show();
        QCoreApplication::processEvents();
        auto *list = editor.findChild<QListWidget *>("mappingList");
        QVERIFY(list);
        QCOMPARE(list->count(), 2); // global + click
        list->setCurrentRow(1);
        QCoreApplication::processEvents();
        auto *properties = editor.findChild<QTreeWidget *>("mappingProperties");
        QVERIFY(properties && properties->topLevelItemCount() >= 3);
        const auto before = editor.document()->root();
        QVERIFY(!editor.loadScript("invalid", QString(), &error));
        QCOMPARE(editor.document()->root(), before);
        editor.document()->undoStack()->setClean();
        editor.close();
    }
    void overlayDragAndInputIsolation()
    {
        KeymapDocument doc;
        QString error;
        QVERIFY(doc.addNode("KMT_CLICK", &error));
        KeymapOverlay overlay(&doc);
        overlay.resize(400, 200); overlay.show();
        QCOMPARE(overlay.normalizedAt(QPointF(200, 100)), QPointF(0.5, 0.5));
        QCOMPARE(overlay.normalizedAt(QPointF(-20, 500)), QPointF(0, 1));
        QSignalSpy selected(&overlay, &KeymapOverlay::nodeSelected);
        QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(200, 100));
        QTest::mouseMove(&overlay, QPoint(300, 150));
        QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(300, 150));
        QCOMPARE(doc.handles().first().position, QPointF(0.75, 0.75));
        QCOMPARE(doc.undoStack()->count(), 2); // add + one complete drag
        QVERIFY(!selected.isEmpty());
        QTest::keyClick(&overlay, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(doc.handles().first().position, QPointF(0.5, 0.5));
        overlay.resize(200, 400);
        QCOMPARE(overlay.normalizedAt(QPointF(100, 200)), QPointF(0.5, 0.5));
    }
    void captureQtKeyNames()
    {
        KeyCaptureButton capture("Key_A"); capture.show();
        QSignalSpy spy(&capture, &KeyCaptureButton::captured);
        QTest::mouseClick(&capture, Qt::LeftButton);
        QTest::keyClick(&capture, Qt::Key_F8);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().first().toString(), QString("Key_F8"));
        QTest::mouseClick(&capture, Qt::LeftButton);
        QTest::mouseClick(&capture, Qt::RightButton);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().first().toString(), QString("RightButton"));
        QTest::mouseClick(&capture, Qt::LeftButton);
        QTest::keyClick(&capture, Qt::Key_Escape);
        QCOMPARE(spy.count(), 0);
    }
    void losslessRoundTrip()
    {
        const QByteArray json = R"({"switchKey":"Key_QuoteLeft","extension":{"version":9},"keyMapNodes":[{"type":"KMT_CLICK","key":"Key_A","pos":{"x":0.2,"y":0.3},"switchMap":false,"comment":"custom","extension":[1,2]}]})";
        KeymapDocument doc;
        QString error;
        QVERIFY2(doc.load(json, &error), qPrintable(error));
        QVERIFY(doc.setValue({"keyMapNodes","0","key"}, "Key_B", &error));
        auto expected = QJsonDocument::fromJson(json).object();
        auto nodes = expected["keyMapNodes"].toArray();
        auto node = nodes[0].toObject(); node["key"] = "Key_B"; nodes[0] = node;
        expected["keyMapNodes"] = nodes;
        QCOMPARE(QJsonDocument::fromJson(doc.json()).object(), expected);
        doc.undoStack()->undo();
        QCOMPARE(doc.root(), QJsonDocument::fromJson(json).object());
        QVERIFY(!doc.isDirty());
        doc.undoStack()->redo();
        QVERIFY(doc.isDirty());
    }
    void malformedImportIsAtomic()
    {
        KeymapDocument doc;
        const auto before = doc.root();
        QString error;
        QVERIFY(!doc.load("{broken", &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(doc.root(), before);
        QVERIFY(!doc.load(R"({"switchKey":"not_a_qt_key","keyMapNodes":[]})", &error));
        QVERIFY(!doc.load(R"({"switchKey":"Key_A","keyMapNodes":[{"type":"KMT_DRAG","key":"Key_B"}]})", &error));
        QCOMPARE(doc.root(), before);
    }
    void allTypesAndCoordinateBounds()
    {
        KeymapDocument doc;
        QString error;
        for (const auto &type : KeymapDocument::types()) {
            QVERIFY2(doc.addNode(type, &error), qPrintable(type + ": " + error));
            QVERIFY2(KeymapDocument::validate(doc.root(), &error), qPrintable(error));
        }
        QVERIFY(doc.handles().size() >= 10);
        QVERIFY(!doc.setValue({"keyMapNodes","0","pos","x"}, 1.5, &error));
        QVERIFY(!doc.setValue({"keyMapNodes","0","key"}, "Key_Bogus", &error));
        auto handles = doc.handles();
        const auto first = handles.first();
        QVERIFY(doc.moveHandle(first, QPointF(-0.3, 2.0), &error));
        QCOMPARE(doc.handles().first().position, QPointF(0, 1));
        doc.undoStack()->undo();
        QCOMPARE(doc.handles().first().position, first.position);
    }
    void saveAndDirtyState()
    {
        KeymapDocument doc;
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QString error;
        QVERIFY(doc.addNode("KMT_CLICK", &error));
        QVERIFY(doc.isDirty());
        QVERIFY(!doc.save(temp.path(), &error));
        QVERIFY(doc.isDirty());
        const auto path = temp.filePath("map.json");
        QVERIFY2(doc.save(path, &error), qPrintable(error));
        QVERIFY(!doc.isDirty());
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        KeymapDocument reopened;
        QVERIFY(reopened.load(file.readAll(), &error));
        QCOMPARE(reopened.root(), doc.root());
        QVERIFY(doc.removeNode(0, &error));
        doc.undoStack()->undo();
        QVERIFY(!doc.isDirty());
        QCOMPARE(doc.root(), reopened.root());
    }
    void shippedScripts()
    {
        QDir dir(QStringLiteral(FIXTURE_DIR));
        const auto names = dir.entryList({"*.json"}, QDir::Files);
        QVERIFY2(names.size() >= 5, "Bundled scripts must be available");
        for (const auto &name : names) {
            QFile file(dir.filePath(name)); QVERIFY(file.open(QIODevice::ReadOnly));
            const auto bytes = file.readAll();
            KeymapDocument doc; QString error;
            QVERIFY2(doc.load(bytes, &error), qPrintable(name + ": " + error));
            QCOMPARE(doc.root(), QJsonDocument::fromJson(bytes).object());
        }
    }
};
QTEST_MAIN(KeymapEditorTest)
#include "tst_keymapeditor.moc"
