#pragma once

#define OFFER_TOKEN_SIZE 9
#define OFFER_MESSAGE_SIZE sizeof("ACIU ... /trade/3823362517/gijsH9EbZpE/")

namespace Offer
{
	bool Accept(CURL* curl, const char* offerId, const char* partner64)
	{
		Log("Accepting trade offer...");

		// 2 slashes on purpose
		const size_t urlSz = sizeof("https://steamcommunity.com/tradeoffer//accept") - 1 + OFFER_ID_SIZE;
		char url[urlSz] = "https://steamcommunity.com/tradeoffer/";

		strcat_s(url, sizeof(url), offerId);
		strcat_s(url, sizeof(url), "/");
		curl_easy_setopt(curl, CURLOPT_REFERER, url);

		strcat_s(url, sizeof(url), "accept");
		curl_easy_setopt(curl, CURLOPT_URL, url);

		const size_t postFieldsSz = sizeof("serverid=1"
			"&captcha="
			"&tradeofferid="
			"&partner="
			"&sessionid=") - 1 + OFFER_ID_SIZE - 1 + STEAMID64_SIZE - 1 + sizeof(Config::sessionID);

		char postFields[postFieldsSz] = "serverid=1"
			"&captcha="
			"&tradeofferid=";
		strcat_s(postFields, sizeof(postFields), offerId);

		strcat_s(postFields, sizeof(postFields), "&partner=");
		strcat_s(postFields, sizeof(postFields), partner64);

		strcat_s(postFields, sizeof(postFields), "&sessionid=");
		strcat_s(postFields, sizeof(postFields), Config::sessionID);

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

		// Confirmation required: {"tradeid":null,"needs_mobile_confirmation":true,"needs_email_confirmation":true,"email_domain":"web.de"}
		// No confirmation required: {"tradeid":"2251163828378018553"}
		CURLcode res = curl_easy_perform(curl);
		curl_easy_setopt(curl, CURLOPT_REFERER, NULL);

		if (res != CURLE_OK)
		{
			long httpCode;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
			printf("%s (%d)\n", curl_easy_strerror(res), httpCode);
			return false;
		}

		printf("ok\n");
		return true;
	}

	bool Send(CURL* curl, const char* partner32, const char* token, const char* message, const char* items, char* outOfferId)
	{
		Log("Creating trade offer...");

		const size_t refererSz = sizeof("https://steamcommunity.com/tradeoffer/new/?partner=&token=") - 1 +
			STEAMID32_SIZE - 1 + OFFER_TOKEN_SIZE;

		char referer[refererSz] = "https://steamcommunity.com/tradeoffer/new/?partner=";
		strcat_s(referer, sizeof(referer), partner32);
		strcat_s(referer, sizeof(referer), "&token=");
		strcat_s(referer, sizeof(referer), token);
		curl_easy_setopt(curl, CURLOPT_REFERER, referer);

		const size_t postFieldsSz = sizeof("partner="
			"&trade_offer_create_params={\"trade_offer_access_token\":\"\"}"
			"&tradeoffermessage="
			"&json_tradeoffer={\"newversion\":true,\"version\":2,\"me\":{\"assets\":,\"currency\":[],\"ready\":false},\"them\":{\"assets\":[],\"currency\":[],\"ready\":false}}"
			"&sessionid="
			"&serverid=1"
			"&captcha=") - 1 + 
			STEAMID64_SIZE - 1 + OFFER_TOKEN_SIZE - 1 + OFFER_MESSAGE_SIZE - 1 + strlen(items) + sizeof(Config::sessionID);

		char* postFields = (char*)malloc(postFieldsSz);
		sprintf_s(postFields, postFieldsSz,
			"partner=%llu"
			"&trade_offer_create_params={\"trade_offer_access_token\":\"%s\"}"
			"&tradeoffermessage=%s"
			"&json_tradeoffer={\"newversion\":true,\"version\":2,\"me\":{\"assets\":%s,\"currency\":[],\"ready\":false},\"them\":{\"assets\":[],\"currency\":[],\"ready\":false}}"
			"&sessionid=%s"
			"&serverid=1"
			"&captcha=",
			SteamID32To64(atol(partner32)),
			token,
			message,
			items,
			Config::sessionID);

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/tradeoffer/new/send");

		CURLcode res = curl_easy_perform(curl);

		free(postFields);
		curl_easy_setopt(curl, CURLOPT_REFERER, NULL);

		if (res != CURLE_OK)
		{
			long httpCode;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
			printf("%s (%d)\n", curl_easy_strerror(res), httpCode);
			return false;
		}

		rapidjson::Document parsed;
		parsed.Parse(response.data);

		rapidjson::Value::ConstMemberIterator tradeofferid = parsed.FindMember("tradeofferid");
		if (tradeofferid == parsed.MemberEnd())
		{
			printf("request unsucceeded\n");
			return false;
		}

		strcpy_s(outOfferId, OFFER_ID_SIZE, tradeofferid->value.GetString());

		printf("ok\n");
		return true;
	}
}