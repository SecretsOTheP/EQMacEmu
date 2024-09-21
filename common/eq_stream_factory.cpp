#include "global_define.h"
#include "eqemu_logsys.h"
#include "eq_stream_factory.h"

#ifdef _WINDOWS
#include <winsock2.h>
#include <io.h>
#include <stdio.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <iostream>
#include <fcntl.h>
#include <atomic>
#include <unordered_set>
#include <unordered_map>

#include "op_codes.h"

extern std::unordered_set<uint32> ipWhitelist;
extern std::mutex ipMutex;
extern bool bSkipFactoryAuth;
static std::atomic_bool s_checkTimeoutRunning;

EQStreamFactory::EQStreamFactory(EQStreamType type, int port, uint32 timeout)
	: Timeoutable(5000), stream_timeout(timeout)
{
	StreamType = type;
	Port = port;
	sock = -1;
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

	if (ReaderThread.joinable()) {
		ReaderThread.join();
	}

	//if (WriterNewThread.joinable()) {
	//	WriterNewThread.join();
	//}

	if (WriterOldThread.joinable()) {
		WriterOldThread.join();
	}
}

void EQStreamFactory::Stop() {
	StopReader();
	StopWriterNew();
	StopWriterOld();
}

void EQStreamFactory::StopReader() {
	std::lock_guard<std::mutex> lock(MReaderRunning);
	ReaderRunning = false;
}

void EQStreamFactory::StopWriterNew() {
	std::unique_lock<std::mutex> lock(MWriterRunningNew);
	WriterRunningNew = false;
	lock.unlock();
	WriterWorkNew.notify_one();
}

void EQStreamFactory::StopWriterOld() {
	std::unique_lock<std::mutex> lock(MWriterRunningOld);
	WriterRunningOld = false;
	lock.unlock();
	WriterWorkOld.notify_one();
}

void EQStreamFactory::SignalWriterNew() {
	WriterWorkNew.notify_one();
}

void EQStreamFactory::SignalWriterOld() {
	WriterWorkOld.notify_one();
}

bool EQStreamFactory::Open()
{
	struct sockaddr_in address;
#ifndef WIN32
	pthread_t t1, t2;
#endif
	/* Setup internet address information.
	This is used with the bind() call */
	memset((char *)&address, 0, sizeof(address));
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

	ReaderThread = std::thread(&EQStreamFactory::ReaderLoop, this);
	//WriterNewThread = std::thread(&EQStreamFactory::WriterLoopNew, this);
	WriterOldThread = std::thread(&EQStreamFactory::WriterLoopOld, this);
	return true;
}

std::shared_ptr<EQStream> EQStreamFactory::Pop()
{
	std::shared_ptr<EQStream> s = nullptr;
	std::lock_guard<std::mutex> lock(MNewStreams);
	if (!NewStreams.empty()) {
		s = NewStreams.front();
		NewStreams.pop();
		s->PutInUse();
	}

	return s;
}


std::shared_ptr<EQOldStream> EQStreamFactory::PopOld()
{
	std::shared_ptr<EQOldStream> s = nullptr;
	std::lock_guard<std::mutex> lock(MNewOldStreams);
	if (!NewOldStreams.empty()) {
		s = NewOldStreams.front();
		NewOldStreams.pop();
		s->PutInUse();
	}

	return s;
}

void EQStreamFactory::Push(std::shared_ptr<EQStream> s)
{
	std::lock_guard<std::mutex> lock(MNewStreams);
	NewStreams.push(s);
}

void EQStreamFactory::PushOld(std::shared_ptr<EQOldStream> s)
{
	std::lock_guard<std::mutex> lock(MNewOldStreams);
	NewOldStreams.push(s);
}

