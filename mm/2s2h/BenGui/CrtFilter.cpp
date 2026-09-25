#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include <fast/Fast3dWindow.h>
#include <fast/interpreter.h>
#include "2s2h/BenPort.h"
#include "2s2h/BenGui/BenMenu.h"
#include "2s2h/BenGui/BenGui.hpp"

/*  Console Variables are grouped under gCrtFilter (CVAR_PREFIX_CRT_FILTER).

    The following cvars are used in Libultraship and can be edited here:
        - Enabled         - Runs the finished game image through a RetroArch shader preset (via librashader).
        - Preset          - Path of the .slangp preset, relative to the app directory.
        - Lines           - Vertical render resolution while the filter is active.
        - AspectMode      - 0 = 4:3, 1 = 16:9, 2 = match the window.
        - IntegerScale    - Display the image at an integer multiple of Lines, so every scanline is equally thick.
        - Params.<name>   - Overrides for the runtime parameters of the preset.
        - Hdr.Enabled     - Presents in HDR (scRGB) while the display is in HDR mode. DirectX 11 only.
        - Hdr.CrtNits     - Brightness of white in the filtered image, makes up for the light lost to scanlines.
        - Hdr.PaperWhiteNits - Brightness of white for the menus and for the game image while the filter is off.

    While the filter is active Libultraship ignores the internal resolution, advanced resolution and N64 mode
    settings. They apply again, untouched, as soon as the filter is turned off.
*/

#define CVAR_CRT_FILTER(var) CVAR_PREFIX_CRT_FILTER "." var

