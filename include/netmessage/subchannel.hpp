#ifndef __TINY_CSGO_CLIENT_SUBCHANNEL_HPP__
#define __TINY_CSGO_CLIENT_SUBCHANNEL_HPP__

#ifdef _WIN32
#pragma once
#endif

#include "../common/datafragments.h"

#define FLIPBIT(v,b) if (v&b) v &= ~b; else v |= b;

#define NET_FRAMES_BACKUP	128		// must be power of 2
#define NET_FRAMES_MASK		(NET_FRAMES_BACKUP-1)
#define MAX_SUBCHANNELS		8		// we have 8 alternative send&wait bits

#define SUBCHANNEL_FREE		0	// subchannel is free to use
#define SUBCHANNEL_TOSEND	1	// subchannel has data, but not send yet
#define SUBCHANNEL_WAITING	2   // sbuchannel sent data, waiting for ACK
#define SUBCHANNEL_DIRTY	3	// subchannel is marked as dirty during changelevel

struct subChannel_s
{
	int				startFraggment[MAX_STREAMS];
	int				numFragments[MAX_STREAMS];
	int				sendSeqNr;
	int				state; // 0 = free, 1 = scheduled to send, 2 = send & waiting, 3 = dirty
	int				index; // index in m_SubChannels[]

	void Free()
	{
		state = SUBCHANNEL_FREE;
		sendSeqNr = -1;
		for (int i = 0; i < MAX_STREAMS; i++)
		{
			numFragments[i] = 0;
			startFraggment[i] = -1;
		}
	}
};

#endif // !__TINY_CSGO_CLIENT_SUBCHANNEL_HPP__
