#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>

#include "dbus/portal.h"

namespace dbus {

class LinkOpener {
public:
  explicit LinkOpener(Portal &portal);

  void openLink(const std::string &url);

private:
  static constexpr char INTERFACE_NAME[] = "org.freedesktop.portal.OpenURI";
  static constexpr char METHOD_NAME[] = "OpenURI";

  const sdbus::InterfaceName interface_name_;
  const sdbus::MethodName method_name_;

  Portal &portal_;
};

} // namespace dbus
