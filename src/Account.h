#pragma once

#define ACCOUNT_SAVED_FIELDS_SZ offsetof(CAccount, name)

class CAccount
{
	char		marketApiKey[Market::apiKeySz + 1] = "";
	char		identitySecret[Steam::Guard::secretsSz + 1] = "";
	char		deviceId[Steam::Guard::deviceIdBufSz] = "";
	char		steamApiKey[Steam::apiKeyBufSz] = "";

	char		steamId64[UINT64_MAX_STR_SIZE] = "";

	// commented out because oauth seems to be gone
	//char		oauthToken[Steam::Auth::oauthTokenBufSz] = "";
	//char		loginToken[Steam::Auth::loginTokenBufSz] = "";

	char		refreshToken[Steam::Auth::jwtBufSz] = "";

	//char		sessionId[Steam::Auth::sessionIdBufSz] = "";


	// everything below isn't saved

	char		name[PATH_MAX] = "";

	class COffer
	{
	public:
		char marketHash[Market::hashBufSz];
		char tradeOfferId[Steam::Trade::offerIdBufSz];

		COffer(const char* hash, const char* offerId)
		{
			strcpy(marketHash, hash);
			strcpy(tradeOfferId, offerId);
		}
	};

	std::vector<COffer>			sentOffers[(int)Market::Market::COUNT];
	std::vector<std::string>	givenItemIds[(int)Market::Market::COUNT];
	std::vector<std::string>	takenItemIds[(int)Market::Market::COUNT];
	std::vector<std::string>	givenOfferIds[(int)Market::Market::COUNT];
	std::vector<std::string>	takenOfferIds[(int)Market::Market::COUNT];


public:
	static constexpr const char	directory[] = "accounts";
	static constexpr const char	extension[] = ".bin";

private:
	static constexpr int		scryptCost = 16;			// (128 * (2^16) * 8) = 64 MB RAM
	static constexpr int		scryptBlockSz = 8;
	static constexpr int		scryptParallel = 1;

	static constexpr size_t		keySz = AES_256_KEY_SIZE;
	static constexpr size_t		saltSz = (128 / 8);			// NIST recommends at least 128 bits
	static constexpr size_t		ivSz = GCM_NONCE_MID_SZ;
	static constexpr size_t		authTagSz = (128 / 8);		// max allowed tag size is 128 bits


	bool Save(const char* encryptPass)
	{
		byte salt[saltSz];
		byte iv[ivSz];
		byte authTag[authTagSz];
		byte cipher[ACCOUNT_SAVED_FIELDS_SZ];

		const bool encryptFailed = 
			!Crypto::Encrypt(encryptPass, keySz, scryptCost, scryptBlockSz, scryptParallel,
				(byte*)this, ACCOUNT_SAVED_FIELDS_SZ,
				salt, saltSz,
				iv, ivSz, 
				authTag, authTagSz,
				cipher);

		if (encryptFailed)
			return false;

		Log(LogChannel::GENERAL, "Saving...");

		const std::filesystem::path dir(directory);

		if (!std::filesystem::exists(dir) && !std::filesystem::create_directory(dir))
		{
			putsnn("accounts directory creation failed\n");
			return false;
		}

		char path[PATH_MAX];

		char* pathEnd = path;
		pathEnd = stpcpy(pathEnd, directory);
		pathEnd = stpcpy(pathEnd, "/");
		pathEnd = stpcpy(pathEnd, name);
		strcpy(pathEnd, extension);

		FILE* file = u8fopen(path, "wb");
		if (!file)
		{
			putsnn("file creation failed\n");
			return false;
		}

		const bool writeFailed = 
			((fwrite(salt, sizeof(byte), sizeof(salt), file)		!= sizeof(salt)) ||
			(fwrite(iv, sizeof(byte), sizeof(iv), file)				!= sizeof(iv)) ||
			(fwrite(authTag, sizeof(byte), sizeof(authTag), file)	!= sizeof(authTag)) ||
			(fwrite(cipher, sizeof(byte), sizeof(cipher), file)		!= sizeof(cipher)));

		fclose(file);

		if (writeFailed)
		{
			putsnn("writing failed\n");
			return false;
		}

		putsnn("ok\n");
		return true;
	}

