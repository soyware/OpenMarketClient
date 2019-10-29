#pragma once

#define CAPTCHA_GID_SIZE 20

namespace Captcha
{
	bool Refresh(CURL* curl, char* outGid)
	{
		Log("Refreshing captcha...");

		CURLdata data;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&data);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login/refreshcaptcha/");

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		rapidjson::Document doc;
		doc.Parse(data.data);

		if (doc["gid"].IsInt())
			strcpy_s(outGid, CAPTCHA_GID_SIZE, "-1");
		else
			strcpy_s(outGid, CAPTCHA_GID_SIZE, doc["gid"].GetString());

		std::cout << "ok\n";
		return true;
	}

	bool Handle(CURL* curl, const char* gid, char* outAnswer)
	{
		Log("Getting captcha...");

		char url[72] = "https://steamcommunity.com/login/rendercaptcha/?gid=";
		strcat_s(url, sizeof(url), gid);
		curl_easy_setopt(curl, CURLOPT_URL, url);

		FILE* file;
		fopen_s(&file, "captcha.png", "wb");
		if (!file)
		{
			std::cout << "file creation failed\n";
			return false;
		}

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)file);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		CURLcode res = curl_easy_perform(curl);
		fclose(file);

		if (res != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		system("start \"\" \"captcha.png\"");

		std::cout << "ok. Enter captcha: ";
		std::cin >> outAnswer;

		return true;
	}
}