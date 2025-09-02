#pragma once

namespace Steam
{
	namespace Guard
	{
		const size_t secretsSz = PlainToBase64Size(WC_SHA_DIGEST_SIZE, WC_NO_NL_ENC);

		const size_t deviceIdBufSz = sizeof("android:") - 1 + 36 + 1;

		const size_t twoFactorCodeBufSz = 5 + 1;
		const size_t confTagMaxLen = 32; // everyone does 32 char tag limit, no idea why
		const size_t confHashSz = PlainToBase64Size(WC_SHA_DIGEST_SIZE, WC_NO_NL_ENC);
		const size_t confIdBufSz = UINT64_MAX_STR_SIZE;
		const size_t confKeyBufSz = UINT64_MAX_STR_SIZE;

		const size_t confQueueParamsBufSz =
			sizeof("m=android&p=") - 1 + deviceIdBufSz - 1 +
			sizeof("&a=") - 1 + UINT64_MAX_STR_SIZE - 1 +
			sizeof("&k=") - 1 + confHashSz * 3 + // multiply by 3 due to URL encoding
			sizeof("&t=") - 1 + UINT64_MAX_STR_SIZE - 1 +
			sizeof("&tag=") - 1 + confTagMaxLen + 1;

		time_t timeDiff = 0;

		bool SyncTime(CURL* curl)
		{
			Log(LogChannel::STEAM, "Syncing time...");

			curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/ITwoFactorService/QueryTime/v1/");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

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

			const char* serverTime = iterResponse->value["server_time"].GetString();

			timeDiff = time(nullptr) - atoll(serverTime);

			putsnn("ok\n");
			return true;
		}

		inline time_t GetSteamTime()
		{
			return time(nullptr) + timeDiff;
		}

		// out buffer size must be at least twoFactorCodeBufSz
		bool GenerateTwoFactorAuthCode(const char* sharedSecret, char* out)
		{
			Log(LogChannel::STEAM, "Generating two factor auth code...");

			byte rawShared[WC_SHA_DIGEST_SIZE + 1];
			word32 rawSharedSz = sizeof(rawShared);

			if (Base64_Decode((byte*)sharedSecret, secretsSz, rawShared, &rawSharedSz))
			{
				putsnn("shared secret decoding failed\n");
				return false;
			}

			// https://en.wikipedia.org/wiki/Time-based_one-time_password
			// https://www.rfc-editor.org/rfc/rfc4226#section-5.3
			const time_t totpInterval = 30;
			time_t totpCounter = (GetSteamTime() / totpInterval);

			// The Key (K), the Counter (C), and Data values are hashed high-order byte first
#ifdef LITTLE_ENDIAN_ORDER
			totpCounter = byteswap64(totpCounter);
#endif // LITTLE_ENDIAN

			byte hmacHash[WC_SHA_DIGEST_SIZE];

			Hmac hmac;
			if (wc_HmacSetKey(&hmac, WC_SHA, rawShared, rawSharedSz) ||
				wc_HmacUpdate(&hmac, (byte*)&totpCounter, sizeof(totpCounter)) ||
				wc_HmacFinal(&hmac, hmacHash))
			{
				putsnn("HMAC failed\n");
				return false;
			}

			const byte hotpOffset = (hmacHash[sizeof(hmacHash) - 1] & 0xF);
			uint32_t hotpBinCode = *(uint32_t*)(hmacHash + hotpOffset);

			// We treat the dynamic binary code as a 31-bit, unsigned, big-endian integer
#ifdef LITTLE_ENDIAN_ORDER
			hotpBinCode = byteswap32(hotpBinCode);
#endif // LITTLE_ENDIAN

			hotpBinCode &= 0x7FFFFFFF;

			const char codeChars[] = "23456789BCDFGHJKMNPQRTVWXY";
			const size_t codeCharsCount = sizeof(codeChars) - 1;

			for (size_t i = 0; i < (twoFactorCodeBufSz - 1); ++i)
			{
				out[i] = codeChars[hotpBinCode % codeCharsCount];
				hotpBinCode /= codeCharsCount;
			}
			out[twoFactorCodeBufSz - 1] = '\0';

			putsnn("ok\n");
			return true;
		}

