#include <asterui/asterui.hpp>

#include "inc/main.h"
#include "inc/natives.h"
#include "inc/types.h"

#include <nlohmann/json.hpp>

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>

namespace {

constexpr char kPageUrl[] = "examples/demo-ui/index.html";
constexpr int32_t kLayer = 60;
constexpr Blip kWaypointBlip = 8;
constexpr uint64_t kToggleSuppressionMs = 250;

HMODULE g_module = nullptr;
std::atomic<bool> g_unloading = false;
std::atomic<bool> g_toggleRequested = false;
std::atomic<uint64_t> g_ignoreToggleUntil = 0;
std::mutex g_eventMutex;
std::string g_pendingEvent;

uint64_t NowMs() {
    return static_cast<uint64_t>(GetTickCount64());
}

void OnKeyboardMessage(
    DWORD key,
    WORD,
    BYTE,
    BOOL,
    BOOL,
    BOOL,
    BOOL isUpNow) {
    if (key == VK_F5 && isUpNow) {
        g_toggleRequested.store(true);
    }
}

void OnAsterUiEvent(
    uint32_t,
    const char* name,
    const char* payload,
    void*) {
    if (!name || std::string(name) != "demo:command" || !payload) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_eventMutex);
    g_pendingEvent = payload;
}

asterui::Api ResolveAsterUi() {
    HMODULE module = GetModuleHandleA("AsterUI.asi");
    if (!module) {
        return {};
    }
    const auto getApi = reinterpret_cast<AsterGetApiFn>(
        GetProcAddress(module, "AsterUI_GetApi"));
    if (!getApi) {
        return {};
    }
    return asterui::Api(getApi(ASTERUI_API_VERSION));
}

struct PlayerSnapshot {
    int health = 0;
    int armor = 0;
    int wanted = 0;
    int hour = 0;
    int minute = 0;
    bool inVehicle = false;
    bool invincible = false;
    bool waypointActive = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float speedKph = 0.0f;
    float waypointX = 0.0f;
    float waypointY = 0.0f;
    float vehicleHealth = 0.0f;
};

struct TeleportState {
    bool active = false;
    std::string label;
};

struct NamedLocation {
    const char* id;
    const char* label;
    float x;
    float y;
    float z;
};

constexpr std::array<NamedLocation, 7> kLocations = {{
    {"legion", "Legion Square", 215.6f, -810.9f, 30.7f},
    {"airport", "LS International Airport", -1034.6f, -2731.5f, 20.2f},
    {"vespucci", "Vespucci Beach", -1586.0f, -1042.7f, 13.0f},
    {"casino", "Diamond Casino", 925.0f, 46.0f, 81.0f},
    {"sandy", "Sandy Shores", 1848.6f, 3683.9f, 34.3f},
    {"paleto", "Paleto Bay", -275.3f, 6635.6f, 7.5f},
    {"chiliad", "Mount Chiliad", 501.9f, 5604.3f, 797.2f},
}};

PlayerSnapshot ReadPlayerSnapshot(bool invincible) {
    PlayerSnapshot snapshot;
    snapshot.invincible = invincible;
    snapshot.hour = TIME::GET_CLOCK_HOURS();
    snapshot.minute = TIME::GET_CLOCK_MINUTES();

    const Ped ped = PLAYER::PLAYER_PED_ID();
    if (ENTITY::DOES_ENTITY_EXIST(ped)) {
        const Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(ped, FALSE);
        const Entity tracked = ENTITY::DOES_ENTITY_EXIST(vehicle) ? vehicle : ped;
        const Vector3 coords = ENTITY::GET_ENTITY_COORDS(tracked, TRUE);
        snapshot.health = ENTITY::GET_ENTITY_HEALTH(ped);
        snapshot.armor = PED::GET_PED_ARMOUR(ped);
        snapshot.inVehicle = ENTITY::DOES_ENTITY_EXIST(vehicle) != 0;
        snapshot.x = coords.x;
        snapshot.y = coords.y;
        snapshot.z = coords.z;
        snapshot.speedKph = ENTITY::GET_ENTITY_SPEED(tracked) * 3.6f;
        if (snapshot.inVehicle) {
            snapshot.vehicleHealth =
                static_cast<float>(ENTITY::GET_ENTITY_HEALTH(vehicle));
        }
    }

    const Player player = PLAYER::PLAYER_ID();
    if (PLAYER::IS_PLAYER_PLAYING(player)) {
        snapshot.wanted = PLAYER::GET_PLAYER_WANTED_LEVEL(player);
    }
    if (UI::IS_WAYPOINT_ACTIVE()) {
        const Blip blip = UI::GET_FIRST_BLIP_INFO_ID(kWaypointBlip);
        if (UI::DOES_BLIP_EXIST(blip)) {
            const Vector3 coords = UI::GET_BLIP_INFO_ID_COORD(blip);
            snapshot.waypointActive = true;
            snapshot.waypointX = coords.x;
            snapshot.waypointY = coords.y;
        }
    }
    return snapshot;
}

