#pragma once
#include "SdCardManager.hpp"


class ShellOutput
{
public:
    virtual void write(const char* text) = 0;
    virtual void clear() = 0;
};

enum class ShellResult
{
    Continue,
    Exit,
    Reboot,
    Shutdown
};

size_t split(char* str, char delimiter, char* tokens[], size_t maxTokens);
void executeLs(ShellOutput& output, const char* path);
void executeCd(ShellOutput& output, const char* path, char* currentDirectory, size_t& currentDirectoryLength);
void executeOtherCommands(ShellOutput& output, const char* command);

class Shell {
    public:
        Shell() = default;
        char workingDirectory[256] = "/PocketBox";
        size_t workingDirectoryNameLength = 10; // Length of "/PocketBox"
        char* argv[16]; // Array to hold command arguments
        ShellResult executeCommand(const char* command, ShellOutput& output) {
            
            char commandCopy[256];
            strncpy(commandCopy, command, sizeof(commandCopy));
            commandCopy[sizeof(commandCopy) - 1] = '\0'; // Ensure null-termination

            size_t argc = split(commandCopy, ' ', argv, sizeof(argv) / sizeof(argv[0]));

            if (argc == 0) {
                Serial.println("No command entered.");
                return ShellResult::Continue;
            }

            const char* cmd = argv[0];

            if (strcmp(cmd, "ls") == 0) {
                Serial.println("Executing ls command...");
                executeLs(output, workingDirectory);

            } else if (strcmp(cmd, "cd") == 0) {
                // Implement cd command logic here
                Serial.println("Executing cd command...");
                executeCd(output, argv[1], workingDirectory, workingDirectoryNameLength);
            } else if (strcmp(cmd, "pwd") == 0) {
                Serial.println("Executing pwd command...");
                // Implement pwd command logic here
                executeOtherCommands(output, command);
            }
            else if (strcmp(cmd, "exit") == 0) {
                Serial.println("Exiting shell...");
                return ShellResult::Exit;
            }
            else if (strcmp(cmd,"reboot") == 0) {
                Serial.println("Rebooting system...");
                return ShellResult::Reboot;
            }
            else if (strcmp(cmd,"shutdown") == 0) {
                Serial.println("Shutting down system...");
                return ShellResult::Shutdown;
            }
            else if (strcmp(cmd,"clear") == 0) {
                Serial.println("Clearing output...");
                output.clear();
            }
            else {
                Serial.print("Unknown command: ");
                Serial.println(cmd);
                executeOtherCommands(output,"unknown command");
            }
            

        return ShellResult::Continue;
        }


    
    private:
        FileSystemManager fileSystemManager;

};


