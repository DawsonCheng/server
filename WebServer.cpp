
#include "Configure.h"
#include <Core/Utils/Log.h>
#include <Core/Utils/Compress.h>
#include <Core/Utils/File.h>
#include <Core/Utils/Ssh2Client.h>
#include <Core/Common/Module.h>
#include <Core/Common/Test.h>
#include <Core/Utils/Encoding.h>
#include <ThirdParty/LibJson/reader.h>
#include <boost/make_shared.hpp>
#include <Core/Utils/Singleton.h>
#include "WebServer.h"
#include "TestManger.h"
#include "TestManger.h"

WebContext::WebContext()
{
    m_nSessionId = 0;
    m_nUserId = 0;
    m_nGroupId = 0;
    m_lpChannel = NULL;
    m_lpHttpEnabled = NULL;
    m_nForwardAuthorized = -1;
}

WebContext::~WebContext()
{

}

WebServer::WebServer() : Utils::HttpServer("WebServer")
{
    m_strJwtKeyName = "jwt";
    m_nCookieExpireTime = 3600 * 24;
}

WebServer::~WebServer()
{
}

bool WebServer::Initialize()
{
    std::string strError;
    _RegisterAll();
    WebServerConfigure &nConfig   = Configure::Instance().m_nWebServer;
    DatabaseConfigure  &nDbConfig = Configure::Instance().m_nDatabase;

    m_nDBPool.reset(new DatabaseConnectionPool(nConfig.m_nConnectionThreadCount, nConfig.m_nConnectionThreadCount+1));
    if(!m_nDBPool->Init(nDbConfig.m_strUrl, nDbConfig.m_strUser, nDbConfig.m_strPassword))
    {
        __ULOG_ERROR(__ULOG_FMT("WebServer", "Connect to database failed"));
        return false;
    }

    m_nRESTRequestHelper.reset(new RESTRequestHelper(m_nDBPool));
    m_strServerName         = nConfig.m_strName;
    m_bKeepAlive            = nConfig.m_bKeepAlive;
    m_nMaxClient            = nConfig.m_nMaxClients;
    m_nMaxHeaderSize        = nConfig.m_nMaxHeaderSize;
    m_nReceiveTimeout       = nConfig.m_nReceiveTimeout;
    m_nCacheExpireTime      = nConfig.m_nCacheExpireTime;
    m_nConnectionServiceTimes  = nConfig.m_nConnectionServiceTimes;
    m_nConnectionIdleTime   = nConfig.m_nConnectionIdleTime;
    m_strFileDirectory      = nConfig.m_strDirectory;
    m_strDefaultContentType = nConfig.m_strDefContentType;
    (* m_pContentTypes)     = nConfig.m_nHttpContentTypes;
    (* m_pSslMapPorts)      = nConfig.m_nSslMapPorts;
    m_nConnectionThreadCount = nConfig.m_nConnectionThreadCount;

    if( nConfig.m_nListenAddresses.size() > 0 && !Utils::HttpServer::Startup(nConfig.m_nListenAddresses) )
    {
        __ULOG_ERROR(__ULOG_FMT("WebServer", "Start http server failed"));
        return false;
    }

    std::string file = Utils::String::Format("%s/%s", Utils::GetWorkingDirectory().c_str(), nConfig.m_strInterfaceDescribeFile.c_str());
    if(!m_nRESTRequestHelper->LoadInterfaceFile(file))
    {
        __ULOG_ERROR(__ULOG_FMT("WebServer", "Start http server failed  RESTRequestHelper load file failed"));
        return false;
    }
    m_pJsonWebToken.reset(new Jwt::JsonWebToken());
	m_pJsonWebToken->SetJsonWebTokenIss(nConfig.m_strJsonWebTokenIss.empty()? "LiveServer" : nConfig.m_strJsonWebTokenIss);
	m_pJsonWebToken->SetJsonWebTokenIssueds(nConfig.m_strJsonWebTokenIssueds.empty()? "NetVista" : nConfig.m_strJsonWebTokenIssueds);
	m_pJsonWebToken->SetJsonWebTokenTimeout(nConfig.m_nJsonWebTokenTimeout <= 0? 300 : nConfig.m_nJsonWebTokenTimeout);
    m_strAuthorizationUrl = nConfig.m_strAuthorizationUrl;
    //m_nSummaryDisplay.Init();
    return true;

}

bool WebServer::Exit()
{
    Utils::HttpServer::Cleanup();
    return true;
}

void WebServer::_RegisterAll()
{

    m_nMethods["/newDeviceId"] = &WebServer::OnNewDeviceId;
    m_nMethods["/startTest"] = &WebServer::OnStartTest;
    m_nMethods["/stopTest"] = &WebServer::OnStopTest;
}