bool SnapshotChanged(
    const PlayerSnapshot& previous,
    const PlayerSnapshot& current) {
    return previous.health != current.health ||
        previous.armor != current.armor ||
        previous.wanted != current.wanted ||
        previous.hour != current.hour ||
        previous.minute != current.minute ||
        previous.inVehicle != current.inVehicle ||
        previous.invincible != current.invincible ||
        previous.waypointActive != current.waypointActive ||
        std::abs(previous.x - current.x) > 0.25f ||
        std::abs(previous.y - current.y) > 0.25f ||
        std::abs(previous.z - current.z) > 0.25f ||
        std::abs(previous.speedKph - current.speedKph) > 0.4f ||
        std::abs(previous.waypointX - current.waypointX) > 0.1f ||
        std::abs(previous.waypointY - current.waypointY) > 0.1f ||
        std::abs(previous.vehicleHealth - current.vehicleHealth) > 0.5f;
}

void SendSnapshot(
    const asterui::Api& api,
    uint32_t browser,
    const PlayerSnapshot& snapshot,
    const TeleportState& teleport,
    const std::string& message) {
    try {
        nlohmann::json state = {
            {"invincible", snapshot.invincible},
            {"health", snapshot.health},
            {"armor", snapshot.armor},
            {"wanted", snapshot.wanted},
            {"inVehicle", snapshot.inVehicle},
            {"vehicleHealth", snapshot.vehicleHealth},
            {"speedKph", snapshot.speedKph},
            {"x", snapshot.x},
            {"y", snapshot.y},
            {"z", snapshot.z},
            {"hour", snapshot.hour},
            {"minute", snapshot.minute},
            {"waypointActive", snapshot.waypointActive},
            {"waypointX", snapshot.waypointX},
            {"waypointY", snapshot.waypointY},
            {"teleportPending", teleport.active},
            {"teleportLabel", teleport.label},
            {"message", message},
        };
        api.SendEvent(browser, "demo:state", state.dump().c_str());
    } catch (...) {
    }
}

void SendState(
    const asterui::Api& api,
    uint32_t browser,
    bool invincible,
    const TeleportState& teleport,
    const std::string& message) {
    SendSnapshot(
        api,
        browser,
        ReadPlayerSnapshot(invincible),
        teleport,
        message);
}

void SetInvincible(bool enabled) {
    PLAYER::SET_PLAYER_INVINCIBLE(PLAYER::PLAYER_ID(), enabled);
}

void HealPlayer() {
    const Ped ped = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(ped)) {
        return;
    }
    ENTITY::SET_ENTITY_HEALTH(ped, 200);
    PED::SET_PED_ARMOUR(ped, 100);
}

void ClearWanted() {
    const Player player = PLAYER::PLAYER_ID();
    PLAYER::CLEAR_PLAYER_WANTED_LEVEL(player);
    PLAYER::SET_PLAYER_WANTED_LEVEL_NOW(player, FALSE);
}

Vehicle CurrentVehicle() {
    const Ped ped = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(ped)) {
        return 0;
    }
    const Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(ped, FALSE);
    return ENTITY::DOES_ENTITY_EXIST(vehicle) ? vehicle : 0;
}

std::string RepairCurrentVehicle() {
    const Vehicle vehicle = CurrentVehicle();
    if (!vehicle) {
        return "No current vehicle";
    }
    VEHICLE::SET_VEHICLE_FIXED(vehicle);
    VEHICLE::SET_VEHICLE_DIRT_LEVEL(vehicle, 0.0f);
    return "Vehicle repaired";
}

