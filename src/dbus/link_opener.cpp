#include "dbus/link_opener.h"

#include <iostream>

namespace dbus {

LinkOpener::LinkOpener(Portal &portal)
    : interface_(INTERFACE_NAME), method_(METHOD_NAME), portal_(portal) {}

void LinkOpener::openLink(const std::string &url) {
  Portal::Results options;
  Portal::Callback callback = [url](std::uint32_t code, const Portal::Results &) {
    if (code)
      std::cerr << "Failed to open link " << url << "with error code: " << code << "\n";
  };

  portal_.request(
      interface_,
      method_,
      std::move(options),
      [&](sdbus::MethodCall &call) { call << std::string{""} << url; },
      std::move(callback)
  );
}

} // namespace dbus
