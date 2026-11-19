#pragma once
#include <QObject>
#include <QString>
#include <QUrl>
#include <QRect>
#include <functional>

class QWidget;

namespace wire::squawk {
// A system web view for the YouTube embed player.
// Why not QWebEngineView: Qt's prebuilt WebEngine has no H.264/AAC ("proprietary codecs"), and YouTube live
// streams need them (measured: MediaSource h264=false aac=false → player error 5). macOS uses WKWebView
// (system codecs, hardware decoding, its own process). Other platforms: available() == false, the pane offers
// "Open on YouTube" instead (a WebView2 host would be the Windows equivalent).
// The web view is added as a plain subview of the top-level window's NSView, not through
// QWidget::createWindowContainer: a native child makes Qt flush the whole window on every repaint
// (measured: the 30 fps ticker cost ≈ 40 % CPU with a container, ≈ 3 % without).
class NativeTv : public QObject {
public:
  explicit NativeTv(QWidget* parent);
  ~NativeTv() override;
  bool available() const;
  void setGeometry(const QRect& inWindow);           // top-level window coordinates (logical px)
  void setVisible(bool on);
  bool isVisible() const { return m_visible; }
  void loadHtml(const QString& html, const QUrl& base);
  void clear();                                      // unload the player (no audio, no CPU)
  std::function<void(const QString&)> onMessage;     // window.webkit.messageHandlers.dw.postMessage(text)
  void deliver(const QString& m) { if (onMessage) onMessage(m); }
private:
  QWidget* m_host = nullptr;
  bool m_visible = false;
  QRect m_geo;
  void* m_view = nullptr;      // WKWebView*
  void* m_handler = nullptr;   // script message handler
};
}