		// out buffer size must be at least confHashBufSz
		// outLen is the size of out buffer on input and result size on output
		bool GenerateConfirmationHash(const char* identitySecret, time_t timestamp, const char* tag, byte* out, word32* outSz)
		{
			size_t tagLen = strlen(tag);
			if (tagLen > confTagMaxLen)
				tagLen = confTagMaxLen;

			const size_t msgBufSz = sizeof(timestamp) + confTagMaxLen;
			byte msg[msgBufSz];
			const size_t msgSz = sizeof(timestamp) + tagLen;

#ifdef LITTLE_ENDIAN_ORDER
			timestamp = byteswap64(timestamp);
#endif // LITTLE_ENDIAN

			*(time_t*)msg = timestamp;

			memcpy(msg + sizeof(timestamp), tag, tagLen);

			byte rawIdentity[WC_SHA_DIGEST_SIZE + 1];
			word32 rawIdentitySz = sizeof(rawIdentity);

			Hmac hmac;
			byte hmacHash[WC_SHA_DIGEST_SIZE];

			const bool success =
				(!Base64_Decode((byte*)identitySecret, secretsSz, rawIdentity, &rawIdentitySz) &&
					!wc_HmacSetKey(&hmac, WC_SHA, rawIdentity, rawIdentitySz) &&
					!wc_HmacUpdate(&hmac, msg, msgSz) &&
					!wc_HmacFinal(&hmac, hmacHash) &&
					!Base64_Encode_NoNl(hmacHash, sizeof(hmacHash), out, outSz));

			return success;
		}

		// out buffer size must be at least confQueueParamsBufSz
		bool GenerateConfirmationQueryParams(CURL* curl, const char* steamId64, const char* identitySecret, 
			const char* deviceId, const char* tag, char* out)
		{
			const time_t timestamp = GetSteamTime();

			byte hash[confHashSz];
			word32 hashSz = confHashSz;

			if (!GenerateConfirmationHash(identitySecret, timestamp, tag, hash, &hashSz))
				return false;

			char* escapedHash = curl_easy_escape(curl, (char*)hash, hashSz);

			char* outEnd = out;
			outEnd = stpcpy(outEnd, "m=android&p=");
			outEnd = stpcpy(outEnd, deviceId);
			outEnd = stpcpy(outEnd, "&a=");
			outEnd = stpcpy(outEnd, steamId64);
			outEnd = stpcpy(outEnd, "&k=");
			outEnd = stpcpy(outEnd, escapedHash);
			outEnd = stpcpy(outEnd, "&t=");
			outEnd = stpcpy(outEnd, std::to_string(timestamp).c_str());
			outEnd = stpcpy(outEnd, "&tag=");
			strcpy(outEnd, tag);

			curl_free(escapedHash);

			return true;
		}

		bool FetchConfirmations(CURL* curl, const char* steamId64, const char* identitySecret, 
			const char* deviceId, rapidjson::Document* out)
		{
			Log(LogChannel::STEAM, "Fetching confirmations...");

			char postFields[confQueueParamsBufSz];
			if (!GenerateConfirmationQueryParams(curl, steamId64, identitySecret, deviceId, "conf", postFields))
			{
				putsnn("query params generation failed\n");
				return false;
			}

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/mobileconf/getlist");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

			const CURLcode respCode = curl_easy_perform(curl);

			if (respCode != CURLE_OK)
			{
				Curl::PrintError(curl, respCode);
				return false;
			}

			out->Parse(response.data);

			if (out->HasParseError())
			{
				putsnn("JSON parsing failed\n");
				return false;
			}

			if (!(*out)["success"].GetBool())
			{
				putsnn("request unsucceeded\n");
				return false;
			}

			putsnn("ok\n");
			return true;
		}

		bool AcceptConfirmation(CURL* curl, 
			const char* steamId64, const char* identitySecret, const char* deviceId, const char* offerId)
		{
			rapidjson::Document docConfs;
			if (!FetchConfirmations(curl, steamId64, identitySecret, deviceId, &docConfs))
				return false;

			Log(LogChannel::STEAM, "Accepting confirmation...");

			const char cId[] = "&op=allow&cid=";
			const char cK[] = "&ck=";

			const size_t postFieldsBufSz = 
				confQueueParamsBufSz - 1 + 
				sizeof(cId) - 1 + confIdBufSz - 1 +
				sizeof(cK) - 1 + confKeyBufSz - 1 + 1;

			char postFields[postFieldsBufSz];

			if (!GenerateConfirmationQueryParams(curl, steamId64, identitySecret, deviceId, "allow", postFields))
			{
				putsnn("query params generation failed\n");
				return false;
			}

			const char* confId = nullptr;
			const char* confNonce = nullptr;

			const rapidjson::Value& confs = docConfs["conf"];
			const rapidjson::SizeType confCount = confs.Size();

			for (rapidjson::SizeType i = 0; i < confCount; ++i)
			{
				const rapidjson::Value& conf = confs[i];

				if (conf["type"].GetInt() == 2 && !strcmp(conf["creator_id"].GetString(), offerId))
				{
					confId = conf["id"].GetString();
					confNonce = conf["nonce"].GetString();
				}
			}

			if (!confId || !confNonce)
			{
				putsnn("finding confirmation params failed\n");
				return false;
			}

			char* postFieldsEnd = postFields + strlen(postFields);
			postFieldsEnd = stpcpy(postFieldsEnd, cId);
			postFieldsEnd = stpcpy(postFieldsEnd, confId);
			postFieldsEnd = stpcpy(postFieldsEnd, cK);
			strcpy(postFieldsEnd, confNonce);

			Curl::CResponse respOp;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respOp);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/mobileconf/ajaxop");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

