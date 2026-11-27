#pragma once

#include <string>
#include <vector>

namespace asterui::host {

class RequestPolicy {
public:
    RequestPolicy() = default;
    RequestPolicy(
        std::string uiRoot,
        const std::vector<std::string>& developmentOrigins);

    bool IsAllowed(const std::string& url, bool isNavigation) const;
    static std::string NormalizeOrigin(const std::string& url);

private:
    std::string uiRoot_;
    std::vector<std::string> developmentOrigins_;
};

}  // namespace asterui::host
