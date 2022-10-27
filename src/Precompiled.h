#pragma once

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif // _WIN32

#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <filesystem>

#ifdef _WIN32

// target windows 7
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <shellapi.h>
#include <combaseapi.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif // !PATH_MAX

#define WOLFSSL_LIB
#include <IDE/WIN/user_settings.h>
#undef WOLFSSL_LIB

#else

#include <termios.h>
#include <unistd.h>

#include <wolfssl/options.h>

#endif // _WIN32

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "wolfssl/wolfcrypt/error-crypt.h"
#include "wolfssl/wolfcrypt/coding.h"
#include "wolfssl/wolfcrypt/rsa.h"
#include "wolfssl/wolfcrypt/aes.h"
#include "wolfssl/wolfcrypt/pwdbased.h"
#include "wolfssl/wolfcrypt/hmac.h"
#include "wolfssl/version.h"
#include "curl/curl.h"

using namespace std::chrono_literals;

#define UINT32_MAX_STR_SIZE sizeof("4294967295")
#define UINT64_MAX_STR_SIZE sizeof("18446744073709551615")