#ifndef _EQSTREAM_H
#define _EQSTREAM_H

#include <vector>
#include <map>
#include <queue>
#include <deque>

#ifndef WIN32
#include <netinet/in.h>
#endif

#include "../common/misc.h"
#include "../common/opcodemgr.h"
#include "../common/timer.h"

#include "eq_packet.h"
#include "eq_stream_intf.h"
#include "eq_stream_type.h"
#include "mutex.h"
#include "queue.h"
#include "eqemu_logsys.h"

//for logsys
//#define _L "%s:%d: "
//#define __L , long2ip(remote_ip).c_str(), ntohs(remote_port)

class EQApplicationPacket;
class EQProtocolPacket;

#define FLAG_COMPRESSED	0x01
#define FLAG_ENCODED	0x04

#ifndef RATEBASE
#define RATEBASE	1048576
#endif

#ifndef DECAYBASE
#define DECAYBASE	78642
#endif

#ifndef RETRANSMIT_TIMEOUT_MULT
#define RETRANSMIT_TIMEOUT_MULT 3.0
#endif

#ifndef RETRANSMIT_TIMEOUT_MAX
#define RETRANSMIT_TIMEOUT_MAX 5000
#endif

#ifndef AVERAGE_DELTA_MAX
#define AVERAGE_DELTA_MAX 2500
#endif

#ifndef RETRANSMIT_ACKED_PACKETS
#define RETRANSMIT_ACKED_PACKETS true
#endif

#ifndef MAX_SESSION_RETRIES
#define MAX_SESSION_RETRIES 30
#endif

#define PM_ACTIVE    0 // Comment: manager is up and running
#define PM_FINISHING 1 // Comment: manager received closing bits and is going to send final packet
#define PM_FINISHED  2 // Comment: manager has sent closing bits back to client


template <typename type>                    // LO_BYTE
type  LO_BYTE (type a) {return (a&=0xff);}  
template <typename type>                    // HI_BYTE 
type  HI_BYTE (type a) {return (a&=0xff00);} 
template <typename type>                    // LO_WORD
type  LO_WORD (type a) {return (a&=0xffff);}  
template <typename type>                    // HI_WORD 
type  HI_WORD (type a) {return (a&=0xffff0000);} 
template <typename type>                    // HI_LOSWAPshort
type  HI_LOSWAPshort (type a) {return (LO_BYTE(a)<<8) | (HI_BYTE(a)>>8);}  
template <typename type>                    // HI_LOSWAPlong
type  HI_LOSWAPlong (type a) {return (LO_WORD(a)<<16) | (HIWORD(a)>>16);}  

#define EQOLDSTREAM_OUTBOUD_THRESHOLD 9

// Added struct
typedef struct
{
	uchar*  buffer;
	uint16   size;
}MySendPacketStruct;


struct ACK_INFO
{
	ACK_INFO() 
	{
		// Set properties to 0
		dwARQ = 0;
		dbASQ_high = dbASQ_low = 0;
		dwGSQ = 0;
		dwGSQcount = 0;
	}

	uint16   dwARQ;			// Comment: Current request ack
	uint16   dbASQ_high : 8;	//TODO: What is this one?
	uint16	dbASQ_low  : 8;	//TODO: What is this one?
	uint16   dwGSQ;			// Comment: Main sequence number SHORT#2
	uint16	dwGSQcount;

};

#pragma pack(1)
struct SessionRequest {
	uint32 UnknownA;
	uint32 Session;
	uint32 MaxLength;
};

struct SessionResponse {
	uint32 Session;
	uint32 Key;
	uint8 UnknownA;
	uint8 Format;
	uint8 UnknownB;
	uint32 MaxLength;
	uint32 UnknownD;
};

//Deltas are in ms, representing round trip times
struct SessionStats {
/*000*/	uint16 RequestID;
/*002*/	uint32 last_local_delta;
/*006*/	uint32 average_delta;
/*010*/	uint32 low_delta;
/*014*/	uint32 high_delta;
/*018*/	uint32 last_remote_delta;
/*022*/	uint64 packets_sent;
/*030*/	uint64 packets_received;
/*038*/
};

#pragma pack()

