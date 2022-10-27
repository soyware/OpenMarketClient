#include "Precompiled.h"
#include "Misc.h"
#include "Curl.h"
#include "Crypto.h"
#include "Steam/Steam.h"
#include "Market.h"
#include "Account.h"

#define OPENMARKETCLIENT_VERSION "0.3.0"

/*	
* TODO:
* better logging
*/

void SetLocale()
{
	// LC_ALL breaks SetConsoleOutputCP
	setlocale(LC_TIME, "");
#ifdef _WIN32
	_setmode(_fileno(stdin), _O_U16TEXT);
	SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32
}

void PrintVersion()
{
	putsnn("OpenMarketClient v" OPENMARKETCLIENT_VERSION ", built with "
		"libcurl v" LIBCURL_VERSION ", "
		"wolfSSL v" LIBWOLFSSL_VERSION_STRING ", "
		"rapidJSON v" RAPIDJSON_VERSION_STRING "\n");
}

bool SetWorkDirToExeDir()
{
	const char* exeDir = GetExeDir();
	if (!exeDir)
		return false;

#ifdef _WIN32
	const size_t wideExeDirLen = PATH_MAX;
	wchar_t wideExeDir[wideExeDirLen];

	if (!MultiByteToWideChar(CP_UTF8, 0, exeDir, -1, wideExeDir, wideExeDirLen))
		return false;

#pragma warning(suppress:4996)
	return !_wchdir(wideExeDir);

#else
	return !chdir(exeDir);
#endif // _WIN32
}

namespace Args
{
	bool		printHelp = false;
	bool		newAcc = false;
	const char* proxy = nullptr;

	void Parse(int argc, char** const argv)
	{
		if (argc < 2)
			return;

		for (int i = 0; i < argc; ++i)
		{
			if (!strcmp(argv[i], "--help"))
				printHelp = true;

			else if (!strcmp(argv[i], "--new"))
				newAcc = true;

			else if ((i < (argc - 1)) && !strcmp(argv[i], "--proxy")) // check if second to last argument
				proxy = argv[i + 1];
		}
	}

	void PrintHelp()
	{
		putsnn("--help\t\t\t\t\t\t\tPrint help\n"
			"--new\t\t\t\t\t\t\tEnter new account manually\n"
			"--proxy [scheme://][username:password@]host[:port]\tSet global proxy\n");
	}
}

void InitSavedAccounts(CURL* curl, const char* sessionId, const char* encryptPass, std::vector<CAccount>* accounts)
{
	const std::filesystem::path dir(CAccount::dirName);

	if (!std::filesystem::exists(dir))
		return;

	for (const auto& entry : std::filesystem::directory_iterator(dir))
	{
		const auto& path = entry.path();
		const auto& extension = path.extension();

		const bool isMaFile = !extension.compare(".maFile");

		if (extension.compare(CAccount::extension) && !isMaFile)
			continue;

		const auto& stem = path.stem();

#ifdef _WIN32
		const auto* widePath = path.c_str();
		const auto* wideFilenameNoExt = stem.c_str();

		char szPath[PATH_MAX];
		char szFilenameNoExt[sizeof(CAccount::name)];

		if (!WideCharToMultiByte(CP_UTF8, 0, widePath, -1, szPath, sizeof(szPath), NULL, NULL) ||
			!WideCharToMultiByte(CP_UTF8, 0, wideFilenameNoExt, -1, szFilenameNoExt, sizeof(szFilenameNoExt), NULL, NULL))
		{
			Log(LogChannel::GENERAL, "One of the account's filename UTF-16 to UTF-8 mapping failed, skipping\n");
			continue;
		}

#else
		const char* szPath = path.c_str();
		const char* szFilenameNoExt = stem.c_str();
#endif

		CAccount account;

		if (account.Init(curl, sessionId, encryptPass, szFilenameNoExt, szPath, isMaFile))
			accounts->emplace_back(account);
		else
			Log(LogChannel::GENERAL, "Skipping \"%s\"\n", szFilenameNoExt);
	}
}

