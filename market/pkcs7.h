#pragma once

/* return size of padded data, padded to blockSz chunks, or negative on error */
int wc_PKCS7_GetPadSize(word32 inputSz, word32 blockSz)
{
	int padSz;

	if (blockSz == 0)
		return BAD_FUNC_ARG;

	padSz = blockSz - (inputSz % blockSz);

	return padSz;
}


/* pad input data to blockSz chunk, place in outSz. out must be big enough
 * for input + pad bytes. See wc_PKCS7_GetPadSize() helper. */
int wc_PKCS7_PadData(byte* in, word32 inSz, byte* out, word32 outSz,
	word32 blockSz)
{
	int i, padSz;

	if (in == NULL || inSz == 0 ||
		out == NULL || outSz == 0)
		return BAD_FUNC_ARG;

	padSz = wc_PKCS7_GetPadSize(inSz, blockSz);

	if (outSz < (inSz + padSz))
		return BAD_FUNC_ARG;

	XMEMCPY(out, in, inSz);

	for (i = 0; i < padSz; i++) {
		out[inSz + i] = (byte)padSz;
	}

	return inSz + padSz;
}