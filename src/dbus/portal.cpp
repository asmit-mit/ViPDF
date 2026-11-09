#include "dbus/portal.h"

namespace dbus {

Portal::Portal()
    : conn_(sdbus::createSessionBusConnection()),
      proxy_(sdbus::createProxy(*conn_, dest_name_, sdbus::ObjectPath{OBJ_PATH})) {
  conn_->enterEventLoopAsync();

  sender_ = conn_->getUniqueName();
  sender_.erase(0, 1);
  for (auto &c : sender_)
    if (c == '.')
      c = '_';
}

void Portal::request(
    const sdbus::InterfaceName &interface,
    const sdbus::MethodName &method,
    Results options,
    const ArgWriter &write_args,
    Callback onResponse
) {
  const std::string token = "tok" + std::to_string(++counter_);
  options["handle_token"] = sdbus::Variant{token};

  sdbus::ObjectPath request_path{OBJ_PATH + "/request/" + sender_ + "/" + token};
  auto req = sdbus::createProxy(*conn_, dest_name_, request_path);

  req->uponSignal(request_signal_)
      .onInterface(request_interface_)
      .call([this, token, onResponse](std::uint32_t code, Results results) {
        if (onResponse)
          onResponse(code, results);

        std::lock_guard lock(mutex_);
        auto it = pending_.find(token);
        if (it != pending_.end()) {
          finished_.push_back(std::move(it->second));
          pending_.erase(it);
        }
      });

  {
    std::lock_guard lock(mutex_);
    finished_.clear();
    pending_[token] = std::move(req);
  }

  auto call = proxy_->createMethodCall(interface, method);
  write_args(call);
  call << options;
  proxy_->callMethod(call);
}

} // namespace dbus
