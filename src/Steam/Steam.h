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
}

#include "Misc.h"
#include "Captcha.h"
#include "Trade.h"
#include "Guard.h"
#include "Auth.h"