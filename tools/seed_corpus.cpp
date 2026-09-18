// Generates a realistic corpus so Napkin can be run and measured under load.
//
// Never touches the real profile: it writes to whatever --data-dir names, and
// the application is pointed at the same place with XDG_DATA_HOME.
//
//   napkin_seed --data-dir /tmp/napkin-load --images 500 --texts 500
//   XDG_DATA_HOME=/tmp/napkin-load/share XDG_CONFIG_HOME=/tmp/napkin-load/config ./napkin

#include "data/BufferRepository.h"
#include "data/Database.h"
#include "data/ItemRepository.h"
#include "app/Paths.h"
#include "data/Statement.h"
#include "domain/BufferService.h"
#include "media/BlobStore.h"

#include <QBuffer>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QProcess>
#include <QRandomGenerator>
#include <QTemporaryDir>

#include <cstdio>

using namespace napkin;

namespace {

QRandomGenerator* rng = nullptr;

int pick(int lo, int hi) { return lo + int(rng->bounded(hi - lo + 1)); }

const char* kWords[] = {
    "invoice", "deploy", "staging", "postgres", "migration", "rollback", "token",
    "kubernetes", "latency", "throughput", "regression", "changelog", "release",
    "hostname", "certificate", "renewal", "backup", "restore", "snapshot",
    "dashboard", "threshold", "quarterly", "onboarding", "retrospective",
    "bandwidth", "checksum", "payload", "endpoint", "throttle", "quota",
};
constexpr int kWordCount = int(sizeof(kWords) / sizeof(kWords[0]));

QString words(int n)
{
    QStringList out;
    for (int i = 0; i < n; ++i) out << QString::fromLatin1(kWords[pick(0, kWordCount - 1)]);
    return out.join(QChar(' '));
}

// --- text ------------------------------------------------------------------
// The shapes people actually paste: a one-line command, an address, a stack
// trace, a paragraph of notes. Length spread matters more than realism of
// content, because it is what drives card measurement and the 420px clamp.
QString makeText(int index)
{
    switch (index % 6) {
    case 0:
        return QStringLiteral("ssh deploy@%1.internal -p %2")
            .arg(words(1)).arg(pick(2000, 9000));
    case 1:
        return QStringLiteral("%1 %2\n%3 Street\n%4 %5")
            .arg(pick(1, 99)).arg(words(1)).arg(words(1)).arg(words(1)).arg(pick(10000, 99999));
    case 2:
        return QStringLiteral("https://%1.example.org/%2/%3?ref=%4")
            .arg(words(1), words(1), words(1)).arg(pick(1000, 9999));
    case 3: {
        QString s = QStringLiteral("Traceback (most recent call last):\n");
        for (int i = 0, n = pick(4, 12); i < n; ++i)
            s += QStringLiteral("  File \"%1.py\", line %2, in %3\n    %4\n")
                     .arg(words(1)).arg(pick(1, 900)).arg(words(1), words(4));
        return s + QStringLiteral("RuntimeError: %1").arg(words(5));
    }
    case 4: {
        QString s;
        for (int i = 0, n = pick(3, 9); i < n; ++i)
            s += QStringLiteral("- %1\n").arg(words(pick(4, 14)));
        return s;
    }
    default: {
        // The long one: past the 420px clamp, so it exercises clipping.
        QString s;
        for (int i = 0, n = pick(6, 20); i < n; ++i)
            s += words(pick(12, 30)) + QStringLiteral(".\n\n");
        return s;
    }
    }
}

// --- images ----------------------------------------------------------------
// Drawn, not noise: a solid-colour rectangle compresses to nothing and would
// make the blob store look far cheaper than it is. These land in the range a
// real screenshot does.
QImage drawSyntheticScreenshot(int w, int h, int seed)
{
    QImage img(w, h, QImage::Format_RGB32);
    QLinearGradient bg(0, 0, w, h);
    bg.setColorAt(0, QColor::fromHsv((seed * 37) % 360, 40, 250));
    bg.setColorAt(1, QColor::fromHsv((seed * 73) % 360, 60, 200));
    QPainter p(&img);
    p.fillRect(img.rect(), bg);
    p.setRenderHint(QPainter::Antialiasing);

    // Window chrome, panels, and lines standing in for text.
    p.fillRect(0, 0, w, h / 18, QColor(60, 63, 68));
    for (int i = 0, n = pick(6, 18); i < n; ++i) {
        const int x = pick(0, w - 60), y = pick(h / 18, h - 40);
        const int rw = pick(40, w / 2), rh = pick(20, h / 4);
        p.fillRect(x, y, rw, rh, QColor::fromHsv(pick(0, 359), pick(30, 180), pick(120, 250), 180));
    }
    p.setPen(QColor(30, 30, 30, 200));
    for (int y = h / 12; y < h - 10; y += pick(9, 22))
        p.drawLine(pick(8, 40), y, pick(w / 3, w - 20), y);

    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPointSize(std::max(8, h / 40));
    p.setFont(f);
    p.drawText(QRect(10, 0, w - 20, h / 18), Qt::AlignVCenter,
               QStringLiteral("%1 — %2").arg(words(2)).arg(seed));
    p.end();
    return img;
}

QByteArray encode(const QImage& img, const char* format, int quality)
{
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, format, quality);
    return out;
}

