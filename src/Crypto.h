#pragma once

#ifndef BASE64_LINE_SZ
#define BASE64_LINE_SZ 64
#endif // !BASE64_LINE_SZ

// Source: https://github.com/wolfSSL/wolfssl/blob/master/wolfcrypt/src/coding.c
constexpr word32 Base64ToPlainSize(size_t inLen)
{
	word32 plainSz = inLen - ((inLen + (BASE64_LINE_SZ - 1)) / BASE64_LINE_SZ);
	plainSz = (plainSz * 3 + 3) / 4;
	return plainSz;
}

constexpr word32 PlainToBase64Size(size_t inLen, Escaped escaped)
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

constexpr size_t GetBase64PaddedLen(size_t inLen)
{
	size_t outLen = inLen;

	const size_t multiple = 4;
	const size_t remainder = inLen % multiple;
	if (remainder != 0)
		outLen += multiple - remainder;

	return outLen;
}

void Base64ToBase64URL(char* in, size_t len)
{
	for (size_t i = 0; i < len; ++i)
	{
		if (in[i] == '-')
			in[i] = '+';
		else if (in[i] == '_')
			in[i] = '/';
	}
}

namespace Crypto
{
	bool Encrypt(const char* password, word32 keySz, int scryptCost, int scryptBlockSz, int scryptParallel,
		const byte* plaintext, word32 plaintextSz,
		byte* outSalt, word32 outSaltSz,
		byte* outIV, word32 outIVSz,
		byte* outAuthTag, word32 outAuthTagSz,
		byte* outCipher)
	{
		Log(LogChannel::GENERAL, "Encrypting...");

		WC_RNG rng;
		if (wc_InitRng(&rng))
		{
			putsnn("RNG init failed\n");
			return false;
		}

		const bool rngFailed = (wc_RNG_GenerateBlock(&rng, outSalt, outSaltSz) || wc_RNG_GenerateBlock(&rng, outIV, outIVSz));

		wc_FreeRng(&rng);

		if (rngFailed)
		{
			putsnn("RNG generation failed\n");
			return false;
		}

		byte* key = (byte*)malloc(keySz);
		if (!key)
		{
			putsnn("key allocation failed\n");
			return false;
		}

		Aes aes;

		const bool stretchFailed =
			(wc_scrypt(key, (byte*)password, strlen(password), outSalt, outSaltSz, scryptCost, scryptBlockSz, scryptParallel, keySz) ||
			wc_AesGcmSetKey(&aes, key, keySz));

		free(key);

		if (stretchFailed)
		{
			putsnn("key stretching or setting AES key failed\n");
			return false;
		}

		if (wc_AesGcmEncrypt(&aes, outCipher, plaintext, plaintextSz, outIV, outIVSz, outAuthTag, outAuthTagSz, nullptr, 0))
		{
			putsnn("encryption failed\n");
			return false;
		}

		putsnn("ok\n");
		return true;
	}

	bool Decrypt(const char* password, word32 keySz, int scryptCost, int scryptBlockSz, int scryptParallel,
		const byte* cipher, size_t cipherSz,
		const byte* salt, word32 saltSz,
		const byte* iv, word32 ivSz,
		const byte* authTag, word32 authTagSz,
		byte* outPlaintext)
	{
		Log(LogChannel::GENERAL, "Decrypting...");

		byte* key = (byte*)malloc(keySz);
		if (!key)
		{
			putsnn("key allocation failed\n");
			return false;
		}

		Aes aes;

		const bool stretchFailed =
			(wc_scrypt(key, (byte*)password, strlen(password), salt, saltSz, scryptCost, scryptBlockSz, scryptParallel, keySz) ||
			wc_AesGcmSetKey(&aes, key, keySz));

		free(key);

		if (stretchFailed)
		{
			putsnn("key stretching or setting AES key failed\n");
			return false;
		}

		if (wc_AesGcmDecrypt(&aes, outPlaintext, cipher, cipherSz, iv, ivSz, authTag, authTagSz, nullptr, 0))
		{
			putsnn("wrong password or decryption failed\n");
			return false;
		}

		putsnn("ok\n");
		return true;
	}
}