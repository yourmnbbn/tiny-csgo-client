# tiny-csgo-client-limited
 Tiny csgo client (Limited) only focus on keeping alive connection and it doesn't require any other binary dependencies to run the executable. You can run multiple instances of the client at the same time with this limited version. **Note that you need to use ticketgen to generate valid tickets ahead of time and pass it to the client using command line -ticket**, generating tickets requires a valid steam account login.
 
 - **The generated ticket is only valid when ticketgen tools running, when that process being terminated, the ticket is automatically cancelled and it's nolonger valid.**
 
## Dependencies
 - [hl2sdk-csgo](https://github.com/alliedmodders/hl2sdk/tree/6eb8f5b2f6cbc57ddd19d1646c7ee1266d2a1ad0) <-- Make sure to use this instead of the latest.
 - [Asio](https://github.com/chriskohlhoff/asio) 
 - CMake

## Compile and Run 
1. Configure path of hl2sdk-csgo and Asio in `build.(bat/sh)`.
2. Run `build.(bat/sh)` to compile the project.
3. Run `tiny-csgo-client-limited(.exe)` with necessary commandline.


