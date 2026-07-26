#include "Shell.hpp"


size_t split(char* str, char delimiter, char* tokens[], size_t maxTokens)
{
    size_t count = 0;


    while (*str && count < maxTokens)
    {
        while (*str == delimiter)
            ++str;

        if (!*str)
            break;

        tokens[count++] = str;

        while (*str && *str != delimiter)
            ++str;

        if (*str)
            *str++ = '\0';
    }

    return count;
}


void executeLs(ShellOutput& output, const char* path)
{
    File dir = SD.open(path);

    if (!dir || !dir.isDirectory())
    {
        output.write("Cannot open directory\r\n");
        return;
    }

    while (true)
    {
        File file = dir.openNextFile();

        if (!file)
            break;

        if (file.isDirectory())
        {
            output.write("\x1b[1;34m"); // parlak mavi
            output.write(file.name());
            output.write("/\x1b[0m");
        }
        else
        {
            output.write("\x1b[0m");
            output.write(file.name());
        }

        output.write("  ");
        file.close();
    }

    output.write("\r\n");
    dir.close();
}

void executeOtherCommands(ShellOutput& output, const char* command)
{
    output.write("command not found: ");
    output.write(command);
    output.write("\r\n");
}

void normalizePath(char* path)
{
    char normalized[256];
    normalized[0] = '/';
    normalized[1] = '\0';

    char temp[256];
    strncpy(temp, path, sizeof(temp));

    char* segments[32];
    size_t count = split(temp, '/', segments, 32);

    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(segments[i], "") == 0)
            continue;

        if (strcmp(segments[i], ".") == 0)
            continue;

        if (strcmp(segments[i], "..") == 0)
        {
            char* lastSlash = strrchr(normalized, '/');

            if (lastSlash && lastSlash != normalized)
            {
                *lastSlash = '\0';

                lastSlash = strrchr(normalized, '/');

                if (!lastSlash)
                {
                    strcpy(normalized, "/");
                }
            }

            continue;
        }

        if (strcmp(normalized, "/") != 0)
            strcat(normalized, "/");

        strcat(normalized, segments[i]);
    }

    strncpy(path, normalized, 256);
}


bool resolvePath(
    const char* currentDirectory,
    const char* inputPath,
    char* resolvedPath,
    size_t resolvedPathSize)
{
    if (currentDirectory == nullptr ||
        inputPath == nullptr ||
        resolvedPath == nullptr ||
        resolvedPathSize == 0)
    {
        return false;
    }

    int written;

    if (inputPath[0] == '/')
    {
        written = snprintf(
            resolvedPath,
            resolvedPathSize,
            "%s",
            inputPath
        );
    }
    else
    {
        written = snprintf(
            resolvedPath,
            resolvedPathSize,
            "%s/%s",
            currentDirectory,
            inputPath
        );
    }

    if (written < 0 ||
        static_cast<size_t>(written) >= resolvedPathSize)
    {
        resolvedPath[0] = '\0';
        return false;
    }

    normalizePath(resolvedPath);

    return true;
}


void executeCd(ShellOutput& output, const char* path, char* currentDirectory, size_t& currentDirectoryLength)
{
    if (!path || strlen(path) == 0)
    {
        output.write("cd: missing argument\r\n");
        return;
    }

    char newPath[256];

    if (!resolvePath(
            currentDirectory,
            path,
            newPath,
            sizeof(newPath)))
    {
        output.write("cd: path is too long\r\n");
        return;
    }
    
    File dir = SD.open(newPath);

    if (!dir || !dir.isDirectory())
    {
        output.write("cd: no such directory: ");
        output.write(path);
        output.write("\r\n");

        return;
    }

    strncpy(currentDirectory, newPath, 256);
    currentDirectory[255] = '\0';
    currentDirectoryLength = strlen(currentDirectory);
}


void executeCat(
    ShellOutput& output,
    const char* path,
    const char* currentDirectory)
{
    if (path == nullptr || strlen(path) == 0)
    {
        output.write("cat: missing argument\r\n");
        return;
    }

    char fullPath[256];

    if (!resolvePath(currentDirectory, path, fullPath, sizeof(fullPath)))
    {
        output.write("cat: path is too long\r\n");
        return;
    }

    File file = SD.open(fullPath);

    if (!file)
    {
        output.write("cat: no such file: ");
        output.write(path);
        output.write("\r\n");
        return;
    }

    if (file.isDirectory())
    {
        output.write("cat: is a directory: ");
        output.write(path);
        output.write("\r\n");

        file.close();
        return;
    }

    char buffer[128];

    while (file.available())
    {
        size_t bytesRead = file.read(
            reinterpret_cast<uint8_t*>(buffer),
            sizeof(buffer) - 1
        );

        if (bytesRead == 0)
            break;

        buffer[bytesRead] = '\0';
        output.write(buffer);
    }

    file.close();
}

