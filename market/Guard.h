#pragma once

// Credits: https://github.com/geel9/SteamAuth/blob/master/SteamAuth/SteamGuardAccount.cs

#define TWOFA_SIZE 6

#define CONF_TAG_SIZE 32
#define CONF_ID_SIZE 12
#define CONF_KEY_SIZE 20

// multiply by 3 due to url encoding
#define CONF_QUEUE_PARAMS_SIZE (sizeof("p=&a=&k=&t=&m=android&tag=") - 1 + \
	sizeof(Config::deviceid) - 1 + \
	sizeof(Config::steamid64) - 1 + \
	PlainToBase64Size(WC_SHA_DIGEST_SIZE, WC_NO_NL_ENC) * 3 + \
	sizeof("4294967295") - 1 + \
	CONF_TAG_SIZE)

namespace Guard
{
	time_t timeDiff;
	const char charPool[] = "23456789BCDFGHJKMNPQRTVWXY";

	bool Sync(CURL* curl)
	{
		Log("Syncing time with steam...");

		curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/ITwoFactorService/QueryTime/v1");
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "steamid=0");

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curl_write_function);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		rapidjson::Document parsed;
		parsed.Parse(response.data);

		timeDiff = time(nullptr) - atoll(parsed["response"]["server_time"].GetString());

		std::cout << "ok\n";
		return true;
	}

	inline time_t GetSteamTime()
	{
		return time(nullptr) + timeDiff;
	}

	bool GenerateCode(char* out)
	{
		Log("Generating 2FA code...");

		byte sharedRaw[Base64ToPlainSize(sizeof(Config::shared) - 1)];
		word32 sharedRawLen = sizeof(sharedRaw);

		if (Base64_Decode((const byte*)Config::shared, sizeof(Config::shared) - 1, sharedRaw, &sharedRawLen))
		{
			std::cout << "base64 decoding failed\n";
			return false;
		}

		time_t timestamp = GetSteamTime() / 30;

		byte timeArray[sizeof(timestamp)];
		for (int i = sizeof(timestamp) - 1; i >= 0; --i)
		{
			timeArray[i] = (byte)(timestamp & 0xFF);
			timestamp >>= 8;
		}

		Hmac hmac;
		byte result[WC_SHA_DIGEST_SIZE];

		if (wc_HmacSetKey(&hmac, WC_SHA, sharedRaw, sharedRawLen) || wc_HmacUpdate(&hmac, timeArray, sizeof(timeArray)) || wc_HmacFinal(&hmac, result))
		{
			std::cout << "fail\n";
			return false;
		}

		byte start = (byte)(result[sizeof(result) - 1] & 0xF);

		int codePoint = (((result[start] & 0x7F) << 24) | 
			((result[start + 1] & 0xFF) << 16) | 
			((result[start + 2] & 0xFF) << 8) | 
			(result[start + 3] & 0xFF));

		for (unsigned int i = 0; i < (TWOFA_SIZE - 1); ++i)
		{
			out[i] = charPool[codePoint % (sizeof(charPool) - 1)];
			codePoint /= (sizeof(charPool) - 1);
		}
		out[TWOFA_SIZE - 1] = '\0';

		std::cout << "ok\n";
		return true;
	}

	bool GenerateConfirmationHash(const char* tag, byte* out, word32* outLen)
	{
		time_t timestamp = GetSteamTime();

		byte msg[sizeof(timestamp) + CONF_TAG_SIZE];
		for (int i = sizeof(timestamp) - 1; i >= 0; --i)
		{
			msg[i] = (byte)(timestamp & 0xFF);
			timestamp >>= 8;
		}

		const size_t tagLen = strlen(tag);
		strcpy_s((char*)(msg + sizeof(timestamp)), CONF_TAG_SIZE, tag);

		byte identityRaw[Base64ToPlainSize(sizeof(Config::identity) - 1)];
		word32 identityRawLen = sizeof(identityRaw);

		if (Base64_Decode((const byte*)Config::identity, sizeof(Config::identity) - 1, identityRaw, &identityRawLen))
			return false;
		
		Hmac hmac;
		byte hashRaw[WC_SHA_DIGEST_SIZE];

		bool hmacFailed = (wc_HmacSetKey(&hmac, WC_SHA, identityRaw, identityRawLen) || 
			wc_HmacUpdate(&hmac, msg, sizeof(timestamp) + tagLen) || 
			wc_HmacFinal(&hmac, hashRaw));

		if (hmacFailed || Base64_Encode_NoNl(hashRaw, sizeof(hashRaw), out, outLen))
			return false;

		return true;
	}

	bool GenerateConfirmationQueryParams(CURL* curl, const char* tag, char* out)
	{
		byte hash[PlainToBase64Size(WC_SHA_DIGEST_SIZE, WC_NO_NL_ENC)];
		word32 hashLen = sizeof(hash);

		if (!GenerateConfirmationHash(tag, hash, &hashLen))
			return false;

		char* escHash = curl_easy_escape(curl, (const char*)hash, hashLen);

		sprintf_s(out, CONF_QUEUE_PARAMS_SIZE, "p=%s"
			"&a=%s"
			"&k=%s"
			"&t=%lld"
			"&m=android&tag=%s",
			Config::deviceid,
			Config::steamid64,
			escHash,
			GetSteamTime(),
			tag);

		curl_free(escHash);

		return true;
	}

	bool FetchConfirmations(CURL* curl, CURLdata* out)
	{
		Log("Fetching confirmations...");

		char postFields[CONF_QUEUE_PARAMS_SIZE];
		if (!GenerateConfirmationQueryParams(curl, "conf", postFields))
		{
			std::cout << "query params generation failed\n";
			return false;
		}

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)out);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/mobileconf/conf");
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		if (strstr(out->data, "Oh nooooooes!"))
		{
			std::cout << "fail\n";
			return false;
		}

		if (strstr(out->data, "Nothing to confirm"))
		{
			std::cout << "none\n";
			return true;
		}

		std::cout << "ok\n";
		return true;
	}

	bool FindConfirmationParams(const char* confirmations, const char* offerId, const char** outId, const char** outKey)
	{
		const size_t offerAttribSz = sizeof("data-creator=\"") - 1 + OFFER_ID_SIZE;
		char offerAttrib[offerAttribSz] = "data-creator=\"";
		strcat_s(offerAttrib, sizeof(offerAttrib), offerId);

		const char* pAttrib = strstr(confirmations, offerAttrib);
		if (!pAttrib)
			return false;

		const char* tag = pAttrib;
		do --tag; while (tag[0] != '<');
		*outId = strstr(tag, "data-confid=") + sizeof("data-confid=\"") - 1;
		*outKey = strstr(*outId, "data-key=") + sizeof("data-key=\"") - 1;

		return true;
	}

	bool AcceptConfirmations(CURL* curl, char(* const offerIds)[OFFER_ID_SIZE], size_t offerCount)
	{
		CURLdata confirmations;
		if (!FetchConfirmations(curl, &confirmations))
			return false;

		Log("Accepting trade offer confirmations...");

		const size_t confParamsLen = (sizeof("&cid[]=&ck[]=") - 1 + CONF_ID_SIZE - 1 + CONF_KEY_SIZE - 1);
		const size_t postFieldsSz = CONF_QUEUE_PARAMS_SIZE - 1 + sizeof("&op=allow") + offerCount * confParamsLen;

		char* postFields = new char[postFieldsSz];
		if (!GenerateConfirmationQueryParams(curl, "allow", postFields))
		{
			delete[] postFields;
			std::cout << "query params generation failed\n";
			return false;
		}

		strcat_s(postFields, postFieldsSz, "&op=allow");

		int confCount = 0;

		for (size_t i = 0; i < offerCount; ++i)
		{
			const char* pId;
			const char* pKey;
			if (!FindConfirmationParams(confirmations.data, offerIds[i], &pId, &pKey))
				continue;

			strcat_s(postFields, postFieldsSz, "&cid[]=");
			strncat_s(postFields, postFieldsSz, pId, (strchr(pId, '\"') - pId));
			strcat_s(postFields, postFieldsSz, "&ck[]=");
			strncat_s(postFields, postFieldsSz, pKey, (strchr(pKey, '\"') - pKey));

			++confCount;
		}

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/mobileconf/multiajaxop");
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

		CURLcode res = curl_easy_perform(curl);

		delete[] postFields;

		if (res != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		rapidjson::Document parsed;
		parsed.Parse(response.data);

		if (!parsed["success"].GetBool())
		{
			std::cout << "request unsucceeded\n";
			return false;
		}

		if (confCount != offerCount)
			std::cout << "confirmed " << confCount << " out of " << offerCount << "offers\n";
		else
			std::cout << "ok\n";

		return true;
	}
}