
//
#include <Core/Utils/Log.h>
#include "Database.h"
#include "Configure.h"


//
// class SqlConnectionHolder
//

//
// class Database
//
Database::Database()
{
    m_pConnection = NULL;
    m_nMaxConnectionCount = 30;
    m_nCacheConnectionCount = 10;
}

Database::~Database()
{
    assert(NULL == m_pConnection);
    assert(m_nConnectionPool.empty());
}

bool Database::Initialize()
{
    assert(NULL == m_pConnection);

    int nErrorCode = 0;
    std::string strError;
    DatabaseConfigure &nDbConfig = Configure::Instance().m_nDatabase;

    __ULOG_INFO(__ULOG_FMT("Database", "Connecting to server(%s), user(%s)..."), nDbConfig.m_strUrl.c_str(), nDbConfig.m_strUser.c_str());

    m_pConnection = Storage::SqlDriverManager::Instance().Connect(nDbConfig.m_strUrl, nDbConfig.m_strUser, nDbConfig.m_strPassword, nErrorCode, strError);
    if( NULL == m_pConnection )
    {
        __ULOG_ERROR(__ULOG_FMT_ERR("Database", "Connect failed"), (uint32)nErrorCode, strError.c_str());
        return false;
    }
    m_pConnection->SetLobColumns(nDbConfig.m_nLobColumns);
    m_pConnection->SetShowSqlLog(nDbConfig.m_bShowSqlLog);


    __ULOG_INFO(__ULOG_FMT("Database", "Connected"));

    if( m_nMaxConnectionCount <= 0 ) m_nMaxConnectionCount = 20;
    else if( m_nMaxConnectionCount > 1000 ) m_nMaxConnectionCount = 1000;
    if( m_nCacheConnectionCount <= 0 ) m_nCacheConnectionCount = 5;
    if( m_nCacheConnectionCount > m_nMaxConnectionCount ) m_nCacheConnectionCount = m_nMaxConnectionCount;


    __ULOG_INFO(__ULOG_FMT("Database", "Maximum connection count: "_SIZE_TFMT_), m_nMaxConnectionCount);
    __ULOG_INFO(__ULOG_FMT("Database", "Cache connection count: "_SIZE_TFMT_), m_nCacheConnectionCount);
    __ULOG_INFO(__ULOG_FMT("Database", "Multi table enabled: %s"), nDbConfig.m_bMultiTableEnabled ? "yes" : "no");
    __ULOG_INFO(__ULOG_FMT("Database", "Multi table time: %s"), nDbConfig.m_nMultiTableTime > 0 ? Utils::String::FormatMicroTime("Y-m-d H:i:s", nDbConfig.m_nMultiTableTime).c_str() : "disabled");
    return true;
}

bool Database::Exit()
{
    if( NULL != m_pConnection )
    {
        m_pConnection->Close();
        m_pConnection = Storage::SqlDriverManager::Instance().Release(m_pConnection);
    }

    for ( size_t i = 0;i < m_nResultConnection.size(); i++)
    {
        if( NULL != m_nResultConnection[i] )
        {
            m_nResultConnection[i]->Close();
            m_nResultConnection[i] = Storage::SqlDriverManager::Instance().Release(m_nResultConnection[i]);
        }
    }

    for( Storage::SqlConnectionPtrList::iterator itr = m_nConnectionPool.begin(); itr != m_nConnectionPool.end(); itr ++ )
    {
        Storage::SqlConnection *pConnection = (*itr);
        if( pConnection->GetRefCount() > 0 )
        {
            __ULOG_WARNING(__ULOG_FMT("Database", "Cache connection busy(%d)"), pConnection->GetRefCount());
        }
        pConnection = Storage::SqlDriverManager::Instance().Release(pConnection);
    }
    m_nConnectionPool.clear();

    return true;
}



