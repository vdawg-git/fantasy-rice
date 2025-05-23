#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <any>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/shaders/Shaders.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include "shaders.hpp"

#define private public
#include <hyprland/src/render/OpenGL.hpp>
#undef private

inline HANDLE PHANDLE = nullptr;

typedef void (*useProgramOriginal)(GLuint);
inline CFunctionHook *s_useProgramHook = nullptr;

typedef void (*applyScreenShaderOriginal)(const std::string &path);
inline CFunctionHook *s_applyScreenShaderHook = nullptr;

static int s_socketFD = -1;
static float s_loudness = 5.0f;
inline GLint s_loudnessUniform = -1;

void hkUseProgram(GLuint prog)
{
    // call original function
    (*(useProgramOriginal)s_useProgramHook->m_original)(prog);

    auto finalShader = g_pHyprOpenGL.get()->m_finalScreenShader; // NOLINT

    if (prog != finalShader.program)
        return;

    if (s_loudnessUniform == -1)
    {
        s_loudnessUniform = glGetUniformLocation(prog, "loudness");
    }

    glUniform1f(s_loudnessUniform, s_loudness);
}

int onTick(void *)
{
    char buf[64] = {};
    ssize_t len = read(s_socketFD, buf, sizeof(buf) - 1);
    if (len > 0)
    {
        try
        {
            s_loudness = std::stof(buf);
        }
        catch (...)
        {
            s_loudness = 0.0f;
        }
    }

    return 0;
}

void setupSocket()
{
    s_socketFD = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s_socketFD < 0)
        throw std::runtime_error("Failed to create socket");

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/audio_monitor.sock", sizeof(addr.sun_path) - 1);

    if (connect(s_socketFD, (sockaddr *)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("Failed to connect to audio_monitor.sock");

    fcntl(s_socketFD, F_SETFL, O_NONBLOCK);
}

void setupHook(
    HANDLE handle, const std::string &name, const std::string &demangled, const void *hook, CFunctionHook *bindingVariable)
{
    auto toSearch = HyprlandAPI::findFunctionsByName(PHANDLE, name);
    for (auto &fn : toSearch)
    {
        if (!fn.demangled.contains(demangled))
            continue;

        bindingVariable = HyprlandAPI::createFunctionHook(PHANDLE, fn.address, hook);

        bool success = bindingVariable && bindingVariable->hook();
        if (!success)
        {
            throw std::runtime_error(std::format("[audio-viz] Failed to hook {}", name));
        }

        return;
    }

    throw std::runtime_error(std::format("[audio-viz] Failed to find {} function to hook", name));
}

void ensureHyprlandVersionMatch()
{
    const std::string HASH = __hyprland_api_get_hash();
    if (HASH != GIT_COMMIT_HASH)
    {
        HyprlandAPI::addNotification(PHANDLE, "[audio-viz] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)", CHyprColor{1.0, 0.2, 0.2, 1.0},
                                     5000);

        throw std::runtime_error("[audio-viz] Version mismatch");
    }
}

extern "C" APICALL PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;
    ensureHyprlandVersionMatch();

    setupSocket();
    setupHook(PHANDLE, "useProgram", "CHyprOpenGLImpl", (void *)::hkUseProgram, s_useProgramHook);

    HyprlandAPI::addNotification(PHANDLE, "Visualizer plugin loaded! 🎶", CHyprColor{0.2f, 1.0f, 0.4f, 1.0f}, 4000);

    return {"hypr-visualizer", "Minimal shader-based music visualizer", "Dawg", "1.0"};
}

extern "C" APICALL void PLUGIN_EXIT()
{
    // close socket on plugin unload
    if (s_socketFD != -1)
        close(s_socketFD);
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    // comment
    return HYPRLAND_API_VERSION;
}