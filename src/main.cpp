#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

// Include Dear ImGui Headers
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace fs = std::filesystem;

constexpr int BORDER_SIZE = 5;

using WindowPtr = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;
using RendererPtr = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;
using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

struct AppState {
    std::vector<std::string> image_files;
    std::size_t current_index = 0;
    std::string parent_dir;
    bool trigger_context_menu = false;

    WindowPtr window{nullptr, SDL_DestroyWindow};
    RendererPtr renderer{nullptr, SDL_DestroyRenderer};
    TexturePtr texture{nullptr, SDL_DestroyTexture};

    AppState() = default;
    ~AppState() = default;

    static bool is_supported_image(const fs::path &p) {
        if (!p.has_extension()) return false;
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
        return (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".webp");
    }

    bool load_image_at_index(bool first_load) {
        if (image_files.empty() || !renderer) return false;

        const auto &filename = image_files[current_index];
        fs::path full = fs::path(parent_dir) / filename;

        // Release previous texture via RAII
        texture.reset();

        SDL_Surface *surf = IMG_Load(full.string().c_str());
        if (!surf) return false;

        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer.get(), surf);
        float img_w = static_cast<float>(surf->w);
        float img_h = static_cast<float>(surf->h);
        SDL_DestroySurface(surf);

        if (!tex) return false;
        texture.reset(tex);

