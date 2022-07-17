#ifndef __TINY_CSGO_CLIENT_SPLITMESSAGE_HPP__
#define __TINY_CSGO_CLIENT_SPLITMESSAGE_HPP__

#ifdef _WIN32
#pragma once
#endif

#include "netmessages.h"
#include "../common/bitbuf.h"

#define PAD_NUMBER(number, boundary) \
	( ((number) + ((boundary)-1)) / (boundary) ) * (boundary)

#define HEADER_BYTES	9
#define	NET_MAX_MESSAGE	PAD_NUMBER( ( NET_MAX_PAYLOAD + HEADER_BYTES ), 16 )

// Split long packets.  Anything over 1460 is failing on some routers
typedef struct
{
	int		currentSequence;
	int		splitCount;
	int		totalSize;
	int		nExpectedSplitSize;
	char	buffer[NET_MAX_MESSAGE];	// This has to be big enough to hold the largest message
} LONGPACKET;

// Use this to pick apart the network stream, must be packed
#pragma pack(1)
typedef struct
{
	int		netID;
	int		sequenceNumber;
	int		packetID : 16;
	int		nSplitSize : 16;
} SPLITPACKET;
#pragma pack()

#define MAX_ROUTABLE_PAYLOAD		1200
#define MIN_USER_MAXROUTABLE_SIZE	576  // ( X.25 Networks )
#define MAX_USER_MAXROUTABLE_SIZE	MAX_ROUTABLE_PAYLOAD

#define MAX_SPLIT_SIZE	(MAX_USER_MAXROUTABLE_SIZE - sizeof( SPLITPACKET ))
#define MIN_SPLIT_SIZE	(MIN_USER_MAXROUTABLE_SIZE - sizeof( SPLITPACKET ))

// Calculate MAX_SPLITPACKET_SPLITS according to the smallest split size
#define MAX_SPLITPACKET_SPLITS ( NET_MAX_MESSAGE / MIN_SPLIT_SIZE )
#define SPLIT_PACKET_STALE_TIME		15.0f

class CSplitPacketEntry
{
public:
	CSplitPacketEntry()
	{
		int i;
		for (i = 0; i < MAX_SPLITPACKET_SPLITS; i++)
		{
			splitflags[i] = -1;
		}

		memset(&netsplit, 0, sizeof(netsplit));
		lastactivetime = 0.0f;
	}

public:
	int				splitflags[MAX_SPLITPACKET_SPLITS];
	LONGPACKET		netsplit;
	// host_time the last time any entry was received for this entry
	float			lastactivetime;
};

inline CSplitPacketEntry g_splitEntry;

bool NET_GetLong(bf_read& msg, size_t& packetSize)
{
	int				packetNumber, packetCount, sequenceNumber, offset;
	short			packetID;
	SPLITPACKET*	pHeader;

	if (packetSize < sizeof(SPLITPACKET))
	{
		printf("Invalid split packet length %i\n", packetSize);
		return false;
	}

	CSplitPacketEntry* entry = &g_splitEntry;
	//entry->lastactivetime = net_time;
	pHeader = (SPLITPACKET*)msg.GetBasePointer();
	// pHeader is network endian correct
	sequenceNumber = LittleLong(pHeader->sequenceNumber);
	packetID = LittleShort((short)pHeader->packetID);
	// High byte is packet number
	packetNumber = (packetID >> 8);
	// Low byte is number of total packets
	packetCount = (packetID & 0xff);

	int nSplitSizeMinusHeader = (int)LittleShort((short)pHeader->nSplitSize);
	if (nSplitSizeMinusHeader < MIN_SPLIT_SIZE ||
		nSplitSizeMinusHeader > MAX_SPLIT_SIZE)
	{
		printf("NET_GetLong:  Split packet with invalid split size (number %i/ count %i) where size %i is out of valid range [%d - %d ]\n",
			packetNumber,
			packetCount,
			nSplitSizeMinusHeader,
			MIN_SPLIT_SIZE,
			MAX_SPLIT_SIZE);
		return false;
	}

	if (packetNumber >= MAX_SPLITPACKET_SPLITS ||
		packetCount > MAX_SPLITPACKET_SPLITS)
	{
		printf("NET_GetLong:  Split packet with too many split parts (number %i/ count %i) where %i is max count allowed\n",
			packetNumber,
			packetCount,
			MAX_SPLITPACKET_SPLITS);
		return false;
	}

	// First packet in split series?
	if (entry->netsplit.currentSequence == -1 ||
		sequenceNumber != entry->netsplit.currentSequence)
	{
		entry->netsplit.currentSequence = sequenceNumber;
		entry->netsplit.splitCount = packetCount;
		entry->netsplit.nExpectedSplitSize = nSplitSizeMinusHeader;
	}

	if (entry->netsplit.nExpectedSplitSize != nSplitSizeMinusHeader)
	{
		printf("NET_GetLong:  Split packet with inconsistent split size (number %i/ count %i) where size %i not equal to initial size of %i\n",
			packetNumber,
			packetCount,
			nSplitSizeMinusHeader,
			entry->netsplit.nExpectedSplitSize
		);
		return false;
	}

	int size = packetSize - sizeof(SPLITPACKET);

	if (entry->splitflags[packetNumber] != sequenceNumber)
	{
		// Last packet in sequence? set size
		if (packetNumber == (packetCount - 1))
		{
			entry->netsplit.totalSize = (packetCount - 1) * nSplitSizeMinusHeader + size;
		}

		entry->netsplit.splitCount--;		// Count packet
		entry->splitflags[packetNumber] = sequenceNumber;

		printf("<-- Split packet %4i/%4i seq %5i size %4i mtu %4i\n",
			packetNumber + 1,
			packetCount,
			sequenceNumber,
			size,
			nSplitSizeMinusHeader + sizeof(SPLITPACKET));
	}
	else
	{
		printf("NET_GetLong:  Ignoring duplicated split packet %i of %i ( %i bytes )\n", packetNumber + 1, packetCount, size);
	}


	// Copy the incoming data to the appropriate place in the buffer
	offset = (packetNumber * nSplitSizeMinusHeader);
	memcpy(entry->netsplit.buffer + offset, msg.GetBasePointer() + sizeof(SPLITPACKET), size);

	// Have we received all of the pieces to the packet?
	if (entry->netsplit.splitCount <= 0)
	{
		entry->netsplit.currentSequence = -1;	// Clear packet
		if (entry->netsplit.totalSize > sizeof(entry->netsplit.buffer))
		{
			printf("Split packet too large! %d bytes\n", entry->netsplit.totalSize);
			return false;
		}

		memcpy((void*)msg.GetBasePointer(), entry->netsplit.buffer, entry->netsplit.totalSize);
		packetSize = entry->netsplit.totalSize;
		return true;
	}

	return false;
}

#endif // !__TINY_CSGO_CLIENT_SPLITMESSAGE_HPP__