Storage::SqlConnection *Database::AccuireConnection( int nPrivilegeLevel )
{
    Utils::AutoLock __access__(m_nPoolMutex);

    for( Storage::SqlConnectionPtrList::iterator itr = m_nConnectionPool.begin(); itr != m_nConnectionPool.end(); itr ++ )
    {
        Storage::SqlConnection *pConnection = (*itr);
        if( pConnection->GetRefCount() == 0 &&
            pConnection->GetPrivilegeLevel() == nPrivilegeLevel )
        {
            pConnection->AddRef();
            return pConnection;
        }
    }

    if( m_nConnectionPool.size() >= m_nMaxConnectionCount )
    {
        __ULOG_WARNING(__ULOG_FMT("Database","Connection is too many ,MaxConnectionCount = " _U64FMT_ ), m_nMaxConnectionCount);
        return m_pConnection;
    }

    DatabaseConfigure &nDbConfig = Configure::Instance().m_nDatabase;
    std::string strUserName = nDbConfig.m_strUser;
    std::string strPassword = nDbConfig.m_strPassword;
    std::string strUrl = nDbConfig.m_strUrl;

    if ( strUserName.empty() )
    {
        __ULOG_TRACE(__ULOG_FMT("Database", "Database usename is empty, allocate new connection failed"));
        return NULL;
    }

    int nErrorCode = 0;
    std::string strError;
    Storage::SqlConnection *pConnection = Storage::SqlDriverManager::Instance().Connect(strUrl, strUserName, strPassword, nErrorCode, strError);
    if( NULL == pConnection )
    {
        __ULOG_ERROR(__ULOG_FMT_ERR("Database", "Allocate new connection failed"), (uint32)nErrorCode, strError.c_str());
        return NULL;
    }

    // set default attributes
    pConnection->SetLobColumns(nDbConfig.m_nLobColumns);
    pConnection->SetShowSqlLog(nDbConfig.m_bShowSqlLog);
    pConnection->AddRef();
    pConnection->SetPrivilegeLevel(nPrivilegeLevel);

    m_nConnectionPool.push_back(pConnection);

    __ULOG_TRACE(__ULOG_FMT("Database", "Allocated new connection, now "_SIZE_TFMT_" connection(s)"), m_nConnectionPool.size());

    return pConnection;
}

Storage::SqlConnection *Database::RecycleConnection( Storage::SqlConnection *pConnection )
{
    Utils::AutoLock __access__(m_nPoolMutex);

    bool bFound = false;
    for( Storage::SqlConnectionPtrList::iterator itr = m_nConnectionPool.begin(); itr != m_nConnectionPool.end(); itr ++ )
    {
        Storage::SqlConnection *pItem = (*itr);
        if( pItem == pConnection )
        {
            bFound = true;
            if( pConnection->GetRefCount() > 0 ) pConnection->ReleaseRef();
            if( pConnection->GetRefCount() == 0 && m_nConnectionPool.size() > m_nCacheConnectionCount )
            {
                pConnection->Close();
                pConnection = Storage::SqlDriverManager::Instance().Release(pConnection);
                m_nConnectionPool.erase(itr);
                __ULOG_TRACE(__ULOG_FMT("Database", "Released a connection, now "_SIZE_TFMT_" connection(s)"), m_nConnectionPool.size());
            }
            break;
        }
    }

    if( !bFound )
    {
        //__ULOG_ERROR(__ULOG_FMT("Database", "Recyle a wild connection item"));
        //assert(false);
    }
    return NULL;
}


DatabaseConnectionPool::DatabaseConnectionPool( int nResidentConnectionSize /*= 5*/, int nMaxConnectionSize /*= 10*/ ):m_nResidentConnectionSize(nResidentConnectionSize),
                                                                                                                       m_nMaxConnectionSize(nMaxConnectionSize)
{
}

DatabaseConnectionPool::~DatabaseConnectionPool()
{
    Utils::AutoLock __access__(m_nPoolListMutex);
    for(Storage::SqlConnectionPtrList::iterator itr= m_nConnectionList.begin(); itr != m_nConnectionList.end(); )
    {
        (*itr)->Close();
        Storage::SqlDriverManager::Instance().Release(*itr);
        itr = m_nConnectionList.erase(itr);
    }

}

bool DatabaseConnectionPool::Init( std::string strUrl, std::string strUserName, std::string strPassword, int nPrivilegeLevel )
{
    m_nDatabaseUrl = strUrl;
    m_nUserName = strUserName;
    m_nPassword = strPassword;
    m_nPrivilegeLevel = nPrivilegeLevel;
    int nErrorCode = 0;
    std::string strError;
    Storage::SqlConnection *pConnection = Storage::SqlDriverManager::Instance().Connect(m_nDatabaseUrl, m_nUserName, m_nPassword, nErrorCode, strError);
    if( NULL == pConnection )
    {
        __ULOG_ERROR(__ULOG_FMT_ERR("DatabaseConnectionPool", "Allocate new connection failed"), (uint32)nErrorCode, strError.c_str());
        return false;
    }
    DatabaseConfigure &nDbConfig = Configure::Instance().m_nDatabase;
    pConnection->SetLobColumns(nDbConfig.m_nLobColumns);
    pConnection->SetShowSqlLog(nDbConfig.m_bShowSqlLog);
    m_nConnectionList.push_back(pConnection);
    return true;
}

