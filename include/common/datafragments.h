#ifndef __TINY_CSGO_CLIENT_DATAFRAGMENTS__
#define __TINY_CSGO_CLIENT_DATAFRAGMENTS__

#ifdef _WIN32
#pragma once
#endif

typedef void* FileHandle_t;

inline constexpr int MAX_STREAMS = 2;
inline constexpr int MAX_OSPATH = 260;
inline constexpr int MAX_FILE_SIZE_BITS = 26;
inline constexpr int FRAGMENT_BITS = 8;
inline constexpr int FRAGMENT_SIZE = (1 << FRAGMENT_BITS);
inline constexpr int NET_MAX_PAYLOAD_BITS = 18;
inline constexpr int MAX_FILE_SIZE = ((1 << MAX_FILE_SIZE_BITS) - 1);
inline constexpr auto FILESYSTEM_INVALID_HANDLE = (FileHandle_t)nullptr;

typedef struct dataFragments_s
{
	FileHandle_t	file;			// open file handle
	char			filename[MAX_OSPATH]; // filename
	char* buffer;			// if NULL it's a file
	unsigned int	bytes;			// size in bytes
	unsigned int	bits;			// size in bits
	unsigned int	transferID;		// only for files
	bool			isCompressed;	// true if data is bzip compressed
	unsigned int	nUncompressedSize; // full size in bytes
	bool			asTCP;			// send as TCP stream
	bool			isReplayDemo;	// if it's a file, is it a replay .dem file?
	int				numFragments;	// number of total fragments
	int				ackedFragments; // number of fragments send & acknowledged
	int				pendingFragments; // number of fragments send, but not acknowledged yet
} dataFragments_t;

#endif // !__TINY_CSGO_CLIENT_DATAFRAGMENTS__

