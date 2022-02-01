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

	dataStruct->data = (char*)realloc(dataStruct->data, dataStruct->size + realSize + 1);
	memcpy(&(dataStruct->data[dataStruct->size]), contents, realSize);
	dataStruct->size += realSize;
	dataStruct->data[dataStruct->size] = '\0';

	return realSize;
}

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
