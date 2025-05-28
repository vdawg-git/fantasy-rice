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

// including loudness
const int BANDS_AMOUNT = 13;

inline HANDLE PHANDLE = nullptr;

typedef void (*useProgramOriginal)(void *, GLuint);
inline CFunctionHook *s_useProgramHook = nullptr;

typedef void (*applyScreenShaderOriginal)(void *, const std::string &path);
inline CFunctionHook *s_applyScreenShaderHook = nullptr;

static int s_socketFD = -1;
static std::atomic<bool> s_shouldExit{false};
static std::thread s_socketThread;
static GLuint s_finalScreenShaderProgram = -1;

static std::atomic<std::shared_ptr<std::vector<float>>> s_audioBands;

inline GLint s_loudnessUniform = -1;
inline GLint s_subwooferUniform = -1;
inline GLint s_subtoneUniform = -1;
inline GLint s_kickdrumUniform = -1;
inline GLint s_lowBassUniform = -1;
inline GLint s_bassBodyUniform = -1;
inline GLint s_midBassUniform = -1;
inline GLint s_warmthUniform = -1;
inline GLint s_lowMidsUniform = -1;
inline GLint s_midsMoodyUniform = -1;
inline GLint s_upperMidsUniform = -1;
inline GLint s_attackUniform = -1;
inline GLint s_highsUniform = -1;

void hkUseProgram(void *thisptr, GLuint prog)
{
    (*(useProgramOriginal)s_useProgramHook->m_original)(thisptr, prog);

    if (prog == s_finalScreenShaderProgram)
    {
        auto bandsData = *s_audioBands.load().get();
        glUniform1f(s_loudnessUniform, bandsData[0]);
        glUniform1f(s_subwooferUniform, bandsData[1]);
        glUniform1f(s_subtoneUniform, bandsData[2]);
        glUniform1f(s_kickdrumUniform, bandsData[3]);
        glUniform1f(s_lowBassUniform, bandsData[4]);
        glUniform1f(s_bassBodyUniform, bandsData[5]);
        glUniform1f(s_midBassUniform, bandsData[6]);
        glUniform1f(s_warmthUniform, bandsData[7]);
        glUniform1f(s_lowMidsUniform, bandsData[8]);
        glUniform1f(s_midsMoodyUniform, bandsData[9]);
        glUniform1f(s_upperMidsUniform, bandsData[10]);
        glUniform1f(s_attackUniform, bandsData[11]);
        glUniform1f(s_highsUniform, bandsData[12]);
    }
}

void hkApplyScreenShader(void *thisptr, const std::string &path)
{
    // Call original function first
    (*(applyScreenShaderOriginal)s_applyScreenShaderHook->m_original)(thisptr, path);

    s_finalScreenShaderProgram = g_pHyprOpenGL->m_finalScreenShader.program;

    s_loudnessUniform = glGetUniformLocation(s_finalScreenShaderProgram, "loudness");
    s_subwooferUniform = glGetUniformLocation(s_finalScreenShaderProgram, "subwoofer");
    s_subtoneUniform = glGetUniformLocation(s_finalScreenShaderProgram, "subtone");
    s_kickdrumUniform = glGetUniformLocation(s_finalScreenShaderProgram, "kickdrum");
    s_lowBassUniform = glGetUniformLocation(s_finalScreenShaderProgram, "lowBass");
    s_bassBodyUniform = glGetUniformLocation(s_finalScreenShaderProgram, "bassBody");
    s_midBassUniform = glGetUniformLocation(s_finalScreenShaderProgram, "midBass");
    s_warmthUniform = glGetUniformLocation(s_finalScreenShaderProgram, "warmth");
    s_lowMidsUniform = glGetUniformLocation(s_finalScreenShaderProgram, "lowMids");
    s_midsMoodyUniform = glGetUniformLocation(s_finalScreenShaderProgram, "mids");
    s_upperMidsUniform = glGetUniformLocation(s_finalScreenShaderProgram, "upperMids");
    s_attackUniform = glGetUniformLocation(s_finalScreenShaderProgram, "attack");
    s_highsUniform = glGetUniformLocation(s_finalScreenShaderProgram, "highs");

    auto bands = *s_audioBands.load();
    HyprlandAPI::addNotification(PHANDLE, std::format("{}", bands), CHyprColor{0.2f, 1.0f, 0.4f, 1.0f}, 4000);
    // HyprlandAPI::addNotification(PHANDLE, std::format("{}\n{}\n{}\nHooked applyScreenShader was called", s_finalScreenShaderProgram, s_loudnessUniform, s_loudness.load()), CHyprColor{0.2f, 1.0f, 0.4f, 1.0f}, 4000);
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
                auto parsed = parseFloats(lastLine, BANDS_AMOUNT);
                s_audioBands.store(std::make_shared<std::vector<float>>(std::move(parsed)));
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
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
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
        HyprlandAPI::addNotification(PHANDLE, "[audio-viz] Audio socket not available yet, will retry in background...", CHyprColor{1.0, 1.0, 0.2, 1.0}, 3000);
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

    // init bands pointer empty
    s_audioBands.store(std::make_shared<std::vector<float>>(BANDS_AMOUNT));

    setupSocket();
    setupHook(PHANDLE, "applyScreenShader", "CHyprOpenGLImpl", (void *)::hkApplyScreenShader, s_applyScreenShaderHook);
    setupHook(PHANDLE, "useProgram", "CHyprOpenGLImpl", (void *)::hkUseProgram, s_useProgramHook);

    HyprlandAPI::addNotification(PHANDLE, "Visualizer plugin loaded! 🎶", CHyprColor{0.2f, 1.0f, 0.4f, 1.0f}, 4000);

    return {"hypr-visualizer", "Minimal shader-based music visualizer", "Dawg", "1.0"};
}