void executeRm(
    ShellOutput& output,
    const char* path,
    const char* currentDirectory)
{
    if (!path || path[0] == '\0')
    {
        output.write("rm: missing argument\r\n");
        return;
    }

    char fullPath[256];

    if (!resolvePath(
            currentDirectory,
            path,
            fullPath,
            sizeof(fullPath)))
    {
        output.write("rm: path is too long\r\n");
        return;
    }

    File file = SD.open(fullPath);

    if (!file)
    {
        output.write("rm: no such file: ");
        output.write(path);
        output.write("\r\n");
        return;
    }

    if (file.isDirectory())
    {
        file.close();

        output.write("rm: is a directory: ");
        output.write(path);
        output.write("\r\n");
        return;
    }

    file.close();

    if (!SD.remove(fullPath))
    {
        output.write("rm: failed to remove: ");
        output.write(path);
        output.write("\r\n");
    }
}

void executeMkdir(
    ShellOutput& output,
    const char* path,
    const char* currentDirectory)
{
    if (!path || path[0] == '\0')
    {
        output.write("mkdir: missing argument\r\n");
        return;
    }

    char fullPath[256];

    if (!resolvePath(
            currentDirectory,
            path,
            fullPath,
            sizeof(fullPath)))
    {
        output.write("mkdir: path is too long\r\n");
        return;
    }

    File existing = SD.open(fullPath);

    if (existing)
    {
        existing.close();

        output.write("mkdir: file or directory already exists: ");
        output.write(path);
        output.write("\r\n");
        return;
    }

    if (!SD.mkdir(fullPath))
    {
        output.write("mkdir: failed to create directory: ");
        output.write(path);
        output.write("\r\n");
    }
}

void executeTouch(
    ShellOutput& output,
    const char* path,
    const char* currentDirectory)
{
    if (!path || path[0] == '\0')
    {
        output.write("touch: missing argument\r\n");
        return;
    }

    char fullPath[256];

    if (!resolvePath(
            currentDirectory,
            path,
            fullPath,
            sizeof(fullPath)))
    {
        output.write("touch: path is too long\r\n");
        return;
    }

    File existing = SD.open(fullPath);

    if (existing)
    {
        if (existing.isDirectory())
        {
            existing.close();

            output.write("touch: is a directory: ");
            output.write(path);
            output.write("\r\n");
            return;
        }

        existing.close();
        return;
    }

    File file = SD.open(fullPath, FILE_WRITE);

    if (!file)
    {
        output.write("touch: failed to create file: ");
        output.write(path);
        output.write("\r\n");
        return;
    }

    file.close();
}

void executeCp(
    ShellOutput& output,
    const char* sourcePath,
    const char* destinationPath,
    const char* currentDirectory)
{
    if (!sourcePath || sourcePath[0] == '\0' ||
        !destinationPath || destinationPath[0] == '\0')
    {
        output.write("cp: missing argument\r\n");
        return;
    }

    char source[256];
    char destination[256];

    if (!resolvePath(
            currentDirectory,
            sourcePath,
            source,
            sizeof(source)) ||
        !resolvePath(
            currentDirectory,
            destinationPath,
            destination,
            sizeof(destination)))
    {
        output.write("cp: path is too long\r\n");
        return;
    }

    if (strcmp(source, destination) == 0)
    {
        output.write("cp: source and destination are the same\r\n");
        return;
    }

    File sourceFile = SD.open(source, FILE_READ);

    if (!sourceFile)
    {
        output.write("cp: no such file: ");
        output.write(sourcePath);
        output.write("\r\n");
        return;
    }

    if (sourceFile.isDirectory())
    {
        sourceFile.close();

        output.write("cp: source is a directory: ");
        output.write(sourcePath);
        output.write("\r\n");
        return;
    }

    File existingDestination = SD.open(destination);

    if (existingDestination)
    {
        existingDestination.close();
        sourceFile.close();

        output.write("cp: destination already exists: ");
        output.write(destinationPath);
        output.write("\r\n");
        return;
    }

    File destinationFile = SD.open(destination, FILE_WRITE);

    if (!destinationFile)
    {
        sourceFile.close();

        output.write("cp: failed to create destination: ");
        output.write(destinationPath);
        output.write("\r\n");
        return;
    }

    uint8_t buffer[256];
    bool success = true;

    while (sourceFile.available())
    {
        size_t bytesRead =
            sourceFile.read(buffer, sizeof(buffer));

        if (bytesRead == 0)
            break;

        size_t bytesWritten =
            destinationFile.write(buffer, bytesRead);

        if (bytesWritten != bytesRead)
        {
            success = false;
            break;
        }
    }

    sourceFile.close();
    destinationFile.close();

    if (!success)
    {
        SD.remove(destination);

        output.write("cp: failed while copying file\r\n");
    }
}

