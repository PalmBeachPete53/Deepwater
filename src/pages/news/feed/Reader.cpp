#include "pages/news/feed/Reader.h"
#include "ui/Kit.h"
#include "ui/Touch.h"
#include "pages/news/feed/FeedWidgets.h"
#include "news/Translate.h"
#include "market/QuoteHub.h"
#include "market/Instruments.h"
#include "ui/Theme.h"
#include "pages/news/ThemeColor.h"
#include "core/I18n.h"
#include <QCoreApplication>
#include "ui/Format.h"
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QLineEdit>
#include <QMouseEvent>
#include <QDesktopServices>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextLayout>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QWheelEvent>
#include <QMenu>
#include <QSet>
#include <cmath>

namespace news::pages {
namespace {
QStringList wrapLines(const QString& text, const QFont& f, double width) {
  QStringList out;
  QTextLayout tl(text, f);
  tl.beginLayout();
  for (;;) { QTextLine l = tl.createLine(); if (!l.isValid()) break; l.setLineWidth(width); out << text.mid(l.textStart(), l.textLength()).trimmed(); }
  tl.endLayout();
  return out;
}
const news::pages::Dw kBody{"#e4e3e0"};
}

// ------------------------------------------------------------------------------------------------ article
ArticleWidget::ArticleWidget(FeedState* st, QWidget* parent) : QWidget(parent), m_st(st) {
  setMouseTracking(true);
  m_doc.setDocumentMargin(0);
  m_doc.setDefaultFont(theme::condensed(16, QFont::Medium));
  // translations arrive in batches for every story of the feed: rebuilding the article (and losing the find
  // highlights) is needed only when one of its own texts arrived, and not while the reader is hidden
  connect(st, &FeedState::textChanged, this, [this] {
    if (!m_st->fr) return;
    if (!isVisible()) { m_trDirty = true; return; }
    if (trSignature() != m_trSig) retranslate();
  });
}
void ArticleWidget::showEvent(QShowEvent*) {
  if (!m_trDirty) return;
  m_trDirty = false;
  if (m_st->fr && trSignature() != m_trSig) retranslate();
}
// What the article's text depends on, besides the story: language, translator state, which of its texts are translated
QString ArticleWidget::trSignature() const {
  const auto& tr = news::Translate::instance();
  int n = tr.cached(m_s.title).isEmpty() ? 0 : 1;
  for (const QString& t : m_paras) if (!tr.cached(t).isEmpty()) ++n;
  return QString("%1|%2|%3").arg(m_st->fr).arg(tr.usable()).arg(n);
}

QRectF ArticleWidget::colRect() const {
  const double w = width() < 520 ? width() - 32.0 : std::min(861.0, width() - 80.0);   // phones: 16 px margins
  return QRectF((width() - w) / 2, 0, w, height());
}
QRectF ArticleWidget::chipRect() const { const QRectF c = colRect(); return QRectF(c.right() - 153, 15, 59, 33); }

void ArticleWidget::setStory(const news::Story& s) {
  m_s = s; m_hits.clear(); m_firstMatchY = -1; m_findWord.clear();
  // English paragraphs from what the feed gives
  QTextDocument tmp;
  tmp.setHtml(!s.contentHtml.isEmpty() ? s.contentHtml : "<p>" + s.summary.toHtmlEscaped() + "</p>");
  m_paras.clear(); m_heads.clear();
  for (QTextBlock b = tmp.begin(); b.isValid(); b = b.next()) {
    const QString t = b.text().simplified();
    if (t.isEmpty()) continue;
    m_paras << t;
    const bool shortLine = t.size() < 70 && !t.endsWith('.') && !t.endsWith(':');
    m_heads << (b.blockFormat().headingLevel() > 0 || b.charFormat().fontWeight() >= QFont::Bold || shortLine);
  }
  retranslate();
  if (m_st->fr) news::Translate::instance().request(QStringList{s.title} + m_paras, this, [this] { retranslate(); });
}

QString ArticleWidget::plainText() const { return m_paras.join("\n"); }

void ArticleWidget::retranslate() {
  const auto& tr = news::Translate::instance();
  const bool fr = m_st->fr && tr.usable();
  QString html;
  if (!fr && !m_s.contentHtml.isEmpty()) html = m_s.contentHtml;
  else {
    m_pending = false;
    for (int i = 0; i < m_paras.size(); ++i) {
      QString t = m_paras[i];
      if (fr) { const QString c = tr.cached(t); if (c.isEmpty()) m_pending = true; else t = c; }
      html += (m_heads.value(i) ? "<h3>" : "<p>") + t.toHtmlEscaped() + (m_heads.value(i) ? "</h3>" : "</p>");
    }
  }
  if (!fr) m_pending = false;
  else if (tr.cached(m_s.title).isEmpty()) m_pending = true;
  m_doc.setDefaultStyleSheet(mapQss("a { color: #e8c998; text-decoration: underline; } h2, h3, h4 { color: #ffffff; font-weight: 500; font-size: 16px; }"));
  m_doc.setHtml(html);
  for (QTextBlock b = m_doc.begin(); b.isValid(); b = b.next()) {
    QTextCursor c(b);
    QTextBlockFormat bf = b.blockFormat();
    bf.setLineHeight(26, QTextBlockFormat::FixedHeight);
    bf.setTopMargin(0); bf.setBottomMargin(26);
    const bool head = bf.headingLevel() > 0;
    bf.setHeadingLevel(0);
    c.setBlockFormat(bf);
    if (head) {   // sub-heads: same size and weight as the body, white (reference "Monday, Oct 26")
      QTextCharFormat cf; cf.setFont(theme::condensed(16, QFont::Medium)); cf.setForeground(theme::map("#ffffff"));
      c.movePosition(QTextCursor::StartOfBlock); c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
      c.setCharFormat(cf);
    }
  }
  rebuild();
  m_trSig = trSignature();
  // the new document dropped the highlight cursors: find the word again (no scroll, the reader keeps its place)
  if (!m_findWord.isEmpty()) find(m_findWord);
  emit languageToggled();
}

void ArticleWidget::rebuild() {
  const QRectF col = colRect();
  const QString title = m_st->fr ? m_st->tx(m_s.title) : m_s.title;
  m_titlePx = col.width() < 480 ? 28 : col.width() < 700 ? 38 : 46;   // narrow windows: smaller titles (phones 28)
  m_titleLines = wrapLines(title, theme::sans(m_titlePx, QFont::Bold), col.width());
  double y = 54 + m_titleLines.size() * (m_titlePx + 8);
  y += 8 + 3 + 18;     // gold underline
  m_imgH = m_s.image.isEmpty() ? 0 : std::round(col.width() * 451.0 / 860.0);
  y += m_imgH + (m_imgH > 0 ? 28 : 6);
  m_bodyTop = y;
  m_doc.setTextWidth(col.width());
  y += m_doc.size().height() + 6;
  m_readRect = QRectF(col.x(), y, 260, 36);
  y += 36 + 18;
  if (width() < 520) y += 40;   // phones: the note goes under the button (beside it, it was cut after 3 words a line)
  setFixedHeight(int(y + 40));
  update();
}

void ArticleWidget::resizeEvent(QResizeEvent* e) { if (e->oldSize().width() != width()) rebuild(); }

int ArticleWidget::find(const QString& word) {
  m_findWord = word;
  m_hits.clear(); m_firstMatchY = -1;
  if (word.trimmed().size() >= 2) {
    QTextCursor c(&m_doc);
    while (!(c = m_doc.find(word.trimmed(), c)).isNull()) {
      m_hits << c;
      if (m_firstMatchY < 0) m_firstMatchY = m_bodyTop + m_doc.documentLayout()->blockBoundingRect(c.block()).top();
    }
  }
  update();
  return int(m_hits.size());
}

void ArticleWidget::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), theme::map("#000000"));
  const QRectF col = colRect();
  // meta
  p.setFont(theme::mono(11, QFont::Bold, 0.8)); p.setPen(theme::map("#e8c998"));
  const QFontMetricsF fm(p.font());
  p.drawText(QRectF(col.x(), 17, 300, 18), Qt::AlignLeft | Qt::AlignVCenter, sourceLabel(m_s.source));
  // reference: "INVESTINGLIVE · ADAM BUTTON" (no age); the age shows when the feed gives no author
  QString meta = "·  " + (m_s.author.isEmpty() ? ageUpper(m_s.time) : m_s.author.toUpper());
  p.setFont(theme::mono(11, QFont::Normal, 0.8)); p.setPen(theme::map("#727169"));
  p.drawText(QRectF(col.x() + fm.horizontalAdvance(sourceLabel(m_s.source)) + 8, 17, 500, 18), Qt::AlignLeft | Qt::AlignVCenter, meta);
  // language chip
  const QRectF cr = chipRect();
  const auto& tr = news::Translate::instance();
  p.fillRect(cr, m_hoverChip ? theme::map("#4a3d03") : theme::map("#382f01"));
  p.setPen(theme::map("#5a4a12")); p.drawRect(cr.adjusted(0.5, 0.5, -0.5, -0.5));
  p.setFont(theme::mono(13, QFont::Bold)); p.setPen(theme::map("#f2d37a"));
  const QString lang = m_st->fr && tr.usable() ? (m_pending ? "FR..." : "FR") : "EN";
  p.drawText(cr.adjusted(18, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, lang);
  p.setPen(QPen(theme::map("#f2d37a"), 1));
  const double gx = cr.x() + 9, gy = cr.center().y();
  p.drawLine(QPointF(gx, gy - 3), QPointF(gx + 6, gy - 3)); p.drawLine(QPointF(gx + 1, gy + 3), QPointF(gx + 5, gy - 2));
  p.drawLine(QPointF(gx + 6, gy + 5), QPointF(gx + 8, gy)); p.drawLine(QPointF(gx + 8, gy), QPointF(gx + 10, gy + 5));
  p.setFont(theme::mono(9, QFont::Normal, 0.3)); p.setPen(m_st->fr && !tr.usable() ? theme::map("#8a5a52") : theme::map("#494742"));
  p.drawText(QRectF(cr.right() + 4, cr.y(), col.right() - cr.right() - 4, cr.height()), Qt::AlignLeft | Qt::AlignVCenter,
             m_st->fr && !tr.usable() ? QCoreApplication::translate("news", "FR UNAVAILABLE") : m_st->fr ? QCoreApplication::translate("news", "AUTO-TRANSLATED") : QCoreApplication::translate("news", "ORIGINAL"));
  // title
  p.setFont(theme::sans(m_titlePx, QFont::Bold)); p.setPen(theme::map("#f5f3ee"));
  double y = 54;
  const double tl = m_titlePx + 8;
  for (const auto& l : m_titleLines) { p.drawText(QRectF(col.x(), y, col.width() + 40, tl), Qt::AlignLeft | Qt::AlignVCenter, l); y += tl; }
  y += 8;
  p.fillRect(QRectF(col.x(), y, 55, 3), theme::map("#f1d699"));
  y += 21;
  if (m_imgH > 0) {
    const QRectF ir(col.x(), y, col.width(), m_imgH);
    p.drawPixmap(ir.topLeft(), storyPixmap(m_s, ir.size().toSize(), this));
    p.setPen(theme::map("#2a2a28")); p.drawRect(ir.adjusted(0.5, 0.5, -0.5, -0.5));
  }
  // body
  p.save();
  p.translate(col.x(), m_bodyTop);
  QAbstractTextDocumentLayout::PaintContext ctx;
  ctx.palette.setColor(QPalette::Text, kBody);
  for (const auto& h : m_hits) {
    QAbstractTextDocumentLayout::Selection sel; sel.cursor = h;
    sel.format.setBackground(theme::map("#6b5a10")); sel.format.setForeground(theme::map("#fff4c2"));
    ctx.selections << sel;
  }
  m_doc.documentLayout()->draw(&p, ctx);
  p.restore();
  // read on site
  const QRectF rr = m_readRect;
  p.fillRect(rr, m_hoverRead ? theme::map("#2e2604") : theme::map("#120e03"));
  p.setPen(theme::map("#6b5d1a")); p.drawRect(rr.adjusted(0.5, 0.5, -0.5, -0.5));
  p.setFont(theme::mono(11, QFont::Bold, 1.2)); p.setPen(theme::map("#f3e4a5"));
  const QString host = QUrl(m_s.link).host().remove(QRegularExpression("^www\\."));
  p.drawText(rr, Qt::AlignCenter, QCoreApplication::translate("news", "READ ON %1  ↗").arg(host.toUpper()));
  p.setFont(theme::mono(10)); p.setPen(theme::map("#5b5750"));
  const QRectF nr = width() < 520 ? QRectF(rr.x(), rr.bottom() + 8, col.width(), 40)
                                  : QRectF(rr.right() + 14, rr.y(), col.right() - rr.right() - 14, rr.height());
  p.drawText(nr, (width() < 520 ? Qt::AlignTop : Qt::AlignVCenter) | Qt::AlignLeft | Qt::TextWordWrap,
             m_s.contentHtml.isEmpty() ? QCoreApplication::translate("news", "The feed carries a summary only; the full article opens in your browser.")
                                       : QCoreApplication::translate("news", "Text as published in the source's feed."));
}