	bool Load(const char* path, const char* decryptPass)
	{
		Log(LogChannel::GENERAL, "Reading...");

		unsigned char* contents = nullptr;
		long contentsSz = 0;
		if (!ReadFile(path, &contents, &contentsSz))
		{
			putsnn("fail\n");
			return false;
		}

		putsnn("ok\n");

		const byte* salt = contents;
		const byte* iv = salt + saltSz;
		const byte* authTag = iv + ivSz;
		const byte* cipher = authTag + authTagSz;

		const bool decryptFailed = 
			!Crypto::Decrypt(decryptPass, keySz, scryptCost, scryptBlockSz, scryptParallel,
				cipher, ACCOUNT_SAVED_FIELDS_SZ,
				salt, saltSz,
				iv, ivSz,
				authTag, authTagSz,
				(byte*)this);

		free(contents);

		if (decryptFailed)
			return false;

		return true;
	}

	// outUsername buffer size must be at least Steam::Auth::usernameBufSz
	// outSharedSecret buffer size must be at least Steam::Guard::secretsSz + 1
	bool ImportMaFile(const char* path, char* outUsername, char* outSharedSecret)
	{
		Log(LogChannel::GENERAL, "Importing maFile...");

		unsigned char* contents = nullptr;
		long contentsSz = 0;
		if (!ReadFile(path, &contents, &contentsSz))
		{
			putsnn("reading failed\n");
			return false;
		}

		if (contents[0] != '{')
		{
			free(contents);
			putsnn("invalid maFile, disable encryption in SDA before importing\n");
			return false;
		}

		rapidjson::Document parsed;
		parsed.Parse((char*)contents, contentsSz);

		free(contents);

		if (parsed.HasParseError())
		{
			putsnn("JSON parsing failed\n");
			return false;
		}

		const auto iterSession = parsed.FindMember("Session");
		if (iterSession == parsed.MemberEnd() || iterSession->value.ObjectEmpty())
		{
			putsnn("session info not found or empty\n");
			return false;
		}

		//const char* steamLoginSecure = iterSession->value["SteamLoginSecure"].GetString();
		//const char* steamLoginSecureDelim = strchr(steamLoginSecure, '%');

		strcpy(identitySecret, parsed["identity_secret"].GetString());
		strcpy(deviceId, parsed["device_id"].GetString());
		//strcpy(sessionId, iterSession->value["SessionID"].GetString());
		//strcpy(loginToken, steamLoginSecureDelim + 6);
		//strcpy(oauthToken, iterSession->value["OAuthToken"].GetString());	// commented out because oauth seems to be gone
		//stpncpy(steamId64, steamLoginSecure, (steamLoginSecureDelim - steamLoginSecure))[0] = '\0';
		strcpy(steamId64, std::to_string(iterSession->value["SteamID"].GetUint64()).c_str());

		strcpy(outUsername, parsed["account_name"].GetString());
		strcpy(outSharedSecret, parsed["shared_secret"].GetString());

		putsnn("ok\n");
		return true;
	}

	bool EnterMarketApiKey(CURL* curl)
	{
		while (true)
		{
			if (!GetUserInputString("Enter market API key", marketApiKey, Market::apiKeySz + 1, Market::apiKeySz))
				return false;

			rapidjson::Document parsed;
			if (Market::GetProfileStatus(curl, marketApiKey, &parsed))
				break;
		}
		return true;
	}

	bool EnterIdentitySecret(CURL* curl)
	{
		while (true)
		{
			if (!GetUserInputString("Enter Steam Guard identity_secret", 
					identitySecret, Steam::Guard::secretsSz + 1, Steam::Guard::secretsSz))
				return false;

			rapidjson::Document doc;
			if (Steam::Guard::FetchConfirmations(curl, steamId64, identitySecret, deviceId, &doc))
				break;
		}
		return true;
	}

