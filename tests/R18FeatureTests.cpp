#include "../include/GpsManager.h"
#include "../include/BrowserSetupProtocol.h"
#include "../include/ForumAfkSession.h"
#include <filesystem>
#include <cstdio>
#include <windows.h>
namespace {
int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::printf("[FAIL] R18: %s at %d\n",#x,__LINE__);}}while(0)
void CheckpointIsCreatedByNavigationAndRenderedFrames(){
    auto root=std::filesystem::path("build/r18-gps-tests")/std::to_string(GetTickCount64());std::filesystem::create_directories(root);
    keybinder::GpsPosition position{100,200,30,0};bool available=true,visible=false;int created=0,removed=0;unsigned int active=0;
    std::vector<std::string> messages;
    keybinder::GpsManager gps;
    gps.Initialize((root/"gps.xml").string(),[&](const auto& s){messages.push_back(s);},[&](auto& p){p=position;return available;},
        [&](const keybinder::GpsEntry* target){
            if(target){visible=true;active=target->id;++created;CHECK(target->position.x==100&&target->position.y==200);}
            else {if(visible)++removed;visible=false;active=0;}
        });
    CHECK(gps.Set("Normaler Checkpoint"));const auto id=gps.Entries()[0].id;
    position.x+=100;
    CHECK(gps.Navigate(id));
    CHECK(visible&&active==id&&created==1); // No game process/update tick has occurred.
    const auto baseline=messages.size();
    gps.Render();CHECK(visible&&created>=2);
    available=false;gps.Render();CHECK(!visible&&gps.ActiveDestination()!=nullptr);
    available=true;gps.Render();CHECK(visible&&active==id);
    position.interior=1;gps.Render();CHECK(!visible&&gps.ActiveDestination()!=nullptr);
    position.interior=0;position.x=100;gps.Render();
    CHECK(!visible&&gps.ActiveDestination()==nullptr);CHECK(messages.size()==baseline+1);
    for(int i=0;i<5;++i)gps.Render();CHECK(messages.size()==baseline+1);
    CHECK(gps.Navigate(id));gps.Render();CHECK(visible); // Starting at the goal keeps the checkpoint.
    position.x+=5;gps.Render();position.x=100;gps.Render();CHECK(!visible);
    CHECK(removed>=3);
    CHECK(sizeof(keybinder::browser_setup::SetupProgress)==284);
}
void ForumCommandsFollowVisibilityWithoutRepeatingEveryFrame(){
    keybinder::ForumAfkSession session;std::vector<std::string> commands;
    session.SetSender([&](const std::string& command){commands.push_back(command);return true;});
    session.SetVisible(false);CHECK(commands.empty());
    session.SetVisible(true);CHECK(commands==std::vector<std::string>{"/afk"});
    for(int i=0;i<10;++i)session.SetVisible(true);CHECK(commands.size()==1);
    session.SetVisible(false);CHECK(commands==std::vector<std::string>({"/afk","/back"}));
    session.SetVisible(false);CHECK(commands.size()==2);
    session.SetVisible(true);session.SetVisible(false);CHECK(commands.size()==4);
    keybinder::ForumAfkSession unavailable;std::vector<std::string> failed;
    unavailable.SetSender([&](const std::string& command){failed.push_back(command);return false;});
    unavailable.SetVisible(true);unavailable.SetVisible(false);CHECK(failed==std::vector<std::string>{"/afk"});
    keybinder::ForumAfkSession throwing;throwing.SetSender([](const std::string&)->bool{throw 1;});
    throwing.SetVisible(true);throwing.SetVisible(false);CHECK(true);
}
}
int RunR18FeatureTests(){checks=failures=0;CheckpointIsCreatedByNavigationAndRenderedFrames();ForumCommandsFollowVisibilityWithoutRepeatingEveryFrame();std::printf("R18FeatureTests: %d checks, %d failures\n",checks,failures);return failures;}