			const CURLcode respCodeOp = curl_easy_perform(curl);

			if (respCodeOp != CURLE_OK)
			{
				Curl::PrintError(curl, respCodeOp);
				return false;
			}

			rapidjson::Document parsedOp;
			parsedOp.ParseInsitu(respOp.data);

			if (parsedOp.HasParseError())
			{
				putsnn("JSON parsing failed\n");
				return false;
			}

			if (!parsedOp["success"].GetBool())
			{
				putsnn("request unsucceeded\n");
				return false;
			}

			putsnn("ok\n");
			return true;
		}

		// unused
		bool AcceptConfirmations(CURL* curl, 
			const char* steamId64, const char* identitySecret, const char* deviceId, 
			const char** offerIds, size_t offerIdCount)
		{
			rapidjson::Document docConfs;
			if (!FetchConfirmations(curl, steamId64, identitySecret, deviceId, &docConfs))
				return false;

			Log(LogChannel::STEAM, "Accepting confirmations...");

			const char opAllow[] = "&op=allow";

			const size_t confParamsLen = sizeof("&cid[]=&ck[]=") - 1 + confIdBufSz - 1 + confKeyBufSz - 1;
			const size_t postFieldsBufSz = confQueueParamsBufSz - 1 + sizeof(opAllow) - 1 + confParamsLen * offerIdCount + 1;

			char* postFields = (char*)malloc(postFieldsBufSz);
			if (!postFields)
			{
				putsnn("allocation failed\n");
				return false;
			}

			if (!GenerateConfirmationQueryParams(curl, steamId64, identitySecret, deviceId, "allow", postFields))
			{
				free(postFields);
				putsnn("query params generation failed\n");
				return false;
			}

			size_t confirmedCount = 0;

			char* postFieldsEnd = postFields + strlen(postFields);
			postFieldsEnd = stpcpy(postFieldsEnd, opAllow);

			for (size_t i = 0; i < offerIdCount; ++i)
			{
				const char* confId = nullptr;
				const char* confNonce = nullptr;

				const rapidjson::Value& confs = docConfs["conf"];
				const rapidjson::SizeType confCount = confs.Size();

				for (rapidjson::SizeType j = 0; j < confCount; ++j)
				{
					const rapidjson::Value& conf = confs[j];

					if (conf["type"].GetInt() == 2 && !strcmp(conf["creator_id"].GetString(), offerIds[i]))
					{
						confId = conf["id"].GetString();
						confNonce = conf["nonce"].GetString();
					}
				}

				if (!confId || !confNonce)
					continue;

				postFieldsEnd = stpcpy(postFieldsEnd, "&cid[]=");
				postFieldsEnd = stpcpy(postFieldsEnd, confId);
				postFieldsEnd = stpcpy(postFieldsEnd, "&ck[]=");
				postFieldsEnd = stpcpy(postFieldsEnd, confNonce);

				++confirmedCount;
			}

			Curl::CResponse respMultiOp;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respMultiOp);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/mobileconf/multiajaxop");
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

			const CURLcode respCodeMultiOp = curl_easy_perform(curl);

			free(postFields);

			if (respCodeMultiOp != CURLE_OK)
			{
				Curl::PrintError(curl, respCodeMultiOp);
				return false;
			}

			rapidjson::Document parsedMultiOp;
			parsedMultiOp.ParseInsitu(respMultiOp.data);

			if (parsedMultiOp.HasParseError())
			{
				putsnn("JSON parsing failed\n");
				return false;
			}

			if (!parsedMultiOp["success"].GetBool())
			{
				putsnn("request unsucceeded\n");
				return false;
			}

			if (confirmedCount != offerIdCount)
				printf("accepted %zu out of %zu\n", confirmedCount, offerIdCount);
			else
				putsnn("ok\n");

			return true;
		}

		// out buffer size must be at least deviceIdBufSz
		bool GetDeviceId(CURL* curl, const char* steamId64, const char* accessToken, char* out)
		{
			Log(LogChannel::STEAM, "Getting Steam Guard Mobile device ID...");

			const char postFieldAccessToken[] = "access_token=";
			const char postFieldSteamId[] = "&steamid=";

			const size_t postFieldsBufSz =
				sizeof(postFieldAccessToken) - 1 + Auth::jwtBufSz +
				sizeof(postFieldSteamId) - 1 + UINT64_MAX_STR_SIZE - 1 + 1;

			char postFields[postFieldsBufSz];

			char* postFieldsEnd = postFields;
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldAccessToken);
			postFieldsEnd = stpcpy(postFieldsEnd, accessToken);
			postFieldsEnd = stpcpy(postFieldsEnd, postFieldSteamId);
			strcpy(postFieldsEnd, steamId64);

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://api.steampowered.com/ITwoFactorService/QueryStatus/v1/");
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

			const char* deviceId = iterResponse->value["device_identifier"].GetString();

			strcpy(out, deviceId);

			putsnn("ok\n");
			return true;
		}
	}
}