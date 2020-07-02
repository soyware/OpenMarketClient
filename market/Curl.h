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
		std::cerr << "realloc returned NULL\n";
		return 0;
	}

	dataStruct->data = newMem;
	memcpy(&(dataStruct->data[dataStruct->size]), contents, realSize);
	dataStruct->size += realSize;
	dataStruct->data[dataStruct->size] = '\0';

	return realSize;
}

bool DownloadCACert(CURL* curl, FILE* file)
{
	Log("Downloading CA certificate...");

	curl_easy_setopt(curl, CURLOPT_URL, "https://curl.haxx.se/ca/cacert.pem");
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)file);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

	CURLcode res = curl_easy_perform(curl);
	fclose(file);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);

	if (res != CURLE_OK)
	{
		std::cout << "fail\n";
		return false;
	}

	std::cout << "ok\n";
	return true;
}

bool SetCACert(CURL* curl)
{
	FILE* file = fopen("cacert.pem", "wbx");
	if (!file || DownloadCACert(curl, file))
	{
		curl_easy_setopt(curl, CURLOPT_CAINFO, "cacert.pem");
		return true;
	}

	return false;
}