// Napkin — Phase 0 spike.
//
// Disposable. Its only job is to retire the three risks that could invalidate
// the design before Phase 1 commits to it:
//
//   1. Clipboard image paste on Wayland/KDE, from Spectacle *and* from a
//      browser, honouring the fixed format-preference order in SPEC.md §4.
//   2. The blob write ordering of invariant 6: fsync the file, fsync the
//      directory, atomic rename, and only then commit the database row.
//   3. SQLite in WAL mode surviving kill -9 (SPEC.md §8, acceptance 10/11).
//
// Nothing here is meant to survive into Phase 1 except the conclusions.

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include <sqlite3.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>

namespace {

QString dataDir()
{
    const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(d);
    QFile::setPermissions(d, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return d;
}

// Invariant 6: the blob is durable on disk before any row can reference it.
// A crash at any point here leaves at worst an orphan blob, never a dangling
// reference. Returns the sha256 hex, or an empty string on failure.
QString writeBlob(const QByteArray& png, QString* err)
{
    const QString hash =
        QString::fromLatin1(QCryptographicHash::hash(png, QCryptographicHash::Sha256).toHex());
    const QString dir = dataDir() + "/blobs/" + hash.left(2);
    QDir().mkpath(dir);

    const QString finalPath = dir + "/" + hash + ".png";
    if (QFile::exists(finalPath))
        return hash;  // content-addressed: identical paste dedupes for free

    const QString tmpPath = finalPath + ".tmp";
    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly)) { *err = "open temp: " + f.errorString(); return {}; }
    if (f.write(png) != png.size())   { *err = "short write: " + f.errorString(); return {}; }
    if (!f.flush())                   { *err = "flush: " + f.errorString(); return {}; }
    if (::fsync(f.handle()) != 0)     { *err = "fsync file"; return {}; }
    f.close();

    if (!QFile::rename(tmpPath, finalPath)) { *err = "rename failed"; return {}; }

    // Rename durability needs the containing directory fsynced too.
    if (int dfd = ::open(dir.toLocal8Bit().constData(), O_RDONLY | O_DIRECTORY); dfd >= 0) {
        ::fsync(dfd);
        ::close(dfd);
    }
    return hash;
}

class Db {
public:
    bool open(QString* err)
    {
        const QString path = dataDir() + "/spike.db";
        if (sqlite3_open(path.toUtf8().constData(), &db_) != SQLITE_OK) {
            *err = QString::fromUtf8(sqlite3_errmsg(db_));
            return false;
        }
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);

        // SPEC.md §8: WAL + synchronous=NORMAL means we lose data only on OS or
        // power loss, never on application crash or kill -9.
        for (const char* p : {"PRAGMA journal_mode=WAL;",
                              "PRAGMA synchronous=NORMAL;",
                              "PRAGMA foreign_keys=ON;"})
            exec(p, err);

