#pragma once

namespace Steam
{
	namespace Auth
	{
		const size_t jwtBufSz = 600;
	}

	void RateLimit()
	{
		static std::chrono::high_resolution_clock::time_point nextRequestTime;
		std::this_thread::sleep_until(nextRequestTime);

		const auto curTime = std::chrono::high_resolution_clock::now();
		const auto requestInterval = 1s;
		nextRequestTime = curTime + requestInterval;
	}

	CURLcode curl_easy_perform(CURL* curl)
	{
		RateLimit();

		return ::curl_easy_perform(curl);
	}

	inline uint64_t SteamID32To64(uint32_t id32)
	{
		return (id32 | 0x110000100000000);
	}

	inline uint32_t SteamID64To32(uint64_t id64)
	{
		return (id64 & 0xFFFFFFFF);
	}
}

#include "Misc.h"
#include "Captcha.h"
#include "Trade.h"
#include "Guard.h"
#include "Auth.h"