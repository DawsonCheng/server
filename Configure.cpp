

#include <Core/Utils/Utils.h>
#include <Core/Utils/Log.h>
#include <Core/Utils/String.h>
#include <Core/Common/Module.h>
#include <Core/Common/Test.h>
#include "Configure.h"

SslConfigureItem::SslConfigureItem()
{
    m_bVerifyPeer = false;
    m_nSslType = Utils::Socket::SSL_SERVER_V23;
}

SslConfigureItem::~SslConfigureItem()
{
}

bool SslConfigureItem::Load( const Utils::XmlNodePtr pXmlNode, Common::ConfigureBase &nConfig )
{
    if( !Common::ModuleConfigure::Load(pXmlNode) )
    {
        return false;
    }

    Utils::XmlNodeParser nParser(pXmlNode);
    nParser.GetProperty("VerifyPeer" , m_bVerifyPeer);
    nParser.GetProperty("location" , m_strSslControlType);
    nParser.GetProperty("domain" , m_strDomain);
    nParser.GetProperty("CACert" , m_strRootCACertFile);
    nParser.GetProperty("UserCert" , m_strUserCertFile);
    nParser.GetProperty("UserPrivateKeyFile" , m_strUserPriKeyFile);
    nParser.GetProperty("UserPrivateKeyFilePassword" , m_strUserPriKeyPassword);
    nParser.GetProperty("ConfigureName" , m_strConfigureName);

    if( !m_strRootCACertFile.empty() && !Utils::File::IsAbsolutePath(m_strRootCACertFile) )
    {
        m_strRootCACertFile = Utils::String::Format("%s/%s/%s", nConfig.m_strPath.c_str(), Common::Module::DATA_NAME, m_strRootCACertFile.c_str());
    }

    if( !m_strUserCertFile.empty() && !Utils::File::IsAbsolutePath(m_strUserCertFile) )
    {
        m_strUserCertFile = Utils::String::Format("%s/%s/%s", nConfig.m_strPath.c_str(), Common::Module::DATA_NAME, m_strUserCertFile.c_str());
    }

    if( !m_strUserPriKeyFile.empty() && !Utils::File::IsAbsolutePath(m_strUserPriKeyFile) )
    {
        m_strUserPriKeyFile = Utils::String::Format("%s/%s/%s", nConfig.m_strPath.c_str(), Common::Module::DATA_NAME, m_strUserPriKeyFile.c_str());
    }
    return true;
}

SslConfigure::SslConfigure()
{
}

SslConfigure::~SslConfigure()
{
}

bool SslConfigure::Load( const Utils::XmlNodePtr pXmlNode, Common::ConfigureBase &nConfig )
{
    if( !Common::ModuleConfigure::Load(pXmlNode) )
    {
        return false;
    }

    Utils::XmlNodeParser nParser(pXmlNode);
    Utils::XmlNodeList nSslConfigure = nParser.GetChildNodeList("Item");
    for( Utils::XmlNodeList::iterator it = nSslConfigure.begin() ; it != nSslConfigure.end() ; ++it )
    {
        SslConfigureItem nItem;
        if( nItem.Load(*it , nConfig) )
        {
            m_nSslItemConfigure[nItem.m_strConfigureName] = nItem;
        }
    }
    return true;
}

ResoveCodeConfigure::ResoveCodeConfigure()
{
}

ResoveCodeConfigure::~ResoveCodeConfigure()
{
}

bool ResoveCodeConfigure::Load( const Utils::XmlNodePtr pXmlNode, Common::ConfigureBase &nConfig )
{
	if( !Common::ModuleConfigure::Load(pXmlNode) )
	{
		return false;
	}

	Utils::XmlNodeParser nParser(pXmlNode);
	Utils::XmlNodeList nSslConfigure = nParser.GetChildNodeList("Item");
	for( Utils::XmlNodeList::iterator it = nSslConfigure.begin() ; it != nSslConfigure.end() ; ++it )
	{

		Utils::XmlNodeParser nParser(*it);
		int32 nCode = 0;
		std::string strMeaning = "";
		nParser.GetProperty("Code" , nCode);
		nParser.GetProperty("Meaning" , strMeaning);
		m_nResoveCodeMap.insert(std::make_pair(nCode,strMeaning));
	}
	return true;
}


