#include "global_define.h"
#include "eqemu_logsys.h"
#include "eq_stream_factory.h"

#ifdef _WINDOWS
	#include <winsock2.h>
	#include <process.h>
	#include <io.h>
	#include <stdio.h>
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <sys/select.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <pthread.h>
#endif

#include <iostream>
#include <fcntl.h>

#include "op_codes.h"

ThreadReturnType EQStreamFactoryReaderLoop(void *eqfs)
{
	EQStreamFactory *fs=(EQStreamFactory *)eqfs;

	fs->ReaderLoop();

	THREAD_RETURN(nullptr);
}

ThreadReturnType EQStreamFactoryWriterLoop(void *eqfs)
{
	EQStreamFactory *fs=(EQStreamFactory *)eqfs;

	fs->WriterLoop();

	THREAD_RETURN(nullptr);
}

EQStreamFactory::EQStreamFactory(EQStreamType type, int port, uint32 timeout)
	: Timeoutable(5000), stream_timeout(timeout)
{
	StreamType = type;
	Port = port;
	sock =-1;
}

void EQStreamFactory::Close()
{
	Stop();

#ifdef _WINDOWS
	closesocket(sock);
#else
	close(sock);
#endif
	sock = -1;
}

bool EQStreamFactory::Open()
{
struct sockaddr_in address;
#ifndef WIN32
	pthread_t t1,t2;
#endif
	/* Setup internet address information.
	This is used with the bind() call */
	memset((char *) &address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(Port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	/* Setting up UDP port for new clients */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		return false;
	}

	if (bind(sock, (struct sockaddr *) &address, sizeof(address)) < 0) {
		close(sock);
		sock = -1;
		return false;
	}
	#ifdef _WINDOWS
		unsigned long nonblock = 1;
		ioctlsocket(sock, FIONBIO, &nonblock);
	#else
		fcntl(sock, F_SETFL, O_NONBLOCK);
	#endif
	//moved these because on windows the output was delayed and causing the console window to look bad
	#ifdef _WINDOWS
		_beginthread(EQStreamFactoryReaderLoop,0, this);
		_beginthread(EQStreamFactoryWriterLoop,0, this);
	#else
		pthread_create(&t1,nullptr,EQStreamFactoryReaderLoop,this);
		pthread_create(&t2,nullptr,EQStreamFactoryWriterLoop,this);
	#endif
	return true;
}

EQStream *EQStreamFactory::Pop()
{
EQStream *s = nullptr;
	MNewStreams.lock();
	if (!NewStreams.empty()) {
		s=NewStreams.front();
		NewStreams.pop();
	}
	MNewStreams.unlock();

	return s;
}

void EQStreamFactory::PushOld(EQOldStream *s)
{
	//cout << "Push():Locking MNewStreams" << endl;
	MNewStreams.lock();
	try {
		s->PutInUse();
	}
	catch (...) {
		fprintf(stderr, "Catching Stream Crash.");
		MNewStreams.unlock();
		return;
	}
	NewOldStreams.push(s);
	MNewStreams.unlock();
	//cout << "Push(): Unlocking MNewStreams" << endl;
}


void EQStreamFactory::Push(EQStream *s)
{
	MNewStreams.lock();
	try {
		s->PutInUse();
	}
	catch (...) {
		fprintf(stderr, "Catching Stream Crash.");
		MNewStreams.unlock();
		return;
	}
	NewStreams.push(s);
	MNewStreams.unlock();
}


EQOldStream *EQStreamFactory::PopOld()
{
EQOldStream *s=nullptr;
	//cout << "Pop():Locking MNewStreams" << endl;
	MNewStreams.lock();
	if (NewOldStreams.size()) {
		s=NewOldStreams.front();
		NewOldStreams.pop();
	}
	MNewStreams.unlock();
	//cout << "Pop(): Unlocking MNewStreams" << endl;

	return s;
}

void EQStreamFactory::ReaderLoop()
{
fd_set readset;
std::map<std::pair<uint32, uint16>, EQStream*>::iterator stream_itr;
std::map<std::pair<uint32, uint16>, EQOldStream *>::iterator oldstream_itr;
int num;
int length;
unsigned char buffer[2048];
sockaddr_in from;
int socklen=sizeof(sockaddr_in);
timeval sleep_time;
//time_t now;

	ReaderRunning=true;
	while(sock!=-1) {
		MReaderRunning.lock();
		if (!ReaderRunning) {
			MReaderRunning.unlock();
			break;
		}
		MReaderRunning.unlock();

		FD_ZERO(&readset);
		FD_SET(sock,&readset);

		sleep_time.tv_sec=30;
		sleep_time.tv_usec=0;
		if ((num=select(sock+1,&readset,nullptr,nullptr,&sleep_time))<0) {
			// What do we wanna do?
			continue;
		} else if (num==0)
			continue;

		if(sock == -1)
			break;		//somebody closed us while we were sleeping.

		if (FD_ISSET(sock,&readset)) {
#ifdef _WINDOWS
			if ((length=recvfrom(sock,(char*)buffer,sizeof(buffer),0,(struct sockaddr*)&from,(int *)&socklen)) < 2)
#else
			if ((length=recvfrom(sock,buffer,2048,0,(struct sockaddr *)&from,(socklen_t *)&socklen)) < 2)
#endif
			{
				// What do we wanna do?
			} else {
				MStreams.lock();
				stream_itr = Streams.find(std::make_pair(from.sin_addr.s_addr, from.sin_port));
				oldstream_itr = OldStreams.find(std::make_pair(from.sin_addr.s_addr, from.sin_port));
				if (stream_itr == Streams.end() && oldstream_itr == OldStreams.end()) {
					if (buffer[1]==OP_SessionRequest) {
						EQStream *s = new EQStream(from);
						s->SetStreamType(StreamType);
						Streams[std::make_pair(from.sin_addr.s_addr, from.sin_port)]=s;
						Push(s);
						WriterWork.Signal();
						s->AddBytesRecv(length);
						s->Process(buffer,length);
						s->SetLastPacketTime(Timer::GetCurrentTime());
					}
					else {
						EQOldStream *s = new EQOldStream(from, sock);
						s->SetStreamType(OldStream);
						OldStreams[std::make_pair(from.sin_addr.s_addr, from.sin_port)]=s;
						PushOld(s);
						WriterWork.Signal();
						//s->AddBytesRecv(length);
						s->SetLastPacketTime(Timer::GetCurrentTime());
						s->ReceiveData(buffer,length);
					}

					MStreams.unlock();
				} else {

					//newstr
					stream_itr = Streams.find(std::make_pair(from.sin_addr.s_addr, from.sin_port));
					EQStream *curstream = nullptr;
					if(stream_itr != Streams.end())
					curstream = stream_itr->second;
					//oldstr
					oldstream_itr = OldStreams.find(std::make_pair(from.sin_addr.s_addr, from.sin_port));
					EQOldStream *oldcurstream = nullptr;
					if(oldstream_itr != OldStreams.end())
					oldcurstream = oldstream_itr->second;

					if(curstream != nullptr)
					{
						//dont bother processing incoming packets for closed connections
						if(curstream->CheckClosed())
							curstream = nullptr;
						else
							curstream->PutInUse();
						MStreams.unlock();	//the in use flag prevents the stream from being deleted while we are using it.

						if(curstream) {
							curstream->AddBytesRecv(length);
							curstream->Process(buffer,length);
							curstream->SetLastPacketTime(Timer::GetCurrentTime());
							curstream->ReleaseFromUse();
						}
					}
					else if(oldcurstream != nullptr)
					{
						if(oldcurstream->CheckClosed())
							oldcurstream = nullptr;
						else
							oldcurstream->PutInUse();

						MStreams.unlock();	//the in use flag prevents the stream from being deleted while we are using it.

						if(oldcurstream) {
							//oldcurstream->AddBytesRecv(length);
							oldcurstream->ParceEQPacket(length, buffer);
							oldcurstream->SetLastPacketTime(Timer::GetCurrentTime());
							oldcurstream->ReleaseFromUse();
						}
					}
					else
					{
						MStreams.unlock();
					}
				}
			}
		}
	}
}

void EQStreamFactory::CheckTimeout()
{
	//lock streams the entire time were checking timeouts, it should be fast.
	MStreams.lock();

	unsigned long now=Timer::GetCurrentTime();
	std::map<std::pair<uint32, uint16>,EQStream *>::iterator stream_itr;

	for(stream_itr = Streams.begin();stream_itr != Streams.end();) {
		EQStream *s = stream_itr->second;

		s->CheckTimeout(now, stream_timeout);

		EQStreamState state = s->GetState();

		//not part of the else so we check it right away on state change
		if (state==CLOSED) {
			if (s->IsInUse()) {
				//give it a little time for everybody to finish with it
			} else {
				//everybody is done, we can delete it now
				auto temp = stream_itr;
				++stream_itr;
				//let whoever has the stream outside delete it
				delete temp->second;
				Streams.erase(temp);
				continue;
			}
		}

		++stream_itr;
	}
	now=Timer::GetCurrentTime();
	std::map<std::pair<uint32, uint16>,EQOldStream *>::iterator oldstream_itr;
	for(oldstream_itr = OldStreams.begin();oldstream_itr != OldStreams.end();) {
		EQOldStream *s = oldstream_itr->second;
		s->CheckTimeout(now, stream_timeout);

		EQStreamState state = s->GetState();
		//not part of the else so we check it right away on state change
		if (state==CLOSED) {
			if (s->IsInUse()) {
				//give it a little time for everybody to finish with it
			} else {
				//everybody is done, we can delete it now
				//cout << "Removing connection" << endl;
				auto temp = oldstream_itr;
				++oldstream_itr;
				//let whoever has the stream outside delete it
				delete temp->second;
				OldStreams.erase(temp);
				continue;
			}
		}

		++oldstream_itr;
	}
	MStreams.unlock();
}

void EQStreamFactory::WriterLoop()
{
	std::vector<EQStream *> wants_write;
	std::vector<EQStream *>::iterator cur,end;
	std::vector<EQOldStream *> old_wants_write;
	std::vector<EQOldStream *>::iterator oldcur,oldend;
	bool decay=false;
	uint32 stream_count;

	Timer DecayTimer(20);

	WriterRunning=true;
	DecayTimer.Enable();
	while(sock != -1) {
		//if (!havework) {
			//WriterWork.Wait();
		//}
		MWriterRunning.lock();
		if (!WriterRunning)
			break;
		MWriterRunning.unlock();

		wants_write.clear();
		old_wants_write.clear();

		decay=DecayTimer.Check();

		//copy streams into a seperate list so we dont have to keep
		//MStreams locked while we are writting
		MStreams.lock();
		for(auto stream_itr = Streams.begin();stream_itr != Streams.end();stream_itr++) {
			// If it's time to decay the bytes sent, then let's do it before we try to write
			if (decay)
				stream_itr->second->Decay();

			//bullshit checking, to see if this is really happening, GDB seems to think so...
			if(stream_itr->second == nullptr) {
				fprintf(stderr, "ERROR: nullptr Stream encountered in EQStreamFactory::WriterLoop for: %i:%i", stream_itr->first.first, stream_itr->first.second);
				continue;
			}

			if (stream_itr->second->HasOutgoingData()) {
				stream_itr->second->PutInUse();
				wants_write.push_back(stream_itr->second);
			}
		}
		for(auto oldstream_itr = OldStreams.begin();oldstream_itr != OldStreams.end();oldstream_itr++) {

			//bullshit checking, to see if this is really happening, GDB seems to think so...
			if(oldstream_itr->second == nullptr) {
				fprintf(stderr, "ERROR: nullptr Stream encountered in EQStreamFactory::WriterLoop for: %i:%i", oldstream_itr->first.first, oldstream_itr->first.second);
				continue;
			}		
			
			oldstream_itr->second->CheckTimers();

			//Commented this so all streams, regardless of them having data, send data out. This is so keepalive packets don't screw up the data rate calculations. Slightly more CPU used.
			//if (oldstream_itr->second->HasOutgoingData()) {
					oldstream_itr->second->PutInUse();
					old_wants_write.push_back(oldstream_itr->second);
			//}						
		}

		MStreams.unlock();
		//do the actual writes
		cur = wants_write.begin();
		end = wants_write.end();

		for(; cur != end; ++cur) {
			(*cur)->Write(sock);
			(*cur)->ReleaseFromUse();
		}

		//do the actual writes
		oldcur = old_wants_write.begin();
		oldend = old_wants_write.end();
		for(; oldcur != oldend; ++oldcur) {
			(*oldcur)->SendPacketQueue();
			(*oldcur)->ReleaseFromUse();
		}

		Sleep(10);

		MStreams.lock();
		stream_count=Streams.size() + OldStreams.size();
		MStreams.unlock();
		if (!stream_count) {
			WriterWork.Wait();
		}
	}
}
