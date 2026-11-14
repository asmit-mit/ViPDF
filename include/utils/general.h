#pragma once

#include <algorithm>
#include <cctype>
#include <string>

#include <SFML/Graphics.hpp>

namespace utils {

inline bool isNumber(const std::string &s) {
  return !s.empty() &&
         std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

inline sf::Color hexToRGB(const uint32_t hex) {
  return sf::Color((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

inline sf::Color hexToRGBA(const uint32_t hex) {
  return sf::Color((hex >> 24) & 0xFF, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

inline std::string escapeLike(const std::string &s) {
  std::string out;
  for (char c : s) {
    if (c == '%' || c == '_' || c == '\\')
      out += '\\';
    out += c;
  }
  return out;
}

} // namespace utils
