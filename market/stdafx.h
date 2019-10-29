#pragma once

#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <iostream>
#include <conio.h>
#include <thread>
using namespace std::chrono_literals;

#pragma warning(push, 0)

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "IDE/WIN/user_settings.h"
#include "wolfssl/wolfcrypt/coding.h"
#include "wolfssl/wolfcrypt/rsa.h"
#include "wolfssl/wolfcrypt/error-crypt.h"
#include "wolfssl/wolfcrypt/asn.h"
#include "wolfssl/wolfcrypt/wc_encrypt.h"
#include "wolfssl/wolfcrypt/hmac.h"
#include "curl/curl.h"

#pragma warning(pop)

#define MARKET_SECRET_LEN 5
#define OFFERID_LEN 11
#define STEAMID64_LEN 18