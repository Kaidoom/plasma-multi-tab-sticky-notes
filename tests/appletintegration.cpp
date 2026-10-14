// SPDX-License-Identifier: BSD-2-Clause
#include <KConfigGroup>
#include <KConfigLoader>
#include <KConfigPropertyMap>
#include <KPackage/PackageLoader>
#include <Plasma/Corona>
#include <Plasma/PluginLoader>
#include <PlasmaQuick/AppletQuickItem>
#include <QAbstractItemModelTester>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMimeData>
#include <QProcess>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <QWheelEvent>

using namespace Qt::StringLiterals;

class TestCorona : public Plasma::Corona
{
public:
    QRect screenGeometry(int) const override { return { 0, 0, 1024, 768 }; }
};

static QList<QQuickItem *> visualItems(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> result;
    if (root->objectName() == name)
        result.append(root);
    for (auto *child : root->childItems())
        result.append(visualItems(child, name));
    return result;
}

struct ClipboardRestore {
    QMimeData *previous = new QMimeData;
    ClipboardRestore()
    {
        if (const auto *data = QApplication::clipboard()->mimeData()) {
            for (const auto &format : data->formats())
                previous->setData(format, data->data(format));
        }
    }
    ~ClipboardRestore() { QApplication::clipboard()->setMimeData(previous); }
};

static QString plain(const QString &html)
{
    QTextDocument d;
    d.setHtml(html);
    return d.toPlainText();
}
static QString stored(const QString &id)
{
    QFile file(qEnvironmentVariable("XDG_DATA_HOME") + u"/plasma_tabbed_notes/"_s + id);
    if (!file.open(QIODevice::ReadOnly))
        return { };
    return QString::fromUtf8(file.readAll());
}
static QVariant evaluate(QObject *object, const QString &code)
{
    QQmlExpression expression(qmlContext(object), object, code);
    auto result = expression.evaluate();
    if (expression.hasError())
        qFatal("%s", qPrintable(expression.error().toString()));
    return result;
}
static QString quoted(const QString &text)
{
    const auto array = QJsonDocument(QJsonArray { text }).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(array.mid(1, array.size() - 2));
}

struct Harness {
    TestCorona corona;
    Plasma::Containment *containment;
    Plasma::Applet *applet = nullptr;
    PlasmaQuick::AppletQuickItem *item = nullptr;
    QQuickWindow window;
    Harness(bool panel = false)
    {
        corona.setKPackage(KPackage::PackageLoader::self()->loadPackage(u"Plasma/Shell"_s, u"org.kde.plasma.desktop"_s));
        containment = corona.createContainment(u"null"_s);
        if (panel) {
            containment->setFormFactor(Plasma::Types::Horizontal);
            containment->setLocation(Plasma::Types::BottomEdge);
        }
        window.resize(panel ? 48 : 400, panel ? 48 : 400);
    }
    void load(uint id = 0, const QString &state = { }, const QString &legacyId = { })
    {
        applet = Plasma::PluginLoader::self()->loadApplet(u"dev.kaidoom.tabbednotes"_s, id);
        containment->addApplet(applet);
        if (!state.isEmpty()) {
            applet->configScheme()->findItemByName(u"tabState"_s)->setProperty(state);
            applet->configuration()->insert(u"tabState"_s, state);
        }
        if (!legacyId.isEmpty()) {
            applet->configScheme()->findItemByName(u"noteId"_s)->setProperty(legacyId);
            applet->configuration()->insert(u"noteId"_s, legacyId);
        }
        item = PlasmaQuick::AppletQuickItem::itemForApplet(applet);
        if (item) {
            item->setParentItem(window.contentItem());
            item->setSize(window.size());
        }
        window.show();
    }
    ~Harness() { delete applet; }
    QObject *model() const { return item->findChild<QObject *>(u"tabModel"_s); }
    QQuickItem *editor() const { return item->property("mainTextArea").value<QQuickItem *>(); }
    QString id() const { return model()->property("activeId").toString(); }
    int count() const { return model()->property("count").toInt(); }
    QString state() const { return applet->configuration()->value(u"tabState"_s).toString(); }
    QVariant run(const QString &code) const { return evaluate(item, code); }
    void insert(const QString &text) const
    {
        editor()->forceActiveFocus();
        evaluate(editor(), u"insert(length, "_s + quoted(QString(text).toHtmlEscaped().replace(u" "_s, u"&nbsp;"_s)) + u")"_s);
    }
    void select(const QString &id) const { run(u"selectTab("_s + quoted(id) + u")"_s); }
    void reload()
    {
        const auto id = applet->id();
        delete applet;
        applet = nullptr;
        item = nullptr;
        load(id);
    }
};

