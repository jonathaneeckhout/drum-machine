#include "engine.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
namespace {
constexpr std::array<const char*, 4> names{"KICK", "SNARE", "HI-HAT", "CLAP"};
constexpr std::array<const char*, 4> files{"kick.wav", "snare.wav", "hat.wav", "clap.wav"};
constexpr std::array<const char*, 4> descriptions{"LOW / ROUND", "BODY / SNAP", "SHORT / BRIGHT", "WIDE / TEXTURED"};
constexpr ImU32 background = IM_COL32(19, 22, 25, 255);
constexpr ImU32 panel = IM_COL32(27, 31, 35, 255);
constexpr ImU32 border = IM_COL32(48, 54, 60, 255);
constexpr ImU32 ink = IM_COL32(233, 238, 235, 255);
constexpr ImU32 muted = IM_COL32(137, 149, 151, 255);
constexpr ImU32 mint = IM_COL32(167, 234, 195, 255);
constexpr std::array<ImU32, 4> colors{
    mint, IM_COL32(241, 186, 128, 255), IM_COL32(147, 185, 224, 255), IM_COL32(197, 170, 220, 255)};

void require(bool ok, const char* context) {
    if (!ok) throw std::runtime_error(std::string(context) + ": " + SDL_GetError());
}

std::array<drum::Sample, 4> loadSamples(const fs::path& assets) {
    std::array<drum::Sample, 4> samples;
    const SDL_AudioSpec target{SDL_AUDIO_F32, 2, drum::sampleRate};
    for (int i = 0; i < 4; ++i) {
        const auto path = assets / "samples" / files[i];
        SDL_AudioSpec source{};
        Uint8* bytes = nullptr;
        Uint32 length = 0;
        require(SDL_LoadWAV(path.c_str(), &source, &bytes, &length), path.c_str());
        std::unique_ptr<Uint8, decltype(&SDL_free)> sourceOwner(bytes, SDL_free);
        Uint8* converted = nullptr;
        int convertedLength = 0;
        require(SDL_ConvertAudioSamples(&source, bytes, static_cast<int>(length), &target,
                                       &converted, &convertedLength), "Convert sample");
        std::unique_ptr<Uint8, decltype(&SDL_free)> convertedOwner(converted, SDL_free);
        samples[i].pcm.resize(static_cast<std::size_t>(convertedLength) / sizeof(float));
        std::memcpy(samples[i].pcm.data(), converted, static_cast<std::size_t>(convertedLength));
    }
    return samples;
}

struct Audio {
    drum::Engine& engine;
    SDL_AudioStream* stream = nullptr;
    std::atomic<bool> failed{false};
    std::array<float, 1024> scratch{};
    explicit Audio(drum::Engine& engineRef) : engine(engineRef) {
        const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, drum::sampleRate};
        stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, callback, this);
        require(stream != nullptr, "Open audio output");
        if (!SDL_ResumeAudioStreamDevice(stream)) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
            require(false, "Start audio output");
        }
    }
    ~Audio() { if (stream) SDL_DestroyAudioStream(stream); }
    static void SDLCALL callback(void* userdata, SDL_AudioStream* stream, int additional, int) {
        auto& audio = *static_cast<Audio*>(userdata);
        constexpr int bytesPerFrame = 2 * sizeof(float);
        int frames = (std::max(additional, 0) + bytesPerFrame - 1) / bytesPerFrame;
        while (frames > 0) {
            const int count = std::min(frames, 512);
            audio.engine.render(audio.scratch.data(), static_cast<std::size_t>(count));
            if (!SDL_PutAudioStreamData(stream, audio.scratch.data(), count * bytesPerFrame)) {
                audio.failed.store(true);
                return;
            }
            frames -= count;
        }
    }
};

void writeDemo(const fs::path& path, drum::Engine& engine, drum::Controls& controls) {
    controls.playing.store(true);
    constexpr int frames = drum::sampleRate * 4; // Two bars at 120 BPM.
    std::vector<float> pcm(frames * 2);
    engine.render(pcm.data(), frames);
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write " + path.string());
    const auto u16 = [&out](unsigned value) { out.put(value & 255); out.put((value >> 8) & 255); };
    const auto u32 = [&u16](unsigned value) { u16(value & 65535); u16(value >> 16); };
    out.write("RIFF", 4); u32(36 + frames * 4); out.write("WAVEfmt ", 8);
    u32(16); u16(1); u16(2); u32(drum::sampleRate); u32(drum::sampleRate * 4);
    u16(4); u16(16); out.write("data", 4); u32(frames * 4);
    for (float value : pcm) u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lrint(value * 32767))));
    if (!out) throw std::runtime_error("Failed writing " + path.string());
}