void ArticleWidget::mouseMoveEvent(QMouseEvent* e) {
  const QPointF pos = e->position();
  const bool hc = chipRect().contains(pos), hr = m_readRect.contains(pos);
  QString anchor;
  if (pos.y() >= m_bodyTop) anchor = m_doc.documentLayout()->anchorAt(pos - QPointF(colRect().x(), m_bodyTop));
  setCursor(hc || hr || !anchor.isEmpty() ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (hc != m_hoverChip || hr != m_hoverRead) { m_hoverChip = hc; m_hoverRead = hr; update(); }
}
void ArticleWidget::mousePressEvent(QMouseEvent* e) {
  const QPointF pos = e->position();
  if (chipRect().contains(pos)) {
    m_st->fr = !m_st->fr; m_st->save();
    if (m_st->fr) news::Translate::instance().request(QStringList{m_s.title} + m_paras, this, [this] { retranslate(); });
    emit m_st->textChanged();
    retranslate();
    return;
  }
  if (m_readRect.contains(pos)) { ui::openWebLink(m_s.link); return; }
  if (pos.y() >= m_bodyTop) {
    const QString a = m_doc.documentLayout()->anchorAt(pos - QPointF(colRect().x(), m_bodyTop));
    if (!a.isEmpty()) ui::openWebLink(a, m_s.link);   // links inside the story: relative to its page
  }
}

// ------------------------------------------------------------------------------------------------ automated read
AutoReadPanel::AutoReadPanel(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_find = new QLineEdit(this);
  m_find->setPlaceholderText(QCoreApplication::translate("news", "Find in this article..."));
  m_find->setStyleSheet(mapQss("QLineEdit{background:#0a0805;border:1px solid #272215;border-radius:0;padding:0 8px;color:#e8e3d6;"
                        "font-family:'Rajdhani';font-size:14px;font-weight:500;} QLineEdit:focus{border-color:#5e4f1c;}"));
  QPalette pl = m_find->palette(); pl.setColor(QPalette::PlaceholderText, theme::map("#413c33")); m_find->setPalette(pl);
  m_orb.setInterval(83);   // 12 fps, only while visible
  connect(&m_orb, &QTimer::timeout, this, [this] { m_phase += 0.05; const int ob = orbBoxH(); update(QRect(width() / 2 - ob / 2, 34, ob, ob - 9)); });  // the halo specks only
  connect(&market::QuoteHub::instance(), &market::QuoteHub::updated, this, [this] { if (isVisible()) update(); });
}
void AutoReadPanel::showEvent(QShowEvent*) { m_orb.start(); market::QuoteHub::instance().setActive(this, true); }
void AutoReadPanel::hideEvent(QHideEvent*) { m_orb.stop(); market::QuoteHub::instance().setActive(this, false); }
void AutoReadPanel::resizeEvent(QResizeEvent*) { m_find->setGeometry(7, height() - 34, width() - 46, 27); }

void AutoReadPanel::setFrench(bool french) {
  if (french == m_french) return;
  m_french = french;
  if (french) news::Translate::instance().request(m_r.points, this, [this] { update(); });
  update();
}
void AutoReadPanel::setArticle(const QString& title, const QString& text, bool french) {
  m_r = news::autoRead(title, text);
  m_loaded = true;
  m_french = french;
  if (french) news::Translate::instance().request(m_r.points, this, [this] { update(); });   // key points follow the article language
  QStringList syms;
  for (const auto& s : m_r.symbols) if (market::find(s)) syms << s;
  market::QuoteHub::instance().subscribe(this, syms, isVisible());
  update();
}

void AutoReadPanel::paintOrb(QPainter& p, const QPointF& c, double r) const {
  // static layers (glow, sphere, text) rendered once; per frame: a pulse scale and the drifting embers
  static QPixmap base;
  static double baseR = 0;
  static QString baseLang;
  static int baseRev = -1;
  const double R = r * 1.6;
  if (base.isNull() || baseR != r || baseLang != i18n::code() || baseRev != theme::revision()) {
    baseR = r; baseLang = i18n::code(); baseRev = theme::revision();
    const qreal dpr = 2.0;
    base = QPixmap(QSize(int(2 * R), int(2 * R)) * dpr); base.setDevicePixelRatio(dpr); base.fill(Qt::transparent);
    QPainter b(&base);
    b.setRenderHint(QPainter::Antialiasing, true);
    const QPointF o(R, R);
    // reference: dark red-brown sphere with mottled surface, narrow bright amber rim, soft outer halo
    QRadialGradient glow(o, r * 1.5);
    glow.setColorAt(0.6, theme::map(210, 140, 45, 150)); glow.setColorAt(0.7, theme::map(160, 95, 25, 70));
    glow.setColorAt(0.85, theme::isDark() ? theme::map(90, 50, 10, 25) : theme::map(210, 140, 45, 0));   // no dark smudge on light themes
    glow.setColorAt(1, theme::map(0, 0, 0, 0));
    b.setPen(Qt::NoPen); b.setBrush(glow); b.drawEllipse(o, r * 1.5, r * 1.5);
    QRadialGradient body(o, r);   // centred: an even rim all around (reference), not a crescent
    body.setColorAt(0, theme::map("#6b3a1a")); body.setColorAt(0.7, theme::map("#5a2e12")); body.setColorAt(0.9, theme::map("#4a250c"));
    body.setColorAt(0.965, theme::map("#8c5218")); body.setColorAt(1, theme::map("#d08a30"));
    b.setBrush(body); b.drawEllipse(o, r, r);
    // mottled surface (fixed pattern, deterministic)
    for (int i = 0; i < 26; ++i) {
      const double a = i * 2.3999, rr = r * 0.75 * std::sqrt(std::fmod(i * 0.381966, 1.0));
      const QPointF q = o + QPointF(std::cos(a) * rr, std::sin(a) * rr);
      QRadialGradient blot(q, r * 0.22);
      blot.setColorAt(0, theme::map(30, 12, 4, 70)); blot.setColorAt(1, theme::map(30, 12, 4, 0));
      b.setBrush(blot); b.drawEllipse(q, r * 0.22, r * 0.22);
    }
    const bool small = r < 60;   // short windows: smaller orb, smaller lettering
    QFont of = theme::display(small ? 12 : 17, QFont::DemiBold, small ? 0.8 : 1.5);
    const QString orbText = QCoreApplication::translate("news", "AUTO READ");
    while (of.pixelSize() > 9 && QFontMetricsF(of).horizontalAdvance(orbText) > 2 * r * 0.84) of.setPixelSize(of.pixelSize() - 1);   // French is longer
    b.setFont(of); b.setPen(theme::map("#f6e7cb"));
    b.drawText(QRectF(o.x() - r, o.y() - (small ? 15 : 22), 2 * r, small ? 18 : 26), Qt::AlignCenter, QCoreApplication::translate("news", "AUTO READ"));
    b.setFont(theme::mono(small ? 6 : 7, QFont::Bold, small ? 0.8 : 1.6)); b.setPen(theme::map("#af9b80"));
    b.drawText(QRectF(o.x() - r, o.y() + (small ? 4 : 6), 2 * r, 14), Qt::AlignCenter, QCoreApplication::translate("news", "RULES · NO LLM"));
  }
  p.save();
  p.drawPixmap(QPointF(c.x() - R, c.y() - R), base);   // unscaled: the motion comes from the embers
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen);
  for (int i = 0; i < 36; ++i) {
    const double a = i * 2.39996 + m_phase * (0.25 + (i % 5) * 0.04);
    const double rr = r * (1.02 + 0.25 * std::fmod(i * 0.618, 1.0));   // specks in the halo
    const QPointF q = c + QPointF(std::cos(a) * rr, std::sin(a) * rr);
    const int al = 30 + int(60 * (0.5 + 0.5 * std::sin(m_phase * 3 + i)));
    p.setBrush(theme::map(255, 196, 110, al)); p.drawEllipse(q, 1.0, 1.0);   // map() is cached per colour
  }
  p.restore();
}

