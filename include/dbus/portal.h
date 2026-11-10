#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <vector>

namespace dbus {

class Portal {
public:
  using Results = std::map<std::string, sdbus::Variant>;
  using ArgWriter = std::function<void(sdbus::MethodCall &)>;
  using Callback = std::function<void(std::uint32_t code, const Results &)>;

  Portal();

  bool available() const;
  bool request(
      const sdbus::InterfaceName &interface,
      const sdbus::MethodName &method,
      Results options,
      const ArgWriter &write_args,
      Callback on_response
  );

  static std::string errorReason(const Results &results);

private:
  void fail(const std::string &token, const std::string &msg, const Callback &onResponse);

private:
  const std::string DEST_NAME = "org.freedesktop.portal.Desktop";
  const std::string OBJ_PATH = "/org/freedesktop/portal/desktop";
  const std::string REQUEST_INTERFACE = "org.freedesktop.portal.Request";
  const std::string REQUEST_SIGNAL = "Response";

  const sdbus::ServiceName dest_name_{DEST_NAME};
  const sdbus::ObjectPath obj_path_{OBJ_PATH};
  const sdbus::SignalName request_signal_{REQUEST_SIGNAL};
  const sdbus::InterfaceName request_interface_{REQUEST_INTERFACE};

  std::unique_ptr<sdbus::IConnection> conn_;
  std::unique_ptr<sdbus::IProxy> proxy_;
  std::string sender_;
  std::atomic<int> counter_{0};

  std::mutex mutex_;
  std::map<std::string, std::unique_ptr<sdbus::IProxy>> pending_;
  std::vector<std::unique_ptr<sdbus::IProxy>> finished_;
};

} // namespace dbus
