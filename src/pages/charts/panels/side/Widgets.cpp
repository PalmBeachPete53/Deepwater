// Charts tab — side panel building blocks (MIT, Copyright (c) 2026 Lewis Jackson — docs/charts/NOTICE.md).
#include "pages/charts/panels/side/Widgets.h"
#include "pages/charts/Icons.h"
#include "pages/charts/Style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QTextLayout>
#include <QVBoxLayout>
#include <cmath>

namespace charts::ui::side {

namespace {
constexpr int kRowH = 31;    // padding 6 + line 18.2 (13 px * 1.4) + 6, rounded like the browser's layout
constexpr int kPadX = 10, kGap = 8, kChgW = 64, kXW = 18;
QFont symFont() { return style::font(13, QFont::DemiBold); }
QFont exFont() { return style::font(11); }
QFont numFont() { return style::monoFont(12); }
}  // namespace

// ============================== WatchRows ==============================

WatchRows::WatchRows(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);
  setCursor(Qt::PointingHandCursor);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void WatchRows::setRows(const QList<Row>& rows) {
  rows_ = rows;
  if (hover_ >= rows_.size()) hover_ = -1;
  updateGeometry();
  update();
}

QSize WatchRows::sizeHint() const {
  if (rows_.isEmpty()) return {300, 10 + 17 + 10};   // .pad.hint-text
  return {300, int(rows_.size()) * kRowH};
}

int WatchRows::rowAt(int y) const {
  const int r = y / kRowH;
  return y >= 0 && r < rows_.size() ? r : -1;
}

QRect WatchRows::xRect(int row) const {
  return QRect(width() - kPadX - kXW, row * kRowH, kXW, kRowH);
}

void WatchRows::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  const auto& v = style::v();
  if (rows_.isEmpty()) {
    p.setFont(style::font(12));
    p.setPen(v.muted);
    p.drawText(QRect(12, 10, width() - 24, height() - 20), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               QCoreApplication::translate("charts", "Empty list. Add a symbol above."));
    return;
  }
  // grid-template-columns: 1fr auto 64px 18px — the auto column is as wide as the widest price.
  const QFontMetrics nfm(numFont());
  int pxW = 0;
  for (const auto& r : rows_) pxW = std::max(pxW, nfm.horizontalAdvance(r.price));
  const int right = width() - kPadX;
  const int xX = right - kXW, chgX = xX - kGap - kChgW, pxX = chgX - kGap - pxW;
  const int symW = std::max(0, pxX - kGap - kPadX);
  const QFontMetrics sfm(symFont()), efm(exFont());
  for (int i = 0; i < rows_.size(); ++i) {
    const auto& r = rows_[i];
    const QRect row(0, i * kRowH, width(), kRowH);
    if (r.current) p.fillRect(row, v.accentSoft);
    else if (i == hover_) p.fillRect(row, v.hover);
    // .sym: bold ticker, then " " + exchange (11 px muted); one line, ellipsis at the end.
    const int base = row.top() + (kRowH + sfm.ascent() - sfm.descent()) / 2;
    p.setFont(symFont());
    p.setPen(v.text);
    const QString t = sfm.elidedText(r.ticker, Qt::ElideRight, symW);
    p.drawText(kPadX, base, t);
    const int tw = sfm.horizontalAdvance(t) + sfm.horizontalAdvance(' ');
    if (t == r.ticker && tw < symW) {
      p.setFont(exFont());
      p.setPen(v.muted);
      p.drawText(kPadX + tw, base, efm.elidedText(r.exchange, Qt::ElideRight, symW - tw));
    }
    p.setFont(numFont());
    p.setPen(v.text);
    p.drawText(QRect(pxX, row.top(), pxW, kRowH), Qt::AlignRight | Qt::AlignVCenter, r.price);
    p.setPen(r.up ? v.up : v.down);
    p.drawText(QRect(chgX, row.top(), kChgW, kRowH), Qt::AlignRight | Qt::AlignVCenter, r.change);
    if (i == hover_) {   // .wl-row:hover .x { opacity: 1 }; 14 px icon in an 18 px column
      const QRect xr = xRect(i);
      icons::paint(p, QRectF(xr.left() + 2, xr.center().y() - 7 + 0.5, 14, 14), icons::kClose,
                   hoverX_ ? v.text : v.muted);
    }
  }
}

void WatchRows::mouseMoveEvent(QMouseEvent* e) {
  const int r = rowAt(int(e->position().y()));
  const bool onX = r >= 0 && xRect(r).contains(e->position().toPoint());
  if (r != hover_ || onX != hoverX_) {
    hover_ = r;
    hoverX_ = onX;
    if (r >= 0) setToolTip(onX ? QCoreApplication::translate("charts", "Remove") : QString());
    update();
  }
}

void WatchRows::leaveEvent(QEvent*) {
  hover_ = -1;
  hoverX_ = false;
  update();
}

void WatchRows::mousePressEvent(QMouseEvent* e) {
  if (e->button() != Qt::LeftButton) return;
  const int r = rowAt(int(e->position().y()));
  if (r < 0) return;
  const QString sym = rows_[r].sym;
  if (xRect(r).contains(e->position().toPoint())) emit removed(sym);
  else emit picked(sym);
}

// ============================== Blocks ==============================

