#include "SshManager.hpp"
#include "libssh_esp32.h"
#include "SSHKeyManager.hpp"
#include "System.hpp"


SSHKeyManager keyManager{
    "/PocketBox/System/ssh_host_ed25519_key"
};

ssh_key hostKey = nullptr;
EscapeState escapeState = EscapeState::None;
size_t cursorPosition = 0;

SSHManager::SSHManager(Shell& shell)
    : shell(shell)
{
}

bool SSHManager::ensureHostKey(const char* path)
{
    if (SD.exists(path))
        return true;

    ssh_key key = nullptr;

    if (ssh_pki_generate(
            SSH_KEYTYPE_ED25519,
            0,
            &key) != SSH_OK)
    {
        Serial.println("Host key generation failed");
        return false;
    }

    int result = ssh_pki_export_privkey_file(
        key,
        nullptr,   // passphrase yok
        nullptr,
        nullptr,
        path
    );

    ssh_key_free(key);

    if (result != SSH_OK)
    {
        Serial.println("Host key save failed");
        return false;
    }

    return true;
}

bool SSHManager::begin(
    const char* username,
    const char* password,
    const char* hostKeyPath,
    uint16_t port)
{
    if (running)
        return true;
    if(System::getInstance().isSDCardInserted == false){
        Serial.println("SD kart takili degil, SSH server baslatilamadi.");
        return false;
    }
    if(!System::getInstance().wifiManager.isConnected()){
        Serial.println("Wi-Fi baglantisi yok, SSH server baslatilamadi.");
        return false;
    }

    this->username = username;
    this->password = password;
    this->hostKeyPath = hostKeyPath;
    this->port = port;

    libssh_begin();



    running = true;

    System::getInstance().setSshStatus(true);
    
    BaseType_t result = xTaskCreate(
        taskEntry,
        "SSHServer",
        12288,
        this,
        3,
        &taskHandle
    );

    if (result != pdPASS)
    {
        running = false;
        taskHandle = nullptr;
        return false;
    }

    return true;
}

void SSHManager::taskEntry(void* parameter)
{
    auto* manager = static_cast<SSHManager*>(parameter);
    manager->serverTask();

    manager->taskHandle = nullptr;
    vTaskDelete(nullptr);
}




void SSHManager::serverTask()
{
    serverBind = ssh_bind_new();

    if (!serverBind)
    {
        running = false;
        return;
    }

    ssh_bind_options_set(
        serverBind,
        SSH_BIND_OPTIONS_BINDPORT,
        &port
    );

    // ssh_bind_options_set(
    //     serverBind,
    //     SSH_BIND_OPTIONS_HOSTKEY,
    //     hostKeyPath
    // );

    //=========

    hostKey = keyManager.loadOrCreate();

    if (!hostKey)
    {
        Serial.println("Could not prepare SSH host key.");
        running = false;
        return;
    }

    if (ssh_bind_options_set(
            serverBind,
            SSH_BIND_OPTIONS_IMPORT_KEY,
            hostKey) != SSH_OK)
    {
        Serial.println("Host key import failed");
        ssh_key_free(hostKey);
        hostKey = nullptr;
        running = false;
        return;
    }
    //----------------

    if (ssh_bind_listen(serverBind) < 0)
    {
        Serial.println(ssh_get_error(serverBind));
        ssh_bind_free(serverBind);
        serverBind = nullptr;
        running = false;
        return;
    }

    Serial.printf("SSH server started on port %u\n", port);

    while (running)
    {
        ssh_session session = ssh_new();

        if (!session)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (ssh_bind_accept(serverBind, session) != SSH_OK)
        {
            ssh_free(session);
            continue;
        }

        if (ssh_handle_key_exchange(session) != SSH_OK)
        {
            ssh_disconnect(session);
            ssh_free(session);
            continue;
        }

        if (!authenticate(session))
        {
            ssh_disconnect(session);
            ssh_free(session);
            continue;
        }

        ssh_channel channel = acceptShellChannel(session);

        if (channel)
        {
            escapeState = EscapeState::None;
            cursorPosition = 0;
            
            handleClient(session, channel);

            ssh_channel_send_eof(channel);
            ssh_channel_close(channel);
            ssh_channel_free(channel);
        }

        ssh_disconnect(session);
        ssh_free(session);
    }

    if (serverBind)
    {
        ssh_bind_free(serverBind);
        serverBind = nullptr;
    }

    // Bind key'i sahiplenmiş olduğu için tekrar ssh_key_free çağırma.
    hostKey = nullptr;
}