AgentServerConfigure::AgentServerConfigure()
{
    m_nMaxClient = 100;
	m_nAgentListenPort = "3015";
}

AgentServerConfigure::~AgentServerConfigure()
{

}

bool AgentServerConfigure::Load(const Utils::XmlNodePtr pXmlNode)
{
    if( !Common::ModuleConfigure::Load(pXmlNode) )
    {
        return false;
    }

    Utils::XmlNodeParser nParser(pXmlNode);

    nParser.GetProperty("ListenAddress", m_strListenAddress);
    nParser.GetProperty("MaxClients", m_nMaxClient);

	//Agent Install Server

	nParser.GetProperty("packetName",m_nPacketName);
	if( nParser.GetProperty("AgentListenPort",m_nAgentListenPort) )
	{
		m_nAgentListenPort = "3015";
	}
    return true;
}

//
// class WebServerConfigure
//

WebServerConfigure::WebServerConfigure()
{
    m_bKeepAlive        = false;
    m_bSslEnable        = false;
    m_nMaxClients       = 128;
    m_nMaxHeaderSize    = 81920; // 80KB
    m_nReceiveTimeout   = 300;   // 5 minutes
    m_nCacheExpireTime  = Utils::SECOND_UNITS_PER_DAY;
    m_nConnectionServiceTimes = 20;
    m_nConnectionIdleTime = 2 * Utils::SECOND_UNITS_PER_MINUTE; // seconds

    m_strDefFileName    = "index.html";
    m_strDefContentType = "text/html";

    m_bCompressContent  = true;
    m_bAllowSameIpLogin = true;
    m_bValidateLoginIp  = true;
    m_nSessionTimeout   = 60 * Utils::MICRO_UNITS_PER_SEC;
    m_nQueryMaxLimit    = 1000;
    m_nExportMaxLimit   = 65534;
    m_nExportCsvLimit   = 45000;
    m_bAuthorizationAble = false;
}

WebServerConfigure::~WebServerConfigure()
{
}