	bool DidJWTExpire(const char* jwt)
	{
		const char* pszJwtHeaderEnd = strchr(jwt, '.');
		if (!pszJwtHeaderEnd)
		{
			Log(LogChannel::GENERAL, "Invalid refresh token\n");
			return false;
		}

		const char* pszJwtPayloadEnd = strchr(pszJwtHeaderEnd + 1, '.');
		if (!pszJwtPayloadEnd)
		{
			Log(LogChannel::GENERAL, "Invalid refresh token\n");
			return false;
		}

		const size_t encJwtPayloadSz = pszJwtPayloadEnd - pszJwtHeaderEnd - 1;

		const size_t encJwtPayloadPaddedSz = GetBase64PaddedLen(encJwtPayloadSz);
		byte* encJwtPayloadPadded = (byte*)malloc(encJwtPayloadPaddedSz);
		if (!encJwtPayloadPadded)
		{
			Log(LogChannel::GENERAL, "Padded JWT payload allocation failed\n");
			return false;
		}

		memcpy(encJwtPayloadPadded, pszJwtHeaderEnd + 1, encJwtPayloadSz);

		// add padding at the end
		for (size_t i = encJwtPayloadSz; i < encJwtPayloadPaddedSz; ++i)
			encJwtPayloadPadded[i] = '=';

		word32 jwtPayloadSz = Base64ToPlainSize(encJwtPayloadPaddedSz, WC_NO_NL_ENC);
		// add null for rapidjson
		char* jwtPayload = (char*)malloc(jwtPayloadSz + 1);
		if (!jwtPayload)
		{
			free(encJwtPayloadPadded);
			Log(LogChannel::GENERAL, "Plaintext JWT payload allocation failed\n");
			return false;
		}

		Base64URLToBase64(jwtPayload, jwtPayloadSz);

		if (Base64_Decode(encJwtPayloadPadded, encJwtPayloadPaddedSz, (byte*)jwtPayload, &jwtPayloadSz))
		{
			free(encJwtPayloadPadded);
			free(jwtPayload);

			Log(LogChannel::GENERAL, "JWT payload decoding failed\n");
			return false;
		}

		jwtPayload[jwtPayloadSz] = '\0';

		free(encJwtPayloadPadded);

		rapidjson::Document docJwtPayload;
		docJwtPayload.ParseInsitu(jwtPayload);

		const time_t exp = docJwtPayload["exp"].GetUint64();

		free(jwtPayload);

		const time_t curTime = time(nullptr);

		return (curTime >= exp);
	}

public:
	bool Init(CURL* curl, const char* sessionId, const char* encryptPass, 
		const char* argName = nullptr, const char* path = nullptr, bool isMaFile = false)
	{
		char username[Steam::Auth::usernameBufSz] = "";
		char sharedSecret[Steam::Guard::secretsSz + 1] = "";

		if (!name[0])
		{
			if (argName)
				strcpy(name, argName);
			else if (!GetUserInputString("Enter new account alias", name, sizeof(name)))
				return false;
		}

		CLoggingContext loggingContext(name);

		if (path)
		{
			if (isMaFile)
			{
				if (!ImportMaFile(path, username, sharedSecret))
					return false;
			}
			else
			{
				if (!Load(path, encryptPass))
					return false;
			}
		}

		char accessToken[Steam::Auth::jwtBufSz] = "";

		bool loginRequired = true;

		// commented out because oauth seems to be gone
		//if (oauthToken[0])
		//{
		//	const int refreshRes = Steam::Auth::RefreshOAuthSession(curl, oauthToken, loginToken);

		//	if (refreshRes < 0)
		//		return false;
		//	else if (refreshRes == 0)
		//		Log(LogChannel::GENERAL, "[%s] Steam OAuth token is invalid or has expired, login required\n", name);
		//	else
		//		loginRequired = false;
		//}

		if (refreshToken[0])
		{
			if (!DidJWTExpire(refreshToken) &&
				Steam::SetRefreshCookie(curl, steamId64, refreshToken) && 
				Steam::Auth::RefreshJWTSession(curl, accessToken) &&
				Steam::SetLoginCookie(curl, steamId64, accessToken))
				loginRequired = false;
			else
				Log(LogChannel::GENERAL, "Steam refresh token is invalid or has expired, login required\n");
		}

		bool loggedIn = !loginRequired;

		if (loginRequired)
		{
			char password[Steam::Auth::passwordBufSz];

			if ((username[0] || GetUserInputString("Enter Steam username", username, sizeof(username))) &&
				GetUserInputString("Enter Steam password", password, sizeof(password), 8, false))
			{
				char clientId[Steam::Auth::clientIdBufSz];
				char requestId[Steam::Auth::requestIdBufSz];

				if (Steam::Auth::BeginAuthSessionViaCredentials(curl, username, password, steamId64, clientId, requestId))
				{
					for (size_t i = 0; i < 3; ++i)
					{
						char twoFactorCode[Steam::Guard::twoFactorCodeBufSz] = "";

						if (sharedSecret[0])
							Steam::Guard::GenerateTwoFactorAuthCode(sharedSecret, twoFactorCode);

						if (!twoFactorCode[0])
						{
							if (!GetUserInputString("Enter Steam Guard Mobile Authenticator code",
								twoFactorCode, sizeof(twoFactorCode), Steam::Guard::twoFactorCodeBufSz - 1))
								break;
						}

						if (!Steam::Auth::UpdateAuthSessionWithSteamGuardCode(curl, steamId64, clientId, twoFactorCode))
							break;

						if (Steam::Auth::PollAuthSessionStatus(curl, clientId, requestId, refreshToken, accessToken))
						{
							loggedIn = true;
							break;
						}

						std::this_thread::sleep_for(5s);
					}

					if (loggedIn)
					{
						if (!Steam::SetRefreshCookie(curl, steamId64, refreshToken) ||
							!Steam::SetLoginCookie(curl, steamId64, accessToken))
						{
							memset(refreshToken, 0, sizeof(refreshToken));
							memset(accessToken, 0, sizeof(accessToken));

							loggedIn = false;
						}
					}
				}
			}

			memset(password, 0, sizeof(password));
		}

		memset(username, 0, sizeof(username));
		memset(sharedSecret, 0, sizeof(sharedSecret));

		if (!loggedIn)
			return false;

		if (!deviceId[0] && !Steam::Guard::GetDeviceId(curl, steamId64, accessToken, deviceId))
			return false;

		memset(accessToken, 0, sizeof(accessToken));

		if (!steamApiKey[0] && !Steam::GetApiKey(curl, sessionId, steamApiKey))
			return false;

		if (!identitySecret[0] && !EnterIdentitySecret(curl))
			return false;

		if (!marketApiKey[0] && !EnterMarketApiKey(curl))
			return false;

		if (loginRequired || isMaFile)
		{
			if (Save(encryptPass))
			{
				if (isMaFile)
					std::filesystem::remove(path);
			}
		}

		memset(refreshToken, 0, sizeof(refreshToken));

		if (!Steam::SetInventoryPublic(curl, sessionId, steamId64))
			return false;

		if (!Steam::AcknowledgeTradeProtection(curl, sessionId))
			return false;

		if (!Market::SetSteamDetails(curl, marketApiKey, steamApiKey))
			return false;

		if (!Market::CanSell(curl, marketApiKey))
			return false;

		return true;
	}

private:
	// remove inactive and cancel expired
	bool CancelExpiredSentOffers(CURL* curl, const char* sessionId)
	{
		bool allEmpty = true;

		for (const auto& marketSentOffers : sentOffers)
		{
			if (!marketSentOffers.empty())
			{
				allEmpty = false;
				break;
			}
		}

		if (allEmpty)
			return true;

		const time_t timestamp = time(nullptr);

		rapidjson::Document docOffers;
		// include inactive offers accepted within 5 mins ago so they are kept in sentOffers
		if (!Steam::Trade::GetOffers(curl, steamApiKey, 
				true, false, false, true, false, nullptr, timestamp - (5 * 60), 0, &docOffers))
			return false;

		const rapidjson::Value& offersResp = docOffers["response"];

		const auto iterSentOffers = offersResp.FindMember("trade_offers_sent");

		if (iterSentOffers == offersResp.MemberEnd())
		{
			for (auto& marketSentOffers : sentOffers)
				marketSentOffers.clear();

			return true;
		}

		const rapidjson::Value& steamSentOffers = iterSentOffers->value;
		const rapidjson::SizeType steamSentOffersCount = steamSentOffers.Size();

		if (!steamSentOffersCount)
		{
			for (auto& marketSentOffers : sentOffers)
				marketSentOffers.clear();

			return true;
		}

		bool allOk = true;

		for (auto& marketSentOffers : sentOffers)
		{
			for (auto iterSentOffer = marketSentOffers.begin(); iterSentOffer != marketSentOffers.end(); )
			{
				bool erase = true;

				const char* sentOfferId = iterSentOffer->tradeOfferId;

				for (rapidjson::SizeType i = 0; i < steamSentOffersCount; ++i)
				{
					const rapidjson::Value& offer = steamSentOffers[i];

					const char* offerId = offer["tradeofferid"].GetString();

					if (strcmp(sentOfferId, offerId))
						continue;

					const time_t timeUpdated = offer["time_updated"].GetInt64();
					const time_t timeSinceUpdate = timestamp - timeUpdated;

					if (Market::offerTTL < timeSinceUpdate)
					{
						if (!Steam::Trade::Cancel(curl, sessionId, sentOfferId))
						{
							erase = false;
							allOk = false;
						}
					}
					else
						erase = false;

					break;
				}

				if (erase)
					iterSentOffer = marketSentOffers.erase(iterSentOffer);
				else
					++iterSentOffer;
			}
		}

		return allOk;
	}

