# tiny-csgo-client
 Tiny csgo client for connecting dedicated server, this is a very incomplete and experimental project. You have to use at least c++20 to run the code.
 
## Dependencies
 - [hl2sdk-csgo](https://github.com/alliedmodders/hl2sdk/tree/csgo)
 - [Asio](https://github.com/chriskohlhoff/asio)
 - [cstrike15_src](https://github.com/perilouswithadollarsign/cstrike15_src/) (For proto_oob.h only)

## Example Usage
```cpp
#include "client.hpp"

int main(int argc, char** argv)
{
	Client cl;
	if(!cl.PrepareSteamAPI())
		return 0;
 
	cl.BindServer("127.0.0.1", "yourmnbbn", "", 27015);
	cl.RunClient();
 
	return 0;
}

```

## Credit
Thanks to [Leystryku](https://github.com/Leystryku), [his project](https://github.com/Leystryku/leysourceengineclient) has made a great example.
