#pragma once

#define SAVED_MEMBERS_OFFSET (offsetof(CAccount, marketApiKey))
#define SAVED_MEMBERS_SIZE (sizeof(CAccount) - SAVED_MEMBERS_OFFSET)

class CAccount
{
public:
	char		name[128] = "";

	// everything below gets saved
	char		marketApiKey[Market::apiKeySz + 1] = "";
	char		identitySecret[Steam::Guard::secretsSz + 1] = "";
	char		deviceId[Steam::Guard::deviceIdBufSz] = "";
	char		steamApiKey[Steam::apiKeyBufSz] = "";

	char		steamId64[UINT64_MAX_STR_SIZE] = "";

	char		oauthToken[Steam::Auth::oauthTokenBufSz] = "";
	char		loginToken[Steam::Auth::loginTokenBufSz] = "";

	//char		refreshToken[Steam::Auth::jwtBufSz] = "";
	//char		accessToken[Steam::Auth::jwtBufSz] = "";

	//char		sessionId[Steam::Auth::sessionIdBufSz] = "";

	static constexpr const char	dirName[] = "accounts";
	static constexpr const char	extension[] = ".bin";

	static constexpr int		pbkdfIterationCount = 1000000;
	static constexpr int		pbkdfHashAlgo = WC_SHA256;
	static constexpr size_t		pbkdfSaltSz = (128 / 8);	// NIST recommends at least 128 bits

	static constexpr size_t		keySz = AES_256_KEY_SIZE;
	static constexpr size_t		ivSz = GCM_NONCE_MID_SZ;
	static constexpr size_t		authTagSz = (128 / 8);		// max allowed tag size is 128 bits

	bool Save(const char* encryptPass)
	{
		byte plaintext[SAVED_MEMBERS_SIZE];
		memcpy(plaintext, (char*)this + SAVED_MEMBERS_OFFSET, sizeof(plaintext));

		byte salt[pbkdfSaltSz];
		byte iv[ivSz];
		byte authTag[authTagSz];

		byte* encrypted = nullptr;
		word32 encryptedSz = 0;

		const bool encryptFailed = 
			!Crypto::Encrypt(encryptPass, keySz, pbkdfIterationCount, pbkdfHashAlgo, 
				plaintext, sizeof(plaintext),
				salt, pbkdfSaltSz,
				iv, ivSz, 
				authTag, authTagSz,
				&encrypted, &encryptedSz);

		memset(plaintext, 0, sizeof(plaintext));

		if (encryptFailed)
			return false;

		Log(LogChannel::GENERAL, "[%s] Saving...", name);

		const std::filesystem::path dir(dirName);

		if (!std::filesystem::exists(dir) && !std::filesystem::create_directory(dir))
		{
			putsnn("accounts directory creation failed\n");
			return false;
		}

		char path[PATH_MAX];

		char* pathEnd = path;
		pathEnd = stpcpy(pathEnd, dirName);
		pathEnd = stpcpy(pathEnd, "/");
		pathEnd = stpcpy(pathEnd, name);
		strcpy(pathEnd, extension);

		FILE* file = u8fopen(path, "wb");
		if (!file)
		{
			putsnn("file creation failed\n");
			return false;
		}

		const bool writeFailed = 
			((fwrite(salt, sizeof(byte), pbkdfSaltSz, file)		!= pbkdfSaltSz) ||
			(fwrite(iv, sizeof(byte), ivSz, file)				!= ivSz) ||
			(fwrite(authTag, sizeof(byte), authTagSz, file)		!= authTagSz) ||
			(fwrite(encrypted, sizeof(byte), encryptedSz, file) != encryptedSz));

		free(encrypted);
		fclose(file);

		if (writeFailed)
		{
			putsnn("writing failed\n");
			return false;
		}

		putsnn("ok\n");
		return true;
	}

