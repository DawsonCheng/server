

#ifndef LIVESERVER_LIVESERVER_H
#define LIVESERVER_LIVESERVER_H

#include <Core/Common/Service.h>
#include <Core/Utils/Utils.h>

class LiveServer : public Common::Service
{
public:
    LiveServer(Common::Arguments &arg);
    virtual  ~LiveServer();

    void Init();
    virtual int RunLoop();

private:
    Utils::SharedMemory m_nShareStatus;


};

#endif //LIVESERVER_H