bool WebServer::OnHttpRequest( int32 nClientId, Utils::HttpContext &nHttpContext, Utils::Socket &nChannel, bool &bEnable )
{

    int nErrorCode = Common::ERR_SUCCESS;
    const WebServerConfigure &nConfig = Configure::Instance().m_nWebServer;

    WebContext nWebContext;

    nWebContext.m_lpChannel = &nChannel;
    nWebContext.m_lpHttpEnabled = &bEnable;
    nWebContext.m_bCompressResponse = nConfig.m_bCompressContent;
    nWebContext.m_strClientIp = nChannel.GetPeerAddress().ToString();
    nWebContext.m_nJsonRequest  = Json::Value(Json::objectValue);
    nWebContext.m_nJsonResponse = Json::Value(Json::objectValue);
    nWebContext.m_nSessionId = 0; //nHttpContext.m_nParameters.GetValue("sessionId", (int64)0);
    std::string strCookie  = nHttpContext.m_nHeaders.GetValue("cookie", std::string(""));
    std::string strJwt     = nHttpContext.m_nParameters.GetValue("jwt", std::string(""));
	std::string strHost	   = Utils::String::Format("http://%s",nHttpContext.m_nHeaders.GetValue("host",std::string("")).c_str());

    if( strJwt.empty() && std::string::npos != strCookie.find(m_strJwtKeyName) )
    {
        Utils::StringMap nProperties;
        Utils::String::ParseAttributes(strCookie, nProperties, ';', '=', true, true, true);
        strJwt = nProperties[m_strJwtKeyName];
	}

	do 
	{
		std::string iss;
		std::string strNewJwt;
		uint32 nJsonWebTokenCode = m_pJsonWebToken->ValidateJWT(strJwt, iss, nWebContext.m_strUsername, nWebContext.m_nUserId);
		if(nConfig.m_bAuthorizationAble && nJsonWebTokenCode != Jwt::JsonWebToken::ERR_SUCCESS)
		{

			if ( nJsonWebTokenCode == Jwt::JsonWebToken::ERR_TIMEOUT && nHttpContext.m_strUri != "/" )
			{
				nHttpContext.m_nCode = Utils::HttpContext::ERR_HTTP_UNAUTHORIZED;
				nErrorCode = Common::ERR_NO_SUCH_SESSION;
				break;
			}

			nErrorCode = Common::ERR_ACCESS_DENIED;
			nHttpContext.m_nCode = Utils::HttpContext::ERR_HTTP_MOVED_TEMPORARILY;
			std::string strUrl = Utils::String::Format("%s/?systemIdentification=%s",strHost.c_str(),nConfig.m_strSystemIdentification.c_str());
			for (  Utils::StringMap::const_iterator itr = nHttpContext.m_nParameters.GetData().begin(); itr != nHttpContext.m_nParameters.GetData().end(); itr ++ )
			{
				Utils::String::AppendFormat(strUrl, "&%s=%s", itr->first.c_str(), itr->second.c_str());
			}
		
			nWebContext.m_nResponseHeaders.SetValue(Utils::HttpContext::HTTP_LOCATION_NAME,  Utils::String::Format("%s&subsystem=%s",m_strAuthorizationUrl.c_str(), Utils::String::UrlEncode(strUrl).c_str()));
			__ULOG_TRACE(__ULOG_FMT("WebServer","Validate Jwt failed jwt=%s "),strJwt.c_str());
			break;
		}
		else
		{
			std::string newIss;
			m_pJsonWebToken->CreateJWT(strNewJwt, iss, nWebContext.m_strUsername, nWebContext.m_nUserId);
			nWebContext.m_nSetCookies.SetValue(Utils::String::Format("jwt=%s;HttpOnly;expires=%s",
				strNewJwt.c_str(), Utils::String::FormatLongTime2(time(NULL) + m_nCookieExpireTime, true).c_str()));
			if (nHttpContext.m_strUrl.find("jwt") != std::string::npos)
			{
				nHttpContext.m_nCode = Utils::HttpContext::ERR_HTTP_MOVED_TEMPORARILY;
				for (  Utils::StringMap::const_iterator itr = nHttpContext.m_nParameters.GetData().begin(); itr != nHttpContext.m_nParameters.GetData().end(); itr ++ )
				{
					bool bfirst = true;
					if (itr->first != "systemIdentification" && itr->first != "jwt")
					{
						if(bfirst) 
						{
							Utils::String::AppendFormat(strHost, "/?%s=%s", itr->first.c_str(), itr->second.c_str());
							bfirst = false;
						}else
						{
							Utils::String::AppendFormat(strHost, "&%s=%s", itr->first.c_str(), itr->second.c_str());
						}
					}
				}
				nWebContext.m_nResponseHeaders.SetValue(Utils::HttpContext::HTTP_LOCATION_NAME,  strHost);
				break;
			}
		}

		WebUrlMethodMap::iterator itrMethod = m_nMethods.find(nHttpContext.m_strUri);
		if( itrMethod == m_nMethods.end() && (!m_nRESTRequestHelper->IsRESTRequest(nHttpContext.m_strUri)))
		{
			//__ULOG_INFO(__ULOG_FMT("WebServer","http request not find uri .uri = %s"),nHttpContext.m_strUri.c_str());
			// default
			//return Utils::HttpServer::OnHttpRequest(nClientId, nHttpContext, nChannel, bEnable);
			return _ResponseHttpRequest(nHttpContext, nWebContext, nChannel, bEnable);
		}

		if( !nHttpContext.m_strBody.empty() )
		{
			std::string strContentEncoding = nHttpContext.m_nHeaders.GetValue(Utils::HttpContext::HTTP_CONTENT_ENCODING_NAME, "");
			bool bCompressed = false;
			bool bUsingGzip = false;
			if( Utils::String::Compare2(strContentEncoding.c_str(), "gzip", true) == 0 )
			{
				bCompressed = true;
				bUsingGzip = true;
			}
			else if( Utils::String::Compare2(strContentEncoding.c_str(), "deflate", true) == 0 )
			{
				bCompressed = true;
				bUsingGzip = false;
			}
			if( bCompressed )
			{
				std::string strNewContent;
				int nResult = Utils::ZlibProvider::Inflate(nHttpContext.m_strBody.c_str(), nHttpContext.m_strBody.size(), strNewContent, bUsingGzip);
				if( nResult != Utils::ZlibProvider::RESULT_OK )
				{
					__ULOG_TRACE(__ULOG_FMT("WebServer", "Decode compressed http data from(%s) failed, code(%d)"),
								 nWebContext.m_strClientIp.c_str(), nResult);
					nErrorCode = Common::ERR_INTERNAL_ERROR;
				}
				else
				{
					nHttpContext.m_strBody = strNewContent;
				}
			}

			if( Utils::String::Compare2(nHttpContext.m_strContentType.c_str(), "application/json", true) == 0 )
			{
				Json::Reader nJsonReader;
				if( !nJsonReader.parse(nHttpContext.m_strBody, nWebContext.m_nJsonRequest, false) )
				{
					__ULOG_TRACE(__ULOG_FMT("WebServer", "Parse json request from(%s) failed, request(%s), error(%s)"),
								 nWebContext.m_strClientIp.c_str(),
								 nHttpContext.m_strBody.c_str(),
								 nJsonReader.getFormatedErrorMessages().c_str());
					nErrorCode = Common::ERR_INTERNAL_ERROR;
				}
			}
		}


		do
		{
			if( Common::ERR_SUCCESS != nErrorCode )
			{
				break;
			}
			if(m_nRESTRequestHelper->IsRESTRequest(nHttpContext.m_strUri))
			{
				nHttpContext.m_nCode = m_nRESTRequestHelper->RESTHttpRequest(nHttpContext, nWebContext);
			}
			else
			{
				WebUrlMethodProc pMethod = itrMethod->second;
				nHttpContext.m_nCode = (this->*pMethod)(nHttpContext, nWebContext);
			}

		} while (false);

	} while (false);

    if( Common::ERR_SUCCESS != nErrorCode && nErrorCode != Common::ERR_ACCESS_DENIED )
    {
        nHttpContext.m_nCode = Utils::HttpContext::ERR_HTTP_SUCCESS;

        if( nHttpContext.m_strUri == "/" )
        {
            nWebContext.m_nJsonResponse = Json::Value::null;
            nWebContext.m_strResponse = Utils::String::Format("%u, %s", nErrorCode, Common::GetErrorDescribe(nErrorCode));
        }
        else
        {
            nWebContext.m_nJsonResponse["errorCode"] = (Json::Int)nErrorCode;
            nWebContext.m_nJsonResponse["errorDescription"] = Common::GetErrorDescribe(nErrorCode);
        }
    }

    return _SendContextResponse(nHttpContext, nWebContext, nChannel, bEnable);
}

