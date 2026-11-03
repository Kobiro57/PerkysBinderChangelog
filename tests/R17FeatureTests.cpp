#include "../include/GpsManager.h"
#include "../include/GpsRadarOwnership.h"
#include "../include/PersistentPaths.h"
#include <cstdio>
#include <filesystem>
#include <windows.h>
namespace {
int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("[FAIL] R17: %s at %d\n",#x,__LINE__);}}while(0)
void NavigationAtSavedLocationKeepsMarkerUntilDepartureAndReturn(){
    auto dir=std::filesystem::path("build/r17-gps-tests")/std::to_string(GetTickCount64());std::filesystem::create_directories(dir);
    keybinder::GpsPosition position{100,200,30,0};std::vector<std::string> messages;
    keybinder::GpsManager gps;gps.Initialize((dir/"gps.xml").string(),[&](const auto& s){messages.push_back(s);},[&](auto& p){p=position;return true;});
    CHECK(gps.Set("Testziel"));CHECK(gps.Navigate(gps.Entries()[0].id));auto baseline=messages.size();
    for(int i=0;i<10;++i)gps.Update();CHECK(gps.ActiveDestination()!=nullptr);CHECK(messages.size()==baseline);
    position.x+=2;gps.Update();CHECK(gps.ActiveDestination()!=nullptr);
    position.x+=3;gps.Update();CHECK(gps.ActiveDestination()!=nullptr);
    position.x=100;gps.Update();CHECK(gps.ActiveDestination()==nullptr);CHECK(messages.size()==baseline+1);
    for(int i=0;i<5;++i)gps.Update();CHECK(messages.size()==baseline+1);
    position.x+=100;CHECK(gps.Navigate(gps.Entries()[0].id));position.x=100;gps.Update();CHECK(gps.ActiveDestination()==nullptr);
    CHECK(messages.size()==baseline+3); // Navigation started + exactly one arrival.
    CHECK(keybinder::PersistentPaths::BrowserDir()==keybinder::PersistentPaths::RootDir()+"\\Browser");
}
void StaleRadarHandlesNeverOwnRecycledServerSlots(){
    keybinder::GpsPosition target{100,200,30,0};
    keybinder::GpsRadarTraceSnapshot trace{true,19,4,{100,200,30-0.9f,0}};
    const int handle=(19<<16)|12;
    CHECK(keybinder::OwnsGpsRadarTrace(handle,target,trace));
    CHECK(!keybinder::OwnsGpsRadarTrace(-1,target,trace));
    CHECK(!keybinder::OwnsGpsRadarTrace((19<<16)|175,target,trace));
    trace.generation=20;CHECK(!keybinder::OwnsGpsRadarTrace(handle,target,trace));
    trace.generation=19;trace.inUse=false;CHECK(!keybinder::OwnsGpsRadarTrace(handle,target,trace));
    trace.inUse=true;trace.type=1;CHECK(!keybinder::OwnsGpsRadarTrace(handle,target,trace));
    trace.type=4;trace.position.x+=1;CHECK(!keybinder::OwnsGpsRadarTrace(handle,target,trace));
    trace.position.x=100;trace.position.z+=1;CHECK(!keybinder::OwnsGpsRadarTrace(handle,target,trace));
    trace.position.z=30-0.9f;CHECK(keybinder::OwnsGpsRadarTrace(handle,target,trace));
}
}
int RunR17FeatureTests(){checks=failures=0;NavigationAtSavedLocationKeepsMarkerUntilDepartureAndReturn();StaleRadarHandlesNeverOwnRecycledServerSlots();std::printf("R17FeatureTests: %d checks, %d failures\n",checks,failures);return failures;}