// remove inactive and cancel if expired
bool CancelExpiredOffers(CURL* curl, const char* sessionId, const char* steamApiKey, std::vector<std::string>* sentOfferIds)
{
	if (!sentOfferIds->size())
		return true;

	rapidjson::Document parsed;
	// UINT32_MAX so we don't get historical (inactive) offers
	if (!Steam::Trade::GetOffers(curl, steamApiKey, true, false, false, true, false, nullptr, UINT32_MAX, 0, &parsed))
		return false;

	const rapidjson::Value& response = parsed["response"];

	const auto iterActiveSentOffers = response.FindMember("trade_offers_sent");

	const rapidjson::SizeType activeSentOffersCount = 
		((iterActiveSentOffers != response.MemberEnd()) ? iterActiveSentOffers->value.Size() : 0);

	const time_t timestamp = time(nullptr);

	bool allOk = true;

	for (auto sentOfferIdIter = sentOfferIds->begin(); sentOfferIdIter != sentOfferIds->end(); )
	{
		bool remove = true;

		for (rapidjson::SizeType i = 0; i < activeSentOffersCount; ++i)
		{
			const rapidjson::Value& offer = iterActiveSentOffers->value[i];

			const char* id = offer["tradeofferid"].GetString();

			if (!strcmp(sentOfferIdIter->c_str(), id))
			{
				// double check
				const int state = offer["trade_offer_state"].GetInt();

				if (state == (int)Steam::Trade::ETradeOfferState::ACTIVE)
				{
					const time_t timeUpdated = offer["time_updated"].GetInt64();

					if (Market::offerTTL < (timestamp - timeUpdated))
					{
						if (!Steam::Trade::Cancel(curl, sessionId, id))
							allOk = false;
					}
					else
						remove = false;
				}

				break;
			}
		}

		if (remove)
			sentOfferIdIter = sentOfferIds->erase(sentOfferIdIter);
		else
			++sentOfferIdIter;
	}

	return allOk;
}

int main(int argc, char** argv)
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	SetLocale();
	PrintVersion();

	Args::Parse(argc, argv);

	if (Args::printHelp)
	{
		Args::PrintHelp();
		Pause();
		return 0;
	}

	char encryptPass[Crypto::passwordBufSz];
	if (!GetUserInputString("Enter encryption password", encryptPass, Crypto::passwordBufSz, Crypto::minPasswordLen, false))
	{
		Pause();
		return 1;
	}

	if (!SetWorkDirToExeDir())
	{
		Log(LogChannel::GENERAL, "Setting working directory failed\n");
		Pause();
		return 1;
	}

	CURL* curl = Curl::Init(Args::proxy);
	if (!curl)
	{
		Pause();
		return 1;
	}

	if (!Steam::Guard::SyncTime(curl))
	{
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		Pause();
		return 1;
	}

	char sessionId[Steam::sessionIdBufSz];

	if (!Steam::GenerateSessionId(sessionId) || !Steam::SetSessionCookie(curl, sessionId))
	{
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		Pause();
		return 1;
	}

	std::vector<CAccount> accounts;

	if (Args::newAcc)
	{
		CAccount account;

		if (account.Init(curl, sessionId, encryptPass))
			accounts.emplace_back(account);
	}

	InitSavedAccounts(curl, sessionId, encryptPass, &accounts);

	if (!accounts.size())
	{
		Log(LogChannel::GENERAL, "No accounts started, adding a new one\n");

		CAccount account;

		if (account.Init(curl, sessionId, encryptPass))
			accounts.emplace_back(account);
	}

	const size_t accountCount = accounts.size();

	if (!accountCount)
	{
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		Pause();
		return 1;
	}

	memset(encryptPass, 0, sizeof(encryptPass));

#ifdef _WIN32
	// prevent sleep
	SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
