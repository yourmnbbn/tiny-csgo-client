# Ticket Generating Tool
This tool is used to get the steam auth session ticket of an app. You can use tiny-csgo-client without steamapi by using command line `-ticket`. And you can probably run multiple client instances at the same time using ticket to connect.

## Compile and run
1. Configure directory of hl2sdk-csgo in build.bat.
2. Run build.bat.
3. Move `steam_appid.txt` and `steam_api.dll` next to the executable.
4. Run ticketgen.exe.
5. The ticket will be shown in the console output, and there will be an output file in the same folder. 
