#pragma once

#include "PKCS7.h"

namespace Config
{
	char username[32] = { 0 };
	char password[32] = { 0 };
	char shared[29] = { 0 };
	char identity[29] = { 0 };
	char deviceid[45] = { 0 };
	//char steamapikey[33] = { 0 };
	char marketApiKey[32] = { 0 };
	char steamid64[STEAMID64_SIZE] = { 0 };

	char sessionid[25] = { 0 };

	char* const fields[] = 
	{
		username,
		password,
		shared,
		identity,
		deviceid,
		/*steamapikey,*/
		marketApiKey,
		steamid64
	};

	constexpr size_t fieldSizes[] = 
	{
		sizeof(username),
		sizeof(password),
		sizeof(shared),
		sizeof(identity),
		sizeof(deviceid),
		/*sizeof(steamapikey),*/
		sizeof(marketApiKey),
		sizeof(steamid64)
	};

	constexpr size_t fieldsSize = []() constexpr
	{
		size_t result = 0;
		for (size_t i = 0; i < std::size(fields); ++i)
			result += fieldSizes[i];
		return result;
	}();

	const size_t fieldsPaddedSize = fieldsSize + _PKCS7_GetPadSize(fieldsSize, AES_BLOCK_SIZE);

	const char filename[] = "config";
	const char delimiter = '\n';

	const size_t keySize = AES_256_KEY_SIZE;
	const size_t saltSize = 16;
	const size_t ivSize = GCM_NONCE_MID_SZ;
	const size_t authTagSize = AES_BLOCK_SIZE;
	const int iterationCount = 50000;

	void Enter()
	{
		if (!username[0])
		{
			Log("Enter username: ");
			std::cin >> username;
		}
		
		if (!password[0])
		{
			Log("Enter password: ");
			SetStdinEcho(false);
			std::cin >> password;
			SetStdinEcho(true);
			std::cout << '\n';
		}

		if (!shared[0])
		{
			Log("Enter shared secret: ");
			std::cin >> shared;
		}

		if (!identity[0])
		{
			Log("Enter identity secret: ");
			std::cin >> identity;
		}

		if (!deviceid[0])
		{
			Log("Enter device id: ");
			std::cin >> deviceid;
		}

		if (!marketApiKey[0])
		{
			Log("Enter market api-key: ");
			std::cin >> marketApiKey;
		}
	}

	void ZeroLoginDetails()
	{
		for (unsigned int i = 0; i < 3; ++i)
			memset(fields[i], 0, fieldSizes[i]);
	}

	bool Write()
	{
		Log("Enter config encryption password: ");
		char encryptKey[keySize];
		SetStdinEcho(false);
		std::cin >> encryptKey;
		SetStdinEcho(true);
		std::cout << '\n';

		byte summary[fieldsPaddedSize];
		byte* start = summary;

		for (size_t i = 0; i < std::size(fields); ++i)
		{
			// append field and delimiter, strcat method is 4 lines too but slower
			size_t len = strlen(fields[i]);
			memcpy(start, fields[i], len);
			start[len] = delimiter;
			start += len + 1;
		}

		Log("Encrypting config...");

		byte salt[saltSize];
		byte iv[ivSize];

		WC_RNG rng;
		if (wc_InitRng(&rng))
		{
			std::cout << "rng init failed\n";
			return false;
		}

		bool rngFailed = (wc_RNG_GenerateBlock(&rng, salt, sizeof(salt)) || wc_RNG_GenerateBlock(&rng, iv, sizeof(iv)));
		wc_FreeRng(&rng);

		if (rngFailed)
		{
			std::cout << "rng failed\n";
			return false;
		}

		const size_t summaryLen = (start - summary);
		const size_t summaryPaddedLen = _PKCS7_PadData(summary, summaryLen, sizeof(summary), AES_BLOCK_SIZE);

		byte keyStretched[keySize];
		Aes aes;
		byte* encrypted = new byte[summaryPaddedLen];
		byte authTag[authTagSize];

		if (wc_PBKDF2(keyStretched, (const byte*)encryptKey, strlen(encryptKey), salt, sizeof(salt), iterationCount, keySize, WC_SHA256) ||
			wc_AesGcmSetKey(&aes, keyStretched, keySize) ||
			wc_AesGcmEncrypt(&aes, encrypted, summary, summaryPaddedLen, iv, sizeof(iv), authTag, sizeof(authTag), nullptr, 0))
		{
			delete[] encrypted;
			std::cout << "fail\n";
			return false;
		}

		std::cout << "ok\n";

		memset(encryptKey, 0, sizeof(encryptKey));
		memset(keyStretched, 0, sizeof(keyStretched));
		memset(summary, 0, sizeof(summary));

		Log("Writing config...");

		FILE* file = fopen(filename, "wb");
		if (!file)
		{
			delete[] encrypted;
			std::cout << "fail\n";
			return false;
		}

		fwrite(salt, sizeof(byte), saltSize, file);
		fwrite(iv, sizeof(byte), ivSize, file);
		fwrite(authTag, sizeof(byte), authTagSize, file);
		fwrite(encrypted, sizeof(byte), summaryPaddedLen, file);

		fclose(file);

		delete[] encrypted;

		std::cout << "ok\n";
		return true;
	}

