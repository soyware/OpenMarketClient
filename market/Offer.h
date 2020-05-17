#pragma once

namespace Offer
{
	/*
	bool Check(CURL* curl, const char* offerId, const char* secretCode)
	{
		Log("Checking trade offer...");

		char url[124] = "https://api.steampowered.com/IEconService/GetTradeOffer/v1/?key=";
		strcat_s(url, sizeof(url), Config::steamapikey);
		strcat_s(url, sizeof(url), "&tradeofferid=");
		strcat_s(url, sizeof(url), offerId);
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

		if (doc["response"].ObjectEmpty())
		{
			std::cout << "invalid trade id\n";
			return false;
		}
		else if (strncmp(doc["response"]["offer"]["message"].GetString(), secretCode, MARKET_SECRET_LEN - 1))
		{
			std::cout << "incorrect secret code\n";
			return false;
		}
		else if (doc["response"]["offer"]["trade_offer_state"].GetInt() != 2)
		{
			std::cout << "inactive trade offer\n";
			return false;
		}

		std::cout << "ok\n";
		return true;
	}
	*/

	bool Accept(CURL* curl, const char* offerId, const char* partner64)
	{
		Log("Accepting trade offer...");

		char url[56] = "https://steamcommunity.com/tradeoffer/";
		strcat_s(url, sizeof(url), offerId);
		strcat_s(url, sizeof(url), "/");
		curl_easy_setopt(curl, CURLOPT_REFERER, url);
		strcat_s(url, sizeof(url), "accept");
		curl_easy_setopt(curl, CURLOPT_URL, url);

		char postFields[108] = "tradeofferid=";
		strcat_s(postFields, sizeof(postFields), offerId);

		strcat_s(postFields, sizeof(postFields), "&partner=");
		strcat_s(postFields, sizeof(postFields), partner64);

		strcat_s(postFields, sizeof(postFields), "&sessionid=");
		strcat_s(postFields, sizeof(postFields), g_sessionID);

		strcat_s(postFields, sizeof(postFields), "&serverid=1&"
			"captcha=");

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

		// Confirmation required: {"tradeid":null,"needs_mobile_confirmation":true,"needs_email_confirmation":true,"email_domain":"web.de"}
		// No confirmation required: {"tradeid":"2251163828378018553"}
		CURLcode res = curl_easy_perform(curl);
		curl_easy_setopt(curl, CURLOPT_REFERER, NULL);

		if (res != CURLE_OK)
		{
			long code;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
			std::cout << curl_easy_strerror(res) << " (" << code << ")\n";
			return false;
		}

		std::cout << "ok\n";
		return true;
	}

	bool Send(CURL* curl, const char* partner32, const char* token, const char* message, const char* items, char* outOfferId)
	{
		Log("Creating trade offer...");

		char referer[80] = "https://steamcommunity.com/tradeoffer/new/?partner=";
		strcat_s(referer, sizeof(referer), partner32);
		strcat_s(referer, sizeof(referer), "&token=");
		strcat_s(referer, sizeof(referer), token);
		curl_easy_setopt(curl, CURLOPT_REFERER, referer);

		char postFields[1024];
		sprintf_s(postFields, sizeof(postFields),
			"partner=%llu&"
			"trade_offer_create_params={\"trade_offer_access_token\":\"%s\"}&"
			"tradeoffermessage=%s&"
			"json_tradeoffer={\"newversion\":true,\"version\":2,\"me\":{\"assets\":%s,\"currency\":[],\"ready\":false},\"them\":{\"assets\":[],\"currency\":[],\"ready\":false}}&"
			"sessionid=%s&"
			"serverid=1&"
			"captcha=",
			76561197960265728 + atoi(partner32),
			token,
			message,
			items,
			g_sessionID);

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/tradeoffer/new/send");

		CURLcode res = curl_easy_perform(curl);
		curl_easy_setopt(curl, CURLOPT_REFERER, NULL);

		if (res != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		rapidjson::Document doc;
		doc.Parse(data.data);

		rapidjson::Value::ConstMemberIterator tradeofferid = doc.FindMember("tradeofferid");
		if (tradeofferid == doc.MemberEnd())
		{
			std::cout << "request unsucceeded\n";
			return false;
		}

		strcpy_s(outOfferId, OFFERID_LEN, tradeofferid->value.GetString());
		std::cout << "ok\n";
		return true;
	}
}