void text(ImDrawList* draw, float x, float y, const char* label, float size = 16, ImU32 color = ink) {
    ImGui::PushFont(nullptr, size);
    draw->AddText({x, y}, color, label);
    ImGui::PopFont();
}

bool hit(const char* id, float x, float y, float width, float height) {
    ImGui::SetCursorScreenPos({x, y});
    return ImGui::InvisibleButton(id, {width, height});
}

bool button(ImDrawList* draw, const char* id, float x, float y, float width,
            float height, const char* label, bool filled = false) {
    const bool pressed = hit(id, x, y, width, height);
    const bool hovered = ImGui::IsItemHovered();
    draw->AddRectFilled({x, y}, {x + width, y + height},
        filled ? (hovered ? IM_COL32(190, 249, 214, 255) : mint) :
        (hovered ? IM_COL32(53, 60, 65, 255) : IM_COL32(36, 41, 46, 255)), 7);
    if (!filled) draw->AddRect({x, y}, {x + width, y + height}, border, 7);
    ImGui::PushFont(nullptr, 16);
    const ImVec2 extent = ImGui::CalcTextSize(label);
    draw->AddText({x + (width - extent.x) / 2, y + (height - extent.y) / 2},
                  filled ? background : ink, label);
    ImGui::PopFont();
    if (ImGui::IsItemFocused()) draw->AddRect({x - 2, y - 2}, {x + width + 2, y + height + 2}, mint, 8);
    return pressed;
}

void tooltip(const char* value) {
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", value);
}

struct Interface {
    std::array<float, 4> previousVolumes{0.75f, 0.75f, 0.75f, 0.75f};
    float meter = 0.0f;
    std::string deviceName;

