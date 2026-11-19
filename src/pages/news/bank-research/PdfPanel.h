#pragma once
#include <QWidget>
#include <QAbstractScrollArea>
#include <QHash>
#include <QImage>
#include <QTimer>
#include <QSet>
#include "banks/Pdf.h"
#include "banks/Research.h"

class QVariantAnimation;
class QLineEdit;
class QLabel;

namespace banks::res {

// Pages of a PDF rendered by CoreGraphics (banks::pdf) into images, drawn in a scroll area. Pages render on a
// worker thread at the current zoom; only the visible pages (± 1) are kept. Wheel scrolling is animated.
class PdfDocView : public QAbstractScrollArea {
  Q_OBJECT
public:
  explicit PdfDocView(QWidget* parent = nullptr);
  void setFile(const QString& file);                 // empty clears
  const banks::pdf::Info& info() const { return m_info; }
  void setMessage(const QString& msg);               // shown instead of pages (loading, errors)
  int pageCount() const { return m_info.pages; }
  int currentPage() const;                           // 0-based, page at the top of the view
  void goToPage(int i);
  double zoom() const { return m_zoom; }             // 1.0 = 100 % (96 dpi)
  void setZoom(double z);                            // also leaves fit-width mode
  void fitWidth();
  void rotate();
  void scrollBy(int dy);                             // animated
signals:
  void stateChanged();
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void scrollContentsBy(int dx, int dy) override;
  void wheelEvent(QWheelEvent*) override;
private:
  void relayout();
  void requestVisible();
  void renderSoon();                                 // requestVisible once a resize / Ctrl+wheel burst settles
  void applyZoom(double z, bool deferRender);
  double renderScale(int i, double s, qreal dpr) const;   // s × dpr, lowered so one page stays under kMaxPagePixels
  double scale() const;                              // pixels per point
  QSizeF pageSize(int i) const;                      // rotated, in points
  QString m_file;
  banks::pdf::Info m_info;
  QVector<qreal> m_top;                              // y of each page (logical px) at the current scale
  double m_zoom = 1.0;
  bool m_fit = true;
  int m_rot = 0;
  QHash<int, QImage> m_img;                          // rendered at m_imgScale
  double m_imgScale = 0;
  QSet<int> m_pending;
  int m_generation = 0;
  QString m_msg;
  QVariantAnimation* m_anim;
  int m_scrollTarget = 0;
  QTimer m_renderTimer;                              // single shot, 150 ms (renderSoon)
};

// Page thumbnails (the toolbar's ≡ button), rendered on a worker thread at a small scale; click to jump.
class ThumbStrip : public QWidget {
public:
  explicit ThumbStrip(PdfDocView* view, QWidget* parent);
  void setFile(const QString& file, const banks::pdf::Info& info);
  void setCurrent(int page) { if (page != m_cur) { m_cur = page; update(); } }
  QSize sizeHint() const override;
protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void showEvent(QShowEvent*) override;
private:
  QRectF thumbRect(int i) const;
  void request();
  PdfDocView* m_view;
  QString m_file;
  banks::pdf::Info m_info;
  QHash<int, QImage> m_img;
  QSet<int> m_pending;
  int m_cur = 0, m_gen = 0;
};

// Floating viewer that replaces the globe: note header (category · date, bank, title), PDF / SOURCE / × buttons,
// a toolbar laid out like the browser PDF viewer of the reference (≡ thumbnails · title | page n / N, zoom −/+, fit,
// rotate | save, print, ⋮ more) and the document.
class PdfPanel : public QWidget {
  Q_OBJECT
public:
  explicit PdfPanel(QWidget* parent = nullptr);
  void openNote(const banks::Note& n);
  QString noteId() const { return m_note.id; }
  PdfDocView* view() const { return m_view; }
signals:
  void closed();
protected:
  void paintEvent(QPaintEvent*) override;
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;
  void resizeEvent(QResizeEvent*) override;
private:
  void refreshToolbar();
  void savePdf();
  banks::Note m_note;
  QString m_file;
  PdfDocView* m_view;
  ThumbStrip* m_thumbs;
  QAbstractScrollArea* m_thumbScroll;
  QWidget* m_toolbar;
  QWidget* m_headButtons;
  QList<QWidget*> m_wideOnly;
  QLabel* m_docTitle;
  QLineEdit* m_pageEdit;
  QLabel* m_pageCount;
  QLabel* m_zoomLabel;
  QTimer m_blink;
  bool m_cursorOn = true;
};
}  // namespace banks::res