void AutoReadPanel::paintEvent(QPaintEvent* ev) {
  QPainter p(this);
  const double W = width(), H = height();
  p.fillRect(rect(), theme::map("#080603"));
  p.setPen(theme::map("#272215")); p.drawRect(QRectF(0.5, 0.5, W - 1, H - 1));
  // header
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(Qt::NoPen); p.setBrush(theme::map("#eedda2")); p.drawEllipse(QPointF(12, 16), 2.5, 2.5); p.setBrush(Qt::NoBrush);
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setFont(theme::condensed(12, QFont::DemiBold)); p.setPen(theme::map("#aca89e"));
  p.drawText(QRectF(20, 6, 200, 20), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "AUTOMATED READ"));
  const QRectF chip(W - 66, 7, 57, 18);
  p.fillRect(chip, theme::map("#2f110f")); p.setPen(theme::map("#4a1c18")); p.drawRect(chip.adjusted(0.5, 0.5, -0.5, -0.5));
  p.setFont(theme::condensed(11, QFont::DemiBold)); p.setPen(theme::map("#b76e6c"));
  p.drawText(chip, Qt::AlignCenter, QCoreApplication::translate("news", "NO LLM"));
  p.setFont(theme::condensed(11, QFont::DemiBold)); p.setPen(theme::map("#ad9e7a"));
  p.drawText(QRectF(W - 220, 6, 148, 20), Qt::AlignRight | Qt::AlignVCenter, m_loaded ? QCoreApplication::translate("news", "ARTICLE LOADED") : QCoreApplication::translate("news", "WAITING"));
  // orb box
  const QRectF ob(1, 32, W - 2, orbBoxH());
  p.fillRect(ob, theme::map("#0b0804"));
  p.setPen(theme::map("#1f1a10")); p.drawLine(ob.bottomLeft() + QPointF(0, 0.5), ob.bottomRight() + QPointF(0, 0.5));
  p.drawLine(ob.topLeft(), ob.topRight());
  paintOrb(p, QPointF(W / 2, ob.center().y() - 2), ob.height() > 150 ? 72 : 44);   // short windows: smaller orb
  if (ev->rect().bottom() <= ob.bottom() + 1 && ev->rect().top() >= ob.top() - 1) return;   // orb frame only

  // content
  double y = ob.bottom() + 11;
  const double x = 9, w = W - 18;
  p.setFont(theme::condensed(14, QFont::Medium)); p.setPen(theme::map("#b7b5b0"));
  p.drawText(QRectF(x, y, w, 18), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "Rule-based read: figures, banks, markets."));
  y += 28;
  auto label = [&](const QString& t) {
    p.setFont(theme::mono(9, QFont::Bold, 2)); p.setPen(theme::map("#8f8466"));
    p.drawText(QRectF(x, y, w, 14), Qt::AlignLeft | Qt::AlignVCenter, t); y += 19;
  };
  const double bottom = H - 46;
  const QFont bf = theme::condensed(14, QFont::Medium);
  if (!m_r.points.isEmpty() && y + 19 + wrapLines(m_r.points.first(), bf, w - 12).size() * 17 < bottom - 40) {
    label(QCoreApplication::translate("news", "KEY POINTS"));
    p.setFont(bf);
    const int maxPoints = H > 820 ? 5 : 3;   // short windows keep room for the quotes
    int shown = 0;
    for (const auto& pt0 : m_r.points) {
      const QString pt = m_french ? news::Translate::instance().text(pt0) : pt0;
      const QStringList lines = wrapLines(pt, bf, w - 12);
      if (shown >= maxPoints || (shown > 0 && y + lines.size() * 17 > bottom - 160)) break;
      ++shown;
      p.fillRect(QRectF(x, y + 7, 4, 4), theme::map("#d9c884"));
      p.setPen(theme::map("#d6d2c6"));
      for (const auto& l : lines) { p.drawText(QRectF(x + 12, y, w - 12, 17), Qt::AlignLeft | Qt::AlignVCenter, l); y += 17; }
      y += 5;
    }
    y += 6;
  }
  if (!m_r.banks.isEmpty() || !m_r.countries.isEmpty()) {
    label(QCoreApplication::translate("news", "MENTIONED"));
    p.setFont(theme::mono(10, QFont::Bold, 0.5));
    double cx = x;
    for (const auto& b0 : m_r.banks + m_r.countries) {
      const QString b = m_r.banks.contains(b0) ? b0 : QCoreApplication::translate("news", b0.toUtf8().constData());
      const double cw = QFontMetricsF(p.font()).horizontalAdvance(b.toUpper()) + 14;
      if (cx + cw > x + w) { cx = x; y += 24; }
      const bool bank = m_r.banks.contains(b0);
      const QRectF r(cx, y, cw, 19);
      p.fillRect(r, bank ? theme::map("#483e01") : theme::map("#14110a"));
      p.setPen(bank ? theme::map("#6b5d1a") : theme::map("#2a2418")); p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
      p.setPen(bank ? theme::map("#f3e4a5") : theme::map("#a9a59a")); p.drawText(r, Qt::AlignCenter, b.toUpper());
      cx += cw + 6;
    }
    y += 31;
  }
  if (!m_r.symbols.isEmpty() && y + 19 + 23 <= bottom - 40) {
    label(QCoreApplication::translate("news", "LIVE QUOTES"));
    for (const auto& s : m_r.symbols) {
      if (y + 22 > bottom - 40) break;
      const auto q = market::QuoteHub::instance().quote(s);
      p.setFont(theme::mono(11, QFont::Bold)); p.setPen(theme::map("#e9e5d8"));
      p.drawText(QRectF(x, y, 120, 20), Qt::AlignLeft | Qt::AlignVCenter, market::display(s));
      p.setFont(theme::mono(11)); p.setPen(theme::map("#a9a7a0"));
      p.drawText(QRectF(x + 90, y, 110, 20), Qt::AlignRight | Qt::AlignVCenter, q.valid() ? fmt::price(q.price, s) : fmt::dash());
      p.setPen(q.valid() && std::isfinite(q.changePct) ? (q.changePct >= 0 ? theme::map("#8bc6cd") : theme::map("#d69997")) : theme::map("#5b5750"));
      p.drawText(QRectF(x + 200, y, w - 200, 20), Qt::AlignRight | Qt::AlignVCenter,
                 q.valid() && std::isfinite(q.changePct) ? fmt::pct(q.changePct) : QString("—"));
      p.fillRect(QRectF(x, y + 21, w, 1), theme::map("#15120a"));
      y += 23;
    }
    y += 10;
  }
  if (y + 19 + 51 < bottom) {
    label(QCoreApplication::translate("news", "TONE"));
    p.setFont(theme::mono(10)); p.setPen(m_r.tone > 0 ? theme::map("#8bc6cd") : m_r.tone < 0 ? theme::map("#d69997") : theme::map("#a9a59a"));
    const QString t = m_r.tone > 0 ? QCoreApplication::translate("news", "HAWKISH / STRONG LEANING") : m_r.tone < 0 ? QCoreApplication::translate("news", "DOVISH / WEAK LEANING") : QCoreApplication::translate("news", "NEUTRAL");
    p.drawText(QRectF(x, y, w, 16), Qt::AlignLeft | Qt::AlignVCenter, t);
    y += 17;
    p.setPen(theme::map("#6b685f"));
    if (!m_r.toneWords.isEmpty()) { p.drawText(QRectF(x, y, w, 16), Qt::AlignLeft | Qt::AlignVCenter, m_r.toneWords); y += 17; }
    p.drawText(QRectF(x, y, w, 16), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "~%1 MIN READ · %2 WORDS").arg(m_r.readMinutes).arg(m_r.words));
  }
  // find button
  const QRectF b(W - 33, H - 33, 24, 24);
  p.fillRect(b, theme::map("#2a2405")); p.setPen(theme::map("#5e5320")); p.drawRect(b.adjusted(0.5, 0.5, -0.5, -0.5));
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(theme::map("#d9c884"), 1.3)); p.drawEllipse(b.center() + QPointF(-1.5, -1.5), 4, 4);
  p.drawLine(b.center() + QPointF(1.5, 1.5), b.center() + QPointF(5, 5));
  p.fillRect(QRectF(1, H - 42, W - 2, 1), theme::map("#1a160c"));
}

