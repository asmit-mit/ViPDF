#pragma once

#include <memory>
#include <sdbus-c++/sdbus-c++.h>

namespace dbus {

class Portal {
public:
  Portal();

  sdbus::IProxy &getProxy();

private:
  static constexpr char DEST_NAME[] = "org.freedesktop.portal.Desktop";
  static constexpr char OBJ_PATH[] = "/org/freedesktop/portal/desktop";

  std::unique_ptr<sdbus::IConnection> conn_;
  std::unique_ptr<sdbus::IProxy> proxy_;
};

} // namespace dbus
