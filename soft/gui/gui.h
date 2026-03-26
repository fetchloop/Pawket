#ifndef _PAWKET_GUI
#define _PAWKET_GUI

#include "ext/imgui.h"
#include "ext/imgui_impl_win32.h"
#include "ext/imgui_impl_dx11.h"

#include <d3d11.h>
#include <tchar.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <chrono>

#include "../util/list.h"
#include "../util/time.h"
#include "../handler.h"
#include "../util/io.h"

// DX11 state
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static bool g_Minimized = false;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const char* protocol_str(Pawket::Packet::Protocol proto)
{
    switch (proto)
    {
    case Pawket::Packet::Protocol::TCP: return "TCP";
    case Pawket::Packet::Protocol::UDP: return "UDP";
    case Pawket::Packet::Protocol::ICMP: return "ICMP";
    case Pawket::Packet::Protocol::IGMP: return "IGMP";
    case Pawket::Packet::Protocol::SCTP: return "SCTP";
    case Pawket::Packet::Protocol::OTHER: return "OTHER";
    default: return "?";
    }
}

static bool icontains(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return true;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char a, char b) {
            return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
        }
    );
    return it != haystack.end();
}

static std::string packet_search_key(const Pawket::PACKET& p)
{
    char src[INET_ADDRSTRLEN]{}, dst[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &p.source.addr, src, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &p.destination.addr, dst, INET_ADDRSTRLEN);

    std::string key;
    key.reserve(96);
    key += src; key += ':'; key += std::to_string(p.source.port); key += ' ';
    key += dst; key += ':'; key += std::to_string(p.destination.port); key += ' ';
    key += protocol_str(p.protocol); key += ' ';
    key += (p.direction == Pawket::Packet::Direction::INCOMING ? "incoming" : "outgoing"); key += ' ';
    key += Pawket::Time::get_string_timestamp(p.timestamp);
    return key;
}

static const Pawket::Packet::Protocol proto_order[] = {
    Pawket::Packet::Protocol::TCP,
    Pawket::Packet::Protocol::UDP,
    Pawket::Packet::Protocol::ICMP,
    Pawket::Packet::Protocol::IGMP,
    Pawket::Packet::Protocol::SCTP,
    Pawket::Packet::Protocol::OTHER,
};
static constexpr int PROTO_ORDER_LEN = 6;
static int proto_sort_idx = -1;

static void cycle_proto_sort()
{
    ++proto_sort_idx;
    if (proto_sort_idx >= PROTO_ORDER_LEN)
        proto_sort_idx = -1;
}

static int proto_sort_key(Pawket::Packet::Protocol p)
{
    if (proto_sort_idx == -1) return 0;
    return (p == proto_order[proto_sort_idx]) ? 0 : 1;
}

static const char* proto_header_label()
{
    if (proto_sort_idx == -1) return "Protocol";
    static char buf[32];
    snprintf(buf, sizeof(buf), "Protocol: %s ^", protocol_str(proto_order[proto_sort_idx]));
    return buf;
}

static bool dir_sort_active = false;
static bool dir_sort_ascending = true;
static void click_dir_sort()
{
    if (dir_sort_active)
        dir_sort_ascending = !dir_sort_ascending;
    else
    {
        dir_sort_active = true;
        dir_sort_ascending = true;
    }
}

static const char* dir_header_label()
{
    if (!dir_sort_active) return "Direction";
    return dir_sort_ascending ? "Direction ^" : "Direction v";
}

static int dir_sort_key(Pawket::Packet::Direction dir)
{
    switch (dir)
    {
    case Pawket::Packet::Direction::INCOMING: return 0;
    case Pawket::Packet::Direction::OUTGOING: return 1;
    default: return 2;
    }
}

namespace Pawket
{
    namespace GUI
    {
        void render_loop()
        {
            ImGui_ImplWin32_EnableDpiAwareness();
            float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(
                ::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

            WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
                GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"Pawket", nullptr };
            ::RegisterClassExW(&wc);
            HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Pawket", WS_OVERLAPPEDWINDOW,
                100, 100, (int)(800 * main_scale), (int)(600 * main_scale),
                nullptr, nullptr, wc.hInstance, nullptr);

