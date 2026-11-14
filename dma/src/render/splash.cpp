#define NOMINMAX
#include "splash.h"
#include "core/memory.h"

#include "imgui.h"

#include <Windows.h>
#include <wincodec.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace {

struct GifFrame {
    std::vector<uint8_t> bgra;
    int delayMs = 42;
};

std::mutex g_mu;
std::string g_stage = "Starting overlay";
std::string g_detail;
float g_progress = 0.05f;
std::atomic<bool> g_ready{false};
std::chrono::steady_clock::time_point g_readyAt{};
float g_fade = 1.f;

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
ID3D11Texture2D* g_tex = nullptr;
ID3D11ShaderResourceView* g_srv = nullptr;
std::vector<GifFrame> g_frames;
int g_videoW = 0;
int g_videoH = 0;
int g_frame = 0;
std::chrono::steady_clock::time_point g_lastFrame{};

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring path(buf);
    const auto slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        path.resize(slash);
    return path;
}

bool FileExists(const std::wstring& path) {
    const DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring FindGif() {
    const std::wstring dir = ExeDir();
    const std::wstring names[] = {
        dir + L"\\assets\\loading_animation.gif",
        dir + L"\\loading_animation.gif",
    };
    for (const auto& p : names) {
        if (FileExists(p))
            return p;
    }
    return {};
}

void ReleaseGpu() {
    if (g_srv) {
        g_srv->Release();
        g_srv = nullptr;
    }
    if (g_tex) {
        g_tex->Release();
        g_tex = nullptr;
    }
}

bool CreateTexture(int w, int h) {
    if (!g_device || w <= 0 || h <= 0)
        return false;
    ReleaseGpu();

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_device->CreateTexture2D(&td, nullptr, &g_tex)) || !g_tex)
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = td.Format;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MipLevels = 1;
    if (FAILED(g_device->CreateShaderResourceView(g_tex, &sd, &g_srv)) || !g_srv)
        return false;
    g_videoW = w;
    g_videoH = h;
    return true;
}

UINT MetaU(IWICMetadataQueryReader* meta, const wchar_t* name, UINT fallback) {
    if (!meta)
        return fallback;
    PROPVARIANT value;
    PropVariantInit(&value);
    UINT out = fallback;
    if (SUCCEEDED(meta->GetMetadataByName(name, &value))) {
        if (value.vt == VT_UI1)
            out = value.bVal;
        else if (value.vt == VT_UI2)
            out = value.uiVal;
        else if (value.vt == VT_UI4)
            out = value.ulVal;
    }
    PropVariantClear(&value);
    return out;
}

void BlitBgra(std::vector<uint8_t>& canvas, UINT cw, UINT ch,
              const uint8_t* src, UINT fw, UINT fh, UINT left, UINT top) {
    if (!src || canvas.empty() || !fw || !fh)
        return;
    for (UINT y = 0; y < fh; ++y) {
        const UINT dy = top + y;
        if (dy >= ch)
            break;
        for (UINT x = 0; x < fw; ++x) {
            const UINT dx = left + x;
            if (dx >= cw)
                break;
            const uint8_t* p = src + (static_cast<size_t>(y) * fw + x) * 4;
            if (p[3] == 0)
                continue;
            uint8_t* d = canvas.data() + (static_cast<size_t>(dy) * cw + dx) * 4;
            d[0] = p[0];
            d[1] = p[1];
            d[2] = p[2];
            d[3] = 255;
        }
    }
}

bool UploadFrame(int index) {
    if (!g_tex || !g_context || g_frames.empty() || index < 0 ||
        index >= static_cast<int>(g_frames.size()))
        return false;
    const auto& fr = g_frames[static_cast<size_t>(index)];
    const size_t expected = static_cast<size_t>(g_videoW) * static_cast<size_t>(g_videoH) * 4;
    if (fr.bgra.size() != expected)
        return false;
    ID3D11ShaderResourceView* none = nullptr;
    g_context->PSSetShaderResources(0, 1, &none);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(g_context->Map(g_tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)) || !mapped.pData)
        return false;
    const UINT srcPitch = static_cast<UINT>(g_videoW * 4);
    auto* dst = static_cast<uint8_t*>(mapped.pData);
    const uint8_t* src = fr.bgra.data();
    if (mapped.RowPitch == srcPitch) {
        memcpy(dst, src, expected);
    } else {
        for (int y = 0; y < g_videoH; ++y)
            memcpy(dst + static_cast<size_t>(y) * mapped.RowPitch,
                   src + static_cast<size_t>(y) * srcPitch, srcPitch);
    }
    g_context->Unmap(g_tex, 0);
    return true;
}

