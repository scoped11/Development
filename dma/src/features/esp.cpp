#include "esp.h"
#include "skeleton_pose.h"
#include "game/names.hpp"
#include "render/renderer.h"
#include "imgui.h"
#include "math/vector.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {
    ImU32 ToImCol(const Colour& c) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a));
    }

    void DrawCornerBox(ImDrawList* dl, float x, float y, float w, float h, ImU32 col, float thickness = 1.5f) {
        float cornerLen = std::fminf(w, h) * 0.22f;
        ImU32 shadow = IM_COL32(0, 0, 0, 160);
        auto line = [&](ImVec2 a, ImVec2 b) {
            dl->AddLine(a, b, shadow, thickness + 1.2f);
            dl->AddLine(a, b, col, thickness);
        };

        line(ImVec2(x, y), ImVec2(x + cornerLen, y));
        line(ImVec2(x, y), ImVec2(x, y + cornerLen));
        line(ImVec2(x + w, y), ImVec2(x + w - cornerLen, y));
        line(ImVec2(x + w, y), ImVec2(x + w, y + cornerLen));
        line(ImVec2(x, y + h), ImVec2(x + cornerLen, y + h));
        line(ImVec2(x, y + h), ImVec2(x, y + h - cornerLen));
        line(ImVec2(x + w, y + h), ImVec2(x + w - cornerLen, y + h));
        line(ImVec2(x + w, y + h), ImVec2(x + w, y + h - cornerLen));
    }

    void DrawFullBox(ImDrawList* dl, float x, float y, float w, float h, ImU32 col, float thickness = 1.5f) {
        dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(0, 0, 0, 160), 0.f, 0, thickness + 1.2f);
        dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col, 0.f, 0, thickness);
    }

    void DrawHealthBar(ImDrawList* dl, float x, float y, float h, float healthPct) {
        float barW = 4.f;
        float barX = x - barW - 3.f;

        dl->AddRectFilled(ImVec2(barX - 1, y - 1), ImVec2(barX + barW + 1, y + h + 1),
            IM_COL32(0, 0, 0, 180));

        float fillH = h * (healthPct / 100.f);
        float fillY = y + h - fillH;

        ImU32 healthCol;
        if (healthPct > 75.f)      healthCol = IM_COL32(0, 255, 0, 255);
        else if (healthPct > 50.f) healthCol = IM_COL32(255, 255, 0, 255);
        else if (healthPct > 25.f) healthCol = IM_COL32(255, 165, 0, 255);
        else                       healthCol = IM_COL32(255, 0, 0, 255);

        dl->AddRectFilled(ImVec2(barX, fillY), ImVec2(barX + barW, y + h), healthCol);
    }

    void DrawArmorBar(ImDrawList* dl, float x, float y, float h, float armorPct) {
        float barW = 4.f;
        float barX = x - barW - 9.f;
        dl->AddRectFilled(ImVec2(barX - 1, y - 1), ImVec2(barX + barW + 1, y + h + 1),
            IM_COL32(0, 0, 0, 180));
        float fillH = h * std::clamp(armorPct / 100.f, 0.f, 1.f);
        float fillY = y + h - fillH;
        dl->AddRectFilled(ImVec2(barX, fillY), ImVec2(barX + barW, y + h),
            IM_COL32(80, 170, 255, 255));
    }

    ImFont* EspFont() {
        ImFont* font = Renderer::Get().EspFont();
        return font ? font : ImGui::GetFont();
    }

    float EspFontSize() {
        const float size = ImGui::GetFontSize();
        return size > 1.f ? size : 16.f;
    }

    std::string Lower(std::string text) {
        for (char& c : text)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    }

    Colour PlayerColour(const GameEntity& ent, const ESPConfig& config) {
        if (!ent.isAlive) return config.deadColour;
        if (ent.isTeammate && config.teamEsp) return config.teamColour;
        return Names::RoleColour(ent.role, config);
    }

    Colour LootColour(int tier, const Colour& fallback) {
        return Names::RarityColour(tier, fallback);
    }

    bool ProjectUnity(const Vector3& world, const FMinimalViewInfo& view, const CameraData& cam,
                      int overlayW, int overlayH, Vector2& out) {
        const int projW = cam.gameWidth > 0 ? cam.gameWidth : overlayW;
        const int projH = cam.gameHeight > 0 ? cam.gameHeight : overlayH;
        if (!WorldToScreenUnity(world, view, projW, projH, out))
            return false;
        if (projW > 0 && projW != overlayW)
            out.x *= static_cast<float>(overlayW) / static_cast<float>(projW);
        if (projH > 0 && projH != overlayH)
            out.y *= static_cast<float>(overlayH) / static_cast<float>(projH);
        return true;
    }

    void CameraBasis(const FMinimalViewInfo& camera, Vector3& forward, Vector3& right, Vector3& up) {
        constexpr float DEG2RAD = 3.14159265358979323846f / 180.f;
        float yaw = camera.rotation.yaw * DEG2RAD;
        float pitch = camera.rotation.pitch * DEG2RAD;
        forward = {
            std::sinf(yaw) * std::cosf(pitch),
            std::sinf(pitch),
            std::cosf(yaw) * std::cosf(pitch)
        };
        Vector3 worldUp = { 0.f, 1.f, 0.f };
        right = worldUp.Cross(forward).Normalized();
        up = forward.Cross(right);
    }

    void DrawOffscreenArrow(ImDrawList* dl, const Vector3& worldPos, const FMinimalViewInfo& camera,
                            int screenWidth, int screenHeight, ImU32 col, float distM, bool scale) {
        Vector3 forward, right, up;
        CameraBasis(camera, forward, right, up);
        Vector3 delta = worldPos - camera.location;
        float x = delta.Dot(right);
        float z = delta.Dot(forward);
        if (x * x + z * z < 1.f) return;

        float ang = std::atan2f(x, z);
        float margin = 28.f;
        float rx = screenWidth * 0.5f - margin;
        float ry = screenHeight * 0.5f - margin;
        float cx = screenWidth * 0.5f;
        float cy = screenHeight * 0.5f;
        float size = scale ? std::clamp(16.f - distM * 0.02f, 8.f, 18.f) : 14.f;
        ImVec2 tip(cx + std::sinf(ang) * rx, cy - std::cosf(ang) * ry);
        ImVec2 a(tip.x - std::sinf(ang) * size + std::cosf(ang) * (size * 0.42f),
                 tip.y + std::cosf(ang) * size + std::sinf(ang) * (size * 0.42f));
        ImVec2 b(tip.x - std::sinf(ang) * size - std::cosf(ang) * (size * 0.42f),
                 tip.y + std::cosf(ang) * size - std::sinf(ang) * (size * 0.42f));
        dl->AddTriangleFilled(tip, a, b, col);
        char buf[24];
        snprintf(buf, sizeof(buf), "%.0fm", distM);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(tip.x - ts.x * 0.5f, tip.y + 6.f), col, buf);
    }

    ImU32 Shade(ImU32 col, float mul) {
        int r = static_cast<int>(((col >> 0) & 255) * mul);
        int g = static_cast<int>(((col >> 8) & 255) * mul);
        int b = static_cast<int>(((col >> 16) & 255) * mul);
        int a = (col >> 24) & 255;
        r = std::clamp(r, 0, 255); g = std::clamp(g, 0, 255); b = std::clamp(b, 0, 255);
        return IM_COL32(r, g, b, a);
    }

    void DrawGunIcon(ImDrawList* dl, float x, float y, int type, ImU32 col) {
        const float s = 16.f;
        ImVec2 o(x - s, y - 8.f);
        auto p = [&](float px, float py) { return ImVec2(o.x + px, o.y + py); };
        ImU32 dark = Shade(col, 0.45f);
        ImU32 mid = Shade(col, 0.75f);
        dl->AddRectFilled(p(-2.f, -2.f), p(s * 2.f + 2.f, 18.f), IM_COL32(0, 0, 0, 90), 3.f);
        if (type == 6) {
            dl->AddRectFilled(p(2.f, 6.f), p(22.f, 11.f), col, 1.2f);
            dl->AddRectFilled(p(20.f, 5.f), p(28.f, 10.f), mid, 1.f);
            dl->AddRectFilled(p(8.f, 10.f), p(12.f, 17.f), dark, 1.f);
            dl->AddRectFilled(p(4.f, 3.f), p(10.f, 6.f), mid);
        } else if (type == 14) {
            dl->AddRectFilled(p(0.f, 6.f), p(28.f, 11.f), col, 1.2f);
            dl->AddRectFilled(p(6.f, 2.f), p(18.f, 6.f), mid);
            dl->AddRectFilled(p(8.f, 10.f), p(13.f, 17.f), dark, 1.f);
            dl->AddRectFilled(p(18.f, 4.f), p(22.f, 6.f), dark);
        } else if (type == 13) {
            dl->AddQuadFilled(p(0.f, 10.f), p(30.f, 4.f), p(30.f, 9.f), p(0.f, 14.f), col);
            dl->AddRectFilled(p(10.f, 12.f), p(15.f, 18.f), dark, 1.f);
            dl->AddRectFilled(p(22.f, 3.f), p(30.f, 6.f), mid);
        } else {
            dl->AddRectFilled(p(0.f, 6.f), p(30.f, 11.f), col, 1.2f);
            dl->AddRectFilled(p(4.f, 1.f), p(16.f, 6.f), mid);
            dl->AddRectFilled(p(8.f, 10.f), p(13.f, 18.f), dark, 1.f);
            dl->AddRectFilled(p(24.f, 4.f), p(32.f, 8.f), mid);
            dl->AddRectFilled(p(26.f, 2.f), p(30.f, 5.f), dark);
        }
    }

    void DrawChams(ImDrawList* dl, const GameEntity& ent, const CameraData& camera,
                   int screenWidth, int screenHeight, ImU32 col, int style) {
        if (!ent.position.IsValid()) return;
        Vector3 joints[6] = {
            ent.skeletonPelvis.IsValid() ? ent.skeletonPelvis : ent.position,
            ent.skeletonChest.IsValid() ? ent.skeletonChest : ent.position + Vector3{0, 1.1f, 0},
            ent.skeletonNeck.IsValid() ? ent.skeletonNeck : ent.position + Vector3{0, 1.45f, 0},
            ent.skeletonHead.IsValid() ? ent.skeletonHead : (ent.viewHead.IsValid() ? ent.viewHead : ent.position + Vector3{0, 1.7f, 0}),
            ent.position,
            ent.viewHead.IsValid() ? ent.viewHead : ent.position + Vector3{0, 1.7f, 0}
        };
        Vector2 pts[6]{};
        int ok = 0;
        for (int i = 0; i < 6; ++i) {
            if (ProjectUnity(joints[i], camera.viewInfo, camera, screenWidth, screenHeight, pts[i]))
                ++ok;
        }
        if (ok < 3) return;
        const std::string role = Lower(ent.role);
        const bool female = Lower(ent.gender).find("female") != std::string::npos;
        const bool cop = role.find("cop") != std::string::npos && role.find("hub") == std::string::npos;
        ImU32 pants = col, shirt = col, skin = IM_COL32(210, 170, 130, 150);
        if (cop) {
            pants = IM_COL32(18, 32, 72, 150);
            shirt = IM_COL32(28, 52, 120, 160);
            skin = IM_COL32(200, 160, 120, 150);
        } else if (role.find("robber") != std::string::npos) {
            pants = IM_COL32(28, 28, 30, 150);
            shirt = IM_COL32(48, 44, 42, 160);
        } else if (role.find("prisoner") != std::string::npos) {
            pants = IM_COL32(180, 90, 20, 150);
            shirt = IM_COL32(220, 120, 30, 160);
        } else if (role.find("settler") != std::string::npos) {
            pants = IM_COL32(70, 78, 42, 150);
            shirt = IM_COL32(110, 96, 58, 160);
        } else {
            pants = (col & 0x00FFFFFF) | 0x88000000;
            shirt = (col & 0x00FFFFFF) | 0x99000000;
        }
        auto quad = [&](int a, int b, ImU32 fill, float w) {
            ImVec2 pa(pts[a].x, pts[a].y), pb(pts[b].x, pts[b].y);
            float nx = pb.y - pa.y, ny = pa.x - pb.x;
            float len = std::sqrtf(nx * nx + ny * ny);
            if (len < 1.f) return;
            nx /= len; ny /= len;
            dl->AddQuadFilled(ImVec2(pa.x + nx * w, pa.y + ny * w),
                              ImVec2(pa.x - nx * w, pa.y - ny * w),
                              ImVec2(pb.x - nx * w, pb.y - ny * w),
                              ImVec2(pb.x + nx * w, pb.y + ny * w), fill);
        };
        const float body = female ? 5.2f : 7.4f;
        auto boneLine = [&](int a, int b, ImU32 lineCol, float thick) {
            dl->AddLine(ImVec2(pts[a].x, pts[a].y), ImVec2(pts[b].x, pts[b].y), lineCol, thick);
        };
        if (style == 2) {
            boneLine(4, 0, col, 1.6f);
            boneLine(0, 1, col, 1.8f);
            boneLine(1, 2, col, 1.6f);
            boneLine(2, 3, col, 1.4f);
            dl->AddCircle(ImVec2(pts[3].x, pts[3].y), female ? 5.f : 6.5f, col, 16, 1.6f);
            return;
        }
        if (style == 0) {
            pants = shirt = skin = (col & 0x00FFFFFF) | 0x99000000;
        }
        if (style == 3) {
            ImU32 outline = IM_COL32(0, 0, 0, 220);
            quad(4, 0, outline, body * 1.15f);
            quad(0, 1, outline, body * 1.2f);
            quad(1, 2, outline, body * 0.9f);
            quad(2, 3, outline, female ? 5.4f : 6.4f);
        }
        quad(4, 0, pants, body * 0.9f);
        quad(0, 1, shirt, body);
        quad(1, 2, shirt, body * 0.7f);
        quad(2, 3, skin, female ? 4.2f : 5.2f);
        dl->AddCircleFilled(ImVec2(pts[3].x, pts[3].y), female ? 5.f : 6.5f, skin);
        if (cop) {
            dl->AddCircleFilled(ImVec2(pts[1].x, pts[1].y), 3.4f, IM_COL32(212, 175, 55, 210));
            dl->AddCircle(ImVec2(pts[3].x, pts[3].y), 7.2f, IM_COL32(40, 80, 180, 200), 12, 1.6f);
        }
    }

    void DrawWorldCircle(ImDrawList* dl, const Vector3& center, float radius,
                         const CameraData& camera, int sw, int sh, ImU32 col) {
        Vector2 last{};
        bool have = false;
        for (int i = 0; i <= 24; ++i) {
            float a = (6.2831853f * i) / 24.f;
            Vector3 p{ center.x + std::cosf(a) * radius, center.y, center.z + std::sinf(a) * radius };
            Vector2 s;
            if (!ProjectUnity(p, camera.viewInfo, camera, sw, sh, s)) {
                have = false;
                continue;
            }
            if (have)
                dl->AddLine(ImVec2(last.x, last.y), ImVec2(s.x, s.y), col, 1.6f);
            last = s;
            have = true;
        }
    }

    struct Quat { float x, y, z, w; };

    Quat MulQuat(Quat a, Quat b) {
        return {
            a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
        };
    }

    Vector3 RotateQuat(Quat q, Vector3 v) {
        Vector3 u{ q.x, q.y, q.z };
        return u * (2.f * u.Dot(v)) + v * (q.w * q.w - u.Dot(u)) + u.Cross(v) * (2.f * q.w);
    }

    bool DrawWireHull(ImDrawList* dl, const GameEntity& ent, const CameraData& camera,
                      int screenWidth, int screenHeight, ImU32 col,
                      float& x, float& y, float& w, float& h) {
        if (!ent.hasHull || !ent.hullSize.IsValid()) return false;
        float len = ent.hullQuat[0] * ent.hullQuat[0] + ent.hullQuat[1] * ent.hullQuat[1] +
                    ent.hullQuat[2] * ent.hullQuat[2] + ent.hullQuat[3] * ent.hullQuat[3];
        Quat q = len > 0.5f && len < 1.5f
            ? Quat{ ent.hullQuat[0], ent.hullQuat[1], ent.hullQuat[2], ent.hullQuat[3] }
            : Quat{ 0.f, 0.f, 0.f, 1.f };
        Vector3 half = ent.hullSize * 0.5f;
        Vector3 center = ent.position + RotateQuat(q, ent.hullCenter);
        Vector3 corner[8];
        Vector2 screen[8];
        bool ok[8] = {};
        int n = 0;
        for (int i = 0; i < 8; ++i) {
            float sx = (i & 1) ? 1.f : -1.f;
            float sy = (i & 2) ? 1.f : -1.f;
            float sz = (i & 4) ? 1.f : -1.f;
            corner[i] = center + RotateQuat(q, { half.x * sx, half.y * sy, half.z * sz });
            ok[i] = ProjectUnity(corner[i], camera.viewInfo, camera, screenWidth, screenHeight, screen[i]);
            if (ok[i]) ++n;
        }
        if (n < 2) return false;

        auto edge = [&](int a, int b) {
            if (!ok[a] || !ok[b]) return;
            ImVec2 p(screen[a].x, screen[a].y);
            ImVec2 qv(screen[b].x, screen[b].y);
            dl->AddLine(p, qv, IM_COL32(0, 0, 0, 170), 2.6f);
            dl->AddLine(p, qv, col, 1.4f);
        };
        for (int i = 0; i < 8; ++i) {
            for (int bit = 1; bit <= 4; bit <<= 1) {
                int j = i | bit;
                if (j != i && j < 8)
                    edge(i, j);
            }
        }

        float minX = 1.e9f, minY = 1.e9f, maxX = -1.e9f, maxY = -1.e9f;
        for (int i = 0; i < 8; ++i) {
            if (!ok[i]) continue;
            minX = std::fminf(minX, screen[i].x);
            minY = std::fminf(minY, screen[i].y);
            maxX = std::fmaxf(maxX, screen[i].x);
            maxY = std::fmaxf(maxY, screen[i].y);
        }
        x = minX;
        y = minY;
        w = std::fmaxf(8.f, maxX - minX);
        h = std::fmaxf(8.f, maxY - minY);
        return true;
    }

    void DrawCarBlip(ImDrawList* dl, float x, float y, ImU32 col) {
        dl->AddCircleFilled(ImVec2(x, y), 4.5f, col);
        dl->AddCircle(ImVec2(x, y), 4.5f, IM_COL32(0, 0, 0, 180), 12, 1.2f);
        ImVec2 bodyMin(x - 8.f, y - 16.f);
        ImVec2 bodyMax(x + 8.f, y - 7.f);
        dl->AddRectFilled(bodyMin, bodyMax, col, 2.f);
        dl->AddRect(bodyMin, bodyMax, IM_COL32(0, 0, 0, 180), 2.f, 0, 1.1f);
        dl->AddCircleFilled(ImVec2(x - 4.f, y - 6.f), 2.3f, IM_COL32(20, 20, 20, 230));
        dl->AddCircleFilled(ImVec2(x + 4.f, y - 6.f), 2.3f, IM_COL32(20, 20, 20, 230));
    }

    void DrawSegment(ImDrawList* dl, const Vector3& from, const Vector3& to,
                     const CameraData& camera, int screenWidth, int screenHeight) {
        if (from.Distance(to) > 2.5f)
            return;
        Vector2 a, b;
        if (!ProjectUnity(from, camera.viewInfo, camera, screenWidth, screenHeight, a) ||
            !ProjectUnity(to, camera.viewInfo, camera, screenWidth, screenHeight, b))
            return;
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        if (dx * dx + dy * dy > 600.f * 600.f)
            return;
        ImU32 col = IM_COL32(255, 255, 255, 230);
        dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), IM_COL32(0, 0, 0, 160), 2.4f);
        dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), col, 1.3f);
        dl->AddCircleFilled(ImVec2(b.x, b.y), 2.2f, col);
    }

    void DrawPosedSkeleton(ImDrawList* dl, const GameEntity& ent, const CameraData& camera,
                           int screenWidth, int screenHeight) {
        float yaw = std::atan2f(ent.aimForward.x, ent.aimForward.z);
        if (!std::isfinite(yaw)) yaw = 0.f;
        Vector3 forward{ std::sinf(yaw), 0.f, std::cosf(yaw) };
        Vector3 right{ std::cosf(yaw), 0.f, -std::sinf(yaw) };
        float scale = ent.isCrouching ? 0.62f : 1.f;
        Vector3 root = ent.position;
        if (ent.viewHead.IsValid() && ent.position.Distance(ent.viewHead) < 0.55f) {
            float h = ent.markerHeight > 0.8f ? ent.markerHeight : 1.65f;
            root = ent.viewHead;
            root.y -= h * scale;
        }
        auto joint = [&](float side, float height, float ahead) {
            return root + right * side + forward * ahead + Vector3{ 0.f, height * scale, 0.f };
        };

        if (ent.inVehicle) {
            scale = 1.f;
            float h = ent.markerHeight > 0.4f ? ent.markerHeight : 0.95f;
            Vector3 hip = joint(0.f, h * 0.28f, 0.f);
            Vector3 chest = joint(0.f, h * 0.58f, 0.02f);
            Vector3 neck = joint(0.f, h * 0.78f, 0.03f);
            Vector3 head = joint(0.f, h * 0.96f, 0.04f);
            Vector3 shoulderL = joint(-0.16f, h * 0.66f, 0.02f);
            Vector3 elbowL = joint(-0.20f, h * 0.46f, 0.18f);
            Vector3 handL = joint(-0.10f, h * 0.40f, 0.30f);
            Vector3 shoulderR = joint(0.16f, h * 0.66f, 0.02f);
            Vector3 elbowR = joint(0.20f, h * 0.46f, 0.18f);
            Vector3 handR = joint(0.10f, h * 0.40f, 0.30f);
            Vector3 hipL = joint(-0.09f, h * 0.28f, 0.02f);
            Vector3 kneeL = joint(-0.10f, h * 0.30f, 0.30f);
            Vector3 footL = joint(-0.10f, 0.02f, 0.46f);
            Vector3 hipR = joint(0.09f, h * 0.28f, 0.02f);
            Vector3 kneeR = joint(0.10f, h * 0.30f, 0.30f);
            Vector3 footR = joint(0.10f, 0.02f, 0.46f);
            const Vector3 lines[][2] = {
                { hip, chest }, { chest, neck }, { neck, head },
                { chest, shoulderL }, { shoulderL, elbowL }, { elbowL, handL },
                { chest, shoulderR }, { shoulderR, elbowR }, { elbowR, handR },
                { hip, hipL }, { hipL, kneeL }, { kneeL, footL },
                { hip, hipR }, { hipR, kneeR }, { kneeR, footR },
            };
            for (const auto& line : lines)
                DrawSegment(dl, line[0], line[1], camera, screenWidth, screenHeight);
            return;
        }

        Vector3 hip = joint(0.f, 0.95f, 0.f);
        Vector3 chest = joint(0.f, 1.32f, 0.f);
        Vector3 neck = joint(0.f, 1.52f, 0.f);
        Vector3 head = joint(0.f, 1.72f, 0.02f);
        Vector3 shoulderL = joint(-0.18f, 1.45f, 0.f);
        Vector3 elbowL = joint(-0.34f, 1.16f, 0.06f);
        Vector3 handL = joint(-0.22f, 0.96f, 0.12f);
        Vector3 shoulderR = joint(0.18f, 1.45f, 0.f);
        Vector3 elbowR = joint(0.34f, 1.16f, 0.06f);
        Vector3 handR = joint(0.22f, 0.96f, 0.12f);
        float swing = std::sinf(ent.walkPhase);
        if (!std::isfinite(swing)) swing = 0.f;
        float reach = ent.isSprinting ? 0.34f : 0.22f;
        Vector3 hipL = joint(-0.10f, 0.92f, 0.f);
        Vector3 kneeL = joint(-0.12f, 0.48f, swing * reach);
        Vector3 footL = joint(-0.10f, 0.04f, swing * reach * 1.3f);
        Vector3 hipR = joint(0.10f, 0.92f, 0.f);
        Vector3 kneeR = joint(0.12f, 0.48f, -swing * reach);
        Vector3 footR = joint(0.10f, 0.04f, -swing * reach * 1.3f);
        elbowL = joint(-0.34f, 1.16f, -swing * reach);
        handL = joint(-0.22f, 0.96f, -swing * reach * 1.2f);
        elbowR = joint(0.34f, 1.16f, swing * reach);
        handR = joint(0.22f, 0.96f, swing * reach * 1.2f);

        const Vector3 lines[][2] = {
            { hip, chest }, { chest, neck }, { neck, head },
            { chest, shoulderL }, { shoulderL, elbowL }, { elbowL, handL },
            { chest, shoulderR }, { shoulderR, elbowR }, { elbowR, handR },
            { hip, hipL }, { hipL, kneeL }, { kneeL, footL },
            { hip, hipR }, { hipR, kneeR }, { kneeR, footR },
        };
        for (const auto& line : lines)
            DrawSegment(dl, line[0], line[1], camera, screenWidth, screenHeight);
    }

    bool UseStickSkeleton(const GameEntity& ent) {
        // "Hubber" is a normal player role. Treating that substring as the
        // lobby forced a fake stick on every player, so it slid off the mesh.
        return ent.boneCount < 2 ||
            static_cast<int>(ent.boneParents.size()) != ent.boneCount ||
            ent.boneStride < 5 || ent.boneLocal < 0 || ent.boneLength < 0 ||
            static_cast<int>(ent.boneFloats.size()) < ent.boneCount * ent.boneStride;
    }

    void DrawSkeleton(ImDrawList* dl, const GameEntity& ent, const CameraData& camera,
                      int screenWidth, int screenHeight) {
        if (!ent.position.IsValid())
            return;
        if (UseStickSkeleton(ent)) {
            DrawPosedSkeleton(dl, ent, camera, screenWidth, screenHeight);
            return;
        }

        const std::vector<Vector3>* positions = nullptr;
        const std::vector<std::pair<int, int>>* segments = nullptr;
        SkeletonPose::Result livePose;
        if (ent.skeletonPoseCached && ent.skeletonSegments.size() >= 2) {
            positions = &ent.skeletonPositions;
            segments = &ent.skeletonSegments;
        } else {
            livePose = SkeletonPose::Build(ent);
            if (!livePose.ok) {
                DrawPosedSkeleton(dl, ent, camera, screenWidth, screenHeight);
                return;
            }
            positions = &livePose.positions;
            segments = &livePose.segments;
        }

        for (const auto& seg : *segments) {
            if (seg.first < 0 || seg.second < 0 ||
                seg.first >= static_cast<int>(positions->size()) ||
                seg.second >= static_cast<int>(positions->size()))
                continue;
            DrawSegment(dl, (*positions)[static_cast<size_t>(seg.first)],
                        (*positions)[static_cast<size_t>(seg.second)],
                        camera, screenWidth, screenHeight);
        }
    }

    void DrawLabel(ImDrawList* dl, float x, float& y, const char* text, ImU32 col) {
        ImFont* font = EspFont();
        const float size = EspFontSize();
        ImVec2 textSize = font->CalcTextSizeA(size, 1e9f, 0.f, text);
        float tx = x - textSize.x / 2.f;
        dl->AddText(font, size, ImVec2(tx + 1.f, y + 1.f), IM_COL32(0, 0, 0, 210), text);
        dl->AddText(font, size, ImVec2(tx, y), col, text);
        y -= textSize.y + 1.f;
    }

    void DrawLabelDown(ImDrawList* dl, float x, float& y, const char* text, ImU32 col) {
        ImFont* font = EspFont();
        const float size = EspFontSize();
        ImVec2 textSize = font->CalcTextSizeA(size, 1e9f, 0.f, text);
        float tx = x - textSize.x / 2.f;
        dl->AddText(font, size, ImVec2(tx + 1.f, y + 1.f), IM_COL32(0, 0, 0, 210), text);
        dl->AddText(font, size, ImVec2(tx, y), col, text);
        y += textSize.y + 1.f;
    }

    void DrawLootIcon(ImDrawList* dl, float x, float y, const std::string& name, bool cash) {
        ImVec2 c(x, y + 6.f);
        if (cash) {
            dl->AddCircleFilled(c, 7.f, IM_COL32(30, 150, 70, 235));
            dl->AddCircle(c, 7.f, IM_COL32(0, 0, 0, 180), 12, 1.2f);
            dl->AddText(ImVec2(c.x - 3.f, c.y - 7.f), IM_COL32(255, 255, 255, 240), "$");
            return;
        }
        std::string n = Lower(name);
        ImU32 fill = IM_COL32(210, 170, 50, 235);
        const char* mark = "L";
        if (n.find("heal") != std::string::npos || n.find("med") != std::string::npos ||
            n.find("bandage") != std::string::npos) {
            fill = IM_COL32(190, 50, 50, 235);
            mark = "+";
        } else if (n.find("ammo") != std::string::npos || n.find("mag") != std::string::npos) {
            fill = IM_COL32(220, 180, 40, 235);
            mark = "A";
        } else if (n.find("armor") != std::string::npos || n.find("armour") != std::string::npos) {
            fill = IM_COL32(70, 130, 210, 235);
            mark = "V";
        }
        dl->AddRectFilled(ImVec2(c.x - 7.f, c.y - 8.f), ImVec2(c.x + 7.f, c.y + 8.f), fill, 3.f);
        dl->AddRect(ImVec2(c.x - 7.f, c.y - 8.f), ImVec2(c.x + 7.f, c.y + 8.f),
                    IM_COL32(0, 0, 0, 180), 3.f, 0, 1.2f);
        dl->AddText(ImVec2(c.x - 3.f, c.y - 7.f), IM_COL32(255, 255, 255, 240), mark);
    }
}