bool SSHManager::authenticate(ssh_session session)
{
    while (running)
    {
        ssh_message message = ssh_message_get(session);

        if (!message)
            return false;

        bool accepted = false;

        if (ssh_message_type(message) == SSH_REQUEST_AUTH &&
            ssh_message_subtype(message) == SSH_AUTH_METHOD_PASSWORD)
        {
            const char* receivedUsername =
                ssh_message_auth_user(message);

            const char* receivedPassword =
                ssh_message_auth_password(message);

            accepted =
                strcmp(receivedUsername, username) == 0 &&
                strcmp(receivedPassword, password) == 0;

            if (accepted)
            {
                ssh_message_auth_reply_success(message, 0);
                ssh_message_free(message);
                return true;
            }
        }

        ssh_message_auth_set_methods(
            message,
            SSH_AUTH_METHOD_PASSWORD
        );

        ssh_message_reply_default(message);
        ssh_message_free(message);
    }

    return false;


    //return true; // Şu anda tüm kullanıcıları kabul et
}


ssh_channel SSHManager::acceptShellChannel(ssh_session session)
{
    ssh_channel channel = nullptr;

    // Önce session channel açılmasını bekle.
    while (running && !channel)
    {
        ssh_message message = ssh_message_get(session);

        if (!message)
            return nullptr;

        if (ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN &&
            ssh_message_subtype(message) == SSH_CHANNEL_SESSION)
        {
            channel =
                ssh_message_channel_request_open_reply_accept(message);
        }
        else
        {
            ssh_message_reply_default(message);
        }

        ssh_message_free(message);
    }

    // Sonra PTY ve shell taleplerini işle.
    while (running)
    {
        ssh_message message = ssh_message_get(session);

        if (!message)
            return nullptr;

        if (ssh_message_type(message) == SSH_REQUEST_CHANNEL)
        {
            int subtype = ssh_message_subtype(message);

            if (subtype == SSH_CHANNEL_REQUEST_PTY)
            {
                ssh_message_channel_request_reply_success(message);
            }
            else if (subtype == SSH_CHANNEL_REQUEST_SHELL)
            {
                ssh_message_channel_request_reply_success(message);
                ssh_message_free(message);
                return channel;
            }
            else
            {
                ssh_message_reply_default(message);
            }
        }
        else
        {
            ssh_message_reply_default(message);
        }

        ssh_message_free(message);
    }

    return nullptr;
}