bool WebServer::_ResponseHttpRequest( Utils::HttpContext &nContext, WebContext &nWebContext, Utils::Socket &nChannel, bool &bEnable )
{

	uint32 nErrorCode = Utils::HttpContext::ERR_HTTP_SUCCESS;
	uint64 nFileSize = 0;
	bool bRedirect = false;
	bool bNotModified = false;
	bool bDownload  = nContext.m_nParameters.GetValue("download", false);
	bool bKeepAlive = (m_bKeepAlive && nContext.m_nServicedTimes < m_nConnectionServiceTimes);
	std::string strClient = nContext.m_nParameters.GetValue("client", "");
	Utils::File nFile;
	Utils::FileAttribute nFileAttr;
	std::string strRedirectUrl;
	std::string strFilePath;
	std::string strContentType = m_strDefaultContentType;
	std::string strContentEncoding = "none";
	std::string strUri = nContext.m_strUri;
	std::string strIndexFileName = m_strIndexFileName;

	if( strClient == "speedtester" )
	{
		int nBodySize = (size_t)nContext.m_nParameters.GetValue("size", (int)0);
		if( nBodySize > 0 )
		{
			__ULOG_TRACE(__ULOG_FMT("HttpServer", "%s, SpeedTester GET %s,%u,%d"), 
				nChannel.GetPeerAddress().ToLongString().c_str(),
				nContext.m_strUrl.c_str(),
				nErrorCode,
				nBodySize);

			std::string strHttpContext = Utils::String::Format("%s %u %s\r\n"
				"Server: %s\r\n"
				"%s: %d\r\n"
				"%s: %s\r\n",
				Utils::HttpContext::HTTP_VERSION_11_NAME,
				nErrorCode,
				Utils::HttpContext::GetErrorDescribe(nErrorCode),
				m_strServerName.c_str(),
				Utils::HttpContext::HTTP_CONTENT_LENGTH_NAME, nBodySize,
				Utils::HttpContext::HTTP_CONNECTION_NAME, bKeepAlive ? "keep-alive" : "close");

			const Utils::StringArray &nAllSetCokies = nWebContext.m_nSetCookies.GetData();
			for ( Utils::StringArray::const_iterator itr = nAllSetCokies.begin(); itr != nAllSetCokies.end(); itr ++ )
			{
				Utils::String::AppendFormat(strHttpContext, "Set-Cookie: %s\r\n", (*itr).c_str());
			}
			if(strUri == "/index.html") strHttpContext += "Cache-Control: no-store\r\n";
			strHttpContext += "\r\n";

			if( !nChannel.SendHuge(strHttpContext.c_str(), strHttpContext.size()) ) 
			{
				__ULOG_TRACE(__ULOG_FMT_ERR("HttpServer", "Send speed http header failed ,size("_SIZE_TFMT_")"),
					strHttpContext.size(), __UERR_CODE, __UERR_STR);
				return false;
			}

			return nContext.SendBody4SpeedTest(nChannel, bEnable, nBodySize);
		}
	}

	uint16 nListenPort = nChannel.GetAddress().GetPort();
	if( m_pIndexNames->find(nListenPort) != m_pIndexNames->end() )
	{
		strIndexFileName = (*m_pIndexNames)[nListenPort];
	}

	if( nContext.m_nLength > 0 && !nContext.m_bBodyReceived )
	{
		char nBodyData = 0;
		size_t nBodyReceived = 0;

		while( nBodyReceived < nContext.m_nLength )
		{
			if( !nChannel.Receive(&nBodyData, 1, bEnable) )
			{
				__ULOG_TRACE(__ULOG_FMT_ERR("HttpServer", "Receive http request data failed"), __UERR_CODE, __UERR_STR);
				return false;
			}
			nBodyReceived ++;
		}
		nContext.m_bBodyReceived = true;
	}

	do 
	{
		if( strUri.size() == 0 || strUri[0] != '/' ) strUri = "/" + strUri;
		if( strUri == "/" ) strUri += strIndexFileName;
		if( strUri.find("/../") != std::string::npos ||
			strUri.find("\\..\\") != std::string::npos ||
			strUri.find("/..\\") != std::string::npos ||
			strUri.find("\\../") != std::string::npos )
		{
			__ULOG_INFO(__ULOG_FMT("HttpServer", "Uri(%s) access denied to visit parent path, forbiden"), strUri.c_str());
			nErrorCode = Utils::HttpContext::ERR_HTTP_FORBIDDEN;
			break;
		}

		strFilePath = m_strFileDirectory + strUri;

		if( !Utils::File::GetAttribue(strFilePath, nFileAttr) )
		{
			__ULOG_TRACE(__ULOG_FMT("HttpServer", "Uri(%s) file not exist"), strUri.c_str());
			nErrorCode = Utils::HttpContext::ERR_HTTP_NOT_FOUNT;
			break;
		}
		else if( nFileAttr.m_bDirectory )
		{
			if( strUri[strUri.size() - 1] != '/' )
			{
				// redirect
				strRedirectUrl = strUri + "/";
				bRedirect = true;
				break;
			}

			Utils::String::AppendFormat(strFilePath, "/%s", strIndexFileName.c_str());

			if( !Utils::File::GetAttribue(strFilePath, nFileAttr) )
			{
				__ULOG_INFO(__ULOG_FMT("HttpServer", "Uri(%s) default file(%s) not exist"), strUri.c_str(), strIndexFileName.c_str());
				nErrorCode = Utils::HttpContext::ERR_HTTP_NOT_FOUNT;
				break;
			}
		}

		nFileSize = nFileAttr.m_nSize;

		std::string strFileExt = Utils::String::ToLower(Utils::File::GetExtensionName(strFilePath));
		std::string strLastModified = Utils::String::FormatLongTime(nFileAttr.m_nModifyTime, true);

		if( m_pCompressTypes->find(strFileExt) != m_pCompressTypes->end() )
		{
			strContentEncoding = (*m_pCompressTypes)[strFileExt];

			if( strFileExt.size() + 1 < strFilePath.size() )
			{
				std::string strActualFile = strFilePath.substr(0, strFilePath.size() - strFileExt.size() - 1);
				strFileExt = Utils::String::ToLower(Utils::File::GetExtensionName(strActualFile));
			}
		}

		if( m_pContentTypes->find(strFileExt) != m_pContentTypes->end() )
		{
			strContentType = (*m_pContentTypes)[strFileExt];
		}

		std::string strRequestModified = nContext.m_nHeaders.GetValue(Utils::HttpContext::HTTP_IF_MODIFIED_SINCE_NAME, "");
		if( !bDownload && Utils::String::Compare(strRequestModified, strLastModified, true) == 0 )
		{
			bNotModified = true;
		}
		else
		{
			if( !nFile.Open(strFilePath, Utils::File::FILE_M_READ) )
			{
				__ULOG_ERROR(__ULOG_FMT_ERR("HttpServer", "Open uri(%s),file(%s) failed"), strUri.c_str(), strFilePath.c_str(), __UERR_CODE, __UERR_STR);
				nErrorCode = Utils::HttpContext::ERR_HTTP_SERVER_ERROR;
				break;
			}
		}
	}while(false);

	std::string strHttpContext;
	if( bRedirect )
	{
		nErrorCode = Utils::HttpContext::ERR_HTTP_MOVED_PERMANENTLY;
		nFileSize = 0;

		strHttpContext = Utils::String::Format("%s %u %s\r\n"
			"Server: %s\r\n"
			"%s: 0\r\n"
			"%s: %s\r\n"
			"%s: %s\r\n"
			"\r\n",
			Utils::HttpContext::HTTP_VERSION_11_NAME,
			nErrorCode,
			Utils::HttpContext::GetErrorDescribe(nErrorCode),
			m_strServerName.c_str(),
			Utils::HttpContext::HTTP_CONTENT_LENGTH_NAME,
			Utils::HttpContext::HTTP_LOCATION_NAME, strRedirectUrl.c_str(),
			Utils::HttpContext::HTTP_CONNECTION_NAME, bKeepAlive ? "keep-alive" : "close");
	}
	else if( bNotModified )
	{
		nErrorCode = Utils::HttpContext::ERR_HTTP_NOT_MODIFIED;
		nFileSize = 0;

		strHttpContext = Utils::String::Format("%s %u %s\r\n"
			"Server: %s\r\n"
			"%s: 0\r\n"
			"%s: %s\r\n"
			"\r\n",
			Utils::HttpContext::HTTP_VERSION_11_NAME,
			nErrorCode,
			Utils::HttpContext::GetErrorDescribe(nErrorCode),
			m_strServerName.c_str(),
			Utils::HttpContext::HTTP_CONTENT_LENGTH_NAME,
			Utils::HttpContext::HTTP_CONNECTION_NAME, bKeepAlive ? "keep-alive" : "close");
	}
	else
	{
		if( Utils::HttpContext::ERR_HTTP_SUCCESS != nErrorCode )
		{
			nFileSize = (uint64)strlen(Utils::HttpContext::GetErrorDescribe(nErrorCode));
		}

		std::string strDispName  = nContext.m_nParameters.GetValue("filename", "");
		std::string strFileName  = Utils::File::GetNameFromPath(strUri);
		std::string strDateValue = Utils::String::FormatLongTime(time(NULL), true);
		std::string strExpiresValue = Utils::String::FormatLongTime(time(NULL) + m_nCacheExpireTime, true);
		std::string strLastModified = Utils::String::FormatLongTime(nFileAttr.m_nModifyTime, true);

		strHttpContext = Utils::String::Format("%s %u %s\r\n"
			"Server: %s\r\n"
			"%s: %s\r\n"
			"%s: "_U64FMT_"\r\n"
			"%s: %s\r\n"
			"%s: %s\r\n"
			"%s: %s\r\n"
			"%s: %s\r\n"
			"%s: %s\r\n",
			Utils::HttpContext::HTTP_VERSION_11_NAME,
			nErrorCode,
			Utils::HttpContext::GetErrorDescribe(nErrorCode),
			m_strServerName.c_str(),
			Utils::HttpContext::HTTP_CONTENT_TYPE_NAME, strContentType.c_str(),
			Utils::HttpContext::HTTP_CONTENT_LENGTH_NAME, nFileSize,
			Utils::HttpContext::HTTP_CONTENT_ENCODING_NAME, strContentEncoding.c_str(),
			Utils::HttpContext::HTTP_CONNECTION_NAME, bKeepAlive ? "keep-alive" : "close",
			Utils::HttpContext::HTTP_DATE_NAME, strDateValue.c_str(),
			Utils::HttpContext::HTTP_EXPIRES_NAME, strExpiresValue.c_str(),
			Utils::HttpContext::HTTP_LAST_MODIFIED_NAME, strLastModified.c_str());

		if( bDownload )
		{
			if( strDispName.empty() )
			{
				strDispName = strFileName;
			}

			nContext.FormatDispositionName(strDispName);
			Utils::String::AppendFormat(strHttpContext, "%s: attachment; filename=\"%s\"\r\n", Utils::HttpContext::HTTP_CONTENT_DISP_NAME, strDispName.c_str());
		}

		if( Utils::HttpContext::ERR_HTTP_SUCCESS != nErrorCode )
		{
			strHttpContext += Utils::HttpContext::GetErrorDescribe(nErrorCode);
		}
	}

	__ULOG_TRACE(__ULOG_FMT("HttpServer", "%s, GET %s,%u,"_U64FMT_), 
		nChannel.GetPeerAddress().ToLongString().c_str(),
		nContext.m_strUrl.c_str(),
		nErrorCode,
		nFileSize);

	const Utils::StringArray &nAllSetCokies = nWebContext.m_nSetCookies.GetData();
	for ( Utils::StringArray::const_iterator itr = nAllSetCokies.begin(); itr != nAllSetCokies.end(); itr ++ )
	{
		Utils::String::AppendFormat(strHttpContext, "Set-Cookie: %s\r\n", (*itr).c_str());
	}

	if(strUri == "/index.html") 
	{
		strHttpContext += "Cache-Control: no-store\r\n";
	}
	strHttpContext += "\r\n";

	if( !nChannel.SendHuge(strHttpContext.c_str(), strHttpContext.size()) )
	{
		__ULOG_TRACE(__ULOG_FMT_ERR("HttpServer", "Send http header failed ,size("_SIZE_TFMT_")"),
			strHttpContext.size(), __UERR_CODE, __UERR_STR);
		return false;
	}

	if( Utils::HttpContext::ERR_HTTP_SUCCESS != nErrorCode )
	{
		return bKeepAlive;
	}

	uint64 nTotalSend = 0;
	const size_t nBufferSize = 8192;
	char nSendBuffer[nBufferSize] = {0};

	while( nTotalSend < nFileSize )
	{
		size_t nNeedRead = Utils::Min<size_t>(nBufferSize, (size_t)(nFileSize - nTotalSend));
		size_t nActualRead = nFile.Read(nSendBuffer, 1, nNeedRead);

		if( nActualRead > 0 && nChannel.Send(nSendBuffer, (int)nActualRead) != (int)nActualRead )
		{
			__ULOG_TRACE(__ULOG_FMT_ERR("HttpServer", "Send http file data failed, size("_SIZE_TFMT_")"), nActualRead, __UERR_CODE, __UERR_STR);
			nFile.Close();
			return false;
		}

		if( nActualRead != nNeedRead )
		{
			__ULOG_TRACE(__ULOG_FMT_ERR("HttpServer", "Read file content failed"), __UERR_CODE, __UERR_STR);
			nFile.Close();
			return false;
		}

		nTotalSend += nActualRead;
	}

	nFile.Close();

	return bKeepAlive;
}