bool WebServerConfigure::Load( const Utils::XmlNodePtr pXmlNode, Common::ConfigureBase &nConfig )
{
    if( !Common::ModuleConfigure::Load(pXmlNode) )
    {
        return false;
    }

    Utils::XmlNodeParser nParser(pXmlNode);

    std::string strListenAddress;
    nParser.GetProperty("ListenAddress", strListenAddress);
    m_strListenAddress = strListenAddress;
    std::string strPort = m_strListenAddress.substr(m_strListenAddress.find(':') + 1);
    m_nListenPort = atoi(strPort.c_str());

    Utils::StringList nListenAddresses;
    Utils::String::Split(strListenAddress, nListenAddresses, ',', -1, true);

    m_nListenAddresses.clear();
    for( Utils::StringList::iterator itrAddress = nListenAddresses.begin(); itrAddress != nListenAddresses.end(); itrAddress ++ )
    {
        Utils::InetAddress nNewAddress(*itrAddress);
        m_nListenAddresses.push_back(nNewAddress);
    }

    if( !nParser.GetProperty("Directory", m_strDirectory) )
    {
        m_strDirectory = Utils::String::Format("%s/%s", nConfig.m_strPath.c_str(), Common::Module::WEB_NAME);
    }

    if( !Utils::File::IsAbsolutePath(m_strDirectory) )
    {
        m_strDirectory = Utils::String::Format("%s/%s", nConfig.m_strPath.c_str(), m_strDirectory.c_str());
    }

    Utils::File::NormalizePath(m_strDirectory);

    nParser.GetProperty("KeepAlive", m_bKeepAlive);
    nParser.GetProperty("MaxClients", m_nMaxClients);
    nParser.GetProperty("MaxHeaderSize", m_nMaxHeaderSize);
    nParser.GetProperty("ReceiveTimeout", m_nReceiveTimeout);
    nParser.GetProperty("CacheExpireTime", m_nCacheExpireTime);
    nParser.GetProperty("ConnectionIdleTime", m_nConnectionIdleTime);
    nParser.GetProperty("ConnectionServiceTimes", m_nConnectionServiceTimes);
    nParser.GetProperty("DefFileName", m_strDefFileName);
    nParser.GetProperty("DefContentType", m_strDefContentType);
    nParser.GetProperty("RESTInterfaceFile", m_strInterfaceDescribeFile);
	nParser.GetProperty("ConnectionThreadCount",m_nConnectionThreadCount);
    nParser.GetProperty("AuthorzationUrl",m_strAuthorizationUrl);
	nParser.GetProperty("AuthorzationAble",m_bAuthorizationAble);
	nParser.GetProperty("JsonWebTokenIss",    m_strJsonWebTokenIss);
	nParser.GetProperty("JsonWebTokenIssueds",m_strJsonWebTokenIssueds);
	nParser.GetProperty("JsonWebTokenTimeout",m_nJsonWebTokenTimeout);
	nParser.GetProperty("SystemIdentification",m_strSystemIdentification);

    if( m_strDefFileName.find(':') != std::string::npos )
    {
        Utils::String::ParseAttributes(m_strDefFileName, m_nDefFileNames, ',', ':', false, false, true);
    }

    if( m_nDefFileNames.size() > 0 )
    {
        m_strDefFileName = m_nDefFileNames.begin()->second;
    }

    Utils::XmlNodeList nContentTypeNodes;
    nParser.GetChildNodeList("Mime", nContentTypeNodes);
    for( Utils::XmlNodeList::iterator itr = nContentTypeNodes.begin(); itr != nContentTypeNodes.end(); itr ++ )
    {
        Utils::XmlNodeParser nMime(*itr);
        std::string strExtName, strContentType;

        nMime.GetProperty("Ext", strExtName);
        nMime.GetProperty("Type", strContentType);
        m_nHttpContentTypes[strExtName] = strContentType;
    }


    nParser.GetProperty("CompressContent", m_bCompressContent);
    nParser.GetProperty("CompressMinSize", m_nCompressMinSize);
    nParser.GetProperty("AllowSameIpLogin", m_bAllowSameIpLogin);
    nParser.GetProperty("ValidateLoginIp", m_bValidateLoginIp);
    nParser.GetProperty("QueryMaxLimit", m_nQueryMaxLimit);
    nParser.GetProperty("ExportMaxLimit", m_nExportMaxLimit);
    nParser.GetProperty("ExportCsvLimit", m_nExportCsvLimit);

    if( nParser.GetProperty("SessionTimeout", m_nSessionTimeout) )
    {
        m_nSessionTimeout *= Utils::MICRO_UNITS_PER_SEC;
    }

    std::string strNoCompressUserAgents;
    nParser.GetProperty("NoCompressUserAgents", strNoCompressUserAgents);
    Utils::String::Split(strNoCompressUserAgents, m_nNoCompressUserAgents, ';', -1, true);

    for( Utils::StringList::iterator itrAgent = m_nNoCompressUserAgents.begin(); itrAgent != m_nNoCompressUserAgents.end(); itrAgent ++ )
    {
        Utils::String::MakeLower(*itrAgent);
    }

    std::string strSslMapPorts;
    Utils::StringList nSslMapPorts;

    nParser.GetProperty("SslEnable", m_bSslEnable);
    nParser.GetProperty("SslMapPorts", strSslMapPorts);

    Utils::String::Split(strSslMapPorts, nSslMapPorts, ',', -1, true);
    m_nSslMapPorts.clear();
    for( Utils::StringList::iterator itrPort = nSslMapPorts.begin(); itrPort != nSslMapPorts.end(); itrPort ++ )
    {
        m_nSslMapPorts.insert(Utils::String::ParseNumber(*itrPort, (uint16)0));
    }

	
	
    return true;
}

bool WebServerConfigure::IsCompressNotSupported( const std::string &strUserAgent ) const
{
    if( m_nNoCompressUserAgents.size() == 0 )
    {
        return false;
    }

    std::string strLowerAgent = Utils::String::ToLower(strUserAgent);
    for( Utils::StringList::const_iterator itrAgent = m_nNoCompressUserAgents.begin(); itrAgent != m_nNoCompressUserAgents.end(); itrAgent ++ )
    {
        if( strLowerAgent.find(*itrAgent) != std::string::npos )
        {
            return true;
        }
    }

    return false;
}


