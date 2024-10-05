/*
	Copyright (C) 2005 Michael S. Finger

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "global_define.h"
#include "eqemu_logsys.h"
#include "eq_packet.h"
#include "eq_stream.h"
#include "op_codes.h"
#include "crc16.h"
#include "platform.h"
#include "strings.h"
#include "rulesys.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>

#ifdef _WINDOWS
	#include <time.h>
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <sys/time.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include <fcntl.h>
	#include <arpa/inet.h>
#endif

//for logsys
#define _L "[{}]:[{}]: "
#define __L , long2ip(remote_ip).c_str(), ntohs(remote_port)

uint16 EQStream::MaxWindowSize=2048;

void EQStream::init(bool resetSession) {
	// we only reset these statistics if it is a 'new' connection
	if (resetSession)
	{
		streamactive = false;
		sessionAttempts = 0;
	}
	active_users = 0;
	Session=0;
	Key=0;
	MaxLen=0;
	NextInSeq=0;
	NextOutSeq=0;
	NextAckToSend=-1;
	LastAckSent=-1;
	MaxSends=5;
	LastPacket=0;
	LastSent = 0;
	oversize_buffer=nullptr;
	oversize_length=0;
	oversize_offset=0;
	RateThreshold=RATEBASE/250;
	DecayRate=DECAYBASE/250;
	BytesWritten=0;
	SequencedBase = 0;
	stream_startup = true;
	NextSequencedSend = 0;

	if(GetExecutablePlatform() == ExePlatformWorld || GetExecutablePlatform() == ExePlatformZone || GetExecutablePlatform() == ExePlatformUCS) {
		retransmittimer = Timer::GetCurrentTime();
		retransmittimeout = 500 * RETRANSMIT_TIMEOUT_MULT;
	}

	OpMgr = nullptr;
	if(uint16(SequencedBase + SequencedQueue.size()) != NextOutSeq) {
		LogNetcodeDetail(_L "init Invalid Sequenced queue: BS [{}] + SQ [{}] != NOS [{}]" __L, SequencedBase, SequencedQueue.size(), NextOutSeq);
	}
	
	if(NextSequencedSend > (SequencedBase + SequencedQueue.size())) {
		LogNetcodeDetail(_L "init Next Send Sequence is beyond the end of the queue NSS [{}] > SQ [{}]" __L, NextSequencedSend, SequencedQueue.size());
	}
}

EQRawApplicationPacket *EQStream::MakeApplicationPacket(EQProtocolPacket *p)
{
	EQRawApplicationPacket *ap=nullptr;
	LogNetcodeDetail(_L "Creating new application packet, length {}" __L, p->size);
	// _raw(NET__APP_CREATE_HEX, 0xFFFF, p);
	ap = p->MakeAppPacket();
	return ap;
}

EQRawApplicationPacket *EQStream::MakeApplicationPacket(const unsigned char *buf, uint32 len)
{
	EQRawApplicationPacket *ap=nullptr;
	LogNetcodeDetail(_L "Creating new application packet, length {}" __L, len);
	ap = new EQRawApplicationPacket(buf, len);
	return ap;
}

EQProtocolPacket *EQStream::MakeProtocolPacket(const unsigned char *buf, uint32 len) {
	uint16 proto_opcode = ntohs(*(const uint16 *)buf);

	//advance over opcode.
	buf += 2;
	len -= 2;

	return(new EQProtocolPacket(proto_opcode, buf, len));
}

void EQStream::ProcessPacket(EQProtocolPacket *p)
{
	uint32 processed=0, subpacket_length=0;
	if (p == nullptr)
		return;
	// Raw Application packet
	if (p->opcode > 0xff) {
		p->opcode = htons(p->opcode); //byte order is backwards in the protocol packet
		EQRawApplicationPacket *ap=MakeApplicationPacket(p);
		if (ap)
			InboundQueuePush(ap);
		return;
	}

	if (!Session && p->opcode!=OP_SessionRequest && p->opcode!=OP_SessionResponse) {
		LogNetcodeDetail(_L "Session not initialized, packet ignored" __L);
		// _raw(NET__DEBUG, 0xFFFF, p);
		return;
	}

	switch (p->opcode) {
		case OP_Combined: {
			processed=0;
			while(processed < p->size) {
				subpacket_length=*(p->pBuffer+processed);
				EQProtocolPacket *subp=MakeProtocolPacket(p->pBuffer+processed+1,subpacket_length);
				LogNetcodeDetail(_L "Extracting combined packet of length [{}]" __L, subpacket_length);
				// _raw(NET__NET_CREATE_HEX, 0xFFFF, subp);
				subp->copyInfo(p);
				ProcessPacket(subp);
				delete subp;
				processed+=subpacket_length+1;
			}
		}
		break;

		case OP_AppCombined: {
			processed=0;
			while(processed<p->size) {
				EQRawApplicationPacket *ap=nullptr;
				if ((subpacket_length=(unsigned char)*(p->pBuffer+processed))!=0xff) {
					LogNetcodeDetail(_L "Extracting combined app packet of length [{}], short len" __L, subpacket_length);
					ap=MakeApplicationPacket(p->pBuffer+processed+1,subpacket_length);
					processed+=subpacket_length+1;
				} else {
					subpacket_length=ntohs(*(uint16 *)(p->pBuffer+processed+1));
					LogNetcodeDetail(_L "Extracting combined app packet of length [{}], short len" __L, subpacket_length);
					ap=MakeApplicationPacket(p->pBuffer+processed+3,subpacket_length);
					processed+=subpacket_length+3;
				}
				if (ap) {
					ap->copyInfo(p);
					InboundQueuePush(ap);
				}
			}
		}
		break;

		case OP_Packet: {
			if(!p->pBuffer || (p->Size() < 4))
			{
				LogNetcodeDetail(_L "Received OP_Packet that was of malformed size" __L);
				break;
			}
			uint16 seq=ntohs(*(uint16 *)(p->pBuffer));
			SeqOrder check=CompareSequence(NextInSeq,seq);
			if (check == SeqFuture) {
				LogNetcodeDetail(_L "Future OP_Packet: Expecting Seq = [{}], but got Seq = [{}]" __L, NextInSeq, seq);
				// _raw(NET__DEBUG, seq, p);

				PacketQueue[seq]=p->Copy();
				LogNetcodeDetail(_L "OP_Packet Queue size = [{}]" __L, PacketQueue.size());

				//SendOutOfOrderAck(seq);

			} else if (check == SeqPast) {
				LogNetcodeDetail(_L "Duplicate OP_Packet: Expecting Seq = [{}], but got Seq = [{}]" __L, NextInSeq, seq);
				// _raw(NET__DEBUG, seq, p);
				SendOutOfOrderAck(seq); //we already got this packet but it was out of order
			} else {
				// In case we did queue one before as well.
				EQProtocolPacket *qp=RemoveQueue(seq);
				if (qp) {
					LogNetcodeDetail("[NET_TRACE] OP_Packet: Removing older queued packet with sequence [{}]", seq);
					delete qp;
				}

				SetNextAckToSend(seq);
				NextInSeq++;
				// Check for an embedded OP_AppCombinded (protocol level 0x19)
				if (*(p->pBuffer+2)==0x00 && *(p->pBuffer+3)==0x19) {
					EQProtocolPacket *subp=MakeProtocolPacket(p->pBuffer+2,p->size-2);
					LogNetcodeDetail(_L "seq [{}], Extracting combined packet of length [{}]" __L, seq, subp->size);
					// _raw(NET__NET_CREATE_HEX, seq, subp);
					subp->copyInfo(p);
					ProcessPacket(subp);
					delete subp;
				} else {
					EQRawApplicationPacket *ap=MakeApplicationPacket(p->pBuffer+2,p->size-2);
					if (ap) {
						ap->copyInfo(p);
						InboundQueuePush(ap);
					}
				}
			}
		}
		break;

		case OP_Fragment: {
			if(!p->pBuffer || (p->Size() < 4))
			{
				LogNetcodeDetail(_L "Received OP_Fragment that was of malformed size" __L);
				break;
			}
			uint16 seq=ntohs(*(uint16 *)(p->pBuffer));
			SeqOrder check=CompareSequence(NextInSeq,seq);
			if (check == SeqFuture) {
				LogNetcodeDetail(_L "Future OP_Fragment: Expecting Seq = [{}], but got Seq = [{}]" __L, NextInSeq, seq);
				// _raw(NET__DEBUG, seq, p);

				PacketQueue[seq]=p->Copy();
				LogNetcodeDetail( _L "OP_Fragment Queue size = [{}]" __L, PacketQueue.size());

				//SendOutOfOrderAck(seq);

			} else if (check == SeqPast) {
				LogNetcodeDetail(_L "Duplicate OP_Fragment: Expecting Seq = [{}], but got Seq = [{}]" __L, NextInSeq, seq);
				// _raw(NET__DEBUG, seq, p);
				SendOutOfOrderAck(seq);
			} else {
				// In case we did queue one before as well.
				EQProtocolPacket *qp=RemoveQueue(seq);
				if (qp) {
					LogNetcode("[NET_TRACE] OP_Fragment: Removing older queued packet with sequence [{}]", seq);
					delete qp;
				}
				SetNextAckToSend(seq);
				NextInSeq++;
				if (oversize_buffer) {
					memcpy(oversize_buffer+oversize_offset,p->pBuffer+2,p->size-2);
					oversize_offset+=p->size-2;
					LogNetcodeDetail(_L "Fragment of oversized of length [{}], seq [{}]: now at [{}]/[{}]" __L, p->size - 2, seq, oversize_offset, oversize_length);
					if (oversize_offset==oversize_length) {
						if (*(p->pBuffer+2)==0x00 && *(p->pBuffer+3)==0x19) {
							EQProtocolPacket *subp=MakeProtocolPacket(oversize_buffer,oversize_offset);
							LogNetcodeDetail(_L "seq [{}], Extracting combined oversize packet of length [{}]" __L, seq, subp->size);
							//// _raw(NET__NET_CREATE_HEX, subp);
							subp->copyInfo(p);
							ProcessPacket(subp);
							delete subp;
						} else {
							EQRawApplicationPacket *ap=MakeApplicationPacket(oversize_buffer,oversize_offset);
							LogNetcodeDetail(_L "seq [{}], completed combined oversize packet of length [{}]" __L, seq, ap->size);
							if (ap) {
								ap->copyInfo(p);
								InboundQueuePush(ap);
							}
						}
						delete[] oversize_buffer;
						oversize_buffer=nullptr;
						oversize_offset=0;
					}
				} else {
					oversize_length=ntohl(*(uint32 *)(p->pBuffer+2));
					oversize_buffer=new unsigned char[oversize_length];
					memcpy(oversize_buffer,p->pBuffer+6,p->size-6);
					oversize_offset=p->size-6;
					LogNetcodeDetail(_L "First fragment of oversized of seq [{}]: now at [{}]/[{}]" __L, seq, oversize_offset, oversize_length);
				}
			}
		}
		break;
		case OP_KeepAlive: {
			NonSequencedPush(new EQProtocolPacket(p->opcode,p->pBuffer,p->size));
			LogNetcodeDetail(_L "Received and queued reply to keep alive Opcode: [{}] Size: [{}]" __L, p->opcode, p->size);
		}
		break;
		case OP_Ack: {
			if(!p->pBuffer || (p->Size() < 4))
			{
				LogNetcodeDetail(_L "Received OP_Ack that was of malformed size" __L);
				break;
			}
			uint16 seq=ntohs(*(uint16 *)(p->pBuffer));
			AckPackets(seq);

			if(GetExecutablePlatform() == ExePlatformWorld || GetExecutablePlatform() == ExePlatformZone || GetExecutablePlatform() == ExePlatformUCS) {
				retransmittimer = Timer::GetCurrentTime();
			}
		}
		break;
		case OP_SessionRequest: {
			if(p->Size() < sizeof(SessionRequest))
			{
				LogNetcodeDetail(_L "Received OP_SessionRequest that was of malformed size" __L);
				break;
			}
			if (GetState() == ESTABLISHED) {
				//LogNetcodeDetail( _L "Received OP_SessionRequest in ESTABLISHED state [{}] streamactive [{}] attempt [{}]" __L, GetState(), streamactive, sessionAttempts);

				// client seems to try a max of 30 times (initial+3 retries) then gives up, giving it a few more attempts just in case
				// streamactive means we identified the opcode for the stream, we cannot re-establish this connection
				if (streamactive || (sessionAttempts > MAX_SESSION_RETRIES))
				{
					_SendDisconnect();
					SetState(CLOSED);
					break;
				}
			}
			sessionAttempts++;
			// we set established below, so statistics will not be reset for session attempts/stream active.
			init(GetState() != ESTABLISHED);
			OutboundQueueClear();
			SessionRequest *Request=(SessionRequest *)p->pBuffer;
			Session=ntohl(Request->Session);
			SetMaxLen(ntohl(Request->MaxLength));
			LogNetcodeDetail(_L "Received OP_SessionRequest: session [{}], maxlen [{}]" __L, (unsigned long)Session, MaxLen);
			SetState(ESTABLISHED);
			Key=0x11223344;
			SendSessionResponse();
		}
		break;
		case OP_SessionResponse: {
			if(p->Size() < sizeof(SessionResponse))
			{
				LogNetcodeDetail(_L "Received OP_SessionResponse that was of malformed size" __L);
				break;
			}

			init();
			OutboundQueueClear();
			SessionResponse *Response=(SessionResponse *)p->pBuffer;
			SetMaxLen(ntohl(Response->MaxLength));
			Key=ntohl(Response->Key);
			NextInSeq=0;
			SetState(ESTABLISHED);
			if (!Session)
				Session=ntohl(Response->Session);
			compressed=(Response->Format&FLAG_COMPRESSED);
			encoded=(Response->Format&FLAG_ENCODED);

			LogNetcodeDetail(_L "Received OP_SessionResponse: session [{}], maxlen [{}], key [{}], compressed? [{}], encoded? [{}]" __L, (unsigned long)Session, MaxLen, (unsigned long)Key, compressed ? "yes" : "no", encoded ? "yes" : "no");

			// Kinda kludgy, but trie for now
			if (StreamType==UnknownStream) {
				if (compressed) {
					if (remote_port==9000 || (remote_port==0 && p->src_port==9000)) {
						SetStreamType(WorldStream);
					} else {
						SetStreamType(ZoneStream);
					}
				} else if (encoded) {
					SetStreamType(ChatOrMailStream);
				} else {
					SetStreamType(LoginStream);
				}
			}
		}
		break;
		case OP_SessionDisconnect: {
			//NextInSeq=0;
			EQStreamState state = GetState();
			if(state == ESTABLISHED) {
				//client initiated disconnect?
				LogNetcodeDetail(_L "Received unsolicited OP_SessionDisconnect. Treating like a client-initiated disconnect." __L);
				_SendDisconnect();
				SetState(CLOSED);
			} else if(state == CLOSING) {
				//we were waiting for this anyways, ignore pending messages, send the reply and be closed.
				LogNetcodeDetail(_L "Received OP_SessionDisconnect when we have a pending close, they beat us to it. Were happy though." __L);
				_SendDisconnect();
				SetState(CLOSED);
			} else {
				//we are expecting this (or have already gotten it, but dont care either way)
				LogNetcodeDetail(_L "Received expected OP_SessionDisconnect. Moving to closed state." __L);
				SetState(CLOSED);
			}
		}
		break;
		case OP_OutOfOrderAck: {
			if(!p->pBuffer || (p->Size() < 4))
			{
				LogNetcodeDetail(_L "Received OP_OutOfOrderAck that was of malformed size" __L);
				break;
			}
			uint16 seq=ntohs(*(uint16 *)(p->pBuffer));
			MOutboundQueue.lock();

			if(uint16(SequencedBase + SequencedQueue.size()) != NextOutSeq) {
				LogNetcodeDetail(_L "Pre-OOA Invalid Sequenced queue: BS [{}] + SQ [{}] != NOS [{}]" __L, SequencedBase, SequencedQueue.size(), NextOutSeq);
			}
			
			if(NextSequencedSend > (SequencedBase + SequencedQueue.size())) {
				LogNetcodeDetail(_L "Pre-OOA Next Send Sequence is beyond the end of the queue NSS [{}] > SQ [{}]" __L, NextSequencedSend, SequencedQueue.size());
			}
			//if the packet they got out of order is between our last acked packet and the last sent packet, then its valid.
			if (CompareSequence(SequencedBase,seq) != SeqPast && CompareSequence(NextOutSeq,seq) == SeqPast) {
				LogNetcodeDetail( _L "Received OP_OutOfOrderAck for sequence [{}], starting retransmit at the start of our unacked buffer (seq [{}], was [{}])." __L,
					seq, SequencedBase, SequencedBase+NextSequencedSend);

				bool retransmit_acked_packets = false;
				if(GetExecutablePlatform() == ExePlatformWorld || GetExecutablePlatform() == ExePlatformZone) {
					retransmit_acked_packets = RETRANSMIT_ACKED_PACKETS;
				}

				if(!retransmit_acked_packets) {
					uint16 sqsize = SequencedQueue.size();
					uint16 index = seq - SequencedBase;
					LogNetcodeDetail(_L "OP_OutOfOrderAck marking packet acked in queue (queue index = [{}], queue size = [{}])." __L, index, sqsize);
					if (index < sqsize) {
						std::deque<EQProtocolPacket *>::iterator sitr;
						sitr = SequencedQueue.begin();
						sitr += index;
						(*sitr)->acked = true;
					}
				}

				if(RETRANSMIT_TIMEOUT_MULT) {
					retransmittimer = Timer::GetCurrentTime();
				}

				NextSequencedSend = 0;
			} else {
				LogNetcodeDetail(_L "Received OP_OutOfOrderAck for out-of-window [{}]. Window ([{}]->[{}])." __L, seq, SequencedBase, NextOutSeq);
			}

			if(uint16(SequencedBase + SequencedQueue.size()) != NextOutSeq) {
				LogNetcodeDetail(_L "Post-OOA Invalid Sequenced queue: BS [{}] + SQ [{}] != NOS [{}]" __L, SequencedBase, SequencedQueue.size(), NextOutSeq);
			}

			if(NextSequencedSend > (SequencedBase + SequencedQueue.size())) {
				LogNetcodeDetail(_L "Post-OOA Next Send Sequence is beyond the end of the queue NSS [{}] > SQ [{}]" __L, NextSequencedSend, SequencedQueue.size());
			}
			MOutboundQueue.unlock();
		}
		break;
		case OP_SessionStatRequest: {
			if(p->Size() < sizeof(SessionStats))
			{
				LogNetcodeDetail(_L "Received OP_SessionStatRequest that was of malformed size" __L);
				break;
			}
			SessionStats *Stats=(SessionStats *)p->pBuffer;
			LogNetcodeDetail(_L "Received Stats: [{}] packets received, [{}] packets sent, Deltas: local [{}], ([{}] <- [{}] -> [{}]) remote [{}]" __L,
				(unsigned long)ntohl(Stats->packets_received), (unsigned long)ntohl(Stats->packets_sent), (unsigned long)ntohl(Stats->last_local_delta),
				(unsigned long)ntohl(Stats->low_delta), (unsigned long)ntohl(Stats->average_delta),
				(unsigned long)ntohl(Stats->high_delta), (unsigned long)ntohl(Stats->last_remote_delta));
			uint64 x=Stats->packets_received;
			Stats->packets_received=Stats->packets_sent;
			Stats->packets_sent=x;
			NonSequencedPush(new EQProtocolPacket(OP_SessionStatResponse,p->pBuffer,p->size));
			//AdjustRates(ntohl(Stats->average_delta));

			if(GetExecutablePlatform() == ExePlatformWorld || GetExecutablePlatform() == ExePlatformZone || GetExecutablePlatform() == ExePlatformUCS) {
				if(RETRANSMIT_TIMEOUT_MULT && ntohl(Stats->average_delta)) {
					//recalculate retransmittimeout using the larger of the last rtt or average rtt, which is multiplied by the rule value
					if((ntohl(Stats->last_local_delta) + ntohl(Stats->last_remote_delta)) > (ntohl(Stats->average_delta) * 2)) {
						retransmittimeout = (ntohl(Stats->last_local_delta) + ntohl(Stats->last_remote_delta)) 
							* RETRANSMIT_TIMEOUT_MULT;
					} else {
						retransmittimeout = ntohl(Stats->average_delta) * 2 * RETRANSMIT_TIMEOUT_MULT;
					}
					if(retransmittimeout > RETRANSMIT_TIMEOUT_MAX)
						retransmittimeout = RETRANSMIT_TIMEOUT_MAX;
					LogNetcodeDetail( _L "Retransmit timeout recalculated to %dms" __L, retransmittimeout);
				}
			}
		}
		break;
		case OP_SessionStatResponse: {
			LogNetcodeDetail(_L "Received OP_SessionStatResponse. Ignoring." __L);
		}
		break;
		case OP_OutOfSession: {
			LogNetcodeDetail(_L "Received OP_OutOfSession. Ignoring." __L);
		}
		break;
		default:
			EQRawApplicationPacket *ap = MakeApplicationPacket(p);
			if (ap)
				InboundQueuePush(ap);
			break;
	}
}

void EQStream::QueuePacket(const EQApplicationPacket *p, bool ack_req)
{
	if (p == nullptr) {
		return;
	}

	EQApplicationPacket *newp = p->Copy();

	if (newp != nullptr)
		FastQueuePacket(&newp, ack_req);
}

void EQStream::FastQueuePacket(EQApplicationPacket **p, bool ack_req)
{
	EQApplicationPacket *pack=*p;
	*p = nullptr;		//clear caller's pointer.. effectively takes ownership

	if(pack == nullptr)
		return;

	if(OpMgr == nullptr || *OpMgr == nullptr) {
		LogNetcodeDetail(_L "Packet enqueued into a stream with no opcode manager, dropping." __L);
		delete pack;
		return;
	}

	uint16 opcode = 0;
	if(pack->GetOpcodeBypass() != 0) {
		opcode = pack->GetOpcodeBypass();
	} else {
		opcode = (*OpMgr)->EmuToEQ(pack->emu_opcode);
	}

	if (!ack_req) {
		NonSequencedPush(new EQProtocolPacket(opcode, pack->pBuffer, pack->size));
		delete pack;
	} else {
		SendPacket(opcode, pack);
	}
}

void EQStream::SendPacket(uint16 opcode, EQApplicationPacket *p)
{
	uint32 chunksize, used;
	uint32 length;

	if (LogSys.log_settings[Logs::PacketServerClient].is_category_enabled == 1)	{
		if (p->GetOpcode() != OP_SpecialMesg)
		{
			LogPacketServerClient("[{}] - [{:#06x}] Size: [{}]", OpcodeManager::EmuToName(p->GetOpcode()), p->GetOpcode(), p->Size());
		}
	}

	if (LogSys.log_settings[Logs::PacketServerClient].is_category_enabled == 1){
		if (p->GetOpcode() != OP_SpecialMesg){
			LogPacketServerClient("[{}] - [{:#06x}] Size: [{}] [{}]", OpcodeManager::EmuToName(p->GetOpcode()), p->GetOpcode(), p->Size(), DumpPacketToString(p).c_str());
		}
	}

	// Convert the EQApplicationPacket to 1 or more EQProtocolPackets
	if (p->size>(MaxLen-8)) { // proto-op(2), seq(2), app-op(2) ... data ... crc(2)
		LogNetcode(_L "Making oversized packet, len [{}]" __L, p->Size());

		auto tmpbuff = new unsigned char[p->size + 3];
		length = p->serialize(opcode, tmpbuff);
		if (length != p->Size())
			LogNetcode(_L "Packet adjustment, len [{}] to [{}]" __L, p->Size(), length);

		auto out = new EQProtocolPacket(OP_Fragment, nullptr, MaxLen - 4);
		*(uint32 *)(out->pBuffer + 2) = htonl(length);
		used = MaxLen - 10;
		memcpy(out->pBuffer + 6, tmpbuff, used);
		LogNetcode(_L "First fragment: used [{}]/[{}]. Payload size [{}] in the packet" __L, used, length, p->size);
		SequencedPush(out);

		while (used<length) {
			out = new EQProtocolPacket(OP_Fragment, nullptr, MaxLen - 4);
			chunksize = std::min(length - used, MaxLen - 6);
			memcpy(out->pBuffer + 2, tmpbuff + used, chunksize);
			out->size = chunksize + 2;
			SequencedPush(out);
			used += chunksize;
			LogNetcode(_L "Subsequent fragment: len [{}], used [{}]/[{}]." __L, chunksize, used, length);
		}
		delete p;
		delete[] tmpbuff;
	} else {

		auto tmpbuff = new unsigned char[p->Size() + 3];
		length=p->serialize(opcode, tmpbuff+2) + 2;

		auto out = new EQProtocolPacket(OP_Packet, tmpbuff, length);

		delete[] tmpbuff;
		SequencedPush(out);
		delete p;
	}
}

void EQStream::SequencedPush(EQProtocolPacket * p)
{
		MOutboundQueue.lock();
		if (uint16(SequencedBase + SequencedQueue.size()) != NextOutSeq) {
			LogNetcode( _L "Pre-Push Invalid Sequenced queue: BS [%u] + SQ [%u] != NOS [%u]" __L, SequencedBase, SequencedQueue.size(), NextOutSeq);
		}
		if (NextSequencedSend > (SequencedBase + SequencedQueue.size())) {
			LogNetcode(_L "Pre-Push Next Send Sequence is beyond the end of the queue NSS [{}] > SQ [{}]" __L, NextSequencedSend, SequencedQueue.size());
		}

		LogNetcode(_L "Pushing sequenced packet [{}] of length [{}]. Base Seq is [{}]." __L, NextOutSeq, p->size, SequencedBase);
		*(uint16*)(p->pBuffer) = htons(NextOutSeq);
		SequencedQueue.push_back(p);
		NextOutSeq++;

		if (uint16(SequencedBase + SequencedQueue.size()) != NextOutSeq) {
			LogNetcode(_L "Push Invalid Sequenced queue: BS [{}] + SQ [{}] != NOS [{}]" __L, SequencedBase, SequencedQueue.size(), NextOutSeq);
		}
		if (NextSequencedSend > (SequencedBase + SequencedQueue.size())) {
			LogNetcode(_L "Push Next Send Sequence is beyond the end of the queue NSS [{}] > SQ [{}]" __L, NextSequencedSend, SequencedQueue.size());
		}
		MOutboundQueue.unlock();
}


void EQStream::NonSequencedPush(EQProtocolPacket *p)
{
	MOutboundQueue.lock();
	LogNetcode(_L "Pushing non-sequenced packet of length [{}]" __L, p->size);
	NonSequencedQueue.push(p);
	MOutboundQueue.unlock();
}

void EQStream::SendAck(uint16 seq)
{
	uint16 Seq=htons(seq);
	LogNetcodeDetail(_L "Sending ack with sequence [[{}]]" __L, seq);
	SetLastAckSent(seq);
	NonSequencedPush(new EQProtocolPacket(OP_Ack,(unsigned char *)&Seq,sizeof(uint16)));
}

void EQStream::SendOutOfOrderAck(uint16 seq)
{
	LogNetcodeDetail(_L "Sending out of order ack with sequence [[{}]]" __L, seq);
	uint16 Seq=htons(seq);
	NonSequencedPush(new EQProtocolPacket(OP_OutOfOrderAck,(unsigned char *)&Seq,sizeof(uint16)));
}

void EQStream::Write(int eq_fd)
{
	std::queue<EQProtocolPacket *> ReadyToSend;
	bool SeqEmpty=false, NonSeqEmpty=false;
	std::deque<EQProtocolPacket *>::iterator sitr;

	// Check our rate to make sure we can send more
	MRate.lock();
	int32 threshold=RateThreshold;
	MRate.unlock();
	if (BytesWritten > threshold) {
		return;
	}

	// If we got more packets to we need to ack, send an ack on the highest one
	MAcks.lock();
	if (CompareSequence(LastAckSent, NextAckToSend) == SeqFuture)
		SendAck(NextAckToSend);
	MAcks.unlock();

	// Lock the outbound queues while we process
	MOutboundQueue.lock();

	// Place to hold the base packet t combine into
	EQProtocolPacket *p=nullptr;

	if(GetExecutablePlatform() == ExePlatformWorld || GetExecutablePlatform() == ExePlatformZone || stream_startup) {
		// if we have a timeout defined and we have not received an ack recently enough, retransmit from beginning of queue
		if (RETRANSMIT_TIMEOUT_MULT && !SequencedQueue.empty() && NextSequencedSend &&
			(GetState()==ESTABLISHED) && ((retransmittimer+retransmittimeout) > Timer::GetCurrentTime())) {
			LogNetcodeDetail(_L "Timeout since last ack received, starting retransmit at the start of our unacked buffer (seq [{}], was [{}])."
				__L, SequencedBase, SequencedBase + NextSequencedSend);
			NextSequencedSend = 0;
			retransmittimer = Timer::GetCurrentTime(); // don't want to endlessly retransmit the first packet
		}
	}

	// Find the next sequenced packet to send from the "queue"
	sitr = SequencedQueue.begin();
	if (sitr!=SequencedQueue.end())
	sitr += NextSequencedSend;

	// Loop until both are empty or MaxSends is reached
	while(!SeqEmpty || !NonSeqEmpty) {

		// See if there are more non-sequenced packets left
		if (!NonSequencedQueue.empty()) {
			if (!p) {
				// If we don't have a packet to try to combine into, use this one as the base
				// And remove it form the queue
				p = NonSequencedQueue.front();
				LogNetcodeDetail(_L "Starting combined packet with non-seq packet of len [{}]" __L, p->size);
				NonSequencedQueue.pop();
			} else if (!p->combine(NonSequencedQueue.front())) {
				// Tryint to combine this packet with the base didn't work (too big maybe)
				// So just send the base packet (we'll try this packet again later)
				LogNetcodeDetail(_L "Combined packet full at len [{}], next non-seq packet is len [{}]" __L, p->size, (NonSequencedQueue.front())->size);
				ReadyToSend.push(p);
				BytesWritten+=p->size;
				p=nullptr;

				if (BytesWritten > threshold) {
					// Sent enough this round, lets stop to be fair
					LogNetcodeDetail(_L "Exceeded write threshold in nonseq ([{}] > [{}])" __L, BytesWritten, threshold);
					break;
				}
			} else {
				// Combine worked, so just remove this packet and it's spot in the queue
				LogNetcodeDetail(_L "Combined non-seq packet of len [{}], yeilding [{}] combined." __L, (NonSequencedQueue.front())->size, p->size);
				delete NonSequencedQueue.front();
				NonSequencedQueue.pop();
			}
		} else {
			// No more non-sequenced packets
			NonSeqEmpty=true;
		}

		if (sitr!=SequencedQueue.end()) {
			if(uint16(SequencedBase + SequencedQueue.size()) != NextOutSeq) {
				LogNetcodeDetail( _L "Pre-Send Seq NSS=%d Invalid Sequenced queue: BS [{}] + SQ [{}] != NOS [{}]" __L, NextSequencedSend, SequencedBase, SequencedQueue.size(), NextOutSeq);
			}

			if(NextSequencedSend > (SequencedBase + SequencedQueue.size())) {
				LogNetcodeDetail(_L "Pre-Send Next Send Sequence is beyond the end of the queue NSS [{}] > SQ [{}]" __L, NextSequencedSend, SequencedQueue.size());
			}
			uint16 seq_send = SequencedBase + NextSequencedSend;	//just for logging...
			
			if(SequencedQueue.empty()) {
				LogNetcodeDetail(_L "Tried to write a packet with an empty queue ([{}] is past next out [{}])" __L, seq_send, NextOutSeq);
				SeqEmpty=true;
				continue;
			}

			if(GetExecutablePlatform() == ExePlatformWorld || GetExecutablePlatform() == ExePlatformZone || GetExecutablePlatform() == ExePlatformUCS) {
				if (!RETRANSMIT_ACKED_PACKETS && (*sitr)->acked) {
					LogNetcodeDetail(_L "Not retransmitting seq packet [{}] because already marked as acked" __L, seq_send);
					sitr++;
					NextSequencedSend++;
				} else if (!p) {
					// If we don't have a packet to try to combine into, use this one as the base
					// Copy it first as it will still live until it is acked
					p=(*sitr)->Copy();
					LogNetcodeDetail(_L "Starting combined packet with seq packet [{}] of len [{}]" __L, seq_send, p->size);
					++sitr;
					NextSequencedSend++;
				} else if (!p->combine(*sitr)) {
					// Trying to combine this packet with the base didn't work (too big maybe)
					// So just send the base packet (we'll try this packet again later)
					LogNetcodeDetail(_L "Combined packet full at len [{}], next seq packet [{}] is len [{}]" __L, p->size, seq_send, (*sitr)->size);
					ReadyToSend.push(p);
					BytesWritten+=p->size;
					p=nullptr;

					if (BytesWritten > threshold) {
						// Sent enough this round, lets stop to be fair
						LogNetcodeDetail(_L "Exceeded write threshold in seq ([{}] > [{}])" __L, BytesWritten, threshold);
						break;
					}
				} else {
					// Combine worked
					LogNetcodeDetail(_L "Combined seq packet [{}] of len [{}], yielding [{}] combined." __L, seq_send, (*sitr)->size, p->size);
					++sitr;
					NextSequencedSend++;
				}
			} else {
				if (!p) {
					// If we don't have a packet to try to combine into, use this one as the base
					// Copy it first as it will still live until it is acked
					p=(*sitr)->Copy();
					if (p != nullptr)
					{
						LogNetcodeDetail(_L "Starting combined packet with seq packet [{}] of len [{}]" __L, seq_send, p->size);
					}
					else
					{
						LogNetcodeDetail(_L "Starting combined packet with seq packet [{}]" __L, seq_send);
					}

					++sitr;
					NextSequencedSend++;
				} else if (!p->combine(*sitr)) {
					// Trying to combine this packet with the base didn't work (too big maybe)
					// So just send the base packet (we'll try this packet again later)
					LogNetcodeDetail(_L "Combined packet full at len [{}], next seq packet [{}] is len [{}]" __L, p->size, seq_send, (*sitr)->size);
					ReadyToSend.push(p);
					BytesWritten+=p->size;
					p=nullptr;

					if (BytesWritten > threshold) {
						// Sent enough this round, lets stop to be fair
						LogNetcodeDetail(_L "Exceeded write threshold in seq ([{}] > [{}])" __L, BytesWritten, threshold);
						break;
					}
				} else {
					// Combine worked
					LogNetcodeDetail(_L "Combined seq packet [{}] of len [{}], yeilding [{}] combined." __L, seq_send, (*sitr)->size, p->size);
					++sitr;
					NextSequencedSend++;
				}
			}

			if(uint16(SequencedBase + SequencedQueue.size()) != NextOutSeq) {
				LogNetcodeDetail(_L "Post send Invalid Sequenced queue: BS [{}] + SQ [{}] != NOS [{}]" __L, SequencedBase, SequencedQueue.size(), NextOutSeq);
			}
			if(NextSequencedSend > (SequencedBase + SequencedQueue.size())) {
				LogNetcodeDetail(_L "Post send Next Send Sequence is beyond the end of the queue NSS [{}] > SQ [{}]" __L, NextSequencedSend, SequencedQueue.size());
			}
		} else {
			// No more sequenced packets
			SeqEmpty=true;
		}
	}

	// We have a packet still, must have run out of both seq and non-seq, so send it
	if (p) {
		LogNetcodeDetail(_L "Final combined packet not full, len [{}]" __L, p->size);
		ReadyToSend.push(p);
		BytesWritten+=p->size;
	}

	// Unlock the queue
	MOutboundQueue.unlock();

	if (!ReadyToSend.empty()) {
		LastSent = Timer::GetCurrentTime();
	}

	// Send all the packets we "made"
	while(!ReadyToSend.empty()) {
		p = ReadyToSend.front();
		WritePacket(eq_fd,p);
		delete p;
		ReadyToSend.pop();
	}

	//see if we need to send our disconnect and finish our close
	if(SeqEmpty && NonSeqEmpty) {
		//no more data to send
		if(CheckState(CLOSING)) {
			LogNetcodeDetail(_L "All outgoing data flushed, closing stream." __L );
			//we are waiting for the queues to empty, now we can do our disconnect.
			//this packet will not actually go out until the next call to Write().
			_SendDisconnect();
			SetState(DISCONNECTING);
		}
	}
}

void EQStream::WritePacket(int eq_fd, EQProtocolPacket *p)
{
	uint32 length;
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr=remote_ip;
	address.sin_port=remote_port;
#ifdef NOWAY
	uint32 ip=address.sin_addr.s_addr;
	std::cout << "Sending to: "
		<< (int)*(unsigned char *)&ip
		<< "." << (int)*((unsigned char *)&ip+1)
		<< "." << (int)*((unsigned char *)&ip+2)
		<< "." << (int)*((unsigned char *)&ip+3)
		<< "," << (int)ntohs(address.sin_port) << "(" << p->size << ")" << std::endl;

	p->DumpRaw();
	std::cout << "-------------" << std::endl;
#endif
	length=p->serialize(buffer);
	if (p->opcode!=OP_SessionRequest && p->opcode!=OP_SessionResponse) {
		if (compressed) {
			uint32 newlen=EQProtocolPacket::Compress(buffer,length, _tempBuffer, 2048);
			memcpy(buffer,_tempBuffer,newlen);
			length=newlen;
		}
		if (encoded) {
			EQProtocolPacket::ChatEncode(buffer,length,Key);
		}

		*(uint16 *)(buffer+length)=htons(CRC16(buffer,length,Key));
		length+=2;
	}
	//dump_message_column(buffer,length,"Writer: ");
	sendto(eq_fd,(char *)buffer,length,0,(sockaddr *)&address,sizeof(address));
	AddBytesSent(length);
}

void EQStream::SendSessionResponse()
{
	auto out = new EQProtocolPacket(OP_SessionResponse, nullptr, sizeof(SessionResponse));
	SessionResponse *Response=(SessionResponse *)out->pBuffer;
	Response->Session=htonl(Session);
	Response->MaxLength=htonl(MaxLen);
	Response->UnknownA=2;
	Response->Format=0;
	if (compressed)
		Response->Format|=FLAG_COMPRESSED;
	if (encoded)
		Response->Format|=FLAG_ENCODED;
	Response->Key=htonl(Key);

	out->size=sizeof(SessionResponse);

	LogNetcodeDetail(_L "Sending OP_SessionResponse: session [{}], maxlen= [{}], key = [{}], compressed? [{}], encoded? [{}]" __L,
		(unsigned long)Session, MaxLen, Key, compressed?"yes":"no", encoded?"yes":"no");

	NonSequencedPush(out);
}

void EQStream::SendSessionRequest()
{
	auto out = new EQProtocolPacket(OP_SessionRequest, nullptr, sizeof(SessionRequest));
	SessionRequest *Request=(SessionRequest *)out->pBuffer;
	memset(Request,0,sizeof(SessionRequest));
	Request->Session=htonl(time(nullptr));
	Request->MaxLength=htonl(512);

	LogNetcodeDetail(_L "Sending OP_SessionRequest: session [{}], maxlen = [{}]" __L, (unsigned long)ntohl(Request->Session), ntohl(Request->MaxLength));

	NonSequencedPush(out);
}

void EQStream::SendKeepAlive()
{
	if (GetState() != ESTABLISHED)
		return;

	auto out = new EQProtocolPacket(OP_KeepAlive, nullptr, 0);
	NonSequencedPush(out);

	LogNetcodeDetail(_L "Sending OP_KeepAlive: session [{}]" __L, (unsigned long)Session);

}

void EQStream::_SendDisconnect()
{
	if(GetState() == CLOSED)
		return;

	auto out = new EQProtocolPacket(OP_SessionDisconnect, nullptr, sizeof(uint32));
	*(uint32 *)out->pBuffer=htonl(Session);
	NonSequencedPush(out);

	LogNetcodeDetail(_L "Sending OP_SessionDisconnect: session [{}]" __L, (unsigned long)Session);
}

void EQStream::InboundQueuePush(EQRawApplicationPacket *p)
{
	MInboundQueue.lock();
	InboundQueue.push_back(p);
	MInboundQueue.unlock();
}

EQApplicationPacket *EQStream::PopPacket()
{
EQRawApplicationPacket *p=nullptr;

	MInboundQueue.lock();
	if (!InboundQueue.empty()) {
		auto itr = InboundQueue.begin();
		p=*itr;
		InboundQueue.erase(itr);
	}
	MInboundQueue.unlock();

	if (p) {
		if (OpMgr != nullptr && *OpMgr != nullptr) {
			EmuOpcode emu_op = (*OpMgr)->EQToEmu(p->opcode);
			if (emu_op == OP_Unknown) {
				// Log(Logs::General, Logs::Client_Server_Packet_Unhandled, "Unknown :: [%s - 0x%04x] [Size: %u] %s", OpcodeManager::EmuToName(p->GetOpcode()), p->opcode, p->Size(), DumpPacketToString(p).c_str());
			} 
			p->SetOpcode(emu_op);
		}
	}

	return p;
}

EQRawApplicationPacket *EQStream::PopRawPacket()
{
EQRawApplicationPacket *p=nullptr;

	MInboundQueue.lock();
	if (!InboundQueue.empty()) {
		auto itr = InboundQueue.begin();
		p=*itr;
		InboundQueue.erase(itr);
	}
	MInboundQueue.unlock();

	//resolve the opcode if we can.
	if(p) {
		if(OpMgr != nullptr && *OpMgr != nullptr) {
			EmuOpcode emu_op = (*OpMgr)->EQToEmu(p->opcode);
			if(emu_op == OP_Unknown) {
				LogNetcode("Unable to convert EQ opcode {:#04x} to an Application opcode", p->opcode);
			}

			p->SetOpcode(emu_op);
		}
	}

	return p;
}

EQRawApplicationPacket *EQStream::PeekPacket()
{
EQRawApplicationPacket *p=nullptr;

	MInboundQueue.lock();
	if (!InboundQueue.empty()) {
		auto itr = InboundQueue.begin();
		p=*itr;
	}
	MInboundQueue.unlock();

	return p;
}

void EQStream::InboundQueueClear()
{
EQApplicationPacket *p=nullptr;

	LogNetcodeDetail(_L "Clearing inbound queue" __L);

	MInboundQueue.lock();
	if (!InboundQueue.empty()) {
		std::vector<EQRawApplicationPacket *>::iterator itr;
		for(itr=InboundQueue.begin();itr!=InboundQueue.end();++itr) {
			p=*itr;
			delete p;
		}
		InboundQueue.clear();
	}
	MInboundQueue.unlock();
}

bool EQStream::HasOutgoingData()
{
	bool flag;

	MOutboundQueue.lock();
	flag=(!NonSequencedQueue.empty());
	if (!flag) {
		//not only wait until we send it all, but wait until they ack everything.
		flag = !SequencedQueue.empty();
	}
	MOutboundQueue.unlock();

	if (!flag) {
		MAcks.lock();
		flag= (NextAckToSend>LastAckSent);
		MAcks.unlock();
	}

	return flag;
}

void EQStream::OutboundQueueClear()
{
	EQProtocolPacket *p=nullptr;

	LogNetcodeDetail(_L "Clearing outbound queue" __L);

	MOutboundQueue.lock();
	while(!NonSequencedQueue.empty()) {
		delete NonSequencedQueue.front();
		NonSequencedQueue.pop();
	}
	if(!SequencedQueue.empty()) {
		std::deque<EQProtocolPacket *>::iterator itr;
		for(itr=SequencedQueue.begin();itr!=SequencedQueue.end();++itr) {
			p=*itr;
			delete p;
		}
		SequencedQueue.clear();
	}
	MOutboundQueue.unlock();
}

void EQStream::PacketQueueClear()
{
	EQProtocolPacket *p=nullptr;

	LogNetcodeDetail(_L "Clearing future packet queue" __L);

	if(!PacketQueue.empty()) {
		std::map<unsigned short,EQProtocolPacket *>::iterator itr;
		for(itr=PacketQueue.begin();itr!=PacketQueue.end();++itr) {
			p=itr->second;
			delete p;
		}
		PacketQueue.clear();
	}
}

void EQStream::Process(const unsigned char *buffer, const uint32 length)
{
static unsigned char newbuffer[2048];
	uint32 newlength=0;
	if (EQProtocolPacket::ValidateCRC(buffer,length,Key)) {
		if (compressed) {
			newlength=EQProtocolPacket::Decompress(buffer,length,newbuffer,2048);
		} else {
			memcpy(newbuffer,buffer,length);
			newlength=length;
			if (encoded)
				EQProtocolPacket::ChatDecode(newbuffer,newlength-2,Key);
		}
		if (buffer[1]!=0x01 && buffer[1]!=0x02 && buffer[1]!=0x1d)
			newlength-=2;
		EQProtocolPacket *p = MakeProtocolPacket(newbuffer,newlength);
		ProcessPacket(p);
		delete p;
		ProcessQueue();
	} else {
		/*std::string s = {};
		for (uint32 i = 0; i < length; i++) {
			int value = buffer[i];
			s += fmt::format("{:02x} ", value);
		}*/
		LogNetcodeDetail(_L "Incoming packet failed checksum" __L);
		//_SendDisconnect();
		//SetState(CLOSED);
	}
}

