#include "dbus/file_printer.h"

#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <unistd.h>

namespace dbus {

FilePrinter::FilePrinter(Portal &portal)
    : interface_(INTERFACE_NAME), print_method_(PRINT_METHOD),
      prepare_print_method_(PREPARE_PRINT_METHOD), portal_(portal) {}

bool FilePrinter::printFile(const std::string &path) {
  const std::string parent{""};
  const std::string title = std::filesystem::path(path).filename().string();

  bool expected = false;
  if (!busy_->compare_exchange_strong(expected, true))
    return false;

  std::shared_ptr<void> guard(nullptr, [busy = busy_](void *) { *busy = false; });

  Portal::Callback onPreparePrint =
      [this, guard, parent, title, path](std::uint32_t code, const Portal::Results &res) {
        if (code) {
          std::cerr << "Print dialog cancelled or failed with code: " << code << "\n";
          return;
        }

        const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
          std::perror("open");
          return;
        }

        Portal::Results options;

        Portal::Callback onPrint = [guard, title](std::uint32_t print_code, const Portal::Results &) {
          if (print_code)
            std::cerr << "Failed to print document with code: " << print_code << "\n";
        };

        try {
          portal_.request(
              interface_,
              print_method_,
              std::move(options),
              [&](sdbus::MethodCall &call) { call << parent << title << sdbus::UnixFd(fd); },
              std::move(onPrint)
          );
        } catch (const sdbus::Error &e) {
        }

        ::close(fd);
      };

  portal_.request(
      interface_,
      prepare_print_method_,
      {},
      [&](sdbus::MethodCall &call) {
        call << parent << title << Portal::Results{} << Portal::Results{};
      },
      std::move(onPreparePrint)
  );

  return true;
}

} // namespace dbus