class OpcodeManager;
class EQRawApplicationPacket;

class Fragment
{
public:
	Fragment();
	~Fragment();

	void SetData(uchar* d, uint32 s);
	void ClearData();

	uchar* GetData() 
	{
		return this->data;
	}

	uint32  GetSize() 
	{ 
		return this->size; 
	}

private:
	uchar*	data;
	uint32	size;
};

class FragmentGroup
{
public:
	FragmentGroup(uint16 seq, uint16 opcode, uint16 num_fragments);
	~FragmentGroup();

	void Add(uint16 frag_id, uchar* data, uint32 size);
	uchar* AssembleData(uint32* size);

	uint16 GetSeq()
	{
		return seq;
	}

	uint16 GetOpcode()
	{
		return opcode;
	}

private:
	uint16 seq;				// Sequence number
	uint16 opcode;			// Fragment group's opcode
	uint16 num_fragments;	//TODO: What is this one?
	Fragment* fragment;		//TODO: What is this one?
};


class FragmentGroupList
{
public:
	void Add(FragmentGroup* add_group);
	FragmentGroup* Get(uint16 find_seq);
	void Remove(uint16 remove_seq);
	void RemoveAll();

private:
	std::vector<FragmentGroup*> fragment_group_list;
};

class EQStream : public EQStreamInterface {
	friend class EQStreamPair;	//for collector.
	protected:
		typedef enum {
			SeqPast,
			SeqInOrder,
			SeqFuture
		} SeqOrder;

		uint32 remote_ip;
		uint16 remote_port;
		uint8 buffer[8192];
		unsigned char *oversize_buffer;
		uint32 oversize_offset,oversize_length;
		uint8 app_opcode_size;
		EQStreamType StreamType;
		bool compressed,encoded;
		uint32 retransmittimer;
		uint32 retransmittimeout;

		uint16 sessionAttempts;
		bool streamactive;

		//uint32 buffer_len;

		uint32 Session, Key;
		uint16 NextInSeq;
		uint32 MaxLen;
		uint16 MaxSends;
		uint16 stale_count;

		uint8 active_users;	//how many things are actively using this
		Mutex MInUse;

		EQStreamState State;
		Mutex MState;

		uint32 LastPacket;
		uint32 LastSent;
		Mutex MVarlock;

		// Ack sequence tracking.
		long NextAckToSend;
		long LastAckSent;
		long GetNextAckToSend();
		long GetLastAckSent();
		void AckPackets(uint16 seq);
		void SetNextAckToSend(uint32);
		void SetLastAckSent(uint32);

		Mutex MAcks;

		// Packets waiting to be sent (all protected by MOutboundQueue)
		std::queue<EQProtocolPacket *> NonSequencedQueue;
		std::deque<EQProtocolPacket *> SequencedQueue;
		uint16 NextOutSeq;
		uint16 SequencedBase;	//the sequence number of SequencedQueue[0]
		bool stream_startup;
		long NextSequencedSend;	//index into SequencedQueue
		Mutex MOutboundQueue;

		//a buffer we use for compression/decompression
		unsigned char _tempBuffer[2048];

		// Packets waiting to be processed
		std::vector<EQRawApplicationPacket *> InboundQueue;
		std::map<unsigned short,EQProtocolPacket *> PacketQueue;		//not mutex protected, only accessed by caller of Process()
		Mutex MInboundQueue;

		static uint16 MaxWindowSize;

		int32 BytesWritten;

		Mutex MRate;
		int32 RateThreshold;
		int32 DecayRate;


		OpcodeManager **OpMgr;

		EQRawApplicationPacket *MakeApplicationPacket(EQProtocolPacket *p);
		EQRawApplicationPacket *MakeApplicationPacket(const unsigned char *buf, uint32 len);
		EQProtocolPacket *MakeProtocolPacket(const unsigned char *buf, uint32 len);
		void SendPacket(uint16 opcode, EQApplicationPacket *p);

		void SetState(EQStreamState state);

