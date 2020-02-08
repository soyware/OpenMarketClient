#include "stdafx.h"
#include "Helpers.h"
#include "Config.h"
#include "Curl.h"

char g_sessionID[25];

#include "Guard.h"
#include "Login.h"
#include "Offer.h"
#include "Market.h"

// TODO:
// import sda maFile

int main()
{
	setlocale(LC_ALL, "");
	Log("MarketsBot v0.1.2\n");

	curl_global_init(CURL_GLOBAL_ALL);
	CURL* curl = curl_easy_init();
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

		bool readcfg = Config::Read();

		if (!readcfg)
			Config::Enter();

		if (!SetCACert(curl) || !Guard::Sync(curl) || !Login::DoLogin(curl) || !Login::GetSessionId(curl))
		{
			std::cout << "Press any key to exit...";
			_getch();
			return 1;
		}

		if (!readcfg)
		{
			Login::GetSteamIdApiKey(curl);
			Config::Write();
		}

		while (true)
		{
			Market::Ping(curl, Config::marketapikey);

			for (int market = 0; market < MARKET_COUNT; ++market)
			{
				int status = Market::CheckItems(curl, market);

				if (market == MARKET_CSGO && status == ITEM_STATUS_GIVE)
				{
					rapidjson::Document doc;
					if (!Market::RequestDetails(curl, &doc))
						continue;

					const rapidjson::Value& offers = doc["offers"];
					rapidjson::SizeType offercount = offers.Size();

					char(* const tradeofferids)[OFFERID_LEN] = new char[offercount][OFFERID_LEN];

					for (rapidjson::SizeType i = 0; i < offercount; ++i)
					{
						const rapidjson::Value& offer = offers[i];

						rapidjson::StringBuffer buffer;
						rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
						offer["items"].Accept(writer);

						// partner returned by market is sometimes a string, sometimes an int
						char partner[11];
						if (offer["partner"].IsInt())
							_itoa_s(offer["partner"].GetInt(), partner, sizeof(partner), 10);

						while (!Offer::Send(curl, offer["partner"].IsInt() ? partner : offer["partner"].GetString(), offer["token"].GetString(),
							offer["tradeoffermessage"].GetString(), buffer.GetString(), tradeofferids[i]))
							std::this_thread::sleep_for(5s);

						std::this_thread::sleep_for(5s);
					}

					Guard::AcceptConfirmations(curl, tradeofferids, offercount);

					delete[] tradeofferids;
				}
				else if (status == ITEM_STATUS_GIVE || status == ITEM_STATUS_TAKE)
				{
					char offerId[OFFERID_LEN];
					char partnerId[STEAMID64_LEN];
					//char secretCode[MARKET_SECRET_LEN];
					if (Market::RequestOffer(curl, market, (status == ITEM_STATUS_TAKE), offerId, partnerId/*, secretCode*/)
						/*&& CheckOffer(curl, offerId, secretCode)*/ &&
						Offer::Accept(curl, offerId, partnerId))
						Guard::AcceptConfirmation(curl, offerId);
				}
			}

			std::this_thread::sleep_for(1min);
		}
		curl_easy_cleanup(curl);
	}
	curl_global_cleanup();
	return 0;
}