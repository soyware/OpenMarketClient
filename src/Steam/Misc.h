#pragma once

namespace Steam
{
	const size_t sessionIdBufSz = 24 + 1;
	const size_t apiKeyBufSz = 32 + 1;

	// steam login token must be set when calling this
	// out buffer size must be at least apiKeyBufSz
	bool GetApiKey(CURL* curl, const char* sessionId, char* out)
	{
		Log(LogChannel::STEAM, "Checking if the account has an API key...");

		Curl::CResponse respKey;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respKey);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/dev/apikey?l=english");
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		CURLcode respCode = curl_easy_perform(curl);

		if (respCode != CURLE_OK)
		{
			Curl::PrintError(curl, respCode);
			return false;
		}

		// put it here so destructor doesn't get called before we copied
		Curl::CResponse respRegister;

		const char keySubstr[] = ">Key: ";
		const char* foundKeySubstr = strstr(respKey.data, keySubstr);
		if (!foundKeySubstr)
		{
			respKey.Empty(); // not needed anymore

			putsnn("no\n");

			Log(LogChannel::STEAM, "Registering new API key for the account...");

			const char postFieldSession[] = "domain=localhost&agreeToTerms=agreed&sessionid=";

			const size_t postFieldsBufSz = sizeof(postFieldSession) - 1 + sessionIdBufSz - 1 + 1;
			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldSession);
			strcpy(postFieldsEnd, sessionId);

			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respRegister);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/dev/registerkey");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

			respCode = curl_easy_perform(curl);

			if (respCode != CURLE_OK)
			{
				Curl::PrintError(curl, respCode);
				return false;
			}

			foundKeySubstr = strstr(respRegister.data, keySubstr);
			if (!foundKeySubstr)
			{
				putsnn("registration failed\n");
				return false;
			}

			putsnn("ok\n");
		}
		else
			putsnn("yes\n");

		const char* keyStart = foundKeySubstr + sizeof(keySubstr) - 1;
		stpncpy(out, keyStart, (strchr(keyStart, '<') - keyStart))[0] = '\0';

		return true;
	}

	// out buffer size must be at least sessionIdBufSz
	bool GenerateSessionId(char* out)
	{
		byte rawSessionId[(sessionIdBufSz - 1) / 2];

		WC_RNG rng;

		if (wc_InitRng(&rng))
		{
			Log(LogChannel::STEAM, "Session ID generation failed: RNG init failed\n");
			return false;
		}

		if (wc_RNG_GenerateBlock(&rng, rawSessionId, sizeof(rawSessionId)))
		{
			wc_FreeRng(&rng);
			Log(LogChannel::STEAM, "Session ID generation failed: RNG generation failed\n");
			return false;
		}

		wc_FreeRng(&rng);

		word32 sessionIdSz = sessionIdBufSz;

		if (Base16_Encode(rawSessionId, sizeof(rawSessionId), (byte*)out, &sessionIdSz))
		{
			Log(LogChannel::STEAM, "Session ID generation failed: encoding failed\n");
			return false;
		}

		return true;
	}

	bool SetSessionCookie(CURL* curl, const char* sessionId)
	{
		const char cookieSessionStart[] =
			"steamcommunity.com"
			"\tFALSE"
			"\t/"
			"\tTRUE"
			"\t0"
			"\tsessionid"
			"\t";

		const size_t cookieSessionBufSz = sizeof(cookieSessionStart) - 1 + sessionIdBufSz - 1 + 1;
		char cookieSession[cookieSessionBufSz];

		char* cookieSessionEnd = cookieSession;
		cookieSessionEnd = stpcpy(cookieSessionEnd, cookieSessionStart);
		strcpy(cookieSessionEnd, sessionId);

		if (curl_easy_setopt(curl, CURLOPT_COOKIELIST, cookieSession) != CURLE_OK)
		{
			Log(LogChannel::STEAM, "Setting session ID cookie failed\n");
			return false;
		}

		return true;
	}

	bool SetLoginCookie(CURL* curl, const char* steamId64, const char* loginToken)
	{
		const char cookieLoginStart[] =
			"#HttpOnly_steamcommunity.com"
			"\tFALSE"
			"\t/"
			"\tTRUE"
			"\t0"
			"\tsteamLoginSecure"
			"\t";

		const size_t cookieLoginBufSz = sizeof(cookieLoginStart) - 1 + Auth::jwtBufSz - 1 + 1;
		char cookieLogin[cookieLoginBufSz];

		char* cookieLoginEnd = cookieLogin;
		cookieLoginEnd = stpcpy(cookieLoginEnd, cookieLoginStart);
		cookieLoginEnd = stpcpy(cookieLoginEnd, steamId64);
		cookieLoginEnd = stpcpy(cookieLoginEnd, "%7C%7C");
		strcpy(cookieLoginEnd, loginToken);

		if (curl_easy_setopt(curl, CURLOPT_COOKIELIST, cookieLogin) != CURLE_OK)
		{
			Log(LogChannel::STEAM, "Setting login cookie failed\n");
			return false;
		}

		return true;
	}

	bool SetJWTCookies(CURL* curl, const char* steamId64, const char* refreshToken, const char* accessToken)
	{
		Log(LogChannel::STEAM, "Setting refresh cookie...");

		const char cookieRefreshStart[] =
			"#HttpOnly_login.steampowered.com"	/* Hostname */
			"\tFALSE"							/* Include subdomains */
			"\t/"								/* Path */
			"\tTRUE"							/* Secure */
			"\t0"								/* Expiry in epoch time format. 0 == Session */
			"\tsteamRefresh_steam"				/* Name */
			"\t";								/* Value */

		const size_t cookieRefreshBufSz = sizeof(cookieRefreshStart) - 1 + Auth::jwtBufSz - 1 + 1;
		char cookieRefresh[cookieRefreshBufSz];

		char* cookieRefreshEnd = cookieRefresh;
		cookieRefreshEnd = stpcpy(cookieRefreshEnd, cookieRefreshStart);
		cookieRefreshEnd = stpcpy(cookieRefreshEnd, steamId64);
		cookieRefreshEnd = stpcpy(cookieRefreshEnd, "%7C%7C");
		strcpy(cookieRefreshEnd, refreshToken);

		if (curl_easy_setopt(curl, CURLOPT_COOKIELIST, cookieRefresh) != CURLE_OK)
		{
			putsnn("fail\n");
			return false;
		}

		putsnn("ok\n");

		return SetLoginCookie(curl, steamId64, accessToken);
	}

	bool SetInventoryPublic(CURL* curl, const char* sessionId, const char* steamId64)
	{
		Log(LogChannel::STEAM, "Setting inventory visibility to public...");

		// get current privacy settings

		const char urlStart[] = "https://steamcommunity.com/profiles/";
		const char urlPath[] = "/ajaxsetprivacy/";

		const size_t urlBufSz =
			sizeof(urlStart) - 1 + UINT64_MAX_STR_SIZE - 1 +
			sizeof(urlPath) - 1 + 1;

		char url[urlBufSz];

		char* urlEnd = url;
		urlEnd = stpcpy(urlEnd, urlStart);
		urlEnd = stpcpy(urlEnd, steamId64);
		strcpy(urlEnd, urlPath);

		const char postFieldSession[] = "sessionid=";
		const char postFieldPrivacy[] = "&Privacy=";
		const char postFieldCommentPerm[] = "&eCommentPermission=";

		const size_t postFieldsBufSz =
			sizeof(postFieldSession) - 1 + sessionIdBufSz - 1 +
			sizeof(postFieldPrivacy) - 1 + 132 +
			sizeof(postFieldCommentPerm) - 1 + 1 + 1;

		char postFields[postFieldsBufSz];

		char* postFieldsEnd = postFields;
		postFieldsEnd = stpcpy(postFieldsEnd, postFieldSession);
		postFieldsEnd = stpcpy(postFieldsEnd, sessionId);
		postFieldsEnd = stpcpy(postFieldsEnd, postFieldPrivacy);
		postFieldsEnd = stpcpy(postFieldsEnd, "{\"PrivacyProfile\":3}");
		strcpy(postFieldsEnd, postFieldCommentPerm); // empty comm perm to make steam return our current settings

		Curl::CResponse response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

		CURLcode respCode = curl_easy_perform(curl);

		if (respCode != CURLE_OK)
		{
			Curl::PrintError(curl, respCode);
			return false;
		}

		if (!strcmp(response.data, "null"))
		{
			putsnn("get request unsucceeded\n");
			return false;
		}

		rapidjson::Document parsed;
		parsed.ParseInsitu(response.data);

		if (parsed.HasParseError())
		{
			putsnn("get JSON parsing failed\n");
			return false;
		}

		if (!parsed["success"].GetInt())
		{
			putsnn("get request unsucceeded\n");
			return false;
		}

		rapidjson::Value& privacy = parsed["Privacy"];
		rapidjson::Value& privacySettings = privacy["PrivacySettings"];

		rapidjson::Value& privacyProfile = privacySettings["PrivacyProfile"];
		rapidjson::Value& privacyInventory = privacySettings["PrivacyInventory"];
		rapidjson::Value& privacyInventoryGifts = privacySettings["PrivacyInventoryGifts"];

		const int privacyProfileVal = privacyProfile.GetInt();

		// 3 public
		// 2 friends only
		// 1 private

		if ((privacyProfileVal == 3) &&
			(privacyInventory.GetInt() == 3) &&
			(privacyInventoryGifts.GetInt() == 3))
		{
			putsnn("already public\n");
			return true;
		}

		for (auto itr = privacySettings.MemberBegin(); itr != privacySettings.MemberEnd(); ++itr)
		{
			// if a setting privacy is higher than profile privacy
			// e.g. friends set public, yet profile private
			// make friends private after we set profile public
			if (privacyProfileVal < itr->value.GetInt())
				itr->value.SetInt(privacyProfileVal);
		}

		privacyProfile.SetInt(3);
		privacyInventory.SetInt(3);
		privacyInventoryGifts.SetInt(3);

		rapidjson::StringBuffer privacySettingsStrBuf;
		rapidjson::Writer<rapidjson::StringBuffer> privacySettingsWriter(privacySettingsStrBuf);

		if (!privacySettings.Accept(privacySettingsWriter))
		{
			putsnn("converting privacy settings JSON to string failed\n");
			return false;
		}

		// convert comment to other settings values
		// 1 public
		// 0 friends only
		// 2 private
		int newCommentPerm = ((privacy["eCommentPermission"].GetInt() + 1) % 3) + 1;

		response.Empty(); // not needed anymore

		// same reasoning as above
		if (privacyProfileVal < newCommentPerm)
			newCommentPerm = privacyProfileVal;

		newCommentPerm = (newCommentPerm + 1) % 3; // convert back

		postFieldsEnd = postFields;
		postFieldsEnd = stpcpy(postFieldsEnd, postFieldSession);
		postFieldsEnd = stpcpy(postFieldsEnd, sessionId);
		postFieldsEnd = stpcpy(postFieldsEnd, postFieldPrivacy);
		postFieldsEnd = stpcpy(postFieldsEnd, privacySettingsStrBuf.GetString());
		postFieldsEnd = stpcpy(postFieldsEnd, postFieldCommentPerm);
		strcpy(postFieldsEnd, std::to_string(newCommentPerm).c_str());

		Curl::CResponse respSet;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respSet);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

		respCode = curl_easy_perform(curl);

		if (respCode != CURLE_OK)
		{
			Curl::PrintError(curl, respCode);
			return false;
		}

		if (!strcmp(respSet.data, "null"))
		{
			putsnn("set request unsucceeded\n");
			return false;
		}

		rapidjson::Document parsedSet;
		parsedSet.ParseInsitu(respSet.data);

		if (parsedSet.HasParseError())
		{
			putsnn("set JSON parsing failed\n");
			return false;
		}

		if (parsedSet["success"].GetInt() != 1)
		{
			putsnn("set request unsucceeded\n");
			return false;
		}

		putsnn("ok\n");
		return true;
	}
}