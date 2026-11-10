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

bool FilePrinter::available() const {
  return portal_.available();
}

FilePrinter::PrintStatus FilePrinter::printFile(const std::string &path) {
  if (!available())
    return PrintStatus::Unavailable;

  const std::string parent{""};
  const std::string title = std::filesystem::path(path).filename().string();

  bool expected = false;
  if (!busy_->compare_exchange_strong(expected, true))
    return PrintStatus::Busy;

  std::shared_ptr<void> guard(nullptr, [busy = busy_](void *) { *busy = false; });

  Portal::Callback onPreparePrint =
      [this, guard, parent, title, path](std::uint32_t code, const Portal::Results &res) {
        if (code == 1)
          return;
        if (code != 0) {
          std::cerr << "Failed to open print dialog for " << path << ": "
                    << Portal::errorReason(res) << "\n";
          return;
        }

        const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
          std::cerr << "Failed to open " << path << ": " << std::strerror(errno) << "\n";
          return;
        }

        Portal::Results options;

        Portal::Callback onPrint =
            [guard, path](std::uint32_t print_code, const Portal::Results &print_res) {
              if (print_code == 2)
                std::cerr << "Failed to print " << path << ": " << Portal::errorReason(print_res)
                          << "\n";
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

  if (!portal_.request(
          interface_,
          prepare_print_method_,
          Portal::Results{},
          [&](sdbus::MethodCall &call) {
            call << parent << title << Portal::Results{} << Portal::Results{};
          },
          std::move(onPreparePrint)
      ))
    return PrintStatus::Unavailable;

  return PrintStatus::Started;
}

} // namespace dbus