	enum class MarketStatus
	{
		SOLD = (1 << 0),
		BOUGHT = (1 << 1)
	};

	int GetMarketStatus(CURL* curl, int market, rapidjson::Document* outDocItems)
	{
		if (!Market::GetItems(curl, marketApiKey, market, outDocItems))
		{
			Log(LogChannel::GENERAL, "[%s] Getting items status failed\n", Market::marketNames[market]);
			return -1;
		}

		auto& marketGivenItemIds = givenItemIds[market];
		auto& marketTakenItemIds = takenItemIds[market];

		int marketStatus = 0;

		const rapidjson::Value& items = (*outDocItems)["items"];
		const rapidjson::SizeType itemCount = (items.IsArray() ? items.Size() : 0);

		for (rapidjson::SizeType i = 0; i < itemCount; ++i)
		{
			const rapidjson::Value& item = items[i];

			// status is a char, convert it to int
			const int itemStatus = (item["status"].GetString()[0] - '0');

			if (itemStatus == (int)Market::ItemStatus::GIVE)
			{
				// poor mans 'trading protection' check
				const int left = item["left"].GetInt();
				if (left < 1)
					continue;

				const char* itemId = item["item_id"].GetString();

				bool given = false;

				for (const auto& givenItemId : marketGivenItemIds)
				{
					if (!strcmp(itemId, givenItemId.c_str()))
					{
						given = true;
						break;
					}
				}

				if (!given)
				{
					marketGivenItemIds.emplace_back(itemId);

					const char* itemName = item["market_hash_name"].GetString();
					Log(LogChannel::GENERAL, "[%s] Sold \"%s\"\n", Market::marketNames[market], itemName);
				}

				marketStatus |= (int)MarketStatus::SOLD;
			}
			else if (itemStatus == (int)Market::ItemStatus::TAKE)
			{
				// poor mans 'trading protection' check
				const int left = item["left"].GetInt();
				if (left < 1)
					continue;

				const char* itemId = item["item_id"].GetString();

				bool taken = false;

				for (const auto& takenItemId : marketTakenItemIds)
				{
					if (!strcmp(itemId, takenItemId.c_str()))
					{
						taken = true;
						break;
					}
				}

				if (!taken)
				{
					marketTakenItemIds.emplace_back(itemId);

					const char* itemName = item["market_hash_name"].GetString();
					Log(LogChannel::GENERAL, "[%s] Bought \"%s\"\n", Market::marketNames[market], itemName);
				}

				marketStatus |= (int)MarketStatus::BOUGHT;
			}
		}

		if (!(marketStatus & (int)MarketStatus::SOLD))
			marketGivenItemIds.clear();

		if (!(marketStatus & (int)MarketStatus::BOUGHT))
			marketTakenItemIds.clear();

		return marketStatus;
	}

