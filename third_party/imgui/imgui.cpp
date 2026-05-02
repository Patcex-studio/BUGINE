#include "imgui.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace ImGui {

static IO g_io;
static Style g_style;
static ImGuiContext g_context;
static ImDrawList g_foreground_draw_list;
static ImDrawList g_window_draw_list;
static ImGuiViewport g_main_viewport;
static ImGuiContext* g_current_context = nullptr;

void IMGUI_CHECKVERSION() {}

ImGuiContext* CreateContext(void* /*shared_font_atlas*/) {
    g_current_context = &g_context;
    return g_current_context;
}

void DestroyContext(ImGuiContext* ctx) {
    if (ctx == &g_context) {
        g_current_context = nullptr;
    }
}

ImGuiContext* GetCurrentContext() {
    return g_current_context;
}

IO& GetIO() {
    return g_io;
}

Style& GetStyle() {
    return g_style;
}

void StyleColorsDark() {
    // No-op in stub backend.
}

void NewFrame() {
}

void EndFrame() {
}

void Render() {
}

void* GetDrawData() {
    return nullptr;
}

bool Begin(const char* /*name*/, bool* /*p_open*/, ImGuiWindowFlags /*flags*/) {
    return true;
}

bool Begin(const char* name) {
    return Begin(name, nullptr, ImGuiWindowFlags_None);
}

void End() {
}

bool BeginChild(const char* /*str_id*/, const ImVec2& /*size*/, bool /*border*/) {
    return true;
}

void EndChild() {
}

void Text(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
}

void TextColored(const ImVec4& /*col*/, const char* fmt, ...) {
    Text(fmt, nullptr);
}

void TextUnformatted(const char* /*text*/) {
}

void SameLine(float /*offset_from_start_x*/, float /*spacing*/) {
}

bool InputText(const char* /*label*/, std::string* /*str*/, int /*flags*/) {
    return false;
}

bool InputInt(const char* /*label*/, int* /*v*/) {
    return false;
}

bool InputFloat(const char* /*label*/, float* /*v*/, float /*step*/, float /*step_fast*/, const char* /*format*/) {
    return false;
}

bool Checkbox(const char* /*label*/, bool* /*v*/) {
    return false;
}

bool Button(const char* /*label*/) {
    return false;
}

bool SliderFloat(const char* /*label*/, float* /*v*/, float /*v_min*/, float /*v_max*/) {
    return false;
}

bool ListBox(const char* /*label*/, int* /*current_item*/, const char* const /*items*/[], int /*items_count*/) {
    return false;
}

bool BeginPopupContextItem(const char* /*str_id*/) {
    return false;
}

bool MenuItem(const char* /*label*/, const char* /*shortcut*/, bool /*selected*/, bool /*enabled*/) {
    return false;
}

void EndPopup() {
}

bool Selectable(const char* /*label*/, bool /*selected*/) {
    return false;
}

bool IsMouseDoubleClicked(int /*button*/) {
    return false;
}

void PushID(int /*id*/) {
}

void PushID(const char* /*str_id*/) {
}

void PopID() {
}

bool TreeNode(void* /*ptr_id*/, const char* /*fmt*/, ...) {
    return false;
}

void TreePop() {
}

ImDrawList* GetWindowDrawList() {
    return &g_window_draw_list;
}

ImDrawList* GetForegroundDrawList() {
    return &g_foreground_draw_list;
}

ImGuiViewport* GetMainViewport() {
    return &g_main_viewport;
}

ImVec2 GetCursorScreenPos() {
    return ImVec2(0.0f, 0.0f);
}

void SetNextWindowPos(const ImVec2& /*pos*/, ImGuiCond /*cond*/) {
}

void SetNextWindowSize(const ImVec2& /*size*/, ImGuiCond /*cond*/) {
}

void SetNextWindowBgAlpha(float /*alpha*/) {
}

ImU32 ColorConvertFloat4ToU32(const ImVec4& col) {
    uint32_t r = static_cast<uint32_t>(col.x * 255.0f) & 0xFF;
    uint32_t g = static_cast<uint32_t>(col.y * 255.0f) & 0xFF;
    uint32_t b = static_cast<uint32_t>(col.z * 255.0f) & 0xFF;
    uint32_t a = static_cast<uint32_t>(col.w * 255.0f) & 0xFF;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

ImU32 GetColorU32(const ImVec4& col) {
    return ColorConvertFloat4ToU32(col);
}

float GetScrollY() {
    return 0.0f;
}

float GetScrollMaxY() {
    return 0.0f;
}

void SetScrollHereY(float /*center_y_ratio*/) {
}

float GetFrameHeightWithSpacing() {
    return 1.0f;
}

void PlotLines(const char* /*label*/, const float* /*values*/, int /*values_count*/, int /*values_offset*/, const char* /*overlay_text*/, float /*scale_min*/, float /*scale_max*/, const ImVec2& /*graph_size*/) {
}

} // namespace ImGui
