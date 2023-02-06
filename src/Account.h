#pragma once

#define ACCOUNT_SAVED_FIELDS_SZ offsetof(CAccount, name)

class CAccount
{
public:
	char		marketApiKey[Market::apiKeySz + 1] = "";
	char		identitySecret[Steam::Guard::secretsSz + 1] = "";
	char		deviceId[Steam::Guard::deviceIdBufSz] = "";
	char		steamApiKey[Steam::apiKeyBufSz] = "";

	char		steamId64[UINT64_MAX_STR_SIZE] = "";

	char		oauthToken[Steam::Auth::oauthTokenBufSz] = "";
	char		loginToken[Steam::Auth::loginTokenBufSz] = "";

	//char		refreshToken[Steam::Auth::jwtBufSz] = "";
	//char		accessToken[Steam::Auth::jwtBufSz] = "";

	//char		sessionId[Steam::Auth::sessionIdBufSz] = "";


	// everything below isn't saved


	char		name[128] = "";

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


	static constexpr const char	directory[] = "accounts";
	static constexpr const char	extension[] = ".bin";

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

		Log(LogChannel::GENERAL, "[%s] Saving...", name);

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
		Log(LogChannel::GENERAL, "[%s] Reading...", name);

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
		Log(LogChannel::GENERAL, "[%s] Importing maFile...", name);

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
		strcpy(oauthToken, iterSession->value["OAuthToken"].GetString());
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

