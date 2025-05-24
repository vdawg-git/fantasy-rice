#define WLR_USE_UNSTABLE

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>

#include <any>
#include <atomic>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/shaders/Shaders.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/Window.hpp>
#include <hyprland/src/managers/HookSystemManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#undef private

#include "shaders.hpp"

inline HANDLE PHANDLE = nullptr;

typedef void (*useProgramOriginal)(void *, GLuint);
inline CFunctionHook *s_useProgramHook = nullptr;

typedef void (*applyScreenShaderOriginal)(void *, const std::string &path);
inline CFunctionHook *s_applyScreenShaderHook = nullptr;

static int s_socketFD = -1;
static std::atomic<float> s_loudness{0.0f};
static std::string s_readBuffer; // Buffer for incomplete reads
static std::mutex s_bufferMutex; // Protect the read buffer
static std::atomic<bool> s_shouldExit{false};
static std::thread s_socketThread;
static GLuint s_finalScreenShaderProgram = -1;
static bool s_finalScreenShaderProgramReset = true;
inline GLint s_loudnessUniform = -1;
static float s_tmp = -1.0;

void hkUseProgram(void *thisptr, GLuint prog)
{
    (*(useProgramOriginal)s_useProgramHook->m_original)(thisptr, prog);

    // this function does get called.
    // but never this part down here
    if (prog == s_finalScreenShaderProgram)
    {
        glUniform1f(s_loudnessUniform, s_loudness.load());
    }
}

void hkApplyScreenShader(void *thisptr, const std::string &path)
{
    // Call original function first
    (*(applyScreenShaderOriginal)s_applyScreenShaderHook->m_original)(thisptr, path);

    s_finalScreenShaderProgram = g_pHyprOpenGL.get()->m_finalScreenShader.program;
    s_loudnessUniform = glGetUniformLocation(s_finalScreenShaderProgram, "loudness");

    HyprlandAPI::addNotification(PHANDLE, std::format("{} - {} - {} - Hooked applyScreenShader was called", s_tmp, s_finalScreenShaderProgram, s_loudnessUniform), CHyprColor{0.2f, 1.0f, 0.4f, 1.0f}, 4000);
}

