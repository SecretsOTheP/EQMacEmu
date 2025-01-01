#ifndef DBCORE_H
#define DBCORE_H

#ifdef _WINDOWS
#include <winsock2.h>
#include <windows.h>
#endif

#include "mutex.h"
#include "mysql_request_result.h"
#include "types.h"

#include <mysql.h>
#include <string.h>
#include <mutex>

#define CR_SERVER_GONE_ERROR    2006
#define CR_SERVER_LOST          2013

class DBcore {
public:
	enum eStatus { Closed, Connected, Error };

	DBcore();
	~DBcore();
	eStatus	GetStatus() { return pStatus; }
	MySQLRequestResult	QueryDatabase(const char* query, uint32 querylen, bool retryOnFailureOnce = true);
	MySQLRequestResult	QueryDatabase(const std::string& query, bool retryOnFailureOnce = true);
	void TransactionBegin();
	void TransactionCommit();
	void TransactionRollback();
	std::string Escape(const std::string &s);
	uint32	DoEscapeString(char* tobuf, const char* frombuf, uint32 fromlen);
	void	ping();

	bool DoesTableExist(const std::string &table_name);

	MYSQL*	getMySQL(){ return &mysql; }

protected:
	bool	Open(const char* iHost, const char* iUser, const char* iPassword, const char* iDatabase, uint32 iPort, uint32* errnum = 0, char* errbuf = 0, bool iCompress = false, bool iSSL = false);
private:
	bool	Open(uint32* errnum = nullptr, char* errbuf = nullptr);

	MYSQL	mysql;
	Mutex	MDatabase;
	eStatus pStatus;

	std::mutex m_query_lock{};

	char*	pHost;
	char*	pUser;
	char*	pPassword;
	char*	pDatabase;
	bool	pCompress;
	uint32	pPort;
	bool	pSSL;

};


#endif

