#pragma once

#ifdef _WIN32

#define _CRT_SECURE_NO_WARNINGS

#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <io.h>

#else

#define strcpy_s(a, b, c) strcpy(a, c)
#define strcat_s(a, b, c) strcat(a, c)
#define strncat_s(a, b, c, d) strncat(a, c, d)
#define strncpy_s(a, b, c, d) strncpy(a, c, d)
#define sprintf_s(a, b, ...) sprintf(a, __VA_ARGS__)

#include <termios.h>
#include <unistd.h>

#endif // _WIN32

#include <iostream>
#include <thread>
#include <filesystem>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "wolfssl/options.h"
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