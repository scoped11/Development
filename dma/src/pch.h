#pragma once

#define NOMINMAX
#include <Windows.h>

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <functional>

#include <d3d11.h>
#include <dxgi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include <vmmdll.h>
#include <leechcore.h>
#pragma comment(lib, "vmm.lib")
#pragma comment(lib, "leechcore.lib")

#include "imgui.h"
#include "json.hpp"
#include "math/vector.h"