long EQStream::GetNextAckToSend()
{
	MAcks.lock();
	long l=NextAckToSend;
	MAcks.unlock();

	return l;
}

long EQStream::GetLastAckSent()
{
	MAcks.lock();
	long l=LastAckSent;
	MAcks.unlock();

	return l;
}

void EQStream::AckPackets(uint16 seq)
{
	std::deque<EQProtocolPacket *>::iterator itr, tmp;

	MOutboundQueue.lock();
	stream_startup = false;
	//do a bit of sanity checking.
	if(uint16(SequencedBase + SequencedQueue.size()) != NextOutSeq) {
		LogNetcodeDetail(_L "Pre-Ack Invalid Sequenced queue: BS [{}] + SQ [{}] != NOS [{}]" __L, SequencedBase, SequencedQueue.size(), NextOutSeq);
	}
	if(NextSequencedSend > (SequencedBase + SequencedQueue.size())) {
		LogNetcodeDetail(_L "Pre-Ack Next Send Sequence is beyond the end of the queue NSS [{}] > SQ [{}]" __L, NextSequencedSend, SequencedQueue.size());
	}

	SeqOrder ord = CompareSequence(SequencedBase, seq);
	if(ord == SeqInOrder) {
		//they are not acking anything new...
		LogNetcodeDetail(_L "Received an ack with no window advancement (seq [{}])." __L, seq);
	} else if(ord == SeqPast) {
		//they are nacking blocks going back before our buffer, wtf?
		LogNetcodeDetail(_L "Received an ack with backward window advancement (they gave [{}], our window starts at [{}]). This is bad." __L, seq, SequencedBase);
	} else {
		LogNetcodeDetail(_L "Received an ack up through sequence [{}]. Our base is [{}]." __L, seq, SequencedBase);


		//this is a good ack, we get to ack some blocks.
		seq++;	//we stop at the block right after their ack, counting on the wrap of both numbers.
		while(SequencedBase != seq) {
			if(SequencedQueue.empty()) {
				LogNetcodeDetail(_L "OUT OF PACKETS acked packet with sequence [{}]. Next send is [{}] before this." __L, (unsigned long)SequencedBase, NextSequencedSend);
				SequencedBase = NextOutSeq;
				NextSequencedSend = 0;
				break;
			}
			LogNetcodeDetail(_L "Removing acked packet with sequence [{}]. Next send is [{}] before this." __L, (unsigned long)SequencedBase, NextSequencedSend);
			//clean out the acked packet
			delete SequencedQueue.front();
			SequencedQueue.pop_front();
			//adjust our "next" pointer
			if(NextSequencedSend > 0)
				NextSequencedSend--;
			//advance the base sequence number to the seq of the block after the one we just got rid of.
			SequencedBase++;
		}
		if(uint16(SequencedBase + SequencedQueue.size()) != NextOutSeq) {
			LogNetcodeDetail(_L "Post-Ack on [{}] Invalid Sequenced queue: BS [{}] + SQ [{}] != NOS [{}]" __L, seq, SequencedBase, SequencedQueue.size(), NextOutSeq);
		}
		if(NextSequencedSend > (SequencedBase + SequencedQueue.size())) {
			LogNetcodeDetail(_L "Post-Ack Next Send Sequence is beyond the end of the queue NSS [{}] > SQ [{}]" __L, NextSequencedSend, SequencedQueue.size());
		}
	}

	MOutboundQueue.unlock();
}

