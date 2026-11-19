#include "pages/news/bank-research/BankResearchPage.h"
#include "pages/news/bank-research/Columns.h"
#include "pages/news/bank-research/GlobeView.h"
#include "pages/news/bank-research/PdfPanel.h"
#include "pages/news/bank-research/Look.h"
#include "banks/Registry.h"
#include "banks/Research.h"
#include "app/Pages.h"
#include <QPainter>
#include <QCoreApplication>
#include "core/I18n.h"
#include <QTimer>
#include <QKeyEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <cmath>

DW_PAGE("news/bank-research", banks::res::BankResearchPage)

namespace banks::res {

BankResearchPage::BankResearchPage(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setFocusPolicy(Qt::StrongFocus);
  m_strip = new Strip(this);
  m_left = new LeftColumn(this);
  m_right = new RegistryColumn(this);
  m_centre = new QWidget(this);
  m_globe = new GlobeView(m_centre);
  m_pdf = new PdfPanel(m_centre);
  m_pdf->hide();

  // badge: institutions and regions from the registry, source of the assets
  QSet<int> regions; for (const auto& b : banks::systemic()) regions.insert(int(b.region));
  m_strip->setBadge(QCoreApplication::translate("banks", "%1 INSTITUTIONS · %2 REGIONS · SEC EDGAR").arg(banks::systemic().size()).arg(regions.size()));

  m_left->onOpen = [this](const QString& id) { openNote(id); };
  m_right->onClick = [this](const QString& id) { selectBank(id); };
  connect(m_globe, &GlobeView::bankClicked, this, &BankResearchPage::selectBank);
  connect(m_pdf, &PdfPanel::closed, this, &BankResearchPage::closeNote);

  auto& research = banks::Research::instance();
  connect(&research, &banks::Research::updated, this, [this] { m_left->reload(); applyTestHooks(); });
  connect(&banks::Registry::instance(), &banks::Registry::updated, m_right, qOverload<>(&QWidget::update));
  connect(&banks::Registry::instance(), &banks::Registry::filingsUpdated, this, [this] { m_left->reload(); });

  // Cursors blink every 500 ms. While the globe turns they toggle on its frames (the frame nearest each 500 ms step),
  // so the blink shares the frame's repaint pass; the timer does it when the globe is still (PDF open, frozen).
  m_blink.setInterval(500);
  auto toggle = [this] {
    m_nextBlink += 500;
    setBlink(!blinkOn());
    m_strip->update(QRect(0, 0, 720, 37));
    m_left->update(QRect(0, 40, 300, 50));
    m_right->update(QRect(0, 0, m_right->width(), 46));
  };
  connect(&m_blink, &QTimer::timeout, this, [this, toggle] {
    if (m_globe->animating()) return;
    m_nextBlink = m_blinkClock.elapsed();
    toggle();
  });
  connect(m_globe, &GlobeView::frame, this, [this, toggle] {
    const qint64 t = m_blinkClock.elapsed();
    if (t - m_nextBlink > 1000) m_nextBlink = t;   // back from a pause: resync
    if (t >= m_nextBlink - 33) toggle();
  });

  bool ok = false;
  const int tab = qEnvironmentVariable("DW_BANKRES_TAB").toInt(&ok);
  if (ok) m_left->setTab(tab);
}

void BankResearchPage::resizeEvent(QResizeEvent*) {
  const int W = width(), H = height();
  // Side columns keep the reference widths (362 / 338) at 1600 px and scale with the window: narrower on laptops
  // (≥ 300 / 270) so the globe and the PDF keep a usable centre, wider on big screens (≤ 480 / 420).
  const int leftW = qBound(300, int(std::lround(W * 0.22625)), 480);
  const int rightW = qBound(270, int(std::lround(W * 0.21125)), 420);
  m_strip->setGeometry(0, 0, W, 37);
  m_left->setGeometry(0, 37, leftW, H - 37);
  m_right->setGeometry(W - rightW, 37, rightW, H - 37);
  m_centre->setGeometry(leftW, 37, W - leftW - rightW, H - 37);
  m_globe->setGeometry(m_centre->rect());
  // the PDF panel fills the centre with 15 px margins, but no wider than 1600 px (the page itself stops at 1150 px:
  // longer text lines are hard to read), centred on ultrawide screens
  QRect pr = m_centre->rect().adjusted(15, 15, -14, -14);
  if (pr.width() > 1600) pr = QRect(pr.center().x() - 800, pr.top(), 1600, pr.height());
  m_pdf->setGeometry(pr);
}

void BankResearchPage::paintEvent(QPaintEvent*) { QPainter p(this); p.fillRect(rect(), look::page); }

void BankResearchPage::showEvent(QShowEvent*) {
  m_blinkClock.start(); m_nextBlink = 500;
  m_blink.start();
  banks::Research::instance().refresh();
  banks::Registry::instance().refresh();
  m_left->reload();
  applyTestHooks();
}
void BankResearchPage::hideEvent(QHideEvent*) { m_blink.stop(); }
void BankResearchPage::preload() {
  banks::Research::instance().refresh();
  banks::Registry::instance().refresh();
}

void BankResearchPage::keyPressEvent(QKeyEvent* e) {
  if (e->key() == Qt::Key_Escape && m_pdf->isVisible()) { closeNote(); return; }
  QWidget::keyPressEvent(e);
}

void BankResearchPage::openNote(const QString& id) {
  const banks::Note* n = banks::Research::instance().note(id);
  if (!n) return;
  m_left->setSelected(id);
  m_pdf->openNote(*n);
  m_pdf->show(); m_pdf->raise();
  m_globe->setPaused(true);
  m_globe->hide();
  setFocus();
}

void BankResearchPage::closeNote() {
  m_pdf->hide();
  m_left->setSelected({});
  m_globe->show();
  m_globe->setPaused(false);
}

void BankResearchPage::selectBank(const QString& id) {
  m_right->setSelected(id);
  m_globe->setSelectedBank(id);
  m_left->setBankFilter(id);
}

// Dev hooks for screenshots (documented in the header).
void BankResearchPage::applyTestHooks() {
  if (m_hooksDone) return;
  const auto notes = m_left->list();
  bool ok = false;
  const int hover = qEnvironmentVariable("DW_BANKRES_HOVER").toInt(&ok);
  if (ok && !notes->noteAt(hover).isEmpty()) { notes->setForcedHover(hover); m_hooksDone = true; }
  const int open = qEnvironmentVariable("DW_BANKRES_OPEN").toInt(&ok);
  if (ok && !notes->noteAt(open).isEmpty()) {
    m_hooksDone = true;
    openNote(notes->noteAt(open));
    const int scroll = qEnvironmentVariable("DW_BANKRES_SCROLL").toInt(&ok);
    if (ok) QTimer::singleShot(2500, this, [this, scroll] { m_pdf->view()->verticalScrollBar()->setValue(scroll); });
  }
}
}  // namespace banks::res