		void SendSessionResponse();
		void SendSessionRequest();
		void SendKeepAlive();
		void SendAck(uint16 seq);
		void SendOutOfOrderAck(uint16 seq);
		void QueuePacket(EQProtocolPacket *p);
		void SendPacket(EQProtocolPacket *p);
		void NonSequencedPush(EQProtocolPacket *p);
		void SequencedPush(EQProtocolPacket *p);
		void WritePacket(int fd,EQProtocolPacket *p);


		uint32 GetKey() { return Key; }
		void SetKey(uint32 k) { Key=k; }
		void SetSession(uint32 s) { Session=s; }

		void ProcessPacket(EQProtocolPacket *p);

		void InboundQueuePush(EQRawApplicationPacket *p);
		EQRawApplicationPacket *PeekPacket();	//for collector.
		EQRawApplicationPacket *PopRawPacket();	//for collector.

		void InboundQueueClear();
		void OutboundQueueClear();
		void PacketQueueClear();

		void ProcessQueue();
		EQProtocolPacket *RemoveQueue(uint16 seq);

		void _SendDisconnect();

		void init(bool resetSession = true);
	public:
		EQStream() { init(); remote_ip = 0; remote_port = 0; State = UNESTABLISHED; StreamType = UnknownStream; compressed = true; encoded = false; app_opcode_size = 2; bytes_sent = 0; bytes_recv = 0; create_time = Timer::GetTimeSeconds(); sessionAttempts = 0; streamactive = false; }
		EQStream(sockaddr_in addr) { init(); remote_ip = addr.sin_addr.s_addr; remote_port = addr.sin_port; State = UNESTABLISHED; StreamType = UnknownStream; compressed = true; encoded = false; app_opcode_size = 2; bytes_sent = 0; bytes_recv = 0; create_time = Timer::GetTimeSeconds(); sessionAttempts = 0; streamactive = false; }
		virtual ~EQStream() { RemoveData(); SetState(CLOSED); }
		void SetMaxLen(uint32 length) { MaxLen=length; }

		//interface used by application (EQStreamInterface)
		virtual void QueuePacket(const EQApplicationPacket *p, bool ack_req=true);
		virtual void FastQueuePacket(EQApplicationPacket **p, bool ack_req=true);
		virtual EQApplicationPacket *PopPacket();
		virtual void Close();
		virtual uint32 GetRemoteIP() const { return remote_ip; }
		virtual uint16 GetRemotePort() const { return remote_port; }
		virtual void ReleaseFromUse() { /* LogNetcodeDetail( _L "ReleaseFromUse [{}]" __L, active_users); */ MInUse.lock(); if (active_users > 0) active_users--; MInUse.unlock(); }
		virtual void RemoveData() { InboundQueueClear(); OutboundQueueClear(); PacketQueueClear(); /*if (CombinedAppPacket) delete CombinedAppPacket;*/ }
		virtual bool CheckState(EQStreamState state) { return GetState() == state; }
		virtual std::string Describe() const { return("Direct EQStream"); }

		void SetOpcodeManager(OpcodeManager **opm) { OpMgr = opm; }
		virtual OpcodeManager *GetOpcodeManager() const
		{
			return (*OpMgr);
		};

		void CheckTimeout(uint32 now, uint32 timeout=30000);
		bool HasOutgoingData();
		void Process(const unsigned char *data, const uint32 length);
		void SetLastPacketTime(uint32 t) {LastPacket=t;}
		void SetLastSentTime(uint32 t) { LastSent = t; }
		void Write(int eq_fd);

		// whether or not the stream has been assigned (we passed our stream match)
		void SetActive(bool val) { streamactive = val; }

		virtual bool IsInUse() { bool flag; MInUse.lock(); flag=(active_users>0); MInUse.unlock(); return flag; }
		inline void PutInUse() { /* LogNetcodeDetail( _L "PutInUse [{}]" __L, active_users); */ MInUse.lock(); active_users++; MInUse.unlock(); }

		inline EQStreamState GetState() { EQStreamState s; MState.lock(); s=State; MState.unlock(); return s; }

		static SeqOrder CompareSequence(uint16 expected_seq , uint16 seq);

		bool CheckActive() { return GetState()==ESTABLISHED; }
		bool CheckClosed() { return GetState()==CLOSED; }
		void SetOpcodeSize(uint8 s) { app_opcode_size = s; }
		void SetStreamType(EQStreamType t);
		inline const EQStreamType GetStreamType() const { return StreamType; }
		static const char *StreamTypeString(EQStreamType t);