std::string CleanCurrentVehicle() {
    const Vehicle vehicle = CurrentVehicle();
    if (!vehicle) {
        return "No current vehicle";
    }
    VEHICLE::SET_VEHICLE_DIRT_LEVEL(vehicle, 0.0f);
    return "Vehicle cleaned";
}

std::string StopCurrentVehicle() {
    const Vehicle vehicle = CurrentVehicle();
    if (!vehicle) {
        return "No current vehicle";
    }
    ENTITY::SET_ENTITY_VELOCITY(vehicle, 0.0f, 0.0f, 0.0f);
    VEHICLE::SET_VEHICLE_FORWARD_SPEED(vehicle, 0.0f);
    return "Vehicle stopped";
}

std::string FlipCurrentVehicle() {
    const Vehicle vehicle = CurrentVehicle();
    if (!vehicle) {
        return "No current vehicle";
    }
    const Vector3 coords = ENTITY::GET_ENTITY_COORDS(vehicle, TRUE);
    const float heading = ENTITY::GET_ENTITY_HEADING(vehicle);
    ENTITY::SET_ENTITY_COORDS_NO_OFFSET(
        vehicle,
        coords.x,
        coords.y,
        coords.z + 0.75f,
        FALSE,
        FALSE,
        TRUE);
    ENTITY::SET_ENTITY_ROTATION(vehicle, 0.0f, 0.0f, heading, 2, TRUE);
    ENTITY::SET_ENTITY_VELOCITY(vehicle, 0.0f, 0.0f, 0.0f);
    VEHICLE::SET_VEHICLE_ON_GROUND_PROPERLY(vehicle);
    return "Vehicle righted";
}

void SetWorldTime(int hour) {
    hour = hour < 0 ? 0 : (hour > 23 ? 23 : hour);
    TIME::SET_CLOCK_TIME(hour, 0, 0);
}

std::string SetWorldWeather(const std::string& weather) {
    if (weather == "clear") {
        char value[] = "EXTRASUNNY";
        GAMEPLAY::SET_WEATHER_TYPE_NOW(value);
        return "Weather set to clear";
    }
    if (weather == "clouds") {
        char value[] = "CLOUDS";
        GAMEPLAY::SET_WEATHER_TYPE_NOW(value);
        return "Weather set to cloudy";
    }
    if (weather == "rain") {
        char value[] = "RAIN";
        GAMEPLAY::SET_WEATHER_TYPE_NOW(value);
        return "Weather set to rain";
    }
    if (weather == "fog") {
        char value[] = "FOGGY";
        GAMEPLAY::SET_WEATHER_TYPE_NOW(value);
        return "Weather set to fog";
    }
    return "Unknown weather";
}

bool TryGetGroundZ(float x, float y, float* result) {
    constexpr std::array<float, 9> kProbeHeights = {
        1000.0f,
        750.0f,
        500.0f,
        350.0f,
        200.0f,
        100.0f,
        50.0f,
        10.0f,
        0.0f,
    };
    for (const float probe : kProbeHeights) {
        float ground = 0.0f;
        if (GAMEPLAY::GET_GROUND_Z_FOR_3D_COORD(
                x,
                y,
                probe,
                &ground,
                FALSE) &&
            std::isfinite(ground) &&
            ground > -150.0f &&
            ground < 1500.0f) {
            *result = ground;
            return true;
        }
    }
    return false;
}

bool TryGetRoadZ(float x, float y, float* outX, float* outY, float* outZ) {
    Vector3 node = {};
    if (!PATHFIND::GET_CLOSEST_VEHICLE_NODE(
            x,
            y,
            1000.0f,
            &node,
            0,
            3.0f,
            0.0f)) {
        return false;
    }
    if (!std::isfinite(node.x) ||
        !std::isfinite(node.y) ||
        !std::isfinite(node.z)) {
        return false;
    }
    *outX = node.x;
    *outY = node.y;
    *outZ = node.z;
    return true;
}

