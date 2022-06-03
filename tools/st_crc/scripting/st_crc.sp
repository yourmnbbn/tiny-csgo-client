#include <sourcemod>

public Plugin myinfo =
{
    name = "st_crc",
    author = "yourmnbbn",
    description = "Get send table crc value",
    version = "1.0.0",
    url = "https://github.com/yourmnbbn"
};

public void OnPluginStart()
{
    RegServerCmd("sm_stcrc", Command_GetSendTableCRC);
}

public Action Command_GetSendTableCRC(int args)
{
    GameData config = new GameData("st_crc.games");
    if(!config)
    {
        LogError("Can't find gamedata st_crc.games.txt");
        return Plugin_Handled;
    }

    Address g_SendTableCRC = config.GetAddress("g_SendTableCRC");
    if(g_SendTableCRC == Address_Null)
    {
        LogError("Can't get address of g_SendTableCRC");
        return Plugin_Handled;
    }

    int crc = LoadFromAddress(g_SendTableCRC, NumberType_Int32);
    LogMessage("Get sendtable crc 0x%X(%d)", crc, crc);

    delete config;
    return Plugin_Handled;
}