bool  WebServer::_SendContextResponse( Utils::HttpContext &nHttpContext, WebContext &nWebContext, Utils::Socket &nChannel, bool &bEnable )
{
    const WebServerConfigure &nConfig = Configure::Instance().m_nWebServer;
    bool bKeepAlive = (m_bKeepAlive && nHttpContext.m_nServicedTimes < m_nConnectionServiceTimes);
    bool bJsonContent = false;

    if( !nWebContext.m_nJsonResponse.isNull() && nWebContext.m_nJsonResponse.size() > 0 )
    {
        bJsonContent = true;
        nWebContext.m_strResponse = nWebContext.m_nJsonResponse.toFastRestrictString();
    }

    std::string strUsedEncoding;
    if( nWebContext.m_bCompressResponse && nWebContext.m_strResponse.size() >= nConfig.m_nCompressMinSize )
    {
        do
        {
            std::string strUserAgent = nHttpContext.m_nHeaders.GetValue(Utils::HttpContext::HTTP_USER_AGENT_NAME, std::string(""));
            if( nConfig.IsCompressNotSupported(strUserAgent) )
            {
                // IE does not support compression using ajax
                break;
            }

            Utils::StringList nAcceptEncodings;
            std::string strAcceptEncoding = nHttpContext.m_nHeaders.GetValue(Utils::HttpContext::HTTP_ACCEPT_ENCODING_NAME, std::string(""));
            Utils::String::Split(strAcceptEncoding, nAcceptEncodings, ',');

            for( Utils::StringList::iterator itrEncoding = nAcceptEncodings.begin(); itrEncoding != nAcceptEncodings.end(); itrEncoding ++ )
            {
                std::string strEncoding = Utils::String::ToLower(Utils::String::Trim(*itrEncoding));
                if( strEncoding == "gzip" )
                {
                    // gzip first
                    strUsedEncoding = strEncoding;
                    break;
                }

                if( strEncoding == "deflate" )
                {
                    strUsedEncoding = strEncoding;
                }
            }

            if( strUsedEncoding.size() == 0 )
            {
                break;
            }

            int64 nCompressBeginTime = Utils::GetHighResolutionTime();
            std::string strCompressed;

            int nCompressResult = Utils::ZlibProvider::Deflate(nWebContext.m_strResponse.c_str(), nWebContext.m_strResponse.size(),
                                                               strCompressed, Utils::ZlibProvider::BEST_COMPRESSION, strUsedEncoding == "gzip");
            if( Utils::ZlibProvider::RESULT_OK == nCompressResult )
            {
                int64 nCompressUsedTime = Utils::GetHighResolutionTime() - nCompressBeginTime;
                __ULOG_TRACE(__ULOG_FMT("WebServer", "Compress url(%s) using(%s) with response ratio(%0.2lf%%), new size("_SIZE_TFMT_"), used "_I64FMT_" ms"),
                             nHttpContext.m_strUri.c_str(),
                             strUsedEncoding.c_str(),
                             double(strCompressed.size()) * 100.0 / double(nWebContext.m_strResponse.size()),
                             strCompressed.size(),
                             nCompressUsedTime / Utils::MICRO_UNITS_PER_MILLI);

                nWebContext.m_strResponse = strCompressed;
            }
            else
            {
                __ULOG_ERROR(__ULOG_FMT("WebServer", "Compress data using(%s) failed, zerror(%d)"), strUsedEncoding.c_str(), nCompressResult);

                strUsedEncoding.clear();
            }

        } while (false);
    }

    std::string strHeader = Utils::String::Format("%s %u %s\r\n"
                                                          "Server: %s\r\n"
                                                          "Content-Length: "_SIZE_TFMT_"\r\n"
                                                          "Connection: %s\r\n",
                                                  Utils::HttpContext::HTTP_VERSION_11_NAME, nHttpContext.m_nCode, Utils::HttpContext::GetErrorDescribe(nHttpContext.m_nCode),
                                                  m_strServerName.c_str(),
                                                  nWebContext.m_strResponse.size(),
                                                  bKeepAlive ? "keep-alive" : "close");

    // add extra headers
    bool bContentTypeExist = false;
    const Utils::StringMap &nAllValues = nWebContext.m_nResponseHeaders.GetData();
    for( Utils::StringMap::const_iterator itr = nAllValues.begin(); itr != nAllValues.end(); itr ++ )
    {
        Utils::String::AppendFormat(strHeader, "%s: %s\r\n", itr->first.c_str(), itr->second.c_str());

        if( Utils::String::Compare2(itr->first.c_str(), Utils::HttpContext::HTTP_CONTENT_TYPE_NAME, true) == 0 )
        {
            bContentTypeExist = true;
        }
    }

    if( !bContentTypeExist )
    {
        if( bJsonContent ) strHeader += "Content-Type: application/json; charset=utf-8\r\n";
        else 
		{
			strHeader += "Content-Type: text/html\r\n";
			strHeader += "Cache-Control: no-cache\r\n";
		}
    }

    if( strUsedEncoding.size() > 0 )
    {
        Utils::String::AppendFormat(strHeader, "Content-Encoding: %s\r\n", strUsedEncoding.c_str());
    }
	const Utils::StringArray &nAllSetCokies = nWebContext.m_nSetCookies.GetData();
	for ( Utils::StringArray::const_iterator itr = nAllSetCokies.begin(); itr != nAllSetCokies.end(); itr ++ )
	{
		Utils::String::AppendFormat(strHeader, "Set-Cookie: %s\r\n", (*itr).c_str());
	}

    strHeader += "\r\n";

    if( !nChannel.SendHuge(strHeader.c_str(), strHeader.size())  )
    {
        __ULOG_TRACE(__ULOG_FMT_ERR("WebServer", "Send url(%s) response header failed"),
                     nHttpContext.m_strUri.c_str(), __UERR_CODE, __UERR_STR);
        return false;
    }

    if( nWebContext.m_strResponse.size() > 0 &&
        !nChannel.SendHuge(nWebContext.m_strResponse.c_str(), nWebContext.m_strResponse.size()) )
    {
        __ULOG_TRACE(__ULOG_FMT_ERR("WebServer", "Send url(%s) response body failed"),
                     nHttpContext.m_strUri.c_str(), __UERR_CODE, __UERR_STR);
        return false;
    }

    return true;
}