Storage::SqlConnection* DatabaseConnectionPool::GetOnceConnection()
{

    do
    {
        Utils::AutoLock __access__(m_nPoolListMutex);
        for( Storage::SqlConnectionPtrList::iterator itr = m_nConnectionList.begin(); itr != m_nConnectionList.end(); itr ++ )
        {
            Storage::SqlConnection *pConnection = (*itr);
            if( pConnection->GetRefCount() == 0 )
            {
                pConnection->AddRef();
                return pConnection;
            }
        }
        if( m_nConnectionList.size() >= m_nMaxConnectionSize )
        {
            __ULOG_WARNING(__ULOG_FMT("Database","Connection is too many ,MaxConnectionCount = " _U64FMT_ ),m_nMaxConnectionSize);
            return NULL;
        }
    } while (false);


    int nErrorCode = 0;
    std::string strError;
    DatabaseConfigure &nDbConfig = Configure::Instance().m_nDatabase;
    Storage::SqlConnection *pConnection = Storage::SqlDriverManager::Instance().Connect(m_nDatabaseUrl, m_nUserName, m_nPassword, nErrorCode, strError);
    if( NULL == pConnection )
    {
        __ULOG_ERROR(__ULOG_FMT_ERR("DatabaseConnectionPool", "Allocate new connection failed"), (uint32)nErrorCode, strError.c_str());
        return NULL;
    }

    // set default attributes
    pConnection->SetLobColumns(nDbConfig.m_nLobColumns);
    pConnection->SetShowSqlLog(nDbConfig.m_bShowSqlLog);
    pConnection->AddRef();
    pConnection->SetPrivilegeLevel(m_nPrivilegeLevel);


    Utils::AutoLock __access__(m_nPoolListMutex);
    m_nConnectionList.push_back(pConnection);
    return pConnection;

}

void DatabaseConnectionPool::RecyleConnection( Storage::SqlConnection *pConnection )
{
    Utils::AutoLock __access__(m_nPoolListMutex);
    if( pConnection == NULL )
    {
        return;
    }
    bool bFound = false;
    for( Storage::SqlConnectionPtrList::iterator itr = m_nConnectionList.begin(); itr != m_nConnectionList.end(); itr ++ )
    {
        Storage::SqlConnection *pItem = (*itr);
        if( pItem == pConnection )
        {
            bFound = true;
            if( pConnection->GetRefCount() > 0 ) pConnection->ReleaseRef();
            if( pConnection->GetRefCount() == 0 && m_nConnectionList.size() > m_nResidentConnectionSize )
            {
                pConnection->Close();
                pConnection = Storage::SqlDriverManager::Instance().Release(pConnection);
                m_nConnectionList.erase(itr);
                __ULOG_TRACE(__ULOG_FMT("DatabaseConnectionPool", "Released a connection, now "_SIZE_TFMT_" connection(s)"), m_nConnectionList.size());
            }
            break;
        }
    }

    if( !bFound )
    {
        __ULOG_ERROR(__ULOG_FMT("DatabaseConnectionPool", "Recyle a wild connection item"));
        assert(false);
    }
}
OnceConnection::OnceConnection(DatabaseConnectionPool *pConnectionPool ):m_lpConnectionPool(pConnectionPool)
{
    if( m_lpConnectionPool != NULL )
    {
        m_lpConnection = pConnectionPool->GetOnceConnection();
    }

}

OnceConnection::~OnceConnection()
{
    m_lpConnectionPool->RecyleConnection(m_lpConnection);
}

bool DBUtils::CheckTable(const std::string &tableName, DatabaseConnectionPool *connectionPool)
{
    OnceConnection onceConnection(connectionPool);
    Storage::SqlConnection &sqlConnection = onceConnection.GetConnectionRef();
    Json::Value result;
    if(!sqlConnection.ShowTableStaus(tableName, result))
    {
        __ULOG_ERROR(__ULOG_FMT_ERR("DBUtils","check table failed "),sqlConnection.GetErrorCode(),sqlConnection.GetErrorMsg());
        return false;
    }

    return !result.empty();
}

bool DBUtils::CreateTable(const std::string &tableName, DatabaseConnectionPool *connectionPool,
                          const std::string &baseName)
{
    OnceConnection onceConnection(connectionPool);
    Storage::SqlConnection &sqlConnection = onceConnection.GetConnectionRef();
    bool bCreated =false;
    bool bChanged =false;
    if( !sqlConnection.SyncTableStructure(tableName, baseName, "id", bCreated, bChanged) )
    {
        __ULOG_ERROR(__ULOG_FMT_ERR("DBUtils", "Sync multi table(%s) from(%s) failed"),
                     tableName.c_str(), baseName.c_str(), (uint32)sqlConnection.GetErrorCode(), sqlConnection.GetErrorMsg());
        return false;
    }
    return true;
}