            if (!CreateDeviceD3D(hwnd))
            {
                CleanupDeviceD3D();
                ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
                return;
            }

            ::ShowWindow(hwnd, SW_SHOWDEFAULT);
            ::UpdateWindow(hwnd);

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

            // Apply theme from config
            if (Config::config.dark_mode)
                ImGui::StyleColorsDark();
            else
                ImGui::StyleColorsLight();

            ImGuiStyle& style = ImGui::GetStyle();
            style.ScaleAllSizes(main_scale);
            style.FontScaleDpi = main_scale;

            ImGui_ImplWin32_Init(hwnd);
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

            // Set clear color to match theme
            ImVec4 clear_color = Config::config.dark_mode
                ? ImVec4(0.1f, 0.1f, 0.1f, 1.0f)
                : ImVec4(0.9f, 0.9f, 0.9f, 1.0f);

            // UI state
            bool recording = true;
            int selected_packet = -1;
            bool show_config = false;
            bool show_export = false;
            float inspect_height = 200.0f;

            // inner widths for hex and ascii panes, updated each frame.
            float hex_inner_width = 0.0f;
            float asc_inner_width = 0.0f;

            std::vector<PACKET> display_list;
            size_t last_seen{ 0 };

            // Search
            char search_buf[128]{};
            std::string search_str;

            // View indices into display_list
            std::vector<int> view_indices;
            bool needs_reindex = true;
            bool needs_resort = false;

            // Rolling packet rate graph
            static constexpr int RATE_HISTORY = 60;
            float rate_history[RATE_HISTORY]{};
            int   rate_history_offset = 0;
            float rate_last_second = 0.0f;
            auto  rate_last_tick = std::chrono::steady_clock::now();

            bool done = false;
            while (!done)
            {
                MSG msg;
                while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);
                    if (msg.message == WM_QUIT) done = true;
                }
                if (done) break;

                if (g_Minimized) { ::Sleep(10); continue; }

