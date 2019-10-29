#pragma once

#include "Captcha.h"

#define MODULUS_SIZE 256 // raw bytes
#define EXPONENT_SIZE 3 // raw bytes
#define TIMESTAMP_SIZE 13

namespace Login
{
	bool GetRSAPublicKey(CURL* curl, const char* escUsername, char* outHexModulus, char* outHexExponent, char* outTimestamp)
	{
		Log("Receiving RSA public key...");

		char postField[48] = "username=";
		strcat_s(postField, sizeof(postField), escUsername);

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_function);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postField);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login/getrsakey/");

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

		strcpy_s(outHexModulus, MODULUS_SIZE * 2 + 1, doc["publickey_mod"].GetString());
		strcpy_s(outHexExponent, EXPONENT_SIZE * 2 + 1, doc["publickey_exp"].GetString());
		strcpy_s(outTimestamp, TIMESTAMP_SIZE, doc["timestamp"].GetString());

		std::cout << "ok\n";
		return true;
	}

	bool EncryptPassword(const char* password, char* out, word32* outSize, const char* hexModulus, const char* hexExponent)
	{
		Log("Encrypting password...");

		byte modulus[MODULUS_SIZE];
		byte exponent[EXPONENT_SIZE];
		size_t modLen = sizeof(modulus);
		size_t expLen = sizeof(exponent);
		RsaKey pubKey;

		if (Base16_Decode((const byte*)hexModulus, MODULUS_SIZE * 2, modulus, &modLen) ||
			Base16_Decode((const byte*)hexExponent, EXPONENT_SIZE * 2, exponent, &expLen) ||
			wc_InitRsaKey(&pubKey, NULL) ||
			wc_RsaPublicKeyDecodeRaw(modulus, modLen, exponent, expLen, &pubKey))
		{
			std::cout << "public key decoding failed\n";
			return false;
		}

		WC_RNG rng;
		byte rsaPass[MODULUS_SIZE];
		int rsaPassLen;

		if (wc_InitRng(&rng) ||
			(0 > (rsaPassLen = wc_RsaPublicEncrypt((const byte*)password, strlen(password), rsaPass, sizeof(rsaPass), &pubKey, &rng))) ||
			Base64_Encode_NoNl(rsaPass, rsaPassLen, (byte*)out, outSize))
		{
			//char err[WOLFSSL_MAX_ERROR_SZ];
			//wc_ErrorString(ret, err);
			std::cout << "encryption failed\n";
			return false;
		}

		wc_FreeRng(&rng);
		wc_FreeRsaKey(&pubKey);

		std::cout << "ok\n";
		return true;
	}

	bool DoLogin(CURL* curl, const char* newCaptchaGid = nullptr)
	{
		char* escUsername = curl_easy_escape(curl, Config::username, 0);
		char rsaModulus[MODULUS_SIZE * 2 + 1];
		char rsaExponent[EXPONENT_SIZE * 2 + 1];
		char rsaTimestamp[TIMESTAMP_SIZE];

		if (!GetRSAPublicKey(curl, escUsername, rsaModulus, rsaExponent, rsaTimestamp))
			return false;

		char encryptedPass[int(MODULUS_SIZE * 1.5f)];
		size_t encryptedPassLen = sizeof(encryptedPass);
		if (!EncryptPassword(Config::password, encryptedPass, &encryptedPassLen, rsaModulus, rsaExponent))
			return false;

		char captchaGid[CAPTCHA_GID_SIZE];
		char captchaText[8] = "";
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
			std::cout << "cookie request failed\n";
			return false;
		}

		char* escEncryptedPass = curl_easy_escape(curl, encryptedPass, encryptedPassLen);
		char* escCaptchaText = curl_easy_escape(curl, captchaText, 0);

		char postFields[640] = "username=";
		strcat_s(postFields, sizeof(postFields), escUsername);

		strcat_s(postFields, sizeof(postFields), "&password=");
		strcat_s(postFields, sizeof(postFields), escEncryptedPass);

		strcat_s(postFields, sizeof(postFields), "&twofactorcode=");
		strcat_s(postFields, sizeof(postFields), twoFactorCode);

		strcat_s(postFields, sizeof(postFields), "&rsatimestamp=");
		strcat_s(postFields, sizeof(postFields), rsaTimestamp);

		strcat_s(postFields, sizeof(postFields), "&captchagid=");
		strcat_s(postFields, sizeof(postFields), captchaGid);

		strcat_s(postFields, sizeof(postFields), "&captcha_text=");
		strcat_s(postFields, sizeof(postFields), escCaptchaText);

		strcat_s(postFields, sizeof(postFields), "&emailauth=&"
			"loginfriendlyname=&"
			"emailsteamid=&"
			"remember_login=false");

		curl_free(escUsername);
		curl_free(escEncryptedPass);
		curl_free(escCaptchaText);

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_function);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login/dologin/");

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		rapidjson::Document doc;
		doc.Parse(data.data);

		if (!doc["success"].GetBool())
		{
			rapidjson::Value::ConstMemberIterator requiresTwoFactor = doc.FindMember("requires_twofactor");
			if (requiresTwoFactor != doc.MemberEnd() && requiresTwoFactor->value.GetBool())
			{
				std::cout << "wrong two factor code\n";
				return false;
			}

			rapidjson::Value::ConstMemberIterator captchaNeeded = doc.FindMember("captcha_needed");
			if (captchaNeeded != doc.MemberEnd() && captchaNeeded->value.GetBool())
			{
				std::cout << "wrong captcha\n";
				return DoLogin(curl, doc["captcha_gid"].GetString());
			}

			std::cout << "steam answered: " << doc["message"].GetString() << '\n';
			return false;
		}

		std::cout << "ok\n";
		return true;
	}

	bool GetSessionId(CURL* curl)
	{
		Log("Getting session id...");

		curl_slist* cookies;
		if ((curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies) != CURLE_OK) || !cookies)
		{
			std::cout << "fail\n";
			return false;
		}

		curl_slist* sessionId = cookies;
		while (sessionId && !strstr(sessionId->data, "\tsessionid\t"))
			sessionId = sessionId->next;

		if (!sessionId)
		{
			curl_slist_free_all(cookies);
			std::cout << "not found\n";
			return false;
		}
		strcpy_s(g_sessionID, sizeof(g_sessionID), strrchr(sessionId->data, '\t') + 1);

		curl_slist_free_all(cookies);

		std::cout << "ok\n";
		return true;
	}

	bool GetSteamIdApiKey(CURL* curl)
	{
		Log("Getting steamid and api-key...");

		curl_slist* cookies;
		if ((curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies) != CURLE_OK) || !cookies)
		{
			std::cout << "fail\n";
			return false;
		}

		curl_slist* steamId = cookies;
		while (steamId && !strstr(steamId->data, "\tsteamLoginSecure\t"))
			steamId = steamId->next;

		if (!steamId)
		{
			curl_slist_free_all(cookies);
			std::cout << "steamid not found\n";
			return false;
		}
		const char* valuestart = strrchr(steamId->data, '\t') + 1;
		strncpy_s(Config::steamid64, sizeof(Config::steamid64), valuestart, (strchr(valuestart, '%') - valuestart));

		curl_slist_free_all(cookies);

		/*
		CURLdata apipage;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&apipage);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/dev/apikey");
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "apikey request failed\n";
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
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&registerpage);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/dev/registerkey");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

			if (curl_easy_perform(curl) != CURLE_OK)
			{
				std::cout << "register apikey request failed\n";
				return false;
			}

			const char* keystart2 = strstr(registerpage.data, ">Key: ");
			if (!keystart2)
			{
				std::cout << "apikey registration failed\n";
				return false;
			}

			keystart2 += sizeof(">Key: ") - 1;
			strncpy_s(Config::steamapikey, sizeof(Config::steamapikey), keystart2, (strchr(keystart2, '<') - keystart2));
		}
		*/

		std::cout << "ok\n";
		return true;
	}
}