	bool GiveItemBot(CURL* curl, const char* sessionId, int market)
	{
		char offerId[Steam::Trade::offerIdBufSz];
		char partnerId64[UINT64_MAX_STR_SIZE];

		if (!Market::RequestGiveBot(curl, marketApiKey, market, offerId, partnerId64))
			return false;

		for (const auto& givenOfferId : givenOfferIds[market])
		{
			if (!strcmp(offerId, givenOfferId.c_str()))
				return true;
		}

		if (!Steam::Trade::Accept(curl, sessionId, offerId, partnerId64))
			return false;

		if (!Steam::Guard::AcceptConfirmation(curl, steamId64, identitySecret, deviceId, offerId))
			return false;

		givenOfferIds[market].emplace_back(offerId);

		return true;
	}

	bool GiveItemsP2P(CURL* curl, const char* sessionId, int market)
	{
		rapidjson::Document docGiveDetails;

		if (!Market::RequestGiveP2PAll(curl, marketApiKey, market, &docGiveDetails))
			return false;

		const rapidjson::Value& offers = docGiveDetails["offers"];
		const rapidjson::SizeType offerCount = offers.Size();

		bool allOk = true;

		for (rapidjson::SizeType i = 0; i < offerCount; ++i)
		{
			const rapidjson::Value& offer = offers[i];

			const char* offerHash = offer["hash"].GetString();

			// check if we haven't sent this offer yet
			bool found = false;

			for (const auto& sentOffer : sentOffers[market])
			{
				if (!strcmp(offerHash, sentOffer.marketHash))
				{
					found = true;
					break;
				}
			}

			if (found)
				continue;

			rapidjson::StringBuffer itemsStrBuf;
			rapidjson::Writer<rapidjson::StringBuffer> itemsWriter(itemsStrBuf);

			if (!offer["items"].Accept(itemsWriter))
			{
				allOk = false;
				Log(LogChannel::GENERAL, 
					"[%s] Converting offer items JSON to string failed\n", Market::marketNames[market]);
				continue;
			}

			char sentOfferId[Steam::Trade::offerIdBufSz];

			if (!Steam::Trade::Send(curl,
				sessionId,
				offer["partner"].GetUint(),
				offer["token"].GetString(),
				offer["tradeoffermessage"].GetString(),
				itemsStrBuf.GetString(),
				sentOfferId))
			{
				allOk = false;
				continue;
			}

			if (!Steam::Guard::AcceptConfirmation(curl, steamId64, identitySecret, deviceId, sentOfferId))
			{
				allOk = false;
				continue;
			}

			sentOffers[market].emplace_back(offerHash, sentOfferId);

			if (!Market::TradeReady(curl, marketApiKey, market, sentOfferId))
			{
				allOk = false;
				continue;
			}
		}

		return allOk;
	}