void EQStream::SetNextAckToSend(uint32 seq)
{
	MAcks.lock();
	LogNetcodeDetail(_L "Set Next Ack To Send to [{}]" __L, (unsigned long)seq);
	NextAckToSend=seq;
	MAcks.unlock();
}

void EQStream::SetLastAckSent(uint32 seq)
{
	MAcks.lock();
	LogNetcodeDetail(_L "Set Last Ack Sent to [{}]" __L, (unsigned long)seq);
	LastAckSent=seq;
	MAcks.unlock();
}

void EQStream::ProcessQueue()
{
	if(PacketQueue.empty()) {
		return;
	}

	EQProtocolPacket *qp=nullptr;
	while((qp=RemoveQueue(NextInSeq))!=nullptr) {
		LogNetcodeDetail(_L "Processing Queued Packet: Seq = [{}]" __L, NextInSeq);
		ProcessPacket(qp);
		delete qp;
		LogNetcodeDetail(_L "OP_Packet Queue size = [{}]" __L, PacketQueue.size());
	}
}

EQProtocolPacket *EQStream::RemoveQueue(uint16 seq)
{
std::map<unsigned short,EQProtocolPacket *>::iterator itr;
EQProtocolPacket *qp=nullptr;
	if ((itr=PacketQueue.find(seq))!=PacketQueue.end()) {
		qp=itr->second;
		PacketQueue.erase(itr);
		LogNetcodeDetail(_L "OP_Packet Queue size = [{}]" __L, PacketQueue.size());
	}
	return qp;
}

