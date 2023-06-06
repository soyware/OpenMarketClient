#pragma once

// puts without newline
inline int putsnn(const char* buf)
{
	return fputs(buf, stdout);
}

enum class LogChannel
{
	GENERAL,
	LIBCURL,
	STEAM,
	MARKET
};

const char* logChannelNames[] =
{
	"",
	"libcurl",
	"Steam",
	"Market"
};

void Log(LogChannel channel, const char* format, ...)
{
	const time_t timestamp = time(nullptr);

	// zh_CN.utf8 locale's time on linux looks like this 2022年10月18日 15时08分28秒
	// so allocate some space
	char dateTime[64];

#ifdef _WIN32
	// windows didn't support utf-8 codepages until recently, so map UTF-16 to UTF-8 instead
	const size_t wideDatatimeLen = sizeof(dateTime);
	wchar_t wideDatetime[wideDatatimeLen];
	wcsftime(wideDatetime, wideDatatimeLen, L"%x %X", localtime(&timestamp));

	if (!WideCharToMultiByte(CP_UTF8, 0, wideDatetime, -1, dateTime, sizeof(dateTime), NULL, NULL))
		strcpy(dateTime, "timestamp UTF-16 to UTF-8 mapping failed");

#else
	strftime(dateTime, sizeof(dateTime), "%x %X", localtime(&timestamp));
#endif // _WIN32

	printf("[%s] ", dateTime);

	if (channel != LogChannel::GENERAL)
		printf("[%s] ", logChannelNames[(size_t)channel]);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

#ifdef _WIN32
void FlashCurrentWindow()
{
	static const HWND hWnd = GetConsoleWindow();
	if (!hWnd) return;
	FlashWindow(hWnd, TRUE);
}
#endif

void SetStdinEcho(bool enable)
{
#ifdef _WIN32
	const HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
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

bool GetUserInputString(const char* msg, char* buf, size_t bufSz, size_t minLen = 1, bool echoStdin = true)
{
	const size_t maxLen = bufSz - 1;

	if (!echoStdin)
		SetStdinEcho(false);

#ifdef _WIN32
	FlashCurrentWindow();

	wchar_t* wideBuf = (wchar_t*)malloc(bufSz * sizeof(wchar_t));
	if (!wideBuf)
	{
		Log(LogChannel::GENERAL, "Wide input buffer allocation failed\n");
		return false;
	}
#endif

	while (true)
	{
		if (1 < minLen)
		{
			if (minLen == maxLen)
				Log(LogChannel::GENERAL, "%s (%u bytes): ", msg, maxLen);
			else
				Log(LogChannel::GENERAL, "%s (%u-%u bytes): ", msg, minLen, maxLen);
		}
		else
			Log(LogChannel::GENERAL, "%s (%u bytes max): ", msg, maxLen);

		size_t len = 0;

#ifdef _WIN32
		wint_t c;
		while ((c = getwchar()) != L'\n' && c != WEOF)
#else
		// if the byte is a part of UTF-8 char it would have 8th bit set
		// therefore we can't accidentally find newline
		int c;
		while ((c = getchar()) != '\n' && c != EOF)
#endif // _WIN32
		{
			if (len <= maxLen)
#ifdef _WIN32
				wideBuf[len] = c;
#else
				buf[len] = c;
#endif // _WIN32

			++len;
		}

		if (!echoStdin)
			putchar('\n');

		if (minLen > len || len > maxLen)
			continue;

#ifdef _WIN32
		wideBuf[len] = L'\0';
#else
		buf[len] = '\0';
#endif // _WIN32

#ifdef _WIN32
		if (!WideCharToMultiByte(CP_UTF8, 0, wideBuf, len + 1, buf, bufSz, NULL, NULL))
		{
			Log(LogChannel::GENERAL, "Input UTF-16 to UTF-8 mapping failed\n");
			continue;
		}
#endif // _WIN32

		break;
	}

#ifdef _WIN32
	free(wideBuf);
#endif // _WIN32

	if (!echoStdin)
		SetStdinEcho(true);

	return true;
}

#ifdef _WIN32

inline void* mempcpy(void* dest, const void* src, size_t size)
{
	return (char*)memcpy(dest, src, size) + size;
}

inline char* stpcpy(char* dest, const char* src)
{
	const size_t len = strlen(src);
	return (char*)memcpy(dest, src, len + 1) + len;
}

inline char* stpncpy(char* dest, const char* src, size_t count)
{
	const size_t len = strnlen(src, count);
	memcpy(dest, src, len);
	dest += len;
	if (len == count)
		return dest;
	return (char*)memset(dest, '\0', count - len);
}

// windows utf-8 fopen
FILE* u8fopen(const char* path, const char* mode)
{
	const size_t widePathLen = PATH_MAX;
	wchar_t widePath[widePathLen];

	const size_t wideModeLen = 32;
	wchar_t wideMode[widePathLen];

	if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, widePathLen) ||
		!MultiByteToWideChar(CP_UTF8, 0, mode, -1, wideMode, wideModeLen))
		return nullptr;

	return _wfopen(widePath, wideMode);
}

