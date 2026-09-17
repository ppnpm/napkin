#include "Lightbox.h"

#include <QGuiApplication>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QMovie>
#include <QScreen>
#include <QVBoxLayout>

namespace napkin {

Lightbox::Lightbox(const QString& imagePath, bool animated, QString caption, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(caption.isEmpty() ? tr("Image") : caption);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    view_ = new QLabel;
    view_->setAlignment(Qt::AlignCenter);
    view_->setMinimumSize(320, 240);
    layout->addWidget(view_);

    QSize natural;
    if (animated) {
        // QMovie decodes frame by frame, so a long animation never sits in
        // memory whole.
        movie_ = new QMovie(imagePath, QByteArray(), this);
        movie_->setCacheMode(QMovie::CacheNone);
        natural = movie_->frameRect().size();
        if (!natural.isValid()) {
            QImageReader reader(imagePath);
            natural = reader.size();
        }
        view_->setMovie(movie_);
        movie_->start();
    } else {
        QImageReader reader(imagePath);
        reader.setAutoTransform(true);
        source_ = QPixmap::fromImage(reader.read());
        natural = source_.size();
    }

    QSize target = natural.isValid() ? natural : QSize(640, 480);
    if (const auto* screen = QGuiApplication::primaryScreen())
        target = target.boundedTo(screen->availableGeometry().size() * 0.85);
    resize(target.expandedTo(QSize(480, 360)));
    rescale();
}

void Lightbox::rescale()
{
    if (movie_) {
        // Never upscale: a blurry enlargement is worse than honest small.
        QSize frame = movie_->frameRect().size();
        if (!frame.isValid()) return;
        movie_->setScaledSize(frame.scaled(view_->size(), Qt::KeepAspectRatio).boundedTo(frame));
        return;
    }
    if (source_.isNull()) return;
    const QSize target =
        source_.size().scaled(view_->size(), Qt::KeepAspectRatio).boundedTo(source_.size());
    view_->setPixmap(source_.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void Lightbox::resizeEvent(QResizeEvent* e)
{
    QDialog::resizeEvent(e);
    rescale();
}

void Lightbox::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) { accept(); return; }
    QDialog::keyPressEvent(e);
}

}  // namespace napkin
