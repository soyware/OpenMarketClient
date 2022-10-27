#pragma once

namespace Steam
{
	namespace Trade
	{
		const size_t offerIdBufSz = UINT64_MAX_STR_SIZE;
		const size_t tokenBufSz = 8 + 1;
		const size_t msgBufSz = sizeof("ABCD ... /trade/") - 1 + UINT64_MAX_STR_SIZE - 1 + sizeof("/abcde12345-/") - 1 + 1;

		enum class ETradeOfferState
		{
			INVALID = 1,
			ACTIVE = 2,
			ACCEPTED = 3,
			COUNTERED = 4,
			EXPIRED = 5,
			CANCELED = 6,
			DECLINED = 7,
			INVALID_ITEMS = 8,
			CREATED_NEEDS_CONFIRMATION = 9,
			CANCELED_BY_SECOND_FACTOR = 10,
			IN_ESCROW = 11,
		};

		bool Accept(CURL* curl, const char* sessionId, const char* offerId, const char* partnerId64)
		{
			Log(LogChannel::STEAM, "Accepting trade offer...");

			const char urlStart[] = "https://steamcommunity.com/tradeoffer/";

			const size_t urlBufSz = sizeof(urlStart) - 1 + offerIdBufSz - 1 + sizeof("/accept") - 1 + 1;
			char url[urlBufSz];

			char* urlEnd = url;
			urlEnd = stpcpy(urlEnd, urlStart);
			urlEnd = stpcpy(urlEnd, offerId);
			urlEnd = stpcpy(urlEnd, "/");

			// libcurl copies the string
			curl_easy_setopt(curl, CURLOPT_REFERER, url);

			strcpy(urlEnd, "accept");

			curl_easy_setopt(curl, CURLOPT_URL, url);

			const char postFieldSession[] = "serverid=1&sessionid=";
			const char postFieldPartnerId64[] = "&partner=";
			const char postFieldOffer[] = "&tradeofferid=";

			const size_t postFieldsBufSz =
				sizeof(postFieldSession) - 1 + sessionIdBufSz - 1 +
				sizeof(postFieldPartnerId64) - 1 + UINT64_MAX_STR_SIZE - 1 +
				sizeof(postFieldOffer) - 1 + offerIdBufSz - 1 + 1;

			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldSession);
			postFieldsEnd = stpcpy(postFieldsEnd, sessionId);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldPartnerId64);
			postFieldsEnd = stpcpy(postFieldsEnd, partnerId64);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldOffer);
			strcpy(postFieldsEnd, offerId);

			// Confirmation required: {"tradeid":null,"needs_mobile_confirmation":true,"needs_email_confirmation":true,"email_domain":"gmail.com"}
			// No confirmation required: {"tradeid":"2251163828378018000"}
			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

			const CURLcode respCode = curl_easy_perform(curl);

			curl_easy_setopt(curl, CURLOPT_REFERER, NULL);

			if (respCode != CURLE_OK)
			{
				Curl::PrintError(curl, respCode);
				return false;
			}

			putsnn("ok\n");
			return true;
		}

		// outOfferId buffer size must be at least offerIdBufSz
		bool Send(CURL* curl, const char* sessionId, uint32_t nPartnerId32, const char* token, const char* message, const char* assets, char* outOfferId)
		{
			Log(LogChannel::STEAM, "Sending trade offer...");

			const std::string partnerId32(std::to_string(nPartnerId32));
			const std::string partnerId64(std::to_string(SteamID32To64(nPartnerId32)));

			const char refererStart[] = "https://steamcommunity.com/tradeoffer/new/?partner=";
			const char refererToken[] = "&token=";

			const size_t refererBufSz =
				sizeof(refererStart) - 1 + UINT32_MAX_STR_SIZE - 1 +
				sizeof(refererToken) - 1 + tokenBufSz - 1 + 1;

			char referer[refererBufSz];

			char* refererEnd = referer;
			refererEnd = stpcpy(refererEnd, refererStart);
			refererEnd = stpcpy(refererEnd, partnerId32.c_str());
			refererEnd = stpcpy(refererEnd, refererToken);
			strcpy(refererEnd, token);

			curl_easy_setopt(curl, CURLOPT_REFERER, referer);

			const char postFieldSession[] = "serverid=1&sessionid=";
			const char postFieldPartnerId64[] = "&partner=";

			const char postFieldAssets[] = "&json_tradeoffer={\"newversion\":true,\"version\":2,\"me\":{\"assets\":";

			const char postFieldToken[] = ",\"currency\":[],\"ready\":false},\"them\":{\"assets\":[],\"currency\":[],\"ready\":false}}"
				"&trade_offer_create_params={\"trade_offer_access_token\":\"";

			const char postFieldMessage[] = "\"}"
				"&tradeoffermessage=";

			const size_t postFieldsBufSz =
				sizeof(postFieldSession) - 1 + sessionIdBufSz - 1 +
				sizeof(postFieldPartnerId64) - 1 + UINT64_MAX_STR_SIZE - 1 +
				sizeof(postFieldAssets) - 1 + strlen(assets) +
				sizeof(postFieldToken) - 1 + tokenBufSz - 1 +
				sizeof(postFieldMessage) - 1 + msgBufSz - 1 + 1;

			char* postFields = (char*)malloc(postFieldsBufSz);
			if (!postFields)
			{
				putsnn("allocation failed\n");
				return false;
			}

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldSession);
			postFieldsEnd = stpcpy(postFieldsEnd, sessionId);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldPartnerId64);
			postFieldsEnd = stpcpy(postFieldsEnd, partnerId64.c_str());
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldAssets);
			postFieldsEnd = stpcpy(postFieldsEnd, assets);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldToken);
			postFieldsEnd = stpcpy(postFieldsEnd, token);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldMessage);
			strcpy(postFieldsEnd, message);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/tradeoffer/new/send");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

			const CURLcode respCode = curl_easy_perform(curl);

			free(postFields);

			curl_easy_setopt(curl, CURLOPT_REFERER, NULL);

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

			const auto iterOfferId = parsed.FindMember("tradeofferid");
			if (iterOfferId == parsed.MemberEnd())
			{
				putsnn("request unsucceeded\n");
				return false;
			}

			strcpy(outOfferId, iterOfferId->value.GetString());

			putsnn("ok\n");
			return true;
		}

		bool Cancel(CURL* curl, const char* sessionId, const char* offerId)
		{
			Log(LogChannel::STEAM, "Cancelling trade offer...");

			const char urlStart[] = "https://steamcommunity.com/tradeoffer/";

			const size_t urlBufSz = sizeof(urlStart) - 1 + offerIdBufSz - 1 + sizeof("/cancel") - 1 + 1;
			char url[urlBufSz];

			char* urlEnd = url;
			urlEnd = stpcpy(urlEnd, urlStart);
			urlEnd = stpcpy(urlEnd, offerId);
			strcpy(urlEnd, "/cancel");

			const char postFieldSession[] = "sessionid=";

			const size_t postFieldsBufSz = sizeof(postFieldSession) - 1 + sessionIdBufSz - 1 + 1;
			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldSession);
			strcpy(postFieldsEnd, sessionId);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

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

			const auto iterOfferId = parsed.FindMember("tradeofferid");
			if ((iterOfferId == parsed.MemberEnd()) || strcmp(iterOfferId->value.GetString(), offerId))
			{
				putsnn("request unsucceeded\n");
				return false;
			}

			putsnn("ok\n");
			return true;
		}

		bool GetOffers(CURL* curl, const char* apiKey, bool getSent, bool getReceived, bool getDescriptions, 
			bool activeOnly, bool historicalOnly, const char* language, uint32_t timeHistoricalCutoff,
			uint32_t cursor, rapidjson::Document* outDoc)
		{
			//Log(LogChannel::STEAM, "Getting trade offers...");

			const char urlStart[] = "https://api.steampowered.com/IEconService/GetTradeOffers/v1/?key=";
			const char urlSent[] = "&get_sent_offers=true";
			const char urlReceived[] = "&get_received_offers=true";
			const char urlDesc[] = "&get_descriptions=true";
			const char urlActive[] = "&active_only=true";
			const char urlHistorical[] = "&historical_only=true";
			const char urlLang[] = "&language=";
			const char urlTime[] = "&time_historical_cutoff=";
			const char urlCursor[] = "&cursor=";

			const size_t urlBufSz = 
				sizeof(urlStart) - 1 + apiKeyBufSz - 1 +
				sizeof(urlSent) - 1 +
				sizeof(urlReceived) - 1 +
				sizeof(urlDesc) - 1 +
				sizeof(urlActive) - 1 +
				sizeof(urlHistorical) - 1 +
				sizeof(urlLang) - 1 + sizeof("portuguese") - 1 + // longest steam language
				sizeof(urlTime) - 1 + UINT32_MAX_STR_SIZE - 1 +
				sizeof(urlCursor) - 1 + UINT32_MAX_STR_SIZE - 1 + 1;

			char url[urlBufSz];

			char* urlEnd = url;
			urlEnd = stpcpy(urlEnd, urlStart);
			urlEnd = stpcpy(urlEnd, apiKey);

			if (getSent)
				urlEnd = stpcpy(urlEnd, urlSent);

			if (getReceived)
				urlEnd = stpcpy(urlEnd, urlReceived);

			if (getDescriptions)
				urlEnd = stpcpy(urlEnd, urlDesc);

			if (activeOnly)
				urlEnd = stpcpy(urlEnd, urlActive);

			if (historicalOnly)
				urlEnd = stpcpy(urlEnd, urlHistorical);

			if (language)
			{
				urlEnd = stpcpy(urlEnd, urlLang);
				urlEnd = stpcpy(urlEnd, language);
			}

			if (timeHistoricalCutoff)
			{
				urlEnd = stpcpy(urlEnd, urlTime);
				urlEnd = stpcpy(urlEnd, std::to_string(timeHistoricalCutoff).c_str());
			}

			if (cursor)
			{
				urlEnd = stpcpy(urlEnd, urlCursor);
				strcpy(urlEnd, std::to_string(cursor).c_str());
			}

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

			const CURLcode respCode = curl_easy_perform(curl);

			if (respCode != CURLE_OK)
			{
				//Curl::PrintError(curl, respCode);
				return false;
			}

			outDoc->Parse(response.data);

			if (outDoc->HasParseError())
			{
				//putsnn("JSON parsing failed\n");
				return false;
			}

			const auto iterResponse = outDoc->FindMember("response");
			if (iterResponse == outDoc->MemberEnd())
			{
				//putsnn("request unsucceeded\n");
				return false;
			}

			//putsnn("ok\n");
			return true;
		}

		// out buffer size must be at least tokenBufSz
		bool GetToken(CURL* curl, char* out)
		{
			Log(LogChannel::STEAM, "Getting trade token...");

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/my/tradeoffers/privacy");
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

			const CURLcode respCode = curl_easy_perform(curl);

			if (respCode != CURLE_OK)
			{
				Curl::PrintError(curl, respCode);
				return false;
			}

			const char searchStr[] = "https://steamcommunity.com/tradeoffer/new/?partner=";

			const char* tradeUrl = strstr(response.data, searchStr);
			if (!tradeUrl)
			{
				putsnn("trade URL not found\n");
				return false;
			}

			const char* tradeToken = (char*)memchr(
				tradeUrl + sizeof(searchStr) - 1, 
				'=', 
				UINT32_MAX_STR_SIZE - 1 + sizeof("&token=") - 1);

			if (!tradeToken)
			{
				putsnn("trade token not found\n");
				return false;
			}

			++tradeToken;

			stpncpy(out, tradeToken, Steam::Trade::tokenBufSz - 1)[0] = '\0';

			putsnn("ok\n");
			return true;
		}
	}
}