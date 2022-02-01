#pragma once

enum ITEM_STATUS
{
	ITEM_STATUS_SELLING = 1,
	ITEM_STATUS_GIVE,
	ITEM_STATUS_WAITING_SELLER,
	ITEM_STATUS_TAKE,
	ITEM_STATUS_GIVEN,
	ITEM_STATUS_CANCELLED,
	ITEM_STATUS_WAITING_ACCEPT
};

enum MARKET_TYPE
{
	MARKET_DOTA,
	MARKET_CSGO,
	MARKET_COUNT
};

namespace Market
{
	rapidjson::SizeType prevItemCount[MARKET_COUNT];

	const char* marketNames[] =
	{
		"DOTA",
		"CSGO"
	};

	void Init()
	{
		for (size_t i = 0; i < MARKET_COUNT; ++i)
			prevItemCount[i] = (rapidjson::SizeType)-1;
	}

	bool Ping(CURL* curl, const char* marketKey)
	{
		const size_t urlSz = sizeof("https://market.dota2.net/api/v2/ping?key=") - 1 + sizeof(Config::marketApiKey);

		char url[urlSz] = "https://market.dota2.net/api/v2/ping?key=";
		strcat_s(url, sizeof(url), marketKey);

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

		return (curl_easy_perform(curl) == CURLE_OK);
	}

	int CheckItems(CURL* curl, int market)
	{
		const size_t urlSz = sizeof("https://market.dota2.net/api/v2/items?key=") - 1 + sizeof(Config::marketApiKey);

		char url[urlSz];
		switch (market)
		{
		case MARKET_DOTA:
			strcpy_s(url, sizeof(url), "https://market.dota2.net/api/v2/items?key=");
			break;

		case MARKET_CSGO:
			strcpy_s(url, sizeof(url), "https://market.csgo.com/api/v2/items?key=");
			break;
		}
		strcat_s(url, sizeof(url), Config::marketApiKey);

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		if (curl_easy_perform(curl) != CURLE_OK)
			return 0;

		rapidjson::Document parsed;
		parsed.Parse(response.data);

		if (!parsed["success"].GetBool())
			return 0;

		int ret = ITEM_STATUS_SELLING;

		const rapidjson::Value& items = parsed["items"];
		rapidjson::SizeType itemsCount = (items.IsNull() ? 0 : items.Size());

		for (rapidjson::SizeType i = 0; i < itemsCount; ++i)
		{
			// returned status is a character
			int status = (items[i]["status"].GetString()[0] - '0');

			if (status == ITEM_STATUS_GIVE || status == ITEM_STATUS_TAKE)
			{
				const char* name = items[i]["market_hash_name"].GetString();

				if (ret == ITEM_STATUS_SELLING)
				{
					ret = status;
					Log("%s: %s \"%s\"", marketNames[market], ((status == ITEM_STATUS_GIVE) ? "Sold" : "Bought"), name);
				}
				else
					printf(", \"%s\"", name);
			}
		}

		if (ret == ITEM_STATUS_SELLING)
		{
			if (prevItemCount[market] != itemsCount)
			{
				prevItemCount[market] = itemsCount;
				Log("%s: %u listing(s)\n", marketNames[market], itemsCount);
			}
		}
		else
			printf("\n");

		return ret;
	}

	bool RequestTakeDetails(CURL* curl, int market, char* outOfferId, char* outPartner64/*, char* outSecret*/)
	{
		Log("Requesting details to recieve item...");

		const size_t urlSz = sizeof("https://market.dota2.net/api/v2/trade-request-take?key=") - 1 + sizeof(Config::marketApiKey);

		char url[urlSz];
		switch (market)
		{
		case MARKET_DOTA:
			strcpy_s(url, sizeof(url), "https://market.dota2.net/api/v2/trade-request-take?key=");
			break;

		case MARKET_CSGO:
			strcpy_s(url, sizeof(url), "https://market.csgo.com/api/v2/trade-request-take?key=");
			break;
		}
		strcat_s(url, sizeof(url), Config::marketApiKey);

		curl_easy_setopt(curl, CURLOPT_URL, url);

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			printf("request failed\n");
			return false;
		}

		rapidjson::Document parsed;
		parsed.Parse(response.data);

		if (!parsed["success"].GetBool())
		{
			printf("request unsucceeded\n");
			return false;
		}

		strcpy_s(outOfferId, OFFER_ID_SIZE, parsed["trade"].GetString());
		strncpy_s(outPartner64, STEAMID64_SIZE, &parsed["profile"].GetString()[36], (STEAMID64_SIZE - 1));
		//strcpy_s(outSecret, MARKET_SECRET_SIZE, doc["secret"].GetString());
		
		printf("ok\n");
		return true;
	}

	bool RequestGiveDetails(CURL* curl, rapidjson::Document* outDoc)
	{
		Log("Requesting details to send items...");

		const size_t urlSz = sizeof("https://market.csgo.com/api/v2/trade-request-give-p2p-all?key=") - 1 + sizeof(Config::marketApiKey);
		char url[urlSz] = "https://market.csgo.com/api/v2/trade-request-give-p2p-all?key=";
		strcat_s(url, sizeof(url), Config::marketApiKey);

		curl_easy_setopt(curl, CURLOPT_URL, url);

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			printf("request failed\n");
			return false;
		}

		outDoc->Parse(response.data);

		if (!(*outDoc)["success"].GetBool())
		{
			rapidjson::Value::ConstMemberIterator error = outDoc->FindMember("error");
			if (error != outDoc->MemberEnd() && !strcmp(error->value.GetString(), "nothing"))
				printf("nothing\n");
			else
				printf("request unsucceeded\n");

			return false;
		}

		printf("ok\n");
		return true;
	}
}