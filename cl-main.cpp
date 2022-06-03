#include <client.hpp>

int main(int argc, char** argv)
{
    Client cl;
    if (!cl.PrepareSteamAPI())
        return 0;
    
    cl.InitCommandInput();
    cl.BindServer("127.0.0.1", "yourmnbbn", "passwd", 27015);
    cl.RunClient();
}