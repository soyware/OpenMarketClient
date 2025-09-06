#include "Precompiled.h"
#include <string>
#include "Misc.h"
#include "Curl.h"
#include "Crypto.h"
#include "Steam/Steam.h"
#include "Market.h"
#include "Account.h"

#define OPENMARKETCLIENT_VERSION "0.4.4"

void SetLocale()
{
	// LC_ALL breaks SetConsoleOutputCP
	setlocale(LC_TIME, "");
#ifdef _WIN32
	_setmode(_fileno(stdin), _O_U16TEXT);
	SetConsoleOutputCP(CP_UTF8);
#endif // _WIN32
}

void PrintVersion()
{
	putsnn("OpenMarketClient v" OPENMARKETCLIENT_VERSION ", built with "
		"libcurl v" LIBCURL_VERSION ", "
		"wolfSSL v" LIBWOLFSSL_VERSION_STRING ", "
		"rapidJSON v" RAPIDJSON_VERSION_STRING "\n");
}

namespace Args
{
	bool		newAcc = false;
	bool		marketUseProxy = false;
	const char* proxy = nullptr;

	void PrintHelp()
	{
		putsnn("Options:\n"
			"--help\t\t\t\t\t\t\tPrint help\n"
			"--new\t\t\t\t\t\t\tAdd a new account by manually entering the details\n"
			"--proxy [scheme://][username:password@]host[:port]\tSets the global proxy\n"
			"--market-use-proxy\t\t\t\t\tTells the market to perform actions using "
				"the proxy specified in --proxy, presumably to avoid Steam bans\n");
	}

	bool Parse(int argc, char** const argv)
	{
		for (int i = 1; i < argc; ++i)
		{
			const char* arg = argv[i];

			if (!strcmp(arg, "--help"))
			{
				PrintHelp();
				return false;
			}
			else if (!strcmp(arg, "--new"))
				newAcc = true;
			else if (!strcmp(arg, "--market-use-proxy"))
				marketUseProxy = true;
			else if ((i < (argc - 1)) && !strcmp(arg, "--proxy")) // check if second to last argument
			{
				proxy = argv[i + 1];
				++i;
			}
			else
				Log(LogChannel::GENERAL, "Unknown argument: %s\n", arg);
		}
		return true;
	}
}

bool SetWorkDirToExeDir()
{
	const char* exeDir = GetExeDir();
	if (!exeDir)
		return false;

#ifdef _WIN32
	const size_t wideExeDirLen = PATH_MAX;
	wchar_t wideExeDir[wideExeDirLen];

	if (!MultiByteToWideChar(CP_UTF8, 0, exeDir, -1, wideExeDir, wideExeDirLen))
		return false;

	return !_wchdir(wideExeDir);
#else

	return !chdir(exeDir);
#endif // _WIN32
}

bool InitSavedAccounts(CURL* curl, const char* sessionId, const char* encryptPass, std::vector<CAccount>* accounts)
{
	const std::filesystem::path dir(CAccount::directory);

	if (!std::filesystem::exists(dir))
		return true;

	bool success = true;

	for (const auto& entry : std::filesystem::directory_iterator(dir))
	{
		const auto& path = entry.path();
		const auto& extension = path.extension();

		const bool isMaFile = !extension.compare(".maFile");

		if (extension.compare(CAccount::extension) && !isMaFile)
			continue;

		const auto& stem = path.stem();

#ifdef _WIN32
		const auto* widePath = path.c_str();
		const auto* wideFilenameNoExt = stem.c_str();

		char szPath[PATH_MAX];
		char szFilenameNoExt[PATH_MAX];

		if (!WideCharToMultiByte(CP_UTF8, 0, widePath, -1, szPath, sizeof(szPath), NULL, NULL) ||
			!WideCharToMultiByte(CP_UTF8, 0, wideFilenameNoExt, -1, szFilenameNoExt, sizeof(szFilenameNoExt), NULL, NULL))
		{
			Log(LogChannel::GENERAL, "One of the account's filename UTF-16 to UTF-8 mapping failed\n");
			success = false;
			break;
		}

#else
		const char* szPath = path.c_str();
		const char* szFilenameNoExt = stem.c_str();
#endif

		CAccount account;

		if (!account.Init(curl, sessionId, encryptPass, szFilenameNoExt, szPath, isMaFile))
		{
			success = false;
			break;
		}

		accounts->emplace_back(account);
	}

	return success;
}

int main(int argc, char** argv)
{
	// disable stdout buffering if stdout is a terminal
	if (_isatty(_fileno(stdout)))
		setvbuf(stdout, nullptr, _IONBF, 0);

	SetLocale();
	PrintVersion();

	if (!Args::Parse(argc, argv))
	{
		Pause();
		return 0;
	}

	if (!SetWorkDirToExeDir())
	{
		Log(LogChannel::GENERAL, "Setting working directory failed\n");
		Pause();
		return 1;
	}

	CURL* curl = Curl::Init(Args::proxy);
	if (!curl)
	{
		Pause();
		return 1;
	}

	char sessionId[Steam::sessionIdBufSz];

	if (!Steam::GenerateSessionId(sessionId) || !Steam::SetSessionCookie(curl, sessionId))
	{
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		Pause();
		return 1;
	}

	if (!Steam::Guard::SyncTime(curl))
	{
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		Pause();
		return 1;
	}

	char encryptPass[64];
	if (!GetUserInputString("Enter encryption password", encryptPass, sizeof(encryptPass), 10, false))
	{
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		Pause();
		return 1;
	}

	std::vector<CAccount> accounts;

	if (Args::newAcc)
	{
		CAccount account;
		while (!account.Init(curl, sessionId, encryptPass));

		accounts.emplace_back(account);
	}

	if (!InitSavedAccounts(curl, sessionId, encryptPass, &accounts))
	{
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		Pause();
		return 1;
	}

	if (accounts.empty())
	{
		Log(LogChannel::GENERAL, "No accounts, adding a new one\n");

		CAccount account;
		while (!account.Init(curl, sessionId, encryptPass));

		accounts.emplace_back(account);
	}

	memset(encryptPass, 0, sizeof(encryptPass));

#ifdef _WIN32
	// prevent sleep
	SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
#endif // _WIN32

	const char* marketProxy = Args::marketUseProxy ? Args::proxy : nullptr;

	const size_t accountCount = accounts.size();

	while (true)
	{
		for (size_t i = 0; i < accountCount; ++i)
			accounts[i].RunMarkets(curl, sessionId, marketProxy);

		if (1 < accountCount)
			putchar('\n');

		std::this_thread::sleep_for(1min);
	}

	curl_easy_cleanup(curl);
	curl_global_cleanup();
	Pause();
	return 0;
}