Blocks::Blocks(QWidget* parent) : QWidget(parent) {
  lay_ = new QVBoxLayout(this);
  lay_->setContentsMargins(0, 0, 0, 0);
  lay_->setSpacing(0);
}

void Blocks::add(QWidget* w, int top, int bottom, int hMargin) {
  const int gap = lastBottom_ < 0 ? top : std::max(lastBottom_, top);
  if (gap > 0) lay_->addSpacing(gap);
  if (hMargin > 0) {
    auto* wrap = new QWidget(this);
    auto* h = new QHBoxLayout(wrap);
    h->setContentsMargins(hMargin, 0, hMargin, 0);
    h->setSpacing(0);
    w->setParent(wrap);
    h->addWidget(w);
    lay_->addWidget(wrap);
  } else {
    w->setParent(this);
    lay_->addWidget(w);
  }
  lastBottom_ = bottom;
}

void Blocks::clear() {
  while (auto* it = lay_->takeAt(0)) {
    if (auto* w = it->widget()) w->deleteLater();
    delete it;
  }
  lastBottom_ = -1;
}

void Blocks::finish() {
  if (lastBottom_ > 0) lay_->addSpacing(lastBottom_);
  lay_->addStretch(1);
}

// ============================== Card ==============================

Card::Card(QWidget* parent, bool clickable, bool link) : QFrame(parent), clickable_(clickable) {
  setProperty("qtc", "card");
  if (link) {
    setProperty("link", "1");
    setAttribute(Qt::WA_Hover);
  }
  if (clickable) setCursor(Qt::PointingHandCursor);
  lay_ = new QVBoxLayout(this);
  lay_->setContentsMargins(11, 9, 11, 9);   // 1 px border + padding 8px 10px
  lay_->setSpacing(0);
}

void Card::addTop(QWidget* left, QWidget* right) {
  auto* row = new QWidget(this);
  auto* h = new QHBoxLayout(row);
  h->setContentsMargins(0, 0, 0, 0);
  h->setSpacing(6);   // .card .top { gap: 6px; align-items: start }
  left->setParent(row);
  h->addWidget(left, 1, Qt::AlignTop);
  if (right) {
    right->setParent(row);
    h->addWidget(right, 0, Qt::AlignTop);
  }
  lay_->addWidget(row);
}

QLabel* Card::addLine(const QString& html) {
  auto* l = label(html, 13, {}, this);
  lay_->addWidget(l);
  return l;
}

QLabel* Card::addSub(const QString& html) {
  auto* l = label(html, 11, "muted", this);
  l->setContentsMargins(0, 2, 0, 0);
  lay_->addWidget(l);
  return l;
}

void Card::mouseReleaseEvent(QMouseEvent* e) {
  QFrame::mouseReleaseEvent(e);
  if (clickable_ && e->button() == Qt::LeftButton && rect().contains(e->position().toPoint())) emit clicked();
}

QLabel* label(const QString& html, int px, const QString& qtc, QWidget* parent) {
  auto* l = new QLabel(html, parent);
  l->setTextFormat(Qt::RichText);
  l->setWordWrap(true);
  l->setFont(style::font(px));
  if (!qtc.isEmpty()) l->setProperty("qtc", qtc);
  l->setAttribute(Qt::WA_TransparentForMouseEvents);   // clicks go to the card
  return l;
}

// ============================== ClampLabel ==============================

ClampLabel::ClampLabel(const QString& text, int lines, QWidget* parent) : QWidget(parent), text_(text), lines_(lines) {
  setFont(style::font(11));
  QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
  sp.setHeightForWidth(true);
  setSizePolicy(sp);
  setAttribute(Qt::WA_TransparentForMouseEvents);
}

QStringList ClampLabel::layoutLines(int w) const {
  QStringList out;
  if (w <= 0) return out;
  QTextLayout tl(text_, font());
  QTextOption opt;
  opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  tl.setTextOption(opt);
  tl.beginLayout();
  QList<QPair<int, int>> spans;
  for (;;) {
    QTextLine line = tl.createLine();
    if (!line.isValid()) break;
    line.setLineWidth(w);
    spans.append({line.textStart(), line.textLength()});
  }
  tl.endLayout();
  const QFontMetrics fm(font());
  for (int i = 0; i < spans.size() && i < lines_; ++i) {
    QString s = text_.mid(spans[i].first, spans[i].second);
    if (i == lines_ - 1 && spans.size() > lines_)
      s = fm.elidedText(text_.mid(spans[i].first).simplified(), Qt::ElideRight, w);
    out << s.trimmed();
  }
  return out;
}

int ClampLabel::heightForWidth(int w) const {
  const QFontMetrics fm(font());
  const int n = int(layoutLines(w).size());
  return n * int(std::ceil(font().pixelSize() * 1.4));
}

QSize ClampLabel::sizeHint() const { return {200, heightForWidth(width() > 0 ? width() : 260)}; }

void ClampLabel::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setFont(font());
  p.setPen(style::colorOf(color_));
  const int lh = int(std::ceil(font().pixelSize() * 1.4));
  int y = 0;
  for (const auto& s : layoutLines(width())) {
    p.drawText(QRect(0, y, width(), lh), Qt::AlignLeft | Qt::AlignVCenter, s);
    y += lh;
  }
}

}  // namespace charts::ui::side