void EQStreamFactory::ReaderLoop()
{
	fd_set readset;
	EQStreamIterator stream_iter;
	EQOldStreamIterator oldstream_iter;
	int num;
	int length;
	unsigned char buffer[2048];
	sockaddr_in from;
	int socklen = sizeof(sockaddr_in);
	timeval sleep_time;
	ReaderRunning = true;
	while (sock != -1) {
		std::unique_lock<std::mutex> reader_lock(MReaderRunning);
		if (!ReaderRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			break;
		}
		reader_lock.unlock();

		if (s_checkTimeoutRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		FD_ZERO(&readset);
		FD_SET(sock, &readset);

		sleep_time.tv_sec = 30;
		sleep_time.tv_usec = 0;
		if ((num = select(sock + 1, &readset, nullptr, nullptr, &sleep_time)) < 0) {
			// What do we wanna do?
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		else if (num == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		if (sock == -1)
			break;		//somebody closed us while we were sleeping.

		if (FD_ISSET(sock, &readset)) {
#ifdef _WINDOWS
			if ((length = recvfrom(sock, (char*)buffer, sizeof(buffer), 0, (struct sockaddr*)&from, (int *)&socklen)) < 2)
#else
			if ((length = recvfrom(sock, buffer, 2048, 0, (struct sockaddr *)&from, (socklen_t *)&socklen)) < 2)
#endif
			{
				// What do we wanna do?
			}
			else {
				auto streamKey = std::make_pair(from.sin_addr.s_addr, from.sin_port);
				bool bFound = false;

				if (!bSkipFactoryAuth)
				{
					ipMutex.lock();
					bFound = ipWhitelist.find(from.sin_addr.s_addr) == ipWhitelist.end() ? false : true;
					ipMutex.unlock();

					if (!bFound)
						continue;
				}
				std::unique_lock<std::mutex> streams_lock(MStreams, std::defer_lock);
				std::unique_lock<std::mutex> old_streams_lock(MOldStreams, std::defer_lock);
				std::lock(streams_lock, old_streams_lock); //lock both mutexes (in order to avoid deadlock)

				//stream_iter = Streams.find(streamKey);
				oldstream_iter = OldStreams.find(streamKey);
				//bool hasNewStream = stream_iter != Streams.end();
				bool hasOldStream = oldstream_iter != OldStreams.end();

				if (/*hasNewStream == false && */ hasOldStream == false) {
	/*				if (buffer[1] == OP_SessionRequest) {
						RecvBuffer data = RecvBuffer(true, length, buffer, streamKey, from);
						ProcessLoopNew(data, stream_iter);
					}
					else {*/
					if (buffer[1] != OP_SessionRequest)
					{
						RecvBuffer data = RecvBuffer(true, length, buffer, streamKey, from);
						ProcessLoopOld(data, oldstream_iter);
					}
					//}
				}
				else {
					//if (hasNewStream) {
						//RecvBuffer data = RecvBuffer(false, length, buffer, streamKey, from);
						//ProcessLoopNew(data, stream_iter);
					//}
				//	else if (hasOldStream) {
						RecvBuffer data = RecvBuffer(false, length, buffer, streamKey, from);
						ProcessLoopOld(data, oldstream_iter);
					//}
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

void EQStreamFactory::ProcessLoopNew(const RecvBuffer& recvBuffer, EQStreamIterator iterator)
{
	const auto& from = recvBuffer.From();
	const auto& length = recvBuffer.Length();
	const auto buffer = recvBuffer.Buffer();

	if (recvBuffer.IsNew()) {
		std::shared_ptr<EQStream> s = std::make_shared<EQStream>(from);
		s->SetStreamType(StreamType);
		Streams[recvBuffer.StreamKey()] = s;
		WriterWorkNew.notify_one();
		Push(s);
		s->AddBytesRecv(length);
		s->Process(buffer, length);
		s->SetLastPacketTime(Timer::GetCurrentTime());
	}
	else {
		std::shared_ptr<EQStream> curstream = iterator->second;

		//dont bother processing incoming packets for closed connections
		if (curstream != nullptr) {
			if (curstream->CheckClosed())
				curstream = nullptr;
			else
				curstream->PutInUse();
		}

		if (curstream) {
			curstream->AddBytesRecv(length);
			curstream->Process(buffer, length);
			curstream->SetLastPacketTime(Timer::GetCurrentTime());
			curstream->ReleaseFromUse();
		}
	}
}

void EQStreamFactory::ProcessLoopOld(const RecvBuffer& recvBuffer, EQOldStreamIterator iterator)
{
	const auto& from = recvBuffer.From();
	const auto& length = recvBuffer.Length();
	auto buffer = recvBuffer.Buffer();

	if (recvBuffer.IsNew()) {
		std::shared_ptr<EQOldStream> s = std::make_shared<EQOldStream>(from, sock);
		s->SetStreamType(StreamType);
		OldStreams[recvBuffer.StreamKey()] = s;
		WriterWorkOld.notify_one();
		PushOld(s);
		//s->AddBytesRecv(length);
		s->ParceEQPacket(length, buffer);
		s->SetLastPacketTime(Timer::GetCurrentTime());
	}
	else {
		std::shared_ptr<EQOldStream> curstream = iterator->second;

		//dont bother processing incoming packets for closed connections
		if (curstream != nullptr) {
			if (curstream->CheckClosed())
				curstream = nullptr;
			else
				curstream->PutInUse();
		}

		if (curstream) {
			//curstream->AddBytesRecv(length);
			curstream->ParceEQPacket(length, buffer);
			curstream->SetLastPacketTime(Timer::GetCurrentTime());
			curstream->ReleaseFromUse();
		}
	}
}

void EQStreamFactory::CheckTimeout()
{
	// give priority for CheckTimeout to do work
	s_checkTimeoutRunning = true;

	//lock streams the entire time were checking timeouts, it should be fast.
	std::unique_lock<std::mutex> streams_lock(MStreams, std::defer_lock);
	std::unique_lock<std::mutex> oldstreams_lock(MOldStreams, std::defer_lock);
	std::lock(streams_lock, oldstreams_lock); //lock both mutexes (in order to avoid deadlock)

	unsigned long now = Timer::GetCurrentTime();
	EQStreamIterator stream_itr;

	for (stream_itr = Streams.begin(); stream_itr != Streams.end();) {
		std::shared_ptr<EQStream> s = stream_itr->second;

		s->CheckTimeout(now, stream_timeout);

		EQStreamState state = s->GetState();

		//not part of the else so we check it right away on state change
		if (state == CLOSED) {
			if (s->IsInUse()) {
				//give it a little time for everybody to finish with it
			}
			else {
				//everybody is done, we can delete it now
				auto temp = stream_itr;
				++stream_itr;
				temp->second = nullptr;
				Streams.erase(temp);
				continue;
			}
		}

		++stream_itr;
	}

	EQOldStreamIterator oldstream_itr;

	for (oldstream_itr = OldStreams.begin(); oldstream_itr != OldStreams.end();) {
		std::shared_ptr<EQOldStream> s = oldstream_itr->second;

		s->CheckTimeout(now, stream_timeout);

		EQStreamState state = s->GetState();

		//not part of the else so we check it right away on state change
		if (state == CLOSED) {
			if (s->IsInUse()) {
				//give it a little time for everybody to finish with it
			}
			else {
				//everybody is done, we can delete it now
				auto temp = oldstream_itr;
				++oldstream_itr;
				temp->second = nullptr;
				OldStreams.erase(temp);
				continue;
			}
		}

		++oldstream_itr;
	}

	s_checkTimeoutRunning = false;
}

void EQStreamFactory::WriterLoopNew() {
	std::vector<std::shared_ptr<EQStream>> wants_write;
	std::vector<std::shared_ptr<EQStream>>::iterator cur, end;
	uint32 stream_count;
	bool decay = false;
	Timer DecayTimer(20);

	WriterRunningNew = true;
	DecayTimer.Enable();
	while (sock != -1) {
		std::unique_lock<std::mutex> writer_lock(MWriterRunningNew);
		if (!WriterRunningNew) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			break;
		}
		writer_lock.unlock();

		if (s_checkTimeoutRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		wants_write.clear();
		decay = DecayTimer.Check();

		//copy streams into a seperate list so we dont have to keep
		//MStreams locked while we are writting
		std::unique_lock<std::mutex> streams_lock(MStreams);
		for (auto stream_itr = Streams.begin(); stream_itr != Streams.end(); ++stream_itr) {
			// If it's time to decay the bytes sent, then let's do it before we try to write
			if (decay)
				stream_itr->second->Decay();

			//bullshit checking, to see if this is really happening, GDB seems to think so...
			if (stream_itr->second == nullptr) {
				fprintf(stderr,
					"ERROR: nullptr Stream encountered in EQStreamFactory::WriterLoop for: %i:%i",
					stream_itr->first.first, stream_itr->first.second);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			if (stream_itr->second->HasOutgoingData()) {
				stream_itr->second->PutInUse();
				wants_write.push_back(stream_itr->second);
			}
		}

		stream_count = Streams.size();
		streams_lock.unlock();

		// do the actual writes
		cur = wants_write.begin();
		end = wants_write.end();

		for (; cur != end; ++cur) {
			(*cur)->Write(sock);
			(*cur)->ReleaseFromUse();
		}

		if (!stream_count) {
			std::unique_lock<std::mutex> writer_work_lock(MWriterRunningNew);
			WriterWorkNew.wait(writer_work_lock);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void EQStreamFactory::WriterLoopOld() {
	std::vector<std::shared_ptr<EQOldStream>> old_wants_write;
	std::vector<std::shared_ptr<EQOldStream>>::iterator oldcur, oldend;
	uint32 stream_count;

	WriterRunningOld = true;
	while (sock != -1) {
		std::unique_lock<std::mutex> writer_lock(MWriterRunningOld);
		if (!WriterRunningOld) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			break;
		}
		writer_lock.unlock();

		if (s_checkTimeoutRunning) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		old_wants_write.clear();

		std::unique_lock<std::mutex> oldstreams_lock(MOldStreams);
		for (auto stream_itr = OldStreams.begin(); stream_itr != OldStreams.end(); ++stream_itr) {

			//bullshit checking, to see if this is really happening, GDB seems to think so...
			if (stream_itr->second == nullptr) {
				fprintf(stderr,
					"ERROR: nullptr Stream encountered in EQStreamFactory::WriterLoop for: %i:%i",
					stream_itr->first.first, stream_itr->first.second);
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
			stream_itr->second->CheckTimers();
			//if (stream_itr->second->HasOutgoingData()) {
			stream_itr->second->PutInUse();
			old_wants_write.push_back(stream_itr->second);
			//}
		}

		stream_count = OldStreams.size();
		oldstreams_lock.unlock();

		// do the actual writes
		oldcur = old_wants_write.begin();
		oldend = old_wants_write.end();

		for (; oldcur != oldend; ++oldcur) {
			(*oldcur)->SendPacketQueue();
			(*oldcur)->ReleaseFromUse();
		}

		if (!stream_count) {
			std::unique_lock<std::mutex> writer_work_lock(MWriterRunningOld);
			WriterWorkOld.wait(writer_work_lock);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}