#pragma once

// https://en.cppreference.com/w/cpp/language/fold
template <typename... Args>
void Log(Args&&... args)
{
	time_t t;
	time(&t);
	tm* lt = localtime(&t);
	char timedate[24];
	strftime(timedate, sizeof(timedate), "%X %x", lt);

	std::cout << timedate << ' ';

	(std::cout << ... << args);
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

void Pause()
{
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4996)

	// stdout isn't a console
	if (!isatty(fileno(stdout)))
		return;

	// run from cmd
	if (getenv("PROMPT"))
		return;

	std::cout << "Press any key to exit...";
	std::cin.ignore(2);

#pragma warning(pop)
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