		void Decay();
		void AdjustRates(uint32 average_delta);

		uint32 bytes_sent;
		uint32 bytes_recv;
		uint32 create_time;

		void AddBytesSent(uint32 bytes)
		{
			bytes_sent += bytes;
		}

		void AddBytesRecv(uint32 bytes)
		{
			bytes_recv += bytes;
		}

		virtual const uint32 GetBytesSent() const { return bytes_sent; }
		virtual const uint32 GetBytesRecieved() const { return bytes_recv; }
		virtual const uint32 GetBytesSentPerSecond() const
		{
			if((Timer::GetTimeSeconds() - create_time) == 0)
				return 0;
			return bytes_sent / (Timer::GetTimeSeconds() - create_time);
		}

		virtual const uint32 GetBytesRecvPerSecond() const
		{
			if((Timer::GetTimeSeconds() - create_time) == 0)
				return 0;
			return bytes_recv / (Timer::GetTimeSeconds() - create_time);
		}

		//used for dynamic stream identification
		class Signature {
		public:
			//this object could get more complicated if needed...
			uint16 ignore_eq_opcode;		//0=dont ignore
			uint16 first_eq_opcode;
			uint32 first_length;			//0=dont check length
		};
		typedef enum {
			MatchNotReady,
			MatchSuccessful,
			MatchFailed
		} MatchState;
		MatchState CheckSignature(const Signature *sig);

};


class EQOldStream : public EQStreamInterface {
	friend class EQStreamPair;	//for collector.
	friend class EQStream;

	public:
		EQOldStream();
		EQOldStream(sockaddr_in in, int fd_sock);
		~EQOldStream();

	protected:
		Mutex MResendQueue;
		Mutex MOutboundQueue;
		Mutex MInboundQueue;
		uint32 remote_ip;
		uint16 remote_port;
		EQStreamState State;
		Mutex MState;
		EQStreamType StreamType;

		uint8 active_users;	//how many things are actively using this
		Mutex MInUse;

	public:
		bool IsTooMuchPending()
		{
			return (packetspending > EQOLDSTREAM_OUTBOUD_THRESHOLD) ? true : false;
		}

		int16 PacketsPending()
		{
			return packetspending;
		}

		void SetDebugLevel(int8 set_level)
		{
			debug_level = set_level;
		}

		void LogPackets(bool logging) 
		{
			LOG_PACKETS = logging; 
		}
				
		// parce/make packets
		void ParceEQPacket(uint16 dwSize, uchar* pPacket);
		void MakeEQPacket(EQProtocolPacket* app, bool ack_req=true); //Make a fragment eq packet and put them on the SQUEUE/RSQUEUE
		void MakeClosePacket();
		// Add ack to packet if requested
		void AddAck(EQOldPacket *pack)
		{
			if(CACK.dwARQ)
			{       
				pack->HDR.b2_ARSP = 1;          //Set ack response field
				pack->dwARSP = CACK.dwARQ;      //ACK current ack number.
				CACK.dwARQ = 0;
			}
		}
		// Timer Functions
				
		//Check all class timers and call proper functions
		void CheckTimers(void); 

		int CheckActive(void) 
		{ 
			if(pm_state == CLOSED)
			{
				return(0);
			}
			else
			{
				return(1);
			}
		}

		virtual void Close();

		// Incomming / Outgoing Ack's
		void IncomingARSP(uint16 dwARSP); 
		void IncomingARQ(uint16 dwARQ);
		void OutgoingARQ(uint16 dwARQ);
		void OutgoingARSP();
		void ResendRequest(uint16 count_size, const uchar* bits, uint16 arsp_start);
		void ResendBefore(uint16 dwARQ);

		void InboundQueueClear();
		void OutboundQueueClear();

		std::deque<EQOldPacket*>			  SendQueue;	//Store packets thats on the send que
		std::vector<EQRawApplicationPacket *> OutQueue;	//parced packets ready to go out of this class