#else

inline FILE* u8fopen(const char* path, const char* mode)
{
	return fopen(path, mode);
}

#endif // _WIN32

// writes pointer to file contents heap to output
bool ReadFile(const char* path, unsigned char** out, long* outSz)
{
	FILE* file = u8fopen(path, "rb");
	if (!file)
		return false;

	long fsize;

	if (fseek(file, 0, SEEK_END) ||
		((fsize = ftell(file)) == -1L) ||
		fseek(file, 0, SEEK_SET))
	{
		fclose(file);
		return false;
	}

	unsigned char* contents = (unsigned char*)malloc(fsize);
	if (!contents)
	{
		fclose(file);
		return false;
	}

	if (fread(contents, sizeof(unsigned char), fsize, file) != fsize)
	{
		fclose(file);
		free(contents);
		return false;
	}

	fclose(file);

	*out = contents;
	*outSz = fsize;

	return true;
}

// get executable dir
const char* GetExeDir()
{
	static char dir[PATH_MAX] = { 0 };
	if (!dir[0])
	{
#ifdef _WIN32
		const size_t wideDirLen = sizeof(dir);
		wchar_t wideDir[wideDirLen];
		if (!GetModuleFileNameW(NULL, wideDir, wideDirLen))
			return nullptr;

		if (!WideCharToMultiByte(CP_UTF8, 0, wideDir, -1, dir, sizeof(dir), NULL, NULL))
			return nullptr;

		// if the byte is a part of UTF-8 char it would have 8th bit set
		// therefore we can't accidentally find backslash
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
	// check if stdout isn't a terminal
	if (!_isatty(_fileno(stdout)))
		return;

	FlashCurrentWindow();

	// check if run by doubleclicking the executable
	if (getenv("PROMPT"))
		return;

	putsnn("Press Enter to continue\n");

	wint_t c;
	while ((c = getwchar()) != L'\n' && c != WEOF);

#endif // _WIN32
}

inline uint64_t SteamID32To64(uint32_t id32)
{
	return (id32 | 0x110000100000000);
}

inline uint32_t SteamID64To32(uint64_t id64)
{
	return (id64 & 0xFFFFFFFF);
}

inline uint32_t byteswap32(uint32_t dw)
{
	uint32_t res;

	res = dw >> 24;
	res |= ((dw & 0x00FF0000) >> 8);
	res |= ((dw & 0x0000FF00) << 8);
	res |= ((dw & 0x000000FF) << 24);

	return res;
}

inline uint64_t byteswap64(uint64_t qw)
{
	uint64_t res;

	res = qw >> 56;
	res |= ((qw & 0x00FF000000000000ull) >> 40);
	res |= ((qw & 0x0000FF0000000000ull) >> 24);
	res |= ((qw & 0x000000FF00000000ull) >> 8);
	res |= ((qw & 0x00000000FF000000ull) << 8);
	res |= ((qw & 0x0000000000FF0000ull) << 24);
	res |= ((qw & 0x000000000000FF00ull) << 40);
	res |= ((qw & 0x00000000000000FFull) << 56);

	return res;
}

void ClearConsole()
{
#ifdef _WIN32
	// is stdout a terminal
	if (!_isatty(_fileno(stdout)))
		return;

	const HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	// Get the number of character cells in the current buffer.
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hStdout, &csbi))
		return;

	// Scroll the rectangle of the entire buffer.
	SMALL_RECT scrollRect;
	scrollRect.Left = 0;
	scrollRect.Top = 0;
	scrollRect.Right = csbi.dwSize.X;
	scrollRect.Bottom = csbi.dwSize.Y;

	// Scroll it upwards off the top of the buffer with a magnitude of the entire height.
	COORD scrollTarget;
	scrollTarget.X = 0;
	scrollTarget.Y = (SHORT)(0 - csbi.dwSize.Y);

	// Fill with empty spaces with the buffer's default text attribute.
	CHAR_INFO fill;
	fill.Char.AsciiChar = ' ';
	fill.Attributes = csbi.wAttributes;

	// Do the scroll
	ScrollConsoleScreenBufferA(hStdout, &scrollRect, NULL, scrollTarget, &fill);

	// Move the cursor to the top left corner too.
	csbi.dwCursorPosition.X = 0;
	csbi.dwCursorPosition.Y = 0;

	SetConsoleCursorPosition(hStdout, csbi.dwCursorPosition);
#else

	putsnn("\x1b[H\x1b[J\x1b[3J");

#endif // _WIN32
}