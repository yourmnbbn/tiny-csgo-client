#include <steam_api.h>
#include <fstream>

#define STEAM_KEYSIZE 2048

int main(int argc, char** argv)
{
    if (!SteamAPI_IsSteamRunning())
	{
		printf("Steam is not running!\n");
		getchar();
        return -1;
	}

	if (!SteamAPI_Init())
	{
		printf("Cannot initialize SteamAPI\n");
		getchar();
        return -1;
	}

	if (!SteamUser())
	{
		printf("Cannot initialize ISteamUser interface\n");
		getchar();
        return -1;
	}

    char ticket[STEAM_KEYSIZE];
    char filename[256];
    unsigned int keysize = 0;

    SteamUser()->GetAuthSessionTicket(ticket, STEAM_KEYSIZE, &keysize);
    snprintf(filename, sizeof(filename), "%llu_windows.txt", SteamUser()->GetSteamID().ConvertToUint64());

    std::ofstream file_out(filename, std::ofstream::out);
    if (file_out.bad() || !file_out.is_open())
        return -1;

    printf("The Auth Ticket is as follow:\n\n");
    printf("windows command line:\n-ticket ");
    char temp[8];
    for (size_t i = 0; i < keysize; i++)
    {
        printf("\\x%02X", ticket[i] & 0xFF);
        snprintf(temp, sizeof(temp), "\\x%02X", ticket[i] & 0xFF);
        file_out << temp;
    }

    file_out.close();

    printf("\nLinux command line:\n-ticket ");
    snprintf(filename, sizeof(filename), "%llu_linux.txt", SteamUser()->GetSteamID().ConvertToUint64());

    file_out.open(filename, std::ofstream::out);
    if (file_out.bad() || !file_out.is_open())
        return -1;
    
    for (size_t i = 0; i < keysize; i++)
    {
        printf("\\\\x%02X", ticket[i] & 0xFF);
        snprintf(temp, sizeof(temp), "\\\\x%02X", ticket[i] & 0xFF);
        file_out << temp;
    }

    file_out.close();

    getchar();
    return 0;
}
