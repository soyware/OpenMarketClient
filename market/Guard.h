#pragma once

// Credits:
//	https://github.com/ran-sama/steam_hardware_authenticator/blob/master/steam_authenticator_ds3231.c
//	https://github.com/geel9/SteamAuth/blob/master/SteamAuth/SteamGuardAccount.cs

#define TWOFA_SIZE 6

namespace Guard
{
	int timeDiff;
	const char charPool[] = "23456789BCDFGHJKMNPQRTVWXY";

	bool Sync(CURL* curl)
	{
		Log("Syncing time with steam...");

		curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/ITwoFactorService/QueryTime/v1");
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

		const char field[] = "steamid=0";
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, field);

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_function);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

		rapidjson::Document doc;
		doc.Parse(data.data);

		timeDiff = time(nullptr) - atoi(doc["response"]["server_time"].GetString());

		std::cout << "ok\n";
		return true;
	}

	inline int GetSteamTime()
	{
		return time(nullptr) + timeDiff;
	}

	bool GenerateCode(char* out)
	{
		Log("Generating 2FA code...");

		byte key[int(sizeof(Config::shared) / 1.3f)];
		size_t keylen = sizeof(key);

		if (Base64_Decode((const byte*)Config::shared, sizeof(Config::shared) - 1, key, &keylen))
		{
			std::cout << "base64 decoding failed\n";
			return false;
		}

		time_t timestamp = GetSteamTime() / 30;

		byte timeArray[8];
		for (size_t i = sizeof(timeArray); i > 0; --i)
		{
			timeArray[i - 1] = (byte)timestamp;
			timestamp >>= 8;
		}

		Hmac hmac;
		byte result[SHA_DIGEST_SIZE];

		if (wc_HmacSetKey(&hmac, WC_SHA, key, keylen) || wc_HmacUpdate(&hmac, timeArray, sizeof(timeArray)) || wc_HmacFinal(&hmac, result))
		{
			std::cout << "generation failed\n";
			return false;
		}

		byte b = (byte)(result[19] & 0xF);
		int codePoint = (result[b] & 0x7F) << 24 | (result[b + 1] & 0xFF) << 16 | (result[b + 2] & 0xFF) << 8 | (result[b + 3] & 0xFF);

		for (int i = 0; i < (TWOFA_SIZE - 1); ++i)
		{
			out[i] = charPool[codePoint % (sizeof(charPool) - 1)];
			codePoint /= (sizeof(charPool) - 1);
		}
		out[TWOFA_SIZE - 1] = '\0';

		std::cout << "ok\n";
		return true;
	}

	bool GenerateConfirmationHash(const char* tag, byte* out, word32* outlen)
	{
		Log("Generating confirmation hash...");

		size_t taglen = strlen(tag);
		int arrlen = ((taglen > 32) ? 8 + 32 : 8 + taglen);

		time_t timestamp = GetSteamTime();

		byte* timeArray = new byte[arrlen];
		for (size_t i = 8; i > 0; --i)
		{
			timeArray[i - 1] = (byte)timestamp;
			timestamp >>= 8;
		}

		memcpy_s((char*)(timeArray + 8), arrlen - 8, tag, taglen);

		byte identityraw[int(sizeof(Config::identity) / 1.3f)];
		size_t identityrawlen = sizeof(identityraw);
		if (Base64_Decode((const byte*)Config::identity, sizeof(Config::identity) - 1, identityraw, &identityrawlen))
		{
			std::cout << "base64 decoding failed\n";
			return false;
		}

		Hmac hmac;
		byte result[SHA_DIGEST_SIZE];

		if (wc_HmacSetKey(&hmac, WC_SHA, identityraw, identityrawlen) || 
			wc_HmacUpdate(&hmac, timeArray, arrlen) || wc_HmacFinal(&hmac, result))
		{
			std::cout << "generation failed\n";
			return false;
		}

		delete[] timeArray;

		if (Base64_Encode_NoNl(result, SHA_DIGEST_SIZE, out, outlen))
		{
			std::cout << "base64 encoding failed\n";
			return false;
		}

		std::cout << "ok\n";
		return true;
	}

	bool GenerateConfirmationQueryParams(CURL* curl, const char* tag, char* out, size_t outsize)
	{
		byte hash[int(SHA_DIGEST_SIZE * 1.5f)];
		size_t hashlen = sizeof(hash);
		if (!GenerateConfirmationHash(tag, hash, &hashlen))
			return false;

		char* escapedhash = curl_easy_escape(curl, (const char*)hash, hashlen);

		char sztime[11];
		_itoa_s(GetSteamTime(), sztime, sizeof(sztime), 10);

		strcpy_s(out, outsize, "p=");
		strcat_s(out, outsize, Config::deviceid);
		strcat_s(out, outsize, "&a=");
		strcat_s(out, outsize, Config::steamid64);
		strcat_s(out, outsize, "&k=");
		strcat_s(out, outsize, escapedhash);
		strcat_s(out, outsize, "&t=");
		strcat_s(out, outsize, sztime);
		strcat_s(out, outsize, "&m=android&tag=");
		strcat_s(out, outsize, tag);

		curl_free(escapedhash);

		return true;
	}

	bool FetchConfirmations(CURL* curl, CURLdata* outpage)
	{
		char postFields[512];
		if (!GenerateConfirmationQueryParams(curl, "conf", postFields, sizeof(postFields)))
			return false;

		Log("Fetching confirmations...");

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)outpage);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/mobileconf/conf");
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		if (strstr(outpage->data, "Oh nooooooes!"))
		{
			std::cout << "fail\n";
			return false;
		}

		if (strstr(outpage->data, "Nothing to confirm"))
		{
			std::cout << "empty\n";
			return true;
		}

		std::cout << "ok\n";
		return true;
	}

	bool AcceptConfirmation(CURL* curl, const char* tradeofferid)
	{
		CURLdata confirmations;
		if (!FetchConfirmations(curl, &confirmations))
			return false;

		char postFields[512];
		if (!GenerateConfirmationQueryParams(curl, "allow", postFields, sizeof(postFields)))
			return false;

		Log("Accepting trade offer confirmation...");

		char offerstring[26] = "data-creator=\"";
		strcat_s(offerstring, sizeof(offerstring), tradeofferid);

		const char* creator = strstr(confirmations.data, offerstring);
		if (!creator)
		{
			std::cout << "no confirmation for offer #" << tradeofferid << '\n';
			return false;
		}
		const char* tag = creator;
		while (tag[0] != '<') --tag;
		const char* confid = strstr(tag, "data-confid=") + sizeof("data-confid=\"") - 1;
		const char* key = strstr(confid, "data-key=") + sizeof("data-key=\"") - 1;

		strcat_s(postFields, sizeof(postFields), "&op=allow&cid=");
		strncat_s(postFields, sizeof(postFields), confid, sizeof("6956216583") - 1);
		strcat_s(postFields, sizeof(postFields), "&ck=");
		strncat_s(postFields, sizeof(postFields), key, strchr(key, '\"') - key);

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/mobileconf/ajaxop");
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

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

		std::cout << "ok\n";
		return true;
	}

	bool AcceptConfirmations(CURL* curl, char(* const tradeofferids)[OFFERID_LEN], size_t offercount)
	{
		CURLdata confirmations;
		if (!FetchConfirmations(curl, &confirmations))
			return false;

		char postFields[512];
		if (!GenerateConfirmationQueryParams(curl, "allow", postFields, sizeof(postFields)))
			return false;

		Log("Accepting trade offer confirmations...");

		strcat_s(postFields, sizeof(postFields), "&op=allow");

		for (size_t i = 0; i < offercount; ++i)
		{
			char offerstring[26] = "data-creator=\"";
			strcat_s(offerstring, sizeof(offerstring), tradeofferids[i]);

			const char* creator = strstr(confirmations.data, offerstring);
			if (!creator)
			{
				std::cout << "no confirmation for offer #" << tradeofferids[i] << '\n';
				return false;
			}
			const char* tag = creator;
			while (tag[0] != '<') --tag;
			const char* confid = strstr(tag, "data-confid=") + sizeof("data-confid=\"") - 1;
			const char* key = strstr(confid, "data-key=") + sizeof("data-key=\"") - 1;

			strcat_s(postFields, sizeof(postFields), "&cid[]=");
			strncat_s(postFields, sizeof(postFields), confid, sizeof("6956216583") - 1);
			strcat_s(postFields, sizeof(postFields), "&ck[]=");
			strncat_s(postFields, sizeof(postFields), key, strchr(key, '\"') - key);
		}

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/mobileconf/multiajaxop");
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

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

		std::cout << "ok\n";
		return true;
	}
}