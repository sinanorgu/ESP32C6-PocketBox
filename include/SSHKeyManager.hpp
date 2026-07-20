#pragma once

#include <Arduino.h>
#include <SD.h>
#include <libssh/libssh.h>

class SSHKeyManager
{
public:
    explicit SSHKeyManager(const char* keyPath);

    // Key varsa yükler, yoksa üretip kaydeder.
    // Başarılıysa geçerli bir ssh_key döndürür.
    // Dönen key'i kullanıcı ssh_key_free() ile temizlemelidir.
    ssh_key loadOrCreate();

    // SD karttan mevcut private key'i yükler.
    ssh_key load();

    // Yeni Ed25519 private key üretir.
    ssh_key generate();

    // Private key'i SD karta kaydeder.
    bool save(ssh_key key);

    bool exists() const;

private:
    bool ensureParentDirectory();
    bool readFile(char*& buffer, size_t& length);

private:
    const char* keyPath;
};