bool LoadGif() {
    const std::wstring path = FindGif();
    if (path.empty()) {
        std::cerr << "[-] Splash GIF missing (assets/loading_animation.gif)\n";
        return false;
    }

    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory))) ||
        !factory) {
        std::cerr << "[-] Splash WIC factory failed\n";
        return false;
    }

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) {
        factory->Release();
        std::cerr << "[-] Splash GIF decode failed\n";
        return false;
    }

    UINT count = 0;
    decoder->GetFrameCount(&count);
    if (count == 0) {
        decoder->Release();
        factory->Release();
        return false;
    }

    g_frames.clear();
    g_frames.reserve(count);
    UINT w = 0, h = 0;
    std::vector<uint8_t> canvas;
    std::vector<uint8_t> backup;
    for (UINT i = 0; i < count; ++i) {
        IWICBitmapFrameDecode* frame = nullptr;
        if (FAILED(decoder->GetFrame(i, &frame)) || !frame)
            continue;
        IWICFormatConverter* converter = nullptr;
        factory->CreateFormatConverter(&converter);
        if (!converter ||
            FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                                         WICBitmapDitherTypeNone, nullptr, 0.0,
                                         WICBitmapPaletteTypeCustom))) {
            if (converter)
                converter->Release();
            frame->Release();
            continue;
        }
        UINT fw = 0, fh = 0;
        converter->GetSize(&fw, &fh);
        if (!fw || !fh) {
            converter->Release();
            frame->Release();
            continue;
        }
        if (!w) {
            w = fw;
            h = fh;
            canvas.assign(static_cast<size_t>(w) * h * 4, 0);
        }
        IWICMetadataQueryReader* meta = nullptr;
        frame->GetMetadataQueryReader(&meta);
        const UINT left = MetaU(meta, L"/imgdesc/Left", 0);
        const UINT top = MetaU(meta, L"/imgdesc/Top", 0);
        const UINT delayTicks = MetaU(meta, L"/grctlext/Delay", 4);
        const UINT disposal = MetaU(meta, L"/grctlext/Disposal", 0);
        if (meta)
            meta->Release();

        std::vector<uint8_t> src(static_cast<size_t>(fw) * fh * 4);
        if (FAILED(converter->CopyPixels(nullptr, fw * 4, static_cast<UINT>(src.size()), src.data()))) {
            converter->Release();
            frame->Release();
            continue;
        }
        converter->Release();
        frame->Release();

        if (disposal == 3)
            backup = canvas;
        BlitBgra(canvas, w, h, src.data(), fw, fh, left, top);

        GifFrame out;
        out.delayMs = delayTicks > 0 ? static_cast<int>(delayTicks) * 10 : 42;
        if (out.delayMs < 20)
            out.delayMs = 20;
        out.bgra = canvas;
        g_frames.push_back(std::move(out));

        if (disposal == 2) {
            for (UINT y = 0; y < fh && top + y < h; ++y) {
                for (UINT x = 0; x < fw && left + x < w; ++x) {
                    uint8_t* d = canvas.data() + (static_cast<size_t>(top + y) * w + (left + x)) * 4;
                    d[0] = d[1] = d[2] = d[3] = 0;
                }
            }
        } else if (disposal == 3 && !backup.empty()) {
            canvas = backup;
        }
    }
    decoder->Release();
    factory->Release();

    if (g_frames.empty() || !CreateTexture(static_cast<int>(w), static_cast<int>(h))) {
        g_frames.clear();
        std::cerr << "[-] Splash GIF upload failed\n";
        return false;
    }
    g_frame = 0;
    UploadFrame(0);
    g_lastFrame = std::chrono::steady_clock::now();
    std::cout << "[+] Splash GIF " << w << "x" << h << " frames=" << g_frames.size() << "\n";
    return true;
}

void PumpFrame() {
    if (g_frames.size() < 2 || !g_tex || !g_context)
        return;
    const auto now = std::chrono::steady_clock::now();
    const int delay = g_frames[static_cast<size_t>(g_frame)].delayMs;
    if (g_lastFrame.time_since_epoch().count() != 0 &&
        now - g_lastFrame < std::chrono::milliseconds(delay))
        return;
    g_lastFrame = now;
    g_frame = (g_frame + 1) % static_cast<int>(g_frames.size());
    UploadFrame(g_frame);
}

} // namespace

namespace Splash {

void SetStage(const char* text, float progress) {
    if (!text || !*text)
        return;
    std::lock_guard<std::mutex> lock(g_mu);
    g_stage = text;
    if (progress >= 0.f)
        g_progress = std::clamp(progress, 0.f, 1.f);
}

void SetDetail(const char* text) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_detail = text ? text : "";
}

std::string Detail() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_detail;
}

void SetProgress(float progress) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_progress = std::clamp(progress, 0.f, 1.f);
}

std::string Stage() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_stage;
}

float Progress() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_progress;
}

