#include <asterui/asterui.hpp>

#include "inc/main.h"
#include "inc/natives.h"
#include "inc/types.h"

#include <nlohmann/json.hpp>

#include <windows.h>

#include <atomic>
#include <mutex>
#include <string>

namespace {

constexpr char kPageUrl[] = "examples/demo-ui/index.html";
constexpr int32_t kLayer = 60;
constexpr Blip kWaypointBlip = 8;

HMODULE g_module = nullptr;
std::atomic<bool> g_unloading = false;
std::atomic<bool> g_toggleRequested = false;
std::mutex g_eventMutex;
std::string g_pendingEvent;

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
    bool inVehicle = false;
    bool invincible = false;
    bool waypointActive = false;
    float waypointX = 0.0f;
    float waypointY = 0.0f;
};

PlayerSnapshot ReadPlayerSnapshot(bool invincible) {
    PlayerSnapshot snapshot;
    snapshot.invincible = invincible;
    const Ped ped = PLAYER::PLAYER_PED_ID();
    if (ENTITY::DOES_ENTITY_EXIST(ped)) {
        snapshot.health = ENTITY::GET_ENTITY_HEALTH(ped);
        snapshot.armor = PED::GET_PED_ARMOUR(ped);
        snapshot.inVehicle = PED::GET_VEHICLE_PED_IS_IN(ped, FALSE) != 0;
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
        previous.inVehicle != current.inVehicle ||
        previous.invincible != current.invincible ||
        previous.waypointActive != current.waypointActive ||
        previous.waypointX != current.waypointX ||
        previous.waypointY != current.waypointY;
}

void SendSnapshot(
    const asterui::Api& api,
    uint32_t browser,
    const PlayerSnapshot& snapshot,
    const std::string& message) {
    try {
        api.SendEvent(
            browser,
            "demo:state",
            nlohmann::json{
                {"invincible", snapshot.invincible},
                {"health", snapshot.health},
                {"armor", snapshot.armor},
                {"wanted", snapshot.wanted},
                {"inVehicle", snapshot.inVehicle},
                {"waypointActive", snapshot.waypointActive},
                {"waypointX", snapshot.waypointX},
                {"waypointY", snapshot.waypointY},
                {"message", message}}.dump().c_str());
    } catch (...) {
    }
}

void SendState(
    const asterui::Api& api,
    uint32_t browser,
    bool invincible,
    const std::string& message) {
    SendSnapshot(
        api,
        browser,
        ReadPlayerSnapshot(invincible),
        message);
}

void SetInvincible(bool enabled) {
    PLAYER::SET_PLAYER_INVINCIBLE(PLAYER::PLAYER_ID(), enabled);
}

void TeleportPlayer(float x, float y, float z) {
    const Ped ped = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(ped)) {
        return;
    }
    Entity target = ped;
    const Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(ped, FALSE);
    if (ENTITY::DOES_ENTITY_EXIST(vehicle)) {
        target = vehicle;
    }
    ENTITY::SET_ENTITY_COORDS(target, x, y, z, FALSE, FALSE, FALSE, TRUE);
}

bool TeleportToWaypoint() {
    if (!UI::IS_WAYPOINT_ACTIVE()) {
        return false;
    }
    const Blip blip = UI::GET_FIRST_BLIP_INFO_ID(kWaypointBlip);
    if (!UI::DOES_BLIP_EXIST(blip)) {
        return false;
    }

    Vector3 coords = UI::GET_BLIP_INFO_ID_COORD(blip);
    STREAMING::REQUEST_COLLISION_AT_COORD(coords.x, coords.y, 1000.0f);
    float ground = coords.z;
    if (GAMEPLAY::GET_GROUND_Z_FOR_3D_COORD(
            coords.x,
            coords.y,
            1000.0f,
            &ground,
            FALSE)) {
        coords.z = ground + 1.0f;
    } else {
        coords.z = 100.0f;
    }

    TeleportPlayer(coords.x, coords.y, coords.z);
    return true;
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

void RepairCurrentVehicle() {
    const Ped ped = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(ped)) {
        return;
    }
    const Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(ped, FALSE);
    if (ENTITY::DOES_ENTITY_EXIST(vehicle)) {
        VEHICLE::SET_VEHICLE_FIXED(vehicle);
        VEHICLE::SET_VEHICLE_DIRT_LEVEL(vehicle, 0.0f);
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
    PlayerSnapshot lastSnapshot;

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

            if (g_toggleRequested.exchange(false)) {
                visible = !visible;
                if (visible) {
                    int width = 1920;
                    int height = 1080;
                    GRAPHICS::GET_SCREEN_RESOLUTION(&width, &height);
                    api.Resize(browser, width, height);
                }
                api.SetFocus(browser, visible);
                api.SetVisible(browser, visible);
                SendState(
                    api,
                    browser,
                    invincible,
                    visible ? "Ready" : "Closed");
                lastSnapshot = ReadPlayerSnapshot(invincible);
            }

            const std::string event = TakePendingEvent();
            if (!event.empty()) {
                try {
                    const auto value = nlohmann::json::parse(event);
                    const std::string action = value.value("action", "");
                    if (action == "request_state") {
                        SendState(api, browser, invincible, "Ready");
                    } else if (action == "set_invincible") {
                        invincible = value.value("value", false);
                        SetInvincible(invincible);
                        SendState(
                            api,
                            browser,
                            invincible,
                            invincible ? "Invincible enabled" : "Invincible disabled");
                    } else if (action == "teleport_waypoint") {
                        SendState(
                            api,
                            browser,
                            invincible,
                            TeleportToWaypoint()
                                ? "Teleported to map waypoint"
                                : "No map waypoint is active");
                    } else if (action == "refresh_waypoint") {
                        SendState(api, browser, invincible, "Waypoint refreshed");
                    } else if (action == "heal") {
                        HealPlayer();
                        SendState(api, browser, invincible, "Player restored");
                    } else if (action == "clear_wanted") {
                        ClearWanted();
                        SendState(api, browser, invincible, "Wanted level cleared");
                    } else if (action == "repair_vehicle") {
                        RepairCurrentVehicle();
                        SendState(api, browser, invincible, "Vehicle repaired");
                    } else if (action == "close") {
                        visible = false;
                        api.SetFocus(browser, false);
                        api.SetVisible(browser, false);
                    }
                } catch (...) {
                    SendState(api, browser, invincible, "Invalid command");
                }
            }

            if (visible) {
                const PlayerSnapshot current =
                    ReadPlayerSnapshot(invincible);
                if (SnapshotChanged(lastSnapshot, current)) {
                    lastSnapshot = current;
                    SendSnapshot(api, browser, current, "Live");
                }
            }
        } catch (...) {
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
