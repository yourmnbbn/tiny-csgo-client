#include <client.hpp>
#include <argparser.hpp>

int main(int argc, char** argv)
{
    ArgParser parser;

    parser.AddOption("-ip", "IPv4 address of the server.", OptionAttr::RequiredWithValue, OptionValueType::STRING);
    parser.AddOption("-port", "Game server port.", OptionAttr::RequiredWithValue, OptionValueType::INT16U);
    parser.AddOption("-pw", "Password of the server", OptionAttr::OptionalWithValue, OptionValueType::STRING);
    parser.AddOption("-name", "Nick name of you.", OptionAttr::RequiredWithValue, OptionValueType::STRING);
    parser.AddOption("-cport", "Client port", OptionAttr::OptionalWithValue, OptionValueType::INT16U);
    parser.AddOption("-ticket", "Auth session ticket from steam", OptionAttr::OptionalWithValue, OptionValueType::STRING);
    parser.AddOption("-ts", "Connect to tiny csgo server", OptionAttr::OptionalWithoutValue);

    try
    {
        parser.ParseArgument(argc, argv);
    }
    catch (const std::exception& e)
    {
        printf("%s\n", e.what());
        return -1;
    }
    
    Client cl(parser);
    if (!cl.PrepareSteamAPI())
        return -1;
    
    cl.InitCommandInput();
    cl.BindServer(parser.GetOptionValueString("-ip"), 
        parser.GetOptionValueString("-name"), 
        parser.GetOptionValueString("-pw"),
        parser.GetOptionValueInt16U("-port"),
        parser.HasOption("-cport") ? parser.GetOptionValueInt16U("-cport") : 27015);
    cl.RunClient();

    return 0;
}