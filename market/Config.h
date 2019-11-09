#pragma once

#include "pkcs7.h"

namespace Config
{
	char username[32];
	char password[32];
	char shared[29];
	char identity[29];
	char deviceid[45];
	char steamid64[STEAMID64_LEN];
	//char steamapikey[33];
	char marketapikey[32];
	char successcheck[] = "hello";

	char* const fields[] = { username, password, shared, identity,
		deviceid, steamid64, /*steamapikey,*/ marketapikey, successcheck };

	constexpr const size_t fieldssize[] = { sizeof(username), sizeof(password), sizeof(shared), sizeof(identity),
		sizeof(deviceid), sizeof(steamid64), /*sizeof(steamapikey),*/ sizeof(marketapikey), sizeof(successcheck) };

	void Enter()
	{
		Log("Enter username: ");
		std::cin >> username;

		Log("Enter password: ");
		SetStdinEcho(false);
		std::cin >> password;
		SetStdinEcho(true);
		std::cout << '\n';

		Log("Enter base64 encoded shared secret: ");
		std::cin >> shared;

		Log("Enter base64 encoded identity secret: ");
		std::cin >> identity;

		Log("Enter device id: ");
		std::cin >> deviceid;

		Log("Enter market api-key: ");
		std::cin >> marketapikey;
	}

	bool Write()
	{
		Log("Enter config encryption password: ");
		char encryptpass[32];
		SetStdinEcho(false);
		std::cin >> encryptpass;
		SetStdinEcho(true);
		std::cout << '\n';

		constexpr const size_t fieldstotalsize = []() constexpr
		{
			size_t result = 0;
			for (size_t i = 0; i < _countof(fields); ++i)
				result += fieldssize[i];
			return result;
		}();

		char summary[fieldstotalsize] = { 0 };
		for (size_t i = 0; i < _countof(fields); ++i)
		{
			strcat_s(summary, sizeof(summary), fields[i]);
			strcat_s(summary, sizeof(summary), "\n");
		}

		Log("Encrypting config...");

		byte salt[16];
		byte iv[16];

		WC_RNG rng;
		if (wc_InitRng(&rng) || wc_RNG_GenerateBlock(&rng, salt, sizeof(salt)) || wc_RNG_GenerateBlock(&rng, iv, sizeof(iv)) || wc_FreeRng(&rng))
		{
			std::cout << "random generation failed\n";
			return false;
		}

		size_t summarylen = strlen(summary);

		int resultlen = summarylen + wc_PKCS7_GetPadSize(summarylen, AES_BLOCK_SIZE);
		byte* result = new byte[resultlen];
		wc_PKCS7_PadData((byte*)summary, summarylen, result, resultlen, AES_BLOCK_SIZE);

		if (0 > wc_CryptKey(encryptpass, strlen(encryptpass), salt, sizeof(salt), 100000,
			PBESTypes::PBE_AES256_CBC, result, resultlen, PKCSTypes::PKCS5v2, iv, 1))
		{
			std::cout << "encryption failed\n";
			return false;
		}

		byte salt64[int(sizeof(salt) * 1.5f)];
		size_t salt64len = sizeof(salt64);

		byte iv64[int(sizeof(iv) * 1.5f)];
		size_t iv64len = sizeof(iv64);

		size_t result64len = resultlen * 1.5f;
		byte* result64 = new byte[result64len];

		if (Base64_Encode_NoNl(salt, sizeof(salt), salt64, &salt64len) ||
			Base64_Encode_NoNl(iv, sizeof(iv), iv64, &iv64len) ||
			Base64_Encode_NoNl(result, resultlen, result64, &result64len))
		{
			std::cout << "base64 encoding failed\n";
			return false;
		}

		delete[] result;

		std::cout << "ok\n";
		Log("Writing config...");

		FILE* file;
		fopen_s(&file, "config", "wb");
		if (!file)
		{
			std::cout << "file creation failed\n";
			return false;
		}

		fwrite(salt64, sizeof(byte), salt64len, file);
		fputc('$', file);
		fwrite(iv64, sizeof(byte), iv64len, file);
		fputc('$', file);
		fwrite(result64, sizeof(byte), result64len, file);

		fclose(file);

		delete[] result64;

		std::cout << "ok\n";
		return true;
	}

	bool Read()
	{
		Log("Reading config...");

		FILE* file;
		fopen_s(&file, "config", "rb");
		if (!file)
		{
			std::cout << "not found or opening failed\n";
			return false;
		}

		fseek(file, 0, SEEK_END);
		size_t fsize = ftell(file);
		rewind(file);

		byte* content = new byte[fsize];
		if (fsize != fread_s(content, fsize, sizeof(byte), fsize, file))
		{
			std::cout << "file reading failed\n";
			return false;
		}
		fclose(file);

		std::cout << "ok\n";
		Log("Decoding config...");

		const byte* saltend = (const byte*)memchr(content, '$', fsize);
		const byte* ivend = (const byte*)memchr(saltend + 1, '$', fsize);

		byte salt64[int(16 * 1.5f)];
		memcpy_s(salt64, sizeof(salt64), content, saltend - content);

		byte iv64[int(16 * 1.5f)];
		memcpy_s(iv64, sizeof(iv64), saltend + 1, ivend - (saltend + 1));

		size_t result64len = fsize - ((ivend + 1) - content);
		byte* result64 = new byte[result64len];
		memcpy_s(result64, result64len, ivend + 1, result64len);

		delete[] content;

		byte salt[int(sizeof(salt64) / 1.3f)];
		size_t saltlen = sizeof(salt);

		byte iv[int(sizeof(iv64) / 1.3f)];
		size_t ivlen = sizeof(iv);

		size_t resultlen = result64len / 1.3f;
		byte* result = new byte[resultlen];

		if (Base64_Decode(salt64, sizeof(salt64), salt, &saltlen) ||
			Base64_Decode(iv64, sizeof(iv64), iv, &ivlen) ||
			Base64_Decode(result64, result64len, result, &resultlen))
		{
			std::cout << "base64 decoding failed\n";
			return false;
		}

		delete[] result64;
		std::cout << "ok\n";

		char* summary = new char[resultlen + 1] { 0 };

		SetStdinEcho(false);
		while (true)
		{
			Log("Enter config decryption password: ");
			char decryptpass[32];
			std::cin >> decryptpass;
			std::cout << '\n';

			Log("Decrypting config...");

			memcpy_s(summary, resultlen, result, resultlen);

			if (0 > wc_CryptKey(decryptpass, strlen(decryptpass), salt, 16, 100000,
				PBESTypes::PBE_AES256_CBC, (byte*)summary, resultlen, PKCSTypes::PKCS5v2, iv, 0))
			{
				std::cout << "decryption failed\n";
				return false;
			}

			char* lastnewline = strrchr(summary, '\n');
			if (!lastnewline || memcmp(lastnewline - (sizeof(Config::successcheck) - 1), Config::successcheck, sizeof(Config::successcheck) - 1))
				std::cout << "wrong password\n";
			else
				break;
		}
		SetStdinEcho(true);

		delete[] result;

		char* fieldstart[_countof(fields)] = { summary };

		for (size_t i = 0; i < (_countof(fields) - 1); ++i)
		{
			fieldstart[i + 1] = strchr(fieldstart[i] + 1, '\n') + 1;
			strncpy_s(fields[i], fieldssize[i], fieldstart[i], (fieldstart[i + 1] - fieldstart[i] - 1));
		}

		delete[] summary;

		std::cout << "ok\n";
		return true;
	}
}