void ESP::Render(const std::vector<GameEntity>& entities,
                 const GameEntity& localPlayer,
                 const CameraData& camera,
                 const ESPConfig& config,
                 int screenWidth, int screenHeight)
{
    if (!config.enabled || !camera.valid) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    int consideredTargets = 0;
    int visibleTargets = 0;

    for (const auto& ent : entities) {
        if (!ent.address || ent.isLocalPlayer) continue;
        if (ent.type != EntityType::Player && !ent.IsValid()) continue;
        if (ent.type == EntityType::Player && !ent.position.IsValid()) continue;
        if (ent.type == EntityType::Player && localPlayer.position.IsValid() &&
            ent.DistanceTo(localPlayer.position) < 1.15f)
            continue;
        if (ent.type == EntityType::Player &&
            ent.position.Distance(camera.viewInfo.location) < 0.65f)
            continue;

        // Category filter and max distance
        float maxDist = 0.f;
        Colour colour;
        bool wantBox = false;
        bool wantHealth = false;

        const bool teammate = !ent.isLocalPlayer &&
            (ent.isTeammate || (!localPlayer.clan.empty() && !ent.clan.empty() &&
                                Lower(localPlayer.clan) == Lower(ent.clan)));

        switch (ent.type) {
            case EntityType::Player: {
                if (!config.showPlayers) continue;
                if (!ent.isAlive && config.hideDead) continue;
                if (ent.isKnocked && config.hideKnocked) continue;
                if (ent.hidingInProp && !config.showHiddenPlayers) continue;
                std::string role = Lower(ent.role);
                bool cop = role.find("cop") != std::string::npos &&
                    role.find("hubber") == std::string::npos;
                bool robber = role.find("robber") != std::string::npos;
                if (cop && !config.showCops) continue;
                if (robber && !config.showRobbers) continue;
                if (!cop && !robber && !role.empty() && !config.showCivilians) continue;
                if (config.hideTeam && teammate) continue;
                if (config.visibleOnly && (ent.hidingInProp || ent.isHidden)) continue;
                maxDist = teammate && config.teamEsp ? config.maxDistanceTeam : config.maxDistancePlayers;
                colour = teammate && config.teamEsp ? config.teamColour : PlayerColour(ent, config);
                wantBox = true;
                wantHealth = true;
                break;
            }
            case EntityType::Item:
            case EntityType::DeathBag:
                if (ent.type == EntityType::DeathBag && !config.showDeathBags) continue;
                if (ent.type == EntityType::Item && !config.showItems) continue;
                if (ent.type == EntityType::Item && !Names::PassesLootFilter(ent.itemType, config)) continue;
                if (ent.cash == 0) continue;
                if (config.minItemValue > 0.f && ent.cash >= 0 && ent.cash < config.minItemValue) continue;
                maxDist = config.maxDistanceItems;
                colour = ent.type == EntityType::DeathBag
                    ? config.deathBagColour : LootColour(ent.lootTier, config.itemColour);
                break;
            case EntityType::Vehicle:
                if (!config.showVehicles) continue;
                maxDist = config.maxDistanceVehicles;
                colour = config.vehicleColour;
                wantBox = true;
                break;
            case EntityType::Mob:
                if (!config.showMobs) continue;
                maxDist = config.maxDistanceMobs;
                colour = (!ent.isAlive) ? config.deadColour : config.mobColour;
                wantBox = true;
                wantHealth = true;
                break;
            case EntityType::Grenade:
                if (!config.showGrenades) continue;
                if (ent.grenadeKind == 1 && !config.showLethalGrenades) continue;
                if (ent.grenadeKind == 2 && !config.showTacticalGrenades) continue;
                maxDist = config.maxDistanceGrenades;
                colour = ent.grenadeKind == 2 ? config.tacticalGrenadeColour : config.lethalGrenadeColour;
                break;
            case EntityType::Container:
            case EntityType::Rack:
                if (!config.showContainers) continue;
                maxDist = config.maxDistanceContainers;
                colour = config.containerColour;
                break;
            case EntityType::Camera:
                if (!config.showCameras) continue;
                maxDist = config.maxDistanceCameras;
                colour = config.cameraColour;
                break;
            case EntityType::Base: {
                if (!config.showBases) continue;
                if (config.baseHideUnsettled && ent.ownerPublicId == 0) continue;
                if (config.baseHasLootOnly && !ent.hasLoot) continue;
                if (config.baseHideRaidLocked && ent.raidLocked) continue;
                const bool mine = ent.isTeammate;
                const bool clan = ent.isClanBase && !mine;
                const bool other = !mine && !clan;
                if (mine && !config.baseMine) continue;
                if (clan && !config.baseClan) continue;
                if (other && !config.baseOthers) continue;
                if (config.baseRaidableOnly && (ent.raidLocked || ent.ownerPublicId == 0)) continue;
                maxDist = config.maxDistanceBases;
                colour = mine ? config.myBaseColour : (clan ? config.clanBaseColour : config.baseColour);
                break;
            }
            case EntityType::Atm:
                if (!config.showAtm) continue;
                maxDist = config.maxDistanceContainers;
                colour = config.atmColour;
                break;
            case EntityType::Safe:
                if (!config.showSafes) continue;
                maxDist = config.maxDistanceContainers;
                colour = config.safeColour;
                break;
            case EntityType::Vault:
                if (!config.showVaults) continue;
                maxDist = config.maxDistanceContainers;
                colour = config.vaultColour;
                break;
            case EntityType::Cargo:
                if (!config.showCargo) continue;
                maxDist = config.maxDistanceItems;
                colour = config.cargoColour;
                break;
            case EntityType::HideSpot:
                if (!config.showHideSpots) continue;
                maxDist = config.maxDistancePlayers;
                colour = Colour(0.75f, 0.55f, 0.95f, 1.f);
                break;
            default:
                continue;
        }

        float dist = ent.DistanceTo(localPlayer.position);
        if (dist > maxDist) continue;
        ++consideredTargets;

        Vector2 screenPos;
        bool projected = ProjectUnity(ent.position, camera.viewInfo, camera, screenWidth, screenHeight, screenPos);
        bool onScreen = projected &&
            screenPos.x >= 0.f && screenPos.x <= screenWidth &&
            screenPos.y >= 0.f && screenPos.y <= screenHeight;
        if (!onScreen) {
            if (ent.type == EntityType::Player && config.drawOffscreen) {
                const std::string role = Lower(ent.role);
                const bool cop = role.find("cop") != std::string::npos;
                const bool robber = role.find("robber") != std::string::npos;
                if (teammate && !config.offscreenTeammates) continue;
                if (!ent.isAlive && !config.offscreenDead) continue;
                if (cop && !config.offscreenCops) continue;
                if (robber && !config.offscreenRobbers) continue;
                DrawOffscreenArrow(dl, ent.position, camera.viewInfo, screenWidth, screenHeight,
                                   ToImCol(colour), dist, config.offscreenScaleDistance);
            }
            continue;
        }
        ++visibleTargets;

        ImU32 col = ToImCol(colour);
        float boxTop = screenPos.y;
        float boxBottom = screenPos.y;
        float boxLeft = screenPos.x;
        float boxRight = screenPos.x;
        bool haveBox = false;

        if (ent.type == EntityType::Vehicle && config.drawBoxes) {
            float hx = 0.f, hy = 0.f, hw = 0.f, hh = 0.f;
            if (DrawWireHull(dl, ent, camera, screenWidth, screenHeight, col,
                             hx, hy, hw, hh)) {
                boxLeft = hx;
                boxTop = hy;
                boxRight = hx + hw;
                boxBottom = hy + hh;
                haveBox = true;
            } else {
                DrawCarBlip(dl, screenPos.x, screenPos.y, col);
            }
        } else if (wantBox && config.drawBoxes) {
            Vector3 headPos = ent.viewHead.IsValid() ? ent.viewHead : ent.position;
            if (!ent.viewHead.IsValid())
                headPos.y += ent.markerHeight > 0.2f ? ent.markerHeight : 1.8f;
            Vector2 headScreen;
            bool headOk = ProjectUnity(headPos, camera.viewInfo, camera, screenWidth, screenHeight, headScreen);
            if (headOk) {
                float boxH = std::fabsf(screenPos.y - headScreen.y);
                float aspect = ent.markerAspect > 0.15f ? ent.markerAspect : 0.5f;
                float boxW = boxH * aspect;
                float boxX = headScreen.x - boxW / 2.f;
                float boxY = headScreen.y;

                if (boxH >= 4.f) {
                    if (config.boxStyle == 0)
                        DrawCornerBox(dl, boxX, boxY, boxW, boxH, col);
                    else
                        DrawFullBox(dl, boxX, boxY, boxW, boxH, col);

                    if (config.drawHealth && wantHealth && ent.maxHealth > 0.f)
                        DrawHealthBar(dl, boxX, boxY, boxH, ent.HealthPercent());
                    if (config.drawArmor && wantHealth && (ent.hasArmor || ent.armorAmount > 0.f)) {
                        float armorPct = ent.maxArmor > 0.f
                            ? (ent.armorAmount / ent.maxArmor) * 100.f
                            : (ent.hasArmor ? 100.f : 0.f);
                        if (armorPct < 1.f && ent.hasArmor)
                            armorPct = 100.f;
                        DrawArmorBar(dl, boxX, boxY, boxH, armorPct);
                    }

                    boxTop = boxY;
                    boxBottom = boxY + boxH;
                    boxLeft = boxX;
                    boxRight = boxX + boxW;
                    haveBox = true;
                }
            }
        }

        // Snapline from bottom of screen to entity position
        if (config.drawSnaplines) {
            dl->AddLine(
                ImVec2(screenWidth / 2.f, static_cast<float>(screenHeight)),
                ImVec2(screenPos.x, screenPos.y),
                col, 1.f);
        }

        if (ent.type == EntityType::Player && config.drawChams)
            DrawChams(dl, ent, camera, screenWidth, screenHeight, col, config.chamStyle);
        if (ent.type == EntityType::Player && config.drawSkeleton)
            DrawSkeleton(dl, ent, camera, screenWidth, screenHeight);
        if (ent.type == EntityType::Grenade && config.drawFragRadius && ent.grenadeKind == 1 && ent.blastRadius > 0.5f)
            DrawWorldCircle(dl, ent.position, ent.blastRadius, camera, screenWidth, screenHeight, col);
        if (ent.type == EntityType::Camera && ent.aimForward.Length() > 0.2f) {
            Vector3 tip = ent.position + ent.aimForward.Normalized() * 6.f;
            Vector2 a, b;
            if (ProjectUnity(ent.position, camera.viewInfo, camera, screenWidth, screenHeight, a) &&
                ProjectUnity(tip, camera.viewInfo, camera, screenWidth, screenHeight, b))
                dl->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), col, 1.6f);
        }

        if (ent.type == EntityType::Player && config.drawAimLines && ent.aimForward.Length() > 0.5f) {
            Vector3 eye = ent.position;
            eye.y += 1.6f;
            Vector3 tip = eye + ent.aimForward.Normalized() * 10.f;
            Vector2 eyeScreen, tipScreen;
            if (ProjectUnity(eye, camera.viewInfo, camera, screenWidth, screenHeight, eyeScreen) &&
                ProjectUnity(tip, camera.viewInfo, camera, screenWidth, screenHeight, tipScreen)) {
                dl->AddLine(ImVec2(eyeScreen.x, eyeScreen.y), ImVec2(tipScreen.x, tipScreen.y), col, 1.5f);
            }
        }

        const ImU32 white = IM_COL32(255, 255, 255, 235);
        const ImU32 blue = IM_COL32(90, 170, 255, 235);
        const bool marker = ent.type == EntityType::Item ||
                            ent.type == EntityType::Grenade ||
                            ent.type == EntityType::Container ||
                            ent.type == EntityType::Camera ||
                            ent.type == EntityType::Base ||
                            ent.type == EntityType::Atm ||
                            ent.type == EntityType::Safe ||
                            ent.type == EntityType::Vault ||
                            ent.type == EntityType::Cargo ||
                            ent.type == EntityType::HideSpot ||
                            ent.type == EntityType::DeathBag ||
                            ent.type == EntityType::Rack;
        if (marker) {
            dl->AddCircleFilled(ImVec2(screenPos.x, screenPos.y), 4.5f, col);
            dl->AddCircle(ImVec2(screenPos.x, screenPos.y), 4.5f, IM_COL32(0, 0, 0, 200), 16, 1.4f);
        }
        // Players need the name clear of the skeleton head. Loot sits above its dot,
        // with the distance kept clear underneath.
        float above = (haveBox ? boxTop : screenPos.y) - (ent.type == EntityType::Player ? 34.f : (marker ? 18.f : 4.f));
        float below = (haveBox ? boxBottom : screenPos.y) + (marker ? 14.f : 2.f);
        float textX = haveBox ? (boxLeft + boxRight) * 0.5f : screenPos.x;

        if (ent.type == EntityType::Player && config.drawNames) {
            std::string name = ent.displayName;
            if (teammate)
                name = "[TEAM] " + name;
            else if (!ent.clan.empty())
                name = "[" + ent.clan + "] " + name;
            DrawLabel(dl, textX, above, name.c_str(), teammate ? ToImCol(config.teamColour) : white);
            const char* faction = Names::FactionLabel(ent.role, teammate);
            if (faction && faction[0])
                DrawLabel(dl, textX, above, faction, ToImCol(PlayerColour(ent, config)));
            if (config.showPoliceUniform && ent.policeUniform && !teammate)
                DrawLabel(dl, textX, above, "Police", ToImCol(config.copColour));
            if (config.showGender && !ent.gender.empty())
                DrawLabel(dl, textX, above, ent.gender.c_str(), IM_COL32(210, 190, 255, 230));
            if (config.showOutfit && !ent.outfit.empty())
                DrawLabel(dl, textX, above, ent.outfit.c_str(), IM_COL32(180, 210, 230, 230));
            if (config.showHair && !ent.hair.empty())
                DrawLabel(dl, textX, above, ent.hair.c_str(), IM_COL32(200, 180, 140, 230));
            if (config.showBeard && !ent.beard.empty())
                DrawLabel(dl, textX, above, ent.beard.c_str(), IM_COL32(170, 150, 120, 230));
        } else if (config.drawNames && !ent.displayName.empty()) {
            if (ent.type == EntityType::Item && ent.isWeapon && config.showGunIcons) {
                DrawGunIcon(dl, textX, above - 2.f, ent.itemType, col);
                above -= 22.f;
            } else {
                if (ent.type == EntityType::Item) {
                    ImVec2 size = ImGui::CalcTextSize(ent.displayName.c_str());
                    DrawLootIcon(dl, textX - size.x * 0.5f - 16.f, above, ent.displayName, ent.cash >= 0);
                }
                DrawLabel(dl, textX, above, ent.displayName.c_str(),
                          (ent.type == EntityType::Item || ent.type == EntityType::DeathBag) ? col : white);
            }
            if (!ent.contents.empty() && config.showContainerContents)
                DrawLabel(dl, textX, above, ent.contents.c_str(), blue);
            if (ent.cash < 0 && !ent.extra.empty())
                DrawLabel(dl, textX, above, ent.extra.c_str(), blue);
            if (ent.cameraBroken)
                DrawLabel(dl, textX, above, "shot out", IM_COL32(255, 90, 90, 235));
            if (ent.drillEndTs > 0) {
                const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                int64_t remain = ent.drillEndTs > 1000000000000LL ? (ent.drillEndTs - nowMs) / 1000 : ent.drillEndTs;
                char drill[48];
                if (remain > 0)
                    snprintf(drill, sizeof(drill), "drill %llds", static_cast<long long>(remain));
                else
                    snprintf(drill, sizeof(drill), "drill ready");
                DrawLabel(dl, textX, above, drill, IM_COL32(255, 180, 80, 235));
            }
            if (ent.type == EntityType::Base) {
                if (ent.vip)
                    DrawLabel(dl, textX, above, "VIP", IM_COL32(255, 220, 80, 235));
                if (ent.openDoors)
                    DrawLabel(dl, textX, above, "doors open", IM_COL32(255, 170, 70, 235));
                if (ent.beingRaided)
                    DrawLabel(dl, textX, above, "RAID", IM_COL32(255, 70, 70, 235));
                if (ent.raidLocked)
                    DrawLabel(dl, textX, above, "raid lock", IM_COL32(255, 140, 80, 235));
                if (ent.decoderCount > 0)
                    DrawLabel(dl, textX, above, "decoder", IM_COL32(180, 140, 255, 235));
                if (ent.hasLoot)
                    DrawLabel(dl, textX, above, "loot", IM_COL32(120, 220, 140, 235));
                if (ent.maxDefensePoints > 0) {
                    char fuse[48];
                    snprintf(fuse, sizeof(fuse), "fuse %d/%d", ent.defensePoints, ent.maxDefensePoints);
                    DrawLabel(dl, textX, above, fuse, IM_COL32(90, 200, 255, 235));
                }
                if (!ent.clan.empty())
                    DrawLabel(dl, textX, above, ent.clan.c_str(), IM_COL32(160, 220, 255, 235));
            }
        }

        if (config.drawDistance) {
            char distBuf[32];
            snprintf(distBuf, sizeof(distBuf), "%.0fm", dist);
            DrawLabelDown(dl, textX, below, distBuf, white);
        }
        if (ent.cash > 0 && config.showItemValues) {
            char money[32];
            snprintf(money, sizeof(money), "$%d", ent.cash);
            DrawLabelDown(dl, textX, below, money, IM_COL32(120, 220, 140, 235));
        }

        if (ent.type == EntityType::Player && config.drawNames) {
            if (ent.hidingInProp || ent.isHidden)
                DrawLabelDown(dl, textX, below, "Hidden", IM_COL32(210, 160, 255, 235));
            if (!ent.inVehicle && ent.isCrouching)
                DrawLabelDown(dl, textX, below, "Crouch", white);
            else if (!ent.inVehicle && ent.isSprinting)
                DrawLabelDown(dl, textX, below, "Sprint", white);
            if (ent.inVehicle)
                DrawLabelDown(dl, textX, below, "In vehicle", blue);
            if (ent.hasArmor && !config.drawArmor) {
                const char* armor = "Armor";
                ImFont* font = EspFont();
                const float size = EspFontSize();
                ImVec2 sz = font->CalcTextSizeA(size, 1e9f, 0.f, armor);
                float ax = haveBox ? boxRight + 18.f : textX + 36.f;
                float ay = haveBox ? (boxTop + boxBottom) * 0.5f - sz.y * 0.5f : below;
                dl->AddText(font, size, ImVec2(ax + 1.f, ay + 1.f), IM_COL32(0, 0, 0, 200), armor);
                dl->AddText(font, size, ImVec2(ax, ay), blue, armor);
            }
        }
    }

}
