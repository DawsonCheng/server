
#ifndef LIVESERVER_WEBSERVER_H
#define LIVESERVER_WEBSERVER_H
#include <Core/Utils/HttpServer.h>
#include <Core/Utils/XmlParser.h>
#include <ThirdParty/LibJson/value.h>
#include <Core/Utils/Singleton.h>
#include <Core/Storage/Sql.h>
#include "Database.h"
#include <boost/asio/detail/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <Core/JsonWebToken/JsonWebToken.h>

class WebContext
{
DISALLOW_COPY_AND_ASSIGN(WebContext);

public:
    int64 m_nSessionId;
    int64 m_nUserId;
    int64 m_nGroupId;
    std::string m_strUsername;
    std::string m_strNickname;
    std::string m_strClientIp;
    Json::Value m_nJsonRequest;
    int m_nForwardAuthorized;

    bool *m_lpHttpEnabled;
    Utils::Socket *m_lpChannel;
    Utils::Parameter m_nResponseHeaders;
    Utils::SetCookie m_nSetCookies;

    bool        m_bCompressResponse;
    Json::Value m_nJsonResponse;
    std::string m_strResponse;

    WebContext();
    ~WebContext();
};
class WebServer;

class RESTRequestHelper
{
public:
    class RESTInterface
    {
    public:
        std::string m_nResource;
        std::string m_nTable;
        Utils::StringSet m_nActions;
        Json::Value m_nAllowConditionWords;
        Utils::StringSet m_nAllowFilterConditions;
        Utils::StringSet m_nResourceWords;
    };


    explicit RESTRequestHelper(const boost::shared_ptr<DatabaseConnectionPool> &DBPool);
    bool IsRESTRequest(std::string &resource);
    bool LoadInterfaceFile(std::string &strfile);
    uint32 RESTHttpRequest(Utils::HttpContext& nHttpContext , WebContext& nWebContext);
private:

    uint32 RESTGETRequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext, RESTInterface &nInterface);
    uint32 RESTPOSTRequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext, RESTInterface &nInterface);
    uint32 RESTPUTRequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext, RESTInterface &nInterface);
    uint32 RESTPATCHRequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext, RESTInterface &nInterface);
    uint32 RESTDELETERequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext, RESTInterface &nInterface);

    int InitFilterParameter(std::string filterWord, Utils::Parameter &httpParameter,
                                Utils::StringSet &allowWords, int defaultValue);
    std::string InitFilterParameter(std::string filterWord, Utils::Parameter &httpParameter,
                            Utils::StringSet &allowWords, std::string defaultValue);
    void InitResourceWords(const Json::Value &parameter, Utils::StringSet &allowWords, Utils::StringMap &wordsMap);

    std::string  InitConditionWord(Utils::Parameter &httpParameter, Json::Value &allowWords);


    typedef std::map<std::string, RESTInterface> RESTInterFaceMap;
    RESTInterFaceMap m_nInterFaceMap;
    Json::Value m_nFilterDefault;
    boost::shared_ptr<DatabaseConnectionPool> m_nDBPool;

};
typedef uint32 (WebServer::*WebUrlMethodProc)( Utils::HttpContext &nHttpContext, WebContext &nWebContext );
typedef std::map<std::string, WebUrlMethodProc> WebUrlMethodMap;
typedef std::map<int64, std::string> ExportNodeNameMap;

class WebServer : public Utils::HttpServer, public Utils::Singleton<WebServer>
{
    friend class Utils::Singleton<WebServer>;
	friend class RESTRequestHelper;
private:
    int64 m_nSessionCleanTime;
    WebUrlMethodMap m_nMethods;
    boost::shared_ptr<RESTRequestHelper>       m_nRESTRequestHelper;
    boost::shared_ptr<DatabaseConnectionPool>  m_nDBPool;
    std::string                                m_strJwtKeyName;
    time_t                                     m_nCookieExpireTime;
    std::string                                m_strAuthorizationUrl;
    WebServer();

    virtual ~WebServer();

    virtual bool OnHttpRequest( int32 nClientId, Utils::HttpContext &nHttpHeaders, Utils::Socket &nChannel, bool &bEnable );

	bool _SendContextResponse( Utils::HttpContext &nHttpContext, WebContext &nWebContext, Utils::Socket &nChannel, bool &bEnable );
	bool _ResponseHttpRequest( Utils::HttpContext &nContext, WebContext &nWebContext, Utils::Socket &nChannel, bool &bEnable );

	void _RegisterAll();
    uint32 OnNewDeviceId(Utils::HttpContext &nHttpContext, WebContext &nWebContext);
    uint32 OnStartTest(Utils::HttpContext &nHttpContext, WebContext &nWebContext);
    uint32 OnStopTest(Utils::HttpContext &nHttpContext, WebContext &nWebContext);
	///Export by ymt
// 	bool _ExportResultList( Json::Value &nResultList, Utils::HttpContext &nHttpContext, WebContext &nWebContext );
// 	bool _ExportResult2OpenSheet( void *lpExcelHandle, void *lpSheetHandle, const Json::Value &nExportConfig, const Json::Value &nResultList );
// 	bool _SaveOpenExcel2Context( void *lpExcelHandle, const std::string &strFilename, WebContext &nWebContext );
// 	void _FillOpenColumnValue( void *lpCellPtr, 
// 		const ExportNodeNameMap &nAllNodeNames,
// 		const ExportNodeNameMap &nAllNodeGroupNames,
// 		const ExportNodeNameMap &nAllDeviceNames,
// 		const ExportNodeNameMap &nSourceAddressNames,
// 		const ExportNodeNameMap &nDestAddressNames,
// 		const ExportNodeNameMap &nAllSystemGroupNames,
// 		const ExportNodeNameMap &nAllNodeMarkingNames,
// 		const Json::Value &nColumnItem, 
// 		const Json::Value &nValue, 
// 		bool &bThresholdExceed,
// 		int nTargetType = 0	);
// 	bool _ConvertHtml2Pdf( const std::string &strHtmlContent, std::string &strPdfContent);

public:
    bool Initialize();
    bool Exit();
    boost::shared_ptr<Jwt::JsonWebToken>       m_pJsonWebToken;

};
#endif //LIVESERVER_WEBSERVER_H