	bool TakeItem(CURL* curl, const char* sessionId, int market, const char* partnerId32 = nullptr)
	{
		char offerId[Steam::Trade::offerIdBufSz];

		if (!Market::RequestTake(curl, marketApiKey, market, partnerId32, offerId))
			return false;

		for (const auto& takenOfferId : takenOfferIds[market])
		{
			if (!strcmp(offerId, takenOfferId.c_str()))
				return true;
		}

		const uint32_t nPartnerId32 = atol(partnerId32);

		const std::string partnerId64(std::to_string(Steam::SteamID32To64(nPartnerId32)));

		if (!Steam::Trade::Accept(curl, sessionId, offerId, partnerId64.c_str()))
			return false;

		takenOfferIds[market].emplace_back(offerId);

		return true;
	}

	bool TakeItems(CURL* curl, const char* sessionId, int market, rapidjson::Document* docItems)
	{
		const rapidjson::Value& items = (*docItems)["items"];
		if (!items.IsArray())
			return true;

		std::unordered_set<std::string> partnerIds32;

		for (const auto& item : items.GetArray())
		{
			const int itemStatus = (item["status"].GetString()[0] - '0');

			if (itemStatus == (int)Market::ItemStatus::TAKE)
			{
				// poor mans 'trading protection' check
				const int left = item["left"].GetInt();
				if (left < 1)
					continue;

				const auto iterBotId = item.FindMember("botid");
				if (iterBotId != item.MemberEnd())
					partnerIds32.insert(iterBotId->value.GetString());
			}
		}

		bool allOk = true;

		for (const auto& partnerId32 : partnerIds32)
		{
			if (!TakeItem(curl, sessionId, market, partnerId32.c_str()))
				allOk = false;
		}

		return allOk;
	}

