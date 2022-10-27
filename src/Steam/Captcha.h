#pragma once

namespace Steam
{
	namespace Captcha
	{
		const size_t gidBufSz = UINT64_MAX_STR_SIZE;
		const size_t answerBufSz = 6 + 1;

		// out buffer size must be at least gidBufSz
		bool GetGID(CURL* curl, char* out)
		{
			Log(LogChannel::STEAM, "Refreshing captcha...");

			Curl::CResponse response;
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
			curl_easy_setopt(curl, CURLOPT_URL, "https://steamcommunity.com/login/refreshcaptcha/");
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

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

			const rapidjson::Value& gid = parsed["gid"];

			if (gid.IsString())
				strcpy(out, gid.GetString());
			else
				strcpy(out, "-1");

			printf("ok\n");
			return true;
		}

		// out buffer size must be at least answerBufSz
		bool GetAnswer(CURL* curl, const char* gid, char* out)
		{
			const char urlPart[] = "https://steamcommunity.com/login/rendercaptcha/?gid=";

			const size_t urlSz = sizeof(urlPart) - 1 + gidBufSz - 1 + 1;
			char url[urlSz];

			char* urlEnd = url;
			urlEnd = stpcpy(urlEnd, urlPart);
			strcpy(urlEnd, gid);

#ifdef _WIN32
			Log(LogChannel::STEAM, "Downloading captcha image...");

			const char filename[] = "captcha.png";

			FILE* file = fopen(filename, "wb");
			if (!file)
			{
				putsnn("file creation failed\n");
				return false;
			}

			curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL); // set write callback to default file write
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

			const CURLcode respCode = curl_easy_perform(curl);

			fclose(file);

			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Curl::CResponse::WriteCallback);

			if (respCode != CURLE_OK)
			{
				Curl::PrintError(curl, respCode);
				return false;
			}

			putsnn("ok\n");

			Log(LogChannel::STEAM, "Opening captcha image...");

			// not sure if needed MSDC says call it before calling ShellExecute
			const HRESULT coInitRes = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			const bool coInitSucceeded = ((coInitRes == S_OK) || (coInitRes == S_FALSE));

			if (32 >= (INT_PTR)ShellExecuteA(NULL, NULL, filename, NULL, NULL, SW_SHOWNORMAL))
			{
				if (coInitSucceeded)
					CoUninitialize();

				putsnn("fail\n");
				return false;
			}

			if (coInitSucceeded)
				CoUninitialize();

			putsnn("ok\n");
#else

			Log(LogChannel::STEAM, "Captcha URL: %s\n", url);
#endif // _WIN32

			if (!GetUserInputString("Enter captcha answer", out, answerBufSz, answerBufSz - 1))
				return false;

			return true;
		}
	}
}