//
// class Configure
//

Configure::Configure()
{
}

Configure::~Configure()
{
}

bool Configure::LoadDoc()
{
    if( !ConfigureBase::LoadDoc() )
    {
        return false;
    }

    Utils::XmlNodeParser nRootNode(m_nXmlDoc.GetRootNode());

    if( !m_nWebServer.Load(nRootNode.GetFirstChildNode("WebServer"), *this) )
    {
        __ULOG_STDERR(__ULOG_FMT("Configure", "Load web server config failed"));
        return false;
    }

    m_nSslConfigure.m_bEnable = false;
    if( !m_nSslConfigure.Load(nRootNode.GetFirstChildNode("SslConfigure") , *this) )
    {
        m_nSslConfigure.m_bEnable = false;
    }

	if( !m_nResoveCodeMeaning.Load(nRootNode.GetFirstChildNode("ResoveCode") , *this) )
	{
		 __ULOG_STDERR(__ULOG_FMT("Configure", "Load ResoveCode config failed"));
	}

    if( !m_nDatabase.Load(nRootNode.GetFirstChildNode("Database")) )
    {
        __ULOG_STDERR(__ULOG_FMT("Configure" , "Load Service Server config failed"));
        return false;
    }
    if( !m_nAgentServer.Load(nRootNode.GetFirstChildNode("AgentServer")) )
    {
        __ULOG_STDERR(__ULOG_FMT("Configure" , "Load Agent Server config failed"));
        return false;
    }
	

    return true;
}


DatabaseConfigure::DatabaseConfigure()
{
    m_bShowSqlLog    = false;
    m_bOptimizeTable = false;
    m_nHistoryCacheSeconds = 86400;
    m_bMultiTableEnabled = false;
    m_nMultiTableTime = 0;

    m_bPartitionEnabled = false;
    m_bPartitionForHourlyAndDailyTable = false;
    m_bNeedTablePartition = false;
    m_nResultConnectionCount = 5;
    m_bTestResultImport = true;
}

DatabaseConfigure::~DatabaseConfigure()
{
}

bool DatabaseConfigure::Load( const Utils::XmlNodePtr pXmlNode )
{
    if( !Common::ModuleConfigure::Load(pXmlNode) )
    {
        return false;
    }

    Utils::XmlNodeParser nParser(pXmlNode);
    Utils::XmlNodeList nList;

    nParser.GetProperty("Url", m_strUrl);
    nParser.GetProperty("User", m_strUser);
    nParser.GetProperty("Password", m_strPassword);
    nParser.GetProperty("ResultStatTopSql", m_strResultStatTopSql);
    nParser.GetProperty("OptimizeTableSql", m_strOptimizeTableSql);
    nParser.GetProperty("LimitDeleteSql", m_strLimitDeleteSql);
    nParser.GetProperty("ShowSqlLog", m_bShowSqlLog);
    nParser.GetProperty("OptimizeTable", m_bOptimizeTable);
    nParser.GetProperty("HistoryCacheSeconds", m_nHistoryCacheSeconds);
    nParser.GetProperty("PartitionTable", m_bPartitionEnabled);
    nParser.GetProperty("ResultConnectionCount", m_nResultConnectionCount);

    std::string strStatTypeNames = "min=MIN;max=MAX;avg=AVG;std=STD";
    nParser.GetProperty("StatTypeNames", strStatTypeNames);
    Utils::String::ParseAttributes(strStatTypeNames, m_nStatTypeNames, ';', '=', true, true, true);


    Utils::XmlNodePtr lpIndexTypesNode = nParser.GetFirstChildNode("IndexType");
    if( NULL != lpIndexTypesNode )
    {
        Utils::XmlNodeParser nIndexTypeNode(lpIndexTypesNode);
        Utils::XmlNodeList nIndexTypeItems = nIndexTypeNode.GetChildNodeList("Item");

        for( Utils::XmlNodeList::iterator itr = nIndexTypeItems.begin(); itr != nIndexTypeItems.end(); itr ++ )
        {
            TestColumnConfigure nNewIndex;

            nNewIndex.Load(*itr);
            nNewIndex.m_bIndex = true;
        }
    }



    Utils::XmlNodePtr lpSystemIndexTypesNode = nParser.GetFirstChildNode("CommonDbItems");
    if( NULL != lpSystemIndexTypesNode )
    {
        Utils::XmlNodeParser nIndexTypeNode(lpSystemIndexTypesNode);
        Utils::XmlNodeList nIndexTypeItems = nIndexTypeNode.GetChildNodeList("Item");

        for( Utils::XmlNodeList::iterator itr = nIndexTypeItems.begin(); itr != nIndexTypeItems.end(); itr ++ )
        {
            TestColumnConfigure nNewIndex;

            nNewIndex.Load(*itr);
            nNewIndex.m_bIndex = true;
        }
    }
	Utils::XmlNodePtr lpSystemStatusResult = nParser.GetFirstChildNode("SystemStatusResultConfig");
	if( NULL != lpSystemStatusResult )
	{
		Utils::XmlNodeParser nParser(lpSystemStatusResult);
		nParser.GetProperty("SystemStatusResultTable",m_strSystemStatusResult);
	}

    return true;
}

