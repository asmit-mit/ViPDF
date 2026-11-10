#pragma once

#include <sdbus-c++/sdbus-c++.h>
#include <string>

#include "dbus/portal.h"

namespace dbus {

class FilePrinter {
public:
  explicit FilePrinter(Portal &portal);

  enum class PrintStatus { Started, Busy, Unavailable };

  bool available() const;
  PrintStatus printFile(const std::string &path);

private:
  static constexpr char INTERFACE_NAME[] = "org.freedesktop.portal.Print";
  static constexpr char PREPARE_PRINT_METHOD[] = "PreparePrint";
  static constexpr char PRINT_METHOD[] = "Print";

  const sdbus::InterfaceName interface_;
  const sdbus::MethodName print_method_;
  const sdbus::MethodName prepare_print_method_;

  std::shared_ptr<std::atomic<bool>> busy_ = std::make_shared<std::atomic<bool>>(false);

  Portal &portal_;
};

} // namespace dbus