uint32 WebServer::OnNewDeviceId(Utils::HttpContext &nHttpContext, WebContext &nWebContext)
{
    std::string strDeviceId;
    int32 nErrorCode = Common::ERR_SUCCESS;
    Json::Value &results = nWebContext.m_nJsonResponse["results"];
    results = Json::Value(Json::arrayValue);

    if( strDeviceId.empty() )
    {
        int64 nTime64Seed = Utils::GetHighResolutionTime();
        strDeviceId = Utils::String::Format("{%08X-%04X-%04X-%04X-%04X%08X}",
                                                (uint32)(nTime64Seed >> 32),
                                                rand() % 0xFFFF,
                                                rand() % 0xFFFF,
                                                rand() % 0xFFFF,
                                                rand() % 0xFFFF,
                                                (uint32)nTime64Seed);
    }
    results[(Json::UInt)0]["m_strDeviceId"] = strDeviceId;
    nWebContext.m_nJsonResponse["errorCode"] = nErrorCode;
    return Utils::HttpContext::ERR_HTTP_SUCCESS;
}

uint32 WebServer::OnStartTest(Utils::HttpContext &nHttpContext, WebContext &nWebContext)
{
    int32 nErrorCode = Common::ERR_SUCCESS;
    const Json::Value &items = nWebContext.m_nJsonRequest["items"];
    for( Json::UInt i =0; i< items.size(); i++)
    {
        const Json::Value &item = items[i];
        int64 testId =item["testId"].asInt64();
    }

    nWebContext.m_nJsonResponse["errorCode"] = nErrorCode;
    return Utils::HttpContext::ERR_HTTP_SUCCESS;
}