void EQStream::SetStreamType(EQStreamType type)
{
	LogNetcodeDetail("{}:{} Changing stream type from {} to {}", long2ip(remote_ip), ntohs(remote_port), StreamTypeString(StreamType), StreamTypeString(type));
	StreamType=type;
	switch (StreamType) {
		case LoginStream:
			app_opcode_size=1;
			compressed=false;
			encoded=false;
			LogNetcodeDetail(_L "Login stream has app opcode size [{}], is not compressed or encoded." __L, app_opcode_size);
			break;
		case ChatOrMailStream:
		case ChatStream:
		case MailStream:
			app_opcode_size=1;
			compressed=false;
			encoded=true;
			LogNetcodeDetail(_L "Chat/Mail stream has app opcode size [{}], is not compressed, and is encoded." __L, app_opcode_size);
			break;
		case ZoneStream:
		case WorldStream:
		case OldStream:
		default:
			app_opcode_size=2;
			compressed=true;
			encoded=false;
			LogNetcodeDetail(_L "World/Zone stream has app opcode size [{}], is compressed, and is not encoded." __L, app_opcode_size);
			break;
	}
}

const char *EQOldStream::StreamTypeString(EQStreamType t)
{
	return "OldStream";
}

const char *EQStream::StreamTypeString(EQStreamType t)
{
	switch (t) {
		case LoginStream:
			return "Login";
			break;
		case WorldStream:
			return "World";
			break;
		case ZoneStream:
			return "Zone";
			break;
		case ChatOrMailStream:
			return "Chat/Mail";
			break;
		case ChatStream:
			return "Chat";
			break;
		case MailStream:
			return "Mail";
			break;
		case UnknownStream:
			return "Unknown";
			break;
	}
	return "UnknownType";
}

//returns SeqFuture if `seq` is later than `expected_seq`
EQStream::SeqOrder EQStream::CompareSequence(uint16 expected_seq , uint16 seq)
{
	if (expected_seq==seq) {
		// Curent
		return SeqInOrder;
	} else if ((seq > expected_seq && (uint32)seq < ((uint32)expected_seq + EQStream::MaxWindowSize)) || seq < (expected_seq - EQStream::MaxWindowSize)) {
		// Future
		return SeqFuture;
	} else {
		// Past
		return SeqPast;
	}
}

void EQStream::SetState(EQStreamState state) {
	MState.lock();
	LogNetcodeDetail(_L "Changing state from [{}] to [{}]" __L, (int)State, (int)state);
	State=state;
	stale_count=0;
	MState.unlock();
}


