/**
 * Emscripten port of projectm-SDLvis
 *  Mischa Spiegelmock, 2014
 *
 */

#include <projectM-4/projectM.h>
#include <projectM-4/playlist.h>

#include <emscripten.h>

#include <GL/gl.h>

#include <SDL.h>

#include <cmath>
#include <string>

const int FPS = 60;

struct projectMApp
{
    projectm_handle projectM{ nullptr };
    projectm_playlist_handle playlist{ nullptr };
    SDL_Window* win{ nullptr };
    SDL_Renderer* renderer{ nullptr };
    SDL_AudioDeviceID audioInputDevice{ 0 };
    int audioInputDevicesCount{ 0 };
    int audioInputDeviceIndex{ 0 };
    int audioChannelsCount{ 0 };
    bool isFullscreen{ false };
};

static projectMApp app;

void audioInputCallbackF32(void* userdata, unsigned char* stream, int len)
{
    if (app.audioChannelsCount == 1)
    {
        projectm_pcm_add_float(app.projectM, reinterpret_cast<float*>(stream), len / sizeof(float), PROJECTM_MONO);
    }
    else if (app.audioChannelsCount == 2)
    {
        projectm_pcm_add_float(app.projectM, reinterpret_cast<float*>(stream), len / sizeof(float), PROJECTM_STEREO);
    }
}

bool selectAudioInput(projectMApp* application, int audioDeviceIndex)
{
    // Valid audio devices in SDL have an ID of at least 2.
    if (app.audioInputDevice >= 2)
    {
        SDL_PauseAudioDevice(app.audioInputDevice, true);
        SDL_CloseAudioDevice(app.audioInputDevice);
        app.audioInputDevice = 0;
    }

    app.audioInputDevicesCount = SDL_GetNumAudioDevices(1);

    if (!app.audioInputDevicesCount)
    {
        fprintf(stderr, "No audio input capture devices detected. Faking audio using random data.\n");
        return false;
    }

    for (int i = 0; i < app.audioInputDevicesCount; ++i)
    {
        printf("Audio device %d: %s\n", i, SDL_GetAudioDeviceName(i, 1));
    }

    // Just wrap to 0 if index is out of range.
    if (audioDeviceIndex < 0 || audioDeviceIndex >= app.audioInputDevicesCount)
    {
        audioDeviceIndex = 0;
    }

    app.audioInputDeviceIndex = audioDeviceIndex;

    SDL_AudioSpec want, have;

    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_F32;
    want.channels = 2;
    want.samples = projectm_pcm_get_max_samples();
    want.callback = &audioInputCallbackF32;

    std::string audioDeviceName = SDL_GetAudioDeviceName(audioDeviceIndex, true);

    // Start with first device
    app.audioInputDevice = SDL_OpenAudioDevice(audioDeviceName.c_str(), true, &want, &have, 0);
    app.audioChannelsCount = have.channels;

    if (app.audioChannelsCount == 1 || app.audioChannelsCount == 2)
    {
        SDL_PauseAudioDevice(app.audioInputDevice, false);

        printf("Audio device specs: Channels=%d, Samplerate=%d, Format=%d\n", have.channels, have.freq, have.format);
    }
    else
    {
        fprintf(stderr, "Audio input capture device has invalid number of channels. Faking audio data.\n");
    }

    return true;
}

void keyHandler(projectMApp* appContext, const SDL_Event& sdl_evt)
{
    auto sdl_mod = static_cast<SDL_Keymod>(sdl_evt.key.keysym.mod);
    SDL_Keycode sdl_keycode = sdl_evt.key.keysym.sym;
    bool keyMod = false;

    // Left or Right Gui or Left Ctrl
    if (sdl_mod & KMOD_LGUI || sdl_mod & KMOD_RGUI || sdl_mod & KMOD_LCTRL)
    {
        keyMod = true;
    }

    // handle keyboard input (for our app first, then projectM)
    switch (sdl_keycode)
    {
        case SDLK_r:
        {
            bool shuffleMode = projectm_playlist_get_shuffle(appContext->playlist);
            projectm_playlist_set_shuffle(appContext->playlist, true);
            projectm_playlist_play_next(appContext->playlist, true);
            projectm_playlist_set_shuffle(appContext->playlist, shuffleMode);
            break;
        }

        case SDLK_n:
            projectm_playlist_play_next(appContext->playlist, true);
            break;

        case SDLK_p:
            projectm_playlist_play_previous(appContext->playlist, true);
            break;

        case SDLK_BACKSPACE:
            projectm_playlist_play_last(appContext->playlist, true);
            break;

        case SDLK_UP:
            projectm_set_beat_sensitivity(appContext->projectM, projectm_get_beat_sensitivity(appContext->projectM) + 0.1f);
            break;

        case SDLK_DOWN:
            projectm_set_beat_sensitivity(appContext->projectM, projectm_get_beat_sensitivity(appContext->projectM) - 0.1f);
            break;

        case SDLK_f:
            if (keyMod)
            {
                appContext->isFullscreen = !appContext->isFullscreen;
                SDL_SetWindowFullscreen(appContext->win, appContext->isFullscreen ? SDL_WINDOW_FULLSCREEN : 0);
                return; // handled
            }
            break;

        case SDLK_i:
            if (keyMod)
            {
                selectAudioInput(appContext, appContext->audioInputDeviceIndex + 1);
                return; // handled
            }
            break;
    }

}

