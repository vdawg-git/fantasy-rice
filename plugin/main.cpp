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

const char *SOCKET_PATH = "/tmp/audio_monitor.sock";

inline HANDLE PHANDLE = nullptr;

typedef void (*useProgramOriginal)(void *, GLuint);
inline CFunctionHook *s_useProgramHook = nullptr;

typedef void (*applyScreenShaderOriginal)(void *, const std::string &path);
inline CFunctionHook *s_applyScreenShaderHook = nullptr;

static int s_socketFD = -1;
static std::atomic<bool> s_shouldExit{false};
static std::thread s_socketThread;
static GLuint s_finalScreenShaderProgram = -1;

static std::atomic<float> s_loudness{0.0f};
static std::atomic<float> s_subBass{0.0f};
static std::atomic<float> s_bass{0.0f};
static std::atomic<float> s_lowMids{0.0f};
static std::atomic<float> s_mids{0.0f};
static std::atomic<float> s_highs{0.0f};

inline GLint s_loudnessUniform = -1;
inline GLint s_subBassUniform = -1;
inline GLint s_bassUniform = -1;
inline GLint s_lowMidsUniform = -1;
inline GLint s_midsUniform = -1;
inline GLint s_highsUniform = -1;

void hkUseProgram(void *thisptr, GLuint prog)
{
    (*(useProgramOriginal)s_useProgramHook->m_original)(thisptr, prog);

    if (prog == s_finalScreenShaderProgram)
    {
        glUniform1f(s_loudnessUniform, s_loudness.load());
        glUniform1f(s_subBassUniform, s_subBass.load());
        glUniform1f(s_bassUniform, s_bass.load());
        glUniform1f(s_lowMidsUniform, s_lowMids.load());
        glUniform1f(s_midsUniform, s_mids.load());
        glUniform1f(s_highsUniform, s_highs.load());
    }
}

void hkApplyScreenShader(void *thisptr, const std::string &path)
{
    // Call original function first
    (*(applyScreenShaderOriginal)s_applyScreenShaderHook->m_original)(thisptr, path);

    s_finalScreenShaderProgram = g_pHyprOpenGL->m_finalScreenShader.program;

    s_loudnessUniform = glGetUniformLocation(s_finalScreenShaderProgram, "loudness");
    s_subBassUniform = glGetUniformLocation(s_finalScreenShaderProgram, "subBass");
    s_bassUniform = glGetUniformLocation(s_finalScreenShaderProgram, "bass");
    s_lowMidsUniform = glGetUniformLocation(s_finalScreenShaderProgram, "lowMids");
    s_midsUniform = glGetUniformLocation(s_finalScreenShaderProgram, "mids");
    s_highsUniform = glGetUniformLocation(s_finalScreenShaderProgram, "highs");

    HyprlandAPI::addNotification(PHANDLE, std::format("{}\n{}\n{}\nHooked applyScreenShader was called", s_finalScreenShaderProgram, s_loudnessUniform, s_loudness.load()), CHyprColor{0.2f, 1.0f, 0.4f, 1.0f}, 4000);
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
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(s_socketFD, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        HyprlandAPI::addNotification(PHANDLE, std::format("Failed to connect to socket: {}", SOCKET_PATH), CHyprColor{0.8f, 0.0f, 0.0f, 1.0f}, 4000);
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

    HyprlandAPI::addNotification(PHANDLE, std::format("Connected to socket: {}", SOCKET_PATH), CHyprColor{0.2f, 1.0f, 0.4f, 1.0f}, 4000);

    return true;
}

// Parses comma delimited floats from a string
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
        throw std::runtime_error(std::format("Expected exactly {} floats! (≧д≦ヾ)", amount));
    }

    return values;
}

// Persistent buffer to accumulate data across multiple reads
static std::string bufferToRead;

void processSocketData()
{
    char buf[256];                                        // Temporary buffer to hold raw bytes
    ssize_t len = read(s_socketFD, buf, sizeof(buf) - 1); // Read up to 255 bytes

    if (len > 0)
    {
        buf[len] = '\0';                  // Null-terminate raw buffer
        bufferToRead += std::string(buf); // Append new data to the persistent buffer

        // Look for the last complete line (ending with '\n')
        size_t lastNewline = bufferToRead.rfind('\n');
        if (lastNewline != std::string::npos)
        {
            std::string lastLine = bufferToRead.substr(0, lastNewline); // All complete lines
            bufferToRead.erase(0, lastNewline + 1);                     // Remove processed lines from buffer

            // Only want the final line of those
            size_t secondLastNewline = lastLine.rfind('\n');
            if (secondLastNewline != std::string::npos)
                lastLine = lastLine.substr(secondLastNewline + 1); // Just the final one

            // Clean up newline (not strictly needed anymore)
            lastLine.erase(std::remove(lastLine.begin(), lastLine.end(), '\n'), lastLine.end());

            try
            {
                auto parsed = parseFloats(lastLine, 6);

                s_loudness.store(parsed[0]);
                s_subBass.store(parsed[1]);
                s_bass.store(parsed[2]);
                s_lowMids.store(parsed[3]);
                s_mids.store(parsed[4]);
                s_highs.store(parsed[5]);
            }
            catch (const std::exception &error)
            {
                HyprlandAPI::addNotification(
                    PHANDLE,
                    std::format("Bad data\n{}\n{}", error.what(), lastLine),
                    CHyprColor{1.0, 0.2, 0.2, 1.0},
                    5000);
            }
        }
        // Else: no complete line yet → do nothing until the buffer has a full line!
    }
    else if (len == 0 || (len < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
    {
        close(s_socketFD);
        s_socketFD = -1;

        HyprlandAPI::addNotification(
            PHANDLE,
            "[audio-viz] Socket error or closed",
            CHyprColor{1.0, 0.2, 0.2, 1.0},
            5000);
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
                tryConnectSocket();
            }
        }
        else
        {
            reconnectCounter = 0;
            processSocketData();
        }

        // Sleep for ~16ms (60fps equivalent)
        std::this_thread::sleep_for(std::chrono::seconds(1));
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
