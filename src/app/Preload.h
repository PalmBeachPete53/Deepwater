#pragma once

// A page whose data takes long to load (dozens of requests to a paced source) implements this: MainWindow builds it
// hidden a few seconds after launch and calls preload() inside a net::BackgroundScope tagged with the page id, so its
// requests wait behind everything the shown page asks for; opening the page promotes them (net::promote). preload()
// starts what showEvent would load, without the page's timers or animations (they start when it is shown).
namespace app {
class Preloadable {
public:
  virtual ~Preloadable() = default;
  virtual void preload() = 0;
};
}