	bool Load(const char* path, const char* decryptPass)
	{
		Log(LogChannel::GENERAL, "[%s] Reading...", name);

		unsigned char* contents = nullptr;
		long contentsSz = 0;
		if (!ReadFile(path, &contents, &contentsSz))
		{
			putsnn("fail\n");
			return false;
		}

		putsnn("ok\n");

		const byte* salt = contents;
		const byte* iv = salt + pbkdfSaltSz;
		const byte* authTag = iv + ivSz;
		const byte* data = authTag + authTagSz;
		const size_t dataSz = contentsSz - (data - contents);

		byte* plaintext = nullptr;
		word32 plaintextSz = 0;

		const bool decryptFailed = 
			!Crypto::Decrypt(decryptPass, keySz, pbkdfIterationCount, pbkdfHashAlgo,
				data, dataSz,
				salt, pbkdfSaltSz,
				iv, ivSz,
				authTag, authTagSz,
				&plaintext, &plaintextSz);

		free(contents);

		if (decryptFailed)
			return false;

		const bool incompatible = (plaintextSz != SAVED_MEMBERS_SIZE);

		if (!incompatible)
			memcpy((char*)this + SAVED_MEMBERS_OFFSET, plaintext, plaintextSz);

		memset(plaintext, 0, plaintextSz);
		free(plaintext);

		if (incompatible)
		{
			Log(LogChannel::GENERAL, "[%s] Incompatible config\n", name);
			return false;
		}

		return true;
	}

	// outUsername buffer size must be at least Steam::Auth::usernameBufSz
	// outSharedSecret buffer size must be at least Steam::Guard::secretsSz + 1
	bool ImportMaFile(const char* path, char* outUsername, char* outSharedSecret)
	{
		Log(LogChannel::GENERAL, "[%s] Importing maFile...", name);

		unsigned char* contents = nullptr;
		long contentsSz = 0;
		if (!ReadFile(path, &contents, &contentsSz))
		{
			putsnn("reading failed\n");
			return false;
		}

		if (contents[0] != '{')
		{
			free(contents);
			putsnn("invalid maFile, disable encryption in SDA before importing\n");
			return false;
		}

		rapidjson::Document parsed;
		parsed.Parse((char*)contents, contentsSz);

		free(contents);

		if (parsed.HasParseError())
		{
			putsnn("JSON parsing failed\n");
			return false;
		}

		const auto iterSession = parsed.FindMember("Session");
		if (iterSession == parsed.MemberEnd() || iterSession->value.ObjectEmpty())
		{
			putsnn("session info not found or empty\n");
			return false;
		}

		//const char* steamLoginSecure = iterSession->value["SteamLoginSecure"].GetString();
		//const char* steamLoginSecureDelim = strchr(steamLoginSecure, '%');

		const std::string steamId(std::to_string(iterSession->value["SteamID"].GetUint64()));

		strcpy(identitySecret, parsed["identity_secret"].GetString());
		strcpy(deviceId, parsed["device_id"].GetString());
		//strcpy(sessionId, iterSession->value["SessionID"].GetString());
		//strcpy(loginToken, steamLoginSecureDelim + 6);
		strcpy(oauthToken, iterSession->value["OAuthToken"].GetString());
		//stpncpy(steamId64, steamLoginSecure, (steamLoginSecureDelim - steamLoginSecure))[0] = '\0';
		strcpy(steamId64, steamId.c_str());

		strcpy(outUsername, parsed["account_name"].GetString());
		strcpy(outSharedSecret, parsed["shared_secret"].GetString());

		putsnn("ok\n");
		return true;
	}

	bool EnterMarketApiKey(CURL* curl)
	{
		while (true)
		{
			if (!GetUserInputString("Enter market API key", marketApiKey, Market::apiKeySz + 1, Market::apiKeySz))
				return false;

			rapidjson::Document parsed;
			if (Market::GetProfileStatus(curl, marketApiKey, &parsed))
				break;
		}
		return true;
	}

	bool EnterIdentitySecret(CURL* curl)
	{
		while (true)
		{
			if (!GetUserInputString("Enter Steam Guard identity_secret", identitySecret, Steam::Guard::secretsSz + 1, Steam::Guard::secretsSz))
				return false;

			Curl::CResponse response;
			if (0 <= Steam::Guard::FetchConfirmations(curl, steamId64, identitySecret, deviceId, &response))
				break;
		}
		return true;
	}

