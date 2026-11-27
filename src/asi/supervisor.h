#pragma once

#include <string>
#include <windows.h>

namespace asterui::supervisor {

void Start(HMODULE module);
void Stop();
void Tick();

bool Send(const std::string& json);
bool IsConnected();
bool IsReady();
uint32_t MaxBrowsers();

}  // namespace asterui::supervisor
