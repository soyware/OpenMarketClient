#pragma once

namespace Market
{
	const size_t apiKeySz = 31;
	const size_t hashBufSz = 50;
	const int offerTTL = (15 * 60);

	enum class Market
	{
		CSGO,
		DOTA,
		TF2,
		RUST,
		GIFTS,

		COUNT
	};

	const char* marketNames[] =
	{
		"CSGO",
		"DOTA",
		"TF2",
		"RUST",
		"GIFTS",
	};

	const char* marketBaseUrls[] =
	{
		"https://market.csgo.com/api/v2/",
		"https://market.dota2.net/api/v2/",
		"https://tf2.tm/api/v2/",
		"https://rust.tm/api/v2/",
		"https://gifts.tm/api/v2/",
	};

	const size_t marketBaseUrlMaxSz = sizeof("https://market.dota2.net/api/v2/");

	//const bool isMarketP2P[] =
	//{
	//	true,
	//	true,
	//	true,
	//	true,
	//	true,
	//};

	enum class ItemStatus
	{
		SELLING = 1,
		GIVE,
		WAITING_SELLER,
		TAKE,
		GIVEN,
		CANCELLED,
		WAITING_ACCEPT
	};
	
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

	bool Ping(CURL* curl, const char* apiKey)
	{
		const char query[] = "ping?key=";

		const size_t urlBufSz = marketBaseUrlMaxSz - 1 + sizeof(query) - 1 + apiKeySz + 1;
		char url[urlBufSz];

		char* urlEnd = url;
		urlEnd = stpcpy(urlEnd, marketBaseUrls[(int)Market::CSGO]);
		urlEnd = stpcpy(urlEnd, query);
		strcpy(urlEnd, apiKey);

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

		return (curl_easy_perform(curl) == CURLE_OK);
	}

	bool GetItems(CURL* curl, const char* apiKey, int market, rapidjson::Document* outDoc)
	{
		const char query[] = "items?key=";

		const size_t urlBufSz = marketBaseUrlMaxSz - 1 + sizeof(query) - 1 + apiKeySz + 1;
		char url[urlBufSz];

		char* urlEnd = url;
		urlEnd = stpcpy(urlEnd, marketBaseUrls[market]);
		urlEnd = stpcpy(urlEnd, query);
		strcpy(urlEnd, apiKey);

		Curl::CResponse response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		if (curl_easy_perform(curl) != CURLE_OK)
			return false;

		outDoc->Parse(response.data);

		if (outDoc->HasParseError())
			return false;

		if (!(*outDoc)["success"].GetBool())
			return false;

		return true;
	}

	// outOfferId buffer size must be at least offerIdBufSz
	// outPartnerId64 buffer size must be at least UINT64_MAX_STR_SIZE
	bool RequestTake(CURL* curl, const char* apiKey, int market, char* outOfferId, char* outPartnerId64)
	{
		Log(LogChannel::MARKET, "[%s] Requesting details to receive items...", marketNames[market]);

		const char query[] = "trade-request-take?key=";

		const size_t urlBufSz = marketBaseUrlMaxSz - 1 + sizeof(query) - 1 + apiKeySz + 1;
		char url[urlBufSz];

		char* urlEnd = url;
		urlEnd = stpcpy(urlEnd, marketBaseUrls[market]);
		urlEnd = stpcpy(urlEnd, query);
		strcpy(urlEnd, apiKey);

		Curl::CResponse response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		const CURLcode respCode = curl_easy_perform(curl);

		if (respCode != CURLE_OK)
		{
			Curl::PrintError(curl, respCode);
			return false;
		}

		rapidjson::Document parsed;
		parsed.ParseInsitu(response.data);

		if (parsed.HasParseError())
		{
			putsnn("JSON parsing failed\n");
			return false;
		}

		if (!parsed["success"].GetBool())
		{
			putsnn("request unsucceeded\n");
			return false;
		}

		const char* offerId = parsed["trade"].GetString();
		const char* partnerUrl = parsed["profile"].GetString();

		strcpy(outOfferId, offerId);
		stpncpy(outPartnerId64, partnerUrl + 36, strlen(partnerUrl + 36) - 1)[0] = '\0';

		putsnn("ok\n");
		return true;
	}

