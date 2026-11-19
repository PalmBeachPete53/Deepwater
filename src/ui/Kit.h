#pragma once
#include <QFrame>
#include <QMessageBox>
#include <QWidget>
#include <QAbstractButton>
#include <QStringList>
#include <QPixmap>

class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QVariantAnimation;

// Shared widget kit. Every page builds on these so the app reads as one system.
namespace ui {

// Panel: dark box, thin border, amber corner brackets, letter-spaced title with an optional ⓘ tooltip.
// Put content in body(); put controls at the right of the title in headerRight().
class Panel : public QFrame {
  Q_OBJECT
public:
  explicit Panel(const QString& title = {}, QWidget* parent = nullptr);
  void setTitle(const QString& t);
  void setInfo(const QString& tooltip);          // shows ⓘ after the title
  void setBrackets(bool tl, bool br);            // which corner brackets are drawn (default both)
  void setHighlighted(bool on);                  // amber border (selected card)
  void setTitleHeight(int px);                   // default 34
  void setPadding(int l, int t, int r, int b);
  QVBoxLayout* body() const { return m_body; }
  QHBoxLayout* headerRight() const { return m_right; }
  QWidget* header() const { return m_header; }
protected:
  void paintEvent(QPaintEvent*) override;
private:
  QWidget* m_header;
  QLabel* m_title;
  QLabel* m_info;
  QHBoxLayout* m_right;
  QVBoxLayout* m_body;
  bool m_tl = true, m_br = true, m_hi = false;
};

// Draws the reference's corner brackets on any painter rect.
void drawBrackets(class QPainter& p, const QRectF& r, const QColor& c, qreal len = 9, bool tl = true, bool br = true,
                  bool tr = false, bool bl = false);

// Row of toggle pills ("ALL 581 | DATA 517 | MOVERS 25"); exactly one active.
class PillBar : public QWidget {
  Q_OBJECT
public:
  enum Style { Underline, Filled, Boxed };
  explicit PillBar(const QStringList& items = {}, QWidget* parent = nullptr, Style s = Filled);
  void setItems(const QStringList& items);
  void setCounts(const QList<int>& counts);      // muted number after each label; -1 hides it
  void setCurrent(int i, bool emitSignal = false);
  int current() const { return m_cur; }
  void setFontPx(int px) { m_px = px; updateGeometry(); update(); }
  QSize sizeHint() const override;
signals:
  void changed(int index);
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void leaveEvent(QEvent*) override;
private:
  QList<QRectF> layoutRects() const;
  QStringList m_items; QList<int> m_counts; int m_cur = 0; int m_hover = -1; Style m_style; int m_px = 11;
};

// "● LIVE" badge; the dot pulses while live, grey "STALE"/"OFFLINE" otherwise.
class LiveBadge : public QWidget {
  Q_OBJECT
public:
  explicit LiveBadge(QWidget* parent = nullptr);
  void setState(bool live, const QString& text = "LIVE");
  QSize sizeHint() const override;
protected:
  void paintEvent(QPaintEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
private:
  bool m_live = true; QString m_text = "LIVE"; QVariantAnimation* m_anim; qreal m_phase = 0;
};

// Small text label with fixed font/colour, the most common building block. Plain text: labels often show names from
// outside (a run a model named, an account read from a file), which QLabel's default would render as HTML. A label
// whose text the app writes as HTML sets Qt::RichText itself.
QLabel* label(const QString& text, const QFont& f, const QColor& c, QWidget* parent = nullptr);

// QMessageBox::question / ::warning with the text taken as plain text, for the same reason.
QMessageBox::StandardButton askPlain(QWidget* parent, const QString& title, const QString& text,
                                     QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
void warnPlain(QWidget* parent, const QString& title, const QString& text);
// A tooltip text that Qt shows as plain text whatever it contains (QToolTip renders anything that looks like HTML).
QString plainTip(const QString& text);

// Placeholder shown while data loads or when a source fails: centered muted text.
class StatusOverlay : public QWidget {
  Q_OBJECT
public:
  explicit StatusOverlay(QWidget* parent);
  void showLoading(const QString& what = "Loading");
  void showError(const QString& msg);
  void clear();
protected:
  void paintEvent(QPaintEvent*) override;
  bool eventFilter(QObject*, QEvent*) override;
private:
  QString m_text; bool m_error = false;
};

// Country flag (resources/flags/<iso2>.svg), cached pixmap at the device pixel ratio.
// Accepts ISO-2 ("us"), currency ("USD" → us, "EUR" → eu) or ISO-3 ("USA").
QPixmap flag(const QString& codeOrCurrency, QSize size, qreal dpr = 2.0);
QString currencyToIso2(const QString& ccy);

// Opens a link that came from outside data (feed story, wire item, bank note) in the browser: http and https only,
// a relative link resolved against `base` (the page it came from). Anything else (file://, smb://, app schemes) is
// refused silently. Returns whether a URL was opened.
bool openWebLink(const QString& link, const QString& base = {});
}