void ApplyGroundTeleport(float x, float y, float groundZ) {
    const Ped ped = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(ped)) {
        return;
    }

    const Vehicle vehicle = CurrentVehicle();
    const Entity target = vehicle ? vehicle : ped;
    const float heading = ENTITY::GET_ENTITY_HEADING(target);
    // Place just below the resolved surface. GTA collision then resolves the
    // entity upward immediately, matching the behavior of trainer teleports
    // without exposing the player to a high-altitude fall.
    const float offset = vehicle ? -0.35f : -0.9f;

    ENTITY::SET_ENTITY_COORDS_NO_OFFSET(
        target,
        x,
        y,
        groundZ + offset,
        FALSE,
        FALSE,
        TRUE);
    ENTITY::SET_ENTITY_HEADING(target, heading);
    ENTITY::SET_ENTITY_VELOCITY(target, 0.0f, 0.0f, 0.0f);
    if (vehicle) {
        VEHICLE::SET_VEHICLE_ON_GROUND_PROPERLY(vehicle);
    }
}

bool BeginTeleport(
    TeleportState& teleport,
    float x,
    float y,
    float z,
    const std::string& label) {
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }
    STREAMING::REQUEST_COLLISION_AT_COORD(x, y, z + 100.0f);
    float ground = 0.0f;
    if (!TryGetGroundZ(x, y, &ground)) {
        float roadX = x;
        float roadY = y;
        float roadZ = z;
        if (!TryGetRoadZ(x, y, &roadX, &roadY, &roadZ)) {
            return false;
        }
        x = roadX;
        y = roadY;
        ground = roadZ;
    }
    ApplyGroundTeleport(x, y, ground);
    teleport.active = false;
    teleport.label = label;
    return true;
}

bool BeginWaypointTeleport(TeleportState& teleport) {
    if (!UI::IS_WAYPOINT_ACTIVE()) {
        return false;
    }
    const Blip blip = UI::GET_FIRST_BLIP_INFO_ID(kWaypointBlip);
    if (!UI::DOES_BLIP_EXIST(blip)) {
        return false;
    }
    const Vector3 coords = UI::GET_BLIP_INFO_ID_COORD(blip);
    return BeginTeleport(
        teleport,
        coords.x,
        coords.y,
        coords.z,
        "GTA map waypoint");
}

bool BeginLocationTeleport(
    TeleportState& teleport,
    const std::string& locationId) {
    for (const NamedLocation& location : kLocations) {
        if (locationId == location.id) {
            return BeginTeleport(
                teleport,
                location.x,
                location.y,
                location.z,
                location.label);
        }
    }
    return false;
}

void SetGameplayControlsLocked(bool locked) {
    const Player player = PLAYER::PLAYER_ID();
    PLAYER::SET_PLAYER_CONTROL(player, locked ? FALSE : TRUE, 0);
    if (locked) {
        CONTROLS::DISABLE_ALL_CONTROL_ACTIONS(0);
        CONTROLS::DISABLE_ALL_CONTROL_ACTIONS(1);
        CONTROLS::DISABLE_ALL_CONTROL_ACTIONS(2);
    }
}

std::string TakePendingEvent() {
    std::lock_guard<std::mutex> lock(g_eventMutex);
    std::string value;
    value.swap(g_pendingEvent);
    return value;
}