	bool RequestGiveBot(CURL* curl, const char* apiKey, int market, char* outTradeOfferId, char* outPartnerId64)
	{
		Log(LogChannel::MARKET, "[%s] Requesting details to send items...", marketNames[market]);

		const char query[] = "trade-request-give?key=";

		const size_t urlBufSz = marketBaseUrlMaxSz - 1 + sizeof(query) - 1 + apiKeySz + 1;
		char url[urlBufSz];

		char* urlEnd = url;
		urlEnd = stpcpy(urlEnd, marketBaseUrls[market]);
		urlEnd = stpcpy(urlEnd, query);
		strcpy(urlEnd, apiKey);

		Curl::CResponse response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		const CURLcode respCode = curl_easy_perform(curl);

		if (respCode != CURLE_OK)
		{
			Curl::PrintError(curl, respCode);
			return false;
		}

		rapidjson::Document parsed;
		parsed.ParseInsitu(response.data);

		if (parsed.HasParseError())
		{
			putsnn("JSON parsing failed\n");
			return false;
		}

		if (!parsed["success"].GetBool())
		{
			const auto iterError = parsed.FindMember("error");
			if (iterError != parsed.MemberEnd() && !strcmp(iterError->value.GetString(), "nothing"))
				putsnn("nothing\n");
			else
				putsnn("request unsucceeded\n");

			return false;
		}

		const char* offerId = parsed["trade"].GetString();
		const char* profile = parsed["profile"].GetString();

		strcpy(outTradeOfferId, offerId);
		stpncpy(outPartnerId64, profile + 36, strlen(profile + 36) - 1)[0] = '\0';

		putsnn("ok\n");
		return true;
	}

	bool RequestGiveP2PAll(CURL* curl, const char* apiKey, int market, rapidjson::Document* outDoc)
	{
		Log(LogChannel::MARKET, "[%s] Requesting details to send items...", marketNames[market]);

		const char query[] = "trade-request-give-p2p-all?key=";

		const size_t urlBufSz = marketBaseUrlMaxSz - 1 + sizeof(query) - 1 + apiKeySz + 1;
		char url[urlBufSz];

		char* urlEnd = url;
		urlEnd = stpcpy(urlEnd, marketBaseUrls[market]);
		urlEnd = stpcpy(urlEnd, query);
		strcpy(urlEnd, apiKey);

		Curl::CResponse response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		const CURLcode respCode = curl_easy_perform(curl);

		if (respCode != CURLE_OK)
		{
			Curl::PrintError(curl, respCode);
			return false;
		}

		outDoc->Parse(response.data);

		if (outDoc->HasParseError())
		{
			putsnn("JSON parsing failed\n");
			return false;
		}

		if (!(*outDoc)["success"].GetBool())
		{
			const auto iterError = outDoc->FindMember("error");
			if (iterError != outDoc->MemberEnd() && !strcmp(iterError->value.GetString(), "nothing"))
				putsnn("nothing\n");
			else
				putsnn("request unsucceeded\n");

			return false;
		}

		putsnn("ok\n");
		return true;
	}

	bool GetProfileStatus(CURL* curl, const char* apiKey, rapidjson::Document* outDoc)
	{
		Log(LogChannel::MARKET, "Getting profile status...");

		const char query[] = "test?key=";

		const size_t urlBufSz = marketBaseUrlMaxSz - 1 + sizeof(query) - 1 + apiKeySz + 1;
		char url[urlBufSz];

		char* urlEnd = url;
		urlEnd = stpcpy(urlEnd, marketBaseUrls[(int)Market::CSGO]);
		urlEnd = stpcpy(urlEnd, query);
		strcpy(urlEnd, apiKey);

		Curl::CResponse response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		const CURLcode respCode = curl_easy_perform(curl);

		if (respCode != CURLE_OK)
		{
			Curl::PrintError(curl, respCode);
			return false;
		}

		outDoc->Parse(response.data);

		if (outDoc->HasParseError())
		{
			putsnn("JSON parsing failed\n");
			return false;
		}

		if (!(*outDoc)["success"].GetBool())
		{
			putsnn("request unsucceeded\n");
			return false;
		}

		putsnn("ok\n");
		return true;
	}

	bool CanSell(CURL* curl, const char* apiKey)
	{
		rapidjson::Document docTest;
		if (!GetProfileStatus(curl, apiKey, &docTest))
			return false;

		const rapidjson::Value& status = docTest["status"];

		if (!status["steam_web_api_key"].GetBool())
		{
			Log(LogChannel::MARKET, "Can't sell on the market: Steam web API key not set\n");
			return false;
		}

		if (!status["user_token"].GetBool())
		{
			Log(LogChannel::MARKET, "Can't sell on the market: Steam trade token not set\n");
			return false;
		}

		if (!status["trade_check"].GetBool())
		{
			Log(LogChannel::MARKET, "Can't sell on the market: trade check required - https://market.csgo.com/check\n");
			return false;
		}

		if (!status["site_notmpban"].GetBool())
		{
			Log(LogChannel::MARKET, "Can't sell on the market: banned\n");
			return false;
		}

		return true;
	}

