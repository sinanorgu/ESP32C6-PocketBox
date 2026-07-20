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