namespace BenGui {
extern std::shared_ptr<BenMenu> mBenMenu;
using namespace UIWidgets;

namespace {
struct CrtPreset {
    const char* label;
    const char* path;
};

const std::vector<CrtPreset> crtPresets = {
    { "2Ship: Composite, soft beam", "shaders/2ship-crt.slangp" },
    { "2Ship: S-Video, clean signal (recommended)", "shaders/2ship-crt-svideo.slangp" },
    { "2Ship: RGB monitor, sharpest", "shaders/2ship-crt-rgb.slangp" },
    { "2Ship: Living room TV (curved, Trinitron colours)", "shaders/2ship-crt-tv.slangp" },
    { "2Ship: Light scanlines (brighter in SDR)", "shaders/2ship-crt-light.slangp" },
    { "Guest Advanced NTSC (composite blend)", "shaders/crt/crt-guest-advanced-ntsc.slangp" },
    { "Guest Advanced (sharp RGB)", "shaders/crt/crt-guest-advanced.slangp" },
    { "Guest Advanced Fast", "shaders/crt/crt-guest-advanced-fast.slangp" },
    { "CRT Royale", "shaders/crt/crt-royale.slangp" },
    { "CRT Geom", "shaders/crt/crt-geom.slangp" },
    { "CRT Lottes", "shaders/crt/crt-lottes.slangp" },
    { "CRT Easymode", "shaders/crt/crt-easymode.slangp" },
    { "CRT Hyllian", "shaders/crt/crt-hyllian.slangp" },
    { "CRT Aperture", "shaders/crt/crt-aperture.slangp" },
    { "CRT Consumer", "shaders/crt/crt-consumer.slangp" },
};
std::vector<const char*> crtPresetLabels;

const std::vector<int32_t> crtLineCounts = { 240, 288, 360, 480, 540, 720 };
std::vector<const char*> crtLineOptions = {
    "240 lines (N64 native) - 3x at 720p, 4x at 1080p, 6x at 1440p, 9x at 4K",
    "288 lines - 5x at 1440p",
    "360 lines - 2x at 720p, 3x at 1080p, 4x at 1440p, 6x at 4K",
    "480 lines - 3x at 1440p",
    "540 lines - 2x at 1080p, 4x at 4K",
    "720 lines - 3x at 4K",
};
std::vector<const char*> crtAspectOptions = { "4:3 (authentic)", "16:9", "Match window" };

std::weak_ptr<Fast::Interpreter> crtInterpreter;
char paramSearch[64] = "";

bool ContainsIgnoreCase(const std::string& text, const std::string& search) {
    auto it = std::search(text.begin(), text.end(), search.begin(), search.end(),
                          [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
    return it != text.end();
}

void ApplySelectedPreset() {
    int32_t index = CVarGetInteger(CVAR_CRT_FILTER("PresetIndex"), 0);
    if (index < 0 || index >= (int32_t)crtPresets.size()) {
        index = 0;
    }
    CVarSetString(CVAR_CRT_FILTER("Preset"), crtPresets[index].path);
}

void ApplySelectedLines() {
    int32_t index = CVarGetInteger(CVAR_CRT_FILTER("LinesIndex"), 0);
    if (index < 0 || index >= (int32_t)crtLineCounts.size()) {
        index = 0;
    }
    CVarSetInteger(CVAR_CRT_FILTER("Lines"), crtLineCounts[index]);
}

void DrawCrtFilterStatus(std::shared_ptr<Fast::Interpreter> interpreter) {
    if (!CVarGetInteger(CVAR_CRT_FILTER("Enabled"), 0)) {
        return;
    }

    if (interpreter->IsPostFilterActive()) {
        ImGui::TextColored(ImVec4(0.0f, 0.85f, 0.55f, 1.0f), "Active: rendering %u x %u, displayed at %u x %u",
                           interpreter->mCurDimensions.width, interpreter->mCurDimensions.height,
                           interpreter->mPostFilterOutputWidth, interpreter->mPostFilterOutputHeight);
    } else if (!interpreter->GetPostFilterError().empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.0f, 0.0f, 1.0f));
        ImGui::TextWrapped("Could not load the shader preset:\n%s", interpreter->GetPostFilterError().c_str());
        ImGui::PopStyleColor();
    }

    if (ImGui::Button("Reload Shader")) {
        interpreter->ReloadPostFilter();
    }
}

void DrawCrtFilterParams(std::shared_ptr<Fast::Interpreter> interpreter) {
    if (!interpreter->IsPostFilterActive()) {
        return;
    }

    std::vector<Fast::PostFilterParam> params = interpreter->GetPostFilterParams();
    if (params.empty() || !ImGui::CollapsingHeader("Shader Parameters")) {
        return;
    }

    bool changed = false;
    if (ImGui::Button("Reset All Parameters")) {
        for (const Fast::PostFilterParam& param : params) {
            interpreter->SetPostFilterParam(param.name, param.initial);
            CVarClear((std::string(CVAR_CRT_FILTER("Params.")) + param.name).c_str());
        }
        changed = true;
    }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##CrtParamSearch", "Search parameters...", paramSearch, sizeof(paramSearch));

    ImGui::BeginChild("CrtFilterParams", ImVec2(0, 0), ImGuiChildFlags_None);
    for (const Fast::PostFilterParam& param : params) {
        const std::string& label = param.description.empty() ? param.name : param.description;
        if (param.minimum >= param.maximum) {
            // Presets use parameters without a range as section headings
            if (paramSearch[0] == '\0') {
                ImGui::SeparatorText(label.c_str());
            }
            continue;
        }
        if (paramSearch[0] != '\0' && !ContainsIgnoreCase(label, paramSearch) &&
            !ContainsIgnoreCase(param.name, paramSearch)) {
            continue;
        }

        float value = param.initial;
        interpreter->GetPostFilterParam(param.name, value);

        ImGui::PushID(param.name.c_str());
        ImGui::TextUnformatted(label.c_str());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 3.0f);
        if (ImGui::SliderFloat("##value", &value, param.minimum, param.maximum, "%.3f",
                               ImGuiSliderFlags_AlwaysClamp)) {
            if (param.step > 0.0f) {
                value = param.minimum + roundf((value - param.minimum) / param.step) * param.step;
            }
            interpreter->SetPostFilterParam(param.name, value);
            CVarSetFloat((std::string(CVAR_CRT_FILTER("Params.")) + param.name).c_str(), value);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            interpreter->SetPostFilterParam(param.name, param.initial);
            CVarClear((std::string(CVAR_CRT_FILTER("Params.")) + param.name).c_str());
            changed = true;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (changed) {
        Ship::Context::GetRawInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
    }
}
} // namespace

void RegisterCrtFilterWidgets() {
    auto fastWnd = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
    crtInterpreter = fastWnd->GetInterpreterWeak();

    for (const CrtPreset& preset : crtPresets) {
        crtPresetLabels.push_back(preset.label);
    }
    // Libultraship only knows the preset path, keep it in sync with the selected entry
    ApplySelectedPreset();
    ApplySelectedLines();

    WidgetPath path = { "Settings", "Graphics", SECTION_COLUMN_3 };

    mBenMenu->AddWidget(path, "CRT Filter", WIDGET_SEPARATOR_TEXT);
    mBenMenu->AddWidget(path, "Enable CRT Filter", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_CRT_FILTER("Enabled"))
        .Options(CheckboxOptions().Tooltip(
            "Simulates a CRT television: scanlines, a soft analog beam instead of square pixels, composite video "
            "blending, bloom and CRT gamma.\n\n"
            "While enabled the game renders at the line count of the simulated TV, overriding the internal "
            "resolution, advanced resolution and N64 mode settings. They apply again when the filter is turned "
            "off.\n\n"
            "Anti-aliasing (MSAA) is still applied and is recommended, the N64 anti-aliased its image as well."
#if defined(__SWITCH__)
            "\n\nThe shaders are built into the game. The heavier presets (CRT Royale, Guest Advanced NTSC) may not "
            "hold 60 FPS docked; Guest Advanced Fast and the 2Ship presets at 240 lines are the lightest."
#elif defined(_WIN32)
            "\n\nWorks with the OpenGL renderer, and with DirectX 11 when librashader.dll sits next to the "
            "executable. The shaders folder must be next to the executable."
#else
            "\n\nWorks with the OpenGL renderer. The shaders folder must be next to the executable."
#endif
            ));
    mBenMenu->AddWidget(path, "Shader Preset", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_CRT_FILTER("PresetIndex"))
        .Callback([](WidgetInfo& info) { ApplySelectedPreset(); })
        .Options(ComboboxOptions()
                     .ComboVec(&crtPresetLabels)
                     .Tooltip("RetroArch shader preset used to draw the image. Loading a preset for the first time "
                              "can take a few seconds while its shaders are compiled."));
    mBenMenu->AddWidget(path, "Vertical Resolution", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_CRT_FILTER("LinesIndex"))
        .Callback([](WidgetInfo& info) { ApplySelectedLines(); })
        .Options(ComboboxOptions().ComboVec(&crtLineOptions).Tooltip(
            "Number of lines the game is rendered with. 240 lines is what the N64 sent to the TV. More lines give a "
            "sharper image with finer scanlines. Pick a count that divides the height of your display evenly, "
            "and keep at least 4 pixels per line (the scale factor) for the beam to keep its soft shape."));
    mBenMenu->AddWidget(path, "Aspect Ratio", WIDGET_CVAR_COMBOBOX)
        .CVar(CVAR_CRT_FILTER("AspectMode"))
        .Options(ComboboxOptions().ComboVec(&crtAspectOptions).Tooltip("Shape of the simulated screen."));
    mBenMenu->AddWidget(path, "Integer Scaling", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_CRT_FILTER("IntegerScale"))
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Displays the image at a whole multiple of its line count so every scanline is equally thick. On a "
            "1440p display 240 lines scale by exactly 6."));
#ifdef _WIN32
    // HDR output is a DXGI swap chain feature, only the Windows build has it
    mBenMenu->AddWidget(path, "HDR Output", WIDGET_SEPARATOR_TEXT);
    mBenMenu->AddWidget(path, "Enable HDR Output", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_CRT_FILTER("Hdr.Enabled"))
        .Options(CheckboxOptions().Tooltip(
            "Presents the game in HDR so the CRT image can be shown brighter than SDR white, making up for the light "
            "lost to the dark gaps between scanlines.\n\n"
            "Requires the DirectX 11 renderer and HDR to be turned on in the Windows display settings (Win+Alt+B)."));
    mBenMenu->AddWidget(path, "CRT Brightness: %.0f nits", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_CRT_FILTER("Hdr.CrtNits"))
        .Options(FloatSliderOptions().Min(100.0f).Max(1000.0f).Step(10.0f).DefaultValue(500.0f).Format("").Tooltip(
            "Brightness of white in the CRT filtered image. Around half of the screen is dark scanline gaps, so "
            "roughly twice the brightness of a normal SDR image looks equally bright."));
    mBenMenu->AddWidget(path, "Menu Brightness: %.0f nits", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar(CVAR_CRT_FILTER("Hdr.PaperWhiteNits"))
        .Options(FloatSliderOptions().Min(80.0f).Max(500.0f).Step(10.0f).DefaultValue(200.0f).Format("").Tooltip(
            "Brightness of white for the menus, and for the game image while the CRT filter is turned off."));
    mBenMenu->AddWidget(path, "HdrStatus", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        auto interpreter = crtInterpreter.lock();
        if (!interpreter || !CVarGetInteger(CVAR_CRT_FILTER("Hdr.Enabled"), 0)) {
            return;
        }
        if (interpreter->GetCurrentRenderingAPI()->IsHdrOutputActive()) {
            ImGui::TextColored(ImVec4(0.0f, 0.85f, 0.55f, 1.0f), "HDR output is active");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.0f, 1.0f));
            ImGui::TextWrapped("Waiting for an HDR display. Turn on HDR in the Windows display settings "
                               "(Win+Alt+B) and use the DirectX 11 renderer.");
            ImGui::PopStyleColor();
        }
    });
#endif
    mBenMenu->AddWidget(path, "CrtFilterCustom", WIDGET_CUSTOM).CustomFunction([](WidgetInfo& info) {
        auto interpreter = crtInterpreter.lock();
        if (!interpreter) {
            return;
        }
        DrawCrtFilterStatus(interpreter);
        DrawCrtFilterParams(interpreter);
    });
}

static RegisterMenuInitFunc initFunc(RegisterCrtFilterWidgets);

} // namespace BenGui
