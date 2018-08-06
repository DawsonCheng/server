
#ifndef LIVESERVER_CONFIGURE_H
#define LIVESERVER_CONFIGURE_H

#include <Core/Utils/Singleton.h>
#include <Core/Common/ConfigureBase.h>
#include <Core/Storage/Sql.h>
class SslConfigureItem : public Common::ModuleConfigure
{
public:
    int m_nSslType;
    bool m_bVerifyPeer;

    std::string m_strRootCACertFile;

    std::string m_strUserCertFile;
    std::string m_strUserPriKeyFile;
    std::string m_strUserPriKeyPassword;

    std::string m_strDomain;
    std::string m_strSslControlType;
    std::string m_strConfigureName;

    SslConfigureItem();
    ~SslConfigureItem();

    virtual bool Load( const Utils::XmlNodePtr pXmlNode, Common::ConfigureBase &nConfig );
};

typedef std::map<std::string , SslConfigureItem> SslConfigureItemMap;

class SslConfigure : public Common::ModuleConfigure
{
public:
    SslConfigureItemMap m_nSslItemConfigure;

    SslConfigure();
    ~SslConfigure();

    virtual bool Load( const Utils::XmlNodePtr pXmlNode, Common::ConfigureBase &nConfig );
};

typedef std::map<int32 , std::string> ResoveCodeMap;

class ResoveCodeConfigure : public Common::ModuleConfigure
{
public:
	ResoveCodeMap m_nResoveCodeMap;

	ResoveCodeConfigure();
	~ResoveCodeConfigure();

	virtual bool Load( const Utils::XmlNodePtr pXmlNode, Common::ConfigureBase &nConfig );
};


class AgentServerConfigure : public Common::ModuleConfigure
{
public:
    std::string m_strListenAddress;
    int         m_nMaxClient;
	
	std::string m_nPacketName;
	std::string m_nAgentListenPort;

    AgentServerConfigure();
    virtual ~AgentServerConfigure();

    virtual bool Load( const Utils::XmlNodePtr pXmlNode );
};

class TestColumnConfigure
{
public:
    std::string m_strName;
    std::string m_strThresholdOperator;
    bool m_bPseudo;
    bool m_bIndex;
    bool m_bQuerySwitch;
    bool m_bPrimary;
    int  m_nIndexType;
    bool m_bLob;

    std::string m_strTypeSwitch;
    std::string m_strLevelSwitch;
    std::string m_strLocationSwitch;
    bool m_bIpSwitch;

    TestColumnConfigure();
    virtual ~TestColumnConfigure();

    virtual bool Load( const Utils::XmlNodePtr pXmlNode );
};


class WebServerConfigure : public Common::ModuleConfigure
{
public:
    bool   m_bKeepAlive;
    bool   m_bSslEnable;
    size_t m_nMaxClients;
    size_t m_nMaxHeaderSize;
    int    m_nReceiveTimeout;
    int    m_nCacheExpireTime;
    int    m_nConnectionIdleTime;
    int    m_nConnectionThreadCount;
    size_t m_nConnectionServiceTimes;
    Utils::InetAddressList m_nListenAddresses;

    std::string m_strListenAddress;
    int m_nListenPort;

    std::string m_strDirectory;
    std::string m_strDefFileName;
    std::string m_strDefContentType;
    Utils::StringMap  m_nDefFileNames;
    Utils::StringMap  m_nHttpContentTypes;
    Utils::StringList m_nNoCompressUserAgents;
    std::set<uint16>  m_nSslMapPorts;
    Utils::StringList m_nAllowedAutoLoginAddress;

    bool   m_bCompressContent;
    size_t m_nCompressMinSize;
    bool   m_bAllowSameIpLogin;
    bool   m_bValidateLoginIp;
    int64  m_nSessionTimeout;
    int64  m_nQueryMaxLimit;
    int64  m_nExportMaxLimit;
    int64  m_nExportCsvLimit;
	

    std::string m_strInterfaceDescribeFile;
	std::string m_strAuthorizationUrl;
    bool        m_bAuthorizationAble;
	std::string	m_strJsonWebTokenIss;
	std::string m_strSystemIdentification;
	std::string	m_strJsonWebTokenIssueds;
	int64		m_nJsonWebTokenTimeout;


    WebServerConfigure();
    virtual ~WebServerConfigure();

    virtual bool Load( const Utils::XmlNodePtr pXmlNode, Common::ConfigureBase &nConfig );

    bool IsCompressNotSupported( const std::string &strUserAgent ) const;
};

class DatabaseConfigure : public Common::ModuleConfigure
{
public:
    std::string m_strUrl;
    std::string m_strUser;
    std::string m_strPassword;
    std::string m_strResultStatTopSql;
    std::string m_strOptimizeTableSql;
    std::string m_strLimitDeleteSql;
    std::string m_strHistoryPath;
    bool m_bShowSqlLog;
    bool m_bOptimizeTable;
    int  m_nHistoryCacheSeconds;
    bool m_bMultiTableEnabled;
    int64 m_nMultiTableTime;
    bool m_bPartitionEnabled;
    bool m_bPartitionForHourlyAndDailyTable;
    bool m_bNeedTablePartition;
    size_t m_nResultConnectionCount;
    bool m_bTestResultImport;

    Utils::StringMap     m_nStatTypeNames;
	std::string		m_strSystemStatusResult;

    Storage::LobColumnMap m_nLobColumns;

    DatabaseConfigure();
    virtual ~DatabaseConfigure();

    virtual bool Load( const Utils::XmlNodePtr pXmlNode );

};
class Configure : public Common::ConfigureBase, public Utils::Singleton<Configure>
{
    friend class Utils::Singleton<Configure>;

private:
    Configure();
    virtual ~Configure();

public:
    WebServerConfigure     m_nWebServer;
    SslConfigure           m_nSslConfigure;
	ResoveCodeConfigure    m_nResoveCodeMeaning;
    DatabaseConfigure      m_nDatabase;
    AgentServerConfigure   m_nAgentServer;

    virtual bool LoadDoc();
    virtual void TraceSummary();
};
#endif //LIVESERVER_CONFIGURE_H