			Curl::CResponse response;
			if (0 <= Steam::Guard::FetchConfirmations(curl, steamId64, identitySecret, deviceId, &response))
				break;
		}
		return true;
	}

	bool Init(CURL* curl, const char* sessionId, 
		const char* encryptPass, const char* name_ = nullptr, const char* path = nullptr, bool isMaFile = false)
	{
		char username[Steam::Auth::usernameBufSz] = "";
		char sharedSecret[Steam::Guard::secretsSz + 1] = "";

		if (name_)
			strcpy(name, name_);
		else if (!GetUserInputString("Enter new account alias", name, sizeof(name)))
			return false;

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

		bool loginRequired = true;

		if (oauthToken[0])
		{
			const int refreshRes = Steam::Auth::RefreshOAuthSession(curl, oauthToken, loginToken);

			if (refreshRes < 0)
				return false;
			else if (refreshRes == 0)
				Log(LogChannel::GENERAL, "[%s] Steam OAuth token is invalid or has expired, login required\n", name);
			else
				loginRequired = false;
		}

		bool loggedIn = !loginRequired;

		if (loginRequired)
		{
			char password[Steam::Auth::passwordBufSz];
			char twoFactorCode[Steam::Guard::twoFactorCodeBufSz] = "";

			const auto EnterTwoFactorAuthCode = [&]
			{
				return GetUserInputString("Enter Steam Guard Mobile Authenticator code",
					twoFactorCode, sizeof(twoFactorCode), Steam::Guard::twoFactorCodeBufSz - 1);
			};

			if ((username[0] || GetUserInputString("Enter Steam username", username, sizeof(username))) &&
				GetUserInputString("Enter Steam password", password, sizeof(password), 8, false) &&
				((sharedSecret[0] && Steam::Guard::GenerateTwoFactorAuthCode(sharedSecret, twoFactorCode)) ||
				EnterTwoFactorAuthCode()))
			{
				const size_t retryCount = 3;

				for (size_t i = 0; i < retryCount; ++i)
				{
					const Steam::Auth::LoginResult loginRes =
						Steam::Auth::DoLogin(curl, username, password, twoFactorCode, steamId64, oauthToken, loginToken);

					if (loginRes == Steam::Auth::LoginResult::OK)
					{
						loggedIn = true;
						break;
					}
					else if (loginRes == Steam::Auth::LoginResult::WRONG_TWO_FACTOR)
					{
						if (sharedSecret[0] || !EnterTwoFactorAuthCode())
							break;
					}
					else if ((loginRes == Steam::Auth::LoginResult::PASS_ENCRYPT_FAILED) ||
							(loginRes == Steam::Auth::LoginResult::UNSUCCEDED) ||
							(loginRes == Steam::Auth::LoginResult::OAUTH_FAILED))
							break;
				}
			}

			memset(password, 0, sizeof(password));
			memset(twoFactorCode, 0, sizeof(twoFactorCode));
		}

		memset(username, 0, sizeof(username));
		memset(sharedSecret, 0, sizeof(sharedSecret));

		if (!loggedIn)
			return false;

		if (!deviceId[0] && !Steam::Guard::GetDeviceId(curl, steamId64, oauthToken, deviceId))
			return false;

		if (!Steam::SetLoginCookie(curl, steamId64, loginToken))
			return false;

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

		if (!Steam::SetInventoryPublic(curl, sessionId, steamId64))
			return false;

		if (!Market::SetSteamDetails(curl, marketApiKey, steamApiKey))
			return false;

		if (!Market::CanSell(curl, marketApiKey))
			return false;

		return true;
	}

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

	int GetMarketStatus(CURL* curl, int market, rapidjson::SizeType* outItemCount)
	{
		rapidjson::Document docItems;
		if (!Market::GetItems(curl, marketApiKey, market, &docItems))
		{
			Log(LogChannel::GENERAL, "[%s] [%s] Getting items status failed\n", name, Market::marketNames[market]);
			return -1;
		}

		auto& marketGivenItemIds = givenItemIds[market];
		auto& marketTakenItemIds = takenItemIds[market];

		int marketStatus = 0;

		const rapidjson::Value& items = docItems["items"];
		const rapidjson::SizeType itemCount = (items.IsArray() ? items.Size() : 0);

		for (rapidjson::SizeType i = 0; i < itemCount; ++i)
		{
			const rapidjson::Value& item = items[i];

			// status is a string, convert it to int
			const int itemStatus = (item["status"].GetString()[0] - '0');

			if (itemStatus == (int)Market::ItemStatus::GIVE)
			{
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
					Log(LogChannel::GENERAL, "[%s] [%s] Sold \"%s\"\n", name, Market::marketNames[market], itemName);
				}

				marketStatus |= (int)MarketStatus::SOLD;
			}
			else if (itemStatus == (int)Market::ItemStatus::TAKE)
			{
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
					Log(LogChannel::GENERAL, "[%s] [%s] Bought \"%s\"\n", name, Market::marketNames[market], itemName);
				}

				marketStatus |= (int)MarketStatus::BOUGHT;
			}
		}

		if (!(marketStatus & (int)MarketStatus::SOLD))
			marketGivenItemIds.clear();

		if (!(marketStatus & (int)MarketStatus::BOUGHT))
			marketTakenItemIds.clear();

		*outItemCount = itemCount;

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
				Log(LogChannel::GENERAL, "[%s] [%s] Converting offer items JSON to string failed\n",
					name, Market::marketNames[market]);
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
		}

		return allOk;
	}

	bool TakeItems(CURL* curl, const char* sessionId, int market)
	{
		char offerId[Steam::Trade::offerIdBufSz];
		char partnerId64[UINT64_MAX_STR_SIZE];

		if (!Market::RequestTake(curl, marketApiKey, market, offerId, partnerId64))
			return false;

		for (const auto& takenOfferId : takenOfferIds[market])
		{
			if (!strcmp(offerId, takenOfferId.c_str()))
				return true;
		}

		if (!Steam::Trade::Accept(curl, sessionId, offerId, partnerId64))
			return false;

		takenOfferIds[market].emplace_back(offerId);

		return true;
	}

	void PrintListings(const rapidjson::SizeType* itemCounts)
	{
		Log(LogChannel::GENERAL, "[%s] Listings: ", name);

		for (int i = 0; i < (int)Market::Market::COUNT; ++i)
		{
			printf("%s: %u", Market::marketNames[i], itemCounts[i]);

			if (i < ((int)Market::Market::COUNT - 1))
				putsnn(" | ");
		}

		putchar('\n');
	}

	bool RunMarkets(CURL* curl, const char* sessionId)
	{
		const int refreshRes = Steam::Auth::RefreshOAuthSession(curl, oauthToken, loginToken);
		if (refreshRes < 0)
		{
			Log(LogChannel::GENERAL, "[%s] Steam session refresh failed\n", name);
			return false;
		}

		if (refreshRes == 0)
		{
			Log(LogChannel::GENERAL, "[%s] Steam OAuth token has expired, restart required\n", name);
			return false;
		}

		if (!Steam::SetLoginCookie(curl, steamId64, loginToken))
		{
			Log(LogChannel::GENERAL, "[%s] Setting Steam login cookie failed\n", name);
			return false;
		}

		bool allOk = true;

		if (!CancelExpiredSentOffers(curl, sessionId))
		{
			allOk = false;
			Log(LogChannel::GENERAL, "[%s] Cancelling some of the expired sent offers failed, "
				"manually cancel the sent offers older than 15 mins if the error continues\n", name);
		}

		if (!Market::Ping(curl, marketApiKey))
			allOk = false;

		rapidjson::SizeType itemCounts[(int)Market::Market::COUNT] = { 0 };

		for (int i = 0; i < (int)Market::Market::COUNT; ++i)
		{
			const int marketStatus = GetMarketStatus(curl, i, &itemCounts[i]);

			if (marketStatus < 0)
			{
				allOk = false;
				continue;
			}

			if (!marketStatus)
				continue;
#ifdef _WIN32
			FlashCurrentWindow();
#endif // _WIN32

			if (marketStatus & (int)MarketStatus::SOLD)
			{
				if (Market::isMarketP2P[i])
				{
					if (!GiveItemsP2P(curl, sessionId, i))
						allOk = false;
				}
				else
				{
					if (!GiveItemBot(curl, sessionId, i))
						allOk = false;
				}
			}
			else
			{
				if (!Market::isMarketP2P[i])
					givenOfferIds[i].clear();
			}


			if (marketStatus & (int)MarketStatus::BOUGHT)
			{
				if (!TakeItems(curl, sessionId, i))
					allOk = false;
			}
			else
				takenOfferIds[i].clear();
		}

		PrintListings(itemCounts);

		return allOk;
	}
};