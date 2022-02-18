#pragma once

#include "Crypto.h"

namespace Config
{
	char username[32] = "";
	char password[32] = "";
	char shared[29] = "";
	char identity[29] = "";
	char deviceID[45] = "";
	//char steamapikey[33] = "";
	char marketApiKey[32] = "";
	char steamID64[STEAMID64_SIZE] = "";

	char sessionID[25] = "";

	char* const fields[] = 
	{
		username,
		password,
		shared,
		identity,
		deviceID,
		/*steamapikey,*/
		marketApiKey,
		steamID64
	};

	const size_t fieldCount = (sizeof(fields) / sizeof(char*));

	constexpr size_t fieldSizes[fieldCount] =
	{
		sizeof(username),
		sizeof(password),
		sizeof(shared),
		sizeof(identity),
		sizeof(deviceID),
		/*sizeof(steamapikey),*/
		sizeof(marketApiKey),
		sizeof(steamID64)
	};

	constexpr size_t GetFieldsSize()
	{
		size_t result = 0;

		for (size_t i = 0; i < fieldCount; ++i)
			result += fieldSizes[i];

		return result;
	}

	const char		fieldDelimiter = '\n';
	const char		filename[] = "config";

	const size_t	encryptionPassSize = 64;

	const size_t	saltSize = 16; // salt shall be at least 128 bits
	const int		pbkdfIterationCount = 50000;
	const int		pbkdfHashAlgo = WC_SHA256;

	const size_t	keySize = AES_256_KEY_SIZE;
	const size_t	ivSize = GCM_NONCE_MID_SZ;
	const size_t	authTagSize = 16; // max allowed tag size is 128 bits

	void Enter()
	{
		if (!username[0])
		{
			Log("Enter Steam username: ");
			GetUserInput(username, sizeof(username));
		}
		
		if (!password[0])
		{
			Log("Enter Steam password: ");
			SetStdinEcho(false);
			GetUserInput(password, sizeof(password));
			SetStdinEcho(true);
			printf("\n");
		}

		if (!shared[0])
		{
			Log("Enter Steam Guard shared_secret: ");
			GetUserInput(shared, sizeof(shared));
		}

		if (!identity[0])
		{
			Log("Enter Steam Guard identity_secret: ");
			GetUserInput(identity, sizeof(identity));
		}

		if (!deviceID[0])
		{
			Log("Enter Steam Guard device_id: ");
			GetUserInput(deviceID, sizeof(deviceID));
		}

		if (!marketApiKey[0])
		{
			Log("Enter market's API-key: ");
			GetUserInput(marketApiKey, sizeof(marketApiKey));
		}
	}

	void ZeroLoginDetails()
	{
		for (UINT i = 0; i < 3; ++i)
			memset(fields[i], 0, fieldSizes[i]);
	}

	bool Write()
	{
		Log("Enter config encryption password: ");

		char encryptPass[encryptionPassSize];
		SetStdinEcho(false);
		GetUserInput(encryptPass, sizeof(encryptPass));
		SetStdinEcho(true);
		printf("\n");

		constexpr size_t fieldsSize = GetFieldsSize();

		char fieldsBuffer[fieldsSize] = "";

		for (size_t i = 0; i < fieldCount; ++i)
		{
			strcat_s(fieldsBuffer, sizeof(fieldsBuffer), fields[i]);

			const size_t fieldLen = strlen(fieldsBuffer);
			fieldsBuffer[fieldLen] = fieldDelimiter;
			fieldsBuffer[fieldLen + 1] = '\0';
		}

		const size_t fieldsLen = strlen(fieldsBuffer);
		const size_t encryptedLen = fieldsLen + Crypto::PKCS7_GetPadSize(fieldsLen, AES_BLOCK_SIZE);

		constexpr size_t encryptedBufSize = fieldsSize + Crypto::PKCS7_GetPadSize(fieldsSize, AES_BLOCK_SIZE);

		byte salt[saltSize];
		byte iv[ivSize];
		byte authTag[authTagSize];
		byte encrypted[encryptedBufSize];

		if (!Crypto::Encrypt((byte*)fieldsBuffer, strlen(fieldsBuffer), 
			encryptPass, keySize, pbkdfIterationCount, pbkdfHashAlgo,
			salt, saltSize,
			iv, ivSize, 
			authTag, authTagSize,
			encrypted))
		{
			return false;
		}

		memset(fieldsBuffer, 0, sizeof(fieldsBuffer));
		memset(encryptPass, 0, sizeof(encryptPass));

		Log("Writing config...");

		const char* dir = GetExecDir();
		if (!dir)
		{
			printf("failed to get executable's directory\n");
			return false;
		}

		char path[PATH_MAX];
		strcpy_s(path, sizeof(path), dir);
		strcat_s(path, sizeof(path), filename);

		FILE* file = fopen(path, "wb");
		if (!file)
		{
			printf("fail\n");
			return false;
		}

		fwrite(salt, sizeof(byte), saltSize, file);
		fwrite(iv, sizeof(byte), ivSize, file);
		fwrite(authTag, sizeof(byte), authTagSize, file);
		fwrite(encrypted, sizeof(byte), encryptedLen, file);

		fclose(file);

		printf("ok\n");
		return true;
	}