        return exec("CREATE TABLE IF NOT EXISTS spike_items("
                    "  id INTEGER PRIMARY KEY,"
                    "  created_at INTEGER NOT NULL,"
                    "  kind TEXT NOT NULL CHECK(kind IN ('text','image')),"
                    "  text TEXT, blob_hash TEXT,"
                    "  width INTEGER, height INTEGER, byte_size INTEGER,"
                    "  source TEXT);", err);
    }

    bool exec(const char* sql, QString* err)
    {
        char* msg = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &msg) != SQLITE_OK) {
            *err = QString::fromUtf8(msg ? msg : "unknown");
            sqlite3_free(msg);
            return false;
        }
        return true;
    }

    bool insertText(const QString& text, QString* err)
    {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO spike_items(created_at,kind,text,source) VALUES(?,'text',?,?)",
            -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, QDateTime::currentMSecsSinceEpoch());
        const QByteArray t = text.toUtf8();
        sqlite3_bind_text(st, 2, t.constData(), t.size(), SQLITE_TRANSIENT);
        sqlite3_bind_text(st, 3, "typed", -1, SQLITE_STATIC);
        const bool ok = sqlite3_step(st) == SQLITE_DONE;
        if (!ok) *err = QString::fromUtf8(sqlite3_errmsg(db_));
        sqlite3_finalize(st);
        return ok;
    }

    bool insertImage(const QString& hash, const QSize& sz, qint64 bytes,
                     const QString& source, QString* err)
    {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_,
            "INSERT INTO spike_items(created_at,kind,blob_hash,width,height,byte_size,source)"
            " VALUES(?,'image',?,?,?,?,?)", -1, &st, nullptr);
        sqlite3_bind_int64(st, 1, QDateTime::currentMSecsSinceEpoch());
        const QByteArray h = hash.toUtf8(), s = source.toUtf8();
        sqlite3_bind_text(st, 2, h.constData(), h.size(), SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 3, sz.width());
        sqlite3_bind_int(st, 4, sz.height());
        sqlite3_bind_int64(st, 5, bytes);
        sqlite3_bind_text(st, 6, s.constData(), s.size(), SQLITE_TRANSIENT);
        const bool ok = sqlite3_step(st) == SQLITE_DONE;
        if (!ok) *err = QString::fromUtf8(sqlite3_errmsg(db_));
        sqlite3_finalize(st);
        return ok;
    }

    int count(const char* kind)
    {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM spike_items WHERE kind=?", -1, &st, nullptr);
        sqlite3_bind_text(st, 1, kind, -1, SQLITE_STATIC);
        int n = 0;
        if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
        return n;
    }

    QString lastImageHash()
    {
        sqlite3_stmt* st = nullptr;
        sqlite3_prepare_v2(db_,
            "SELECT blob_hash FROM spike_items WHERE kind='image' ORDER BY id DESC LIMIT 1",
            -1, &st, nullptr);
        QString h;
        if (sqlite3_step(st) == SQLITE_ROW)
            h = QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(st, 0)));
        sqlite3_finalize(st);
        return h;
    }

private:
    sqlite3* db_ = nullptr;
};

class Spike : public QMainWindow {
public:
    Spike()
    {
        auto* central = new QWidget;
        auto* v = new QVBoxLayout(central);

        info_ = new QLabel;
        info_->setWordWrap(true);
        info_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(info_);

        editor_ = new QPlainTextEdit;
        editor_->setPlaceholderText(
            "Type here, then press Ctrl+V with a screenshot on the clipboard.\n\n"
            "Try: Spectacle (Print) -> Copy to clipboard -> Ctrl+V\n"
            "Try: right-click any image in a browser -> Copy Image -> Ctrl+V");
        v->addWidget(editor_, 1);

        auto* imgScroll = new QScrollArea;
        image_ = new QLabel("no image pasted yet");
        image_->setAlignment(Qt::AlignCenter);
        image_->setMinimumHeight(220);
        imgScroll->setWidget(image_);
        imgScroll->setWidgetResizable(true);
        v->addWidget(imgScroll, 1);

        log_ = new QPlainTextEdit;
        log_->setReadOnly(true);
        log_->setMaximumHeight(150);
        v->addWidget(log_);

        auto* row = new QHBoxLayout;
        auto* saveBtn = new QPushButton("Save text (Ctrl+S)");
        auto* crashBtn = new QPushButton("Simulate crash (SIGKILL)");
        crashBtn->setToolTip("Hard-kills the process. Relaunch to verify WAL durability.");
        row->addWidget(saveBtn);
        row->addWidget(crashBtn);
        row->addStretch();
        v->addLayout(row);

        setCentralWidget(central);
        setWindowTitle("Napkin — Phase 0 spike");
        resize(880, 900);

        QString err;
        if (!db_.open(&err)) {
            log("FATAL: could not open database: " + err);
        } else {
            log("db opened (WAL, synchronous=NORMAL)");
        }
        refreshInfo();
        restoreLastImage();

        auto* paste = new QShortcut(QKeySequence::Paste, this);
        paste->setContext(Qt::ApplicationShortcut);
        connect(paste, &QShortcut::activated, this, &Spike::onPaste);

        connect(saveBtn, &QPushButton::clicked, this, &Spike::onSaveText);
        auto* save = new QShortcut(QKeySequence::Save, this);
        connect(save, &QShortcut::activated, this, &Spike::onSaveText);

        connect(crashBtn, &QPushButton::clicked, this, [] { ::raise(SIGKILL); });
    }

private:
    void log(const QString& s)
    {
        log_->appendPlainText(QDateTime::currentDateTime().toString("hh:mm:ss ") + s);
    }

