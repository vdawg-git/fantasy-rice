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

inline HANDLE PHANDLE = nullptr;

static int g_socketFD = -1;
static float g_loudness = 0.0f;
static GLuint g_shaderProg = 0;
static GLint g_loudnessUniform = -1;
static wl_event_source *g_tick = nullptr;

const std::string FRAG_SHADER = R"glsl(
#version 330 core
uniform float loudness;
out vec4 fragColor;
void main() {
    vec2 uv = gl_FragCoord.xy / vec2(1920.0, 1080.0); // hardcoded res for now
    float r = sin(uv.x * 10.0 + loudness * 5.0);
    float g = sin(uv.y * 10.0 + loudness * 10.0);
    fragColor = vec4(r, g, loudness, 1.0);
}
)glsl";

const std::string VERT_SHADER = R"glsl(
#version 330 core
layout(location = 0) in vec2 pos;
void main() {
    gl_Position = vec4(pos, 0.0, 1.0);
}
)glsl";

GLuint compileShader(GLenum type, const std::string &src)
{
    GLuint shader = glCreateShader(type);
    const char *cstr = src.c_str();
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        // Fetch the error log
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::string typeStr = type == GL_VERTEX_SHADER ? "VERT" : "FRAG";
        throw std::runtime_error("[" + typeStr + "] Shader compile failed: " + std::string(log));
    }

    return shader;
}

GLuint createProgram()
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, VERT_SHADER);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FRAG_SHADER);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
        throw std::runtime_error("Shader link failed");
    return prog;
}

int onTick(void *)
{
    char buf[64] = {};
    ssize_t len = read(g_socketFD, buf, sizeof(buf) - 1);
    if (len > 0)
    {
        try
        {
            g_loudness = std::stof(buf);
        }
        catch (...)
        {
            g_loudness = 0.0f;
        }
    }

    g_pHyprRenderer->makeEGLCurrent();
    glUseProgram(g_shaderProg);
    glUniform1f(g_loudnessUniform, g_loudness);

    // draw fullscreen triangle or screen effect
    g_pHyprRenderer->damageMonitor(g_pHyprRenderer->m_mostHzMonitor.lock()); // force repaint

    int timeout = g_pHyprRenderer->m_mostHzMonitor ? 1000 / g_pHyprRenderer->m_mostHzMonitor->m_refreshRate : 16;
    wl_event_source_timer_update(g_tick, timeout);
    return 0;
}

void setupSocket()
{
    g_socketFD = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_socketFD < 0)
        throw std::runtime_error("Failed to create socket");

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/audio_monitor.sock", sizeof(addr.sun_path) - 1);

    if (connect(g_socketFD, (sockaddr *)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("Failed to connect to audio_monitor.sock");

    fcntl(g_socketFD, F_SETFL, O_NONBLOCK);
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}

extern "C" APICALL PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    const std::string HASH = __hyprland_api_get_hash();

    if (HASH != GIT_COMMIT_HASH)
    {
        HyprlandAPI::addNotification(PHANDLE, "[ht] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)", CHyprColor{1.0, 0.2, 0.2, 1.0},
                                     5000);
        throw std::runtime_error("[ht] Version mismatch");
    }

    g_pHyprRenderer->makeEGLCurrent();
    g_shaderProg = createProgram();
    g_loudnessUniform = glGetUniformLocation(g_shaderProg, "loudness");

    setupSocket();

    g_tick = wl_event_loop_add_timer(g_pCompositor->m_wlEventLoop, &onTick, nullptr);
    wl_event_source_timer_update(g_tick, 1);

    HyprlandAPI::addNotification(PHANDLE, "Visualizer plugin loaded! 🎶", CHyprColor{0.2f, 1.0f, 0.4f, 1.0f}, 4000);
    return {"hypr-visualizer", "Minimal shader-based music visualizer", "Dawg", "1.0"};
}

extern "C" APICALL void PLUGIN_EXIT()
{
    if (g_tick)
        wl_event_source_remove(g_tick);
    if (g_socketFD != -1)
        close(g_socketFD);
    glDeleteProgram(g_shaderProg);
}
