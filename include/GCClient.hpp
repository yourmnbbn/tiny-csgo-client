#ifndef __TINY_CSGO_CLIENT_GCCLIENT_HPP__
#define __TINY_CSGO_CLIENT_GCCLIENT_HPP__

#ifdef _WIN32
#pragma once
#endif

#include <steam_api.h>
#include <steam/isteamgamecoordinator.h>
#include "netmessage/supplement.pb.h"
#include "netmessage/cstrike15_gcmessages.pb.h"

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
};

class GCClient 
{
public:
	//We have to wait for the GC's response of the server reservation id, so this has to be running synchronously
	void		Init(bool async = false);
	void		SendHello();
	uint64_t	GetServerReservationId(uint64_t serverid, uint32_t serverip, uint16_t serverport, uint32_t version);
	bool		SendMessageToGC(uint32_t type, google::protobuf::Message& msg);

private:
	void ProcessWelcomeMessage(char* pData, size_t length);
	void OnGCMessageAvailable(uint32_t msgSize);

private:
	ISteamGameCoordinator*	m_pGameCoordinator = nullptr;
	bool					m_AsyncReceive = false;
};

inline constexpr auto PROTO_FLAG = (1 << 31);
inline GCClient g_GCClient;


void GCClient::Init(bool async)
{
	m_pGameCoordinator = (ISteamGameCoordinator*)SteamClient()->GetISteamGenericInterface(SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMGAMECOORDINATOR_INTERFACE_VERSION);
	
	if (async)
	{
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

	m_AsyncReceive = async;
}

void GCClient::SendHello()
{
	CMsgClientHello hello;
	hello.set_client_session_need(1);

	if (!SendMessageToGC(k_EMsgGCClientHello, hello))
	{
		printf("Failed to send Hello to GC\n");
	}

	if (!m_AsyncReceive)
	{
		printf("Trying to establish connection with GC, this may take a while...\n");

		uint32_t size;
		uint32_t msgType;
		uint32_t retry = 0;
		bool success = false;
		while (true)
		{
			while (m_pGameCoordinator->IsMessageAvailable(&size))
			{
				std::unique_ptr<char[]> memBlock = std::make_unique<char[]>(size);
				auto eResult = m_pGameCoordinator->RetrieveMessage(&msgType, memBlock.get(), size, &size);
				if (eResult != k_EGCResultOK)
				{
				    printf("Error RetrieveGCMessage(%d): %d\n", msgType && 0xFFFF, eResult);
					return;
				}

				if (!(msgType & PROTO_FLAG))
					continue;

				msgType &= 0xFFFF;
				if (msgType == k_EMsgGCClientWelcome)
				{
					printf("GC Connection established for client\n");
					success = true;
					break;
				}
			}
			if (success)
				break;

			//Resend hello to GC until we receive welcome message
			if (!SendMessageToGC(k_EMsgGCClientHello, hello))
				printf("Failed to send Hello to GC\n");

			retry++;
			if (retry > 120) //About 60s
			{
				printf("Can't establish connection with GC after waiting for 60s, can't get reservation id of server, connection maybe rejected by server!\n");
				break;
			}
			std::this_thread::sleep_for(500ms);
		}
	}
}

uint64_t GCClient::GetServerReservationId(uint64_t serverid, uint32_t serverip, uint16_t serverport, uint32_t version)
{
	uint32_t size;
	uint32_t msgType;
	uint32_t retry = 0;

	//Incase we have unhandled message, clear the cache
	if (!m_AsyncReceive)
	{
		while (m_pGameCoordinator->IsMessageAvailable(&size))
		{
			std::unique_ptr<char[]> memBlock = std::make_unique<char[]>(size);
			m_pGameCoordinator->RetrieveMessage(&msgType, memBlock.get(), size, &size);
		}
	}

	CMsgGCCStrike15_v2_ClientRequestJoinServerData msg;
	msg.set_account_id(SteamUser()->GetSteamID().GetAccountID());
	msg.set_serverid(serverid);
	msg.set_server_ip(serverip);
	msg.set_server_port(serverport);
	msg.set_version(version);
	SendMessageToGC(k_EMsgGCCStrike15_v2_ClientRequestJoinServerData, msg);

	if (m_AsyncReceive)
		return 0;

	printf("Trying to request server reservation id from GC, this may take a while...\n");
	while (true)
	{
		while (m_pGameCoordinator->IsMessageAvailable(&size))
		{
			std::unique_ptr<char[]> memBlock = std::make_unique<char[]>(size);
			auto eResult = m_pGameCoordinator->RetrieveMessage(&msgType, memBlock.get(), size, &size);
			if (eResult != k_EGCResultOK)
			{
				printf("Error RetrieveGCMessage(%d): %d\n", msgType && 0xFFFF, eResult);
				return 0;
			}

			if (!(msgType & PROTO_FLAG))
				continue;

			msgType &= 0xFFFF;
			if (msgType == k_EMsgGCCStrike15_v2_ClientRequestJoinServerData)
			{
				CMsgGCCStrike15_v2_ClientRequestJoinServerData response;
				response.ParseFromArray(memBlock.get() + sizeof(GCMsgHdr_t), size - sizeof(GCMsgHdr_t));
				auto reservationid = response.res().reservationid();
				printf("Get server reservation id 0x%llX\n", reservationid);
				return reservationid;
			}
		}

		//Resend request to GC until we receive welcome message
		if (!SendMessageToGC(k_EMsgGCCStrike15_v2_ClientRequestJoinServerData, msg))
			printf("Failed to send ClientRequestJoinServerData to GC\n");

		retry++;
		if (retry > 120) //About 60s
		{
			printf("Can't receive response from GC, getting reservation id of server failed, connection maybe rejected by server!\n");
			return 0;
		}
		std::this_thread::sleep_for(500ms);
	}
}

bool GCClient::SendMessageToGC(uint32_t type, google::protobuf::Message& msg)
{
	auto size = msg.ByteSize() + sizeof(GCMsgHdr_t);
	std::unique_ptr<char[]> memBlock = std::make_unique<char[]>(size);

	type |= PROTO_FLAG;

	GCMsgHdr_t* header = (GCMsgHdr_t*)memBlock.get();
	header->m_eMsg = type;
	header->m_nSrcGCDirIndex = GCProtoBufMsgSrc_Unspecified;

	msg.SerializeToArray(memBlock.get() + sizeof(GCMsgHdr_t), size - sizeof(GCMsgHdr_t));
	return m_pGameCoordinator->SendMessage(type, memBlock.get(), size) == k_EGCResultOK;
}

void GCClient::ProcessWelcomeMessage(char* pData, size_t length)
{
	printf("GC Connection established for client\n");
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
	}
}

#endif // !__TINY_CSGO_CLIENT_GCCLIENT_HPP__
