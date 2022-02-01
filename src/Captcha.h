#pragma once

#define CAPTCHA_GID_SIZE 20
#define CAPTCHA_ANSWER_SIZE 8

namespace Captcha
{
	bool Refresh(CURL* curl, char* outGid)
	{
		Log("Refreshing captcha...");

		CURLdata response;
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login/refreshcaptcha/");

		if (curl_easy_perform(curl) != CURLE_OK)
		{
			printf("request failed\n");
			return false;
		}

		rapidjson::Document parsed;
		parsed.Parse(response.data);

		if (parsed["gid"].IsInt())
			strcpy_s(outGid, CAPTCHA_GID_SIZE, "-1");
		else
			strcpy_s(outGid, CAPTCHA_GID_SIZE, parsed["gid"].GetString());

		printf("ok\n");
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

		const char* dir = GetExecDir();
		if (!dir)
		{
			printf("failed to get executable's directory\n");
			return false;
		}

		char path[PATH_MAX];
		strcpy_s(path, sizeof(path), dir);
		strcat_s(path, sizeof(path), "captcha.png");

		FILE* file = fopen(path, "wb");
		if (!file)
		{
			printf("file creation failed\n");
			return false;
		}

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		CURLcode res = curl_easy_perform(curl);
		fclose(file);

		if (res != CURLE_OK)
		{
			printf("request failed\n");
			return false;
		}

		system("start \"\" \"captcha.png\"");
#else
		Log("Captcha link: %s\n", url);
#endif // _WIN32

		printf("ok\n");

		Log("Enter captcha text: ");
		while (!GetUserInput(outAnswer, CAPTCHA_ANSWER_SIZE));

		return true;
	}
}