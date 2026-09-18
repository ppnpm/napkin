#include "LinkChip.h"
#include "../domain/Links.h"
#include "Icons.h"
#include "Tokens.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QFontMetrics>
#include <QPushButton>
#include <QResizeEvent>
#include <QUrl>
#include <QVBoxLayout>

namespace napkin {
namespace {
using namespace tokens;
constexpr int kPad = 10;
constexpr int kGlyph = 18;
}  // namespace

LinkChip::LinkChip(QWidget* parent) : QWidget(parent)
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(kPad + kGlyph + 10, kPad, kPad, kPad);
    row->setSpacing(10);

    auto* column = new QVBoxLayout;
    column->setSpacing(1);
    host_ = new QLabel;
    host_->setFont(scaled(font(), 0.5, QFont::DemiBold));
    rest_ = new QLabel;
    rest_->setFont(scaled(font(), -1.0));
    column->addWidget(host_);
    column->addWidget(rest_);
    row->addLayout(column, 1);

    open_ = new QPushButton(tr("Open"));
    open_->setCursor(Qt::PointingHandCursor);
    open_->setObjectName(QStringLiteral("linkChipOpen"));
    open_->setFocusPolicy(Qt::TabFocus);
    connect(open_, &QPushButton::clicked, this, [this] { emit openRequested(url_); });
    row->addWidget(open_, 0, Qt::AlignVCenter);

    applyPalette();
}

void LinkChip::setUrl(const QString& url)
{
    url_ = url;
    const QString host = links::hostOf(url);
    host_->setText(host);

    // The path, not the whole URL again: the host is already above it, and
    // repeating it wastes the only line that distinguishes two links to the
    // same site. Elision happens against the width the label actually gets,
    // not against a character count guessed here.
    const QUrl parsed(url, QUrl::StrictMode);
    path_ = parsed.path();
    if (parsed.hasQuery()) path_ += u'?' + parsed.query();
    if (path_ == QLatin1String("/")) path_.clear();
    rest_->setVisible(!path_.isEmpty());
    elidePath();

    // Screen readers get the address in full; the visual chip is a summary and
    // a summary is not enough to decide whether to follow a link.
    setAccessibleName(tr("Link to %1").arg(host));
    setAccessibleDescription(url);
    open_->setAccessibleName(tr("Open %1 in your browser").arg(host));
    setToolTip(url);
}

void LinkChip::elidePath()
{
    const int available = std::max(40, rest_->width());
    rest_->setText(QFontMetrics(rest_->font()).elidedText(path_, Qt::ElideMiddle, available));
}

void LinkChip::resizeEvent(QResizeEvent* e)
{
    elidePath();
    QWidget::resizeEvent(e);
}

int LinkChip::preferredHeight()
{
    return 60;
}

void LinkChip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPalette pal = palette();
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(text(pal, kHairline), 1.0));
    p.setBrush(text(pal, isLightTheme(pal) ? 10 : 16));
    p.drawRoundedRect(box, kRadiusCard, kRadiusCard);

    const QRect glyph(kPad, (height() - kGlyph) / 2, kGlyph, kGlyph);
    icons::drawLink(&p, glyph, text(pal, kTextSecondary));
}

void LinkChip::applyPalette()
{
    const QPalette pal = palette();
    auto tint = [&](QLabel* label, int alpha) {
        QPalette p = label->palette();
        p.setColor(label->foregroundRole(), text(pal, alpha));
        label->setPalette(p);
    };
    tint(host_, kTextPrimary);
    tint(rest_, kTextTertiary);
}

void LinkChip::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::PaletteChange || e->type() == QEvent::ApplicationPaletteChange)
        applyPalette();
    QWidget::changeEvent(e);
}

}  // namespace napkin