                if (g_SwapChainOccluded &&
                    g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
                {
                    ::Sleep(10);
                    continue;
                }
                g_SwapChainOccluded = false;

                if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
                {
                    CleanupRenderTarget();
                    g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
                    g_ResizeWidth = g_ResizeHeight = 0;
                    CreateRenderTarget();
                }

                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                // Track display list size before sync to measure new packets
                size_t prev_display_size = display_list.size();

                // Sync shared packet list
                {
                    std::lock_guard<std::mutex> lock(Pawket::Packet::List::list_mutex);
                    if (Pawket::Packet::List::packet_list.size() > last_seen)
                    {
                        display_list.insert(
                            display_list.end(),
                            Pawket::Packet::List::packet_list.begin() + last_seen,
                            Pawket::Packet::List::packet_list.end()
                        );
                        last_seen = Pawket::Packet::List::packet_list.size();
                        needs_reindex = true;
                    }
                }

                // Accumulate new packets into the current rate bucket
                rate_last_second += (float)(display_list.size() - prev_display_size);

                // Cap to MAX_PACKETS
                if ((int)display_list.size() > Config::config.MAX_PACKETS)
                {
                    int excess = (int)display_list.size() - Config::config.MAX_PACKETS;
                    display_list.erase(display_list.begin(), display_list.begin() + excess);
                    last_seen = Pawket::Packet::List::packet_list.size();
                    if (selected_packet >= 0)
                    {
                        selected_packet -= excess;
                        if (selected_packet < 0) selected_packet = -1;
                    }
                    needs_reindex = true;
                }

                // Rebuild index from display_list + search
                if (needs_reindex)
                {
                    view_indices.clear();
                    view_indices.reserve(display_list.size());
                    for (int i = 0; i < (int)display_list.size(); i++)
                    {
                        if (!search_str.empty() && !icontains(packet_search_key(display_list[i]), search_str))
                            continue;
                        view_indices.push_back(i);
                    }
                    needs_reindex = false;
                    needs_resort = true;
                }

                // Apply sort
                if (needs_resort)
                {
                    std::stable_sort(view_indices.begin(), view_indices.end(),
                        [&](int a, int b)
                        {
                            const PACKET& pa = display_list[a];
                            const PACKET& pb = display_list[b];

                            if (proto_sort_idx != -1)
                            {
                                int ka = proto_sort_key(pa.protocol);
                                int kb = proto_sort_key(pb.protocol);
                                if (ka != kb) return ka < kb;
                            }

                            if (dir_sort_active)
                            {
                                int ka = dir_sort_key(pa.direction);
                                int kb = dir_sort_key(pb.direction);
                                if (ka != kb) return dir_sort_ascending ? ka < kb : ka > kb;
                            }

                            return false;
                        });
                    needs_resort = false;
                }

                // Advance the rate bucket once per second
                auto now_tick = std::chrono::steady_clock::now();
                float elapsed = std::chrono::duration<float>(now_tick - rate_last_tick).count();
                if (elapsed >= 1.0f)
                {
                    rate_history[rate_history_offset] = rate_last_second;
                    rate_history_offset = (rate_history_offset + 1) % RATE_HISTORY;
                    rate_last_second = 0.0f;
                    rate_last_tick = now_tick;
                }

                // Main window
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(io.DisplaySize);
                ImGui::Begin("Pawket", nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoBringToFrontOnFocus
                );

                // Toolbar
                if (recording)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, Config::config.dark_mode
                        ? ImVec4(0.6f, 0.1f, 0.1f, 1.0f)
                        : ImVec4(0.7f, 0.15f, 0.15f, 0.7f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Config::config.dark_mode
                        ? ImVec4(0.75f, 0.15f, 0.15f, 1.0f)
                        : ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Config::config.dark_mode
                        ? ImVec4(0.5f, 0.05f, 0.05f, 1.0f)
                        : ImVec4(0.6f, 0.1f, 0.1f, 0.9f));
                    if (ImGui::Button("Stop Recording"))
                    {
                        recording = false;
                        Handler::capture_running = false;
                    }
                    ImGui::PopStyleColor(3);
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, Config::config.dark_mode
                        ? ImVec4(0.1f, 0.5f, 0.1f, 1.0f)
                        : ImVec4(0.15f, 0.55f, 0.15f, 0.7f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Config::config.dark_mode
                        ? ImVec4(0.15f, 0.65f, 0.15f, 1.0f)
                        : ImVec4(0.2f, 0.65f, 0.2f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Config::config.dark_mode
                        ? ImVec4(0.05f, 0.4f, 0.05f, 1.0f)
                        : ImVec4(0.1f, 0.45f, 0.1f, 0.9f));
                    if (ImGui::Button("Start Recording"))
                    {
                        Handler::capture_running = false;
                        if (Handler::capture_thread.joinable())
                            Handler::capture_thread.join();

                        // Recreate the socket to discard any buffered packets from the OS
                        Handler::setup_socket();

                        {
                            std::lock_guard<std::mutex> lock(Pawket::Packet::List::list_mutex);
                            Pawket::Packet::List::packet_list.clear();
                            last_seen = 0;
                        }

                        display_list.clear();
                        view_indices.clear();
                        selected_packet = -1;
                        needs_reindex = true;
                        needs_resort = false;

                        recording = true;
                        Handler::capture_running = true;
                        Handler::setup_capture_thread();
                    }
                    ImGui::PopStyleColor(3);
                }

                ImGui::SameLine();
                if (ImGui::Button("Config"))
                    show_config = !show_config;

                ImGui::SameLine();
                if (ImGui::Button("Export"))
                    show_export = !show_export;

                ImGui::SameLine();
                if (ImGui::Button("Import PCAP"))
                {
                    // Stop recording and join the thread before importing
                    Handler::capture_running = false;
                    if (Handler::capture_thread.joinable())
                        Handler::capture_thread.join();

                    {
                        std::lock_guard<std::mutex> lock(Pawket::Packet::List::list_mutex);
                        Pawket::Packet::List::packet_list.clear();
                        last_seen = 0;
                    }

                    display_list.clear();
                    view_indices.clear();
                    selected_packet = -1;
                    needs_reindex = true;
                    needs_resort = false;
                    recording = false;

                    // Reset the rate graph so imported data doesnt mess it up
                    std::fill(rate_history, rate_history + RATE_HISTORY, 0.0f);
                    rate_history_offset = 0;
                    rate_last_second = 0.0f;
                    rate_last_tick = std::chrono::steady_clock::now();

                    Pawket::Pcap::import(hwnd);
                }

                ImGui::SameLine();
                ImGui::Text("Packets: %d / %d  |  Visible: %d",
                    (int)display_list.size(), Config::config.MAX_PACKETS,
                    (int)view_indices.size());
                ImGui::Separator();

                // Config panel
                if (show_config)
                {
                    ImGui::Text("Filter:");
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Any", Config::config.filter == Config::FilterType::ANY)) Config::config.filter = Config::FilterType::ANY;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Incoming", Config::config.filter == Config::FilterType::INCOMING)) Config::config.filter = Config::FilterType::INCOMING;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Outgoing", Config::config.filter == Config::FilterType::OUTGOING)) Config::config.filter = Config::FilterType::OUTGOING;

