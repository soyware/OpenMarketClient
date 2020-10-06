#pragma once

/* return size of padded data, padded to blockSz chunks, or negative on error */
constexpr int _PKCS7_GetPadSize(word32 inputSz, word32 blockSz)
{
	if (blockSz == 0)
		return BAD_FUNC_ARG;

	return (blockSz - (inputSz % blockSz));
}


/* pad input data to blockSz chunk in place. in must be big enough
 * for content + pad bytes. inLen is data len, inSz is buffer size */
int _PKCS7_PadData(byte* in, word32 inLen, word32 inSz, word32 blockSz)
{
	if (in == nullptr || inLen == 0)
		return BAD_FUNC_ARG;

	int padSz = _PKCS7_GetPadSize(inLen, blockSz);
	if (padSz == BAD_FUNC_ARG)
		return BAD_FUNC_ARG;

	if (inSz < (inLen + padSz))
		return BAD_FUNC_ARG;

	for (int i = 0; i < padSz; ++i)
		in[inLen + i] = (byte)padSz;

	return inLen + padSz;
}