void presetSwitchFailedEvent(const char* preset_filename,
                             const char* message, void* user_data)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Loading preset %s failed: %s.\n", preset_filename, message);
}

void presetSwitchedEvent(bool isHardCut, unsigned int index, void* context)
{
    auto presetName = projectm_playlist_item(app.playlist, index);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Displaying preset: %s\n", presetName);

    std::string newTitle = "projectM ➫ " + std::string(presetName);
    projectm_playlist_free_string(presetName);

    SDL_SetWindowTitle(app.win, newTitle.c_str());
}

void generateRandomAudioData()
{
    short pcm_data[2][512];

    for (int i = 0; i < 512; i++)
    {
        pcm_data[0][i] = static_cast<short>((static_cast<double>(rand()) / (static_cast<double>(RAND_MAX)) *
                                             (pow(2, 14))));
        pcm_data[1][i] = static_cast<short>((static_cast<double>(rand()) / (static_cast<double>(RAND_MAX)) *
                                             (pow(2, 14))));

        if (i % 2 == 1)
        {
            pcm_data[0][i] = -pcm_data[0][i];
            pcm_data[1][i] = -pcm_data[1][i];
        }
    }

    projectm_pcm_add_int16(app.projectM, &pcm_data[0][0], 512, PROJECTM_STEREO);
}

void renderFrame(void* appArg)
{
    SDL_Event evt;

    auto* appContext = reinterpret_cast<projectMApp*>(appArg);

    SDL_PollEvent(&evt);
    switch (evt.type)
    {
        case SDL_KEYDOWN:
            keyHandler(appContext, evt);
            break;

        case SDL_QUIT:
            emscripten_cancel_main_loop();
            break;
    }

    if (appContext->audioChannelsCount > 2 || appContext->audioChannelsCount < 1)
    {
        generateRandomAudioData();
    }

    glClearColor(0.0, 0.5, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    projectm_opengl_render_frame(appContext->projectM);
    glFlush();

    SDL_RenderPresent(appContext->renderer);
}

int main(int argc, char* argv[])
{
    int width = 1024;
    int height = 1024;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_OPENGL, &app.win, &app.renderer);
    if (!app.win || !app.renderer)
    {
        fprintf(stderr, "Failed to create SDL renderer: %s\n", SDL_GetError());
        return 1;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL renderer initialized.\n");

    app.projectM = projectm_create();
    if (!app.projectM) {
        fprintf(stderr, "Failed to create projectM instance!\n");
        return 1;
    }

    app.playlist = projectm_playlist_create(app.projectM);
    if (!app.playlist)
    {
        fprintf(stderr, "Failed to create the playlist instance!\n");
        return 1;
    }

    projectm_playlist_set_preset_switched_event_callback(app.playlist, &presetSwitchedEvent, nullptr);

    projectm_set_mesh_size(app.projectM, 48, 32);
    projectm_set_fps(app.projectM, FPS);
    projectm_set_window_size(app.projectM, width, height);
    projectm_set_soft_cut_duration(app.projectM, 3.0);
    projectm_set_preset_duration(app.projectM, 5);
    projectm_set_beat_sensitivity(app.projectM, 1.0);
    projectm_set_aspect_correction(app.projectM, true);
    projectm_set_easter_egg(app.projectM, 0);

    projectm_set_preset_switch_failed_event_callback(app.projectM, &presetSwitchFailedEvent, nullptr);

    projectm_playlist_set_shuffle(app.playlist, true);
    projectm_playlist_add_path(app.playlist, "/presets", true, true);

    projectm_playlist_play_next(app.playlist, true);

    printf("projectM initialized.\n");

    // get an audio input device
    if (!selectAudioInput(&app, 0))
    {
        fprintf(stderr, "Failed to open audio input device\n");
        return 1;
    }

    emscripten_set_main_loop_arg(renderFrame, &app, 0, 1);

    return 0;
}