TestColumnConfigure::TestColumnConfigure()
{
    m_bPseudo    = false;
    m_bIndex     = false;
    m_bQuerySwitch    = false;
    m_bPrimary   = false;
    m_bLob       = false;
    m_nIndexType = Common::TEST_INDEX_RESERVED;
    m_strThresholdOperator = ">";

    m_strTypeSwitch = "";
    m_strLevelSwitch = "";
    m_strLocationSwitch = "";
    m_bIpSwitch = true;
}

TestColumnConfigure::~TestColumnConfigure()
{
}

bool TestColumnConfigure::Load( const Utils::XmlNodePtr pXmlNode )
{
    if( NULL == pXmlNode )
    {
        return false;
    }

    Utils::XmlNodeParser nParser(pXmlNode);

    nParser.GetProperty("Name", m_strName);
    nParser.GetProperty("Pseudo", m_bPseudo);
    nParser.GetProperty("Index", m_bIndex);
    nParser.GetProperty("QuerySwitch", m_bQuerySwitch);
    nParser.GetProperty("Primary", m_bPrimary);
    nParser.GetProperty("IndexType", m_nIndexType);
    nParser.GetProperty("Lob", m_bLob);
    nParser.GetProperty("ThresholdOperator", m_strThresholdOperator);

    nParser.GetProperty("TypeSwitch", m_strTypeSwitch);
    nParser.GetProperty("LevelSwitch",m_strLevelSwitch);
    nParser.GetProperty("LocationSwitch",m_strLocationSwitch);
    nParser.GetProperty("IpSwitch",m_bIpSwitch);

    return true;
}

