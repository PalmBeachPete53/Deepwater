#include "app/OptionsDialog.h"
#include "core/I18n.h"
#include "ui/Kit.h"
#include "ui/Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QCoreApplication>

namespace {
constexpr qreal kPad = 26;
}

OptionsDialog::OptionsDialog(QWidget* parent) : QDialog(parent) {
  setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setModal(true);
  setMouseTracking(true);
  setFixedSize(sizeHint());
  // the dialog itself follows a language change at once
  connect(&i18n::Notifier::instance(), &i18n::Notifier::changed, this, [this] { update(); });
  connect(&theme::Notifier::instance(), &theme::Notifier::changed, this, [this] { update(); });
}

QList<OptionsDialog::Area> OptionsDialog::areas() const {
  QList<Area> out;
  out << Area{QRectF(width() - 44, 16, 26, 26), Close};
  const qreal y = 126, w = (width() - 2 * kPad - 10) / 2.0;
  out << Area{QRectF(kPad, y, w, 46), LangEn} << Area{QRectF(kPad + w + 10, y, w, 46), LangFr};
  const auto& th = theme::themes();
  const qreal cw = (width() - 2 * kPad - 10) / 2.0, ch = 54, top = 244;
  for (int i = 0; i < th.size(); ++i)
    out << Area{QRectF(kPad + (i % 2) * (cw + 10), top + (i / 2) * (ch + 8), cw, ch), ThemeFirst + i};
  return out;
}
int OptionsDialog::hitAt(const QPointF& p) const {
  for (const auto& a : areas()) if (a.r.contains(p)) return a.id;
  return None;
}

