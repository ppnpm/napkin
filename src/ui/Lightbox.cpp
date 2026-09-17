#include "Lightbox.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QScreen>
#include <QVBoxLayout>

namespace napkin {

Lightbox::Lightbox(const QPixmap& image, QString caption, QWidget* parent)
    : QDialog(parent), source_(image)
{
    setWindowTitle(caption.isEmpty() ? tr("Image") : caption);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    view_ = new QLabel;
    view_->setAlignment(Qt::AlignCenter);
    view_->setMinimumSize(320, 240);
    layout->addWidget(view_);

    // Open large but never larger than the screen, and never upscale a small
    // image past its own resolution.
    QSize target = source_.size();
    if (const auto* screen = QGuiApplication::primaryScreen()) {
        const QSize available = screen->availableGeometry().size() * 0.85;
        target = target.boundedTo(available);
    }
    resize(target.expandedTo(QSize(480, 360)));
    rescale();
}

void Lightbox::rescale()
{
    if (source_.isNull()) return;
    // Never upscale: a blurry enlargement is worse than honest small.
    const QSize target = source_.size().scaled(view_->size(), Qt::KeepAspectRatio)
                             .boundedTo(source_.size());
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
