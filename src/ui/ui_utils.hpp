#pragma once
#include <cwchar>
#include <locale>
#include <codecvt>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

namespace ui {
    using namespace ftxui;

    // 获取终端宽度
    inline int get_terminal_width() {
        return Terminal::Size().dimx;
    }

    // 跨平台安全 wcwidth
    inline int safe_wcwidth(wchar_t wc) {
    #ifdef _WIN32
        if ((wc >= 0x1100 && wc <= 0x115F) ||
            (wc >= 0x2E80 && wc <= 0xA4CF) ||
            (wc >= 0xAC00 && wc <= 0xD7A3) ||
            (wc >= 0xF900 && wc <= 0xFAFF) ||
            (wc >= 0xFE10 && wc <= 0xFE19) ||
            (wc >= 0xFF01 && wc <= 0xFF60) ||
            (wc >= 0xFFE0 && wc <= 0xFFE6)) {
            return 2;
        }
        return 1;
    #else
        int w = ::wcwidth(wc);
        return (w < 0 ? 0 : w);
    #endif
    }

    // 计算 UTF-8 显示宽度
    inline int utf8_display_width(const std::string& s) {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        std::wstring ws = conv.from_bytes(s);
        int width = 0;
        for (wchar_t wc : ws) {
            width += safe_wcwidth(wc);
        }
        return width;
    }

    // 按宽度切分 UTF-8
    inline std::vector<std::string> split_utf8_by_width(const std::string& s, int max_width) {
        std::vector<std::string> out;
        if (s.empty()) { out.push_back(""); return out; }

        std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
        std::wstring ws = conv.from_bytes(s);

        std::string current;
        int width_now = 0;

        for (wchar_t wc : ws) {
            int w = safe_wcwidth(wc);
            if (width_now + w > max_width) {
                out.push_back(current);
                current.clear();
                width_now = 0;
            }
            current += conv.to_bytes(wc);
            width_now += w;
        }
        if (!current.empty()) out.push_back(current);
        return out;
    }

    // 自动换行（中英文混排）
    inline std::vector<std::string> wrap_paragraph(const std::string& paragraph, int max_width) {
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
                    if (!line.empty()) {
                        lines.push_back(line);
                        line.clear();
                    }
                    if (word_width <= max_width) {
                        line = word;
                    } else {
                        auto pieces = split_utf8_by_width(word, max_width);
                        for (size_t i = 0; i < pieces.size(); ++i) {
                            if (i + 1 < pieces.size()) lines.push_back(pieces[i]);
                            else line = pieces[i];
                        }
                    }
                }
            }
            if (!line.empty()) lines.push_back(line);
        } else {
            lines = split_utf8_by_width(paragraph, max_width);
        }
        if (lines.empty()) lines.push_back("");
        return lines;
    }

    // 生成自动换行的 ftxui 文本 Element 列表
    inline std::vector<Element> make_wrapped_text(const std::string& text_input, int max_width) {
        std::vector<Element> elements;
        std::istringstream iss(text_input);
        std::string paragraph;
        bool first = true;
        while (std::getline(iss, paragraph)) {
            if (!first) elements.push_back(text(""));
            first = false;
            auto wrapped = wrap_paragraph(paragraph, max_width);
            for (auto& line : wrapped) {
                elements.push_back(text(line));
            }
        }
        if (text_input.empty()) {
            elements.push_back(text(""));
        }
        return elements;
    }
}