    void refreshInfo()
    {
        info_->setText(QString("<b>data dir:</b> %1<br>"
                               "<b>persisted:</b> %2 text · %3 image "
                               "(these survived the last restart)")
                           .arg(dataDir())
                           .arg(db_.count("text"))
                           .arg(db_.count("image")));
    }

    void restoreLastImage()
    {
        const QString h = db_.lastImageHash();
        if (h.isEmpty()) return;
        const QString p = dataDir() + "/blobs/" + h.left(2) + "/" + h + ".png";
        QImage img(p);
        if (img.isNull()) {
            log("blob missing for " + h.left(12) + " — would render as broken placeholder");
            return;
        }
        showImage(img, "restored from disk");
    }

    void showImage(const QImage& img, const QString& note)
    {
        image_->setPixmap(QPixmap::fromImage(
            img.scaled(760, 420, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        image_->setToolTip(note);
    }

    // SPEC.md §4 — the format preference order is a contract, not a heuristic.
    //   image/png  ->  any other image/*  ->  text/plain  ->  ignored
    void onPaste()
    {
        const QMimeData* md = QGuiApplication::clipboard()->mimeData();
        if (!md) { log("clipboard: empty"); return; }

        log("clipboard offers: " + md->formats().join(", "));

        QByteArray png;
        QString source;

        if (md->hasFormat("image/png")) {
            png = md->data("image/png");
            source = "image/png (direct)";
        } else if (md->hasImage()) {
            const QImage img = qvariant_cast<QImage>(md->imageData());
            if (!img.isNull()) {
                QBuffer buf(&png);
                buf.open(QIODevice::WriteOnly);
                img.save(&buf, "PNG");
                source = "image/* (transcoded to PNG)";
            }
        }

        if (!png.isEmpty()) {
            QImage img;
            if (!img.loadFromData(png, "PNG")) { log("ERROR: clipboard PNG did not decode"); return; }

            QString err;
            const QString hash = writeBlob(png, &err);
            if (hash.isEmpty()) { log("ERROR: blob write failed: " + err); return; }

            if (!db_.insertImage(hash, img.size(), png.size(), source, &err)) {
                log("ERROR: row insert failed (blob is an orphan, GC will reclaim): " + err);
                return;
            }
            showImage(img, source);
            log(QString("image via %1 — %2x%3, %4 KB, sha256 %5…")
                    .arg(source).arg(img.width()).arg(img.height())
                    .arg(png.size() / 1024).arg(hash.left(12)));
            refreshInfo();
            return;
        }

        if (md->hasText()) {
            editor_->insertPlainText(md->text());
            log(QString("text — %1 chars (image was not offered)").arg(md->text().size()));
            return;
        }

        log("ignored: no image or text representation available");
    }

    void onSaveText()
    {
        const QString t = editor_->toPlainText();
        if (t.isEmpty()) { log("nothing to save (a draft with no content writes no row)"); return; }
        QString err;
        if (!db_.insertText(t, &err)) { log("ERROR: " + err); return; }
        log(QString("text committed — %1 chars").arg(t.size()));
        refreshInfo();
    }

    Db db_;
    QLabel* info_;
    QLabel* image_;
    QPlainTextEdit* editor_;
    QPlainTextEdit* log_;
};

}  // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("napkin");
    Spike w;
    w.show();
    return app.exec();
}