	bool Decrypt(const byte* in, size_t inSz, byte* out)
	{
		const byte* salt = in;
		const byte* iv = salt + saltSize;
		const byte* authTag = iv + ivSize;
		const byte* encSummary = authTag + authTagSize;
		const size_t encSummarySize = inSz - (encSummary - in);

		SetStdinEcho(false);
		while (true)
		{
			Log("Enter config decryption password: ");
			char decryptKey[keySize];
			std::cin >> decryptKey;
			std::cout << '\n';

			Log("Decrypting config...");

			Aes aes;

			byte keyStretched[keySize];
			if (wc_PBKDF2(keyStretched, (const byte*)decryptKey, strlen(decryptKey), salt, saltSize, iterationCount, keySize, WC_SHA256) ||
				wc_AesGcmSetKey(&aes, keyStretched, keySize))
			{
				std::cout << "fail\n";
				return false;
			}

			if (!wc_AesGcmDecrypt(&aes, out, encSummary, encSummarySize, iv, ivSize, authTag, authTagSize, nullptr, 0))
			{
				memset(decryptKey, 0, sizeof(decryptKey));
				memset(keyStretched, 0, sizeof(keyStretched));
				break;
			}
			else
				std::cout << "wrong password\n";
		}
		SetStdinEcho(true);

		std::cout << "ok\n";
		return true;
	}

	bool Read()
	{
		Log("Reading config...");

		FILE* file = fopen(filename, "rb");
		if (!file)
		{
			std::cout << "not found or opening failed\n";
			return false;
		}
		
		long fsize;

		if (fseek(file, 0, SEEK_END) ||
			((fsize = ftell(file)) == -1L) ||
			fseek(file, 0, SEEK_SET))
		{
			fclose(file);
			std::cout << "failed to get file size\n";
			return false;
		}

		byte* contents = new byte[fsize];
		bool readFailed = (fread(contents, sizeof(byte), fsize, file) != fsize);
		fclose(file);

		if (readFailed)
		{
			delete[] contents;
			std::cout << "fail\n";
			return false;
		}

		std::cout << "ok\n";

		byte summary[fieldsPaddedSize];
		bool decryptFailed = (!Decrypt(contents, fsize, summary));
		delete[] contents;

		if (decryptFailed)
			return false;

		const byte* start = summary;

		for (size_t i = 0; i < std::size(fields); ++i)
		{
			const byte* end = (byte*)memchr(start, (int)delimiter, fieldSizes[i]);
			memcpy(fields[i], start, (end - start));
			fields[i][fieldSizes[i] - 1] = '\0';
			start = end + 1;
		}

		memset(summary, 0, sizeof(summary));
		
		return true;
	}

	bool Import()
	{
		Log("Looking for a SDA maFile...");

		std::filesystem::path path;
		std::filesystem::path currentDir(std::filesystem::current_path());

		for (const auto& entry : std::filesystem::directory_iterator(currentDir))
       	{	
			if (!entry.path().extension().compare(".maFile"))
			{
				path = entry.path();
				break;
			}
		}

		if (path.empty())
		{
			std::cout << "not found\n";
			return false;
		}

		std::cout << "found\n";
		Log("Importing maFile...");

#ifdef _WIN32
		FILE* file = _wfopen(path.c_str(), L"rb");
#else
		FILE* file = fopen(path.c_str(), "rb");
#endif // _WIN32
		if (!file)
		{
			std::cout << "opening failed\n";
			return false;
		}
		
		long fsize;

		if (fseek(file, 0, SEEK_END) ||
			((fsize = ftell(file)) == -1L) ||
			fseek(file, 0, SEEK_SET))
		{
			fclose(file);
			std::cout << "failed to get file size\n";
			return false;
		}

		byte* contents = new byte[fsize + 1] { 0 };
		bool readFailed = (fread(contents, sizeof(byte), fsize, file) != fsize);
		fclose(file);

		if (readFailed)
		{
			delete[] contents;
			std::cout << "reading failed\n";
			return false;
		}

		if (contents[0] != '{')
		{
			delete[] contents;
			std::cout << "disable encryption in SDA before importing\n";
			return false;
		}

		rapidjson::Document doc;
		doc.Parse((const char*)contents);

		delete[] contents;

		strcpy_s(username, sizeof(username), doc["account_name"].GetString());
		strcpy_s(shared, sizeof(shared), doc["shared_secret"].GetString());
		strcpy_s(identity, sizeof(identity), doc["identity_secret"].GetString());
		strcpy_s(deviceid, sizeof(deviceid), doc["device_id"].GetString());
		sprintf_s(steamid64, sizeof(steamid64), "%llu", doc["Session"]["SteamID"].GetUint64());

#ifndef _DEBUG
		std::filesystem::remove(path);
#endif // !_DEBUG

		std::cout << "ok\n";
		return true;
	}
}