	private:
		bool ProcessPacket(EQOldPacket* pack, bool from_buffer=false);
		void CheckBufferedPackets();
		EQRawApplicationPacket *MakeApplicationPacket(EQOldPacket *p);

		FragmentGroupList fragment_group_list;
		std::vector<EQOldPacket *> buffered_packets; // Buffer of incoming packets

		EQStreamState    pm_state;  //manager state 
		uint16  dwFragSeq;   //current fragseq
		int8 debug_level;
		bool LOG_PACKETS;
		bool bTimeout;
		bool bTimeoutTrigger;
		bool isWriting;
		int16	packetspending;
		OpcodeManager **OpMgr;
		int listening_socket;
		uint16 arsp_response;

		Mutex MRate;
		int32 RateThreshold;
		int32 DecayRate;

		uint32 LastPacket;
		Mutex MVarlock;
		bool sent_Fin;
		
		int32	datarate_sec;	// bytes/1000ms
		int32	datarate_tic;	// bytes/100ms
		int32	dataflow;

	public:
		//interface used by application (EQStreamInterface)
		virtual void QueuePacket(const EQApplicationPacket *p, bool ack_req=true);
		virtual void FastQueuePacket(EQApplicationPacket **p, bool ack_req=true);
		virtual EQApplicationPacket *PopPacket();
		virtual uint32 GetRemoteIP() const { return remote_ip; }
		virtual uint16 GetRemotePort() const { return remote_port; }
		virtual void ReleaseFromUse() { MInUse.lock(); if(active_users > 0) active_users--; MInUse.unlock(); }
		virtual void RemoveData();
		virtual bool CheckState(EQStreamState state) { return GetState() == state; }
		virtual std::string Describe() const { return("Direct EQOldStream"); }
		virtual bool IsInUse() { bool flag; MInUse.lock(); flag=(active_users>0); MInUse.unlock(); return flag; }
		bool IsWriting() { return isWriting; }
		void SetWriting(bool var) { isWriting = var; } 
		inline void PutInUse() { MInUse.lock(); active_users++; MInUse.unlock(); }
		inline EQStreamState GetState() { EQStreamState s; MState.lock(); s=pm_state; MState.unlock(); return s; }
		void	SendPacketQueue(bool Block = true);
		void	FinalizePacketQueue();
		void	ClearPacketQueue();
		void	FlagPacketQueueForResend();
		void	ReceiveData(uchar* buf, int len);
		void SetStreamType(EQStreamType t);
		inline const EQStreamType GetStreamType() const { return StreamType; }
		static const char *StreamTypeString(EQStreamType t);
		virtual bool IsOldStream()			const { return true; }
		EQStream::MatchState CheckSignature(const EQStream::Signature *sig);
		bool HasOutgoingData();
		bool CheckClosed() { return GetState()==CLOSED; }
		void Process(const unsigned char *data, const uint32 length);
		void CheckTimeout(uint32 now, uint32 timeout=10000);
		void SetState(EQStreamState state);
		void SetLastPacketTime(uint32 t) {LastPacket=t;}

		void SetOpcodeManager(OpcodeManager **opm) { OpMgr = opm; }
		virtual OpcodeManager *GetOpcodeManager() const
		{
			return (*OpMgr);
		};

		void _SendDisconnect();
		void SetTimeOut(bool time) { bTimeout = time; }
		bool GetTimeOut() { return bTimeout; }
		void			SetDataRate(float in_datarate)	{ datarate_sec = (int32) (in_datarate * 1024); datarate_tic = datarate_sec / 10; dataflow = 0; } // conversion from kb/sec to byte/100ms, byte/1000ms
		float			GetDataRate()					{ return (float)datarate_sec / 1024; } // conversion back to kb/second
		inline bool		DataQueueFull()					{ return (dataflow > datarate_sec); }
		inline int32	GetDataFlow()					{ return dataflow; }
		ACK_INFO    SACK; //Server -> client info.
		ACK_INFO    CACK; //Client -> server info.
		uint16       dwLastCACK;
		uint16		 dwLastARSP;
		bool sent_Start;
		Timer* no_ack_sent_timer;
		Timer* keep_alive_timer;
		Timer*	datarate_timer;


};

#endif