void EQStream::CheckTimeout(uint32 now, uint32 timeout) {
	EQStreamState orig_state = GetState();

	bool outgoing_data = HasOutgoingData();	//up here to avoid recursive locking

	if (orig_state == CLOSING && !outgoing_data) {
		LogNetcodeDetail(_L "Out of data in closing state, disconnecting." __L);
		SetState(CLOSED);
	} else if (LastPacket && (now-LastPacket) > timeout) {
		switch(orig_state) {
		case CLOSING:
			//if we time out in the closing state, they are not acking us, just give up
			LogNetcodeDetail(_L "Timeout expired in closing state. Moving to closed state." __L);
			_SendDisconnect();
			SetState(CLOSED);
			break;
		case DISCONNECTING:
			//we timed out waiting for them to send us the disconnect reply, just give up.
			LogNetcodeDetail(_L "Timeout expired in disconnecting state. Moving to closed state." __L);
			SetState(CLOSED);
			break;
		case CLOSED:
			SetLastPacketTime(Timer::GetCurrentTime());
			stale_count++;
			if (stale_count > 1) {
				stale_count = 0;
				this->ReleaseFromUse();
			}
			else {
				LogNetcodeDetail(_L "Timeout expired in closed state??" __L);
			}
			break;
		case ESTABLISHED:
			//we timed out during normal operation. Try to be nice about it.
			//we will almost certainly time out again waiting for the disconnect reply, but oh well.
			LogNetcodeDetail(_L "Timeout expired in established state. Closing connection." __L);
			_SendDisconnect();
			SetState(DISCONNECTING);
			break;
		default:
			break;
		}
	} else if (orig_state == ESTABLISHED && (!LastSent || ((now - LastSent) > 9800))) {
		// send a keepalive
		SendKeepAlive();
	}
}

void EQStream::Decay()
{
	MRate.lock();
	uint32 rate=DecayRate;
	MRate.unlock();
	if (BytesWritten>0) {
		BytesWritten-=rate;
		if (BytesWritten<0)
			BytesWritten=0;
	}
}

void EQStream::AdjustRates(uint32 average_delta)
{
	if(GetExecutablePlatform() == ExePlatformWorld || GetExecutablePlatform() == ExePlatformZone || GetExecutablePlatform() == ExePlatformUCS) {
		if (average_delta && (average_delta <= AVERAGE_DELTA_MAX)) {
			MRate.lock();
			RateThreshold=RATEBASE/average_delta;
			DecayRate=DECAYBASE/average_delta;
			LogNetcodeDetail(_L "Adjusting data rate to thresh [{}], decay [{}] based on avg delta [{}]" __L,
				RateThreshold, DecayRate, average_delta);
			MRate.unlock();
		} else {
			LogNetcodeDetail(_L "Not adjusting data rate because avg delta over max ([{}] > [{}])" __L,
				average_delta, AVERAGE_DELTA_MAX);
		}
	} else {
		if (average_delta) {
			MRate.lock();
			RateThreshold=RATEBASE/average_delta;
			DecayRate=DECAYBASE/average_delta;
			LogNetcodeDetail(_L "Adjusting data rate to thresh {}, decay {} based on avg delta {}" __L,
				RateThreshold, DecayRate, average_delta);
			MRate.unlock();
		}
	}
}

void EQStream::Close() {
	if (CheckClosed())
		return;
	if(HasOutgoingData()) {
		//there is pending data, wait for it to go out.
		LogNetcodeDetail(_L "Stream requested to Close(), but there is pending data, waiting for it." __L);
		SetState(CLOSING);
	} else {
		//otherwise, we are done, we can drop immediately.
		_SendDisconnect();
		LogNetcodeDetail(_L "Stream closing immediate due to Close()" __L);
		SetState(DISCONNECTING);
	}
}


//this could be expanded to check more than the fitst opcode if
//we needed more complex matching
EQStream::MatchState EQStream::CheckSignature(const Signature *sig) {
	EQRawApplicationPacket *p = nullptr;
	MatchState res = MatchNotReady;

	MInboundQueue.lock();
	if (!InboundQueue.empty()) {
		//this is already getting hackish...
		p = InboundQueue.front();
		if(sig->ignore_eq_opcode != 0 && p->opcode == sig->ignore_eq_opcode) {
			if(InboundQueue.size() > 1) {
				p = InboundQueue[1];
			} else {
				p = nullptr;
			}
		}
		if(p == nullptr) {
			//first opcode is ignored, and nothing else remains... keep waiting
		} else if(p->opcode == sig->first_eq_opcode) {
			//opcode matches, check length..
			if(p->size == sig->first_length) {
				LogNetcode("[StreamIdentify] [{}]:[{}]: First opcode matched {:#04x} and length matched [{}]", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), sig->first_eq_opcode, p->size);
				res = MatchSuccessful;
			} else if(sig->first_length == 0) {
				LogNetcode("[StreamIdentify] [{}]:[{}]: First opcode matched {:#04x} and length ([{}]) is ignored", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), sig->first_eq_opcode, p->size);
				res = MatchSuccessful;
			} else {
				//opcode matched but length did not.
				LogNetcode("[StreamIdentify] [{}]:[{}]: First opcode matched {:#04x}, but length [{}] did not match expected [{}]", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), sig->first_eq_opcode, p->size, sig->first_length);
				res = MatchFailed;
			}
		} else {
			//first opcode did not match..
			LogNetcode("[StreamIdentify] [{}]:[{}]: First opcode {:#04x} did not match expected {:#04x}", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), p->opcode, sig->first_eq_opcode);
			res = MatchFailed;
		}
	}
	MInboundQueue.unlock();

	return(res);
}

Fragment::Fragment()
{
	data = 0;
	size = 0;
	memset(this->data, 0, this->size);
}

Fragment::~Fragment()
{
	data = 0;
	size = 0;
}

void Fragment::SetData(uchar* d, uint32 s)
{
	safe_delete_array(data);
	size = s;
	data = new uchar[s];
	memcpy(data, d, s);
}

void Fragment::ClearData()
{
	safe_delete_array(data);
}

FragmentGroup::FragmentGroup(uint16 seq, uint16 opcode, uint16 num_fragments)
{
	this->seq = seq;
	this->opcode = opcode;
	this->num_fragments = num_fragments;
	fragment = new Fragment[num_fragments];
}

FragmentGroup::~FragmentGroup()
{
	for(int i=0;i<num_fragments;i++)
	{
		fragment[i].ClearData();
	}
	safe_delete_array(fragment);//delete[] fragment;
}

void FragmentGroup::Add(uint16 frag_id, uchar* data, uint32 size)
{
	//The frag_id references a fragment within the group
	if(frag_id < num_fragments)
	{
		fragment[frag_id].SetData(data, size);
	}
	//The frag_id is attempting to reference an element outside the bounds of the group array
	else
	{
		return;
	}
}

uchar* FragmentGroup::AssembleData(uint32* size)
{
	uchar* buf;
	uchar* p;
	int i;

	*size = 0;	
	for(i=0; i<num_fragments; i++)
	{
		*size+=fragment[i].GetSize();		
	}
	buf = new uchar[*size];
	p = buf;
	for(i=0; i<num_fragments; i++)
	{
		memcpy(p, fragment[i].GetData(), fragment[i].GetSize());
		p += fragment[i].GetSize();
	}
	return buf;
}


void FragmentGroupList::Add(FragmentGroup* add_group)
{
	fragment_group_list.push_back(add_group);
}

FragmentGroup* FragmentGroupList::Get(uint16 find_seq)
{
	std::vector<FragmentGroup *>::iterator it;
	it = fragment_group_list.begin();
	while (it != fragment_group_list.end()) {
		if ((*it)->GetSeq() == find_seq)
			return (*it);
		++it;
	}
	return nullptr;
}

void FragmentGroupList::Remove(uint16 remove_seq)
{
	std::vector<FragmentGroup *>::iterator it;
	it = fragment_group_list.begin();
	while (it != fragment_group_list.end()) {
		if ((*it)->GetSeq() == remove_seq) {
			safe_delete(*it);
			it = fragment_group_list.erase(it);
			return;
		}
		++it;
	}
}

void FragmentGroupList::RemoveAll()
{
	for (std::vector<FragmentGroup*>::iterator it = fragment_group_list.begin(); it != fragment_group_list.end(); ++it) {
		safe_delete(*it);
	}
}

EQOldStream::EQOldStream(sockaddr_in in, int fd_sock)
{
	pm_state = ESTABLISHED;
	dwLastCACK = 0;
	dwLastARSP = 0;
	dwFragSeq  = 0;
	listening_socket = fd_sock;
	datarate_timer = new Timer(100);
	no_ack_sent_timer = new Timer(250);
	bTimeout = false;
	bTimeoutTrigger = false;
	sent_Start = false;
	sent_Fin = false;

	/* 
		on a normal server there is always data getting sent 
		so the packetloss indicator in eq stays at 0%

		This timer will send dummy data if nothing has been sent
		in 1000ms. This is not needed. When the client doesnt
		get any data it will send a special ack request to ask
		if we are still alive. The eq client will show around 40%
		packetloss at this time. It is not real packetloss. Its
		just thinking there is packetloss since no data is sent.
		The EQ servers doesnt have a timer like this one.

		short version: This timer is not needed, it just keeps
		the green bar green.
	*/
	keep_alive_timer = new Timer(450);
	no_ack_sent_timer->Disable();
	dataflow = 0;
	SetDataRate(RuleI(World,StreamDataRate) * 10);
	debug_level = 0;
	LOG_PACKETS = false;
	isWriting = false;
	OpMgr = nullptr;
	remote_ip = in.sin_addr.s_addr; //in.sin_addr.S_un.S_addr; 
	remote_port = in.sin_port;
	packetspending = 0;
	active_users = 0;
	LastPacket=0;
	RateThreshold=RATEBASE/10;
	DecayRate=DECAYBASE/10;
	bTimeoutTrigger = false;
}

EQOldStream::EQOldStream()
{
	pm_state = ESTABLISHED;
	dwLastCACK = 0;
	dwLastARSP = 0;
	dwFragSeq  = 0;
	listening_socket = 0;		    
	datarate_timer = new Timer(100);
	no_ack_sent_timer = new Timer(250);
	bTimeout = false;
	bTimeoutTrigger = false;
	sent_Start = false;
	sent_Fin = false;
	dataflow = 0;
	SetDataRate(RuleI(World, StreamDataRate) * 5);
	arsp_response = 0;
	/* 
		on a normal server there is always data getting sent 
		so the packetloss indicator in eq stays at 0%

		This timer will send dummy data if nothing has been sent
		in 1000ms. This is not needed. When the client doesnt
		get any data it will send a special ack request to ask
		if we are still alive. The eq client will show around 40%
		packetloss at this time. It is not real packetloss. Its
		just thinking there is packetloss since no data is sent.
		The EQ servers doesnt have a timer like this one.

		short version: This timer is not needed, it just keeps
		the green bar green.
	*/
	keep_alive_timer = new Timer(450);
	no_ack_sent_timer->Disable();

	debug_level = 0;
	LOG_PACKETS = false;
	OpMgr = nullptr;
	remote_ip = 0; 
	remote_port = 0;
	packetspending = 0;
	active_users = 0;
	LastPacket=0;
	isWriting = false;
	RateThreshold=RATEBASE/10;
	DecayRate=DECAYBASE/10;
}

EQOldStream::~EQOldStream()
{
	LogNetcodeDetail("Killing EQOldStream");
	safe_delete(no_ack_sent_timer);//delete no_ack_sent_timer;
	safe_delete(keep_alive_timer);//delete keep_alive_timer;
	safe_delete(datarate_timer);
	LogNetcodeDetail("EQOldStream Killing outbound and inbound packet queue");
	RemoveData();
	SetState(CLOSED);
}

void EQOldStream::ResendBefore(uint16 dwARQ)
{
	MOutboundQueue.lock();
	//LogNetcodeDetail(_L "Resend Before Called [{}] B0." __L, dwARQ);
	std::deque<EQOldPacket*>::iterator it;
	for (it = SendQueue.begin(); it != SendQueue.end();)
	{
		if ((*it)->HDR.a1_ARQ && !(*it)->acked) {
			if (dwARQ > 5000) {
				if ((*it)->dwARQ <= dwARQ) {
					if (dwARQ > 60000) {
						if ((*it)->dwARQ > 5000) {
							(*it)->Resend = true;
							//LogNetcodeDetail(_L "Flagging [{}] for resend B2." __L, (*it)->dwARQ);
						}
					}
					else {
						(*it)->Resend = true;
						//LogNetcodeDetail(_L "Flagging [{}] for resend B3." __L, (*it)->dwARQ);
					}
				}
			}
			else {
				if ((*it)->dwARQ <= dwARQ || (*it)->dwARQ > 60000) {
					(*it)->Resend = true;
					//LogNetcodeDetail(_L "Flagging [{}] for resend B4." __L, (*it)->dwARQ);
				}
			}
		}
		it++;
	}

	MOutboundQueue.unlock();
}

