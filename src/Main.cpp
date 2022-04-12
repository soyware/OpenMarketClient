#include "stdafx.h"
#include "Misc.h"
#include "Config.h"
#include "Curl.h"
#include "Guard.h"
#include "Login.h"
#include "Offer.h"
#include "Market.h"

// TODO:
// dont save password, save cookies? + check if marketapikey is valid 
// release steam guard file at separate repo?

int main()
{
	// LC_ALL breaks SetConsoleOutputCP
	setlocale(LC_TIME, "");
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32

	Log("MarketsBot v0.2.2\n");

	CURL* curl = Curl::Init();
	if (!curl)
	{
		Log("Libcurl init failed\n");
		Pause();
		return 1;
	}

	bool readCfg = Config::Read();

	if (!readCfg)
	{
		const char* dir = GetExecDir();
		if (dir)
		{
			char maFilePath[MAX_PATH];
			bool maFileFound = FindFileByExtension(dir, ".maFile", maFilePath, sizeof(maFilePath));

			if (maFileFound)
				Config::ImportMaFile(maFilePath);
		}

		Config::Enter();
	}
	
	if ( 
#ifdef _WIN32
		!Curl::SetCACert(curl) || // let libcurl use system's default ca-bundle on linux
#endif // _WIN32
		!Guard::Sync(curl) || !Login::DoLogin(curl) || !Login::GetSteamInfo(curl))
	{
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		Pause();
		return 2;
	}

	if (!readCfg)
		Config::Write();

	Config::ZeroLoginDetails();

#ifdef _WIN32
	// prevent sleep
	SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
#endif // _WIN32

	Market::Init();

	while (true)
	{
		Market::Ping(curl, Config::marketApiKey);

		for (int marketIter = 0; marketIter < MARKET_COUNT; ++marketIter)
		{
			int status = Market::CheckItems(curl, marketIter);

			if (status == ITEM_STATUS_GIVE)
			{
#ifdef _WIN32
				FlashCurrentWindow();
#endif // _WIN32
				rapidjson::Document parsed;
				if (!Market::RequestGiveDetails(curl, &parsed))
					continue;

				const rapidjson::Value& offers = parsed["offers"];
				rapidjson::SizeType offerCount = offers.Size();
				if (offerCount < 1)
					continue;

				char (*const offerIds)[OFFER_ID_SIZE] = (char(*)[OFFER_ID_SIZE])malloc(offerCount * OFFER_ID_SIZE);
				if (!offerIds)
				{
#ifdef _DEBUG
					Log("Warning: offer ids buffer allocation failed\n");
#endif // _DEBUG
					continue;
				}

				rapidjson::SizeType sentCount = 0;

				for (rapidjson::SizeType offerIter = 0; offerIter < offerCount; ++offerIter)
				{
					const rapidjson::Value& offer = offers[offerIter];

					rapidjson::StringBuffer buffer;
					rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
					offer["items"].Accept(writer);

					// partner returned by market is sometimes a string, sometimes an int
					const char* pPartner;
					
					char partner32[STEAMID32_SIZE];
					if (offer["partner"].IsInt())
					{
						sprintf_s(partner32, sizeof(partner32), "%d", offer["partner"].GetInt());
						pPartner = partner32;
					}
					else
						pPartner = offer["partner"].GetString();

					if (Offer::Send(curl,
						pPartner,
						offer["token"].GetString(),
						offer["tradeoffermessage"].GetString(),
						buffer.GetString(),
						offerIds[sentCount]))
					{
						++sentCount;
					}

					std::this_thread::sleep_for(10s);
				}
				
				if (sentCount)
					Guard::AcceptConfirmations(curl, offerIds, sentCount);

				free(offerIds);
			}
			else if (status == ITEM_STATUS_TAKE)
			{
#ifdef _WIN32
				FlashCurrentWindow();
#endif // _WIN32
				char offerId[OFFER_ID_SIZE];
				char partnerId[STEAMID64_SIZE];

				if (Market::RequestTakeDetails(curl, marketIter, offerId, partnerId))
					Offer::Accept(curl, offerId, partnerId);
			}
		}

		std::this_thread::sleep_for(1min);
	}

	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return 0;
}