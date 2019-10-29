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
	void Ping(CURL* curl, const char* marketKey)
	{
		Log("Sending ping...");

		char url[73] = "https://market.dota2.net/api/v2/ping?key=";
		strcat_s(url, sizeof(url), marketKey);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

		std::cout << ((curl_easy_perform(curl) == CURLE_OK) ? "ok" : "fail") << '\n';
	}

	int CheckItems(CURL* curl, int market)
	{
		char url[74];
		switch (market)
		{
		case MARKET_DOTA:
			Log("Checking dota items...");
			strcpy_s(url, sizeof(url), "https://market.dota2.net/api/v2/items?key=");
			break;

		case MARKET_CSGO:
			Log("Checking csgo items...");
			strcpy_s(url, sizeof(url), "https://market.csgo.com/api/v2/items?key=");
			break;
		}
		strcat_s(url, sizeof(url), Config::marketapikey);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "request failed\n";
			return 0;
		}

		rapidjson::Document doc;
		doc.Parse(data.data);

		if (!doc["success"].GetBool())
		{
			std::cout << "request unsucceeded\n";
			return 0;
		}

		const rapidjson::Value& items = doc["items"];
		if (items.IsNull())
		{
			std::cout << "no items\n";
			return ITEM_STATUS_SELLING;
		}

		for (rapidjson::SizeType i = 0; i < items.Size(); ++i)
		{
			// status is a character
			int status = items[i]["status"].GetString()[0] - '0';
			if (status == ITEM_STATUS_GIVE)
			{
				std::cout << "sold\n";
				return ITEM_STATUS_GIVE;
			}
			else if (status == ITEM_STATUS_TAKE)
			{
				std::cout << "bought\n";
				return ITEM_STATUS_TAKE;
			}
		}

		std::cout << "ok\n";
		return ITEM_STATUS_SELLING;
	}

	bool RequestOffer(CURL* curl, int market, bool take, char* outOfferId, char* outPartner64/*, char* outSecret*/)
	{
		Log("Requesting trade offer...");

		char url[87];
		switch (market)
		{
		case MARKET_DOTA:
			strcpy_s(url, sizeof(url), take ? "https://market.dota2.net/api/v2/trade-request-take?key=" :
				"https://market.dota2.net/api/v2/trade-request-give?key=");
			break;

		case MARKET_CSGO:
			strcpy_s(url, sizeof(url), "https://market.csgo.com/api/v2/trade-request-take?key=");
			break;
		}
		strcat_s(url, sizeof(url), Config::marketapikey);
		curl_easy_setopt(curl, CURLOPT_URL, url);

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		rapidjson::Document doc;
		doc.Parse(data.data);

		if (!doc["success"].GetBool())
		{
			std::cout << "request unsucceeded\n";
			return false;
		}

		strcpy_s(outOfferId, OFFERID_LEN, doc["trade"].GetString());
		strncpy_s(outPartner64, STEAMID64_LEN, &doc["profile"].GetString()[36], (STEAMID64_LEN - 1));
		//strcpy_s(outSecret, MARKET_SECRET_LEN, doc["secret"].GetString());
		std::cout << "ok\n";
		return true;
	}

	bool RequestDetails(CURL* curl, rapidjson::Document* outDoc)
	{
		Log("Requesting details for trade offers...");

		char url[94] = "https://market.csgo.com/api/v2/trade-request-give-p2p-all?key=";
		strcat_s(url, sizeof(url), Config::marketapikey);
		curl_easy_setopt(curl, CURLOPT_URL, url);

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		outDoc->Parse(data.data);

		if (!(*outDoc)["success"].GetBool())
		{
			rapidjson::Value::ConstMemberIterator error = outDoc->FindMember("error");
			if (error != outDoc->MemberEnd() && !strcmp(error->value.GetString(), "nothing"))
				std::cout << "nothing new\n";
			else
				std::cout << "request unsucceeded\n";

			return false;
		}

		std::cout << "ok\n";
		return true;
	}
}