void EQOldStream::ResendRequest(uint16 count_size, const uchar* bits, uint16 arsp_start) {
	if (count_size == 0 || bits == nullptr)
		return;
	MOutboundQueue.lock();
	int j = 0;
	int i = 0;
	std::deque<EQOldPacket*>::iterator it;
	uint8 bit_fields = bits[0];
	uint16 match_arsp = arsp_start;
	match_arsp++;
	for (it = SendQueue.begin(); it != SendQueue.end() && i < count_size;)
	{
		if ((*it)->HDR.a1_ARQ && (*it)->dwARQ == match_arsp) {
			if (bit_fields & 0x80) {
				// this is an ack actually
				(*it)->Resend = false;
				(*it)->acked = true;
				//LogNetcodeDetail(_L "Acking packet [{}]." __L, match_arsp);
			} else {
				(*it)->Resend = true;
				//LogNetcodeDetail(_L "Flagging [{}] for resend." __L, match_arsp);
			}
			j++;
			match_arsp++;
			if (j < 8) {
				bit_fields <<= 1;
			}
			else {
				j = 0;
				i++;
				if (i == count_size)
					break;
				bit_fields = bits[i];
			}
		}
		else {
			it++;
		}
	}
	MOutboundQueue.unlock();
}

void EQOldStream::IncomingARSP(uint16 dwARSP) 
{
	MOutboundQueue.lock();
	EQOldPacket* pack = 0;
	
	//LogNetcodeDetail( _L "ARSP Received [{}]. ARSP0" __L, dwARSP);
	bool acked;
	std::deque<EQOldPacket*>::iterator it;
	for(it = SendQueue.begin(); it != SendQueue.end();)
	{
		if ((*it)->HDR.a1_ARQ) {
			acked = false;
			if (dwARSP > 5000) {
				if ((*it)->dwARQ <= dwARSP) {
					if (dwARSP > 60000) {
						if ((*it)->dwARQ > 5000)
							acked = true;
					}
					else {
						acked = true;
					}
				}
			}
			else {
				if ((*it)->dwARQ <= dwARSP || (*it)->dwARQ > 60000) {
					acked = true;
				}
			}
			if (acked) {
				//LogNetcodeDetail(_L "Acking Packet [{}]." __L, (*it)->dwARQ);
				safe_delete(*it);
				it = SendQueue.erase(it);
				dwLastARSP = dwARSP;
			} else {
				it++;
			}
		}
		else {
			it++;
		}
	}

	MOutboundQueue.unlock();
}

void EQOldStream::IncomingARQ(uint16 dwARQ) 
{
	uint16 nextACK = dwLastCACK;
	nextACK++;
	if (dwARQ == nextACK) {
		CACK.dwARQ = dwARQ;
		dwLastCACK = dwARQ;
	}
			    
	if (!no_ack_sent_timer->Enabled())
	{
		no_ack_sent_timer->Start(250); // Agz: If we dont get any outgoing packet we can put an 
		// ack response in before 250ms has passed we send a pure ack response        if (debug_level >= 2)
//            cout << Timer::GetCurrentTime() << " no_ack_sent_timer->Start(200)" << endl;
	}
}

void EQOldStream::OutgoingARQ(uint16 dwARQ)   //An ack request is sent
{
}

void EQOldStream::OutgoingARSP(void)
{
	no_ack_sent_timer->Disable(); // Agz: We have sent the ack response
//    if (debug_level >= 2)
//        cout << Timer::GetCurrentTime() << " no_ack_sent_timer->Disable()" << endl;
}

/************ PARCE A EQPACKET ************/
void EQOldStream::ParceEQPacket(uint16 dwSize, uchar* pPacket)
{
	if(pm_state != EQStreamState::ESTABLISHED)
		return;

	MInboundQueue.lock();
	/************ DECODE PACKET ************/
	EQOldPacket* pack = new EQOldPacket(pPacket, dwSize);
	pack->DecodePacket(dwSize, pPacket);
	if (ProcessPacket(pack, false))
	{
		safe_delete(pack);//delete pack;
	}
	CheckBufferedPackets();
	MInboundQueue.unlock();
}

void EQOldStream::RemoveData()
{
	MInboundQueue.lock();
	MOutboundQueue.lock();
	EQRawApplicationPacket* p = 0;	
	std::vector<EQRawApplicationPacket *>::iterator itr=OutQueue.begin();
	while (itr != OutQueue.end()) {
			safe_delete(*itr);
			itr++;
	}
	OutQueue.clear();
	// Send all the packets we "made"
	for(int i = 0; i < buffered_packets.size(); i++) {
		safe_delete(buffered_packets[i]);
	}
	buffered_packets.clear();
	fragment_group_list.RemoveAll();
	for (auto packit = SendQueue.begin(); packit != SendQueue.end();)
	{
		auto p = (*packit);

		safe_delete(p);
		packit = SendQueue.erase(packit);
	}
	MInboundQueue.unlock();
	MOutboundQueue.unlock();
}

/*
	Return true if its safe to delete this packet now, if we buffer it return false
	this way i can skip memcpy the packet when buffering it
*/
bool EQOldStream::ProcessPacket(EQOldPacket* pack, bool from_buffer)
{
	// simulate incoming packet loss
	//if (pack->HDR.a1_ARQ && rand() % 100 < 20) {
	//	Log(Logs::Detail, Logs::Netcode, _L "Dropping incoming packet to simulate packet loss arq %i" __L, pack->dwARQ);
	//	return true;
	//}
	/************ CHECK FOR ACK/SEQ RESET ************/ 
	if(pack->HDR.a5_SEQStart)
	{
		if (dwLastCACK == (unsigned int)pack->dwARQ - (unsigned int)1)
		{
			LogNetcodeDetail( _L "EQOldStream Packet invalid seqstart? {}:{}" __L, pack->dwARQ, pack->dwSEQ);
			return true;
		}
		//      cout << "resetting SACK.dwGSQ1" << endl;
		//      SACK.dwGSQ      = 0;            //Main sequence number SHORT#2
		dwLastCACK      = pack->dwARQ-1;//0;
		LogNetcodeDetail( _L "EQOldStream Packet seqstart [{}]:[{}]" __L, pack->dwARQ, pack->dwSEQ);
		//      CACK.dwGSQ = 0xFFFF; changed next if to else instead
	}

	CACK.dwGSQ = pack->dwSEQ; //Get current sequence #.
	// LogNetcodeDetail(_L "Packet incoming arq[{}]:seq[{}]:arsp[{}]" __L, pack->dwARQ, pack->dwSEQ, pack->dwARSP);

	/************ Process ack responds ************/
	// Quagmire: Moved this to above "ack request" checking in case the packet is dropped in there
	if (pack->HDR.b2_ARSP) {
		IncomingARSP(pack->dwARSP);
	}
	if (pack->HDR.b3_Unknown) {
		ResendBefore(pack->b3ARSP);
	} 
	if (pack->b4_size > 0) {
		ResendRequest(pack->b4_size, pack->ack_fields, pack->dwARSP);
	}

	/************ End process ack rsp ************/

	// Does this packet contain an ack request?
	if(pack->HDR.a1_ARQ)
	{
		uint16 dwnextCACK = dwLastCACK;
		dwnextCACK++;
		if (pack->dwARQ != dwnextCACK) {
			// Is this packet a resend we have already processed?
			if ((pack->dwARQ == dwLastCACK) || // this was last ack request
				(dwLastCACK > 5000 && pack->dwARQ < dwLastCACK && (dwLastCACK < 60000 || (dwLastCACK > 60000 && pack->dwARQ > 5000))) || // in past
				(dwLastCACK < 5000 && (pack->dwARQ > 60000 || pack->dwARQ < dwLastCACK))) { // in past
				// LogNetcodeDetail( _L "Packet discarded [{}]:[{}]" __L, pack->dwARQ, pack->dwSEQ);
				return true;
			}

			// Debug check, if we want to buffer a packet we got from the buffer something is wrong...
			if (from_buffer)
			{
				return true;
				//cerr << "ERROR: Rebuffering a packet in EQPacketManager::ProcessPacket" << endl;
			}

			std::vector<EQOldPacket*>::iterator iterator;
			iterator = buffered_packets.begin();
			while(iterator != buffered_packets.end())
			{
				if ((*iterator)->dwARQ == pack->dwARQ)
				{
					// LogNetcodeDetail( _L "Packet already buffered [{}]" __L, pack->dwARQ);
					return true; // This packet was already buffered
				}
				iterator++;
			}
			if (!no_ack_sent_timer->Enabled())
				no_ack_sent_timer->Start(250);

			// LogNetcodeDetail( _L "Buffering Packet [{}]" __L, pack->dwARQ);
			buffered_packets.push_back(pack);

			return false;
		}
	}

	// this is for dumping the bit flag header
	/*char* str;
	if (pack->b4_size > 0) {
		int j = 0;
		str = new char[pack->b4_size * 8 + 1];
		for (int i = 0; i < pack->b4_size; i++) {
			for (int k = 0; k < 8; k++) {
				sprintf(&str[i * 8 + k], "%s", pack->ack_fields[i] & 0x80 ? "1" : "0");
				pack->ack_fields[i] <<= 1;
			}

		}
		str[pack->b4_size * 8 + 1] = '\0';
	}
	LogNetcodeDetail( _L "Packet incoming arq%i:seq%i:arsp%i a0:%d|a1:%d|a2:%d|a3:%d|a4:%d|a5:%d|a6:%d|a7:%d|b0:%d|b1:%d|b2:%d|b3:%d|b4:%s|b5:%d|b6:%d|b7:%d" __L,
	pack->dwARQ, pack->dwSEQ, pack->dwARSP,
	pack->HDR.a0_Unknown ? 1 : 0,
	pack->HDR.a1_ARQ ? pack->dbASQ_low : 0,
	pack->HDR.a2_Closing ? 1 : 0,
	pack->HDR.a3_Fragment ? 1 : 0,
	pack->HDR.a4_ASQ ? pack->dbASQ_high : 0,
	pack->HDR.a5_SEQStart ? 1 : 0,
	pack->HDR.a6_Closing ? 1 : 0,
	pack->HDR.a7_SEQEnd ? 1 : 0,
	pack->HDR.b0_SpecARQ ? 1 : 0,
	pack->HDR.b1_Unknown ? 1 : 0,
	pack->HDR.b2_ARSP ? 1 : 0,
	pack->HDR.b3_Unknown ? pack->b3ARSP : 0,
	pack->b4_size > 0 ? str : "0",
	pack->HDR.b5_Unknown ? 1 : 0,
	pack->HDR.b6_Unknown ? 1 : 0,
	pack->HDR.b7_Unknown ? 1 : 0);*/

	/************ START ACK REQ CHECK ************/
	if (pack->HDR.a1_ARQ)
		IncomingARQ(pack->dwARQ);

	if (pack->HDR.b0_SpecARQ || pack->HDR.a5_SEQStart) {
		// Send the ack reponse right away.
		no_ack_sent_timer->Start(250);
		no_ack_sent_timer->Trigger();
	}
	/************ END ACK REQ CHECK ************/

	/************ CHECK FOR THREAD TERMINATION ************/
	if(pack->HDR.a2_Closing && pack->HDR.a6_Closing)
	{
		EQStreamState state = GetState();
		if(state == ESTABLISHED) {
			//client initiated disconnect?
			LogNetcodeDetail( _L "EQOldStream Received OP_SessionDisconnect. Treating like a client-initiated disconnect." __L);
			_SendDisconnect();
			SetState(CLOSED);
		} else if(state == CLOSING) {
			//we were waiting for this anyways, ignore pending messages, send the reply and be closed.
			LogNetcodeDetail( _L "EQOldStream Received OP_SessionDisconnect when we have a pending close, they beat us to it. Were happy though." __L);
			_SendDisconnect();
			SetState(CLOSED);
		} else {
			//we are expecting this (or have already gotten it, but dont care either way)
			LogNetcodeDetail( _L "EQOldStream Received expected OP_SessionDisconnect. Moving to closed state." __L);
			SetState(CLOSED);
		}
		return true;
	}
	/************ END CHECK THREAD TERMINATION ************/

	/************ Get ack sequence number ************/
	if(pack->HDR.a4_ASQ) {
		CACK.dbASQ_high = pack->dbASQ_high;
		//LogNetcodeDetail(_L "Packet Flags a1:%d dbASQ_low: %d a4:%d dbASQ_high: %d" __L, pack->HDR.a1_ARQ, pack->dbASQ_low, pack->HDR.a4_ASQ, pack->dbASQ_high);
	
		if(pack->HDR.a1_ARQ)
			CACK.dbASQ_low = pack->dbASQ_low;
	}
	/************ End get ack seq num ************/

	/************ START FRAGMENT CHECK ************/
	/************ IF - FRAGMENT ************/
	if(pack->HDR.a3_Fragment) 
	{
		FragmentGroup* fragment_group = 0;
		fragment_group = fragment_group_list.Get(pack->fraginfo.dwSeq);

		// If we dont have a fragment group with the right sequence number, create a new one
		if (fragment_group == 0)
		{
			fragment_group = new FragmentGroup(pack->fraginfo.dwSeq,pack->dwOpCode, pack->fraginfo.dwTotal);
			fragment_group_list.Add(fragment_group);
		}

		// Add this fragment to the fragment group
		fragment_group->Add(pack->fraginfo.dwCurr, pack->pExtra,pack->dwExtraSize);

		// If we have all the fragments to complete this group
		if(pack->fraginfo.dwCurr == (pack->fraginfo.dwTotal - 1) )
		{
			//Collect fragments and put them as one packet on the OutQueue
			uchar* buf = 0;
			uint32 sizep = 0;
			buf = fragment_group->AssembleData(&sizep);

			//Why does the client sometimes send fragments with 0-size packets, and why do they get completed?
			//This should help prevent a crash.
			if(sizep == 0 || !buf || fragment_group->GetOpcode() == 0)
			{
				safe_delete_array(buf);
				return true;
			}

			EQRawApplicationPacket *app = new EQRawApplicationPacket(fragment_group->GetOpcode(), buf, sizep);
			safe_delete_array(buf);
			OutQueue.push_back(app);
			fragment_group_list.Remove(pack->fraginfo.dwSeq);
			return true;
		}
		else
		{
			return true;
		}
	}
	/************ ELSE - NO FRAGMENT ************/
	else 
	{

		//seems to be happening, hardening this crap
		if(!pack || pack && !pack->pExtra && pack->dwExtraSize > 0 || pack->dwOpCode == 0)
		{
			return true;
		}


		EQRawApplicationPacket *app=MakeApplicationPacket(pack);
		//if(app->GetRawOpcode() != 62272 && (app->GetRawOpcode() != 0 || app->Size() > 2)) //ClientUpdate
		//	LogNetcodeDetail("Received old opcode - 0x%x size: %i", app->GetRawOpcode(), app->Size());
		if(app)
			OutQueue.push_back(app);
		return true;
	}
	/************ END FRAGMENT CHECK ************/

	return true;
}

