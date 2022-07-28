#ifndef __TINY_CSGO_CLIENT_GCCLIENT_HPP__
#define __TINY_CSGO_CLIENT_GCCLIENT_HPP__

#ifdef _WIN32
#pragma once
#endif

#include <steam_api.h>
#include <steam/isteamgamecoordinator.h>
#include "netmessage/supplement.pb.h"

using namespace std::chrono_literals;

enum EGCMsgSupplement
{
	k_EMsgGCClientWelcome =						4004, // GC => client
	k_EMsgGCServerWelcome =						4005, // GC => server
	k_EMsgGCClientHello =						4006, // client => GC
	k_EMsgGCServerHello =						4007, // server => GC
	k_EMsgGCClientConnectionStatus =			4009, // GC => client
	k_EMsgGCServerConnectionStatus =			4010, // GC => server
};

struct GCMsgHdr_t
{
	uint32	m_eMsg;					// The message type
	uint32	m_nSrcGCDirIndex;		// The GC index that this message was sent from (set to the same as the current GC if not routed through another GC)
	uint64	m_ulSteamID;			// User's SteamID
};

class GCClient 
{
public:
	void Init();
	void SendHello();
	bool SendMessageToGC(uint32_t type, google::protobuf::Message& msg);

private:
	void ProcessWelcomeMessage(char* pData, size_t length);
	void OnGCMessageAvailable(uint32_t msgSize);

private:
	ISteamGameCoordinator* m_pGameCoordinator = nullptr;
};

inline constexpr auto PROTO_FLAG = (1 << 31);
inline GCClient g_GCClient;


void GCClient::Init()
{
	m_pGameCoordinator = (ISteamGameCoordinator*)SteamClient()->GetISteamGenericInterface(SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMGAMECOORDINATOR_INTERFACE_VERSION);
	std::thread([this]() {
		while (true)
		{
			uint32_t size;
			while (m_pGameCoordinator->IsMessageAvailable(&size))
			{
				OnGCMessageAvailable(size);
			}
			std::this_thread::sleep_for(200ms);
		}
		}).detach();
}

void GCClient::SendHello()
{
	CMsgClientHello hello;
	hello.set_client_session_need(1);

	if (!SendMessageToGC(k_EMsgGCClientHello, hello))
	{
		printf("Failed to send Hello to GC\n");
	}
}

bool GCClient::SendMessageToGC(uint32_t type, google::protobuf::Message& msg)
{
	auto size = msg.ByteSize() + sizeof(GCMsgHdr_t);
	std::unique_ptr<char[]> memBlock = std::make_unique<char[]>(size);

	type |= PROTO_FLAG;

	GCMsgHdr_t* header = (GCMsgHdr_t*)memBlock.get();
	header->m_eMsg = type;
	header->m_nSrcGCDirIndex = 0;
	header->m_ulSteamID = SteamUser()->GetSteamID().ConvertToUint64();

	msg.SerializeToArray(memBlock.get() + sizeof(GCMsgHdr_t), size - sizeof(GCMsgHdr_t));
	return m_pGameCoordinator->SendMessageA(type, memBlock.get(), size) == k_EGCResultOK;
}

void GCClient::ProcessWelcomeMessage(char* pData, size_t length)
{
	printf("GC Connection established for client\n");
	//TO DO: Parse CMsgClientWelcome here
}

void GCClient::OnGCMessageAvailable(uint32_t msgSize)
{
	std::unique_ptr<char[]> memBlock = std::make_unique<char[]>(msgSize);
	uint32_t msgType;

	auto result = m_pGameCoordinator->RetrieveMessage(&msgType, memBlock.get(), msgSize, &msgSize);
	if (result != k_EGCResultOK)
	{
		printf("GCMessage %d failed to retrive, error %d\n", msgType & 0xFFFF, result);
		return;
	}

	if (!(msgType & PROTO_FLAG))
		return;

	msgType &= 0xFFFF;
	if (msgType == k_EMsgGCClientWelcome)
	{
		ProcessWelcomeMessage(memBlock.get(), msgSize);
	}
	else
	{
		printf("Received GC message type %d, size %d\n", msgType, msgSize);
		//TODO: Handle other GC messages here...
	}
}

#endif // !__TINY_CSGO_CLIENT_GCCLIENT_HPP__
