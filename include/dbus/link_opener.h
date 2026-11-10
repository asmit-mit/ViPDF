#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>

#include "dbus/portal.h"

namespace dbus {

class LinkOpener {
public:
  explicit LinkOpener(Portal &portal);

  bool available() const;
  bool openLink(const std::string &url);

private:
  static constexpr char INTERFACE_NAME[] = "org.freedesktop.portal.OpenURI";
  static constexpr char METHOD_NAME[] = "OpenURI";

  const sdbus::InterfaceName interface_;
  const sdbus::MethodName method_;

  Portal &portal_;
};

} // namespace dbus