	void PrintListings(const rapidjson::SizeType* itemCounts)
	{
		Log(LogChannel::GENERAL, "Listings: ");

		for (int i = 0; i < (int)Market::Market::COUNT; ++i)
		{
			printf("%s: %u", Market::marketNames[i], itemCounts[i]);

			if (i < ((int)Market::Market::COUNT - 1))
				putsnn(" | ");
		}

		putchar('\n');
	}

public:
	bool RunMarkets(CURL* curl, const char* sessionId, const char* proxy)
	{
		CLoggingContext loggingContext(name);

		// commented out because oauth seems to be gone
		//const int refreshRes = Steam::Auth::RefreshOAuthSession(curl, oauthToken, loginToken);
		//if (refreshRes < 0)
		//{
		//	Log(LogChannel::GENERAL, "[%s] Steam session refresh failed\n", name);
		//	return false;
		//}

		//if (refreshRes == 0)
		//{
		//	Log(LogChannel::GENERAL, "[%s] Steam OAuth token has expired, restart required\n", name);
		//	return false;
		//}

		char accessToken[Steam::Auth::jwtBufSz];

		if (!Steam::Auth::RefreshJWTSession(curl, accessToken))
		{
			Log(LogChannel::GENERAL, "Steam session refresh failed\n");
			return false;
		}

		if (!Steam::SetLoginCookie(curl, steamId64, accessToken))
		{
			Log(LogChannel::GENERAL, "Setting Steam login cookie failed\n");
			return false;
		}

		bool allOk = true;

		if (!Market::PingNew(curl, marketApiKey, accessToken, proxy))
			allOk = false;

		memset(accessToken, 0, sizeof(accessToken));

		if (!CancelExpiredSentOffers(curl, sessionId))
		{
			allOk = false;
			Log(LogChannel::GENERAL, "Cancelling some of the expired sent offers failed, "
				"manually cancel the sent offers older than 15 mins if the error persists\n");
		}

		rapidjson::SizeType itemCounts[(int)Market::Market::COUNT] = { 0 };

		for (int marketIter = 0; marketIter < (int)Market::Market::COUNT; ++marketIter)
		{
			rapidjson::Document docItems;
			const int marketStatus = GetMarketStatus(curl, marketIter, &docItems);
			
			if (marketStatus < 0)
			{
				allOk = false;
				continue;
			}

			const rapidjson::Value& items = docItems["items"];
			itemCounts[marketIter] = (items.IsArray() ? items.Size() : 0);

			if (!marketStatus)
				continue;

#ifdef _WIN32
			FlashCurrentWindow();
#endif // _WIN32

			if (marketStatus & (int)MarketStatus::SOLD)
			{
				// commented out because all markets are p2p now
				//if (Market::isMarketP2P[i])
				//{				
					if (!GiveItemsP2P(curl, sessionId, marketIter))
						allOk = false;
				//}
				//else
				//{
					//if (!GiveItemBot(curl, sessionId, i))
					//	allOk = false;
				//}
			}
			else
			{
				//if (!Market::isMarketP2P[i])
				//	givenOfferIds[i].clear();
			}

			if (marketStatus & (int)MarketStatus::BOUGHT)
			{
				if (!TakeItems(curl, sessionId, marketIter, &docItems))
					allOk = false;
			}
			else
				takenOfferIds[marketIter].clear();
		}

		PrintListings(itemCounts);

		return allOk;
	}
};