#pragma once

namespace Steam
{
	namespace Auth
	{
		const size_t usernameBufSz = 63 + 1;
		const size_t passwordBufSz = 63 + 1;

		const size_t modulusSz = 256; // 2048 bit RSA
		const size_t exponentSz = 3;
		const size_t timestampBufSz = UINT64_MAX_STR_SIZE;

		const size_t oauthTokenBufSz = 32 + 1;
		const size_t loginTokenBufSz = 40 + 1;

		const size_t clientIdBufSz = UINT64_MAX_STR_SIZE;
		const size_t requestIdBufSz = 24 + 1;

		enum class LoginResult
		{
			SUCCESS,
			GET_PASS_ENCRYPT_KEY_FAILED,
			PASS_ENCRYPT_FAILED,
			CAPTCHA_FAILED,
			REQUEST_FAILED,
			WRONG_CAPTCHA,
			WRONG_TWO_FACTOR,
			UNSUCCEDED,
			OAUTH_FAILED,
		};

		// unused outdated oauth login start

		// outHexModulus buffer size must be at least modulusSz * 2
		// outHexExponent buffer size must be at least exponentSz * 2
		// outTimestamp buffer size must be at least timestampBufSz
		bool GetPasswordRSAPublicKey(CURL* curl, const char* escUsername, byte* outHexModulus, byte* outHexExponent, char* outTimestamp)
		{
			Log(LogChannel::STEAM, "Getting password RSA public key...");

			const char postFieldUsername[] = "username=";

			// multiple by 3 due to URL encoding
			const size_t postFieldsBufSz = sizeof(postFieldUsername) - 1 + (usernameBufSz - 1) * 3 + 1;
			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldUsername);
			strcpy(postFieldsEnd, escUsername);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login/getrsakey/");
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

			if (!parsed["success"].GetBool())
			{
				putsnn("request unsucceeded\n");
				return false;
			}

			memcpy(outHexModulus, parsed["publickey_mod"].GetString(), modulusSz * 2);
			memcpy(outHexExponent, parsed["publickey_exp"].GetString(), exponentSz * 2);
			strcpy(outTimestamp, parsed["timestamp"].GetString());

			putsnn("ok\n");
			return true;
		}

		// out buffer size must be at least PlainToBase64Size(sizeof(modulus), WC_NO_NL_ENC)
		bool EncryptPassword(const char* password, const byte* modulus, const byte* exponent, byte* out, word32* outSz)
		{
			Log(LogChannel::STEAM, "Encrypting password...");

			RsaKey pubKey;
			if (wc_InitRsaKey(&pubKey, nullptr))
			{
				putsnn("RSA init failed\n");
				return false;
			}

			if (wc_RsaPublicKeyDecodeRaw(modulus, modulusSz, exponent, exponentSz, &pubKey))
			{
				wc_FreeRsaKey(&pubKey);
				putsnn("RSA public key decoding failed\n");
				return false;
			}

			WC_RNG rng;

			if (wc_InitRng(&rng))
			{
				wc_FreeRsaKey(&pubKey);
				putsnn("RNG init failed\n");
				return false;
			}

			byte encrypted[modulusSz];

			const int encryptedSz = wc_RsaPublicEncrypt((byte*)password, strlen(password), encrypted, sizeof(encrypted), &pubKey, &rng);

			const bool success = ((0 <= encryptedSz) && !Base64_Encode_NoNl(encrypted, encryptedSz, out, outSz));

			wc_FreeRsaKey(&pubKey);
			wc_FreeRng(&rng);

			putsnn(success ? "ok\n" : "fail\n");
			return success;
		}

