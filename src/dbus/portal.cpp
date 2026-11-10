#include "dbus/portal.h"

namespace dbus {

Portal::Portal() {
  try {
    conn_ = sdbus::createSessionBusConnection();
    proxy_ = sdbus::createProxy(*conn_, dest_name_, sdbus::ObjectPath{OBJ_PATH});
    conn_->enterEventLoopAsync();

    sender_ = conn_->getUniqueName();
    sender_.erase(0, 1);
    for (auto &c : sender_)
      if (c == '.')
        c = '_';
  } catch (const sdbus::Error &e) {
    proxy_.reset();
    conn_.reset();
  }
}

bool Portal::available() const {
  return conn_ != nullptr;
}

bool Portal::request(
    const sdbus::InterfaceName &interface,
    const sdbus::MethodName &method,
    Results options,
    const ArgWriter &write_args,
    Callback onResponse
) {
  if (!conn_ || !proxy_) {
    if (onResponse) {
      Results err;
      err["error_message"] = sdbus::Variant{std::string{"no session bus connection"}};
      onResponse(2, err);
    }
    return false;
  }

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

  try {
    auto call = proxy_->createMethodCall(interface, method);
    write_args(call);
    call << options;
    proxy_->callMethod(call);
  } catch (const sdbus::Error &e) {
    fail(token, e.getName() + ": " + e.getMessage(), onResponse);
    return false;
  } catch (const std::exception &e) {
    fail(token, e.what(), onResponse);
    return false;
  }

  return true;
}

std::string Portal::errorReason(const Results &results) {
  if (auto it = results.find("error_message"); it != results.end())
    return it->second.get<std::string>();
  return "unknown error";
}

void Portal::fail(const std::string &token, const std::string &msg, const Callback &onResponse) {
  {
    std::lock_guard lock(mutex_);
    pending_.erase(token);
  }
  if (onResponse) {
    Results err;
    err["error_message"] = sdbus::Variant{msg};
    onResponse(2, err);
  }
}

} // namespace dbus
