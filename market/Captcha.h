#pragma once

#define CAPTCHA_GID_SIZE 20
#define CAPTCHA_ANSWER_SIZE 8

namespace Captcha
{
	bool Refresh(CURL* curl, char* outGid)
	{
		Log("Refreshing captcha...");

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&response);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login/refreshcaptcha/");

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			std::cout << "request failed\n";
			return false;
		}

		rapidjson::Document parsed;
		parsed.Parse(response.data);

		if (parsed["gid"].IsInt())
			strcpy_s(outGid, CAPTCHA_GID_SIZE, "-1");
		else
			strcpy_s(outGid, CAPTCHA_GID_SIZE, parsed["gid"].GetString());

		std::cout << "ok\n";
		return true;
	}

	bool Handle(CURL* curl, const char* gid, char* outAnswer)
	{
		const size_t urlSz = sizeof("https://steamcommunity.com/login/rendercaptcha/?gid=") - 1 + CAPTCHA_GID_SIZE;
		char url[urlSz] = "https://steamcommunity.com/login/rendercaptcha/?gid=";

		strcat_s(url, sizeof(url), gid);

#ifdef _WIN32
		Log("Getting captcha...");

		curl_easy_setopt(curl, CURLOPT_URL, url);

		FILE* file = fopen("captcha.png", "wb");
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

		std::cout << "ok\n";
#else
		Log("Captcha link: ", url, '\n');
#endif // _WIN32

		Log("Enter the answer: ");
		std::cin >> outAnswer;

		return true;
	}
}