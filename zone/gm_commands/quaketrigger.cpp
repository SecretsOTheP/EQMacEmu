#include "../client.h"
#include "../worldserver.h"
extern WorldServer worldserver;

void command_quaketrigger(Client* c, const Seperator* sep)
{
	//Arguments?
	if (sep->argnum == 1 && sep->IsNumber(1) && atoi(sep->arg[1]) > QuakeDisabled && atoi(sep->arg[1]) < QuakeMax)
	{
		uint8_t quaketype = atoi(sep->arg[1]);
		auto pack = new ServerPacket(ServerOP_QuakeRequest, sizeof(ServerEarthquakeRequest_Struct));
		ServerEarthquakeRequest_Struct* sqr = (ServerEarthquakeRequest_Struct*)pack->pBuffer;
		sqr->type = (QuakeType)quaketype;
		worldserver.SendPacket(pack);
		safe_delete(pack);
		c->Message(15, "Earthquake request sent to world; a world announcement confirms success.");
	}
	else
	{
		c->Message(15, "Invalid parameters. Usage: #quaketrigger 1|2");
	}
}

