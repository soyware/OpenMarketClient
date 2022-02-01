#pragma once

#include "Captcha.h"

#define MODULUS_LEN 256
#define EXPONENT_LEN 3
#define TIMESTAMP_LEN 13

namespace Login
{
	bool GetRSAPublicKey(CURL* curl, const char* escUsername, byte* outHexModulus, byte* outHexExponent, char* outTimestamp)
	{
		Log("Getting RSA public key...");

		const size_t postFieldSz = sizeof("username=") + strlen(escUsername);
		char* postField = new char[postFieldSz];

		strcpy_s(postField, postFieldSz, "username=");
		strcat_s(postField, postFieldSz, escUsername);

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curl_write_function);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postField);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login/getrsakey/");

		CURLcode res = curl_easy_perform(curl);

		delete[] postField;

		if (res != CURLE_OK)
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

		memcpy(outHexModulus, parsed["publickey_mod"].GetString(), MODULUS_LEN * 2);
		memcpy(outHexExponent, parsed["publickey_exp"].GetString(), EXPONENT_LEN * 2);
		strcpy_s(outTimestamp, TIMESTAMP_LEN + 1, parsed["timestamp"].GetString());

		printf("ok\n");
		return true;
	}

	int EncryptPassword(const char* password, byte* out, word32* outSize, const byte* hexModulus, const byte* hexExponent)
	{
		Log("Encrypting password...");

		byte modulus[MODULUS_LEN];
		byte exponent[EXPONENT_LEN];
		word32 modSz = sizeof(modulus);
		word32 expSz = sizeof(exponent);

		if (Base16_Decode(hexModulus, MODULUS_LEN * 2, modulus, &modSz) ||
			Base16_Decode(hexExponent, EXPONENT_LEN * 2, exponent, &expSz))
		{
			printf("base16 decoding failed\n");
			return false;
		}

		RsaKey pubKey;
		if (wc_InitRsaKey(&pubKey, NULL))
		{
			printf("rsa init failed\n");
			return false;
		}

		if (wc_RsaPublicKeyDecodeRaw(modulus, modSz, exponent, expSz, &pubKey))
		{
			wc_FreeRsaKey(&pubKey);
			printf("public key decoding failed\n");
			return false;
		}

		WC_RNG rng;
		if (wc_InitRng(&rng))
		{
			wc_FreeRsaKey(&pubKey);
			printf("rng init failed\n");
			return false;
		}

		byte rsaPass[MODULUS_LEN];
		int rsaPassSz = wc_RsaPublicEncrypt((const byte*)password, strlen(password), rsaPass, sizeof(rsaPass), &pubKey, &rng);

		bool encryptionFailed = ((rsaPassSz < 0) || Base64_Encode_NoNl(rsaPass, rsaPassSz, out, outSize));

		wc_FreeRng(&rng);
		wc_FreeRsaKey(&pubKey);

		if (encryptionFailed)
		{
			printf("fail\n");
			return false;
		}

		printf("ok\n");
		return true;
	}

	bool DoLogin(CURL* curl, const char* newCaptchaGid = nullptr)
	{
		char* escUsername = curl_easy_escape(curl, Config::username, 0);
		byte rsaModulus[MODULUS_LEN * 2];
		byte rsaExponent[EXPONENT_LEN * 2];
		char rsaTimestamp[TIMESTAMP_LEN + 1];

		if (!GetRSAPublicKey(curl, escUsername, rsaModulus, rsaExponent, rsaTimestamp))
			return false;

		byte encryptedPass[PlainToBase64Size(MODULUS_LEN, WC_NO_NL_ENC)];
		word32 encryptedPassLen = sizeof(encryptedPass);

		if (!EncryptPassword(Config::password, encryptedPass, &encryptedPassLen, rsaModulus, rsaExponent))
			return false;

		char captchaGid[CAPTCHA_GID_SIZE];
		char captchaText[CAPTCHA_ANSWER_SIZE] = "";
		if (!newCaptchaGid)
		{
			if (!Captcha::Refresh(curl, captchaGid))
				return false;
		}
		else
			strcpy_s(captchaGid, sizeof(captchaGid), newCaptchaGid);

		if (strcmp(captchaGid, "-1"))
		{
			if (!Captcha::Handle(curl, captchaGid, captchaText))
				return false;
		}

		char twoFactorCode[TWOFA_SIZE];
		if (!Guard::GenerateCode(twoFactorCode))
			return false;

		Log("Logging in steam...");

		// getting sessionid cookie with HEAD request
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login");
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			printf("cookie request failed\n");
			return false;
		}

		char* escEncryptedPass = curl_easy_escape(curl, (const char*)encryptedPass, encryptedPassLen);
		char* escCaptchaText = curl_easy_escape(curl, captchaText, 0);

		std::string postFields("emailauth="
			"&loginfriendlyname="
			"&emailsteamid="
			"&remember_login=true"
			"&username=");
		postFields.append(escUsername);

		postFields.append("&password=");
		postFields.append(escEncryptedPass);

		postFields.append("&twofactorcode=");
		postFields.append(twoFactorCode);

		postFields.append("&rsatimestamp=");
		postFields.append(rsaTimestamp);

		postFields.append("&captchagid=");
		postFields.append(captchaGid);

		postFields.append("&captcha_text=");
		postFields.append(escCaptchaText);

		curl_free(escUsername);
		curl_free(escEncryptedPass);
		curl_free(escCaptchaText);

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curl_write_function);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login/dologin/");

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			printf("request failed\n");
			return false;
		}

		rapidjson::Document parsed;
		parsed.Parse(response.data);

		if (!parsed["success"].GetBool())
		{
			rapidjson::Value::ConstMemberIterator requiresTwoFactor = parsed.FindMember("requires_twofactor");
			if (requiresTwoFactor != parsed.MemberEnd() && requiresTwoFactor->value.GetBool())
			{
				printf("wrong two factor code\n");
				return false;
			}

			rapidjson::Value::ConstMemberIterator captchaNeeded = parsed.FindMember("captcha_needed");
			if (captchaNeeded != parsed.MemberEnd() && captchaNeeded->value.GetBool())
			{
				printf("wrong captcha\n");
				return DoLogin(curl, parsed["captcha_gid"].GetString());
			}

			printf("recieved response: %s\n", parsed["message"].GetString());
			return false;
		}

		printf("ok\n");
		return true;
	}

	bool GetSteamInfo(CURL* curl)
	{
		Log("Getting steam info...");

		curl_slist* cookies;
		if ((curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies) != CURLE_OK) || !cookies)
		{
			printf("fail\n");
			return false;
		}

		curl_slist* sessionId = cookies;
		while (sessionId && !strstr(sessionId->data, "\tsessionid\t"))
			sessionId = sessionId->next;

		if (!sessionId)
		{
			curl_slist_free_all(cookies);
			printf("sessionid not found\n");
			return false;
		}
		strcpy_s(Config::sessionID, sizeof(Config::sessionID), strrchr(sessionId->data, '\t') + 1);

		if (!Config::steamID64[0])
		{
			curl_slist* steamId = cookies;
			while (steamId && !strstr(steamId->data, "\tsteamLoginSecure\t"))
				steamId = steamId->next;

			if (!steamId)
			{
				curl_slist_free_all(cookies);
				printf("steamid not found\n");
				return false;
			}
			const char* valuestart = strrchr(steamId->data, '\t') + 1;
			strncpy_s(Config::steamID64, sizeof(Config::steamID64), valuestart, (strchr(valuestart, '%') - valuestart));
		}

		curl_slist_free_all(cookies);

		printf("ok\n");
		return true;
	}

	/*
	bool GetSteamApiKey(CURL* curl)
	{
		Log("Getting steam api key...");

		CURLdata apipage;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, apipage);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/dev/apikey");
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			printf("request failed\n");
			return false;
		}

		const char* keystart = strstr(apipage.data, ">Key: ");
		if (keystart)
		{
			keystart += sizeof(">Key: ") - 1;
			strncpy_s(Config::steamapikey, sizeof(Config::steamapikey), keystart, (strchr(keystart, '<') - keystart));
		}
		else
		{
			char postFields[48 + 25 - 1] = "domain=localhost&agreeToTerms=agreed&sessionid=";
			strcat_s(postFields, sizeof(postFields), g_sessionID);

			CURLdata registerpage;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, registerpage);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/dev/registerkey");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

			if (curl_easy_perform(curl) != CURLE_OK)
			{
				printf("register request failed\n");
				return false;
			}

			const char* keystart2 = strstr(registerpage.data, ">Key: ");
			if (!keystart2)
			{
				printf("fail\n");
				return false;
			}

			keystart2 += sizeof(">Key: ") - 1;
			strncpy_s(Config::steamapikey, sizeof(Config::steamapikey), keystart2, (strchr(keystart2, '<') - keystart2));
		}

		printf("ok\n");
		return true;
	}
	*/
}