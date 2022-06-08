#ifndef __TINY_CSGO_CLIENT_USERMSGHANDLER_HPP__
#define __TINY_CSGO_CLIENT_USERMSGHANDLER_HPP__

#ifdef _WIN32
#pragma once
#endif

#include "cstrike15_usermessages.pb.h"
#include "../common/bitbuf.h"

class CUserMsgHandler
{
public:
	static bool HandleUserMessage(int msgType, const void* data, size_t length);

private:
	template<typename MsgObjType>
	static MsgObjType usermsg_cast(const void* data, size_t length);
};

template<typename MsgObjType>
MsgObjType CUserMsgHandler::usermsg_cast(const void* data, size_t length)
{
	MsgObjType obj;
	obj.ParseFromArray(data, length);
	return obj;
}

bool CUserMsgHandler::HandleUserMessage(int msgType, const void* data, size_t length)
{
	//So far we just handle some of the common seen usermessages in the tiny client
	switch (msgType)
	{
	case CS_UM_VGUIMenu:
	{
		auto guiMenu = usermsg_cast<CCSUsrMsg_VGUIMenu>(data, length);
		printf("Receive  CCSUsrMsg_VGUIMenu, menu name:%s \n", guiMenu.name().c_str());
		for (int i = 0; i < guiMenu.subkeys().size(); i++)
		{
			auto subKey = guiMenu.subkeys().Get(i);
			printf("Subkey<%i>: %s\n", i, subKey.name().c_str());
		}
		return true;
	}
	case CS_UM_SayText:
	{
		auto sayText = usermsg_cast<CCSUsrMsg_SayText>(data, length);
		int client = sayText.ent_idx();
		printf("SayText: <Client %d> %s\n", client, sayText.text().c_str());
		return true;
	}
	case CS_UM_SayText2:
	{
		auto sayText2 = usermsg_cast<CCSUsrMsg_SayText2>(data, length);
		int client = sayText2.ent_idx();
		printf("SayText2 : name: %s %s<Client %d>\n", 
			sayText2.msg_name().c_str(), sayText2.params(0).c_str(), client);
		return true;
	}
	case CS_UM_TextMsg:
	{
		auto textMsg = usermsg_cast<CCSUsrMsg_TextMsg>(data, length);
		printf("Receive CCSUsrMsg_TextMsg\n");
		for (int i = 0; i < 5; ++i)
		{
			if(textMsg.params(i).c_str()[0])
				printf("%s\n", textMsg.params(i).c_str());
		}
		return true;
	}
	case CS_UM_ShowMenu: //For handling sourcemod menu
	{
		auto menu = usermsg_cast<CCSUsrMsg_ShowMenu>(data, length);
		int displayTime = menu.display_time();
		int bitsValidSlots = menu.bits_valid_slots();
		
		if (bitsValidSlots)
		{
			printf("ShowMenu<time: %ds> \n%s\n", displayTime, menu.menu_string().c_str());
		}

	}
	case CS_UM_ProcessSpottedEntityUpdate:
	{
		//Do nothing
		return true;
	}
	default: return false;
	}
}

#endif // !__TINY_CSGO_CLIENT_USERMSGHANDLER_HPP__
