/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>
*/
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

// Include Dear ImGui Headers
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <cstdio>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;

const int BORDER_SIZE = 5;

struct AppState {
    char **image_files;
    int total_files;
    int current_index;
    char *parent_dir;
    bool trigger_context_menu; // <-- Add this deferred state flag
};

static bool IsSupportedImage(const char *filename) {
    if (!filename) return false;
    const char *ext = SDL_strrchr(filename, '.');
    if (!ext) return false;
    return (SDL_strcasecmp(ext, ".png") == 0  || SDL_strcasecmp(ext, ".jpg") == 0  || 
            SDL_strcasecmp(ext, ".jpeg") == 0 || SDL_strcasecmp(ext, ".bmp") == 0  || 
            SDL_strcasecmp(ext, ".webp") == 0);
}

static bool LoadImageAtIndex(AppState *state, bool first_load) {
    if (state->total_files <= 0) return false;

    char full_path[1024];
    SDL_snprintf(full_path, sizeof(full_path), "%s%s", state->parent_dir, state->image_files[state->current_index]);

    if (texture) { SDL_DestroyTexture(texture); texture = NULL; }

    SDL_Surface *temp_surface = IMG_Load(full_path);
    if (!temp_surface) return false;

    texture = SDL_CreateTextureFromSurface(renderer, temp_surface);
    float img_w = (float)temp_surface->w;
    float img_h = (float)temp_surface->h;
    SDL_DestroySurface(temp_surface);

    if (!texture) return false;

    if (first_load) {
        SDL_DisplayID primary_display = SDL_GetPrimaryDisplay();
        SDL_Rect display_bounds;
        if (!SDL_GetDisplayUsableBounds(primary_display, &display_bounds)) {
            display_bounds.w = 1920; display_bounds.h = 1080;
        }
        int target_w = (int)img_w; int target_h = (int)img_h;
        if (target_w > display_bounds.w || target_h > display_bounds.h) {
            float scale = SDL_min((float)display_bounds.w / img_w, (float)display_bounds.h / img_h) * 0.90f;
            target_w = (int)(img_w * scale); target_h = (int)(img_h * scale);
        }
        SDL_SetWindowSize(window, target_w, target_h);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    
    SDL_SetWindowTitle(window, state->image_files[state->current_index]);
    return true;
}

static SDL_HitTestResult SDLCALL WindowHitTest(SDL_Window *win, const SDL_Point *area, void *data) {
    // If the mouse is hovering over an open ImGui interface window, skip dragging 
    // so we can interact with menus without shifting the window framework around.
    if (ImGui::GetIO().WantCaptureMouse) {
        return SDL_HITTEST_NORMAL;
    }

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

    AppState *state = (AppState *)SDL_calloc(1, sizeof(AppState));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    char path_buffer[1024];
    SDL_strlcpy(path_buffer, argv[1], sizeof(path_buffer));
    char *last_slash = SDL_strrchr(path_buffer, '/');
#ifdef SDL_PLATFORM_WINDOWS
    if (!last_slash) last_slash = SDL_strrchr(path_buffer, '\\');
#endif

    char filename_buffer[256] = "";
    if (last_slash) {
        SDL_strlcpy(filename_buffer, last_slash + 1, sizeof(filename_buffer));
        *(last_slash + 1) = '\0'; 
        state->parent_dir = SDL_strdup(path_buffer);
    } else {
        SDL_strlcpy(filename_buffer, path_buffer, sizeof(filename_buffer));
        state->parent_dir = SDL_strdup("./");
    }

    int file_count = 0;
    char **files = SDL_GlobDirectory(state->parent_dir, "*", SDL_GLOB_CASEINSENSITIVE, &file_count);
    if (files) {
        state->image_files = (char **)SDL_malloc(sizeof(char *) * file_count);
        for (int i = 0; i < file_count; i++) {
            if (IsSupportedImage(files[i])) {
                state->image_files[state->total_files] = SDL_strdup(files[i]);
                if (SDL_strcmp(files[i], filename_buffer) == 0) state->current_index = state->total_files;
                state->total_files++;
            }
        }
        SDL_free(files);
    }

    Uint32 window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN;
    window = SDL_CreateWindow("SDL3 Image Viewer", 800, 600, window_flags);
    renderer = SDL_CreateRenderer(window, NULL);
    if (!window || !renderer) return SDL_APP_FAILURE;
    
    SDL_SetWindowHitTest(window, WindowHitTest, NULL);

    if (!LoadImageAtIndex(state, true)) return SDL_APP_FAILURE;

    // Initialize Dear ImGui Context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark(); // Sleek dark theme matching our background

    // Setup Platform/Renderer Backends
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    SDL_ShowWindow(window);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    AppState *state = (AppState *)appstate;

    ImGui_ImplSDL3_ProcessEvent(event);

    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;
    
    // Defer the popup request safely by marking our state boolean
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_RIGHT) {
        state->trigger_context_menu = true; 
    }
    
    if (event->type == SDL_EVENT_KEY_DOWN) {
        switch (event->key.key) {
            case SDLK_ESCAPE: return SDL_APP_SUCCESS;
            case SDLK_SPACE:
                if (state->total_files > 1) {
                    state->current_index = (state->current_index + 1) % state->total_files;
                    LoadImageAtIndex(state, false);
                }
                break;
            case SDLK_BACKSPACE:
                if (state->total_files > 1) {
                    state->current_index = (state->current_index - 1 + state->total_files) % state->total_files;
                    LoadImageAtIndex(state, false);
                }
                break;
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    AppState *state = (AppState *)appstate;

    int window_w = 0, window_h = 0;
    float texture_w = 0, texture_h = 0;
    SDL_FRect dst_rect;

    SDL_GetRenderOutputSize(renderer, &window_w, &window_h);
    if (texture) {
        SDL_GetTextureSize(texture, &texture_w, &texture_h);
        float scale = SDL_min((float)window_w / texture_w, (float)window_h / texture_h);
        dst_rect.w = texture_w * scale; dst_rect.h = texture_h * scale;
        dst_rect.x = ((float)window_w - dst_rect.w) / 2.0f; dst_rect.y = ((float)window_h - dst_rect.h) / 2.0f;
    }

    // Start ImGui Frame Rendering Chain
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // ★ Handle deferred menu trigger inside a valid frame window lifecycle
    if (state->trigger_context_menu) {
        ImGui::OpenPopup("mymenu");
        state->trigger_context_menu = false; // Reset flag instantly
    }

    // Render the context layout options safely
    if (ImGui::BeginPopup("mymenu")) {
        if (ImGui::MenuItem("Next Image", "Space", false, state->total_files > 1)) {
            state->current_index = (state->current_index + 1) % state->total_files;
            LoadImageAtIndex(state, false);
        }
        if (ImGui::MenuItem("Previous Image", "Backspace", false, state->total_files > 1)) {
            state->current_index = (state->current_index - 1 + state->total_files) % state->total_files;
            LoadImageAtIndex(state, false);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Esc")) {
            return SDL_APP_SUCCESS;
        }
        ImGui::EndPopup();
    }

    // Render hardware elements
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);
    if (texture) {
        SDL_RenderTexture(renderer, texture, NULL, &dst_rect);
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

    SDL_RenderPresent(renderer);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    AppState *state = (AppState *)appstate;
    if (state) {
        if (state->image_files) {
            for (int i = 0; i < state->total_files; i++) SDL_free(state->image_files[i]);
            SDL_free(state->image_files);
        }
        if (state->parent_dir) SDL_free(state->parent_dir);
        SDL_free(state);
    }

    // Shutdown ImGui Contexts
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
}
