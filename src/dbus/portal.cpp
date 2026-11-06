#include "dbus/portal.h"

namespace dbus {

Portal::Portal()
    : conn_(sdbus::createSessionBusConnection()),
      proxy_(
          sdbus::createProxy(*conn_, sdbus::ServiceName{DEST_NAME}, sdbus::ObjectPath{OBJ_PATH})
      ) {}

sdbus::IProxy &Portal::getProxy() {
  return *proxy_;
}

} // namespace dbus
