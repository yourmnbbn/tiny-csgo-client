# tiny-csgo-client
 Tiny csgo client for connecting dedicated server, this is a very incomplete and experimental project. But it can esatblish connections to server and stay alive as a real player in server. You have to use at least c++20 to run the code.
 
## Dependencies
 - [hl2sdk-csgo](https://github.com/alliedmodders/hl2sdk/tree/6eb8f5b2f6cbc57ddd19d1646c7ee1266d2a1ad0) <-- Make sure to use this instead of the latest.
 - [Asio](https://github.com/chriskohlhoff/asio) 
 - CMake

## Compile and Run 
### Windows
1. Configure path of hl2sdk-csgo and Asio in `build.bat`.
2. Run `build.bat` to compile the project.
3. Move every file in bin/windows next to `tiny-csgo-client.exe`.
4. Run `tiny-csgo-client.exe` with necessary commandline.

### Linux
1. Configure path of hl2sdk-csgo and Asio in `build.sh`.
2. Run `build.sh` to compile the project.
3. Move every file in bin/linux next to `tiny-csgo-client`.
4. Set the directory constains your executable to LD_LIBRARY_PATH.
5. Run `tiny-csgo-client` with necessary commandline.


## Credit
Thanks to [Leystryku](https://github.com/Leystryku), [his project](https://github.com/Leystryku/leysourceengineclient) has made a great example.