		// outSteamId64 buffer size must be at least UINT64_MAX_STR_SIZE
		// outOAuthToken buffer size must be at least oauthTokenBufSz
		// outLoginToken buffer size must be at least loginTokenBufSz
		LoginResult DoLogin(CURL* curl, const char* username, const char* password, const char* twoFactorCode, char* outSteamId64, char* outOAuthToken, char* outLoginToken)
		{
			char* escUsername = curl_easy_escape(curl, username, 0);

			byte rsaHexModulus[modulusSz * 2];
			byte rsaHexExponent[exponentSz * 2];
			char rsaTimestamp[timestampBufSz];

			if (!GetPasswordRSAPublicKey(curl, escUsername, rsaHexModulus, rsaHexExponent, rsaTimestamp))
			{
				curl_free(escUsername);
				return LoginResult::GET_PASS_ENCRYPT_KEY_FAILED;
			}

			Log(LogChannel::STEAM, "Decoding password RSA public key...");

			byte rsaModulus[modulusSz];
			word32 rsaModSz = sizeof(rsaModulus);

			byte rsaExponent[exponentSz];
			word32 rsaExpSz = sizeof(rsaExponent);

			if (Base16_Decode(rsaHexModulus, sizeof(rsaHexModulus), rsaModulus, &rsaModSz) ||
				Base16_Decode(rsaHexExponent, sizeof(rsaHexExponent), rsaExponent, &rsaExpSz))
			{
				curl_free(escUsername);
				putsnn("fail\n");
				return LoginResult::GET_PASS_ENCRYPT_KEY_FAILED;
			}

			putsnn("ok\n");

			constexpr size_t encryptedPassBufSz = PlainToBase64Size(sizeof(rsaModulus), WC_NO_NL_ENC);
			byte encryptedPass[encryptedPassBufSz];
			word32 encryptedPassSz = sizeof(encryptedPass);

			if (!EncryptPassword(password, rsaModulus, rsaExponent, encryptedPass, &encryptedPassSz))
			{
				curl_free(escUsername);
				return LoginResult::PASS_ENCRYPT_FAILED;
			}

			char captchaAnswer[Captcha::answerBufSz] = "";
			char captchaGid[Captcha::gidBufSz];

			if (!Captcha::GetGID(curl, captchaGid) ||
				(strcmp(captchaGid, "-1") && !Captcha::GetAnswer(curl, captchaGid, captchaAnswer)))
			{
				curl_free(escUsername);
				return LoginResult::CAPTCHA_FAILED;
			}

			Log(LogChannel::STEAM, "Logging in...");

			char* escEncryptedPass = curl_easy_escape(curl, (char*)encryptedPass, encryptedPassSz);
			char* escCaptchaAnswer = curl_easy_escape(curl, captchaAnswer, 0);

			const char postFieldUsername[] =
				"oauth_client_id=DE45CD61"
				"&oauth_scope=read_profile%20write_profile%20read_client%20write_client"
				"&remember_login=true"
				"&username=";

			const char postFieldPassword[] = "&password=";
			const char postFieldRsaTimestamp[] = "&rsatimestamp=";
			const char postField2FACode[] = "&twofactorcode=";
			const char postFieldCaptchaGid[] = "&captchagid=";
			const char postFieldCaptchaAnswer[] = "&captcha_text=";

			const size_t postFieldsBufSz =
				sizeof(postFieldUsername) - 1 + (usernameBufSz - 1) * 3 + // multiple by 3 due to URL encoding
				sizeof(postFieldPassword) - 1 + encryptedPassBufSz * 3 +
				sizeof(postFieldRsaTimestamp) - 1 + timestampBufSz - 1 +
				sizeof(postField2FACode) - 1 + Guard::twoFactorCodeBufSz - 1 +
				sizeof(postFieldCaptchaGid) - 1 + Captcha::gidBufSz - 1 +
				sizeof(postFieldCaptchaAnswer) - 1 + (Captcha::answerBufSz - 1) * 3 + 1;

			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldUsername);
			postFieldsEnd = stpcpy(postFieldsEnd, escUsername);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldPassword);
			postFieldsEnd = stpcpy(postFieldsEnd, escEncryptedPass);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldRsaTimestamp);
			postFieldsEnd = stpcpy(postFieldsEnd, rsaTimestamp);
			postFieldsEnd = stpcpy(postFieldsEnd, postField2FACode);
			postFieldsEnd = stpcpy(postFieldsEnd, twoFactorCode);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldCaptchaGid);
			postFieldsEnd = stpcpy(postFieldsEnd, captchaGid);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldCaptchaAnswer);
			strcpy(postFieldsEnd, escCaptchaAnswer);

			curl_free(escUsername);
			curl_free(escEncryptedPass);
			curl_free(escCaptchaAnswer);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login/dologin/");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);
			curl_easy_setopt(curl, CURLOPT_COOKIE, "mobileClient=android");

			const CURLcode respCode = curl_easy_perform(curl);

			curl_easy_setopt(curl, CURLOPT_COOKIE, NULL);

			if (respCode != CURLE_OK)
			{
				Curl::PrintError(curl, respCode);
				return LoginResult::REQUEST_FAILED;
			}

			rapidjson::Document parsed;
			parsed.ParseInsitu(response.data);

			if (parsed.HasParseError())
			{
				putsnn("JSON parsing failed\n");
				return LoginResult::REQUEST_FAILED;
			}

			if (!parsed["success"].GetBool())
			{
				const auto iterRequires2FA = parsed.FindMember("requires_twofactor");
				if (iterRequires2FA != parsed.MemberEnd() && iterRequires2FA->value.GetBool())
				{
					putsnn("wrong two factor code\n");
					return LoginResult::WRONG_TWO_FACTOR;
				}

				const auto iterCaptchaNeeded = parsed.FindMember("captcha_needed");
				if (iterCaptchaNeeded != parsed.MemberEnd() && iterCaptchaNeeded->value.GetBool())
				{
					putsnn("wrong captcha answer\n");
					return LoginResult::WRONG_CAPTCHA;
				}

				const auto iterMessage = parsed.FindMember("message");
				if (iterMessage != parsed.MemberEnd())
				{
					const char* msg = iterMessage->value.GetString();
					if (msg[0])
					{
						puts(msg); // we need newline
						return LoginResult::UNSUCCEDED;
					}
				}

				putsnn("request unsucceeded\n");
				return LoginResult::UNSUCCEDED;
			}

			const auto iterOAuth = parsed.FindMember("oauth");
			if (iterOAuth == parsed.MemberEnd())
			{
				putsnn("OAuth not found\n");
				return LoginResult::OAUTH_FAILED;
			}

			rapidjson::Document parsedOAuth;
			parsedOAuth.Parse(iterOAuth->value.GetString());

			if (parsedOAuth.HasParseError())
			{
				putsnn("JSON parsing failed\n");
				return LoginResult::OAUTH_FAILED;
			}

			const char* steamId64 = parsedOAuth["steamid"].GetString();
			const char* oauthToken = parsedOAuth["oauth_token"].GetString();
			const char* loginToken = parsedOAuth["wgtoken_secure"].GetString();

			strcpy(outSteamId64, steamId64);
			strcpy(outOAuthToken, oauthToken);
			strcpy(outLoginToken, loginToken);

			putsnn("ok\n");
			return LoginResult::SUCCESS;
		}

		// outLoginToken buffer size must be at least loginTokenBufSz
		int RefreshOAuthSession(CURL* curl, const char* oauthToken, char* outLoginToken)
		{
			const char postFieldAccessToken[] = "access_token=";

			const size_t postFieldsBufSz = sizeof(postFieldAccessToken) - 1 + oauthTokenBufSz - 1 + 1;
			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldAccessToken);
			strcpy(postFieldsEnd, oauthToken);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/IMobileAuthService/GetWGToken/v1/");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

			const CURLcode respCode = curl_easy_perform(curl);

			if (respCode != CURLE_OK)
			{
				if (respCode == CURLE_HTTP_RETURNED_ERROR)
				{
					long httpCode;
					curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
					if (httpCode == 401) // unauthorized
					{
						Log(LogChannel::STEAM, "Refreshing OAuth session failed: OAuth token is invalid or has expired\n");
						return 0;
					}
				}

				Log(LogChannel::STEAM, "Refreshing OAuth session failed: ");
				Curl::PrintError(curl, respCode);
				return -1;
			}

			rapidjson::Document parsed;
			parsed.ParseInsitu(response.data);

			if (parsed.HasParseError())
			{
				Log(LogChannel::STEAM, "Refreshing OAuth session failed: JSON parsing failed\n");
				return -1;
			}

			const auto iterResponse = parsed.FindMember("response");
			if (iterResponse == parsed.MemberEnd() || iterResponse->value.ObjectEmpty())
			{
				Log(LogChannel::STEAM, "Refreshing OAuth session failed: request unsucceeded\n");
				return -1;
			}

			const char* loginToken = iterResponse->value["token_secure"].GetString();

			strcpy(outLoginToken, loginToken);

			return 1;
		}



		// outHexModulus buffer size must be at least modulusSz * 2
		// outHexExponent buffer size must be at least exponentSz * 2
		// outTimestamp buffer size must be at least timestampBufSz
		bool GetPasswordRSAPublicKeyJWT(CURL* curl, const char* escUsername, byte* outHexModulus, byte* outHexExponent, char* outTimestamp)
		{
			Log(LogChannel::STEAM, "Getting password RSA public key...");

			const char urlStart[] = "https://api.steampowered.com/IAuthenticationService/GetPasswordRSAPublicKey/v1/?account_name=";

			// multiple by 3 due to URL encoding
			const size_t urlBufSz = sizeof(urlStart) - 1 + (usernameBufSz - 1) * 3 + 1;
			char url[urlBufSz];

			char* urlEnd = url;
			urlEnd = stpcpy(urlEnd, urlStart);
			strcpy(urlEnd, escUsername);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

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

			const auto iterResponse = parsed.FindMember("response");
			if (iterResponse == parsed.MemberEnd() || iterResponse->value.ObjectEmpty())
			{
				putsnn("request unsucceeded\n");
				return false;
			}

			const char* mod = iterResponse->value["publickey_mod"].GetString();
			const char* exp = iterResponse->value["publickey_exp"].GetString();
			const char* timestamp = iterResponse->value["timestamp"].GetString();

			memcpy(outHexModulus, mod, modulusSz * 2);
			memcpy(outHexExponent, exp, exponentSz * 2);
			strcpy(outTimestamp, timestamp);

			putsnn("ok\n");
			return true;
		}

		// outSteamId64 buffer size must be at least UINT64_MAX_STR_SIZE
		// outClientId buffer size must be at least clientIdBufSz
		// outRequestId buffer size must be at least requestIdBufSz
		bool BeginAuthSessionViaCredentials(CURL* curl, const char* username, const char* password, char* outSteamId64, char* outClientId, char* outRequestId)
		{
			byte rsaHexModulus[modulusSz * 2];
			byte rsaHexExponent[exponentSz * 2];
			char rsaTimestamp[timestampBufSz];

			char* escUsername = curl_easy_escape(curl, username, 0);

			if (!GetPasswordRSAPublicKeyJWT(curl, escUsername, rsaHexModulus, rsaHexExponent, rsaTimestamp))
			{
				curl_free(escUsername);
				return false;
			}

			Log(LogChannel::STEAM, "Decoding password RSA public key...");

			byte rsaModulus[modulusSz];
			word32 rsaModSz = sizeof(rsaModulus);

			byte rsaExponent[exponentSz];
			word32 rsaExpSz = sizeof(rsaExponent);

			if (Base16_Decode(rsaHexModulus, sizeof(rsaHexModulus), rsaModulus, &rsaModSz) ||
				Base16_Decode(rsaHexExponent, sizeof(rsaHexExponent), rsaExponent, &rsaExpSz))
			{
				curl_free(escUsername);
				putsnn("fail\n");
				return false;
			}

			putsnn("ok\n");

			constexpr size_t encryptedPassBufSz = PlainToBase64Size(sizeof(rsaModulus), WC_NO_NL_ENC);
			byte encryptedPass[encryptedPassBufSz];
			word32 encryptedPassSz = sizeof(encryptedPass);

			if (!EncryptPassword(password, rsaModulus, rsaExponent, encryptedPass, &encryptedPassSz))
			{
				curl_free(escUsername);
				return false;
			}

			Log(LogChannel::STEAM, "Beginning auth session...");

			char* escEncryptedPass = curl_easy_escape(curl, (char*)encryptedPass, encryptedPassSz);

			const char postFieldAccountName[] = "persistence=1&account_name=";
			const char postFieldEncryptedPass[] = "&encrypted_password=";
			const char postFieldEncryptionTime[] = "&encryption_timestamp=";

			const size_t postFieldsBufSz =
				sizeof(postFieldAccountName) - 1 + (usernameBufSz - 1) * 3 + // multiple by 3 due to URL encoding
				sizeof(postFieldEncryptedPass) - 1 + encryptedPassBufSz * 3 +
				sizeof(postFieldEncryptionTime) - 1 + Guard::twoFactorCodeBufSz - 1 + 1;

			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldAccountName);
			postFieldsEnd = stpcpy(postFieldsEnd, escUsername);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldEncryptedPass);
			postFieldsEnd = stpcpy(postFieldsEnd, escEncryptedPass);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldEncryptionTime);
			strcpy(postFieldsEnd, rsaTimestamp);

			curl_free(escUsername);
			curl_free(escEncryptedPass);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/IAuthenticationService/BeginAuthSessionViaCredentials/v1/");
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

			const auto iterResponse = parsed.FindMember("response");
			if (iterResponse == parsed.MemberEnd() || iterResponse->value.ObjectEmpty())
			{
				putsnn("request unsucceeded\n");
				return false;
			}

			const auto iterSteamId = iterResponse->value.FindMember("steamid");
			if (iterSteamId == iterResponse->value.MemberEnd())
			{
				putsnn("wrong credentials\n");
				return false;
			}

			const char* clientId = iterResponse->value["client_id"].GetString();
			const char* requestId = iterResponse->value["request_id"].GetString();
			const char* steamId64 = iterSteamId->value.GetString();

			strcpy(outClientId, clientId);
			strcpy(outRequestId, requestId);
			strcpy(outSteamId64, steamId64);

			putsnn("ok\n");
			return true;
		}

		bool UpdateAuthSessionWithSteamGuardCode(CURL* curl, const char* steamId64, const char* clientId, const char* twoFactorCode)
		{
			Log(LogChannel::STEAM, "Updating auth session with a Steam Guard code...");

			const char postFieldClientId[] = "client_id=";
			const char postFieldSteamId[] = "&steamid=";
			const char postFieldCode[] = "&code_type=3&code="; // code_type 3 is k_EAuthSessionGuardType_DeviceCode

			const size_t postFieldsBufSz =
				sizeof(postFieldClientId) - 1 + clientIdBufSz - 1 + // multiple by 3 due to URL encoding
				sizeof(postFieldSteamId) - 1 + UINT64_MAX_STR_SIZE - 1 +
				sizeof(postFieldCode) - 1 + Guard::twoFactorCodeBufSz - 1 + 1;

			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldClientId);
			postFieldsEnd = stpcpy(postFieldsEnd, clientId);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldSteamId);
			postFieldsEnd = stpcpy(postFieldsEnd, steamId64);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldCode);
			strcpy(postFieldsEnd, twoFactorCode);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/IAuthenticationService/UpdateAuthSessionWithSteamGuardCode/v1/");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

			const CURLcode respCode = curl_easy_perform(curl);

			if (respCode != CURLE_OK)
			{
				Curl::PrintError(curl, respCode);
				return false;
			}

			putsnn("ok\n");
			return true;
		}

		// outRefreshToken and outAccessToken buffer size must be at least jwtBufSz
		bool PollAuthSessionStatus(CURL* curl, const char* clientId, const char* requestId, char* outRefreshToken, char* outAccessToken)
		{
			Log(LogChannel::STEAM, "Polling auth session status...");

			char* escRequestId = curl_easy_escape(curl, requestId, 0);

			const char postFieldClientId[] = "client_id=";
			const char postFieldRequestId[] = "&request_id=";

			const size_t postFieldsBufSz =
				sizeof(postFieldClientId) - 1 + clientIdBufSz - 1 + // multiple by 3 due to URL encoding
				sizeof(postFieldRequestId) - 1 + (requestIdBufSz - 1) * 3 + 1;

			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldClientId);
			postFieldsEnd = stpcpy(postFieldsEnd, clientId);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldRequestId);
			strcpy(postFieldsEnd, escRequestId);

			curl_free(escRequestId);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/IAuthenticationService/PollAuthSessionStatus/v1/");
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

			const auto iterResponse = parsed.FindMember("response");
			if (iterResponse == parsed.MemberEnd() || iterResponse->value.ObjectEmpty())
			{
				putsnn("request unsucceeded\n");
				return false;
			}

			const auto iterRefreshToken = iterResponse->value.FindMember("refresh_token");
			const auto iterAccessToken = iterResponse->value.FindMember("access_token");
			if (iterRefreshToken == iterResponse->value.MemberEnd() || iterAccessToken == iterResponse->value.MemberEnd())
			{
				putsnn("not logged in\n");
				return false;
			}

			strcpy(outRefreshToken, iterRefreshToken->value.GetString());
			strcpy(outAccessToken, iterAccessToken->value.GetString());

			putsnn("logged in\n");
			return true;
		}

		// unused
		// only works for "client" jwt audience i think
		bool GenerateAccessTokenForApp(CURL* curl, const char* steamId64, const char* refreshToken, char* outAccessToken)
		{
			Log(LogChannel::STEAM, "Generating access token...");

			const char postFieldRefreshToken[] = "refresh_token=";
			const char postFieldSteamId[] = "&steamid=";

			const size_t postFieldsBufSz =
				sizeof(postFieldRefreshToken) - 1 + jwtBufSz - 1 +
				sizeof(postFieldSteamId) - 1 + UINT64_MAX_STR_SIZE - 1 + 1;

			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldRefreshToken);
			postFieldsEnd = stpcpy(postFieldsEnd, refreshToken);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldSteamId);
			strcpy(postFieldsEnd, steamId64);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/IAuthenticationService/GenerateAccessTokenForApp/v1/");
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

			const auto iterResponse = parsed.FindMember("response");
			if (iterResponse == parsed.MemberEnd() || iterResponse->value.ObjectEmpty())
			{
				putsnn("request unsucceeded\n");
				return false;
			}

			const char* accessToken = iterResponse->value["access_token"].GetString();

			strcpy(outAccessToken, accessToken);

			putsnn("ok\n");
			return true;
		}
	}
}