// ------------------------------------------------------------------------------------------------ reader
ReaderView::ReaderView(FeedState* st, QWidget* parent) : QWidget(parent), m_st(st) {
  setMouseTracking(true);
  setAttribute(Qt::WA_OpaquePaintEvent);
  m_scroll = new QScrollArea(this);
  m_scroll->setFrameShape(QFrame::NoFrame);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scroll->setWidgetResizable(true);
  m_scroll->setStyleSheet(QString("QScrollArea{background:%1;}").arg(theme::map("#000000").name()));
  m_article = new ArticleWidget(st);
  m_scroll->setWidget(m_article);
  m_scroll->verticalScrollBar()->setSingleStep(40);
  m_scroll->viewport()->installEventFilter(this);
  m_wheel = new QVariantAnimation(this);
  m_wheel->setDuration(160); m_wheel->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_wheel, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_scroll->verticalScrollBar()->setValue(v.toInt()); });
  m_panel = new AutoReadPanel(this);
  connect(m_panel->findBox(), &QLineEdit::textChanged, this, &ReaderView::findWord);
  connect(st, &FeedState::queueChanged, this, [this] { update(); });
  connect(st, &FeedState::textChanged, this, [this] { update(QRect(0, 0, width(), 46)); m_panel->setFrench(m_st->fr); });
}

