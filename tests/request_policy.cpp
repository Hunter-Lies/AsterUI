#include "host/request_policy.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int wmain() {
    using asterui::host::RequestPolicy;

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "asterui-policy-smoke";
    std::filesystem::create_directories(root / "ui");
    RequestPolicy policy(
        root.string(),
        {"http://127.0.0.1:5173", "http://127.0.0.1:5174/path"});

    const auto check = [](bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAILED: " << message << '\n';
            return false;
        }
        return true;
    };

    bool ok = true;
    ok &= check(
        policy.IsAllowed("http://127.0.0.1:5173/index.html", true),
        "loopback navigation");
    ok &= check(
        policy.IsAllowed("ws://127.0.0.1:5173/", false),
        "loopback websocket");
    ok &= check(
        !policy.IsAllowed("http://127.0.0.1:5174/index.html", true),
        "path-bearing origin and other port blocked");
    ok &= check(
        !policy.IsAllowed("http://localhost.evil/index.html", true),
        "localhost suffix blocked");
    ok &= check(
        !policy.IsAllowed("https://example.com/index.html", true),
        "external navigation blocked");
    ok &= check(
        !policy.IsAllowed("data:text/html,test", true),
        "data navigation blocked");
    ok &= check(
        policy.IsAllowed("data:image/png;base64,AA==", false),
        "embedded data resource");
    ok &= check(
        policy.IsAllowed(
            "file:///" + (root / "ui" / "index.html").generic_string(),
            true),
        "package file");
    ok &= check(
        !policy.IsAllowed(
            "file:///" +
                (root / ".." / "outside.html").lexically_normal().generic_string(),
            true),
        "path traversal blocked");

    if (!ok) {
        return 1;
    }
    std::cout << "AsterUI request policy tests passed\n";
    return 0;
}