void SSHManager::handleClient(
    ssh_session session,
    ssh_channel channel)
{
    SSHOutput output(channel);

    lineLength = 0;
    lineBuffer[0] = '\0';

    output.write(
        "\r\n"
        "PocketBox Shell\r\n"
        "Type 'help' for available commands.\r\n"
    );

    writePrompt(output);

    char receiveBuffer[64];
    bool exitRequested = false;

    while (running &&
           !exitRequested &&
           ssh_channel_is_open(channel) &&
           !ssh_channel_is_eof(channel))
    {
        int received = ssh_channel_read(
            channel,
            receiveBuffer,
            sizeof(receiveBuffer),
            0
        );

        if (received == SSH_ERROR)
            break;

        if (received == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        for (int i = 0; i < received; ++i)
        {
            InputResult result =
                processCharacter(receiveBuffer[i], output);

            if (result == InputResult::ExitSession)
            {
                exitRequested = true;
                break;
            }
        }
    }
}

InputResult SSHManager::processCharacter(
    char character,
    SSHOutput& output)
{
    if (editorActive)
        return processEditorCharacter(character, output);

    if (escapeState == EscapeState::Escape)
    {
        escapeState =
            (character == '[')
            ? EscapeState::CSI
            : EscapeState::None;

        return InputResult::Continue;
    }

    if (escapeState == EscapeState::CSI)
    {
        escapeState = EscapeState::None;

        switch (character)
        {
            case 'A':
                // Yukarı: history
                break;

            case 'B':
                // Aşağı: history
                break;

            case 'C': // Sağ
                if (cursorPosition < lineLength)
                {
                    ++cursorPosition;
                    output.write("\x1b[C");
                }
                break;

            case 'D': // Sol
                if (cursorPosition > 0)
                {
                    --cursorPosition;
                    output.write("\x1b[D");
                }
                break;
        }

        return InputResult::Continue;
    }

    if (character == '\x1B')
    {
        escapeState = EscapeState::Escape;
        return InputResult::Continue;
    }

    if (character == '\r' || character == '\n')
    {
        if (lineLength == 0)
            return InputResult::Continue;

        output.write("\r\n");

        lineBuffer[lineLength] = '\0';

        ShellResult result =
            shell.executeCommand(lineBuffer, output);

        lineLength = 0;
        cursorPosition = 0;
        lineBuffer[0] = '\0';

        if (result == ShellResult::Exit)
            return InputResult::ExitSession;

        if (result == ShellResult::OpenEditor)
        {
            beginEditor(shell.requestedEditorPath(), output);
            return InputResult::Continue;
        }

        writePrompt(output);

        return InputResult::Continue;
    }

    if (character == '\t')
    {
        shell.autoComplete(
            lineBuffer,
            lineLength,
            cursorPosition,
            output
        );

    return InputResult::Continue;
}

    if (character == '\b' || character == 0x7F)
    {
        if (cursorPosition > 0)
        {
            memmove(
                &lineBuffer[cursorPosition - 1],
                &lineBuffer[cursorPosition],
                lineLength - cursorPosition + 1
            );

            --cursorPosition;
            --lineLength;

            output.write("\b");

            output.write(&lineBuffer[cursorPosition]);
            output.write(" ");

            size_t moveBack =
                lineLength - cursorPosition + 1;

            for (size_t i = 0; i < moveBack; ++i)
                output.write("\b");
        }

        return InputResult::Continue;
    }

    if (character == 0x03)
    {
        lineLength = 0;
        cursorPosition = 0;
        lineBuffer[0] = '\0';

        output.write("^C\r\n");
        writePrompt(output);

        return InputResult::Continue;
    }

    if (!isPrintable(static_cast<unsigned char>(character)))
        return InputResult::Continue;

    if (lineLength >= sizeof(lineBuffer) - 1)
        return InputResult::Continue;

    memmove(
        &lineBuffer[cursorPosition + 1],
        &lineBuffer[cursorPosition],
        lineLength - cursorPosition + 1
    );

    lineBuffer[cursorPosition] = character;

    ++cursorPosition;
    ++lineLength;

    // Cursor konumundan itibaren tüm kalan metni tekrar çiz.
    output.write(&lineBuffer[cursorPosition - 1]);

    // Cursor'ı tekrar doğru konuma getir.
    size_t moveBack = lineLength - cursorPosition;

    for (size_t i = 0; i < moveBack; ++i)
        output.write("\b");

    return InputResult::Continue;
}

bool SSHManager::beginEditor(const char* path, SSHOutput& output)
{
    free(editorBuffer);
    editorBuffer = static_cast<char*>(malloc(EditorCapacity + 1U));
    if (!editorBuffer) { output.write("nano: out of memory\r\n"); writePrompt(output); return false; }
    snprintf(editorPath, sizeof(editorPath), "%s", path);
    editorLength = 0;
    File file = SD.open(editorPath, FILE_READ);
    if (file)
    {
        if (file.size() > EditorCapacity)
        { file.close(); free(editorBuffer); editorBuffer = nullptr; output.write("nano: file exceeds 16 KiB\r\n"); writePrompt(output); return false; }
        editorLength = file.readBytes(editorBuffer, file.size());
        file.close();
    }
    editorBuffer[editorLength] = '\0';
    editorCursor = editorLength;
    editorEscapeState = 0;
    editorCsiParameter = 0;
    editorLastWasCarriageReturn = false;
    editorActive = true;
    redrawEditor(output);
    return true;
}

bool SSHManager::saveEditor(SSHOutput& output)
{
    char temporaryPath[272];
    const int written = snprintf(temporaryPath, sizeof(temporaryPath), "%s.nano.tmp", editorPath);
    if (written < 0 || static_cast<size_t>(written) >= sizeof(temporaryPath))
    { output.write("\r\n[nano: path is too long]\r\n"); return false; }
    if (SD.exists(temporaryPath)) SD.remove(temporaryPath);
    File file = SD.open(temporaryPath, FILE_WRITE);
    if (!file) { output.write("\r\n[nano: save failed]\r\n"); return false; }
    const bool ok = file.write(reinterpret_cast<const uint8_t*>(editorBuffer), editorLength) == editorLength;
    file.close();
    if (!ok) { SD.remove(temporaryPath); output.write("\r\n[nano: save failed]\r\n"); return false; }
    if (SD.exists(editorPath) && !SD.remove(editorPath))
    { output.write("\r\n[nano: cannot replace file; data kept in .nano.tmp]\r\n"); return false; }
    if (!SD.rename(temporaryPath, editorPath))
    { output.write("\r\n[nano: rename failed; data kept in .nano.tmp]\r\n"); return false; }
    redrawEditor(output, "saved");
    return true;
}

void SSHManager::closeEditor(SSHOutput& output)
{
    editorActive = false;
    free(editorBuffer);
    editorBuffer = nullptr;
    editorLength = 0;
    editorCursor = 0;
    output.write("\r\n[nano: closed]\r\n");
    writePrompt(output);
}

InputResult SSHManager::processEditorCharacter(char character, SSHOutput& output)
{
    if (character == '\n' && editorLastWasCarriageReturn)
    {
        editorLastWasCarriageReturn = false;
        return InputResult::Continue;
    }
    editorLastWasCarriageReturn = character == '\r';

    if (editorEscapeState == 1)
    {
        if (character == '[')
        {
            editorEscapeState = 2;
            editorCsiParameter = 0;
        }
        else editorEscapeState = 0;
        return InputResult::Continue;
    }
    if (editorEscapeState == 2)
    {
        if (character >= '0' && character <= '9')
        {
            editorCsiParameter = editorCsiParameter * 10 + character - '0';
            return InputResult::Continue;
        }
        editorEscapeState = 0;
        switch (character)
        {
            case 'A': moveEditorVertical(-1); break;
            case 'B': moveEditorVertical(1); break;
            case 'C': if (editorCursor < editorLength) ++editorCursor; break;
            case 'D': if (editorCursor > 0) --editorCursor; break;
            case 'H':
                while (editorCursor > 0 && editorBuffer[editorCursor - 1] != '\n') --editorCursor;
                break;
            case 'F':
                while (editorCursor < editorLength && editorBuffer[editorCursor] != '\n') ++editorCursor;
                break;
            case '~':
                if (editorCsiParameter == 3 && editorCursor < editorLength)
                {
                    memmove(editorBuffer + editorCursor, editorBuffer + editorCursor + 1,
                            editorLength - editorCursor);
                    --editorLength;
                }
                break;
        }
        editorBuffer[editorLength] = '\0';
        redrawEditor(output);
        return InputResult::Continue;
    }
    if (character == '\x1B')
    {
        editorEscapeState = 1;
        return InputResult::Continue;
    }
    if (character == 0x0F || character == 0x13) { saveEditor(output); return InputResult::Continue; }
    if (character == 0x18) { closeEditor(output); return InputResult::Continue; }
    if (character == '\r' || character == '\n')
    {
        if (editorLength < EditorCapacity)
        {
            memmove(editorBuffer + editorCursor + 1, editorBuffer + editorCursor,
                    editorLength - editorCursor + 1);
            editorBuffer[editorCursor++] = '\n';
            ++editorLength;
            redrawEditor(output);
        }
        return InputResult::Continue;
    }
    if (character == '\b' || character == 0x7F)
    {
        if (editorCursor > 0)
        {
            memmove(editorBuffer + editorCursor - 1, editorBuffer + editorCursor,
                    editorLength - editorCursor + 1);
            --editorCursor;
            --editorLength;
            editorBuffer[editorLength] = '\0';
            redrawEditor(output);
        }
        return InputResult::Continue;
    }
    if (!isPrintable(static_cast<unsigned char>(character))) return InputResult::Continue;
    if (editorLength >= EditorCapacity) { output.write("\a"); return InputResult::Continue; }
    memmove(editorBuffer + editorCursor + 1, editorBuffer + editorCursor,
            editorLength - editorCursor + 1);
    editorBuffer[editorCursor++] = character;
    ++editorLength;
    editorBuffer[editorLength] = '\0';
    redrawEditor(output);
    return InputResult::Continue;
}

void SSHManager::moveEditorVertical(int direction)
{
    size_t lineStart = editorCursor;
    while (lineStart > 0 && editorBuffer[lineStart - 1] != '\n') --lineStart;
    const size_t column = editorCursor - lineStart;

    if (direction < 0)
    {
        if (lineStart == 0) return;
        const size_t previousEnd = lineStart - 1;
        size_t previousStart = previousEnd;
        while (previousStart > 0 && editorBuffer[previousStart - 1] != '\n') --previousStart;
        const size_t previousLength = previousEnd - previousStart;
        editorCursor = previousStart + (column < previousLength ? column : previousLength);
    }
    else
    {
        size_t currentEnd = editorCursor;
        while (currentEnd < editorLength && editorBuffer[currentEnd] != '\n') ++currentEnd;
        if (currentEnd == editorLength) return;
        const size_t nextStart = currentEnd + 1;
        size_t nextEnd = nextStart;
        while (nextEnd < editorLength && editorBuffer[nextEnd] != '\n') ++nextEnd;
        const size_t nextLength = nextEnd - nextStart;
        editorCursor = nextStart + (column < nextLength ? column : nextLength);
    }
}

void SSHManager::redrawEditor(SSHOutput& output, const char* status)
{
    output.write("\x1b[2J\x1b[H");
    output.write("PocketBox nano - Ctrl+O save | Ctrl+X exit");
    if (status) { output.write("  ["); output.write(status); output.write("]"); }
    output.write("\r\n---\r\n");

    char text[256];
    size_t textLength = 0;
    for (size_t i = 0; i < editorLength; ++i)
    {
        const size_t needed = editorBuffer[i] == '\n' ? 2U : 1U;
        if (textLength + needed >= sizeof(text))
        {
            text[textLength] = '\0';
            output.write(text);
            textLength = 0;
        }
        if (editorBuffer[i] == '\n')
        {
            text[textLength++] = '\r';
            text[textLength++] = '\n';
        }
        else text[textLength++] = editorBuffer[i];
    }
    if (textLength > 0) { text[textLength] = '\0'; output.write(text); }

    size_t row = 3;
    size_t column = 1;
    for (size_t i = 0; i < editorCursor; ++i)
    {
        if (editorBuffer[i] == '\n') { ++row; column = 1; }
        else ++column;
    }
    char position[32];
    snprintf(position, sizeof(position), "\x1b[%u;%uH",
             static_cast<unsigned>(row), static_cast<unsigned>(column));
    output.write(position);
}

void SSHManager::writePrompt(SSHOutput& output)
{
    output.write("\x1b[1;32mpocketbox\x1b[0m:");
    output.write("\x1b[1;34m");
    output.write(shell.workingDirectory);
    output.write("\x1b[0m$ ");
}

void SSHManager::stop()
{
    running = false;
}
