#pragma once

namespace Crypto
{
	// those 2 funcs are taken from wolfssl, whole pkcs7 is too heavy

	/* return size of padded data, padded to blockSz chunks, or negative on error */
	constexpr int PKCS7_GetPadSize(word32 inputSz, word32 blockSz)
	{
		if (blockSz == 0)
			return BAD_FUNC_ARG;

		return (blockSz - (inputSz % blockSz));
	}

	/* pad input data to blockSz chunk in place. in must be big enough
	 * for content + pad bytes. inLen is data len, inSz is buffer size */
	int PKCS7_PadData(byte* in, word32 inLen, word32 inSz, word32 blockSz)
	{
		if (in == nullptr || inLen == 0)
			return BAD_FUNC_ARG;

		int padSz = PKCS7_GetPadSize(inLen, blockSz);
		if (padSz == BAD_FUNC_ARG)
			return BAD_FUNC_ARG;

		if (inSz < (inLen + padSz))
			return BAD_FUNC_ARG;

		for (int i = 0; i < padSz; ++i)
			in[inLen + i] = (byte)padSz;

		return inLen + padSz;
	}

	// output buffer must be AES_BLOCK_SIZE aligned
	bool Encrypt(const byte* input, word32 inputLen,
		const char* encryptPass, word32 keySize, int pbkdfIterationCount, int pbkdfHashAlgo,
		byte* outSalt, word32 outSaltSize,
		byte* outIV, word32 outIVSize,
		byte* outAuthTag, word32 outAuthTagSize,
		byte* output)
	{
#ifdef _DEBUG
		Log("Encrypting...");
#endif // _DEBUG

		WC_RNG rng;
		if (wc_InitRng(&rng))
		{
#ifdef _DEBUG
			printf("rng init failed\n");
#endif // _DEBUG
			return false;
		}

		bool rngFailed = (wc_RNG_GenerateBlock(&rng, outSalt, outSaltSize) || wc_RNG_GenerateBlock(&rng, outIV, outIVSize));
		wc_FreeRng(&rng);

		if (rngFailed)
		{
#ifdef _DEBUG
			printf("rng generation failed\n");
#endif // _DEBUG
			return false;
		}

		const int paddedInputSize = inputLen + PKCS7_GetPadSize(inputLen, AES_BLOCK_SIZE);

		byte* paddedInput = (byte*)malloc(paddedInputSize);

		byte* keyStretched = (byte*)malloc(keySize);

		if (!paddedInput || !keyStretched)
		{
#ifdef _DEBUG
			printf("allocation failed\n");
#endif // _DEBUG
			return false;
		}

		memcpy(paddedInput, input, inputLen);
		PKCS7_PadData(paddedInput, inputLen, paddedInputSize, AES_BLOCK_SIZE);

		Aes aes;

		if (wc_PBKDF2(keyStretched, (byte*)encryptPass, strlen(encryptPass), outSalt, outSaltSize, pbkdfIterationCount, keySize, pbkdfHashAlgo) ||
			wc_AesGcmSetKey(&aes, keyStretched, keySize) ||
			wc_AesGcmEncrypt(&aes, output, paddedInput, paddedInputSize, outIV, outIVSize, outAuthTag, outAuthTagSize, nullptr, 0))
		{
			free(paddedInput);
			memset(keyStretched, 0, keySize);
			free(keyStretched);

#ifdef _DEBUG
			printf("fail\n");
#endif // _DEBUG
			return false;
		}

		free(paddedInput);
		memset(keyStretched, 0, keySize);
		free(keyStretched);

#ifdef _DEBUG
		printf("ok\n");
#endif // _DEBUG
		return true;
	}

	// output buffer must be AES_BLOCK_SIZE aligned
	bool Decrypt(const byte* input, size_t inputLen,
		const char* decryptPass, word32 keySize, int pbkdfIterationCount, int pbkdfHashAlgo,
		word32 saltSize,
		word32 ivSize,
		word32 authTagSize,
		byte* output)
	{
#ifdef _DEBUG
		Log("Decrypting...");
#endif // _DEBUG

		const byte* salt = input;
		const byte* iv = salt + saltSize;
		const byte* authTag = iv + ivSize;
		const byte* data = authTag + authTagSize;
		const size_t dataSize = inputLen - (data - input);

		Aes aes;

		byte* keyStretched = (byte*)malloc(keySize);
		if (!keyStretched)
		{
#ifdef _DEBUG
			printf("allocation failed\n");
#endif // _DEBUG
			return false;
		}

		if (wc_PBKDF2(keyStretched, (byte*)decryptPass, strlen(decryptPass), salt, saltSize, pbkdfIterationCount, keySize, pbkdfHashAlgo) ||
			wc_AesGcmSetKey(&aes, keyStretched, keySize) ||
			wc_AesGcmDecrypt(&aes, output, data, dataSize, iv, ivSize, authTag, authTagSize, nullptr, 0))
		{
			memset(keyStretched, 0, keySize);
			free(keyStretched);

#ifdef _DEBUG
			printf("fail\n");
#endif // _DEBUG
			return false;
		}

		memset(keyStretched, 0, keySize);
		free(keyStretched);

#ifdef _DEBUG
		printf("ok\n");
#endif // _DEBUG
		return true;
	}
}