void ReaderView::open(const news::Story& s) {
  m_s = s;
  m_article->setStory(s);
  const QString body = m_article->plainText();
  m_panel->setArticle(s.title, body.size() > s.summary.size() ? body : s.summary, m_st->fr);
  m_panel->findBox()->clear();
  m_scroll->verticalScrollBar()->setValue(0);
  update();
}
bool ReaderView::eventFilter(QObject* o, QEvent* e) {
  if (o == m_scroll->viewport() && e->type() == QEvent::Wheel) {
    auto* w = static_cast<QWheelEvent*>(e);
    if (!w->pixelDelta().isNull()) return false;          // trackpads scroll natively
    auto* sb = m_scroll->verticalScrollBar();
    const int from = sb->value();
    if (m_wheel->state() != QAbstractAnimation::Running) m_wheelTarget = from;
    m_wheelTarget = std::clamp(m_wheelTarget - int(w->angleDelta().y() / 120.0 * 160), sb->minimum(), sb->maximum());
    m_wheel->stop(); m_wheel->setStartValue(from); m_wheel->setEndValue(m_wheelTarget); m_wheel->start();
    return true;
  }
  return QWidget::eventFilter(o, e);
}

void ReaderView::scrollArticle(int y) { m_scroll->verticalScrollBar()->setValue(y); }
void ReaderView::findWord(const QString& w) {
  const int n = m_article->find(w);
  if (n > 0 && m_article->firstMatchY() >= 0) m_scroll->verticalScrollBar()->setValue(int(m_article->firstMatchY() - 120));
}