	bool Read()
	{
		Log("Reading config...");

		const char* dir = GetExecDir();
		if (!dir)
		{
			printf("failed to get executable's directory\n");
			return false;
		}

		char path[PATH_MAX];
		strcpy_s(path, sizeof(path), dir);
		strcat_s(path, sizeof(path), filename);

		BYTE* fileContents = nullptr;
		long fileSize = 0;

		if (!ReadFile(path, &fileContents, &fileSize))
		{
			printf("failed to read or not found\n");
			return false;
		}

		printf("ok\n");

		const size_t plaintextDataPaddedSize = fileSize - saltSize - ivSize - authTagSize;
		byte* plaintextData = (byte*)malloc(plaintextDataPaddedSize);

		SetStdinEcho(false);

		while (true)
		{
			Log("Enter config decryption password: ");

			char decryptPass[encryptionPassSize];
			GetUserInput(decryptPass, sizeof(decryptPass));

			printf("\n");

			if (Crypto::Decrypt(fileContents, fileSize, decryptPass,
				keySize, pbkdfIterationCount, pbkdfHashAlgo,
				saltSize, ivSize, authTagSize, plaintextData))
			{
				memset(decryptPass, 0, sizeof(decryptPass));
				break;
			}
		}

		SetStdinEcho(true);

		free(fileContents);

		// trim the padding from the plaintext
		const BYTE paddingSize = plaintextData[plaintextDataPaddedSize - 1];
		plaintextData[plaintextDataPaddedSize - paddingSize] = '\0';

		const byte* fieldStart = plaintextData;

		for (size_t i = 0; i < fieldCount; ++i)
		{
			const byte* fieldEnd = (byte*)memchr(fieldStart, (int)fieldDelimiter, fieldSizes[i]);
			const size_t fieldLen = (fieldEnd - fieldStart);
			memcpy(fields[i], fieldStart, fieldLen);
			fields[i][fieldLen] = '\0';
			fieldStart = fieldEnd + 1;
		}

		memset(plaintextData, 0, plaintextDataPaddedSize);
		free(plaintextData);

		return true;
	}

	bool ImportMaFile(const char* path)
	{
		Log("Importing SDA maFile...");

		BYTE* contents = nullptr;
		long contentsLen = 0;
		if (!ReadFile(path, &contents, &contentsLen))
		{
			printf("failed to read\n");
			return false;
		}

		if (contents[0] != '{')
		{
			free(contents);
			printf("disable encryption in SDA before importing\n");
			return false;
		}

		rapidjson::Document doc;
		doc.Parse((char*)contents, contentsLen);

		free(contents);

		strcpy_s(username, sizeof(username), doc["account_name"].GetString());
		strcpy_s(shared, sizeof(shared), doc["shared_secret"].GetString());
		strcpy_s(identity, sizeof(identity), doc["identity_secret"].GetString());
		strcpy_s(deviceID, sizeof(deviceID), doc["device_id"].GetString());
		sprintf_s(steamID64, sizeof(steamID64), "%llu", doc["Session"]["SteamID"].GetUint64());

#ifndef _DEBUG
		std::filesystem::remove(path);
#endif // !_DEBUG

		printf("ok\n");
		return true;
	}
}