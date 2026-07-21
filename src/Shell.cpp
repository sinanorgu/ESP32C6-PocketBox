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
void executeCd(ShellOutput& output, const char* path, char* currentDirectory, size_t& currentDirectoryLength)
{
    if (!path || strlen(path) == 0)
    {
        output.write("cd: missing argument\r\n");
        return;
    }

    char newPath[256];

    if (path[0] == '/')
    {
        strncpy(newPath, path, sizeof(newPath));
    }
    else
    {
        snprintf(
            newPath,
            sizeof(newPath),
            "%s/%s",
            currentDirectory,
            path);
    }

    // Normalize path
    normalizePath(newPath);
    // char normalized[256];
    // normalized[0] = '/';
    // normalized[1] = '\0';

    // char temp[256];
    // strncpy(temp, newPath, sizeof(temp));

    // char* segments[32];
    // size_t count = split(temp, '/', segments, 32);

    // for (size_t i = 0; i < count; i++)
    // {
    //     if (strcmp(segments[i], "") == 0)
    //         continue;

    //     if (strcmp(segments[i], ".") == 0)
    //         continue;

    //     if (strcmp(segments[i], "..") == 0)
    //     {
    //         char* lastSlash = strrchr(normalized, '/');

    //         if (lastSlash && lastSlash != normalized)
    //         {
    //             *lastSlash = '\0';

    //             lastSlash = strrchr(normalized, '/');

    //             if (!lastSlash)
    //             {
    //                 strcpy(normalized, "/");
    //             }
    //         }

    //         continue;
    //     }

    //     if (strcmp(normalized, "/") != 0)
    //         strcat(normalized, "/");

    //     strcat(normalized, segments[i]);
    // }

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