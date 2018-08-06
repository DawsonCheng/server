

#ifndef LIVESERVER_DATABASE_H
#define LIVESERVER_DATABASE_H
#ifndef __DATABASE_H_INCLUDE__
#define __DATABASE_H_INCLUDE__

#include <Core/Utils/MultiThread.h>
#include <Core/Storage/Sql.h>

class DatabaseConnectionPool;
class OnceConnection
{
private:
	Storage::SqlConnection *m_lpConnection;
	DatabaseConnectionPool *m_lpConnectionPool;

public:
	OnceConnection(DatabaseConnectionPool *pConnectionPool);
	~OnceConnection();

	Storage::SqlConnection *GetConnection(){ return m_lpConnection; }
	inline bool IsDisabled(){return m_lpConnection == NULL ? true : false; }
	inline Storage::SqlConnection &GetConnectionRef(){ assert(NULL != m_lpConnection); return *m_lpConnection; }
};

class DatabaseConnectionPool
{
public:
	DatabaseConnectionPool(int nResidentConnectionSize = 5, int nMaxConnectionSize = 10);
	~DatabaseConnectionPool();
	bool Init(std::string strUrl, std::string strUserName, std::string strPassword, int nPrivilegeLevel = 0 );
	Storage::SqlConnection* GetOnceConnection();
	void RecyleConnection(Storage::SqlConnection *pConnection);

private:
	size_t m_nResidentConnectionSize;
	size_t m_nMaxConnectionSize;
	std::string m_nDatabaseUrl;
	std::string m_nUserName;
	std::string m_nPassword;
	Storage::SqlConnectionPtrList m_nConnectionList;
	Utils::Mutex m_nPoolListMutex;
	int m_nPrivilegeLevel;

};
class Database : public Utils::Singleton<Database>
{
	friend class Utils::Singleton<Database>;

private:
	Utils::Mutex m_nPoolMutex;
	size_t m_nMaxConnectionCount;
	size_t m_nCacheConnectionCount;

	Storage::SqlConnection *m_pConnection;
	Storage::SqlConnectionPtrVector m_nResultConnection;
	Storage::SqlConnectionPtrList m_nConnectionPool;

	std::map<std::string, Json::Value> m_nTableColumns;

	Database();
	virtual ~Database();

public:
	bool Initialize();
	bool Exit();
    
	inline Storage::SqlConnection &GetConnection(){ assert(NULL != m_pConnection); return *m_pConnection; }
	inline bool IsConnected(){ return NULL != m_pConnection; }

	inline Storage::SqlConnection &GetResultConnection(int32 nIndex){ assert(NULL != m_nResultConnection[nIndex]); return *m_nResultConnection[nIndex]; }
	inline bool IsResultConnected(){ return NULL != m_nResultConnection[0]; }

	Storage::SqlConnection *AccuireConnection( int nPrivilegeLevel );
	Storage::SqlConnection *RecycleConnection( Storage::SqlConnection *pConnection );
};

class DBUtils
{
public:
	static bool CheckTable(const std::string &tableName, DatabaseConnectionPool *connectionPool);

	static bool CreateTable(const std::string &tableName, DatabaseConnectionPool *connectionPool,
					 const std::string &baseName);
};

#endif //__DATABASE_H_INCLUDE__

#endif //LIVESERVER_DATABASE_H
