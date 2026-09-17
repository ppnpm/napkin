#include "GuiFixture.h"
#include "../src/media/BlobGc.h"
#include "../src/media/ClipboardContent.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QMimeData>
#include <QtTest>

using namespace napkin;

namespace {

QByteArray makePng(int w, int h, QColor colour = Qt::red)
{
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(colour);
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return out;
}

QByteArray makeJpeg(int w, int h)
{
    QImage image(w, h, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPEG", 90);
    return out;
}

}  // namespace

class TestImages : public QObject {
    Q_OBJECT
private slots:
    // --- clipboard preference order (SPEC.md §4) -----------------------------
    void pngWinsWhenBothAnImageAndTextAreOffered()
    {
        // Exactly what a browser's "Copy Image" puts on the clipboard.
        QMimeData mime;
        mime.setData(QStringLiteral("image/png"), makePng(20, 10));
        mime.setText(QStringLiteral("https://example.com/cat.png"));

        const auto content = readClipboard(&mime);
        QCOMPARE(content.kind, ClipboardContent::Kind::Image);
        QCOMPARE(content.imageMime, QStringLiteral("image/png"));
        QCOMPARE(content.via, QStringLiteral("image/png"));
    }

    void textIsUsedOnlyWhenNoImageIsOffered()
    {
        QMimeData mime;
        mime.setText(QStringLiteral("systemctl restart nginx"));
        const auto content = readClipboard(&mime);
        QCOMPARE(content.kind, ClipboardContent::Kind::Text);
        QCOMPARE(content.text, QStringLiteral("systemctl restart nginx"));
    }

    void anEmptyClipboardIsIgnoredQuietly()
    {
        QMimeData mime;
        QCOMPARE(readClipboard(&mime).kind, ClipboardContent::Kind::None);
        QCOMPARE(readClipboard(nullptr).kind, ClipboardContent::Kind::None);
    }

    // --- blob store ----------------------------------------------------------
    void storingIsContentAddressedSoDuplicatesCostNothing()
    {
        QTemporaryDir dir;
        BlobStore store(dir.path());
        const QByteArray png = makePng(64, 48);

        const auto first = store.store(png);
        const auto second = store.store(png);

        QVERIFY(first.ok);
        QVERIFY(second.ok);
        QCOMPARE(first.hash, second.hash);
        QCOMPARE(first.size, QSize(64, 48));
        QCOMPARE(store.allStoredFiles().size(), size_t(1));  // one copy on disk
    }

    void jpegIsKeptVerbatimRatherThanInflatedIntoPng()
    {
        QTemporaryDir dir;
        BlobStore store(dir.path());
        const QByteArray jpeg = makeJpeg(200, 150);

        const auto stored = store.store(jpeg);
        QVERIFY(stored.ok);
        QCOMPARE(stored.mime, QStringLiteral("image/jpeg"));
        QCOMPARE(stored.byteSize, qint64(jpeg.size()));      // byte for byte
        QVERIFY(store.pathFor(stored.hash, stored.mime).endsWith(QStringLiteral(".jpg")));
    }

    void nonImageBytesAreRefusedWithAReadableMessage()
    {
        QTemporaryDir dir;
        BlobStore store(dir.path());
        const auto stored = store.store(QByteArray("this is not an image at all"));

        QVERIFY(!stored.ok);
        QVERIFY(!stored.error.isEmpty());
        QVERIFY(!stored.error.contains(QStringLiteral("QImage")));  // no internals leak
        QCOMPARE(store.allStoredFiles().size(), size_t(0));
    }

    void oversizedImagesAreRefused()
    {
        QTemporaryDir dir;
        BlobStore store(dir.path());
        const auto stored = store.store(QByteArray(BlobStore::kMaxBytes + 1, 'x'));
        QVERIFY(!stored.ok);
        QVERIFY(stored.error.contains(QStringLiteral("MB")));
    }

    void noTemporaryFileSurvivesASuccessfulWrite()
    {
        QTemporaryDir dir;
        BlobStore store(dir.path());
        store.store(makePng(10, 10));
        for (const auto& name : store.allStoredFiles())
            QVERIFY(!name.endsWith(QStringLiteral(".tmp")));
    }

    // --- garbage collection --------------------------------------------------
    void orphanBlobsAreReclaimedAndReferencedOnesAreNot()
    {
        GuiFixture f;
        const auto kept = f.blobs.store(makePng(30, 30, Qt::green));
        const auto orphan = f.blobs.store(makePng(40, 40, Qt::yellow));
        QVERIFY(kept.ok && orphan.ok);

        const auto id = f.buffers.create();
        f.service.appendTo(id, Item::makeImage(kept.hash, 30, 30, kept.byteSize, {}, kept.mime));

        const auto result = reconcileBlobs(f.items, f.blobs);

        QCOMPARE(result.orphansRemoved, 1);
        QVERIFY(result.missingBlobs.empty());
        QVERIFY(f.blobs.exists(kept.hash, kept.mime));
        QVERIFY(!f.blobs.exists(orphan.hash, orphan.mime));
    }

    void interruptedWritesAreCleanedUp()
    {
        GuiFixture f;
        QDir().mkpath(f.blobs.rootDir() + "/ab");
        QFile stray(f.blobs.rootDir() + "/ab/abcdef.png.tmp");
        QVERIFY(stray.open(QIODevice::WriteOnly));
        stray.write("half a write");
        stray.close();

        const auto result = reconcileBlobs(f.items, f.blobs);
        QCOMPARE(result.temporariesRemoved, 1);
        QVERIFY(!QFile::exists(f.blobs.rootDir() + "/ab/abcdef.png.tmp"));
    }