	bool SetSteamApiKey(CURL* curl, const char* marketApiKey, const char* steamApiKey)
	{
		Log(LogChannel::MARKET, "Setting Steam API key on the market...");

		const char query[] = "set-steam-api-key?key=";
		const char querySteamApiKey[] = "&steam-api-key=";

		const size_t urlBufSz = 
			marketBaseUrlMaxSz - 1 + 
			sizeof(query) - 1 + apiKeySz + 
			sizeof(querySteamApiKey) - 1 + Steam::apiKeyBufSz - 1 + 1;

		char url[urlBufSz];

		char* urlEnd = url;
		urlEnd = stpcpy(urlEnd, marketBaseUrls[(int)Market::CSGO]);
		urlEnd = stpcpy(urlEnd, query);
		urlEnd = stpcpy(urlEnd, marketApiKey);
		urlEnd = stpcpy(urlEnd, querySteamApiKey);
		strcpy(urlEnd, steamApiKey);

		Curl::CResponse response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		const CURLcode respCode = curl_easy_perform(curl);

		if (respCode != CURLE_OK)
		{
			Curl::PrintError(curl, respCode);
			return false;
		}

		rapidjson::Document parsed;
		parsed.ParseInsitu(response.data);

		if (parsed.HasParseError())
		{
			putsnn("JSON parsing failed\n");
			return false;
		}

		if (!parsed["success"].GetBool())
		{
			putsnn("request unsucceeded\n");
			return false;
		}

		putsnn("ok\n");
		return true;
	}

	bool SetSteamTradeToken(CURL* curl, const char* apiKey, const char* tradeToken)
	{
		Log(LogChannel::MARKET, "Setting Steam trade token on the market...");

		const char query[] = "set-trade-token?key=";
		const char queryTradeToken[] = "&token=";

		const size_t urlBufSz =
			marketBaseUrlMaxSz - 1 +
			sizeof(query) - 1 + apiKeySz +
			sizeof(queryTradeToken) - 1 + Steam::Trade::tokenBufSz - 1 + 1;

		char url[urlBufSz];

		char* urlEnd = url;
		urlEnd = stpcpy(urlEnd, marketBaseUrls[(int)Market::CSGO]);
		urlEnd = stpcpy(urlEnd, query);
		urlEnd = stpcpy(urlEnd, apiKey);
		urlEnd = stpcpy(urlEnd, queryTradeToken);
		strcpy(urlEnd, tradeToken);

		Curl::CResponse response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		const CURLcode respCode = curl_easy_perform(curl);

		if (respCode != CURLE_OK)
		{
			Curl::PrintError(curl, respCode);
			return false;
		}

		rapidjson::Document parsed;
		parsed.ParseInsitu(response.data);

		if (parsed.HasParseError())
		{
			putsnn("JSON parsing failed\n");
			return false;
		}

		if (!parsed["success"].GetBool())
		{
			putsnn("request unsucceeded\n");
			return false;
		}

		putsnn("ok\n");
		return true;
	}

	// steam login token must be set when calling this
	bool SetSteamDetails(CURL* curl, const char* apiKey, const char* steamApiKey)
	{
		rapidjson::Document docTest;
		if (!GetProfileStatus(curl, apiKey, &docTest))
			return false;

		const rapidjson::Value& status = docTest["status"];

		if (!status["steam_web_api_key"].GetBool())
		{
			// this fails on the first try sometimes, don't know why
			if (!SetSteamApiKey(curl, apiKey, steamApiKey) && !SetSteamApiKey(curl, apiKey, steamApiKey))
				return false;
		}

		if (!status["user_token"].GetBool())
		{
			char tradeToken[Steam::Trade::tokenBufSz];
			if (!Steam::Trade::GetToken(curl, tradeToken) || !SetSteamTradeToken(curl, apiKey, tradeToken))
				return false;
		}

		return true;
	}

	// unused
	bool GoOffline(CURL* curl, const char* apiKey)
	{
		Log(LogChannel::MARKET, "Going offline...");

		const char query[] = "go-offline?key=";

		const size_t urlBufSz =
			marketBaseUrlMaxSz - 1 +
			sizeof(query) - 1 + apiKeySz + 1;

		char url[urlBufSz];

		char* urlEnd = url;
		urlEnd = stpcpy(urlEnd, marketBaseUrls[(int)Market::CSGO]);
		urlEnd = stpcpy(urlEnd, query);
		strcpy(urlEnd, apiKey);

		Curl::CResponse response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		const CURLcode respCode = curl_easy_perform(curl);

		if (respCode != CURLE_OK)
		{
			Curl::PrintError(curl, respCode);
			return false;
		}

		rapidjson::Document parsed;
		parsed.ParseInsitu(response.data);

		if (parsed.HasParseError())
		{
			putsnn("JSON parsing failed\n");
			return false;
		}

		if (!parsed["success"].GetBool())
		{
			putsnn("request unsucceeded\n");
			return false;
		}

		putsnn("ok\n");
		return true;
	}
}