void EQOldStream::CheckBufferedPackets()
{
// Should use a hash table or sorted list instead....
	int num=0; // Counting buffered packets for debug output
	uint16 nextARQ = dwLastCACK;
	nextARQ++;
	std::vector<EQOldPacket*>::iterator iterator;
	iterator = buffered_packets.begin();
	while(iterator != buffered_packets.end())
	{
		// Check if we have a packet we want already buffered
		if ((*iterator)->dwARQ == nextARQ)
		{
			//LogNetcodeDetail( _L "Processing Packet fromn Buffer Seq # [{}]" __L, nextARQ);
			ProcessPacket((*iterator), true);
			safe_delete(*iterator);
			iterator = buffered_packets.erase(iterator);
			iterator = buffered_packets.begin();
			nextARQ++;
		}
		else
		{
			iterator++;
		}
	}
}

void EQOldStream::MakeClosePacket()
{
	EQOldPacket *pack = new EQOldPacket();
	pack->HDR.a6_Closing    = 1;// Agz: Lets try to uncomment this line again
	pack->HDR.a2_Closing    = 1;// and this
	pack->HDR.a1_ARQ        = 1;// and this
	pack->acked = false;
//      pack->dwARQ             = 1;// and this, no that was not too good
	pack->dwARQ             = SACK.dwARQ++;// try this instead
	SendQueue.push_back(pack);
	return;
}


/************************************************************************/
/************ Make an EQ packet and put it to the send queue ************/
/* 
	APP->size == 0 && app->pBuffer == nullptr if no data.

	Agz: set ack_req = false if you dont want this packet to require an ack
	response from the client, this menas this packet may get lost and not
	resent. This is used by the EQ servers for HP and position updates among 
	other things. WARNING: I havent tested this yet.
*/
void EQOldStream::MakeEQPacket(EQProtocolPacket* app, bool ack_req)
{
	int16 restore_op = 0x0000;

	/************ PM STATE = NOT ACTIVE ************/
	if(CheckState(CLOSED) || CheckState(CLOSING) || CheckState(DISCONNECTING) || !app || app && app->GetRawOpcode() == 0)
	{
		return;
	}

	// Agz:Moved this to after finish check
	if(app == nullptr)
	{
		//cout << "EQPacketManager::MakeEQPacket app == nullptr" << endl;
		return;
	}
	bool bFragment= false; //This is set later on if fragseq should be increased at the end.

	/************ IF opcode is == 0xFFFF it is a request for pure ack creation ************/
	if(app->GetRawOpcode() == 0xFFFF)
	{
		EQOldPacket *pack = new EQOldPacket();
		if (ack_req) {
			pack->HDR.a1_ARQ = 1;
			pack->dwARQ = SACK.dwARQ++;
			pack->acked = false;
			SACK.dwGSQcount = 0;
		}
		//pack->dwOpCode = 0xFFFF;
		keep_alive_timer->Start();
		MOutboundQueue.lock();
		SendQueue.push_back(pack);
		MOutboundQueue.unlock();
		return;
	}
	if (app->size == 19) {
		if (app->opcode == 0x9f40) {
			// we have a mob update opcode - see if the back of the sendqueue has one to add this to
			MOutboundQueue.lock();
			if (!SendQueue.empty()) {
				EQOldPacket *oldpack;
				oldpack = SendQueue.back();
				if (oldpack->resend_count == 0 && oldpack->dwOpCode == 0x9f40) {
					// have matching OP_MobUpdate
					uint32 count = (oldpack->dwExtraSize - 4) / 15;
					if (count < 30) {
						count++;
						uchar *newdata = new uchar[4 + count * 15];
						uchar *olddata = oldpack->pExtra;

						memcpy((void*)newdata, (void*)olddata, oldpack->dwExtraSize);
						memcpy((void*)newdata, &count, sizeof(uint32));
						newdata += oldpack->dwExtraSize;
						app->pBuffer += 4;
						memcpy((void*)newdata, (void*)app->pBuffer, 15);
						newdata -= oldpack->dwExtraSize;
						app->pBuffer -= 4;
						oldpack->dwExtraSize = (uint16)(4 + count * 15);
						oldpack->pExtra = newdata;
						delete[] olddata;
						MOutboundQueue.unlock();
						return;
					}
				}

			}
			MOutboundQueue.unlock();
		}
	}

	/************ CHECK PACKET MANAGER STATE ************/
	int fragsleft = (app->size >> 9);
	
	if(fragsleft)
	{
		ack_req = true; // Fragmented packets must have ackreq set
	}

	MOutboundQueue.lock();
	if(CheckState(EQStreamState::ESTABLISHED))
	/************ PM STATE = ACTIVE ************/
	{
		for (int i=0; i<=fragsleft; i++)
		{
			EQOldPacket *pack = new EQOldPacket();
			//IF NON PURE ACK THEN ALWAYS INCLUDE A ACKSEQ              // Agz: Not anymore... Always include ackseq if not a fragmented packet
			if (i==0 && ack_req) // If this will be a fragmented packet, only include ackseq in first fragment
				pack->HDR.a4_ASQ = 1;                                   // This is what the eq servers does

			/************ Caculate the next ACKSEQ/acknumber ************/
			/************ Check if its a static ackseq ************/
			if( HI_BYTE(app->GetRawOpcode()) == 0x2000)
			{
				if(app->size == 15)
					pack->dbASQ_low = 0xb2;
				else
					pack->dbASQ_low = 0xa1;

			}
			/************ Normal ackseq ************/
			else
			{
				//If not pure ack and opcode != 0x20XX then
				if (ack_req) { // If the caller of this function wants us to put an ack request on this packet
					pack->HDR.a1_ARQ = 1;
					pack->acked = false;
					pack->dwARQ = SACK.dwARQ++;
					SACK.dwGSQcount = 0;
				}

				if(pack->HDR.a1_ARQ && pack->HDR.a4_ASQ) {
					pack->dbASQ_low  = SACK.dbASQ_low;
					pack->dbASQ_high = SACK.dbASQ_high;
				}
				else if(pack->HDR.a4_ASQ) {
					pack->dbASQ_high = SACK.dbASQ_high;
				}
				if(pack->HDR.a4_ASQ)
					SACK.dbASQ_low++;
			}

			/************ Check if this packet should contain op ************/
			if (app->GetRawOpcode() && i == 0) {
				pack->dwOpCode = app->GetRawOpcode();
			}
			/************ End opcode check ************/

			/************ SHOULD THIS PACKET BE SENT AS A FRAGMENT ************/
			if(fragsleft) {
				pack->HDR.a3_Fragment = 1;
			}
			/************ END FRAGMENT CHECK ************/

			if (i == 0) {
				if (LogSys.log_settings[Logs::PacketServerClient].is_category_enabled == 1) {
					EmuOpcode app_opcode = (*OpMgr)->EQToEmu(app->opcode);
					if (app_opcode != OP_SpecialMesg &&
						(!RuleB(EventLog, SkipCommonPacketLogging) ||
							(RuleB(EventLog, SkipCommonPacketLogging) && app_opcode != OP_MobHealth && app_opcode != OP_MobUpdate && app_opcode != OP_ClientUpdate))) {
						LogPacketServerClient("[{}] - [{:#06x}] Size: [{}] {}", OpcodeManager::EmuToName(app_opcode), app->opcode, app->size, DumpProtocolPacketToString(app).c_str());
					}
				}
			}

			if(app->size && app->pBuffer)
			{
				if(pack->HDR.a3_Fragment)
				{
					// If this is the last packet in the fragment group
					if(i == fragsleft) {
						// Calculate remaining bytes for this fragment
						pack->dwExtraSize = app->size-510-512*((app->size/512)-1);
					}
					else if(i == 0) {
						pack->dwExtraSize = 510; // The first packet in a fragment group has 510 bytes for data
					}
					else {
						pack->dwExtraSize = 512; // Other packets in a fragment group has 512 bytes for data
					}
					pack->fraginfo.dwCurr	= i;
					pack->fraginfo.dwSeq	= dwFragSeq;
					pack->fraginfo.dwTotal	= fragsleft + 1;
				}
				else
				{
					pack->dwExtraSize = (uint16)app->size;
				}

				pack->pExtra = new uchar[pack->dwExtraSize];
				memcpy((void*)pack->pExtra, (void*)app->pBuffer, pack->dwExtraSize);
				app->pBuffer += pack->dwExtraSize; //Increase counter
			}
			/************ End update timers ************/

			SendQueue.push_back(pack);
		}//end while

		if(fragsleft)
		{
			dwFragSeq++;
		}
		app->pBuffer -= app->size; //Restore ptr.
		app->opcode = restore_op;
	} //end if
	MOutboundQueue.unlock();
}

void EQOldStream::CheckTimers(void)
{
	/************ Should packets be resent? ************/

	if (datarate_timer->Check(0))
	{
		datarate_timer->Start();
		dataflow -= datarate_tic;
		if (dataflow < 0)
			dataflow = 0;
	}
}

void EQOldStream::QueuePacket(const EQApplicationPacket *p, bool ack_req)
{
//	ack_req = true;	// It's broke right now, dont delete this line till fix it. =P

	if(p == nullptr)
		return;

	if(OpMgr == nullptr || *OpMgr == nullptr) {
		LogNetcode("EQOldStream Packet enqueued into a stream with no opcode manager, dropping.");
		delete p;
		return;
	}
	uint16 opcode = (*OpMgr)->EmuToEQ(p->emu_opcode);
	EQProtocolPacket* pack2 = new EQProtocolPacket(opcode, p->pBuffer, p->size);
	MakeEQPacket( pack2, ack_req);
	delete pack2;
}

void EQOldStream::FastQueuePacket(EQApplicationPacket **p, bool ack_req)
{
	EQApplicationPacket *pack=*p;
	*p = nullptr;		//clear caller's pointer.. effectively takes ownership

	if(pack == nullptr)
		return;

	if(OpMgr == nullptr || *OpMgr == nullptr) {
		LogNetcodeDetail("Packet enqueued into a stream with no opcode manager, dropping.");
		delete pack;
		return;
	}

	uint16 opcode = (*OpMgr)->EmuToEQ(pack->emu_opcode);

//	ack_req = true;	// It's broke right now, dont delete this line till fix it. =P

	if(p == nullptr)
		return;

	EQProtocolPacket* pack2 = new EQProtocolPacket(opcode, pack->pBuffer, pack->size);

	//if(pack->emu_opcode != OP_MobUpdate && pack->emu_opcode != OP_MobHealth && pack->emu_opcode != OP_HPUpdate)
	//	LogNetcodeDetail( _L "Sending old opcode 0x%04x" __L, opcode);
	MakeEQPacket(pack2, ack_req);
	delete pack;
	delete pack2;
}

EQApplicationPacket *EQOldStream::PopPacket()
{
	EQRawApplicationPacket *p=nullptr;
	MInboundQueue.lock();
	if (OutQueue.size()) {
		std::vector<EQRawApplicationPacket *>::iterator itr=OutQueue.begin();
		p=*itr;
		OutQueue.erase(itr);
	}
	MInboundQueue.unlock();

	if(p)
	{
		if(p->GetRawOpcode() == 0 || p->GetRawOpcode() == 0xFFFF || p->size > 0 && !p->pBuffer)
		{
			safe_delete_array(p->pBuffer);
			safe_delete(p);
			return nullptr;
		}
	}
	//resolve the opcode if we can. do not resolve protocol packets in oldstreams
	if(p) {
		if(OpMgr != nullptr && *OpMgr != nullptr && p->GetRawOpcode() != 0 && p->GetRawOpcode() != 0xFFFF) {
			EmuOpcode emu_op = (*OpMgr)->EQToEmu(p->GetRawOpcode());
			p->SetOpcode(emu_op);
		}
	}
	return p;
}


