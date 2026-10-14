#include "../../client.h"
#include "../../worldserver.h"

extern WorldServer worldserver;

// #show queue: ask world for the login-queue report; world answers with emote messages to this character.
void ShowQueue(Client* c, const Seperator* sep)
{
	auto pack = new ServerPacket(ServerOP_QueueStatus, sizeof(ServerQueueStatus_Struct));

	auto s = (ServerQueueStatus_Struct*)pack->pBuffer;
	strn0cpy(s->adminname, c->GetName(), sizeof(s->adminname));

	worldserver.SendPacket(pack);
	safe_delete(pack);
}
