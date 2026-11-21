#pragma once
#include <QDialog>

class QLabel;
class QPushButton;
class QTextBrowser;
namespace updates { class Bar; }

// The header's update icon opens this: the version installed, the newest one (what's new: the release's text), and
// one button — install and start again (macOS, Windows), or the release's page (iOS, where an app cannot install
// itself). Follows app::Updater; closing it does not stop a download.
class UpdateDialog : public QDialog {
  Q_OBJECT
public:
  explicit UpdateDialog(QWidget* parent = nullptr);
  void fit();
protected:
  void paintEvent(QPaintEvent*) override;
  void resizeEvent(QResizeEvent*) override;
  void keyPressEvent(QKeyEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
private:
  void refresh();
  QRectF closeRect() const;
  QLabel* m_status;
  QLabel* m_detail;
  QTextBrowser* m_notes;
  updates::Bar* m_bar;
  QPushButton* m_primary;
  QPushButton* m_secondary;
};
