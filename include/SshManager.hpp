#pragma once
#include "Shell.hpp"
#include <Arduino.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

class SSHOutput : public ShellOutput
{
public:
    explicit SSHOutput(ssh_channel channel)
        : channel(channel)
    {
    }

    void write(const char* text) override
    {
        if (!channel || !text)
            return;

        ssh_channel_write(channel, text, strlen(text));
    }
    void clear() override
    {
        write("\x1b[2J\x1b[H");
    }

private:
    ssh_channel channel;
};

enum class InputResult
{
    Continue,
    ExitSession
};


class SSHManager
{
public:
    explicit SSHManager(Shell& shell);

    bool begin(
        const char* username,
        const char* password,
        const char* hostKeyPath,
        uint16_t port = 22
    );

    void stop();

private:
    static void taskEntry(void* parameter);
    void serverTask();

    bool authenticate(ssh_session session);
    ssh_channel acceptShellChannel(ssh_session session);

    void handleClient(ssh_session session, ssh_channel channel);
    InputResult processCharacter(char character, SSHOutput& output);

    void writePrompt(SSHOutput& output);
    bool ensureHostKey(const char* path);

private:
    Shell& shell;

    const char* username = nullptr;
    const char* password = nullptr;
    const char* hostKeyPath = nullptr;

    uint16_t port = 22;

    ssh_bind serverBind = nullptr;
    TaskHandle_t taskHandle = nullptr;

    char lineBuffer[256]{};
    size_t lineLength = 0;

    bool running = false;
};

enum class EscapeState
{
    None,
    Escape,
    CSI
};

