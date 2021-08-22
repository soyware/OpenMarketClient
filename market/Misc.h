#pragma once

void Log(const char* format, ...)
{
	time_t t;
	time(&t);
	char timedate[24];
	strftime(timedate, sizeof(timedate), "%X %x", localtime(&t));

	std::cout << timedate << ' ';

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

void SetStdinEcho(bool enable)
{
#ifdef _WIN32
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(hStdin, &mode);

	if (!enable)
		mode &= ~ENABLE_ECHO_INPUT;
	else
		mode |= ENABLE_ECHO_INPUT;

	SetConsoleMode(hStdin, mode);

#else
	termios tty;
	tcgetattr(STDIN_FILENO, &tty);

	if (!enable)
		tty.c_lflag &= ~ECHO;
	else
		tty.c_lflag |= ECHO;

	tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif // _WIN32
}

// get executable dir
const char* GetExecDir()
{
	static char dir[PATH_MAX] = { 0 };

	if (!dir[0])
	{
#ifdef _WIN32
		if (!GetModuleFileNameA(NULL, dir, _countof(dir)))
			return nullptr;

		char* del = strrchr(dir, '\\');
#else
		if (readlink("/proc/self/exe", dir, sizeof(dir)) == -1)
			return nullptr;

		char* del = strrchr(dir, '/');
#endif // _WIN32

		if (del)
			*(del + 1) = '\0';
	}

	return dir;
}

void Pause()
{
#ifdef _WIN32
	// stdout isn't a console
#pragma warning ( suppress: 4996 )
	if (!isatty(fileno(stdout)))
		return;

	// run from cmd
	if (getenv("PROMPT"))
		return;

	std::cout << "Press any key to exit...";
	std::cin.ignore(2);
#endif // _WIN32
}

#define BASE64_LINE_SZ 64

// https://github.com/wolfSSL/wolfssl/blob/master/wolfcrypt/src/coding.c#L103
constexpr size_t Base64ToPlainSize(size_t inLen)
{
	word32 plainSz = inLen - ((inLen + (BASE64_LINE_SZ - 1)) / BASE64_LINE_SZ);
	plainSz = (plainSz * 3 + 3) / 4;
	return plainSz;
}

// https://github.com/wolfSSL/wolfssl/blob/master/wolfcrypt/src/coding.c#L300
constexpr size_t PlainToBase64Size(size_t inLen, Escaped escaped)
{
	word32 outSz = (inLen + 3 - 1) / 3 * 4;
	word32 addSz = (outSz + BASE64_LINE_SZ - 1) / BASE64_LINE_SZ;  /* new lines */

	if (escaped == WC_ESC_NL_ENC)
		addSz *= 3;   /* instead of just \n, we're doing %0A triplet */
	else if (escaped == WC_NO_NL_ENC)
		addSz = 0;    /* encode without \n */

	outSz += addSz;

	return outSz;
}

inline uint64_t SteamID32To64(uint32_t steamid32)
{
	return (0x110000100000000 | steamid32);
}

inline uint32_t SteamID64To32(uint64_t steamid64)
{
	return (steamid64 & 0xFFFFFFFF);
}