void EQOldStream::InboundQueueClear()
{
	EQRawApplicationPacket *p=nullptr;

LogNetcode( _L "EQOldStream Clearing inbound queue" __L);

	MInboundQueue.lock();
	std::vector<EQRawApplicationPacket *>::iterator itr=OutQueue.begin();
	while (itr != OutQueue.end()) {
			safe_delete(*itr);
			itr++;
	}
	OutQueue.clear();
	for(int i = 0; i < buffered_packets.size(); i++) {
		safe_delete(buffered_packets[i]);
	}
	buffered_packets.clear();

	MInboundQueue.unlock();
}

void EQOldStream::OutboundQueueClear()
{
	EQOldPacket *p=nullptr;

	LogNetcode( _L "EQOldStream Clearing outbound & resend queue" __L);

	MOutboundQueue.lock();
	for (auto packit = SendQueue.begin(); packit != SendQueue.end();)
	{
		auto p = (*packit);

		safe_delete(p);
		packit = SendQueue.erase(packit);
	}
	SendQueue.clear();

	MOutboundQueue.unlock();
}
void EQOldStream::ReceiveData(uchar* buf, int len)
{
	ParceEQPacket(len, buf);
}

void EQOldStream::SendPacketQueue(bool Block)
{
	MOutboundQueue.lock();
	// Get first send packet on queue and send it!
	EQOldPacket* pack = 0;
	sockaddr_in to;	
	uint32 size;
	uchar* data;
	memset((char *) &to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = remote_port;
	to.sin_addr.s_addr = remote_ip;
	int sentpacket = 0;
	uint32 time_now = Timer::GetCurrentTime();
	std::deque<EQOldPacket*>::iterator packit = SendQueue.begin();

	/************ Should a pure ack be sent? ************/
	if (GetState() == ESTABLISHED) {
		if ((no_ack_sent_timer->Check(0) || keep_alive_timer->Check()))
		{
			EQProtocolPacket app(0xFFFF, nullptr, 0);
			MakeEQPacket(&app, true);
			no_ack_sent_timer->Disable();
		}
	} else if (GetState() == CLOSING) {
		if (!keep_alive_timer->Enabled()) {
			keep_alive_timer->Start(450);
			keep_alive_timer->Trigger();
		}
		if (keep_alive_timer->Check()) {
			// flag resends so we can clear out queue when closing
			packit = SendQueue.begin();
			while (packit != SendQueue.end()) {
				pack = (*packit);
				if (!pack->acked)
				{
					pack->Resend = false;
					pack->LastSent = 0;
				}
				packit++;
			}
		}
	}
	packit = SendQueue.begin();
	while (packit != SendQueue.end() && !DataQueueFull()) {
		pack = (*packit);
		if(pack != nullptr && (pack->resend_count == 0 || pack->LastSent == 0 || // first time sent packets
			(!pack->acked && 
				(pack->Resend && ((time_now - pack->LastSent) > 20)) || // flagged for resend, but was just barely sent
				(!pack->Resend && (pack->resend_count) < 10 && ((time_now - pack->LastSent) > 3000))))) // packet timeout
		{
			if (pack->HDR.a2_Closing && pack->HDR.a6_Closing) //Closing bits. Terminates the connection properly.
			{
				size = pack->ReturnPacket(&data, this);
				sendto(listening_socket, (char*) data, size, 0, (sockaddr*) &to, sizeof(to));
				//LogNetcodeDetail(_L "Sending Closing Packet [{}]." __L, pack->dwARQ);
				dataflow += size;
				pack->LastSent = Timer::GetCurrentTime();
				pack->Resend = false;
				pack->resend_count++;
				safe_delete_array(data);
				if (pack->resend_count > 10) {
					safe_delete(pack);
					packit = SendQueue.erase(packit);
				}
				else {
					packit++;
				}
				continue;
			}
			else //Send a packet!
			{
				if (pack->HDR.a1_ARQ) {
					// if we are too far out on arq's, this will help prevent client from going into desync
					if (pack->dwARQ > dwLastARSP && (pack->dwARQ - dwLastARSP - 100) >= 0)
						break;
					if (pack->dwARQ < dwLastARSP && (65535 - dwLastARSP + pack->dwARQ - 100) >= 0)
						break;
					keep_alive_timer->Disable();
				}
				if (!pack->Resend)
					sentpacket++;
				size = pack->ReturnPacket(&data, this);
				pack->resend_count++;
				pack->Resend = false;

				// uncomment to simulate outgoing packet loss
				//if (pack->resend_count > 25 || rand() % 100 < 20) {
				//	if (pack->HDR.a1_ARQ)
				//		LogNetcodeDetail(_L "Sending Packet [{}]." __L, pack->dwARQ);
				sendto(listening_socket, (char*)data, size, 0, (sockaddr*)&to, sizeof(to));
				//}
				safe_delete_array(data);
				dataflow += size;
				pack->LastSent = Timer::GetCurrentTime();

				if (pack->HDR.a1_ARQ && !pack->HDR.a3_Fragment && (sentpacket > 10) && GetState() == ESTABLISHED) {
					break;
				}

				if (!pack->HDR.a1_ARQ && !pack->HDR.a3_Fragment) {
					safe_delete(pack);
					packit = SendQueue.erase(packit);
					continue;
				}
			}
		}
		packit++;
	}
	if (sentpacket > 0)
		keep_alive_timer->Start(450);
	// ************ Processing finished ************ //
	MOutboundQueue.unlock();
}

void EQOldStream::FlagPacketQueueForResend()
{
	if (GetState() == CLOSED)
		return;

	MOutboundQueue.lock();

	uint32 size;
	uchar* data;
	for (auto packit = SendQueue.begin(); packit != SendQueue.end();)
	{
		auto p = (*packit);
		if (p->HDR.a1_ARQ || p->HDR.a3_Fragment) {
			if (!p->acked) {
				p->LastSent = 0;
				p->Resend = false;
			}
			if (p->Resend) {
				p->LastSent = 0;
				p->Resend = false;
			}
			packit++;
		}
		else {
			safe_delete(p);
			packit = SendQueue.erase(packit);
		}
	}

	// ************ Connection finished ************ //
	MOutboundQueue.unlock();
}

void EQOldStream::ClearPacketQueue()
{
	if (GetState() == CLOSED)
		return;

	MOutboundQueue.lock();

	for (auto packit = SendQueue.begin(); packit != SendQueue.end();)
	{
		auto p = (*packit);

		safe_delete(p);
		packit = SendQueue.erase(packit);
	}
	MOutboundQueue.unlock();

}

void EQOldStream::FinalizePacketQueue()
{
	if(GetState() == CLOSED)
		return;

	MOutboundQueue.lock();
	// Send out our existing queue
	EQOldPacket* p = 0;    
	sockaddr_in to;	
	memset((char *) &to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_port = remote_port;
	to.sin_addr.s_addr = remote_ip;

	uint32 size;
	uchar* data;
	// Set state to closing, and send off the finalized packet.
	if (GetState() != CLOSING)
		MakeClosePacket();
	for (auto packit = SendQueue.begin(); packit != SendQueue.end();)
	{
		p = (*packit);
		if (p->HDR.a1_ARQ || p->HDR.a3_Fragment) {
			if (!p->acked) {
				size = p->ReturnPacket(&data, this);
				sendto(listening_socket, (char*)data, size, 0, (sockaddr*)&to, sizeof(to));
				safe_delete_array(data);
			}
			packit++;
		}
		else {
			safe_delete(p);
			packit = SendQueue.erase(packit);
			continue;
		}
	}
	// ************ Connection finished ************ //
	MOutboundQueue.unlock();
}

bool EQOldStream::HasOutgoingData()
{
	bool flag;

	MOutboundQueue.lock();
	flag=!(SendQueue.empty());
	MOutboundQueue.unlock();
	return flag;
}


void EQOldStream::CheckTimeout(uint32 now, uint32 timeout) {
	bool outgoing_data = HasOutgoingData();	//up here to avoid recursive locking

	EQStreamState orig_state = GetState();
	if (orig_state == CLOSING && !outgoing_data) {
		LogNetcode( _L "EQOldStream Out of data in closing state, disconnecting." __L);
		SetState(CLOSED);
	} else if (LastPacket && (now-LastPacket) > timeout) {
		switch(orig_state) {
		case CLOSING:
			//if we time out in the closing state, they are not acking us, just give up
			LogNetcode( _L "EQOldStream Timeout expired in closing state. Moving to closed state." __L);
			ClearPacketQueue();
			SetState(CLOSED);
			break;
		case DISCONNECTING:
			//we timed out waiting for them to send us the disconnect reply, just give up.
			LogNetcode( _L "EQOldStream Timeout expired in disconnecting state. Moving to closed state." __L);
			ClearPacketQueue();
			SetState(CLOSED);
			break;
		case CLOSED:
			LogNetcode( _L "EQOldStream Timeout expired in closed state??" __L);
			ClearPacketQueue();
			break;
		case ESTABLISHED:
			//we timed out during normal operation. Try to be nice about it.
			//we will almost certainly time out again waiting for the disconnect reply, but oh well.
			LogNetcode( _L "EQOldStream Timeout expired in established state. Closing connection." __L);
			_SendDisconnect();
			SetState(DISCONNECTING);
			break;
		default:
			break;
		}
	}
}

void EQOldStream::SetState(EQStreamState state) {
	MState.lock();

	if(pm_state == CLOSED)
	{
		MState.unlock();
		return;
	}

	LogNetcodeDetail(_L "[EQOldStream] Changing state from[{}] to[{}]" __L, (int)pm_state, (int)state);
	pm_state=state;
	MState.unlock();
}

void EQOldStream::SetStreamType(EQStreamType type)
{
	LogNetcode("[{}]:[{}]: EQOldStream Changing stream type from [{}] to [{}]", long2ip(remote_ip), ntohs(remote_port), StreamTypeString(StreamType), StreamTypeString(type));
	StreamType=type;
}

//this could be expanded to check more than the fitst opcode if
//we needed more complex matching
EQStream::MatchState EQOldStream::CheckSignature(const EQStream::Signature *sig) {
	EQRawApplicationPacket *p = nullptr;
	EQStream::MatchState res = EQStream::MatchState::MatchNotReady;
	MInboundQueue.lock();
	if (!OutQueue.empty()) {
		//this is already getting hackish...
		std::vector<EQRawApplicationPacket *>::iterator itr=OutQueue.begin();
		p=*itr;
		if(sig->ignore_eq_opcode != 0 && p->GetRawOpcode() == sig->ignore_eq_opcode) {
			if(OutQueue.empty()) {
				p = nullptr;
			}
		}
		if(p == nullptr) {
			//first opcode is ignored, and nothing else remains... keep waiting
		} else if(p->GetRawOpcode() == sig->first_eq_opcode) {
			//opcode matches, check length..
			if(p->size == sig->first_length) {
				LogNetcode("[StreamIdentify:EQOldStream] [{}]:[{}]: First opcode matched [{:#06x}] and length matched [{}]", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), sig->first_eq_opcode, p->size);
				res = EQStream::MatchState::MatchSuccessful;
			} else if(sig->first_length == 0) {
				LogNetcode("[StreamIdentify:EQOldStream] [{}]:[{}]: First opcode matched [{:#06x}] and length [({})] is ignored", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), sig->first_eq_opcode, p->size);
				res = EQStream::MatchState::MatchSuccessful;
			} else {
				//opcode matched but length did not.
				LogNetcode("[StreamIdentify:EQOldStream] [{}]:[{}]: First opcode matched [{:#06x}], but length [{}] did not match expected [{}]", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), sig->first_eq_opcode, p->size, sig->first_length);
				res = EQStream::MatchState::MatchFailed;
			}
		} else {
			//first opcode did not match..
			LogNetcode("[StreamIdentify:EQOldStream] [{}]:[{}]: First opcode [{:#06x}] did not match expected [{:#06x}]", long2ip(GetRemoteIP()).c_str(), ntohs(GetRemotePort()), p->GetRawOpcode(), sig->first_eq_opcode);
			res = EQStream::MatchState::MatchFailed;
		}
	}
	MInboundQueue.unlock();

	return(res);
}

void EQOldStream::_SendDisconnect()
{
	FinalizePacketQueue();
}
void EQOldStream::Close() {
	if(HasOutgoingData()) {
		//there is pending data, wait for it to go out.
		LogNetcode("[{}]:[{}]: EQOldStream requested to Close(), but there is pending data, waiting for it.", long2ip(remote_ip), ntohs(remote_port));
		MOutboundQueue.lock();
		MakeClosePacket();
		MOutboundQueue.unlock();
		keep_alive_timer->Stop();
		FlagPacketQueueForResend();
		SetState(CLOSING);
	} else {
		//otherwise, we are done, we can drop immediately.
		_SendDisconnect();
		LogNetcode("[{}]:[{}]: EQOldStream  closing immediate due to Close()", long2ip(remote_ip).c_str(), ntohs(remote_port));
		SetState(DISCONNECTING);
	}
}

EQRawApplicationPacket *EQOldStream::MakeApplicationPacket(EQOldPacket *p)
{
	EQRawApplicationPacket *ap=nullptr;
	LogNetcode("[{}]:[{}]: EQOldStream Creating old application packet, length [{}]", long2ip(remote_ip), ntohs(remote_port), p->dwExtraSize);
	ap = p->MakeAppPacket();
	return ap;
}
