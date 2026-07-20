#include "SSHKeyManager.hpp"

SSHKeyManager::SSHKeyManager(const char* keyPath)
    : keyPath(keyPath)
{
}

bool SSHKeyManager::exists() const
{
    return keyPath && SD.exists(keyPath);
}

ssh_key SSHKeyManager::loadOrCreate()
{
    if (exists())
    {
        Serial.println("Loading SSH host key from SD...");

        ssh_key key = load();

        if (key)
        {
            Serial.println("SSH host key loaded.");
            return key;
        }

        Serial.println("Stored SSH host key is invalid.");
        Serial.println("Generating a new host key...");
    }
    else
    {
        Serial.println("SSH host key not found.");
        Serial.println("Generating a new host key...");
    }

    ssh_key key = generate();

    if (!key)
    {
        Serial.println("SSH host key generation failed.");
        return nullptr;
    }

    if (!save(key))
    {
        Serial.println("SSH host key could not be saved.");
        ssh_key_free(key);
        return nullptr;
    }

    Serial.println("SSH host key generated and saved.");

    return key;
}

ssh_key SSHKeyManager::generate()
{
    ssh_key key = nullptr;

    int result = ssh_pki_generate(
        SSH_KEYTYPE_ED25519,
        0,
        &key
    );

    if (result != SSH_OK)
    {
        if (key)
            ssh_key_free(key);

        return nullptr;
    }

    return key;
}

bool SSHKeyManager::save(ssh_key key)
{
    if (!key || !keyPath)
        return false;

    if (!ensureParentDirectory())
    {
        Serial.println("Could not create SSH key directory.");
        return false;
    }

    char* exportedKey = nullptr;

    int result = ssh_pki_export_privkey_base64(
        key,
        nullptr,  // passphrase
        nullptr,  // auth callback
        nullptr,  // callback data
        &exportedKey
    );

    if (result != SSH_OK || !exportedKey)
    {
        Serial.println("Private key export failed.");

        if (exportedKey)
            ssh_string_free_char(exportedKey);

        return false;
    }

    File file = SD.open(keyPath, FILE_WRITE);

    if (!file)
    {
        Serial.println("Could not open SSH key file for writing.");
        ssh_string_free_char(exportedKey);
        return false;
    }

    // FILE_WRITE bazı SD implementasyonlarında sona ekleme yapabilir.
    // Önceden mevcut dosyayı sildiğimizden emin oluyoruz.
    file.close();
    SD.remove(keyPath);

    file = SD.open(keyPath, FILE_WRITE);

    if (!file)
    {
        Serial.println("Could not recreate SSH key file.");
        ssh_string_free_char(exportedKey);
        return false;
    }

    size_t keyLength = strlen(exportedKey);
    size_t written = file.write(
        reinterpret_cast<const uint8_t*>(exportedKey),
        keyLength
    );

    file.flush();
    file.close();

    ssh_string_free_char(exportedKey);

    if (written != keyLength)
    {
        Serial.println("Incomplete SSH key write.");
        SD.remove(keyPath);
        return false;
    }

    return true;
}

ssh_key SSHKeyManager::load()
{
    char* keyText = nullptr;
    size_t keyLength = 0;

    if (!readFile(keyText, keyLength))
        return nullptr;

    ssh_key key = nullptr;

    int result = ssh_pki_import_privkey_base64(
        keyText,
        nullptr,  // passphrase
        nullptr,  // auth callback
        nullptr,  // callback data
        &key
    );

    delete[] keyText;

    if (result != SSH_OK)
    {
        if (key)
            ssh_key_free(key);

        return nullptr;
    }

    return key;
}

bool SSHKeyManager::readFile(char*& buffer, size_t& length)
{
    buffer = nullptr;
    length = 0;

    if (!keyPath)
        return false;

    File file = SD.open(keyPath, FILE_READ);

    if (!file)
    {
        Serial.println("Could not open SSH key file.");
        return false;
    }

    size_t fileSize = file.size();

    if (fileSize == 0 || fileSize > 8192)
    {
        Serial.println("Invalid SSH key file size.");
        file.close();
        return false;
    }

    buffer = new (std::nothrow) char[fileSize + 1];

    if (!buffer)
    {
        Serial.println("Not enough memory to load SSH key.");
        file.close();
        return false;
    }

    size_t bytesRead = file.read(
        reinterpret_cast<uint8_t*>(buffer),
        fileSize
    );

    file.close();

    if (bytesRead != fileSize)
    {
        delete[] buffer;
        buffer = nullptr;

        Serial.println("Incomplete SSH key read.");
        return false;
    }

    buffer[fileSize] = '\0';
    length = fileSize;

    return true;
}

bool SSHKeyManager::ensureParentDirectory()
{
    if (!keyPath || keyPath[0] != '/')
        return false;

    char path[256];

    strncpy(path, keyPath, sizeof(path));
    path[sizeof(path) - 1] = '\0';

    char* lastSlash = strrchr(path, '/');

    if (!lastSlash)
        return false;

    // Dosya doğrudan root altındaysa klasör oluşturmaya gerek yok.
    if (lastSlash == path)
        return true;

    *lastSlash = '\0';

    if (SD.exists(path))
        return true;

    return SD.mkdir(path);
}