// Phones (compact width): the article across the whole width, the automated read below it (its find box at the
// bottom, in reach of the thumb); the side column of the desktop left the article ~110 px wide.
int ReaderView::phonePanelH() const { return std::clamp(int(height() * 0.36), 220, 340); }
void ReaderView::resizeEvent(QResizeEvent*) {
  const int W = width(), H = height();
  if (ui::compact(W)) {
    const int ph = phonePanelH();
    m_scroll->setGeometry(0, 47, W, H - 47 - ph - 8);
    m_panel->setGeometry(6, H - ph - 6, W - 12, ph);
    return;
  }
  const int pw = std::clamp(int(std::round(W * 0.2025)), 300, 420);   // 324 at the reference
  const int divider = W - pw - 17;
  m_scroll->setGeometry(0, 47, divider, H - 47);
  m_panel->setGeometry(W - pw - 8, 55, pw, H - 55 - 8);
}

QList<ReaderView::Chip> ReaderView::chips() const {
  QList<Chip> out;
  double x = queueX();
  const QFont f = theme::condensed(14, QFont::Medium);
  const QFontMetricsF fm(f);
  for (int i = 0; i < m_st->queue.size(); ++i) {
    const news::Story* s = news::Feed::instance().find(m_st->queue[i]);
    if (!s) continue;
    const QString t = QString("%1. %2").arg(out.size() + 1).arg(m_st->tx(s->title));
    const double tw = std::min(fm.horizontalAdvance(t), 245.0);
    const QRectF r(x, 8, tw + 40, 31);
    if (r.right() > width() - 12 - 50) {   // no room: the rest goes into a "+N" chip with a menu
      int rest = 0;
      for (int k = i; k < m_st->queue.size(); ++k) if (news::Feed::instance().find(m_st->queue[k])) ++rest;
      out << Chip{QRectF(x, 8, 44, 31), QRectF(), "__more:" + QString::number(rest)};
      break;
    }
    out << Chip{r, QRectF(r.right() - 22, r.y(), 20, r.height()), s->id};
    x = r.right() + 6;
  }
  return out;
}

