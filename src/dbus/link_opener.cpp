#include "dbus/link_opener.h"

#include <iostream>

namespace dbus {

LinkOpener::LinkOpener(Portal &portal)
    : interface_(INTERFACE_NAME), method_(METHOD_NAME), portal_(portal) {}

bool LinkOpener::available() const {
  return portal_.available();
}

bool LinkOpener::openLink(const std::string &url) {
  if (!portal_.available())
    return false;

  Portal::Results options;
  Portal::Callback callback = [url](std::uint32_t code, const Portal::Results &results) {
    if (code != 2)
      return;
    std::cerr << "Failed to open link " << url << ": " << Portal::errorReason(results) << "\n";
  };

  return portal_.request(
      interface_,
      method_,
      std::move(options),
      [&url](sdbus::MethodCall &call) { call << std::string{} << url; },
      std::move(callback)
  );
}

} // namespace dbus