static QStringList tabOrder(const Harness &h)
{
    QStringList ids;
    const auto tabs = QJsonDocument::fromJson(h.state().toUtf8()).object().value(u"tabs"_s).toArray();
    for (const auto &tab : tabs)
        ids.append(tab.toObject().value(u"id"_s).toString());
    return ids;
}

static QQuickItem *tabTitle(const Harness &h, const QString &id)
{
    for (auto *button : visualItems(h.item->fullRepresentationItem(), u"tabTitleButton"_s)) {
        if (button->parentItem()->property("noteId").toString() == id)
            return button;
    }
    return nullptr;
}

static void dragPointer(QWindow *window, const QPoint &from, const QPoint &to)
{
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
    for (int step = 1; step <= 12; ++step) {
        QTest::mouseMove(window, from + (to - from) * step / 12, 10);
    }
}

class AppletIntegration : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void discoveryAndRichText()
    {
        const auto plugins = Plasma::PluginLoader::self()->listAppletMetaData(QString());
        QVERIFY(std::any_of(plugins.begin(), plugins.end(), [](const auto &p) { return p.pluginId() == u"dev.kaidoom.tabbednotes"; }));
        QVERIFY(std::any_of(plugins.begin(), plugins.end(), [](const auto &p) { return p.pluginId() == u"org.kde.plasma.notes"; }));
        Harness h;
        h.load();
        QVERIFY2(!h.applet->failedToLaunch(), qPrintable(h.applet->launchErrorMessage()));
        qInfo() << "Loaded plugin:" << h.applet->pluginMetaData().fileName();
        QVERIFY(QFile::exists(u":/qt/qml/plasma/applet/dev/kaidoom/tabbednotes/main.qml"_s));
        QVERIFY2(h.model()->property("ready").toBool(), qPrintable(h.model()->property("error").toString()));
        QTRY_VERIFY(h.editor());
        h.editor()->forceActiveFocus();
        h.editor()->setProperty("text", u"<p><b>Baseline Ω 日本語</b> <a href=\"https://kde.org\">KDE</a></p>"_s);
        QVERIFY(h.run(u"flushEditors()"_s).toBool());
        const auto saved = stored(h.id());
        QVERIFY(saved.contains(u"日本語"));
        QVERIFY(saved.contains(u"https://kde.org"));
        QVERIFY(saved.contains(u"font-weight:700"));
        QVERIFY(!QFile::exists(qEnvironmentVariable("XDG_DATA_HOME") + u"/plasma_notes"_s));
        QTest::qWait(250);
        if (const auto path = qEnvironmentVariable("TABBEDNOTES_SCREENSHOT"); !path.isEmpty())
            QVERIFY(h.window.grabWindow().save(path));
    }

    void rapidSwitchingAndUndo()
    {
        Harness h;
        h.load();
        QTRY_VERIFY(h.editor());
        const auto first = h.id();
        h.insert(u"Alpha Ω"_s);
        QPointer<QQuickItem> firstEditor = h.editor();
        QVERIFY(firstEditor->property("canUndo").toBool());
        h.run(u"addTab()"_s);
        QTRY_VERIFY(h.editor() && h.editor() != firstEditor);
        const auto second = h.id();
        h.insert(u"Beta 日本語"_s);
        QPointer<QQuickItem> secondEditor = h.editor();
        for (int i = 0; i < 20; ++i) {
            h.select(first);
            QCOMPARE(h.editor(), firstEditor.data());
            h.insert(QString::number(i));
            h.select(second);
            QCOMPARE(h.editor(), secondEditor.data());
        }
        const auto alpha = plain(firstEditor->property("text").toString());
        evaluate(secondEditor, u"undo()"_s);
        QCOMPARE(plain(firstEditor->property("text").toString()), alpha);
        QVERIFY(plain(secondEditor->property("text").toString()).isEmpty());
        evaluate(secondEditor, u"redo()"_s);
        QCOMPARE(plain(secondEditor->property("text").toString()), u"Beta 日本語"_s);
        h.select(first);
        evaluate(firstEditor, u"undo()"_s);
        QCOMPARE(plain(secondEditor->property("text").toString()), u"Beta 日本語"_s);
        h.select(second);
        QTest::qWait(1200); // Includes any outgoing editor's deferred save.
        QCOMPARE(plain(stored(first)), plain(firstEditor->property("text").toString()));
        QCOMPARE(plain(stored(second)), u"Beta 日本語"_s);
        h.editor()->forceActiveFocus();
        QTest::keyClick(&h.window, Qt::Key_Tab, Qt::ControlModifier);
        QTRY_COMPARE(h.id(), first);
        QTest::keyClick(&h.window, Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_COMPARE(h.id(), second);
    }

    void lazyRestoreCloseAndRecovery()
    {
        Harness h;
        h.load();
        QTRY_VERIFY(h.editor());
        const auto first = h.id();
        h.insert(u"unvisited after restart"_s);
        QVERIFY(h.run(u"tabModel.renameTab(tabModel.activeId, 'First Ω')"_s).toBool());
        h.run(u"addTab()"_s);
        QTRY_COMPARE(h.count(), 2);
        const auto second = h.id();
        h.insert(u"active note"_s);
        QVERIFY(h.run(u"flushEditors()"_s).toBool());
        h.reload();
        QTRY_VERIFY(h.editor());
        QCOMPARE(h.id(), second);
        QCOMPARE(h.count(), 2);
        QCOMPARE(visualItems(h.item->fullRepresentationItem(), u"noteTextArea"_s).size(), 1);
        QVERIFY(h.run(u"tabModel.hasText("_s + quoted(first) + u")"_s).toBool());
        QCOMPARE(visualItems(h.item->fullRepresentationItem(), u"noteTextArea"_s).size(), 1);
        h.run(u"requestClose("_s + quoted(first) + u")"_s);
        QVERIFY(h.run(u"closeDialog.visible"_s).toBool());
        h.run(u"closeDialog.reject()"_s);
        QCOMPARE(h.count(), 2);
        QVERIFY(h.run(u"tabModel.closeTab("_s + quoted(first) + u")"_s).toBool());
        QCOMPARE(h.count(), 1);
        QCOMPARE(plain(stored(first)), u"unvisited after restart"_s);
        QVERIFY(h.run(u"tabModel.recoverNote("_s + quoted(first) + u")"_s).toBool());
        QTRY_COMPARE(h.count(), 2);
        QVERIFY(h.id() != first);
        QCOMPARE(plain(h.editor()->property("text").toString()), u"unvisited after restart"_s);
        const auto recovered = h.id();
        QVERIFY(h.run(u"tabModel.closeTab(tabModel.activeId)"_s).toBool());
        QVERIFY(h.run(u"tabModel.closeTab(tabModel.activeId)"_s).toBool());
        QCOMPARE(h.count(), 1);
        QVERIFY(h.id() != second);
        QVERIFY(!stored(recovered).isEmpty());
    }

    void duplicationIncludesUnvisitedAndPendingEdits()
    {
        Harness original;
        original.load();
        QTRY_VERIFY(original.editor());
        original.insert(u"first"_s);
        original.run(u"addTab()"_s);
        QTRY_COMPARE(original.count(), 2);
        original.insert(u"second"_s);
        original.run(u"flushEditors()"_s);
        original.reload();
        QTRY_VERIFY(original.editor());
        original.insert(u" pending"_s); // No event-loop turn or timer flush before duplication.
        Harness copy;
        copy.load(0, original.state());
        QTRY_VERIFY(copy.editor());
        QCOMPARE(copy.count(), 2);
        const auto oldTabs = QJsonDocument::fromJson(original.state().toUtf8()).object().value(u"tabs"_s).toArray();
        const auto newTabs = QJsonDocument::fromJson(copy.state().toUtf8()).object().value(u"tabs"_s).toArray();
        for (int i = 0; i < oldTabs.size(); ++i) {
            QVERIFY(oldTabs[i].toObject().value(u"id"_s) != newTabs[i].toObject().value(u"id"_s));
        }
        QCOMPARE(plain(copy.editor()->property("text").toString()), u"second pending"_s);
        copy.insert(u" copy only"_s);
        copy.run(u"flushEditors()"_s);
        QVERIFY(!plain(original.editor()->property("text").toString()).contains(u"copy only"));
        const QString id = newTabs[0].toObject().value(u"id"_s).toString();
        QCOMPARE(plain(stored(id)), u"first"_s);
    }

    void narrowOverflowAndPanel()
    {
        Harness h;
        h.load();
        QTRY_VERIFY(h.editor());
        for (int i = 0; i < 30; ++i)
            h.run(u"addTab()"_s);
        QCOMPARE(h.count(), 31);
        h.window.resize(150, 230);
        h.item->setSize({ 150, 230 });
        QTest::qWait(100);
        QVERIFY(h.editor()->width() > 0);
        QVERIFY(h.editor()->height() > 0);
        auto *list = h.item->fullRepresentationItem()->findChild<QQuickItem *>(u"tabList"_s);
        QVERIFY(list);
        QVERIFY(list->property("contentWidth").toReal() > list->width());
        QVERIFY(list->property("contentX").toReal() > 0);
        Harness panel(true);
        panel.load();
        QTRY_VERIFY(panel.item->compactRepresentationItem());
        panel.item->setExpanded(true);
        QTRY_VERIFY(panel.editor());
        QTRY_VERIFY(panel.item->fullRepresentationItem()->window());
        panel.insert(u"panel note"_s);
        panel.run(u"addTab()"_s);
        panel.insert(u"second panel note"_s);
        panel.run(u"flushEditors()"_s);
        QCOMPARE(plain(stored(panel.id())), u"second panel note"_s);
        panel.item->setExpanded(false);
        panel.item->setExpanded(true);
        QCOMPARE(plain(panel.editor()->property("text").toString()), u"second panel note"_s);
    }

    void renamePasteAndViewState()
    {
        ClipboardRestore clipboard;
        Harness h;
        h.load();
        QTRY_VERIFY(h.editor());
        QTest::qWait(100);
        auto titles = visualItems(h.item->fullRepresentationItem(), u"tabTitleButton"_s);
        QVERIFY(!titles.isEmpty());
        const auto point = titles.first()->mapToScene(QPointF(12, 12)).toPoint();
        QTest::mouseDClick(&h.window, Qt::LeftButton, Qt::NoModifier, point);
        auto fields = visualItems(h.item->fullRepresentationItem(), u"tabRenameField"_s);
        QVERIFY(!fields.isEmpty());
        QTRY_VERIFY(fields.first()->isVisible());
        fields.first()->setProperty("text", u"Renamed Ω 日本語"_s);
        QTest::keyClick(&h.window, Qt::Key_Return);
        QTRY_VERIFY(!fields.first()->isVisible());
        const auto first = h.id();
        auto tabs = QJsonDocument::fromJson(h.state().toUtf8()).object().value(u"tabs"_s).toArray();
        QCOMPARE(tabs[0].toObject().value(u"title"_s).toString(), u"Renamed Ω 日本語"_s);
        auto *mime = new QMimeData;
        mime->setHtml(u"<p><b>Formatted Ω</b> <a href=\"https://kde.org\">link</a></p>"_s);
        mime->setText(u"Formatted Ω link"_s);
        QApplication::clipboard()->setMimeData(mime);
        h.editor()->forceActiveFocus();
        evaluate(h.editor(), u"paste()"_s);
        QVERIFY(h.editor()->property("text").toString().contains(u"font-weight:700"));
        QVERIFY(h.editor()->property("text").toString().contains(u"https://kde.org"));
        h.editor()->forceActiveFocus();
        QCOMPARE(h.window.activeFocusItem(), h.editor());
        // Keep setup and dispatch in one event-loop turn. Synthetic QtTest keys
        // otherwise yield to Wayland, which may reject selection ownership when
        // this test window has no compositor-issued input serial.
        QApplication::clipboard()->setText(u" <tag> & 日本語\nline two"_s);
        QKeyEvent press(QEvent::KeyPress, Qt::Key_V, Qt::ControlModifier);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_V, Qt::ControlModifier);
        QCoreApplication::sendEvent(&h.window, &press);
        QCoreApplication::sendEvent(&h.window, &release);
        QVERIFY(plain(h.editor()->property("text").toString()).contains(u"<tag> & 日本語"));
        h.editor()->setProperty("cursorPosition", 3);
        h.run(u"addTab()"_s);
        h.select(first);
        QTRY_COMPARE(h.editor()->property("cursorPosition").toInt(), 3);
        h.run(u"flushEditors()"_s);
        h.reload();
        QTRY_VERIFY(h.editor());
        QTRY_COMPARE(h.editor()->property("cursorPosition").toInt(), 3);
        tabs = QJsonDocument::fromJson(h.state().toUtf8()).object().value(u"tabs"_s).toArray();
        QCOMPARE(tabs[0].toObject().value(u"title"_s).toString(), u"Renamed Ω 日本語"_s);
    }

    void failedSaveBlocksSwitchWithoutLosingDraft()
    {
        Harness h;
        h.load();
        QTRY_VERIFY(h.editor());
        const auto first = h.id();
        h.insert(u"saved"_s);
        h.run(u"addTab()"_s);
        const auto second = h.id();
        h.select(first);
        h.insert(u" local draft"_s);
        QFile file(qEnvironmentVariable("XDG_DATA_HOME") + u"/plasma_tabbed_notes/"_s + first);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("outside edit");
        file.close();
        h.select(second);
        QCOMPARE(h.id(), first);
        QCOMPARE(plain(h.editor()->property("text").toString()), u"saved local draft"_s);
        QCOMPARE(stored(first), u"outside edit"_s);
        QVERIFY(h.run(u"activeEditor.note.dirty"_s).toBool());
        QVERIFY(!h.run(u"activeEditor.note.error"_s).toString().isEmpty());
    }

    void originalAppletKeepsSeparateFiles()
    {
        Harness h;
        h.load();
        QTRY_VERIFY(h.editor());
        auto *original = h.containment->createApplet(u"org.kde.plasma.notes"_s);
        auto *originalItem = PlasmaQuick::AppletQuickItem::itemForApplet(original);
        QVERIFY(originalItem && !original->failedToLaunch());
        originalItem->setParentItem(h.window.contentItem());
        originalItem->setSize({ 300, 300 });
        QTRY_VERIFY(originalItem->property("mainTextArea").value<QQuickItem *>());
        auto *editor = originalItem->property("mainTextArea").value<QQuickItem *>();
        editor->forceActiveFocus();
        editor->setProperty("text", u"KDE original"_s);
        evaluate(originalItem, u"note.save(mainTextArea.text)"_s);
        const auto id = original->configuration()->value(u"noteId"_s).toString();
        QFile file(qEnvironmentVariable("XDG_DATA_HOME") + u"/plasma_notes/"_s + id);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto before = file.readAll();
        file.close();
        originalItem->setVisible(false);
        h.insert(u"custom fork"_s);
        h.run(u"addTab()"_s);
        h.insert(u"other custom tab"_s);
        h.run(u"flushEditors()"_s);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), before);
        QCOMPARE(plain(QString::fromUtf8(before)), u"KDE original"_s);
        delete original;
    }

    void migrateStandaloneBaseline()
    {
        const QString legacy = u"01234567-0123-0123-0123-0123456789"_s;
        QFile file(qEnvironmentVariable("XDG_DATA_HOME") + u"/plasma_tabbed_notes/"_s + legacy);
        QDir().mkpath(QFileInfo(file).absolutePath());
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<b>baseline contents</b>");
        file.close();
        Harness h;
        h.load(0, { }, legacy);
        QTRY_VERIFY(h.editor());
        QVERIFY(h.id() != legacy);
        QCOMPARE(h.id().size(), 36);
        QCOMPARE(h.count(), 1);
        QCOMPARE(plain(h.editor()->property("text").toString()), u"baseline contents"_s);
        QVERIFY(file.exists());
    }

    void reorderingPreservesEditorsAndDrafts()
    {
        Harness h;
        h.load();
        QTRY_VERIFY(h.editor());
        const auto a = h.id();
        h.insert(u"Alpha Ω"_s);
        QPointer<QQuickItem> editorA = h.editor();
        h.run(u"addTab()"_s);
        const auto b = h.id();
        h.insert(u"Beta 日本語"_s);
        QPointer<QQuickItem> editorB = h.editor();
        h.run(u"addTab()"_s);
        const auto c = h.id();
        h.insert(u"Gamma draft"_s);
        QPointer<QQuickItem> editorC = h.editor();
        auto *model = qobject_cast<QAbstractItemModel *>(h.model());
        QAbstractItemModelTester modelTester(model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy resets(model, &QAbstractItemModel::modelReset);
        QSignalSpy moves(model, &QAbstractItemModel::rowsMoved);
        QVERIFY(h.run(u"activeEditor.note.dirty"_s).toBool());
        QVERIFY(h.run(u"tabModel.moveTab("_s + quoted(c) + u", 0)"_s).toBool());
        QCOMPARE(tabOrder(h), (QStringList { c, a, b }));
        QCOMPARE(h.id(), c);
        QCOMPARE(h.editor(), editorC.data());
        QVERIFY(h.run(u"activeEditor.note.dirty"_s).toBool());
        QVERIFY(h.run(u"tabModel.moveTab("_s + quoted(a) + u", 2)"_s).toBool());
        QCOMPARE(tabOrder(h), (QStringList { c, b, a }));
        QVERIFY(h.run(u"tabModel.moveTab("_s + quoted(b) + u", 0)"_s).toBool());
        QCOMPARE(tabOrder(h), (QStringList { b, c, a }));
        QCOMPARE(h.model()->property("activeIndex").toInt(), 1);
        QCOMPARE(moves.count(), 3);
        QCOMPARE(resets.count(), 0);
        QVERIFY(!h.run(u"tabModel.moveTab('missing', 0)"_s).toBool());
        QVERIFY(!h.run(u"tabModel.moveTab("_s + quoted(a) + u", -1)"_s).toBool());
        QVERIFY(!h.run(u"tabModel.moveTab("_s + quoted(a) + u", 3)"_s).toBool());
        QVERIFY(h.run(u"tabModel.moveTab("_s + quoted(a) + u", 2)"_s).toBool());
        QCOMPARE(moves.count(), 3);
        evaluate(editorC, u"undo()"_s);
        QVERIFY(plain(editorC->property("text").toString()).isEmpty());
        QCOMPARE(plain(editorA->property("text").toString()), u"Alpha Ω"_s);
        QCOMPARE(plain(editorB->property("text").toString()), u"Beta 日本語"_s);
        evaluate(editorC, u"redo()"_s);
        h.run(u"navigate(1)"_s);
        QCOMPARE(h.id(), a);
        QCOMPARE(h.editor(), editorA.data());
        h.run(u"navigate(-1)"_s);
        QCOMPARE(h.id(), c);
        QCOMPARE(h.editor(), editorC.data());
        h.run(u"flushEditors()"_s);
        h.reload();
        QTRY_VERIFY(h.editor());
        QCOMPARE(tabOrder(h), (QStringList { b, c, a }));
        QCOMPARE(h.id(), c);
        QCOMPARE(plain(h.editor()->property("text").toString()), u"Gamma draft"_s);
        QCOMPARE(visualItems(h.item->fullRepresentationItem(), u"noteTextArea"_s).size(), 1);
        QVERIFY(h.run(u"tabModel.moveTab("_s + quoted(a) + u", 0)"_s).toBool());
        QCOMPARE(visualItems(h.item->fullRepresentationItem(), u"noteTextArea"_s).size(), 1);
        QCOMPARE(tabOrder(h), (QStringList { a, b, c }));
        QCOMPARE(h.model()->property("activeIndex").toInt(), 2);
        QCOMPARE(plain(stored(a)), u"Alpha Ω"_s);
        QCOMPARE(plain(stored(b)), u"Beta 日本語"_s);
        QCOMPARE(plain(stored(c)), u"Gamma draft"_s);
    }

    void dragReordering_data()
    {
        QTest::addColumn<bool>("panel");
        QTest::newRow("desktop") << false;
        QTest::newRow("panel-popup") << true;
    }

    void dragReordering()
    {
        QFETCH(bool, panel);
        Harness h(panel);
        h.load();
        if (panel)
            h.item->setExpanded(true);
        QTRY_VERIFY(h.editor());
        const auto a = h.id();
        h.insert(u"First"_s);
        h.run(u"addTab()"_s);
        const auto b = h.id();
        h.run(u"addTab()"_s);
        const auto c = h.id();
        h.select(a);
        QPointer<QQuickItem> editor = h.editor();
        QTest::qWait(100);
        auto *window = h.item->fullRepresentationItem()->window();
        QVERIFY(window);
        auto *titleA = tabTitle(h, a);
        auto *titleC = tabTitle(h, c);
        QVERIFY(titleA && titleC);
        const auto from = titleA->mapToScene(QPointF(10, titleA->height() / 2)).toPoint();
        const auto to = titleC->mapToScene(QPointF(titleC->width() - 3, titleC->height() / 2)).toPoint();
        dragPointer(window, from, to);
        const auto markers = visualItems(h.item->fullRepresentationItem(), u"tabInsertionMarker"_s);
        QVERIFY(!markers.isEmpty());
        QVERIFY(markers.first()->isVisible());
        QCOMPARE(tabOrder(h), (QStringList { a, b, c })); // Commit only on release.
        if (const auto path = qEnvironmentVariable("TABBEDNOTES_REORDER_SCREENSHOT"); !panel && !path.isEmpty()) {
            QTest::qWait(50);
            QVERIFY(window->grabWindow().save(path));
        }
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
        QTRY_COMPARE(tabOrder(h), (QStringList { b, c, a }));
        QCOMPARE(h.id(), a);
        QCOMPARE(h.editor(), editor.data());
        QVERIFY(editor->property("canUndo").toBool());
        QVERIFY(!markers.first()->isVisible());

        // Drag an inactive tab left without changing the selected document.
        QTest::qWait(50);
        auto *titleB = tabTitle(h, b);
        titleC = tabTitle(h, c);
        QVERIFY(titleB && titleC);
        const auto backFrom = titleC->mapToScene(QPointF(10, titleC->height() / 2)).toPoint();
        const auto backTo = titleB->mapToScene(QPointF(2, titleB->height() / 2)).toPoint();
        dragPointer(window, backFrom, backTo);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, backTo);
        QTRY_COMPARE(tabOrder(h), (QStringList { c, b, a }));
        QCOMPARE(h.id(), a);
        QCOMPARE(h.editor(), editor.data());
    }

    void dragOverflowAndCancellation()
    {
        Harness h;
        h.load();
        QTRY_VERIFY(h.editor());
        const auto first = h.id();
        for (int i = 0; i < 7; ++i)
            h.run(u"addTab()"_s);
        h.select(first);
        h.window.resize(210, 260);
        h.item->setSize({ 210, 260 });
        QTest::qWait(100);
        auto *view = h.item->fullRepresentationItem()->findChild<QQuickItem *>(u"tabList"_s);
        QVERIFY(view);
        auto *title = tabTitle(h, first);
        QVERIFY(title);
        const auto order = tabOrder(h);
        const auto from = title->mapToScene(QPointF(10, title->height() / 2)).toPoint();
        const auto edge = view->mapToScene(QPointF(view->width() - 2, view->height() / 2)).toPoint();
        dragPointer(&h.window, from, edge);
        const auto markers = visualItems(h.item->fullRepresentationItem(), u"tabInsertionMarker"_s);
        QVERIFY(markers.first()->isVisible());
        QTRY_VERIFY2_WITH_TIMEOUT(view->property("contentX").toReal() - view->property("originX").toReal() > 250,
            qPrintable(evaluate(view, u"JSON.stringify({x:contentX, origin:originX, width:width, total:contentWidth, drag:bar.dragPosition, id:bar.draggedId, insertion:bar.insertionIndex})"_s).toString()), 3000);
        // The controller must survive virtualization of its original title delegate.
        QTRY_VERIFY_WITH_TIMEOUT(view->property("contentX").toReal() + view->width()
            >= view->property("originX").toReal() + view->property("contentWidth").toReal() - 2, 5000);
        const auto markerPosition = markers.first()->mapToItem(view, QPointF());
        QVERIFY(markerPosition.x() >= 0 && markerPosition.x() < view->width());
        QCOMPARE(markerPosition.y(), 0.0);
        QTest::mouseRelease(&h.window, Qt::LeftButton, Qt::NoModifier, edge);
        auto expected = order;
        expected.move(0, expected.size() - 1);
        QTRY_COMPARE(tabOrder(h), expected);
        QCOMPARE(h.id(), first);
        QVERIFY(!markers.first()->isVisible());
        QTest::qWait(50);
        title = tabTitle(h, first);
        QVERIFY(title);
        const auto right = title->mapToScene(QPointF(10, title->height() / 2)).toPoint();
        const auto left = view->mapToScene(QPointF(2, view->height() / 2)).toPoint();
        dragPointer(&h.window, right, left);
        QVERIFY(markers.first()->isVisible());
        QTest::keyClick(&h.window, Qt::Key_Escape);
        QVERIFY(!markers.first()->isVisible());
        QTest::mouseRelease(&h.window, Qt::LeftButton, Qt::NoModifier, left);
        QCOMPARE(tabOrder(h), expected);
        // Row moves can shift ListView.originX. Wheel scrolling must still
        // reach both ends instead of clamping to the old zero-based range.
        const QPointF wheelPosition = view->mapToScene(QPointF(view->width() / 2, view->height() / 2));
        QWheelEvent wheelRight(wheelPosition, h.window.mapToGlobal(wheelPosition.toPoint()), { }, QPoint(0, -1200), Qt::NoButton,
            Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&h.window, &wheelRight);
        QCOMPARE(view->property("contentX").toReal(),
            view->property("originX").toReal() + view->property("contentWidth").toReal() - view->width());
        QWheelEvent wheelLeft(wheelPosition, h.window.mapToGlobal(wheelPosition.toPoint()), { }, QPoint(0, 1200), Qt::NoButton,
            Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&h.window, &wheelLeft);
        QCOMPARE(view->property("contentX").toReal(), view->property("originX").toReal());
        // Dropping outside the strip also leaves the order unchanged.
        evaluate(view, u"positionViewAtIndex(0, ListView.Beginning)"_s);
        QTest::qWait(50);
        title = tabTitle(h, expected.first());
        QVERIFY(title);
        const auto start = title->mapToScene(QPointF(10, title->height() / 2)).toPoint();
        const auto outside = view->mapToScene(QPointF(view->width() / 2, view->height() + 40)).toPoint();
        dragPointer(&h.window, start, outside);
        QVERIFY(!markers.first()->isVisible());
        QTest::mouseRelease(&h.window, Qt::LeftButton, Qt::NoModifier, outside);
        QCOMPARE(tabOrder(h), expected);
    }

    void malformedStateIsPreserved()
    {
        Harness h;
        h.load(0, u"{not valid JSON"_s);
        QVERIFY(!h.model()->property("ready").toBool());
        QCOMPARE(h.state(), u"{not valid JSON"_s);
        QVERIFY(!h.model()->property("error").toString().isEmpty());
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir sandbox;
    if (!sandbox.isValid())
        return 1;
    qputenv("XDG_CONFIG_HOME", (sandbox.path() + u"/config"_s).toUtf8());
    qputenv("XDG_DATA_HOME", (sandbox.path() + u"/data"_s).toUtf8());
    qputenv("XDG_CACHE_HOME", (sandbox.path() + u"/cache"_s).toUtf8());
    QDir().mkpath(sandbox.path() + u"/data"_s);
    QApplication app(argc, argv);
    AppletIntegration test;
    return QTest::qExec(&test, argc, argv);
}
#include "appletintegration.moc"