void ScriptMain() {
    asterui::Api api;
    uint32_t browser = 0;
    bool visible = false;
    bool invincible = false;
    uint64_t listener = 0;
    uint64_t lastSnapshotAt = 0;
    PlayerSnapshot lastSnapshot;
    TeleportState teleport;

    while (!g_unloading.load()) {
        if (!api.IsValid()) {
            api = ResolveAsterUi();
            if (!api.IsValid()) {
                WAIT(0);
                continue;
            }
        }

        try {
            if (browser == 0) {
                int width = 1920;
                int height = 1080;
                GRAPHICS::GET_SCREEN_RESOLUTION(&width, &height);
                AsterBrowserDesc description = {};
                description.structSize = sizeof(description);
                description.url = kPageUrl;
                description.width = width;
                description.height = height;
                description.transparent = 1;
                browser = api.Create(description);
                if (browser == 0) {
                    WAIT(1000);
                    continue;
                }
                api.SetLayer(browser, kLayer);
                api.SetVisible(browser, false);
                api.SetFocus(browser, false);
                listener = api.AddListener(
                    browser,
                    "demo:command",
                    OnAsterUiEvent);
            }

            if (g_toggleRequested.exchange(false) &&
                NowMs() >= g_ignoreToggleUntil.load()) {
                visible = !visible;
                if (visible) {
                    int width = 1920;
                    int height = 1080;
                    GRAPHICS::GET_SCREEN_RESOLUTION(&width, &height);
                    api.Resize(browser, width, height);
                }
                api.SetFocus(browser, visible);
                api.SetVisible(browser, visible);
                SetGameplayControlsLocked(visible);
                SendState(
                    api,
                    browser,
                    invincible,
                    teleport,
                    visible ? "Demo opened" : "Demo closed");
                lastSnapshot = ReadPlayerSnapshot(invincible);
                lastSnapshotAt = NowMs();
            }

            if (visible) {
                SetGameplayControlsLocked(true);
            }

            const std::string event = TakePendingEvent();
            if (!event.empty()) {
                try {
                    const auto value = nlohmann::json::parse(event);
                    const std::string action = value.value("action", "");
                    if (action == "request_state") {
                        SendState(api, browser, invincible, teleport, "");
                    } else if (action == "set_invincible") {
                        invincible = value.value("value", false);
                        SetInvincible(invincible);
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            invincible
                                ? "Invincibility enabled"
                                : "Invincibility disabled");
                    } else if (action == "heal") {
                        HealPlayer();
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            "Health and armor restored");
                    } else if (action == "clear_wanted") {
                        ClearWanted();
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            "Wanted level cleared");
                    } else if (action == "repair_vehicle") {
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            RepairCurrentVehicle());
                    } else if (action == "clean_vehicle") {
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            CleanCurrentVehicle());
                    } else if (action == "stop_vehicle") {
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            StopCurrentVehicle());
                    } else if (action == "flip_vehicle") {
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            FlipCurrentVehicle());
                    } else if (action == "teleport_waypoint") {
                        const bool started = BeginWaypointTeleport(teleport);
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            started
                                ? "Teleported to GTA map waypoint"
                                : "Teleport failed: no waypoint or ground");
                    } else if (action == "teleport_location") {
                        const bool started = BeginLocationTeleport(
                            teleport,
                            value.value("id", ""));
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            started
                                ? "Teleported to destination"
                                : "Teleport failed: no ground at destination");
                    } else if (action == "set_time") {
                        SetWorldTime(value.value("hour", 12));
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            "World time updated");
                    } else if (action == "set_weather") {
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            SetWorldWeather(value.value("weather", "")));
                    } else if (action == "close") {
                        visible = false;
                        g_ignoreToggleUntil.store(NowMs() + kToggleSuppressionMs);
                        api.SetFocus(browser, false);
                        api.SetVisible(browser, false);
                        SetGameplayControlsLocked(false);
                    } else {
                        SendState(
                            api,
                            browser,
                            invincible,
                            teleport,
                            "Unknown demo command");
                    }
                } catch (...) {
                    SendState(
                        api,
                        browser,
                        invincible,
                        teleport,
                        "Invalid command payload");
                }
            }

            if (visible && NowMs() - lastSnapshotAt >= 250) {
                const PlayerSnapshot current =
                    ReadPlayerSnapshot(invincible);
                lastSnapshotAt = NowMs();
                if (SnapshotChanged(lastSnapshot, current) ||
                    teleport.active) {
                    lastSnapshot = current;
                    SendSnapshot(api, browser, current, teleport, "");
                }
            }
        } catch (...) {
            SetGameplayControlsLocked(false);
            if (listener != 0 && browser != 0 && api.IsValid()) {
                try {
                    api.RemoveListener(listener);
                } catch (...) {
                }
            }
            listener = 0;
            browser = 0;
            visible = false;
            WAIT(1000);
        }

        WAIT(0);
    }

    SetGameplayControlsLocked(false);
    if (browser != 0 && api.IsValid()) {
        try {
            if (listener != 0) {
                api.RemoveListener(listener);
            }
            api.Destroy(browser);
        } catch (...) {
        }
    }
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        scriptRegister(module, ScriptMain);
        keyboardHandlerRegister(OnKeyboardMessage);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_unloading.store(true);
        keyboardHandlerUnregister(OnKeyboardMessage);
        scriptUnregister(module);
    }
    return TRUE;
}