	bool Init(CURL* curl, const char* sessionId, const char* encryptPass, const char* _name = nullptr, const char* path = nullptr, bool isMaFile = false)
	{
		char username[Steam::Auth::usernameBufSz] = "";
		char sharedSecret[Steam::Guard::secretsSz + 1] = "";

		if (_name)
			strcpy(name, _name);
		else
		{
			if (!GetUserInputString("Enter new config name", name, sizeof(name)))
				return false;
		}

		if (path)
		{
			if (isMaFile)
			{
				if (!ImportMaFile(path, username, sharedSecret))
					return false;
			}
			else
			{
				if (!Load(path, encryptPass))
					return false;
			}
		}

		bool loginRequired = true;

		if (oauthToken[0])
		{
			if (Steam::Auth::RefreshOAuthSession(curl, oauthToken, loginToken))
				loginRequired = false;
			else
				Log(LogChannel::GENERAL, "[%s] Steam OAuth session refresh failed (token may have expired), login required\n", name);
		}

		bool loggedIn = !loginRequired;

		if (loginRequired)
		{
			char password[Steam::Auth::passwordBufSz];
			char twoFactorCode[Steam::Guard::twoFactorCodeBufSz] = "";

			if (!username[0])
			{
				if (!GetUserInputString("Enter Steam username", username, sizeof(username)))
					return false;
			}

			if (!GetUserInputString("Enter Steam password", password, sizeof(password), 1, false))
				return false;

			const auto GetTwoFactorAuthCode = [&]
			{
				if (sharedSecret[0])
					Steam::Guard::GenerateTwoFactorAuthCode(sharedSecret, twoFactorCode);

				if (!twoFactorCode[0])
				{
					if (!GetUserInputString("Enter Steam Guard Mobile Authenticator code",
						twoFactorCode, sizeof(twoFactorCode), Steam::Guard::twoFactorCodeBufSz - 1))
						return false;
				}
				return true;
			};

			if (!GetTwoFactorAuthCode())
				return false;

			const size_t retryCount = 3;

			for (size_t i = 0; i < retryCount; ++i)
			{
				const Steam::Auth::LoginResult loginRes =
					Steam::Auth::DoLogin(curl, username, password, twoFactorCode, steamId64, oauthToken, loginToken);

				if (loginRes == Steam::Auth::LoginResult::OK)
				{
					loggedIn = true;
					break;
				}
				else if (loginRes == Steam::Auth::LoginResult::WRONG_TWO_FACTOR)
				{
					if (!GetTwoFactorAuthCode())
						return false;
				}
				else if (loginRes != Steam::Auth::LoginResult::WRONG_CAPTCHA)
					break;
			}

			memset(password, 0, sizeof(password));
			memset(twoFactorCode, 0, sizeof(twoFactorCode));
		}

		memset(username, 0, sizeof(username));
		memset(sharedSecret, 0, sizeof(sharedSecret));

		if (!loggedIn)
			return false;

		if (!deviceId[0] && !Steam::Guard::GetDeviceId(curl, steamId64, oauthToken, deviceId))
			return false;

		if (!identitySecret[0] && !EnterIdentitySecret(curl))
			return false;

		if (!Steam::SetLoginCookie(curl, steamId64, loginToken))
			return false;

		if (!Steam::SetInventoryPublic(curl, sessionId, steamId64))
			return false;

		if (!steamApiKey[0] && !Steam::GetApiKey(curl, sessionId, steamApiKey))
			return false;

		if (!marketApiKey[0] && !EnterMarketApiKey(curl))
			return false;

		if (loginRequired || isMaFile)
		{
			if (Save(encryptPass))
			{
				if (isMaFile)
					std::filesystem::remove(path);
			}
		}

		if (!Market::SetSteamDetails(curl, marketApiKey, steamApiKey))
			return false;

		if (!Market::CanSell(curl, marketApiKey))
			return false;

		return true;
	}
};