    void aMissingBlobIsReportedRatherThanIgnored()
    {
        GuiFixture f;
        const auto id = f.buffers.create();
        f.service.appendTo(id, Item::makeImage(QStringLiteral("deadbeef"), 10, 10, 100));

        const auto result = reconcileBlobs(f.items, f.blobs);
        QCOMPARE(result.missingBlobs.size(), size_t(1));
        QCOMPARE(result.missingBlobs.front(), QStringLiteral("deadbeef"));
    }

    // --- through the UI ------------------------------------------------------
    void pastingAnImageOntoTheStackMakesABufferWithAThumbnail()
    {
        GuiFixture f;
        QMimeData* mime = new QMimeData;
        mime->setData(QStringLiteral("image/png"), makePng(120, 90));
        QApplication::clipboard()->setMimeData(mime);

        f.trigger("pasteAction");

        QCOMPARE(f.buffers.countLive(), 1);
        const auto id = f.buffers.listLive(10).front().id;
        const auto items = f.items.listForBuffer(id);
        QCOMPARE(items.size(), size_t(1));
        QCOMPARE(items[0].type, ItemType::Image);
        QCOMPARE(items[0].width, 120);
        QCOMPARE(items[0].height, 90);
        QCOMPARE(items[0].mime, QStringLiteral("image/png"));

        // And the card can actually draw it.
        QVERIFY(!f.thumbs.forBlob(items[0].blobHash, items[0].mime).isNull());
        const auto row = f.model()->rowForId(id);
        QCOMPARE(f.model()->index(row, 0).data(BufferListModel::ThumbHashRole).toString(),
                 items[0].blobHash);
    }

    void pastingAnImageIntoAnOpenDraftPromotesItRatherThanMakingASecondBuffer()
    {
        GuiFixture f;
        f.trigger("newBufferAction");
        QCOMPARE(f.buffers.countLive(), 0);

        f.view()->expandRow(0, QString());
        QMetaObject::invokeMethod(f.view(), "imagePasted", Qt::DirectConnection,
                                  Q_ARG(QByteArray, makePng(64, 64)),
                                  Q_ARG(QString, QStringLiteral("image/png")));

        QCOMPARE(f.buffers.countLive(), 1);
        QCOMPARE(f.model()->rowCount(), 1);
    }

    void aBufferKeepsTextAndImageTogether()
    {
        GuiFixture f;
        f.trigger("newBufferAction");
        QTest::keyClicks(f.editor(), "Investigate this bug");
        QTRY_COMPARE_WITH_TIMEOUT(f.buffers.countLive(), 1, 2000);

        QMetaObject::invokeMethod(f.view(), "imagePasted", Qt::DirectConnection,
                                  Q_ARG(QByteArray, makePng(80, 60)),
                                  Q_ARG(QString, QStringLiteral("image/png")));

        const auto id = f.buffers.listLive(10).front().id;
        const auto items = f.items.listForBuffer(id);
        QCOMPARE(items.size(), size_t(2));
        QCOMPARE(items[0].type, ItemType::Text);
        QCOMPARE(items[1].type, ItemType::Image);
        QCOMPARE(items[0].position, 0);
        QCOMPARE(items[1].position, 1);

        // The preview reports the count, and still offers a thumbnail.
        const auto row = f.model()->rowForId(id);
        QCOMPARE(f.model()->index(row, 0).data(BufferListModel::SecondaryRole).toString(),
                 QStringLiteral("2 items"));
        QVERIFY(!f.model()->index(row, 0).data(BufferListModel::ThumbHashRole).toString().isEmpty());
    }

    // --- empty trash ---------------------------------------------------------
    void emptyingTheTrashReclaimsTheBlobsItHeld()
    {
        GuiFixture f;
        const auto stored = f.blobs.store(makePng(50, 50));
        const auto id = f.buffers.create();
        f.service.appendTo(id, Item::makeImage(stored.hash, 50, 50, stored.byteSize, {}, stored.mime));
        f.model()->reload();

        f.service.trash(id);
        QVERIFY(f.blobs.exists(stored.hash, stored.mime));  // trash is not deletion

        QCOMPARE(f.service.emptyTrash(), 1);
        reconcileBlobs(f.items, f.blobs);

        QCOMPARE(f.buffers.countTrash(), 0);
        QVERIFY(!f.blobs.exists(stored.hash, stored.mime));
    }

    void emptyingTheTrashLeavesLiveBuffersAlone()
    {
        GuiFixture f;
        const auto live = f.seed("still here");
        const auto doomed = f.seed("not for long");
        f.service.trash(doomed);

        QCOMPARE(f.service.emptyTrash(), 1);
        QCOMPARE(f.buffers.countLive(), 1);
        QVERIFY(f.buffers.find(live).has_value());
        QVERIFY(!f.buffers.find(doomed).has_value());
    }

    void emptyingTheTrashCannotTakeAKeptBuffer()
    {
        GuiFixture f;
        const auto id = f.buffers.create();
        f.buffers.setKept(id, true);
        // Force it into the trash with the keep intact, simulating a bug.
        f.db.exec(QString("UPDATE buffers SET deleted_at = 1 WHERE id = %1").arg(id)
                      .toUtf8().constData());

        QCOMPARE(f.service.emptyTrash(), 0);   // skipped, not destroyed
        QVERIFY(f.buffers.find(id).has_value());
    }
};

QTEST_MAIN(TestImages)
#include "test_images.moc"
