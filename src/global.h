#pragma once

class RcloneDaemon;

class Global {
public:
  // global variable to keep used RC ports
  QList<int> usedRcPorts = QList<int>() << 1 << 2;

  // global count of lsl/lsd rclone proecesses
  int rcloneLsProcessCount = 0;

  // private rclone rcd, owned by MainWindow; null when there is none
  RcloneDaemon *daemon = nullptr;

public:
  Global() = default;
  Global(const Global &) = delete;
  Global(Global &&) = delete;

  static Global &Instance() {
    static Global global;
    return global;
  }
};

namespace {
Global &global = Global::Instance();
}