uint32 WebServer::OnStopTest(Utils::HttpContext &nHttpContext, WebContext &nWebContext)
{
    int32 nErrorCode = Common::ERR_SUCCESS;
    const Json::Value &items = nWebContext.m_nJsonRequest["items"];
    for( Json::UInt i =0; i< items.size(); i++)
    {
        const Json::Value &item = items[i];
        int64 testId =item["testId"].asInt64();
        //TestManger::Instance().CancelTest(testId);
    }
    nWebContext.m_nJsonResponse["errorCode"] = nErrorCode;
    return Utils::HttpContext::ERR_HTTP_SUCCESS;
}


bool RESTRequestHelper::IsRESTRequest(std::string &resource)
{
    if(m_nInterFaceMap.find(resource) == m_nInterFaceMap.end())
    {
        return false;
    }
    return true;
}

bool RESTRequestHelper::LoadInterfaceFile(std::string &strfile)
{
    if( strfile.empty() )
    {
        __ULOG_ERROR(__ULOG_FMT("WebServer", "File of nInterface config is not exist"));
        return false;
    }
    Utils::File file;
    if(!file.Open(strfile, Utils::File::FILE_M_READ|Utils::File::FILE_M_BINARY))
    {
        __ULOG_ERROR(__ULOG_FMT("WebServer", "Open nInterface file failed ,file name = %s"), strfile.c_str());
        return false;
    }

    uint64 size = file.GetSize();
    std::string readBuffer;
    readBuffer.resize((size_t)size);
    file.ReadHuge((char *)readBuffer.data(), (size_t)size);
    Json::Value tempJson;
    if(!tempJson.fromString(readBuffer))
    {
       __ULOG_ERROR(__ULOG_FMT("WebServer", "read interface config failed !"));
    }

    m_nFilterDefault = tempJson["FilterConditionDefault"];
    Json::Value &Interfaces = tempJson["Interfaces"];
    for(Json::UInt i = 0; i< Interfaces.size(); i++)
    {
        Json::Value &nInterfaceJason = Interfaces[i];
        RESTInterface  nInterface;
        nInterface.m_nResource = Utils::String::Format("/%s", nInterfaceJason["resouce"].asString().c_str());
        nInterface.m_nTable    = nInterfaceJason["table"].asString();
        nInterface.m_nAllowConditionWords = nInterfaceJason["conditionWord"];
        Json::Value &actionJson = nInterfaceJason["action"];
        for(Json::UInt i = 0; i <actionJson.size(); i++)
        {
            nInterface.m_nActions.insert(actionJson[i].asString());
        }
        Json::Value &filterConditionJson = nInterfaceJason["FilterCondition"];
        for(Json::UInt i = 0; i <filterConditionJson.size(); i++)
        {
            nInterface.m_nAllowFilterConditions.insert(filterConditionJson[i].asString());
        }
        Json::Value &resourceWord = nInterfaceJason["resouceWord"];
        for( Json::UInt  i = 0; i < resourceWord.size(); i++)
        {
            nInterface.m_nResourceWords.insert(resourceWord[i].asString());
        }
        __ULOG_TRACE(__ULOG_FMT("RESTRequestHelper", "init REST nInterface uri = %s"),nInterface.m_nResource.c_str());
        m_nInterFaceMap.insert(std::make_pair(nInterface.m_nResource, nInterface));
    }
    return true;
}

