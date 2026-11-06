#include "dbus/link_opener.h"

namespace dbus {

LinkOpener::LinkOpener(Portal &portal)
    : interface_name_(INTERFACE_NAME), method_name_(METHOD_NAME), portal_(portal) {}

void LinkOpener::openLink(const std::string &url) {
  auto call = portal_.getProxy().createMethodCall(interface_name_, method_name_);

  std::map<std::string, sdbus::Variant> options;
  call << std::string{""} << url << options;

  portal_.getProxy().callMethod(call);
}

} // namespace dbus
