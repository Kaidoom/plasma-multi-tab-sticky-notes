// SPDX-License-Identifier: BSD-2-Clause
#include <QApplication>
#include <QFile>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
#include <KConfigGroup>
#include <KConfigPropertyMap>
#include <KPackage/PackageLoader>
#include <Plasma/Corona>
#include <Plasma/PluginLoader>
#include <PlasmaQuick/AppletQuickItem>

class TestCorona : public Plasma::Corona
{
public:
    QRect screenGeometry(int) const override { return {0, 0, 1024, 768}; }
};

class AppletIntegration : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void singleNoteBaseline()
    {
        const auto plugins = Plasma::PluginLoader::self()->listAppletMetaData(QString());
        QVERIFY(std::any_of(plugins.begin(), plugins.end(), [](const auto &p) { return p.pluginId() == QStringLiteral("dev.kaidoom.tabbednotes"); }));
        QVERIFY(std::any_of(plugins.begin(), plugins.end(), [](const auto &p) { return p.pluginId() == QStringLiteral("org.kde.plasma.notes"); }));
        TestCorona corona;
        corona.setKPackage(KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/Shell"), QStringLiteral("org.kde.plasma.desktop")));
        auto *containment = corona.createContainment(QStringLiteral("null"));
        QVERIFY(containment);
        auto *applet = containment->createApplet(QStringLiteral("dev.kaidoom.tabbednotes"));
        QVERIFY(applet);
        QVERIFY2(applet->launchErrorMessage().isEmpty(), qPrintable(applet->launchErrorMessage()));
        QVERIFY(QFile::exists(QStringLiteral(":/qt/qml/plasma/applet/dev/kaidoom/tabbednotes/main.qml")));
        auto *item = PlasmaQuick::AppletQuickItem::itemForApplet(applet);
        QVERIFY2(item && !applet->failedToLaunch(), qPrintable(applet->launchErrorMessage()));
        QQuickWindow window;
        window.resize(400, 400);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(400, 400));
        window.show();
        QTRY_VERIFY(item->fullRepresentationItem());
        auto *editor = item->property("mainTextArea").value<QQuickItem *>();
        QVERIFY(editor);
        editor->forceActiveFocus();
        const QString contents = QStringLiteral("<p><b>Baseline Ω 日本語</b> <a href=\"https://kde.org\">KDE</a></p>");
        QVERIFY(editor->setProperty("text", contents));
        QQmlExpression flush(qmlContext(item), item, QStringLiteral("throttledTextSaver.triggered()"));
        flush.evaluate();
        QVERIFY2(!flush.hasError(), qPrintable(flush.error().toString()));
        const QString id = applet->configuration()->value(QStringLiteral("noteId")).toString();
        QVERIFY(!id.isEmpty());
        QFile note(qEnvironmentVariable("XDG_DATA_HOME") + QStringLiteral("/plasma_tabbed_notes/") + id);
        QVERIFY(note.open(QIODevice::ReadOnly));
        const auto saved = note.readAll();
        QVERIFY(saved.contains("Baseline"));
        QVERIFY(saved.contains(QStringLiteral("日本語").toUtf8()));
        QVERIFY(saved.contains("https://kde.org"));
        QVERIFY(!QFile::exists(qEnvironmentVariable("XDG_DATA_HOME") + QStringLiteral("/plasma_notes")));
        QTest::qWait(200);
        if (const auto path = qEnvironmentVariable("TABBEDNOTES_SCREENSHOT"); !path.isEmpty()) {
            QVERIFY(window.grabWindow().save(path));
        }
        applet->config().sync();
        delete applet;
        QVERIFY(note.exists());
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir sandbox;
    if (!sandbox.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", (sandbox.path() + QStringLiteral("/config")).toUtf8());
    qputenv("XDG_DATA_HOME", (sandbox.path() + QStringLiteral("/data")).toUtf8());
    qputenv("XDG_CACHE_HOME", (sandbox.path() + QStringLiteral("/cache")).toUtf8());
    QDir().mkpath(sandbox.path() + QStringLiteral("/data"));
    QApplication app(argc, argv);
    AppletIntegration test;
    return QTest::qExec(&test, argc, argv);
}

#include "appletintegration.moc"