    void draw(drum::Controls& controls, bool audioFailed) {
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::Begin("drum-machine", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleVar();
        auto* draw = ImGui::GetWindowDrawList();
        const float ox = viewport->Pos.x, oy = viewport->Pos.y;
        const float width = viewport->Size.x;
        const float x = ox + 36, y = oy + 30, w = width - 72;
        draw->AddRectFilled(viewport->Pos, {ox + width, oy + viewport->Size.y}, background);

        // Wordmark and compact device identity.
        for (int i = 0; i < 4; ++i) {
            const float height = std::array<float, 4>{15, 29, 22, 11}[i];
            draw->AddRectFilled({x + i * 7, y + 30 - height}, {x + i * 7 + 4, y + 30}, mint, 2);
        }
        text(draw, x + 40, y - 3, "drum-machine", 28);
        text(draw, x, y + 45, "A SMALL MACHINE FOR BIG GROOVES", 13, muted);
        draw->AddCircleFilled({x + w - 135, y + 15}, 3, audioFailed ? colors[1] : mint);
        text(draw, x + w - 123, y + 7, audioFailed ? "AUDIO ERROR" : "SAMPLE ENGINE", 13, muted);
        text(draw, x + w - 137, y + 29, "4 VOICES / 16 STEPS", 12, muted);

        const float ty = y + 90;
        draw->AddRectFilled({x, ty}, {x + w, ty + 100}, panel, 12);
        draw->AddRect({x, ty}, {x + w, ty + 100}, border, 12);
        bool playing = controls.playing.load();
        if (button(draw, "transport", x + 20, ty + 22, 136, 56, playing ? "STOP" : "PLAY", true))
            controls.playing.store(!playing);
        tooltip("Start / stop [Space]. Stop lets current drum hits decay.");
        if (playing) {
            draw->AddRectFilled({x + 39, ty + 44}, {x + 49, ty + 54}, background, 1);
        } else {
            draw->AddTriangleFilled({x + 39, ty + 43}, {x + 39, ty + 57}, {x + 49, ty + 50}, background);
        }
        text(draw, x + 181, ty + 16, "TEMPO", 12, muted);
        float bpm = static_cast<float>(controls.bpm.load());
        ImGui::SetCursorScreenPos({x + 179, ty + 35});
        ImGui::SetNextItemWidth(102);
        ImGui::PushFont(nullptr, 30);
        if (ImGui::DragFloat("##tempo", &bpm, 0.25f, 40, 240, "%.1f", ImGuiSliderFlags_AlwaysClamp))
            controls.bpm.store(bpm);
        ImGui::PopFont();
        tooltip("Drag to set tempo. Ctrl+click to type. 40-240 BPM.");
        text(draw, x + 291, ty + 48, "BPM", 12, muted);
        draw->AddLine({x + 347, ty + 22}, {x + 347, ty + 78}, border);
        text(draw, x + 372, ty + 21, playing ? "SEQUENCING" : "READY WHEN YOU ARE", 13, playing ? mint : ink);
        text(draw, x + 372, ty + 48, "01 PATTERN  /  4/4  /  1/16", 13, muted);
        if (button(draw, "clear", x + w - 213, ty + 31, 83, 38, "Clear"))
            for (auto& pattern : controls.patterns) pattern.store(0);
        tooltip("Clear all steps. Tempo and levels are preserved.");
        if (button(draw, "demo", x + w - 117, ty + 31, 97, 38, "Demo")) {
            drum::Controls demo;
            for (int i = 0; i < 4; ++i) controls.patterns[i].store(demo.patterns[i].load());
        }
        tooltip("Restore the starter groove.");

        const float sy = ty + 129;
        text(draw, x, sy, "STEP SEQUENCER", 15);
        text(draw, x + w - 237, sy + 2, "CLICK A STEP. MAKE IT YOURS.", 12, muted);

        const float gy = sy + 41;
        const float gx = x + 210;
        const float gap = 6;
        const float cell = (w - 230 - gap * 15) / 16;
        const int active = controls.currentStep.load();
        for (int step = 0; step < 16; ++step) {
            const float cx = gx + step * (cell + gap);
            char label[8]; std::snprintf(label, sizeof(label), "%02d", step + 1);
            text(draw, cx + cell / 2 - 7, gy, label, 12, active == step ? mint : muted);
            if (active == step) draw->AddCircleFilled({cx + cell / 2, gy - 9}, 2, mint);
        }
        text(draw, x + 17, gy, "INSTRUMENT / LEVEL", 11, muted);

        for (int track = 0; track < 4; ++track) {
            ImGui::PushID(track);
            const float ry = gy + 27 + track * 85;
            draw->AddRectFilled({x, ry}, {x + w, ry + 76}, panel, 8);
            draw->AddRectFilled({x, ry + 15}, {x + 3, ry + 61}, colors[track], 2);
            if (hit("preview", x + 16, ry + 9, 130, 35)) controls.previewRequests.fetch_or(1u << track);
            tooltip("Click to audition. Keyboard: 1 / 2 / 3 / 4.");
            text(draw, x + 17, ry + 10, names[track], 18, ImGui::IsItemHovered() ? colors[track] : ink);
            text(draw, x + 17, ry + 33, descriptions[track], 10, muted);
            float volume = controls.volumes[track].load();
            ImGui::SetCursorScreenPos({x + 15, ry + 51});
            ImGui::SetNextItemWidth(121);
            if (ImGui::SliderFloat("##level", &volume, 0, 1, "", ImGuiSliderFlags_AlwaysClamp))
                controls.volumes[track].store(volume);
            tooltip("Instrument volume");
            char level[8]; std::snprintf(level, sizeof(level), "%d%%", static_cast<int>(volume * 100 + 0.5f));
            text(draw, x + 144, ry + 52, level, 11, muted);
            if (hit("mute", x + 173, ry + 12, 24, 24)) {
                if (volume > 0) { previousVolumes[track] = volume; controls.volumes[track].store(0); }
                else controls.volumes[track].store(previousVolumes[track]);
            }
            tooltip("Mute / restore this instrument");
            draw->AddRect({x + 173, ry + 12}, {x + 197, ry + 36}, volume == 0 ? colors[track] : border, 4);
            text(draw, x + 180, ry + 17, "M", 12, volume == 0 ? colors[track] : muted);

            auto pattern = controls.patterns[track].load();
            for (int step = 0; step < 16; ++step) {
                ImGui::PushID(step);
                const float cx = gx + step * (cell + gap);
                const float cy = ry + 14;
                const bool on = (pattern & (1u << step)) != 0;
                const bool clicked = hit("step", cx, cy, cell, 48);
                const bool hovered = ImGui::IsItemHovered();
                if (clicked) controls.patterns[track].fetch_xor(static_cast<std::uint16_t>(1u << step));
                const ImU32 off = step % 4 == 0 ? IM_COL32(48, 55, 60, 255) : IM_COL32(38, 44, 49, 255);
                draw->AddRectFilled({cx, cy}, {cx + cell, cy + 48}, on ? colors[track] : off, 5);
                if (hovered || ImGui::IsItemFocused())
                    draw->AddRect({cx - 1, cy - 1}, {cx + cell + 1, cy + 49}, ink, 6, 0, 1.5f);
                if (active == step)
                    draw->AddRect({cx - 2, cy - 2}, {cx + cell + 2, cy + 50}, colors[track], 7, 0, 2);
                draw->AddCircleFilled({cx + cell / 2, cy + 24}, 2.5f, on ? IM_COL32(38, 47, 43, 180) : IM_COL32(82, 91, 96, 255));
                if (hovered) ImGui::SetTooltip("%s / step %d / %s", names[track], step + 1, on ? "on" : "off");
                ImGui::PopID();
            }
            ImGui::PopID();
        }

        const float fy = gy + 27 + 4 * 85 + 13;
        draw->AddLine({x, fy}, {x + w, fy}, border);
        text(draw, x, fy + 19, "SPACE", 11, mint);
        text(draw, x + 42, fy + 19, "play / stop", 11, muted);
        text(draw, x + 146, fy + 19, "1 - 4", 11, mint);
        text(draw, x + 187, fy + 19, "audition", 11, muted);
        text(draw, x + 290, fy + 19, "48 kHz / STEREO", 11, muted);
        meter = std::max(controls.peak.load(), meter * 0.92f);
        text(draw, x + w - 173, fy + 19, "OUT", 11, muted);
        for (int i = 0; i < 18; ++i) {
            const float mx = x + w - 139 + i * 8;
            draw->AddRectFilled({mx, fy + 19}, {mx + 5, fy + 29},
                meter * 18 > i ? (i > 14 ? colors[1] : mint) : IM_COL32(45, 52, 55, 255), 1);
        }
        if (audioFailed) text(draw, x, fy + 44, "Audio output failed. Restart the application and check your audio device.", 12, colors[1]);
        else text(draw, x, fy + 44, deviceName.c_str(), 11, muted);
        ImGui::End();
    }
};

struct Platform {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool imguiReady = false, platformReady = false, rendererReady = false;
    ~Platform() {
        if (rendererReady) ImGui_ImplSDLRenderer3_Shutdown();
        if (platformReady) ImGui_ImplSDL3_Shutdown();
        if (imguiReady) ImGui::DestroyContext();
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
};

// Exercise the actual SDL -> ImGui -> Controls path on dummy video/audio drivers.
void smokeInput(int frame, SDL_Window* window, drum::Controls& controls) {
    if (frame == 4 && controls.patterns[0].load() != 0x1110)
        throw std::runtime_error("Smoke test: clicking a step did not edit the pattern");
    if (frame == 6 && !controls.playing.load())
        throw std::runtime_error("Smoke test: Play button did not start playback");
    if (frame == 10 && controls.playing.load())
        throw std::runtime_error("Smoke test: Space did not stop playback");
    if (frame == 2 || frame == 3 || frame == 4 || frame == 5) {
        const bool grid = frame < 4;
        const float x = grid ? 268.0f : 100.0f;
        const float y = grid ? 355.0f : 170.0f;
        SDL_Event move{};
        move.type = SDL_EVENT_MOUSE_MOTION;
        move.motion.windowID = SDL_GetWindowID(window);
        move.motion.x = x; move.motion.y = y;
        require(SDL_PushEvent(&move), "Inject smoke-test mouse motion");
        SDL_Event button{};
        button.type = frame % 2 == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        button.button.windowID = SDL_GetWindowID(window);
        button.button.button = SDL_BUTTON_LEFT;
        button.button.down = frame % 2 == 0;
        button.button.x = x; button.button.y = y;
        require(SDL_PushEvent(&button), "Inject smoke-test click");
    }
    if (frame == 8 || frame == 9) {
        SDL_Event key{};
        key.type = frame == 8 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        key.key.windowID = SDL_GetWindowID(window);
        key.key.scancode = SDL_SCANCODE_SPACE;
        key.key.key = SDLK_SPACE;
        key.key.down = frame == 8;
        require(SDL_PushEvent(&key), "Inject smoke-test keyboard input");
    }
}
}

int main(int argc, char** argv) {
    try {
        fs::path assets, renderPath, screenshotPath;
        bool smoke = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--smoke-test") smoke = true;
            else if ((arg == "--assets" || arg == "--render-demo" || arg == "--screenshot") && i + 1 < argc) {
                fs::path value = argv[++i];
                if (arg == "--assets") assets = value;
                else if (arg == "--render-demo") renderPath = value;
                else screenshotPath = value;
            } else if (arg == "--help") {
                std::cout << "drum-machine\n  Space: play/stop\n  1-4: audition drums\n"
                             "  --assets DIR        Override bundled sample directory\n"
                             "  --render-demo FILE  Render two bars to a stereo WAV and exit\n"
                             "  --smoke-test        Run 12 UI frames and exit\n"
                             "  --screenshot FILE   Save first frame as BMP\n";
                return 0;
            } else throw std::runtime_error("Unknown or incomplete option: " + arg);
        }
        if (smoke) {
            SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
            SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        }
        SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");
        Platform platform;
        require(SDL_Init(renderPath.empty() ? SDL_INIT_VIDEO | SDL_INIT_AUDIO : 0), "Initialize SDL");
        if (assets.empty()) {
            const char* basePath = SDL_GetBasePath();
            if (!basePath) require(false, "Find executable directory");
            const fs::path base = basePath;
            assets = base / "assets";
            if (!fs::exists(assets)) assets = base / ".." / "share" / "drum-machine" / "assets";
        }
        drum::Controls controls;
        drum::Engine engine(controls, loadSamples(assets));
        if (!renderPath.empty()) {
            writeDemo(renderPath, engine, controls);
            std::cout << "Rendered two bars to " << renderPath << '\n';
            return 0;
        }
        platform.window = SDL_CreateWindow("drum-machine", 1180, 770, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        require(platform.window != nullptr, "Create window");
        SDL_SetWindowMinimumSize(platform.window, 1040, 750);
        platform.renderer = SDL_CreateRenderer(platform.window, nullptr);
        require(platform.renderer != nullptr, "Create renderer");
        SDL_SetRenderVSync(platform.renderer, 1);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        platform.imguiReady = true;
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        style.FontSizeBase = 16;
        style.FrameRounding = 4;
        style.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(background);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.19f, 0.21f, 1);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.27f, 0.27f, 1);
        style.Colors[ImGuiCol_SliderGrab] = ImGui::ColorConvertU32ToFloat4(mint);
        style.Colors[ImGuiCol_SliderGrabActive] = ImGui::ColorConvertU32ToFloat4(mint);
        require(ImGui_ImplSDL3_InitForSDLRenderer(platform.window, platform.renderer), "Initialize UI input");
        platform.platformReady = true;
        require(ImGui_ImplSDLRenderer3_Init(platform.renderer), "Initialize UI renderer");
        platform.rendererReady = true;
        Audio audio(engine);
        Interface ui;
        const SDL_AudioDeviceID device = SDL_GetAudioStreamDevice(audio.stream);
        const char* deviceName = SDL_GetAudioDeviceName(device);
        ui.deviceName = std::string("OUTPUT / ") + (deviceName ? deviceName : "Default audio device");
        bool running = true;
        int frame = 0;
        while (running) {
            if (smoke) smokeInput(frame, platform.window, controls);
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT ||
                    (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(platform.window)))
                    running = false;
            }
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            if (!io.WantTextInput && !ImGui::IsAnyItemActive()) {
                if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) controls.playing.store(!controls.playing.load());
                constexpr std::array<ImGuiKey, 4> keys{ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4};
                for (int i = 0; i < 4; ++i)
                    if (ImGui::IsKeyPressed(keys[i], false)) controls.previewRequests.fetch_or(1u << i);
            }
            ui.draw(controls, audio.failed.load());
            ImGui::Render();
            require(SDL_SetRenderDrawColor(platform.renderer, 19, 22, 25, 255), "Set clear color");
            require(SDL_RenderClear(platform.renderer), "Clear renderer");
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), platform.renderer);
            if (!screenshotPath.empty() && frame == 1) {
                std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(
                    SDL_RenderReadPixels(platform.renderer, nullptr), SDL_DestroySurface);
                require(surface != nullptr, "Read screenshot");
                require(SDL_SaveBMP(surface.get(), screenshotPath.c_str()), "Save screenshot");
            }
            require(SDL_RenderPresent(platform.renderer), "Present frame");
            SDL_Delay(16);
            if (smoke && ++frame >= 12) running = false;
            else if (!smoke) ++frame;
        }
        if (audio.failed.load()) throw std::runtime_error("Audio stream failed");
        if (smoke) std::cout << "UI and audio smoke test passed (12 frames).\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "drum-machine: " << error.what() << '\n';
        return 1;
    }
}