                    ImGui::Text("Debug:");
                    ImGui::SameLine();
                    if (ImGui::RadioButton("On", Config::config.debug == TRUE)) Config::config.debug = !Config::config.debug;

                    ImGui::Text("Theme:");
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Dark", Config::config.dark_mode))
                    {
                        Config::config.dark_mode = true;
                        ImGui::StyleColorsDark();
                        clear_color = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Light", !Config::config.dark_mode))
                    {
                        Config::config.dark_mode = false;
                        ImGui::StyleColorsLight();
                        clear_color = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
                    }

                    ImGui::Text("Max Packets:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(120);
                    ImGui::InputInt("##maxpackets", &Config::config.MAX_PACKETS);
                    if (Config::config.MAX_PACKETS < 1) Config::config.MAX_PACKETS = 1;
                    ImGui::Separator();
                }

                // Export panel
                if (show_export)
                {
                    if (ImGui::Button("Export PCAP"))
                    {
                        Pawket::Pcap::export_pcap(display_list);
                        show_export = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Export JSON"))
                    {
                        Pawket::Pcap::export_json(display_list);
                        show_export = false;
                    }
                    ImGui::Separator();
                }

                // Search bar
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::InputTextWithHint("##search", "Search by IP, port, protocol, direction, time...",
                    search_buf, sizeof(search_buf)))
                {
                    search_str = search_buf;
                    needs_reindex = true;
                }
                ImGui::Separator();

                // Packet table
                float status_bar_height = ImGui::GetFrameHeightWithSpacing();
                float min_inspect = 80.0f;
                float max_inspect = io.DisplaySize.y * 0.6f;
                float table_height = ImGui::GetContentRegionAvail().y - (selected_packet >= 0 ? inspect_height + 6.0f : 0.0f) - status_bar_height - 68.0f;

                if (ImGui::BeginTable("Packets", 4,
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY,
                    ImVec2(0, table_height)))
                {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Source");
                    ImGui::TableSetupColumn("Destination");
                    ImGui::TableSetupColumn(proto_header_label());
                    ImGui::TableSetupColumn(dir_header_label());

                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TableHeader("Source");

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TableHeader("Destination");

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TableHeader(proto_header_label());
                    if (ImGui::IsItemClicked())
                    {
                        cycle_proto_sort();
                        needs_resort = true;
                    }

                    ImGui::TableSetColumnIndex(3);
                    ImGui::TableHeader(dir_header_label());
                    if (ImGui::IsItemClicked())
                    {
                        click_dir_sort();
                        needs_resort = true;
                    }

                    for (int vi = 0; vi < (int)view_indices.size(); vi++)
                    {
                        int i = view_indices[vi];
                        PACKET& p = display_list[i];

                        char source_str[INET_ADDRSTRLEN]{};
                        char dest_str[INET_ADDRSTRLEN]{};
                        inet_ntop(AF_INET, &p.source.addr, source_str, INET_ADDRSTRLEN);
                        inet_ntop(AF_INET, &p.destination.addr, dest_str, INET_ADDRSTRLEN);

                        ImGui::TableNextRow();

                        if (selected_packet != i)
                        {
                            ImVec4 row_col{};
                            switch (p.protocol)
                            {
                            case Pawket::Packet::Protocol::TCP:
                                row_col = (p.direction == Pawket::Packet::Direction::INCOMING)
                                    ? ImVec4(0.05f, 0.25f, 0.2f, 0.4f)
                                    : ImVec4(0.05f, 0.15f, 0.3f, 0.4f);
                                break;
                            case Pawket::Packet::Protocol::UDP:
                                row_col = (p.direction == Pawket::Packet::Direction::INCOMING)
                                    ? ImVec4(0.3f, 0.15f, 0.05f, 0.4f)
                                    : ImVec4(0.3f, 0.25f, 0.05f, 0.4f);
                                break;
                            case Pawket::Packet::Protocol::ICMP:
                                row_col = ImVec4(0.25f, 0.05f, 0.25f, 0.4f);
                                break;
                            case Pawket::Packet::Protocol::IGMP:
                                row_col = ImVec4(0.05f, 0.25f, 0.05f, 0.4f);
                                break;
                            case Pawket::Packet::Protocol::SCTP:
                                row_col = ImVec4(0.2f, 0.2f, 0.05f, 0.4f);
                                break;
                            default:
                                row_col = ImVec4(0.15f, 0.15f, 0.15f, 0.4f);
                                break;
                            }
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(row_col));
                        }

                        if (selected_packet == i)
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                ImGui::GetColorU32(ImVec4(0.2f, 0.4f, 0.6f, 1.0f)));

                        ImGui::TableSetColumnIndex(0);
                        char label[64];
                        snprintf(label, sizeof(label), "%s:%d##row%d", source_str, p.source.port, i);
                        if (ImGui::Selectable(label, selected_packet == i, ImGuiSelectableFlags_SpanAllColumns))
                            selected_packet = (selected_packet == i) ? -1 : i;

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s:%d", dest_str, p.destination.port);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%s", protocol_str(p.protocol));
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%s", p.direction == Pawket::Packet::Direction::INCOMING ? "Incoming" : "Outgoing");
                    }

                    ImGui::EndTable();
                }

                // Splitter
                if (selected_packet >= 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                    ImGui::Button("##splitter", ImVec2(-1, 4.0f));
                    ImGui::PopStyleColor(3);

                    if (ImGui::IsItemHovered())
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

                    if (ImGui::IsItemActive())
                    {
                        inspect_height -= ImGui::GetIO().MouseDelta.y;
                        if (inspect_height < min_inspect) inspect_height = min_inspect;
                        if (inspect_height > max_inspect) inspect_height = max_inspect;
                    }
                }

                // Inspect panel
                if (selected_packet >= 0 && selected_packet < (int)display_list.size())
                {
                    PACKET& p = display_list[selected_packet];

                    char source_str[INET_ADDRSTRLEN]{};
                    char dest_str[INET_ADDRSTRLEN]{};
                    inet_ntop(AF_INET, &p.source.addr, source_str, INET_ADDRSTRLEN);
                    inet_ntop(AF_INET, &p.destination.addr, dest_str, INET_ADDRSTRLEN);

                    ImGui::Text("Packet #%d  |  %s:%d  ->  %s:%d  |  %s  |  %s  |  %d bytes  |  %s",
                        selected_packet,
                        source_str, p.source.port,
                        dest_str, p.destination.port,
                        protocol_str(p.protocol),
                        p.direction == Pawket::Packet::Direction::INCOMING ? "Incoming" : "Outgoing",
                        (int)p.length,
                        Pawket::Time::get_string_timestamp(p.timestamp).c_str()
                    );

                    float available_width = ImGui::GetContentRegionAvail().x;
                    float spacing = ImGui::GetStyle().ItemSpacing.x;
                    float hex_width = (available_width - spacing) * 0.65f;
                    float ascii_width = (available_width - spacing) * 0.35f;

                    // Every pane calculates its own bytes_per_row by itself.
                    float hex_char_w = ImGui::CalcTextSize("XX ").x;
                    float asc_char_w = ImGui::CalcTextSize("X").x;
                    int bytes_per_row_hex = (std::max)(1, (int)((hex_inner_width > 0.0f ? hex_inner_width - hex_char_w * 0.5f : 8.0f) / hex_char_w));
                    int bytes_per_row_asc = (std::max)(1, (int)(asc_inner_width > 0.0f ? asc_inner_width / asc_char_w : 8));

                    std::vector<char> hex_buf(bytes_per_row_hex * 3 + 1);
                    std::vector<char> asc_buf(bytes_per_row_asc + 1);

                    ImGui::BeginChild("HexPane",
                        ImVec2(hex_width, inspect_height - ImGui::GetFrameHeightWithSpacing() * 2), true
                    );

                    // Update cached inner width for next frame.
                    hex_inner_width = ImGui::GetContentRegionAvail().x;

                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetIO().Fonts->Fonts[0]);

                    const char* payload_start = p.raw.data() + p.offset;
                    size_t size = p.length;
                    for (size_t row = 0; row < size; row += bytes_per_row_hex)
                    {
                        size_t cols = (row + bytes_per_row_hex <= size) ? bytes_per_row_hex : (size - row);

                        int hex_pos = 0;
                        for (size_t col = 0; col < (size_t)bytes_per_row_hex; col++)
                        {
                            if (col < cols)
                                hex_pos += snprintf(hex_buf.data() + hex_pos, hex_buf.size() - hex_pos, "%02X ", (BYTE)payload_start[row + col]);
                            else
                                hex_pos += snprintf(hex_buf.data() + hex_pos, hex_buf.size() - hex_pos, "   ");
                        }

                        ImGui::TextUnformatted(hex_buf.data());
                    }

                    ImGui::PopFont();
                    ImGui::EndChild();

                    ImGui::SameLine();

                    ImGui::BeginChild("AsciiPane",
                        ImVec2(ascii_width, inspect_height - ImGui::GetFrameHeightWithSpacing() * 2), true
                    );

                    // Update cached inner width for next frame.
                    asc_inner_width = ImGui::GetContentRegionAvail().x;

                    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetIO().Fonts->Fonts[0]);

                    for (size_t row = 0; row < size; row += bytes_per_row_asc)
                    {
                        size_t cols = (row + bytes_per_row_asc <= size) ? bytes_per_row_asc : (size - row);

                        for (size_t col = 0; col < cols; col++)
                            asc_buf[col] = std::isprint((unsigned char)payload_start[row + col]) ? payload_start[row + col] : '.';
                        asc_buf[cols] = '\0';

                        ImGui::TextUnformatted(asc_buf.data());
                    }

                    ImGui::PopFont();
                    ImGui::EndChild();
                }

                // Packet rate graph
                {
                    float max_rate = *std::max_element(rate_history, rate_history + RATE_HISTORY);
                    char rate_overlay[32];
                    snprintf(rate_overlay, sizeof(rate_overlay), "%.0f pkt/s",
                        rate_history[(rate_history_offset - 1 + RATE_HISTORY) % RATE_HISTORY]);

                    ImGui::Separator();
                    ImGui::PlotHistogram(
                        "##packetrate",
                        rate_history,
                        RATE_HISTORY,
                        rate_history_offset,
                        rate_overlay,
                        0.0f,
                        max_rate + 1.0f,
                        ImVec2(-1.0f, 50.0f)
                    );
                }

                // Status bar
                ImGui::Separator();
                ImGui::Text(recording ? "Recording..." : "Stopped.");

                ImGui::End();

                ImGui::Render();
                const float cc[4] = {
                    clear_color.x * clear_color.w,
                    clear_color.y * clear_color.w,
                    clear_color.z * clear_color.w,
                    clear_color.w
                };
                g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
                g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, cc);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

                HRESULT hr = g_pSwapChain->Present(1, 0);
                g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
            }

            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();

            CleanupDeviceD3D();
            ::DestroyWindow(hwnd);
            ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        }
    }
}

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
            &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) { g_Minimized = true; return 0; }
        g_Minimized = false;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

#endif