void executeMv(
    ShellOutput& output,
    const char* sourcePath,
    const char* destinationPath,
    const char* currentDirectory)
{
    if (!sourcePath || sourcePath[0] == '\0' ||
        !destinationPath || destinationPath[0] == '\0')
    {
        output.write("mv: missing argument\r\n");
        return;
    }

    char source[256];
    char destination[256];

    if (!resolvePath(
            currentDirectory,
            sourcePath,
            source,
            sizeof(source)) ||
        !resolvePath(
            currentDirectory,
            destinationPath,
            destination,
            sizeof(destination)))
    {
        output.write("mv: path is too long\r\n");
        return;
    }

    if (strcmp(source, destination) == 0)
        return;

    File sourceFile = SD.open(source);

    if (!sourceFile)
    {
        output.write("mv: no such file or directory: ");
        output.write(sourcePath);
        output.write("\r\n");
        return;
    }

    sourceFile.close();

    File destinationFile = SD.open(destination);

    if (destinationFile)
    {
        destinationFile.close();

        output.write("mv: destination already exists: ");
        output.write(destinationPath);
        output.write("\r\n");
        return;
    }

    if (!SD.rename(source, destination))
    {
        output.write("mv: failed to move: ");
        output.write(sourcePath);
        output.write("\r\n");
    }
}

void executeRmdir(
    ShellOutput& output,
    const char* path,
    const char* currentDirectory)
{
    if (!path || path[0] == '\0')
    {
        output.write("rmdir: missing argument\r\n");
        return;
    }

    char fullPath[256];

    if (!resolvePath(
            currentDirectory,
            path,
            fullPath,
            sizeof(fullPath)))
    {
        output.write("rmdir: path is too long\r\n");
        return;
    }

    File dir = SD.open(fullPath);

    if (!dir)
    {
        output.write("rmdir: no such directory: ");
        output.write(path);
        output.write("\r\n");
        return;
    }

    if (!dir.isDirectory())
    {
        dir.close();

        output.write("rmdir: not a directory: ");
        output.write(path);
        output.write("\r\n");
        return;
    }

    dir.close();

    if (!SD.rmdir(fullPath))
    {
        output.write("rmdir: failed to remove directory (not empty?)\r\n");
    }
}

void executePwd(
    ShellOutput& output,
    const char* currentDirectory)
{
    output.write(currentDirectory);
    output.write("\r\n");
}

