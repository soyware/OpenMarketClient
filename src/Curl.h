#pragma once

namespace Curl
{
	class CResponse
	{
	public:
		char*	data = nullptr;
		size_t	size = 0;

		CResponse()
		{

		}

		~CResponse()
		{
			free(data);
		}

		CResponse(const CResponse&) = delete;
		CResponse(const CResponse&&) = delete;

		void Empty()
		{
			free(data);
			data = nullptr;
			size = 0;
		}

		static size_t WriteCallback(void* data, size_t size, size_t count, CResponse* out)
		{
			const size_t totalSize = count * size;
			char* newMem = (char*)realloc(out->data, out->size + totalSize + 1);
			if (!newMem)
			{
#ifdef _DEBUG
				putsnn("libcurl write callback realloc failed\n");
#endif // _DEBUG
				return 0;
			}

			out->data = newMem;
			memcpy(out->data + out->size, data, totalSize);
			out->size += totalSize;
			out->data[out->size] = '\0';

			return totalSize;
		}
	};

	void PrintError(CURL* curl, CURLcode respCode)
	{
		if (respCode == CURLE_HTTP_RETURNED_ERROR)
		{
			long httpCode;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
			printf("request failed (HTTP response code %ld)\n", httpCode);
		}
		else
			printf("request failed (libcurl code %d)\n", respCode);
	}

	bool DownloadCACert(CURL* curl, const char* path)
	{
		Log(LogChannel::LIBCURL, "Downloading CA certificate...");

		FILE* file = fopen(path, "wb");
		if (!file)
		{
			putsnn("writing failed\n");
			return false;
		}

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(curl, CURLOPT_URL, "https://curl.haxx.se/ca/cacert.pem");
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

		const CURLcode respCode = curl_easy_perform(curl);

		fclose(file);

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);

		if (respCode != CURLE_OK)
		{
			remove(path);
			PrintError(curl, respCode);
			return false;
		}

		putsnn("ok\n");
		return true;
	}

	bool SetCACert(CURL* curl, const char* path)
	{
		bool certOk = false;

		FILE* file = fopen(path, "rb");
		// check if the certificate file exists and isn't too old
		if (file)
		{
			struct _stat fileStat;

			if (!_fstat(_fileno(file), &fileStat)) {
				const double modTimeDiff = difftime(time(nullptr), fileStat.st_mtime);

				// (90 days)
				certOk = modTimeDiff < (60.0 * 60.0 * 24.0 * 90.0);
			}

			fclose(file);
		}

		if (!certOk && !DownloadCACert(curl, path))
			return false;

		if (curl_easy_setopt(curl, CURLOPT_CAINFO, path) != CURLE_OK)
		{
			Log(LogChannel::LIBCURL, "Setting CA certificate failed\n");
			return false;
		}

		return true;
	}

	CURL* Init(const char* proxy)
	{
		if (curl_global_init(CURL_GLOBAL_ALL))
		{
			Log(LogChannel::LIBCURL, "Global init failed\n");
			return nullptr;
		}

		CURL* curl = curl_easy_init();
		if (!curl)
		{
			curl_global_cleanup();
			Log(LogChannel::LIBCURL, "Easy session init failed\n");
			return nullptr;
		}

		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);

		if (proxy && proxy[0])
		{
			Log(LogChannel::LIBCURL, "Setting a proxy...");

			if (curl_easy_setopt(curl, CURLOPT_PROXY, proxy) != CURLE_OK)
			{
				curl_easy_cleanup(curl);
				curl_global_cleanup();
				putsnn("fail\n");
				return nullptr;
			}

			putsnn("ok\n");
		}

		// let libcurl use system's default on linux
#ifdef _WIN32
		if (!SetCACert(curl, "cacert.pem"))
		{
			curl_easy_cleanup(curl);
			curl_global_cleanup();
			return nullptr;
		}
#endif // _WIN32

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CResponse::WriteCallback);

		return curl;
	}
}