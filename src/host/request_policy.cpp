#include "host/request_policy.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <windows.h>

namespace asterui::host {

namespace {

std::string LowerAscii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

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

int HexValue(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

std::string PercentDecode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2 < value.size()) {
            const int high = HexValue(value[index + 1]);
            const int low = HexValue(value[index + 2]);
            if (high >= 0 && low >= 0) {
                result.push_back(static_cast<char>((high << 4) | low));
                index += 2;
                continue;
            }
        }
        result.push_back(value[index]);
    }
    return result;
}

bool IsLoopbackHost(std::string host) {
    host = LowerAscii(host);
    if (!host.empty() && host.front() == '[') {
        const size_t end = host.find(']');
        return end != std::string::npos &&
            host.substr(0, end + 1) == "[::1]";
    }
    const size_t colon = host.find(':');
    if (colon != std::string::npos) {
        host.resize(colon);
    }
    return host == "localhost" ||
        host == "127.0.0.1";
}

bool IsPathWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& path) {
    std::error_code error;
    const auto normalizedRoot = std::filesystem::weakly_canonical(root, error);
    if (error) {
        return false;
    }
    const auto normalizedPath = std::filesystem::weakly_canonical(path, error);
    if (error) {
        return false;
    }
    std::wstring rootText = normalizedRoot.wstring();
    std::wstring pathText = normalizedPath.wstring();
    if (!rootText.empty() && rootText.back() != L'\\' && rootText.back() != L'/') {
        rootText.push_back(L'\\');
    }
    const auto lower = [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    };
    std::transform(rootText.begin(), rootText.end(), rootText.begin(), lower);
    std::transform(pathText.begin(), pathText.end(), pathText.begin(), lower);
    return pathText == rootText.substr(0, rootText.size() - 1) ||
        pathText.rfind(rootText, 0) == 0;
}

}  // namespace

RequestPolicy::RequestPolicy(
    std::string uiRoot,
    const std::vector<std::string>& developmentOrigins)
    : uiRoot_(std::move(uiRoot)) {
    for (const std::string& candidate : developmentOrigins) {
        const std::string lower = LowerAscii(candidate);
        const size_t separator = lower.find("://");
        if (separator == std::string::npos ||
            lower.find_first_of("/?#", separator + 3) != std::string::npos) {
            continue;
        }
        const std::string origin = NormalizeOrigin(candidate);
        if (!origin.empty() &&
            std::find(
                developmentOrigins_.begin(),
                developmentOrigins_.end(),
                origin) == developmentOrigins_.end()) {
            developmentOrigins_.push_back(origin);
        }
    }
}

std::string RequestPolicy::NormalizeOrigin(const std::string& url) {
    const std::string lower = LowerAscii(url);
    const size_t separator = lower.find("://");
    if (separator == std::string::npos) {
        return {};
    }
    const std::string scheme = lower.substr(0, separator);
    if (scheme != "http" && scheme != "https" &&
        scheme != "ws" && scheme != "wss") {
        return {};
    }
    const size_t authorityStart = separator + 3;
    const size_t path = lower.find_first_of("/?#", authorityStart);
    const std::string authority = path == std::string::npos
        ? lower.substr(authorityStart)
        : lower.substr(authorityStart, path - authorityStart);
    if (authority.empty() ||
        authority.find('@') != std::string::npos ||
        !IsLoopbackHost(authority)) {
        return {};
    }
    const std::string normalizedScheme =
        scheme == "ws" ? "http" : scheme == "wss" ? "https" : scheme;
    return normalizedScheme + "://" + authority;
}

bool RequestPolicy::IsAllowed(
    const std::string& url,
    bool isNavigation) const {
    if (url == "about:blank") {
        return true;
    }
    const std::string lower = LowerAscii(url);
    if (lower.rfind("data:", 0) == 0 ||
        lower.rfind("blob:", 0) == 0) {
        return !isNavigation;
    }
    if (lower.rfind("asterui://app/", 0) == 0) {
        return true;
    }
    if (lower.rfind("file:///", 0) == 0) {
        std::string encodedPath = url.substr(8);
        const size_t suffix = encodedPath.find_first_of("?#");
        if (suffix != std::string::npos) {
            encodedPath.resize(suffix);
        }
        return IsPathWithin(
            std::filesystem::path(Utf8ToWide(uiRoot_)),
            std::filesystem::path(
                Utf8ToWide(PercentDecode(encodedPath))));
    }
    const std::string origin = NormalizeOrigin(url);
    return !origin.empty() &&
        std::find(
            developmentOrigins_.begin(),
            developmentOrigins_.end(),
            origin) != developmentOrigins_.end();
}

}  // namespace asterui::host
