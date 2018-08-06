//
//
#include "LiveServer.h"
#include "Configure.h"
#include "WebServer.h"
#include "TestManger.h"
#include <Core/Utils/Log.h>


int main( int argc, char *argv[] )
{

    Common::Arguments nArgs(argc, argv);
    Utils::Log::InitInstance();
    Configure::InitInstance();
    Storage::SqlDriverManager::InitInstance();
    LiveServer::InitInstance<LiveServer, Common::Arguments &>(nArgs);
    WebServer::InitInstance();
    TestManger::InitInstance();
    std::srand((unsigned int)Utils::GetHighResolutionTime());
    int nRetValue = 0;
    do
    {
        Configure &nConfig = Configure::Instance();
        Utils::Log &nGlobalLog = Utils::Log::Instance();


        // initialize network
        if( !Utils::Network::Initialize() )
        {
            __ULOG_STDERR(__ULOG_FMT_ERR("Main", "Initialize network failed"), __UERR_CODE, __UERR_STR);
            nRetValue = 1;
            break;
        }

        // initialize xml parser
        if( !Utils::Xml::Initialize() )
        {
            __ULOG_STDERR(__ULOG_FMT("Main", "Initialize xml parser failed"));
            nRetValue = 1;
            break;
        }

        if( !nConfig.Load(nArgs.m_strConfigFile, NULL) )
        {
            __ULOG_STDERR(__ULOG_FMT("Main","Load configure(%s) failed"), nArgs.m_strConfigFile.c_str());
            nRetValue = 1;
            break;
        }

        Utils::SetWorkingDirectory(nConfig.m_strPath);

        //nConfig.m_nLog.m_nLog.m_nSizeCapacity = 5 * 1024;
        nGlobalLog.SetLevel(nConfig.m_nLog.m_nLevel);
        nGlobalLog.SetTimeCapacity(nConfig.m_nLog.m_nTimeCapacity);
        nGlobalLog.SetSizeCapacity(nConfig.m_nLog.m_nSizeCapacity);
        nGlobalLog.SetTimeMilliSeconds(nConfig.m_nLog.m_bTimeMilliSeconds);

        if( !nGlobalLog.Open(nConfig.m_nLog.m_nType, nConfig.m_nLog.m_strFile, nConfig.m_nLog.m_bAppend) )
        {
            __ULOG_STDERR(__ULOG_FMT_ERR("Main","Open log(%s) failed"), nConfig.m_nLog.m_strFile.c_str(), __UERR_CODE, __UERR_STR);
            nRetValue = 1;
            break;
        }
        if(!WebServer::Instance().Initialize())
        {
            break;
        }
        if(!TestManger::Instance().Initialize())
        {
            break;
        }

        nConfig.TraceSummary();
        LiveServer::Instance().RunLoop();

        WebServer::Instance().Exit();
    }while (false);
    WebServer::ExitInstance();
	TestManger::ExitInstance();
    LiveServer::ExitInstance();
    Storage::SqlDriverManager::ExitInstance();
    Utils::Log::ExitInstance();
    Configure::ExitInstance();

    return nRetValue;
}

LiveServer::LiveServer(Common::Arguments &arg) :Common::Service(arg , true)
{

}

LiveServer::~LiveServer()
{

}
void LiveServer::Init()
{

}

int LiveServer::RunLoop()
{
    std::string strShareName = Utils::String::Format("%s/%s",
                                                     Configure::Instance().m_strPath.c_str(),
                                                     Configure::Instance().m_strShareName.c_str());
    if( !m_nShareStatus.Map(strShareName, sizeof(Common::ServerShareStatus), false) )
    {
        // ignore error
        __ULOG_ERROR(__ULOG_FMT_ERR("Service", "Open share memory(%s) failed"), strShareName.c_str(), __UERR_CODE, __UERR_STR);
    }

    Common::AgentShareStatus *lpStatus = static_cast<Common::AgentShareStatus *>(m_nShareStatus.GetAddress());
    if( NULL != lpStatus )
    {
        // initialize status
        memset(lpStatus, 0, sizeof(Common::AgentShareStatus));
        lpStatus->m_nStartupTime = Utils::GetMicroTime();
    }
    while (m_bEnabled)
    {
        if( m_nNotifier.ServiceWait(100) )
        {
            __ULOG_INFO(__ULOG_FMT("Main","Notifier triggered, exit ..."));

            m_bEnabled = false;
            break;
        }

        if( NULL != lpStatus )
        {
            lpStatus->m_nHeartbeatCount ++;
            lpStatus->m_nLastHeartbeatTime = Utils::GetMicroTime();
        }

    }
    return true;
}


