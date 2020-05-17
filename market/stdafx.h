#pragma once

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

#define MARKET_SECRET_LEN 6
#define OFFERID_LEN 12
#define STEAMID64_LEN 20