QByteArray makeSvg(int seed)
{
    QString s = QStringLiteral("<svg xmlns='http://www.w3.org/2000/svg' width='%1' height='%2'>")
                    .arg(pick(240, 900)).arg(pick(180, 700));
    for (int i = 0, n = pick(5, 20); i < n; ++i)
        s += QStringLiteral("<circle cx='%1' cy='%2' r='%3' fill='hsl(%4,70%%,60%%)'/>")
                 .arg(pick(0, 800)).arg(pick(0, 600)).arg(pick(10, 90)).arg(pick(0, 359));
    s += QStringLiteral("<text x='10' y='30' font-size='20'>%1 %2</text></svg>")
             .arg(words(2)).arg(seed);
    return s.toUtf8();
}

// ffmpeg is the only way to write a GIF in this Qt build — the plugin reads
// them but does not write them. Optional: without it the corpus simply has no
// animated items, which is said plainly rather than silently skipped.
bool haveFfmpeg()
{
    QProcess p;
    p.start(QStringLiteral("ffmpeg"), {QStringLiteral("-version")});
    return p.waitForFinished(5000) && p.exitCode() == 0;
}

QByteArray makeAnimatedGif(int seed)
{
    QTemporaryDir dir;
    if (!dir.isValid()) return {};
    const int w = pick(160, 480), h = pick(120, 360);
    for (int i = 0; i < 8; ++i) {
        QImage frame(w, h, QImage::Format_RGB32);
        frame.fill(QColor::fromHsv((seed * 23 + i * 45) % 360, 180, 230));
        QPainter p(&frame);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor::fromHsv((seed * 91 + i * 30) % 360, 220, 200));
        p.drawEllipse(QPoint(w / 8 + i * (w * 3 / 4) / 8, h / 2), h / 5, h / 5);
        p.setPen(Qt::black);
        p.drawText(frame.rect(), Qt::AlignBottom | Qt::AlignHCenter,
                   QStringLiteral("%1 %2/8").arg(seed).arg(i + 1));
        p.end();
        frame.save(dir.filePath(QStringLiteral("f%1.png").arg(i, 3, 10, QChar('0'))));
    }
    const QString out = dir.filePath(QStringLiteral("out.gif"));
    QProcess ff;
    ff.start(QStringLiteral("ffmpeg"),
             {QStringLiteral("-y"), QStringLiteral("-loglevel"), QStringLiteral("error"),
              QStringLiteral("-framerate"), QStringLiteral("8"),
              QStringLiteral("-i"), dir.filePath(QStringLiteral("f%03d.png")),
              QStringLiteral("-loop"), QStringLiteral("0"), out});
    if (!ff.waitForFinished(30000) || ff.exitCode() != 0) return {};
    QFile f(out);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

}  // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);   // QPainter and the image plugins need this
    // The same identity the application uses, because that is what decides
    // where QStandardPaths puts the profile.
    QCoreApplication::setApplicationName(QStringLiteral("napkin"));
    QCoreApplication::setOrganizationName(QStringLiteral("napkin"));

    QCommandLineParser parser;
    parser.setApplicationDescription("Seeds a Napkin profile with a load-test corpus.");
    parser.addHelpOption();
    QCommandLineOption dataOpt({"d", "data-dir"}, "Profile directory to write.", "dir");
    QCommandLineOption imgOpt({"i", "images"}, "Image items.", "n", "500");
    QCommandLineOption txtOpt({"t", "texts"}, "Text items.", "n", "500");
    QCommandLineOption shapeOpt({"s", "shape"}, "spread | single", "shape", "spread");
    QCommandLineOption seedOpt("seed", "RNG seed.", "n", "1");
    parser.addOptions({dataOpt, imgOpt, txtOpt, shapeOpt, seedOpt});
    parser.process(app);

    if (!parser.isSet(dataOpt)) {
        fprintf(stderr, "--data-dir is required; it must not be your real profile\n");
        return 2;
    }
    const QString root = parser.value(dataOpt);
    const int nImages = parser.value(imgOpt).toInt();
    const int nTexts = parser.value(txtOpt).toInt();
    const bool single = parser.value(shapeOpt) == QLatin1String("single");

    QRandomGenerator gen(parser.value(seedOpt).toUInt());
    rng = &gen;

    // Resolve the location through the application's own code rather than
    // reconstructing it. Guessing it as <XDG_DATA_HOME>/napkin was wrong —
    // AppDataLocation is <XDG_DATA_HOME>/<organization>/<application>, so the
    // corpus landed one directory above where the application then looked, and
    // the first benchmark run happily measured an empty database.
    qputenv("XDG_DATA_HOME", (root + QStringLiteral("/share")).toUtf8());
    qputenv("XDG_CONFIG_HOME", (root + QStringLiteral("/config")).toUtf8());
    paths::ensureDirs();
    const QString dataDir = paths::dataDir();

    Database db;
    db.open(paths::databaseFile());
    BufferRepository buffers(db);
    ItemRepository items(db);
    BufferService service(db, buffers, items);
    BlobStore blobs(paths::blobsDir());

    const bool gif = haveFfmpeg();
    printf("seeding %d images + %d texts into %s (%s)%s\n", nImages, nTexts,
           qPrintable(dataDir), single ? "one buffer" : "spread across buffers",
           gif ? "" : "  [no ffmpeg: no animated GIFs]");
    fflush(stdout);

    // A realistic spread: most buffers hold one or two things, a few hold a
    // screenshot set. One deliberately large buffer exercises the board.
    QList<int> plan;
    if (single) {
        plan << nImages + nTexts;
    } else {
        int left = nImages + nTexts;
        plan << std::min(left, 120);          // the pathological one
        left -= plan.last();
        while (left > 0) {
            const int n = std::min(left, pick(1, 100) <= 70 ? pick(1, 2) : pick(3, 14));
            plan << n;
            left -= n;
        }
    }

    // Interleaved so buffers are mixed rather than all-image then all-text.
    QList<bool> kinds;
    for (int i = 0; i < nImages; ++i) kinds << true;
    for (int i = 0; i < nTexts; ++i) kinds << false;
    for (int i = kinds.size() - 1; i > 0; --i) kinds.swapItemsAt(i, int(gen.bounded(i + 1)));

    QElapsedTimer total;
    total.start();
    qint64 imageBytes = 0, encodeMs = 0, storeMs = 0;
    int made = 0, animated = 0, failed = 0, cursor = 0;

    const Timestamp now = QDateTime::currentMSecsSinceEpoch();
    for (int b = 0; b < plan.size() && cursor < kinds.size(); ++b) {
        const BufferId id = buffers.create();
        for (int k = 0; k < plan[b] && cursor < kinds.size(); ++k, ++cursor) {
            if (!kinds[cursor]) {
                service.appendTo(id, Item::makeText(makeText(cursor)));
                continue;
            }

            QElapsedTimer t;
            t.start();
            QByteArray bytes;
            QString mime;
            const int roll = pick(1, 100);
            if (gif && roll <= 5) {
                bytes = makeAnimatedGif(cursor);
                mime = QStringLiteral("image/gif");
            } else if (roll <= 10) {
                bytes = makeSvg(cursor);
                mime = QStringLiteral("image/svg+xml");
            } else {
                static const QSize kSizes[] = {{1920, 1080}, {1920, 1080}, {2560, 1440},
                                               {1280, 800},  {800, 600},   {640, 480},
                                               {400, 300},   {220, 220}};
                const QSize s = kSizes[pick(0, 7)];
                const QImage img = drawSyntheticScreenshot(s.width(), s.height(), cursor);
                if (roll <= 40) { bytes = encode(img, "jpeg", 85); mime = QStringLiteral("image/jpeg"); }
                else if (roll <= 48) { bytes = encode(img, "webp", 80); mime = QStringLiteral("image/webp"); }
                else { bytes = encode(img, "png", -1); mime = QStringLiteral("image/png"); }
            }
            encodeMs += t.elapsed();
            if (bytes.isEmpty()) { ++failed; continue; }

            t.restart();
            const auto stored = blobs.store(bytes, mime);
            storeMs += t.elapsed();
            if (!stored.ok) { ++failed; fprintf(stderr, "  %s\n", qPrintable(stored.error)); continue; }

            service.appendTo(id, Item::makeImage(stored.hash, stored.size.width(),
                                                 stored.size.height(), stored.byteSize,
                                                 QStringLiteral("shot-%1").arg(cursor),
                                                 stored.mime, stored.animated));
            imageBytes += stored.byteSize;
            if (stored.animated) ++animated;
            ++made;
        }
        // Spread over the last 90 days so the OLDER section and the sweep have
        // something real to act on. Written directly because touch() means
        // "now" by design and there is no back-dating API — nor should there be
        // one outside a tool like this.
        Statement age(db, "UPDATE buffers SET created_at=?, modified_at=? WHERE id=?");
        const Timestamp when = now - qint64(pick(0, 90)) * 24 * 3600 * 1000;
        age.bind(1, when).bind(2, when).bind(3, qint64(id));
        age.exec();
        if ((b % 25) == 0) { printf("\r  %d/%d items", cursor, int(kinds.size())); fflush(stdout); }
    }

    printf("\r  %d items in %lld buffers\n", cursor, qint64(plan.size()));
    printf("  images stored : %d (%d animated, %d failed)\n", made, animated, failed);
    printf("  image bytes   : %.1f MB\n", imageBytes / 1048576.0);
    printf("  encode time   : %lld ms   store time: %lld ms\n", encodeMs, storeMs);
    printf("  total         : %lld ms\n", total.elapsed());
    printf("\nRun the application against it with:\n"
           "  XDG_DATA_HOME=%s/share XDG_CONFIG_HOME=%s/config ./build/napkin\n",
           qPrintable(root), qPrintable(root));
    return 0;
}