void Configure::TraceSummary()
{
    //
    // common
    //

    ConfigureBase::TraceSummary();
    //
    // web server
    //
    __ULOG_INFO(__ULOG_FMT("Configure", "[Web Server: %s]"), m_nWebServer.m_strName.c_str());
    __ULOG_INFO(__ULOG_FMT("Configure", "Keep Alive: %s"), m_nWebServer.m_bKeepAlive ? "yes" : "no");
    __ULOG_INFO(__ULOG_FMT("Configure", "Max Clients: "_SIZE_TFMT_), m_nWebServer.m_nMaxClients);
    __ULOG_INFO(__ULOG_FMT("Configure", "Max Header Size: "_SIZE_TFMT_), m_nWebServer.m_nMaxHeaderSize);
    __ULOG_INFO(__ULOG_FMT("Configure", "Receive Timeout: %d sec"), m_nWebServer.m_nReceiveTimeout);
    __ULOG_INFO(__ULOG_FMT("Configure", "Cache Expire Time: %d sec"), m_nWebServer.m_nCacheExpireTime);
    __ULOG_INFO(__ULOG_FMT("Configure", "Connection Service Times: "_SIZE_TFMT_), m_nWebServer.m_nConnectionServiceTimes);
    __ULOG_INFO(__ULOG_FMT("Configure", "Connection Idle Time: %d sec"), m_nWebServer.m_nConnectionIdleTime);
    __ULOG_INFO(__ULOG_FMT("Configure", "Listen Address Count: "_SIZE_TFMT_), m_nWebServer.m_nListenAddresses.size());
    __ULOG_INFO(__ULOG_FMT("Configure", "Directory: %s"), m_nWebServer.m_strDirectory.c_str());
    __ULOG_INFO(__ULOG_FMT("Configure", "Default File: %s"), m_nWebServer.m_strDefFileName.c_str());
    __ULOG_INFO(__ULOG_FMT("Configure", "Default File Name Count: "_SIZE_TFMT_), m_nWebServer.m_nDefFileNames.size());
    __ULOG_INFO(__ULOG_FMT("Configure", "Default Content Type: %s"), m_nWebServer.m_strDefContentType.c_str());
    __ULOG_INFO(__ULOG_FMT("Configure", "Mime Type Count: "_SIZE_TFMT_), m_nWebServer.m_nHttpContentTypes.size());
    __ULOG_INFO(__ULOG_FMT("Configure", "No Compress User Agent Count: "_SIZE_TFMT_), m_nWebServer.m_nNoCompressUserAgents.size());
    __ULOG_INFO(__ULOG_FMT("Configure", "Compress Content: %s"), m_nWebServer.m_bCompressContent ? "yes" : "no");
    __ULOG_INFO(__ULOG_FMT("Configure", "Compress Minimum Size: "_SIZE_TFMT_" bytes"), m_nWebServer.m_nCompressMinSize);
    __ULOG_INFO(__ULOG_FMT("Configure", "Allow Same IP Login: %s"), m_nWebServer.m_bAllowSameIpLogin ? "yes" : "no");
    __ULOG_INFO(__ULOG_FMT("Configure", "Validate Login IP: %s"), m_nWebServer.m_bValidateLoginIp ? "yes" : "no");
    __ULOG_INFO(__ULOG_FMT("Configure", "Session Timeout: "_I64FMT_" sec(s)"), m_nWebServer.m_nSessionTimeout / Utils::MICRO_UNITS_PER_SEC);
    //__ULOG_INFO(__ULOG_FMT("Configure", "Max soap clients: "_U64FMT_), m_nWebServer.m_nMaxSoapClients);
    //__ULOG_INFO(__ULOG_FMT("Configure", "Soap Recv Timeout: "_U64FMT_" sec(s)"), m_nWebServer.m_nSoapRecvTimeout);
    //__ULOG_INFO(__ULOG_FMT("Configure", "Soap Send Timeout: "_U64FMT_" sec(s)"), m_nWebServer.m_nSoapSendTimeout);
    //__ULOG_INFO(__ULOG_FMT("Configure", "Soap Accept Timeout: "_U64FMT_" sec(s)"), m_nWebServer.m_nSoapAcceptTimeout);
    __ULOG_INFO(__ULOG_FMT("Configure", "Query Max Limit: "_I64FMT_), m_nWebServer.m_nQueryMaxLimit);
    __ULOG_INFO(__ULOG_FMT("Configure", "Export Max Limit: "_I64FMT_), m_nWebServer.m_nExportMaxLimit);
    __ULOG_INFO(__ULOG_FMT("Configure", "Ssl Enable: %s"), m_nWebServer.m_bSslEnable ? "yes" : "no");
    __ULOG_INFO(__ULOG_FMT("Configure", "Ssl Map Ports: "_SIZE_TFMT_" items"), m_nWebServer.m_nSslMapPorts.size());
    __ULOG_INFO(__ULOG_FMT("Configure", "Database show log %s: "), m_nDatabase.m_bShowSqlLog? "yes" : "no");
	__ULOG_INFO(__ULOG_FMT("Configure", "Load ResoveCode Meaning :"_SIZE_TFMT_), m_nResoveCodeMeaning.m_nResoveCodeMap.size());
    __ULOG_INFO(__ULOG_FMT("Configure", "--------------------- SEPARATOR -----------------------"));
}

