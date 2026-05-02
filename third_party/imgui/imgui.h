#pragma once

#include <cstdint>
#include <cstdarg>
#include <string>
#include <vector>

#define IM_COL32(R, G, B, A) (((uint32_t)(A) << 24) | ((uint32_t)(B) << 16) | ((uint32_t)(G) << 8) | ((uint32_t)(R)))

struct ImVec2 {
    float x, y;
    ImVec2() : x(0.0f), y(0.0f) {}
    ImVec2(float _x, float _y) : x(_x), y(_y) {}
};

struct ImVec4 {
    float x, y, z, w;
    ImVec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    ImVec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

struct ImDrawList {
    void AddCircleFilled(const ImVec2&, float, uint32_t, int = 12, float = 1.0f) {}
    void AddRectFilled(const ImVec2&, const ImVec2&, uint32_t, float = 0.0f, int = 0) {}
    void AddRect(const ImVec2&, const ImVec2&, uint32_t, float = 0.0f, int = 0, float = 1.0f) {}
    void AddCircle(const ImVec2&, float, uint32_t, int = 12, float = 1.0f) {}
    void AddLine(const ImVec2&, const ImVec2&, uint32_t, float = 1.0f) {}
};

struct ImGuiViewport {
    ImVec2 GetCenter() const { return ImVec2(0.0f, 0.0f); }
};

using ImU32 = uint32_t;
using ImTextureID = void*;

enum ImGuiCol_ {
    ImGuiCol_Text,
    ImGuiCol_WindowBg,
    ImGuiCol_FrameBg,
    ImGuiCol_Header,
    ImGuiCol_HeaderHovered,
    ImGuiCol_HeaderActive,
};

enum ImGuiMouseButton_ {
    ImGuiMouseButton_Left = 0,
    ImGuiMouseButton_Right = 1,
    ImGuiMouseButton_Middle = 2,
};

enum ImGuiWindowFlags_ {
    ImGuiWindowFlags_None = 0,
    ImGuiWindowFlags_NoTitleBar = 1 << 0,
    ImGuiWindowFlags_NoResize = 1 << 1,
    ImGuiWindowFlags_NoScrollbar = 1 << 2,
    ImGuiWindowFlags_NoBackground = 1 << 3,
    ImGuiWindowFlags_NoCollapse = 1 << 4,
    ImGuiWindowFlags_NoFocusOnAppearing = 1 << 5,
    ImGuiWindowFlags_NoMove = 1 << 6,
};
using ImGuiWindowFlags = int;

enum ImGuiCond_ {
    ImGuiCond_Always = 1 << 0,
};
using ImGuiCond = int;

enum ImGuiInputTextFlags_ {
    ImGuiInputTextFlags_None = 0,
    ImGuiInputTextFlags_EnterReturnsTrue = 1 << 0,
};
using ImGuiInputTextFlags = int;

enum ImGuiSelectableFlags_ {
    ImGuiSelectableFlags_None = 0,
};
using ImGuiSelectableFlags = int;

enum ImGuiTreeNodeFlags_ {
    ImGuiTreeNodeFlags_DefaultOpen = 1 << 0,
};
using ImGuiTreeNodeFlags = int;

enum ImGuiConfigFlags_ {
    ImGuiConfigFlags_None = 0,
    ImGuiConfigFlags_NavEnableKeyboard = 1 << 0,
    ImGuiConfigFlags_DockingEnable = 1 << 1,
    ImGuiConfigFlags_ViewportsEnable = 1 << 2,
};
using ImGuiConfigFlags = int;

namespace ImGui {

struct ImGuiContext {
};

struct ImGuiStyle {
    std::vector<ImVec4> Colors;
};
using Style = ImGuiStyle;

struct IO {
    ImVec2 DisplaySize = ImVec2(0.0f, 0.0f);
    float Framerate = 0.0f;
    bool WantCaptureMouse = false;
    bool WantCaptureKeyboard = false;
    int ConfigFlags = 0;
};

void IMGUI_CHECKVERSION();
ImGuiContext* CreateContext(void* shared_font_atlas = nullptr);
void DestroyContext(ImGuiContext* ctx = nullptr);
ImGuiContext* GetCurrentContext();
IO& GetIO();
Style& GetStyle();
void StyleColorsDark();
void NewFrame();
void EndFrame();
void Render();
void* GetDrawData();

bool Begin(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = ImGuiWindowFlags_None);
bool Begin(const char* name);
void End();

bool BeginChild(const char* str_id, const ImVec2& size = ImVec2(0, 0), bool border = false);
void EndChild();

void Text(const char* fmt, ...);
void TextColored(const ImVec4& col, const char* fmt, ...);
void TextUnformatted(const char* text);
void TextDisabled(const char* fmt, ...);

void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f);

bool InputText(const char* label, std::string* str, int flags = 0);
bool InputInt(const char* label, int* v);
bool InputFloat(const char* label, float* v, float step = 0.0f, float step_fast = 0.0f, const char* format = "%f");
bool Checkbox(const char* label, bool* v);
bool Button(const char* label);
bool SliderFloat(const char* label, float* v, float v_min, float v_max);
bool ListBox(const char* label, int* current_item, const char* const items[], int items_count);

bool BeginPopupContextItem(const char* str_id = nullptr);
bool MenuItem(const char* label, const char* shortcut = nullptr, bool selected = false, bool enabled = true);
void EndPopup();

bool Selectable(const char* label, bool selected = false, ImGuiSelectableFlags flags = ImGuiSelectableFlags_None);
bool IsMouseDoubleClicked(int button);

void PushID(int id);
void PushID(const char* str_id);
void PopID();

bool TreeNode(void* ptr_id, const char* fmt, ...);
void TreePop();

ImDrawList* GetWindowDrawList();
ImDrawList* GetForegroundDrawList();
ImGuiViewport* GetMainViewport();
ImVec2 GetCursorScreenPos();

void SetNextWindowPos(const ImVec2& pos, ImGuiCond cond = ImGuiCond_Always);
void SetNextWindowSize(const ImVec2& size, ImGuiCond cond = ImGuiCond_Always);
void SetNextWindowBgAlpha(float alpha);

ImU32 ColorConvertFloat4ToU32(const ImVec4& in);
ImU32 GetColorU32(const ImVec4& in);

float GetScrollY();
float GetScrollMaxY();
void SetScrollHereY(float center_y_ratio = 0.5f);

float GetFrameHeightWithSpacing();

void PlotLines(const char* label, const float* values, int values_count, int values_offset = 0, const char* overlay_text = nullptr, float scale_min = 0.0f, float scale_max = 0.0f, const ImVec2& graph_size = ImVec2(0.0f, 0.0f));

} // namespace ImGui