void Shell::autoComplete(
    char* lineBuffer,
    size_t& lineLength,
    size_t& cursorPosition,
    ShellOutput& output)
{
    constexpr size_t LINE_BUFFER_SIZE = 256;
    constexpr size_t PATH_BUFFER_SIZE = 256;

    if (!lineBuffer || cursorPosition > lineLength)
        return;

    // Cursor'dan geriye giderek mevcut argümanın başlangıcını bul.
    size_t tokenStart = cursorPosition;

    while (tokenStart > 0)
    {
        char previous = lineBuffer[tokenStart - 1];

        if (previous == ' ' || previous == '\t')
            break;

        --tokenStart;
    }

    size_t tokenLength = cursorPosition - tokenStart;

    if (tokenLength >= PATH_BUFFER_SIZE)
    {
        output.write("\a");
        return;
    }

    char token[PATH_BUFFER_SIZE];

    memcpy(token, &lineBuffer[tokenStart], tokenLength);
    token[tokenLength] = '\0';

    /*
     * Örnek:
     *
     * token = "folder/tes"
     * directoryPart = "folder"
     * prefix = "tes"
     *
     * token = "tes"
     * directoryPart = "."
     * prefix = "tes"
     */
    char directoryPart[PATH_BUFFER_SIZE];
    char prefix[PATH_BUFFER_SIZE];

    const char* lastSlash = strrchr(token, '/');

    if (lastSlash)
    {
        size_t directoryLength =
            static_cast<size_t>(lastSlash - token);

        if (directoryLength == 0)
        {
            // "/tes" durumunda aranacak klasör root'tur.
            strcpy(directoryPart, "/");
        }
        else
        {
            memcpy(directoryPart, token, directoryLength);
            directoryPart[directoryLength] = '\0';
        }

        strncpy(
            prefix,
            lastSlash + 1,
            sizeof(prefix) - 1
        );

        prefix[sizeof(prefix) - 1] = '\0';
    }
    else
    {
        strcpy(directoryPart, ".");

        strncpy(prefix, token, sizeof(prefix) - 1);
        prefix[sizeof(prefix) - 1] = '\0';
    }

    char resolvedDirectory[PATH_BUFFER_SIZE];

    if (!resolvePath(
            workingDirectory,
            directoryPart,
            resolvedDirectory,
            sizeof(resolvedDirectory)))
    {
        output.write("\a");
        return;
    }

    File directory = SD.open(resolvedDirectory);

    if (!directory || !directory.isDirectory())
    {
        if (directory)
            directory.close();

        output.write("\a");
        return;
    }

    size_t prefixLength = strlen(prefix);
    size_t matchCount = 0;

    char commonPrefix[PATH_BUFFER_SIZE] = {};
    bool singleMatchIsDirectory = false;

    File entry;

    while ((entry = directory.openNextFile()))
    {
        const char* entryPath = entry.name();

        // Bazı FS implementasyonları entry.name() içinde tam path döndürebilir.
        const char* entryName = strrchr(entryPath, '/');

        if (entryName)
            ++entryName;
        else
            entryName = entryPath;

        if (strncmp(entryName, prefix, prefixLength) != 0)
        {
            entry.close();
            continue;
        }

        if (matchCount == 0)
        {
            strncpy(
                commonPrefix,
                entryName,
                sizeof(commonPrefix) - 1
            );

            commonPrefix[sizeof(commonPrefix) - 1] = '\0';
            singleMatchIsDirectory = entry.isDirectory();
        }
        else
        {
            size_t index = 0;

            while (commonPrefix[index] != '\0' &&
                   entryName[index] != '\0' &&
                   commonPrefix[index] == entryName[index])
            {
                ++index;
            }

            commonPrefix[index] = '\0';
            singleMatchIsDirectory = false;
        }

        ++matchCount;
        entry.close();
    }

    directory.close();

    if (matchCount == 0)
    {
        output.write("\a");
        return;
    }

    size_t commonPrefixLength = strlen(commonPrefix);

    // Yazılmış prefix'ten daha fazla ortak bölüm bulunamadı.
    if (commonPrefixLength <= prefixLength)
    {
        output.write("\a");
        return;
    }

    char completion[PATH_BUFFER_SIZE];

    strncpy(
        completion,
        commonPrefix + prefixLength,
        sizeof(completion) - 1
    );

    completion[sizeof(completion) - 1] = '\0';

    size_t completionLength = strlen(completion);

    // Tek eşleşme klasörse sonuna "/" ekle.
    if (matchCount == 1 && singleMatchIsDirectory)
    {
        if (completionLength + 1 < sizeof(completion))
        {
            completion[completionLength++] = '/';
            completion[completionLength] = '\0';
        }
    }

    if (lineLength + completionLength >= LINE_BUFFER_SIZE)
    {
        output.write("\a");
        return;
    }

    /*
     * Cursor satırın ortasındaysa sağ taraftaki metni kaydır.
     * Null terminator da taşındığı için +1 var.
     */
    memmove(
        &lineBuffer[cursorPosition + completionLength],
        &lineBuffer[cursorPosition],
        lineLength - cursorPosition + 1
    );

    memcpy(
        &lineBuffer[cursorPosition],
        completion,
        completionLength
    );

    size_t oldCursorPosition = cursorPosition;

    cursorPosition += completionLength;
    lineLength += completionLength;

    /*
     * Eklenen bölümden itibaren satırın kalanını yeniden çiz.
     */
    output.write(&lineBuffer[oldCursorPosition]);

    // Terminal cursor'ını eklenen metnin sonuna geri getir.
    size_t charactersAfterCursor =
        lineLength - cursorPosition;

    for (size_t i = 0; i < charactersAfterCursor; ++i)
        output.write("\b");
}
