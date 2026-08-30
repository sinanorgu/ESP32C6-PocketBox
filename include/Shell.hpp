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
    Shutdown,
    OpenEditor
};

size_t split(char* str, char delimiter, char* tokens[], size_t maxTokens);
void executeLs(ShellOutput& output, const char* path);
void executeCd(ShellOutput& output, const char* path, char* currentDirectory, size_t& currentDirectoryLength);
void executeOtherCommands(ShellOutput& output, const char* command);
void executeCat(ShellOutput& output, const char* path, const char* currentDirectory);
void executeRm(ShellOutput& output, const char* path, const char* currentDirectory);
void executeMkdir(ShellOutput& output, const char* path, const char* currentDirectory);
void executeTouch(ShellOutput& output, const char* path, const char* currentDirectory);
void executeCp(ShellOutput& output, const char* sourcePath, const char* destinationPath, const char* currentDirectory);
void executeMv(ShellOutput& output, const char* sourcePath, const char* destinationPath, const char* currentDirectory);
void executePwd(ShellOutput& output, const char* currentDirectory);
void executeRmdir(ShellOutput& output, const char* path, const char* currentDirectory);
void executeCpl(ShellOutput& output, const char* path, const char* currentDirectory);
void executeEcho(ShellOutput& output, const char* command, const char* currentDirectory);
bool prepareNano(ShellOutput& output, const char* path, const char* currentDirectory,
                 char* resolvedPath, size_t resolvedPathSize);

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
                Serial.println("Executing cd command...");
                executeCd(output, argv[1], workingDirectory, workingDirectoryNameLength);
            } else if (strcmp(cmd, "pwd") == 0) {
                Serial.println("Executing pwd command...");
                executePwd(output, workingDirectory);
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
            else if (strcmp(cmd,"cat") == 0) {
                Serial.println("Executing cat command...");
                executeCat(output, argv[1], workingDirectory);
            }
            else if(strcmp(cmd,"rm") == 0) {
                Serial.println("Executing rm command...");
                executeRm(output, argv[1], workingDirectory);
            }
            else if(strcmp(cmd,"mkdir") == 0) {
                Serial.println("Executing mkdir command...");
                executeMkdir(output, argv[1], workingDirectory);
            }
            else if(strcmp(cmd,"touch") == 0) {
                Serial.println("Executing touch command...");
                executeTouch(output, argv[1], workingDirectory);
            }
            else if(strcmp(cmd,"cp") == 0) {
                Serial.println("Executing cp command...");
                executeCp(output, argv[1], argv[2], workingDirectory);
            }
            else if(strcmp(cmd,"mv") == 0) {
                Serial.println("Executing mv command...");
                executeMv(output, argv[1], argv[2], workingDirectory);
            }
            else if(strcmp(cmd,"rmdir") == 0) {
                Serial.println("Executing rmdir command...");
                executeRmdir(output, argv[1], workingDirectory);
            }
            else if(strcmp(cmd,"cpl") == 0) {
                Serial.println("Executing cpl command...");
                executeCpl(output, argc > 1 ? argv[1] : nullptr, workingDirectory);
            }
            else if(strcmp(cmd,"echo") == 0) {
                executeEcho(output, command, workingDirectory);
            }
            else if(strcmp(cmd,"nano") == 0) {
                if (prepareNano(output, argc > 1 ? argv[1] : nullptr, workingDirectory,
                                editorPath, sizeof(editorPath)))
                    return ShellResult::OpenEditor;
            }
            else {
                Serial.print("Unknown command: ");
                Serial.println(cmd);
                executeOtherCommands(output,"unknown command");
            }
            
            

        return ShellResult::Continue;
        }
        void autoComplete(char* lineBuffer, size_t& lineLength, size_t& cursorPosition, ShellOutput& output);
        const char* requestedEditorPath() const { return editorPath; }

    
    private:
        FileSystemManager fileSystemManager;
        char editorPath[256]{};

};