bool tryConnectSocket()
{
    if (s_socketFD != -1)
    {
        close(s_socketFD);
        s_socketFD = -1;
    }

    s_socketFD = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s_socketFD < 0)
    {
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/audio_monitor.sock", sizeof(addr.sun_path) - 1);

    if (connect(s_socketFD, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(s_socketFD);
        s_socketFD = -1;
        return false;
    }

    // Set non-blocking mode
    int flags = fcntl(s_socketFD, F_GETFL, 0);
    if (flags != -1)
    {
        fcntl(s_socketFD, F_SETFL, flags | O_NONBLOCK);
    }

    return true;
}

std::vector<float> parseFloats(const std::string &input, u_int amount)
{
    std::stringstream ss(input);
    std::string item;
    std::vector<float> values;

    while (std::getline(ss, item, ','))
    {
        values.push_back(std::stof(item));
    }

    if (values.size() != amount)
    {
        throw std::runtime_error("Expected exactly 5 floats! (≧д≦ヾ)");
    }

    return values;
}

void processSocketData()
{
    char buf[256];
    ssize_t len = read(s_socketFD, buf, sizeof(buf) - 1);

    if (len > 0)
    {
        buf[len] = '\0';

        {
            std::lock_guard<std::mutex> lock(s_bufferMutex);
            s_readBuffer += buf;
        }

        // Process all complete lines in the buffer
        std::lock_guard<std::mutex> lock(s_bufferMutex);
        size_t pos;
        while ((pos = s_readBuffer.find('\n')) != std::string::npos)
        {
            std::string line = s_readBuffer.substr(0, pos);
            s_readBuffer.erase(0, pos + 1);

            // Parse the line
            if (!line.empty())
            {
                try
                {
                    auto newUniforms = parseFloats(line, 6);
                    float loudness = newUniforms[0];
                    float subBass = newUniforms[1];
                    float bass = newUniforms[2];
                    float lowMids = newUniforms[3];
                    float mids = newUniforms[4];
                    float highs = newUniforms[5];

                    s_loudness.store(smoothedLoudness);
                }
                catch (const std::exception &)
                {
                    // Invalid data, ignore this line
                }
            }
        }

        // Keep buffer size reasonable
        if (s_readBuffer.size() > 1024)
        {
            s_readBuffer.clear();
        }
    }
    else if (len == 0)
    {
        // Socket closed by peer
        close(s_socketFD);
        s_socketFD = -1;
        std::lock_guard<std::mutex> lock(s_bufferMutex);
        s_readBuffer.clear();
    }
    else if (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        // Real error occurred
        close(s_socketFD);
        s_socketFD = -1;
        HyprlandAPI::addNotification(
            PHANDLE,
            "[audio-viz] Failure in socket!",
            CHyprColor{1.0, 0.2, 0.2, 1.0},
            5000);
        std::lock_guard<std::mutex> lock(s_bufferMutex);
        s_readBuffer.clear();

        // For EAGAIN/EWOULDBLOCK, just continue - no data available right now
    }
}

void socketWorkerThread()
{
    // Set thread name for debugging
    pthread_setname_np(pthread_self(), "audio-viz-sock");

    int reconnectCounter = 0;

    while (!s_shouldExit.load())
    {
        if (s_socketFD == -1)
        {
            // Try to reconnect every 60 iterations (~1 second at 60fps)
            if (++reconnectCounter > 60)
            {
                reconnectCounter = 0;
                if (tryConnectSocket())
                {
                    // Successfully connected, reset buffer
                    std::lock_guard<std::mutex> lock(s_bufferMutex);
                    s_readBuffer.clear();
                }
            }
        }
        else
        {
            reconnectCounter = 0;
            processSocketData();
        }

        // Sleep for ~16ms (60fps equivalent)
        std::this_thread::sleep_for(std::chrono::seconds(16));
    }

    // Cleanup on exit
    if (s_socketFD != -1)
    {
        close(s_socketFD);
        s_socketFD = -1;
    }
}

void setupSocket()
{
    // Start the socket worker thread
    s_shouldExit.store(false);
    s_socketThread = std::thread(socketWorkerThread);

    // Initial connection attempt (non-blocking)
    if (!tryConnectSocket())
    {
        HyprlandAPI::addNotification(PHANDLE, "[audio-viz] Audio socket not available yet, will retry in background...",
                                     CHyprColor{1.0, 1.0, 0.2, 1.0}, 3000);
    }
    else
    {
        HyprlandAPI::addNotification(PHANDLE, "[audio-viz] Connected to audio socket!",
                                     CHyprColor{0.2, 1.0, 0.2, 1.0}, 3000);
    }
}

void setupHook(
    HANDLE handle, const std::string &name, const std::string &demangled, const void *hook, CFunctionHook *&bindingVariable)
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
        HyprlandAPI::addNotification(
            PHANDLE,
            "[audio-viz] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
            CHyprColor{1.0, 0.2, 0.2, 1.0},
            5000);

        throw std::runtime_error("[audio-viz] Version mismatch");
    }
}

extern "C" APICALL void PLUGIN_EXIT()
{
    // Signal thread to exit
    s_shouldExit.store(true);

    // Wait for thread to finish (with timeout)
    if (s_socketThread.joinable())
    {
        s_socketThread.join();
    }

    // Socket cleanup is handled by the worker thread
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}

extern "C" APICALL PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;
    ensureHyprlandVersionMatch();

    s_finalScreenShaderProgram = g_pHyprOpenGL.get()->m_finalScreenShader.program;

    setupSocket();
    setupHook(PHANDLE, "applyScreenShader", "CHyprOpenGLImpl", (void *)::hkApplyScreenShader, s_applyScreenShaderHook);
    setupHook(PHANDLE, "useProgram", "CHyprOpenGLImpl", (void *)::hkUseProgram, s_useProgramHook);

    HyprlandAPI::addNotification(PHANDLE, "Visualizer plugin loaded! 🎶", CHyprColor{0.2f, 1.0f, 0.4f, 1.0f}, 4000);

    return {"hypr-visualizer", "Minimal shader-based music visualizer", "Dawg", "1.0"};
}