bool Init(ID3D11Device* device, ID3D11DeviceContext* context) {
    g_device = device;
    g_context = context;
    g_ready = false;
    g_fade = 1.f;
    SetStage("Starting overlay", 0.05f);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (!LoadGif())
        std::cerr << "[-] Splash will show status text only\n";
    return true;
}

void Shutdown() {
    ReleaseGpu();
    g_frames.clear();
    g_device = nullptr;
    g_context = nullptr;
}

void MarkReady() {
    if (g_ready.exchange(true))
        return;
    SetStage("Ready", 1.f);
    g_readyAt = std::chrono::steady_clock::now();
}

void MarkBusy() {
    g_ready = false;
    g_fade = 1.f;
}

bool Covering() {
    if (!g_ready.load())
        return true;
    return g_fade > 0.02f;
}

void Tick() {
    auto& mem = Memory::Get();
    if (g_ready.load() && (!mem.IsConnected() || !mem.GameAssemblyBase()))
        MarkBusy();

    if (g_ready.load()) {
        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::chrono::duration<float>(now - g_readyAt).count();
        g_fade = std::clamp(1.f - elapsed / 0.12f, 0.f, 1.f);
    } else {
        g_fade = 1.f;
    }

    if (Covering())
        PumpFrame();
}

void Render() {
    if (!Covering())
        return;

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    const float alpha = g_fade;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(disp);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, alpha));
    ImGui::Begin("##boot_splash", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoMove);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(0, 0), disp, IM_COL32(0, 0, 0, static_cast<int>(255 * alpha)));

    if (g_srv && g_videoW > 0 && g_videoH > 0 && !g_frames.empty()) {
        // Clip the KlingAI watermark and the GIF's own bar. We draw one bar.
        constexpr float kCropBottom = 0.58f;
        const float videoAspect =
            static_cast<float>(g_videoW) / (static_cast<float>(g_videoH) * kCropBottom);
        const float maxW = disp.x;
        const float maxH = disp.y * 0.72f;
        ImVec2 size(maxW, maxW / videoAspect);
        if (size.y > maxH) {
            size.y = maxH;
            size.x = maxH * videoAspect;
        }
        const ImVec2 pos((disp.x - size.x) * 0.5f, std::max(0.f, (maxH - size.y) * 0.5f));
        const ImTextureRef tex((ImTextureID)(intptr_t)g_srv);
        dl->AddImage(tex, pos, ImVec2(pos.x + size.x, pos.y + size.y),
                     ImVec2(0, 0), ImVec2(1.f, kCropBottom),
                     IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)));
    }

    const std::string stage = Stage();
    const std::string detail = Detail();
    const float progress = Progress();
    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize() * 1.15f;
    const ImVec2 textSize = font
        ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, stage.c_str())
        : ImVec2(0, 0);
    const ImVec2 textPos((disp.x - textSize.x) * 0.5f, disp.y * 0.78f);
    if (font) {
        dl->AddText(font, fontSize, ImVec2(textPos.x + 1.f, textPos.y + 1.f),
                    IM_COL32(0, 0, 0, static_cast<int>(180 * alpha)), stage.c_str());
        dl->AddText(font, fontSize, textPos,
                    IM_COL32(235, 242, 248, static_cast<int>(255 * alpha)), stage.c_str());
    }
    float detailBottom = textPos.y + textSize.y;
    if (font && !detail.empty()) {
        const float detailSize = ImGui::GetFontSize() * 0.92f;
        const ImVec2 detailSizePx = font->CalcTextSizeA(detailSize, disp.x * 0.86f, disp.x * 0.86f, detail.c_str());
        const ImVec2 detailPos((disp.x - std::min(detailSizePx.x, disp.x * 0.86f)) * 0.5f,
                               textPos.y + textSize.y + 8.f);
        dl->AddText(font, detailSize, detailPos,
                    IM_COL32(186, 214, 232, static_cast<int>(255 * alpha)),
                    detail.c_str(), nullptr, disp.x * 0.86f);
        detailBottom = detailPos.y + detailSizePx.y;
    }

    const float barW = std::min(disp.x * 0.42f, 520.f);
    const float barH = 8.f;
    const ImVec2 bar0((disp.x - barW) * 0.5f, detailBottom + 16.f);
    const ImVec2 bar1(bar0.x + barW, bar0.y + barH);
    dl->AddRectFilled(bar0, bar1, IM_COL32(12, 18, 24, static_cast<int>(220 * alpha)), 6.f);
    dl->AddRect(bar0, bar1, IM_COL32(90, 170, 220, static_cast<int>(220 * alpha)), 6.f, 0, 1.5f);
    if (progress > 0.f) {
        const float fill = bar0.x + barW * std::clamp(progress, 0.02f, 1.f);
        dl->AddRectFilled(bar0, ImVec2(fill, bar1.y),
                          IM_COL32(90, 170, 220, static_cast<int>(230 * alpha)), 6.f);
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

} // namespace Splash
