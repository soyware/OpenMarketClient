#pragma once

struct CURLdata
{
	char*	data;
	size_t	size;

	CURLdata()
	{
		data = (char*)malloc(1);
		size = 0;
	}

	~CURLdata()
	{
		free(data);
	}

	CURLdata(const CURLdata&) = delete;
	CURLdata(const CURLdata&&) = delete;
};

size_t curl_write_function(void* contents, size_t size, size_t nmemb, void* userp)
{
	size_t realSize = size * nmemb;
	CURLdata* dataStruct = (CURLdata*)userp;

	char* newMem = (char*)realloc(dataStruct->data, dataStruct->size + realSize + 1);
	if (!newMem)
	{
#ifdef _DEBUG
		Log("Warning: curl write callback realloc failed\n");
#endif // _DEBUG
		return 0;
	}

	dataStruct->data = newMem;
	memcpy(&(dataStruct->data[dataStruct->size]), contents, realSize);
	dataStruct->size += realSize;
	dataStruct->data[dataStruct->size] = '\0';

	return realSize;
}

namespace Curl
{
	bool DownloadCACert(CURL* curl, FILE* file)
	{
		Log("Downloading CA certificate...");

		curl_easy_setopt(curl, CURLOPT_URL, "https://curl.haxx.se/ca/cacert.pem");
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

		CURLcode res = curl_easy_perform(curl);

		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);

		if (res != CURLE_OK)
		{
			printf("fail\n");
			return false;
		}

		printf("ok\n");
		return true;
	}

	bool SetCACert(CURL* curl)
	{
		const char* dir = GetExecDir();
		if (!dir)
			return false;

		char path[PATH_MAX];
		strcpy_s(path, sizeof(path), dir);
		strcat_s(path, sizeof(path), "cacert.pem");

		FILE* file;
		if ((file = fopen(path, "rb")) ||
			((file = fopen(path, "wb")) && DownloadCACert(curl, file)))
		{
			fclose(file);
			curl_easy_setopt(curl, CURLOPT_CAINFO, path);
			return true;
		}
		return false;
	}

	CURL* Init()
	{
		CURL* curl = nullptr;
		if (curl_global_init(CURL_GLOBAL_ALL) || !(curl = curl_easy_init()))
		{
			curl_easy_cleanup(curl);
			curl_global_cleanup();
			return nullptr;
		}

		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		return curl;
	}
}