        if (first_load && window) {
            SDL_DisplayID primary_display = SDL_GetPrimaryDisplay();
            SDL_Rect display_bounds;
            if (!SDL_GetDisplayUsableBounds(primary_display, &display_bounds)) {
                display_bounds.w = 1920; display_bounds.h = 1080;
            }
            int target_w = static_cast<int>(img_w);
            int target_h = static_cast<int>(img_h);
            if (target_w > display_bounds.w || target_h > display_bounds.h) {
                float scale = SDL_min(static_cast<float>(display_bounds.w) / img_w,
                                      static_cast<float>(display_bounds.h) / img_h) * 0.90f;
                target_w = static_cast<int>(img_w * scale);
                target_h = static_cast<int>(img_h * scale);
            }
            SDL_SetWindowSize(window.get(), target_w, target_h);
            SDL_SetWindowPosition(window.get(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        }

        SDL_SetWindowTitle(window.get(), filename.c_str());
        return true;
    }
};

static SDL_HitTestResult SDLCALL WindowHitTest(SDL_Window *win, const SDL_Point *area, void *data) {
    if (ImGui::GetIO().WantCaptureMouse) return SDL_HITTEST_NORMAL;

    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    bool top = (area->y <= BORDER_SIZE), bottom = (area->y >= h - BORDER_SIZE);
    bool left = (area->x <= BORDER_SIZE), right = (area->x >= w - BORDER_SIZE);

    if (top && left) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (top && right) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (bottom && left) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (bottom && right) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (top) return SDL_HITTEST_RESIZE_TOP;
    if (bottom) return SDL_HITTEST_RESIZE_BOTTOM;
    if (left) return SDL_HITTEST_RESIZE_LEFT;
    if (right) return SDL_HITTEST_RESIZE_RIGHT;

    return SDL_HITTEST_DRAGGABLE;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (argc < 2 || !argv[1]) return SDL_APP_FAILURE;

    auto state = new AppState();
    *appstate = state;

    fs::path argpath = argv[1];
    if (argpath.has_parent_path()) {
        state->parent_dir = argpath.parent_path().string();
        if (state->parent_dir.back() != fs::path::preferred_separator) state->parent_dir.push_back(fs::path::preferred_separator);
        state->image_files.push_back(argpath.filename().string());
    } else {
        state->parent_dir = std::string("./");
        state->image_files.push_back(argpath.filename().string());
    }

    // Enumerate directory for supported images
    try {
        for (auto &entry : fs::directory_iterator(state->parent_dir)) {
            if (!entry.is_regular_file()) continue;
            if (AppState::is_supported_image(entry.path())) {
                state->image_files.push_back(entry.path().filename().string());
            }
        }
        // remove duplicates and sort
        std::sort(state->image_files.begin(), state->image_files.end());
        state->image_files.erase(std::unique(state->image_files.begin(), state->image_files.end()), state->image_files.end());

        // find initial file index
        auto it = std::find(state->image_files.begin(), state->image_files.end(), argpath.filename().string());
        if (it != state->image_files.end()) state->current_index = static_cast<std::size_t>(std::distance(state->image_files.begin(), it));
    } catch (...) {
        // filesystem errors -> failure
    }

    Uint32 window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN;
    state->window.reset(SDL_CreateWindow("SDL3 Image Viewer", 800, 600, window_flags));
    if (!state->window) { delete state; return SDL_APP_FAILURE; }

    state->renderer.reset(SDL_CreateRenderer(state->window.get(), nullptr));
    if (!state->renderer) { delete state; return SDL_APP_FAILURE; }

    SDL_SetWindowHitTest(state->window.get(), WindowHitTest, nullptr);

    if (!state->load_image_at_index(true)) { delete state; return SDL_APP_FAILURE; }

    // Initialize Dear ImGui Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer Backends
    ImGui_ImplSDL3_InitForSDLRenderer(state->window.get(), state->renderer.get());
    ImGui_ImplSDLRenderer3_Init(state->renderer.get());

    SDL_ShowWindow(state->window.get());
    return SDL_APP_CONTINUE;
}

SDL_AppResult quit(SDL_AppResult rlt) {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    return rlt;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    auto *state = static_cast<AppState*>(appstate);
    ImGui_ImplSDL3_ProcessEvent(event);

    if (event->type == SDL_EVENT_QUIT) return quit(SDL_APP_SUCCESS);

    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_RIGHT) {
        state->trigger_context_menu = true;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        switch (event->key.key) {
            case SDLK_ESCAPE: return quit(SDL_APP_SUCCESS);
            case SDLK_SPACE:
                if (state->image_files.size() > 1) {
                    state->current_index = (state->current_index + 1) % state->image_files.size();
                    state->load_image_at_index(false);
                }
                break;
            case SDLK_BACKSPACE:
                if (state->image_files.size() > 1) {
                    state->current_index = (state->current_index + state->image_files.size() - 1) % state->image_files.size();
                    state->load_image_at_index(false);
                }
                break;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    auto *state = static_cast<AppState*>(appstate);

    int window_w = 0, window_h = 0;
    float texture_w = 0, texture_h = 0;
    SDL_FRect dst_rect{};

    SDL_GetRenderOutputSize(state->renderer.get(), &window_w, &window_h);
    if (state->texture) {
        SDL_GetTextureSize(state->texture.get(), &texture_w, &texture_h);
        float scale = SDL_min(static_cast<float>(window_w) / texture_w, static_cast<float>(window_h) / texture_h);
        dst_rect.w = texture_w * scale; dst_rect.h = texture_h * scale;
        dst_rect.x = (static_cast<float>(window_w) - dst_rect.w) / 2.0f; dst_rect.y = (static_cast<float>(window_h) - dst_rect.h) / 2.0f;
    }

    // Start ImGui Frame Rendering Chain
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (state->trigger_context_menu) {
        ImGui::OpenPopup("mymenu");
        state->trigger_context_menu = false;
    }

    if (ImGui::BeginPopup("mymenu")) {
        if (ImGui::MenuItem("Next Image", "Space", false, state->image_files.size() > 1)) {
            state->current_index = (state->current_index + 1) % state->image_files.size();
            state->load_image_at_index(false);
        }
        if (ImGui::MenuItem("Previous Image", "Backspace", false, state->image_files.size() > 1)) {
            state->current_index = (state->current_index + state->image_files.size() - 1) % state->image_files.size();
            state->load_image_at_index(false);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Esc")) {
            return quit(SDL_APP_SUCCESS);
        }
        ImGui::EndPopup();
    }

    // Render hardware elements
    SDL_SetRenderDrawColor(state->renderer.get(), 30, 30, 30, 255);
    SDL_RenderClear(state->renderer.get());
    if (state->texture) {
        SDL_RenderTexture(state->renderer.get(), state->texture.get(), NULL, &dst_rect);
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), state->renderer.get());

    SDL_RenderPresent(state->renderer.get());
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    auto *state = static_cast<AppState*>(appstate);
    if (state) delete state;
}
