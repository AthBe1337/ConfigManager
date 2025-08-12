#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <sys/ioctl.h>
  #include <unistd.h>
#endif

namespace ui {
  using namespace ftxui;

  // 获取终端宽度（跨平台）
  inline int get_terminal_width() {
  #ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
      return csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
  #else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
      return ws.ws_col;
    }
  #endif
    return 80;
  }

  // ---------- UTF-8 解码与宽度估算（跨平台，不依赖 codecvt/wcwidth） ----------
  // 从 s[pos] 解一个 codepoint，返回 codepoint，并将 pos 移到下一个字节位置。
  // 如果遇到非法序列，会把单字节当作 codepoint。
  inline uint32_t next_codepoint(const std::string &s, size_t &pos) {
    unsigned char c = static_cast<unsigned char>(s[pos]);
    if (c < 0x80) {
      ++pos;
      return c;
    } else if ((c >> 5) == 0x6 && pos + 1 < s.size()) {
      uint32_t cp = (c & 0x1F);
      cp = (cp << 6) | (static_cast<unsigned char>(s[pos+1]) & 0x3F);
      pos += 2;
      return cp;
    } else if ((c >> 4) == 0xE && pos + 2 < s.size()) {
      uint32_t cp = (c & 0x0F);
      cp = (cp << 6) | (static_cast<unsigned char>(s[pos+1]) & 0x3F);
      cp = (cp << 6) | (static_cast<unsigned char>(s[pos+2]) & 0x3F);
      pos += 3;
      return cp;
    } else if ((c >> 3) == 0x1E && pos + 3 < s.size()) {
      uint32_t cp = (c & 0x07);
      cp = (cp << 6) | (static_cast<unsigned char>(s[pos+1]) & 0x3F);
      cp = (cp << 6) | (static_cast<unsigned char>(s[pos+2]) & 0x3F);
      cp = (cp << 6) | (static_cast<unsigned char>(s[pos+3]) & 0x3F);
      pos += 4;
      return cp;
    } else {
      // 不合法或不完整，从单字节继续，避免死循环
      ++pos;
      return c;
    }
  }

  // 判断 codepoint 是否应视作宽字符（宽度 2）
  // 包含常见 CJK / 全角 / emoji 范围；并非穷尽表，但覆盖常见情形。
  inline bool is_wide(uint32_t cp) {
    // 使用若干常用范围（来自 EastAsian + emoji 常见区段）
    // 这些范围覆盖 CJK、Fullwidth forms、Hangul、CJK Compatibility、Emoji 区段等。
    static const std::pair<uint32_t, uint32_t> ranges[] = {
      {0x1100, 0x115F},
      {0x2329, 0x232A},
      {0x2E80, 0xA4CF},
      {0xAC00, 0xD7A3},
      {0xF900, 0xFAFF},
      {0xFE10, 0xFE19},
      {0xFE30, 0xFE6F},
      {0xFF00, 0xFF60},
      {0xFFE0, 0xFFE6},
      {0x1F300, 0x1F64F},
      {0x1F900, 0x1F9FF},
      {0x20000, 0x3FFFD}
    };
    for (auto &r : ranges) {
      if (cp >= r.first && cp <= r.second) return true;
    }
    return false;
  }

  // 计算单个 codepoint 的显示宽度（0 / 1 / 2）
  inline int codepoint_width(uint32_t cp) {
    // 控制字符（C0/C1）或零宽控制符返回 0
    if (cp == 0) return 0;
    if (cp < 0x20 || (cp >= 0x7f && cp < 0xa0)) return 0;
    return is_wide(cp) ? 2 : 1;
  }

  // 计算 UTF-8 字符串的显示列宽
  inline int utf8_display_width(const std::string &s) {
    int width = 0;
    for (size_t i = 0; i < s.size();) {
      uint32_t cp = next_codepoint(s, i);
      width += codepoint_width(cp);
    }
    return width;
  }

  // 按显示列宽拆分（不会破坏多字节字符）
  inline std::vector<std::string> split_utf8_by_width(const std::string &s, int max_width) {
    std::vector<std::string> out;
    if (s.empty()) { out.push_back(""); return out; }

    size_t i = 0;
    std::string current;
    int width_now = 0;
    while (i < s.size()) {
      size_t start = i;
      uint32_t cp = next_codepoint(s, i); // i 已移到下个字符
      int w = codepoint_width(cp);
      // take bytes [start, i)
      std::string bytes = s.substr(start, i - start);
      if (width_now + w > max_width) {
        if (!current.empty()) {
          out.push_back(current);
          current.clear();
          width_now = 0;
        }
        // if single char width > max_width, still push it as line
        if (w > max_width) {
          out.push_back(bytes);
          continue;
        }
      }
      current += bytes;
      width_now += w;
    }
    if (!current.empty()) out.push_back(current);
    return out;
  }

  // 优先按词换行（英文/含空格），否则按显示列宽拆分（中文/长串）
  inline std::vector<std::string> wrap_paragraph(const std::string &paragraph, int max_width) {
    std::vector<std::string> lines;
    if (paragraph.empty()) { lines.push_back(""); return lines; }

    if (paragraph.find(' ') != std::string::npos) {
      std::istringstream iss(paragraph);
      std::string word, line;
      while (iss >> word) {
        int line_width = utf8_display_width(line);
        int word_width = utf8_display_width(word);
        if (line_width + (line.empty() ? 0 : 1) + word_width <= max_width) {
          if (!line.empty()) line += ' ';
          line += word;
        } else {
          if (!line.empty()) { lines.push_back(line); line.clear(); }
          if (word_width <= max_width) {
            line = word;
          } else {
            auto pieces = split_utf8_by_width(word, max_width);
            for (size_t k = 0; k < pieces.size(); ++k) {
              if (k + 1 < pieces.size()) lines.push_back(pieces[k]);
              else line = pieces[k];
            }
          }
        }
      }
      if (!line.empty()) lines.push_back(line);
    } else {
      // 全中文或无空格长串
      lines = split_utf8_by_width(paragraph, max_width);
    }

    if (lines.empty()) lines.push_back("");
    return lines;
  }

  // 生成 ftxui::Element 列表（每行一个 text(...)）
  inline std::vector<Element> make_wrapped_text(const std::string &text_input, int max_width) {
    std::vector<Element> elements;
    std::istringstream iss(text_input);
    std::string paragraph;
    bool first = true;
    while (std::getline(iss, paragraph)) {
      if (!first) elements.push_back(text(""));
      first = false;
      auto wrapped = wrap_paragraph(paragraph, max_width);
      for (auto &ln : wrapped) elements.push_back(text(ln));
    }
    if (text_input.empty()) elements.push_back(text(""));
    return elements;
  }
} // namespace ui