void OptionsDialog::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const auto& c = theme::pal();
  const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
  p.setPen(QPen(c.borderHi, 1)); p.setBrush(theme::map("#0b0804"));
  p.drawRoundedRect(box, 4, 4);
  ui::drawBrackets(p, QRectF(rect()).adjusted(4, 4, -4, -4), c.accent, 12);

  // title row
  p.setPen(Qt::NoPen); p.setBrush(c.accent); p.drawEllipse(QPointF(kPad + 3, 31), 3, 3);
  p.setFont(theme::mono(13, QFont::Bold, 3.0)); p.setPen(c.accentText);
  p.drawText(QPointF(kPad + 14, 36), QCoreApplication::translate("options", "OPTIONS"));
  // close ×
  const QRectF xr = areas().first().r;
  p.setPen(QPen(m_hover == Close ? c.text : c.textDim, 1.6, Qt::SolidLine, Qt::RoundCap));
  p.drawLine(xr.center() + QPointF(-6, -6), xr.center() + QPointF(6, 6));
  p.drawLine(xr.center() + QPointF(6, -6), xr.center() + QPointF(-6, 6));
  p.fillRect(QRectF(kPad, 58, width() - 2 * kPad, 1), c.divider);

  auto section = [&](qreal y, const QString& label, const QString& hint) {
    p.setFont(theme::microLabel()); p.setPen(c.textMute);
    p.drawText(QPointF(kPad, y), label);
    p.setFont(theme::sans(12)); p.setPen(c.textDim);
    p.drawText(QRectF(kPad, y + 6, width() - 2 * kPad, 22), Qt::AlignLeft | Qt::AlignVCenter, hint);
  };

  // language
  section(92, QCoreApplication::translate("options", "LANGUAGE"), QCoreApplication::translate("options", "Interface language. Numbers keep the 1,234.56 format."));
  const bool fr = i18n::current() == i18n::Lang::French;
  for (const auto& a : areas()) {
    if (a.id != LangEn && a.id != LangFr) continue;
    const bool on = (a.id == LangFr) == fr, hov = a.id == m_hover;
    p.setPen(QPen(on ? c.accent : hov ? c.borderHi : c.border, 1));
    p.setBrush(on ? c.accentDim : hov ? c.panelAlt : theme::map("#0e0a05"));
    p.drawRoundedRect(a.r.adjusted(.5, .5, -.5, -.5), 3, 3);
    p.setFont(theme::mono(13, QFont::Bold, 1.5)); p.setPen(on ? c.accent : hov ? c.text : c.textDim);
    p.drawText(a.r.adjusted(16, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, a.id == LangEn ? "ENGLISH" : "FRANÇAIS");
    p.setFont(theme::mono(10, QFont::Medium, 1.5)); p.setPen(on ? theme::alpha(c.accent, 170) : c.textMute);
    p.drawText(a.r.adjusted(0, 0, -16, 0), Qt::AlignVCenter | Qt::AlignRight, a.id == LangEn ? "EN" : "FR");
  }

  // theme
  section(210, QCoreApplication::translate("options", "THEME"), QCoreApplication::translate("options", "Colours of the whole application. Hover a theme for its description."));
  const auto& th = theme::themes();
  const QString cur = theme::currentId();
  for (const auto& a : areas()) {
    if (a.id < ThemeFirst) continue;
    const auto& info = th[a.id - ThemeFirst];
    const auto& r = info.roles;
    const bool on = info.id == cur, hov = a.id == m_hover;
    // each card is drawn in its own theme's colours: a small preview
    p.setPen(QPen(on ? c.accent : hov ? c.borderHi : c.border, on ? 2 : 1));
    p.setBrush(r.background);
    p.drawRoundedRect(a.r.adjusted(.5, .5, -.5, -.5), 3, 3);
    const QColor sw[] = {r.panel, r.accent, r.up, r.down, r.text};
    qreal x = a.r.left() + 12;
    for (const QColor& col : sw) {
      p.setPen(QPen(r.panelLine, 1)); p.setBrush(col);
      p.drawRoundedRect(QRectF(x, a.r.top() + 12, 12, 12), 2, 2);
      x += 15;
    }
    p.setFont(theme::mono(12, QFont::Bold, 1.5)); p.setPen(r.accent);
    p.drawText(QPointF(x + 8, a.r.top() + 23), QCoreApplication::translate("theme", info.name).toUpper());
    p.setFont(theme::mono(9, QFont::Medium, 1.2)); p.setPen(r.textDim);
    const QString kind = info.id == "deepwater" ? QCoreApplication::translate("options", "DEFAULT")
                         : info.dark ? QCoreApplication::translate("options", "DARK") : QCoreApplication::translate("options", "LIGHT");
    p.drawText(QRectF(a.r.left() + 12, a.r.top() + 30, a.r.width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, kind);
    if (on) {   // check mark
      p.setPen(QPen(r.accent, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
      const QPointF o(a.r.right() - 20, a.r.center().y());
      p.drawPolyline(QPolygonF{o + QPointF(-6, 0), o + QPointF(-2, 4), o + QPointF(6, -5)});
    }
  }

  // footer
  p.setFont(theme::mono(10, QFont::Normal, 1.0)); p.setPen(c.textFaint);
  p.drawText(QRectF(kPad, height() - 46, width() - 2 * kPad, 32), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
             QCoreApplication::translate("options", "Changes apply immediately and are saved.  Esc closes."));
}

void OptionsDialog::mouseMoveEvent(QMouseEvent* e) {
  if (m_dragging) { move(pos() + (e->globalPosition() - m_dragFrom).toPoint()); m_dragFrom = e->globalPosition(); return; }
  const int h = hitAt(e->position());
  if (h != m_hover) {
    m_hover = h; setCursor(h == None ? Qt::ArrowCursor : Qt::PointingHandCursor); update();
    setToolTip(h >= ThemeFirst ? QCoreApplication::translate("theme", theme::themes()[h - ThemeFirst].description) : QString());
  }
}
void OptionsDialog::mousePressEvent(QMouseEvent* e) {
  const int h = hitAt(e->position());
  if (h == None && e->position().y() < 58) { m_dragging = true; m_dragFrom = e->globalPosition(); return; }   // drag by the title row
  if (h == Close) { accept(); return; }
  if (h == LangEn) i18n::setLanguage(i18n::Lang::English);
  else if (h == LangFr) i18n::setLanguage(i18n::Lang::French);
  else if (h >= ThemeFirst) theme::setCurrent(theme::themes()[h - ThemeFirst].id);
  update();
}
void OptionsDialog::mouseReleaseEvent(QMouseEvent*) { m_dragging = false; }
void OptionsDialog::leaveEvent(QEvent*) { if (m_hover != None) { m_hover = None; update(); } }
void OptionsDialog::keyPressEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Escape) { accept(); return; }
  QDialog::keyPressEvent(e);
}
