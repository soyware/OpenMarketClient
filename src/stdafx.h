#pragma once

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif // _WIN32

#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <thread>
#include <filesystem>

#ifdef _WIN32

#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <io.h>

#define PATH_MAX MAX_PATH

#define WOLFSSL_LIB
#include <IDE/WIN/user_settings.h>
#undef WOLFSSL_LIB

#else

#include <cassert>

inline char* strcpy_s(char* dest, size_t size, const char* src)
{
    char* ret = strncpy(dest, src, size - 1);
    dest[size - 1] = '\0';
    return ret;
}

inline char* strcat_s(char* dest, size_t size, const char* src)
{
    char* ret = strncat(dest, src, size - 1);
    dest[size - 1] = '\0';
    return ret;
}

inline char* strncpy_s(char* dest, size_t size, const char* src, size_t count)
{
    assert(count <= size);
    char* ret = strncpy(dest, src, count);
    dest[size - 1] = '\0';
    return ret;
}

inline char* strncat_s(char* dest, size_t size, const char* src, size_t count)
{
    assert(count <= size);
    char* ret = strncat(dest, src, count);
    dest[size - 1] = '\0';
    return ret;
}

inline int sprintf_s(char* dest, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(dest, size, format, args);
    va_end(args);
    dest[size - 1] = '\0';
    return ret;
}

#include <termios.h>
#include <unistd.h>

#include <wolfssl/options.h>

#endif // _WIN32

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/wolfcrypt/coding.h"
#include "wolfssl/wolfcrypt/rsa.h"
#include "wolfssl/wolfcrypt/error-crypt.h"
#include "wolfssl/wolfcrypt/aes.h"
#include "wolfssl/wolfcrypt/pwdbased.h"
#include "wolfssl/wolfcrypt/hmac.h"
#include "curl/curl.h"

using namespace std::chrono_literals;

// len + null char
#define OFFER_ID_SIZE 12
#define STEAMID64_SIZE 18
#define STEAMID32_SIZE 11
