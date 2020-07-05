#include "stdafx.h"
#include "Helpers.h"
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
	setlocale(LC_ALL, "");
	Log("MarketsBot v0.2\n");

	CURL* curl = nullptr;
	if (curl_global_init(CURL_GLOBAL_ALL) || !(curl = curl_easy_init()))
	{
		Log("Libcurl init failed\n");
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		Pause();
		return 1;
	}

	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

	bool readCfg = Config::Read();

	if (!readCfg)
	{
		Config::Import();
		Config::Enter();
	}
	
	if (!SetCACert(curl) || !Guard::Sync(curl) || !Login::DoLogin(curl) || !Login::GetSteamInfo(curl))
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
	
	HWND hWnd = GetConsoleWindow();
#endif // _WIN32

	Market::Init();

	while (true)
	{
		Market::Ping(curl, Config::marketApiKey);

		for (int market = 0; market < MARKET_COUNT; ++market)
		{
			int status = Market::CheckItems(curl, market);

			if (status == ITEM_STATUS_GIVE)
			{
#ifdef _WIN32
				FlashWindow(hWnd, TRUE);
#endif // _WIN32
				rapidjson::Document parsed;
				if (!Market::RequestGiveDetails(curl, &parsed))
					continue;

				const rapidjson::Value& offers = parsed["offers"];
				rapidjson::SizeType offerCount = offers.Size();

				const auto offerIds = new char[offerCount][OFFER_ID_SIZE];

				rapidjson::SizeType sentCount = 0;

				for (rapidjson::SizeType i = 0; i < offerCount; ++i)
				{
					const rapidjson::Value& offer = offers[i];

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
				
				if (0 < sentCount)
					Guard::AcceptConfirmations(curl, offerIds, sentCount);

				delete[] offerIds;
			}
			else if (status == ITEM_STATUS_TAKE)
			{
#ifdef _WIN32
				FlashWindow(hWnd, TRUE);
#endif // _WIN32
				char offerId[OFFER_ID_SIZE];
				char partnerId[STEAMID64_SIZE];

				if (Market::RequestTakeDetails(curl, market, offerId, partnerId))
					Offer::Accept(curl, offerId, partnerId);
			}
		}

		std::this_thread::sleep_for(1min);
	}

	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return 0;
}