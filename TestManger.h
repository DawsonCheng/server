

#ifndef LIVESERVER_TESTMANGER_H
#define LIVESERVER_TESTMANGER_H

#include <Core/Common/ModuleService.h>
#include <Core/Utils/Platform.h>
#include "Database.h"


enum TestStatus
{
    ALL,
    READY,
    STARTING,
    RUNNING,
    STOPPING,
    STOPPED
};

class TestManger : public ModuleService ,public Utils::Singleton<TestManger>
{
    friend class Utils::Singleton<TestManger>;
public:

    TestManger();
    virtual ~TestManger();

    bool Initialize();
    void Exit();


private:


};


#endif //LIVESERVER_TESTMANGER_H