uint32 RESTRequestHelper::RESTHttpRequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext)
{
    RESTInterFaceMap::iterator itr = m_nInterFaceMap.find(nHttpContext.m_strUri);
    if( itr == m_nInterFaceMap.end() )
    {
        __ULOG_ERROR(__ULOG_FMT("RESTRequestHelper", " find rest resource failed ! resource %s"),nHttpContext.m_strUri.c_str() );
        return Utils::HttpContext::ERR_HTTP_SERVER_ERROR;
    }

    RESTInterface &nInterface = itr->second;
    if(nHttpContext.m_strMethod == Utils::HttpContext::HTTP_METHOD_GET_NAME)
    {
        return RESTGETRequest(nHttpContext, nWebContext, nInterface);
    }
    else if( nHttpContext.m_strMethod == Utils::HttpContext::HTTP_METHOD_POST_NAME )
    {
        return RESTPOSTRequest(nHttpContext, nWebContext, nInterface);
    }
    else if( nHttpContext.m_strMethod == Utils::HttpContext::HTTP_METHOD_PUT_NAME )
    {
        return RESTPUTRequest(nHttpContext, nWebContext, nInterface);
    }
    else if( nHttpContext.m_strMethod == Utils::HttpContext::HTTP_METHOD_PATCH_NAME )
    {
        return RESTPATCHRequest(nHttpContext, nWebContext, nInterface);
    }
    else if( nHttpContext.m_strMethod == Utils::HttpContext::HTPP_METHOD_DELETE_NAME )
    {
        return RESTDELETERequest(nHttpContext, nWebContext, nInterface);
    }
    return 0;
}

uint32 RESTRequestHelper::RESTGETRequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext, RESTInterface &nInterface)
{
    int32 nErrorCode = Common::ERR_SUCCESS;
	std::string strExportMode= nHttpContext.m_nParameters.GetValue("exportMode", "");
    bool onlyCount = nHttpContext.m_nParameters.GetValue("onlyCount",false);
    std::string strCountWord = nHttpContext.m_nParameters.GetValue("conditionWord","");
	std::string selectWord;
    do
    {
        OnceConnection onceConnection(m_nDBPool.get());
        if(onceConnection.IsDisabled())
        {
            __ULOG_ERROR(__ULOG_FMT("RESTRequestHelper", "Get sql connection failed"));
            nErrorCode = Common::ERR_SERVICE_BUSY;
            break;
        }
        Storage::SqlConnection &connection = onceConnection.GetConnectionRef();
        int start = InitFilterParameter("start", nHttpContext.m_nParameters, nInterface.m_nAllowFilterConditions, m_nFilterDefault["start"].asInt());
        int limit = InitFilterParameter("limit", nHttpContext.m_nParameters, nInterface.m_nAllowFilterConditions, m_nFilterDefault["limit"].asInt());
        std::string sort = InitFilterParameter("sort", nHttpContext.m_nParameters, nInterface.m_nAllowFilterConditions, m_nFilterDefault["sort"].asString());
        std::string dir = InitFilterParameter("dir", nHttpContext.m_nParameters, nInterface.m_nAllowFilterConditions, m_nFilterDefault["dir"].asString());
        std::string strOrderBy   = Utils::String::Format("ORDER BY `%s` %s", sort.c_str(), dir.c_str());
        std::string condition = InitConditionWord(nHttpContext.m_nParameters, nInterface.m_nAllowConditionWords);
		if (!strCountWord.empty())
		{
			selectWord = onlyCount ? Utils::String::Format("count(*), %s", strCountWord.c_str()): "*" ;
		}else
		{
			selectWord = onlyCount ? "count(*)": "*" ;
		}
        std::string strgroupBy = nHttpContext.m_nParameters.GetValue("groupBy", "");
        if(!strgroupBy.empty())
        {
            Utils::String::AppendFormat(condition, "group by %s",strgroupBy.c_str());
        }

        if(!connection.QueryAsJson(nInterface.m_nTable, selectWord, condition, strOrderBy, start, limit, "time", "totalCount", "rows", nWebContext.m_nJsonResponse))
        {	
			if (connection.QueryCount(nInterface.m_nTable,condition) < 0)
            {
                __ULOG_ERROR(__ULOG_FMT_ERR("RESTRequestHelper", "Query %s  table  failed"),
                             nInterface.m_nTable.c_str(), (uint32)connection.GetErrorCode(), connection.GetErrorMsg());
                nErrorCode = Common::ERR_INTERNAL_ERROR;
            }
        }

    }while (false);

	nWebContext.m_nJsonResponse["serverVersion"] = Utils::String::Format("%s/Build %s", Common::Module::LIVE_SERVER_VERSION, Common::Module::BUILD_TIME);
	if (strExportMode.empty())
	{
		nWebContext.m_nJsonResponse["errorCode"] = nErrorCode;
		return Utils::HttpContext::ERR_HTTP_SUCCESS;
	}
	bool bExported = true;
	//bExported = WebServer::Instance()._ExportResultList(nWebContext.m_nJsonResponse["rows"], nHttpContext, nWebContext);
	uint32 nHttpCode = bExported ? Utils::HttpContext::ERR_HTTP_SUCCESS : Utils::HttpContext::ERR_HTTP_SERVER_ERROR;
	return nHttpCode;
}

uint32 RESTRequestHelper::RESTPOSTRequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext, RESTInterface &nInterface)
{
    int32 nErrorCode = Common::ERR_SUCCESS;
    Json::Value &results = nWebContext.m_nJsonResponse["results"];
    results = Json::Value(Json::arrayValue);
    do
    {
        OnceConnection onceConnection(m_nDBPool.get());
        if(onceConnection.IsDisabled())
        {
            __ULOG_ERROR(__ULOG_FMT("RESTRequestHelper", "Get sql connection failed"));
            nErrorCode = Common::ERR_SERVICE_BUSY;
            break;
        }
        Storage::SqlConnection &connection = onceConnection.GetConnectionRef();
        const Json::Value &items = nWebContext.m_nJsonRequest["items"];
        for( Json::UInt i =0; i< items.size(); i++)
        {
            const Json::Value &iterm = items[i];
            int64 id;
            Utils::StringMap wordsMap;
            InitResourceWords(iterm, nInterface.m_nResourceWords, wordsMap);
            if( !connection.Insert(nInterface.m_nTable, wordsMap, id) )
            {
                __ULOG_ERROR(__ULOG_FMT_ERR("RESTRequestHelper", "Insert to <%s> failed"),
                             nInterface.m_nTable.c_str(),(uint32)connection.GetErrorCode(), connection.GetErrorMsg());
                nErrorCode = Common::ERR_INTERNAL_ERROR;
                break;
            }
            results[i]["id"] = (Json::UInt)id;
        }

    }while (false);

    nWebContext.m_nJsonResponse["errorCode"] = nErrorCode;
    return Utils::HttpContext::ERR_HTTP_SUCCESS;
}

