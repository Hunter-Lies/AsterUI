#include "host/ui_scheme.h"

#include "include/cef_parser.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace asterui::host {

namespace {

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    std::wstring result(length, L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        length);
    return result;
}

void ReplaceAll(std::string& value, const std::string& from, const std::string& to) {
    size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
    }
}

}  // namespace

std::string NormalizePageUrl(const std::string& uiRoot, const std::string& url) {
    if (url.rfind("file:///", 0) == 0 || url.rfind("asterui://app/", 0) == 0) {
        return url;
    }
    std::string relative = url;
    while (!relative.empty() && (relative.front() == '/' || relative.front() == '\\')) {
        relative.erase(relative.begin());
    }
    std::replace(relative.begin(), relative.end(), '\\', '/');
    const std::filesystem::path root(Utf8ToWide(uiRoot));
    const std::filesystem::path file = root / std::filesystem::path(Utf8ToWide(relative));
    std::wstring path = file.lexically_normal().wstring();
    std::replace(path.begin(), path.end(), L'\\', L'/');
    std::string encoded = CefURIEncode(CefString(path), false).ToString();
    ReplaceAll(encoded, "%2F", "/");
    ReplaceAll(encoded, "%3A", ":");
    return "file:///" + encoded;
}

}  // namespace asterui::host
