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

// whole PKCS7 is too heavy
#ifndef HAVE_PKCS7
// Source: https://github.com/wolfSSL/wolfssl/blob/master/wolfcrypt/src/pkcs7.c
/* return size of padded data, padded to blockSz chunks, or negative on error */
constexpr int wc_PKCS7_GetPadSize(word32 inputSz, word32 blockSz)
{
	if (blockSz == 0)
		return BAD_FUNC_ARG;

	return (blockSz - (inputSz % blockSz));
}

/* pad input data to blockSz chunk, place in outSz. out must be big enough
 * for input + pad bytes. See PKCS7_GetPadSize() helper. */
int wc_PKCS7_PadData(const byte* in, word32 inSz, byte* out, word32 outSz, word32 blockSz)
{
	if (in == NULL || inSz == 0 ||
		out == NULL || outSz == 0)
		return BAD_FUNC_ARG;

	const int padSz = wc_PKCS7_GetPadSize(inSz, blockSz);

	if (outSz < (inSz + padSz))
		return BAD_FUNC_ARG;

	memcpy(out, in, inSz);

	for (int i = 0; i < padSz; ++i)
		out[inSz + i] = (byte)padSz;

	return inSz + padSz;
}
#endif // !PKCS7

namespace Crypto
{
	bool Encrypt(const char* password, word32 keySz, int pbkdfIterationCount, int pbkdfHashAlgo,
		const byte* input, word32 inputSz,
		byte* outSalt, word32 outSaltSz,
		byte* outIV, word32 outIVSz,
		byte* outAuthTag, word32 outAuthTagSz,
		byte** out, word32* outSz)
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
			(wc_PBKDF2(key, (byte*)password, strlen(password), outSalt, outSaltSz, pbkdfIterationCount, keySz, pbkdfHashAlgo) ||
			wc_AesGcmSetKey(&aes, key, keySz));

		free(key);

		if (stretchFailed)
		{
			putsnn("key stretching or setting AES key failed\n");
			return false;
		}

		const word32 paddedSz = inputSz + wc_PKCS7_GetPadSize(inputSz, AES_BLOCK_SIZE);
		byte* padded = (byte*)malloc(paddedSz);
		if (!padded)
		{
			putsnn("padded input allocation failed\n");
			return false;
		}

		if (0 > wc_PKCS7_PadData(input, inputSz, padded, paddedSz, AES_BLOCK_SIZE))
		{
			free(padded);
			putsnn("padding failed\n");
			return false;
		}

		byte* encrypted = (byte*)malloc(paddedSz);
		if (!encrypted)
		{
			free(padded);
			putsnn("result allocation failed\n");
			return false;
		}

		if (wc_AesGcmEncrypt(&aes, encrypted, padded, paddedSz, outIV, outIVSz, outAuthTag, outAuthTagSz, nullptr, 0))
		{
			free(padded);
			free(encrypted);
			putsnn("encryption failed\n");
			return false;
		}

		free(padded);

		*out = encrypted;
		*outSz = paddedSz;

		putsnn("ok\n");
		return true;
	}

	bool Decrypt(const char* password, word32 keySz, int pbkdfIterationCount, int pbkdfHashAlgo,
		const byte* input, size_t inputSz,
		const byte* salt, word32 saltSz,
		const byte* iv, word32 ivSz,
		const byte* authTag, word32 authTagSz,
		byte** out, word32* outSz)
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
			(wc_PBKDF2(key, (byte*)password, strlen(password), salt, saltSz, pbkdfIterationCount, keySz, pbkdfHashAlgo) ||
			wc_AesGcmSetKey(&aes, key, keySz));

		free(key);

		if (stretchFailed)
		{
			putsnn("key stretching or setting AES key failed\n");
			return false;
		}

		byte* plaintext = (byte*)malloc(inputSz);
		if (!plaintext)
		{
			putsnn("plaintext allocation failed\n");
			return false;
		}

		if (wc_AesGcmDecrypt(&aes, plaintext, input, inputSz, iv, ivSz, authTag, authTagSz, nullptr, 0))
		{
			free(plaintext);
			putsnn("wrong password or decryption failed\n");
			return false;
		}

		*out = plaintext;
		*outSz = inputSz - plaintext[inputSz - 1];

		putsnn("ok\n");
		return true;
	}
}