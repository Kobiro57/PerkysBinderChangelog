#define KEYBINDER_TESTING
#define NLBINDER_NATIVE_GPS_FIXTURE
#include "../src/GpsGameAdapter.cpp"
#include <cstdio>
#include <type_traits>

namespace {
int checks=0,failures=0,sets=0,radarCreates=0,radarClears=0,markerDeletes=0;
std::aligned_storage_t<sizeof(C3dMarker),alignof(C3dMarker)> markerStorage[32]{};
std::aligned_storage_t<sizeof(tRadarTrace),alignof(tRadarTrace)> radarStorage[175]{};
CVector lastCheckpoint;
unsigned short lastType=0;float lastRadius=0;unsigned char lastRed=0,lastGreen=0,lastBlue=0;
void Verify(bool result,const char* label){++checks;if(!result){++failures;std::printf("[FAIL] Native GPS: %s\n",label);}}
tRadarTrace& Trace(int handle){return CRadar::ms_RadarTrace[static_cast<unsigned int>(handle)&0xffff];}
}
unsigned int MAX_NUM_3DMARKERS=32;
C3dMarker* C3dMarkers::m_aMarkerArray=reinterpret_cast<C3dMarker*>(markerStorage);
tRadarTrace* CRadar::ms_RadarTrace=reinterpret_cast<tRadarTrace*>(radarStorage);
void keybinder::Logger::Info(const char*,...){}
void C3dMarker::DeleteMarkerObject(){++markerDeletes;m_pAtomic=nullptr;m_nIdentifier=0;m_nType=MARKER3D_NA;m_bIsUsed=false;m_bMustBeRenderedThisFrame=false;}
void C3dMarkers::PlaceMarkerSet(unsigned int id,unsigned short type,CVector& position,float radius,unsigned char r,unsigned char g,unsigned char b,unsigned char,unsigned short,float,short){
    ++sets;lastCheckpoint=position;lastType=type;lastRadius=radius;lastRed=r;lastGreen=g;lastBlue=b;
    auto& marker=m_aMarkerArray[0];marker.m_nIdentifier=id;marker.m_nType=type;marker.m_pAtomic=reinterpret_cast<RpAtomic*>(1);marker.m_bIsUsed=true;marker.m_bMustBeRenderedThisFrame=false;
}
float CWorld::FindGroundZFor3DCoord(float,float,float,bool* found,CEntity**){*found=true;return 18.0f;}
int CRadar::SetCoordBlip(eBlipType type,CVector position,unsigned int,eBlipDisplay display,char*){
    for(int i=0;i<175;++i){auto& t=ms_RadarTrace[i];if(t.m_bInUse)continue;++radarCreates;t.m_bInUse=true;++t.m_nCounter;t.m_nBlipType=type;t.m_vecPos=position;t.m_nBlipDisplay=display;return (t.m_nCounter<<16)|i;}return -1;
}
void CRadar::ClearBlip(int h){++radarClears;Trace(h).m_bInUse=false;}
void CRadar::ChangeBlipColour(int h,unsigned int color){Trace(h).m_nColour=color;}
void CRadar::ChangeBlipScale(int h,int scale){Trace(h).m_nBlipSize=scale;}
void CRadar::ChangeBlipBrightness(int h,int bright){Trace(h).m_bBright=bright!=0;}
void CRadar::ChangeBlipDisplay(int h,eBlipDisplay display){Trace(h).m_nBlipDisplay=display;}
void CRadar::SetCoordBlipAppearance(int h,unsigned char appearance){Trace(h).m_nCoordBlipAppearance=appearance;}
void CRadar::SetBlipAlwaysDisplayInZoom(int,unsigned char){}

int main(){
    keybinder::GpsEntry first{1,"Ziel",{100,200,20,0}};
    keybinder::RenderGpsDestination(&first);
    Verify(sets==1,"normal native checkpoint created on first call");
    Verify(C3dMarkers::m_aMarkerArray[0].m_bMustBeRenderedThisFrame,"checkpoint explicitly submitted after GTA's update phase");
    Verify(lastType==MARKER3D_CYLINDER&&lastRadius==3,"cylinder checkpoint type and radius");
    Verify(lastRed==255&&lastGreen==0&&lastBlue==0,"red checkpoint");
    Verify(lastCheckpoint.x==100&&lastCheckpoint.y==200&&std::fabs(lastCheckpoint.z-18.1f)<.001f,"ground placement");
    Verify(radarCreates==1&&CRadar::ms_RadarTrace[0].m_nBlipDisplay==BLIP_DISPLAY_BOTH,"minimap checkpoint created");
    Verify(!CRadar::ms_RadarTrace[0].m_bShortRange&&CRadar::ms_RadarTrace[0].m_nBlipSize==3,"visible long-range radar marker");
    keybinder::RenderGpsDestination(&first);Verify(sets==1&&radarCreates==1,"same frame does not duplicate checkpoint");
    auto& native=C3dMarkers::m_aMarkerArray[0];native.m_bIsUsed=false;native.m_bMustBeRenderedThisFrame=false;
    keybinder::RenderGpsDestination(&first);Verify(sets==2,"next rendered frame refreshes checkpoint without Process hook");
    Verify(native.m_bMustBeRenderedThisFrame,"next frame remains visible even when Process is replaced");
    // Recycle the radar slot exactly as a server map-icon update can do.
    auto& external=CRadar::ms_RadarTrace[0];++external.m_nCounter;external.m_vecPos.x=999;external.m_nColour=BLIP_COLOUR_BLUE;
    keybinder::RenderGpsDestination(&first);Verify(radarCreates==2&&radarClears==0,"recycled server slot preserved");
    Verify(external.m_vecPos.x==999&&external.m_nColour==BLIP_COLOUR_BLUE,"foreign position and colour unchanged");
    keybinder::GpsEntry next{2,"Zweites Ziel",{300,400,20,0}};keybinder::RenderGpsDestination(&next);
    Verify(sets==3&&lastCheckpoint.x==300&&lastCheckpoint.y==400,"new destination updates checkpoint immediately");
    keybinder::ClearGpsDestinationMarker();
    Verify(markerDeletes==1,"owned world checkpoint removed");
    Verify(external.m_bInUse&&external.m_vecPos.x==999,"server marker survives navigation stop");
    const auto cleared=radarClears;keybinder::ClearGpsDestinationMarker();Verify(radarClears==cleared,"repeated clear is harmless");
    std::printf("GpsNativeAdapterTests: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