#endif // _WIN32

	std::vector<std::string>* sentOfferIds = new std::vector<std::string>[accountCount];

	while (true)
	{
		for (size_t iterAcc = 0; iterAcc < accountCount; ++iterAcc)
		{
			if (!Steam::Auth::RefreshOAuthSession(curl, accounts[iterAcc].oauthToken, accounts[iterAcc].loginToken) ||
				!Steam::SetLoginCookie(curl, accounts[iterAcc].steamId64, accounts[iterAcc].loginToken))
			{
				Log(LogChannel::GENERAL, "[%s] Steam session refresh failed (OAuth might have expired), restart the program if error continues\n", accounts[iterAcc].name);
				continue;
			}

			if (!CancelExpiredOffers(curl, sessionId, accounts[iterAcc].steamApiKey, &sentOfferIds[iterAcc]))
				Log(LogChannel::GENERAL, "[%s] Cancelling expired offers failed, manually cancel offers older than 15 mins if error continues\n", accounts[iterAcc].name);

			Market::Ping(curl, accounts[iterAcc].marketApiKey);

			rapidjson::SizeType itemCounts[(int)Market::Market::COUNT] = { 0 };

			for (int iterMarket = 0; iterMarket < (int)Market::Market::COUNT; ++iterMarket)
			{
				rapidjson::Document parsedItems;
				if (!Market::GetItems(curl, accounts[iterAcc].marketApiKey, iterMarket, &parsedItems))
				{
					Log(LogChannel::GENERAL, "[%s] [%s] Getting items status failed\n", accounts[iterAcc].name, Market::marketNames[iterMarket]);
					continue;
				}

				const rapidjson::Value& items = parsedItems["items"];
				const rapidjson::SizeType itemCount = (items.IsNull() ? 0 : items.Size());

				itemCounts[iterMarket] = itemCount;

				int marketStatus = 0;

				for (rapidjson::SizeType iterItem = 0; iterItem < itemCount; ++iterItem)
				{
					const rapidjson::Value& item = items[iterItem];

					// status is a character, convert it to int
					const int itemStatus = (item["status"].GetString()[0] - '0');

					if (itemStatus == (int)Market::ItemStatus::GIVE)
					{
						const char* itemName = item["market_hash_name"].GetString();
						Log(LogChannel::GENERAL, "[%s] [%s] Sold \"%s\"\n", accounts[iterAcc].name, Market::marketNames[iterMarket], itemName);
						marketStatus |= Market::Status::SOLD;
					}
					else if (itemStatus == (int)Market::ItemStatus::TAKE)
					{
						const char* itemName = item["market_hash_name"].GetString();
						Log(LogChannel::GENERAL, "[%s] [%s] Bought \"%s\"\n", accounts[iterAcc].name, Market::marketNames[iterMarket], itemName);
						marketStatus |= Market::Status::BOUGHT;
					}
				}

				if (!marketStatus)
					continue;
#ifdef _WIN32
				FlashCurrentWindow();
#endif // _WIN32
				// have to use nested ifs so we can recieve items if sending failed
				if (marketStatus & Market::Status::SOLD)
				{
					rapidjson::Document parsedGive;
					if (Market::RequestGive(curl, accounts[iterAcc].marketApiKey, iterMarket, &parsedGive))
					{
						const size_t prevSentOfferIdsSize = sentOfferIds[iterAcc].size();

						if (Market::isMarketP2P[iterMarket])
						{
							const rapidjson::Value& offers = parsedGive["offers"];
							const rapidjson::SizeType offerCount = offers.Size();

							for (rapidjson::SizeType iterOffer = 0; iterOffer < offerCount; ++iterOffer)
							{
								const rapidjson::Value& offer = offers[iterOffer];

								rapidjson::StringBuffer itemsStrBuf;
								rapidjson::Writer<rapidjson::StringBuffer> itemsWriter(itemsStrBuf);

								if (!offer["items"].Accept(itemsWriter))
									Log(LogChannel::GENERAL, "[%s] [%s] JSON offer items to string failed\n", accounts[iterAcc].name, Market::marketNames[iterMarket]);
								else
								{
									const rapidjson::Value& offerPartner = offer["partner"];

									// partner returned by market can be a string or a number
									const uint32_t nPartner32 = (offerPartner.IsUint() ? offerPartner.GetUint() : atol(offerPartner.GetString()));

									char offerId[Steam::Trade::offerIdBufSz];

									if (Steam::Trade::Send(curl,
										sessionId,
										nPartner32,
										offer["token"].GetString(),
										offer["tradeoffermessage"].GetString(),
										itemsStrBuf.GetString(),
										offerId))
									{
										sentOfferIds[iterAcc].emplace_back(offerId);
									}
								}
							}
						}
						else
						{
							const char* offerId = parsedGive["trade"].GetString();
							const char* partnerUrl = parsedGive["profile"].GetString();

							char partner64[UINT64_MAX_STR_SIZE];
							stpncpy(partner64, partnerUrl + 36, strlen(partnerUrl + 36) - 1)[0] = '\0';

							if (Steam::Trade::Accept(curl, sessionId, offerId, partner64))
								sentOfferIds[iterAcc].emplace_back(offerId);
						}

						const size_t newOfferCount = sentOfferIds[iterAcc].size() - prevSentOfferIdsSize;

						if (newOfferCount)
						{
							Steam::Guard::AcceptConfirmations(curl, accounts[iterAcc].steamId64, accounts[iterAcc].identitySecret, accounts[iterAcc].deviceId, 
								sentOfferIds[iterAcc].data() + prevSentOfferIdsSize, newOfferCount);
						}
					}
				}

				if (marketStatus & Market::Status::BOUGHT)
				{
					char offerId[Steam::Trade::offerIdBufSz];
					char partnerId64[UINT64_MAX_STR_SIZE];

					if (Market::RequestTake(curl, accounts[iterAcc].marketApiKey, iterMarket, offerId, partnerId64))
						Steam::Trade::Accept(curl, sessionId, offerId, partnerId64);
				}
			}

			Log(LogChannel::GENERAL, "[%s] Listings: ", accounts[iterAcc].name);

			for (int iterMarket = 0; iterMarket < (int)Market::Market::COUNT; ++iterMarket)
			{
				printf("%s: %u", Market::marketNames[iterMarket], itemCounts[iterMarket]);

				if (iterMarket < ((int)Market::Market::COUNT - 1))
					putsnn(" | ");
			}

			putchar('\n');
		}

		if (1 < accountCount)
			putchar('\n');

		std::this_thread::sleep_for(Market::pingInterval);
	}

	delete[] sentOfferIds;

	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return 0;
}