QRectF ReaderView::backRect() const {   // fits "RETOUR AU FIL" in French
  const double w = std::max(119.0, QFontMetricsF(theme::mono(11, QFont::Bold, 0.6)).horizontalAdvance(QCoreApplication::translate("news", "BACK TO FEED")) + 40);
  return QRectF(10, 8, w, 30);
}
double ReaderView::queueX() const {
  return backRect().right() + 8 + QFontMetricsF(theme::mono(9, QFont::Normal, 2)).horizontalAdvance(QCoreApplication::translate("news", "QUEUE")) + 8;
}

void ReaderView::paintEvent(QPaintEvent*) {
  QPainter p(this);
  const double W = width();
  p.fillRect(rect(), theme::map("#000000"));
  p.fillRect(QRectF(0, 0, W, 46), theme::map("#0f0a02"));
  p.fillRect(QRectF(0, 46, W, 1), theme::map("#201f14"));
  if (!ui::compact(int(W))) p.fillRect(QRectF(W - std::clamp(std::round(W * 0.2025), 300.0, 420.0) - 17, 47, 1, height() - 47), theme::map("#16140d"));
  const QRectF back = backRect();
  p.fillRect(back, m_hoverBack ? theme::map("#ffe44a") : theme::map("#fed90b"));
  p.setFont(theme::mono(11, QFont::Bold, 0.6)); p.setPen(theme::isDefault() ? theme::map("#1d1702") : theme::onAccent());
  p.drawText(back.adjusted(29, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "BACK TO FEED"));
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(theme::isDefault() ? theme::map("#1d1702") : theme::onAccent(), 1.4));
  const QPointF a(back.x() + 12, back.center().y());
  p.drawLine(a, a + QPointF(8, 0)); p.drawLine(a, a + QPointF(3.5, -3.5)); p.drawLine(a, a + QPointF(3.5, 3.5));
  p.setRenderHint(QPainter::Antialiasing, false);
  p.setFont(theme::mono(9, QFont::Normal, 2)); p.setPen(theme::map("#878279"));
  p.drawText(QRectF(backRect().right() + 8, 8, 60, 30), Qt::AlignLeft | Qt::AlignVCenter, QCoreApplication::translate("news", "QUEUE"));
  const auto cs = chips();
  const QFont f = theme::condensed(14, QFont::Medium);
  for (int i = 0; i < cs.size(); ++i) {
    const Chip& c = cs[i];
    if (c.id.startsWith("__more:")) {
      p.fillRect(c.r, m_hover == i ? theme::map("#2a2405") : theme::map("#191403"));
      p.setPen(theme::map("#3a3418")); p.drawRect(c.r.adjusted(0.5, 0.5, -0.5, -0.5));
      p.setFont(theme::mono(11, QFont::Bold)); p.setPen(theme::map("#d9c37f"));
      p.drawText(c.r, Qt::AlignCenter, "+" + c.id.mid(7));
      continue;
    }
    const bool cur = c.id == m_s.id;
    p.fillRect(c.r, cur ? theme::map("#3f3706") : (m_hover == i ? theme::map("#2a2405") : theme::map("#191403")));
    p.setPen(cur ? theme::map("#534c25") : theme::map("#3a3418")); p.drawRect(c.r.adjusted(0.5, 0.5, -0.5, -0.5));
    const news::Story* s = news::Feed::instance().find(c.id);
    const QString t = QString("%1. %2").arg(i + 1).arg(s ? m_st->tx(s->title) : QString());
    p.setFont(f); p.setPen(cur ? theme::map("#d9c37f") : theme::map("#a8996a"));
    p.drawText(QRectF(c.r.x() + 10, c.r.y(), c.r.width() - 34, c.r.height()), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetricsF(f).elidedText(t, Qt::ElideRight, c.r.width() - 34).replace("…", "..."));
    p.setPen(QPen(m_hover == i && m_hoverX ? theme::map("#f3e4a5") : theme::map("#8f835a"), 1.1));
    const QPointF xc = c.x.center();
    p.drawLine(xc + QPointF(-3, -3), xc + QPointF(3, 3)); p.drawLine(xc + QPointF(3, -3), xc + QPointF(-3, 3));
  }
  if (cs.isEmpty()) {
    p.setFont(theme::mono(10)); p.setPen(theme::map("#4f4b43"));
    const double ew = std::min(400.0, W - queueX() - 8);   // phones: elided at the window edge
    p.drawText(QRectF(queueX(), 8, ew, 30), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetricsF(p.font()).elidedText(QCoreApplication::translate("news", "empty — add stories with + on the cards"), Qt::ElideRight, ew));
  }
}
void ReaderView::mouseMoveEvent(QMouseEvent* e) {
  int h = -1; bool hx = false;
  const auto cs = chips();
  for (int i = 0; i < cs.size(); ++i) if (cs[i].r.contains(e->position())) { h = i; hx = cs[i].x.contains(e->position()); }
  const bool hb = backRect().contains(e->position());
  setCursor(h >= 0 || hb ? Qt::PointingHandCursor : Qt::ArrowCursor);
  if (h != m_hover || hx != m_hoverX || hb != m_hoverBack) { m_hover = h; m_hoverX = hx; m_hoverBack = hb; update(QRect(0, 0, width(), 46)); }
}
void ReaderView::mousePressEvent(QMouseEvent* e) {
  if (backRect().contains(e->position())) { emit back(); return; }
  const auto cs = chips();
  for (const auto& c : cs) {
    if (!c.r.contains(e->position())) continue;
    if (c.id.startsWith("__more:")) {
      QMenu menu(this);
      QSet<QString> shownIds; for (const auto& o : cs) shownIds.insert(o.id);
      int n = 0;
      for (const auto& id : m_st->queue) {
        const news::Story* s = news::Feed::instance().find(id);
        if (!s) continue;
        ++n;
        if (shownIds.contains(id)) continue;
        const QString t = QFontMetrics(menu.font()).elidedText(QString("%1. %2").arg(n).arg(m_st->tx(s->title)), Qt::ElideRight, 420);
        menu.addAction(t, this, [this, id] { if (const news::Story* st = news::Feed::instance().find(id)) open(*st); });
      }
      menu.exec(mapToGlobal(c.r.bottomLeft().toPoint() + QPoint(0, 4)));
      return;
    }
    if (c.x.contains(e->position())) { m_st->queue.removeAll(c.id); m_st->save(); emit m_st->queueChanged(); update(); }
    else if (const news::Story* s = news::Feed::instance().find(c.id)) open(*s);
    return;
  }
}
void ReaderView::leaveEvent(QEvent*) { m_hover = -1; m_hoverX = m_hoverBack = false; update(QRect(0, 0, width(), 46)); }
}
