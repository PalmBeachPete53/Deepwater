// Port of the legend of quant-terminal public/js/chart.js (MIT, Copyright (c) 2026 Lewis Jackson).
#include "pages/charts/pane/Legend.h"
#include "pages/charts/Drawings.h"
#include "pages/charts/Style.h"
#include <QFontMetricsF>
#include <QPainter>
#include <QTextLayout>

namespace charts::ui::pane {

namespace {
constexpr double kLine = 20;      // .legend .row line-height
constexpr double kGap = 8;        // .legend .row gap
constexpr double kBtnW = 18;      // .acts button: 14 px svg + padding 0 2px
constexpr double kBtnGap = 2;     // .acts gap
bool padded(const Legend::Row& r) { return r.kind == Legend::Row::Ind || r.kind == Legend::Row::Compare; }

int noteLines(const QString& text, const QFont& f, double width, QVector<QTextLine>* out = nullptr,
              QTextLayout* keep = nullptr) {
  QTextLayout local(text, f);
  QTextLayout& lay = keep ? *keep : local;
  if (keep) { lay.setText(text); lay.setFont(f); }
  QTextOption o;
  o.setWrapMode(QTextOption::WordWrap);
  lay.setTextOption(o);
  lay.beginLayout();
  int n = 0;
  while (true) {
    QTextLine l = lay.createLine();
    if (!l.isValid()) break;
    l.setLineWidth(width);
    if (out) out->append(l);
    ++n;
  }
  lay.endLayout();
  return std::max(1, n);
}
}  // namespace

Legend::Legend(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
}

void Legend::setRows(QVector<Row> rows) {
  rows_ = std::move(rows);
  if (hover_ >= rows_.size()) hover_ = -1;
  relayout();
}

void Legend::setMaxWidth(double w) {
  if (std::abs(w - maxW_) < 0.5) return;
  maxW_ = w;
  relayout();
}

void Legend::setHover(int row, const QString& act) {
  if (row == hover_ && act == hoverAct_) return;
  hover_ = row;
  hoverAct_ = act;
  relayout();
}

void Legend::relayout() {
  double natural = 0;
  for (int i = 0; i < rows_.size(); ++i) {
    Row& r = rows_[i];
    if (r.kind == Row::Note) {
      const QFont f = style::font(11);
      natural = std::max(natural, std::min(420.0, QFontMetricsF(f).horizontalAdvance(r.note) + 1));
      continue;
    }
    double x = padded(r) ? 2 : 0;
    bool first = true;
    for (Cell& c : r.cells) {
      if (c.acts && hover_ != i) continue;
      if (!first) x += kGap;
      first = false;
      for (int k = 0; k < c.runs.size(); ++k) {
        Run& run = c.runs[k];
        run.w = run.icon.isEmpty() ? QFontMetricsF(run.font).horizontalAdvance(run.text) : kBtnW;
        x += run.w + run.marginRight + (c.acts && k + 1 < c.runs.size() ? kBtnGap : 0);
      }
    }
    if (padded(r)) x += 4;
    natural = std::max(natural, x);
  }
  const double W = std::max(0.0, std::min(maxW_, std::ceil(natural)));
  double y = 0;
  for (Row& r : rows_) {
    double h = kLine;
    if (r.kind == Row::Note) h = kLine * noteLines(r.note, style::font(11), std::min(420.0, W));
    r.rect = QRectF(0, y, W, h);
    y += h;
  }
  resize(int(std::ceil(W)), int(std::ceil(y)));
  update();
}

Legend::Hit Legend::hit(const QPointF& p) const {
  Hit h;
  for (int i = 0; i < rows_.size(); ++i) {
    const Row& r = rows_[i];
    if (!r.rect.contains(p)) continue;
    h.row = i;
    if (r.kind == Row::Note) { h.interactive = true; return h; }
    if (r.kind != Row::Title) h.interactive = true;   // .legend .ind { pointer-events: auto }
    double x = padded(r) ? 2 : 0;
    bool first = true;
    for (const Cell& c : r.cells) {
      if (c.acts && hover_ != i) continue;
      if (!first) x += kGap;
      first = false;
      for (int k = 0; k < c.runs.size(); ++k) {
        const Run& run = c.runs[k];
        if (!run.act.isEmpty() && p.x() >= x && p.x() < x + run.w) {
          h.act = run.act;
          h.interactive = true;
        }
        x += run.w + run.marginRight + (c.acts && k + 1 < c.runs.size() ? kBtnGap : 0);
      }
    }
    return h;
  }
  return h;
}

void Legend::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  const auto& v = style::v();
  for (int i = 0; i < rows_.size(); ++i) {
    const Row& r = rows_[i];
    p.save();
    p.setClipRect(r.rect);
    p.setOpacity(r.opacity);
    if (r.kind == Row::Note) {
      const QFont f = style::font(11);
      QTextLayout lay;
      QVector<QTextLine> lines;
      noteLines(r.note, f, std::min(420.0, r.rect.width()), &lines, &lay);
      p.setPen(r.noteColor.isValid() ? r.noteColor : v.warn);
      const QFontMetricsF fm(f);
      for (int k = 0; k < lines.size(); ++k) {
        const double top = r.rect.top() + k * kLine + (kLine - fm.height()) / 2;
        lines[k].draw(&p, QPointF(-lines[k].x(), top - lines[k].y()));
      }
      p.restore();
      continue;
    }
    if (padded(r) && hover_ == i) {
      p.setPen(Qt::NoPen);
      p.setBrush(v.panel);
      p.drawRoundedRect(r.rect, 3, 3);
    }
    double x = padded(r) ? 2 : 0;
    bool first = true;
    for (const Cell& c : r.cells) {
      if (c.acts && hover_ != i) continue;
      if (!first) x += kGap;
      first = false;
      for (int k = 0; k < c.runs.size(); ++k) {
        const Run& run = c.runs[k];
        if (!run.icon.isEmpty()) {
          const bool hot = hover_ == i && hoverAct_ == run.act;
          const QPixmap pm = renderIcon(run.icon, hot ? v.text : v.muted, 14, devicePixelRatioF());
          p.drawPixmap(QPointF(x + 2, r.rect.top() + (kLine - 14) / 2), pm);
        } else {
          const QFontMetricsF fm(run.font);
          p.setFont(run.font);
          p.setPen(run.color.isValid() ? run.color : v.text);
          const double base = r.rect.top() + (kLine - fm.height()) / 2 + fm.ascent();
          p.drawText(QPointF(x, base), run.text);
        }
        x += run.w + run.marginRight + (c.acts && k + 1 < c.runs.size() ? kBtnGap : 0);
      }
    }
    p.restore();
  }
}

}  // namespace charts::ui::pane