uint32 RESTRequestHelper::RESTPUTRequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext, RESTInterface &nInterface)
{
    int32 nErrorCode = Common::ERR_SUCCESS;
    //Json::Value &results = nWebContext.m_nJsonResponse["results"];
    //results = Json::Value(Json::arrayValue);
    do
    {
        OnceConnection onceConnection(m_nDBPool.get());
        if(onceConnection.IsDisabled())
        {
            __ULOG_ERROR(__ULOG_FMT("RESTRequestHelper", "Get sql connection failed"));
            nErrorCode = Common::ERR_SERVICE_BUSY;
            break;
        }
        Storage::SqlConnection &connection = onceConnection.GetConnectionRef();
        const Json::Value &items = nWebContext.m_nJsonRequest["items"];
        for( Json::UInt i =0; i< items.size(); i++)
        {
            const Json::Value &item = items[i];
            Utils::StringMap wordsMap;
            InitResourceWords(item, nInterface.m_nResourceWords, wordsMap);
            std::string condition = Utils::String::Format("WHERE id = "_I64FMT_ , item["id"].asInt64());
            if( connection.Update(nInterface.m_nTable, wordsMap, condition) < 0 )
            {
                __ULOG_ERROR(__ULOG_FMT_ERR("RESTRequestHelper", "Update to <%s> failed"),
                             nInterface.m_nTable.c_str(),(uint32)connection.GetErrorCode(), connection.GetErrorMsg());
                nErrorCode = Common::ERR_INTERNAL_ERROR;
                break;
            }
        }

    }while (false);

    nWebContext.m_nJsonResponse["errorCode"] = nErrorCode;
    return Utils::HttpContext::ERR_HTTP_SUCCESS;
}

uint32 RESTRequestHelper::RESTPATCHRequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext, RESTInterface &nInterface)
{
    //Todo
    return 0;
}

uint32 RESTRequestHelper::RESTDELETERequest(Utils::HttpContext &nHttpContext, WebContext &nWebContext, RESTInterface &nInterface)
{
    int32 nErrorCode = Common::ERR_SUCCESS;
    //Json::Value &results = nWebContext.m_nJsonResponse["results"];
    //results = Json::Value(Json::arrayValue);
    do
    {
        OnceConnection onceConnection(m_nDBPool.get());
        if(onceConnection.IsDisabled())
        {
            __ULOG_ERROR(__ULOG_FMT("RESTRequestHelper", "Get sql connection failed"));
            nErrorCode = Common::ERR_SERVICE_BUSY;
            break;
        }
        Storage::SqlConnection &connection = onceConnection.GetConnectionRef();
        const Json::Value &items = nWebContext.m_nJsonRequest["items"];
        for( Json::UInt i =0; i< items.size(); i++)
        {
            const Json::Value &item = items[i];
            std::string strSql = Utils::String::Format("DELETE FROM `%s` WHERE `id` = "_I64FMT_, nInterface.m_nTable.c_str(), item["id"].asInt64());
            if( connection.Execute(strSql) < 0 )
            {
                __ULOG_ERROR(__ULOG_FMT_ERR("RESTRequestHelper", "Delete  <%s> failed, sql(%s)"),
                             nInterface.m_nTable.c_str(), strSql.c_str(), (uint32)connection.GetErrorCode(), connection.GetErrorMsg());
                nErrorCode = Common::ERR_INTERNAL_ERROR;
                break;
            }
        }

    }while (false);

    nWebContext.m_nJsonResponse["errorCode"] = nErrorCode;
    return Utils::HttpContext::ERR_HTTP_SUCCESS;
}

RESTRequestHelper::RESTRequestHelper(const boost::shared_ptr<DatabaseConnectionPool> &DBPool) : m_nDBPool(DBPool)
{

}

int RESTRequestHelper::InitFilterParameter(std::string filterWord, Utils::Parameter &httpParameter,
                                           Utils::StringSet &allowWords, int defaultValue)
{
    int filter = defaultValue;
    if( httpParameter.IsValueExist(filterWord) && (allowWords.find(filterWord) != allowWords.end()))
    {
        return httpParameter.GetValue(filterWord, filter);
    }
    return filter;
}

std::string RESTRequestHelper::InitConditionWord(Utils::Parameter &httpParameter, Json::Value &allowWords)
{
    std::string conditionSql;
    for( Json::Value::UInt i = 0; i < allowWords.size(); i++ )
    {
        Json::Value &condition = allowWords[i];
        std::string parameter = condition["parameter"].asString();
        std::string conditionWord = condition["word"].asString();
        std::string conditionOperator = condition["operator"].asString();
        std::string webCondition = httpParameter.GetValue(parameter, "");
        if( httpParameter.IsValueExist(parameter) && !webCondition.empty() )
        {
            if(conditionSql.empty())
            {
                conditionSql = Utils::String::Format("WHERE `%s` %s \'%s\'", conditionWord.c_str(), conditionOperator.c_str(), webCondition.c_str());
            }
            else
            {
                Utils::String::AppendFormat(conditionSql," AND `%s` %s \'%s\'", conditionWord.c_str(), conditionOperator.c_str(), webCondition.c_str());
            }
        }

    }
    return conditionSql;
}

std::string RESTRequestHelper::InitFilterParameter(std::string filterWord, Utils::Parameter &httpParameter,
                                                    Utils::StringSet &allowWords, std::string defaultValue)
{
    std::string filter = defaultValue;
    if( httpParameter.IsValueExist(filterWord) && (allowWords.find(filterWord) != allowWords.end()))
    {
        return httpParameter.GetValue(filterWord, filter);
    }
    return filter;
}

void RESTRequestHelper::InitResourceWords(const Json::Value &parameter, Utils::StringSet &allowWords,
                                          Utils::StringMap &wordsMap)
{
    for (Utils::StringSet::iterator itr = allowWords.begin(); itr != allowWords.end() ; itr++)
    {
        if(parameter.isMember(*itr))
        {
            wordsMap.insert(std::make_pair(*itr, parameter[*itr].asString()));
        }
    }
}


