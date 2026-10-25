#include "raylib.h"
#include "rlgl.h"
#include "game_logic.h"
#include "game_layout.h"
#include "app_settings.h"
#include "menu_state.h"
#include "game_timing.h"
#include "game_presentation.h"
#include "game_audio.h"
#include "visual_feedback.h"
#include "visual_effects.h"
#include "player_upgrades.h"
#include "upgrade_layout.h"
#include "chapter_one.h"
#include "rootwarden.h"
#include "chapter_hud.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <charconv>
#include <random>
#include <string_view>

namespace {
constexpr float kPlayerRadius=threadlight::layout::kPlayerRadius;
using threadlight::presentation::V;
using threadlight::presentation::Thread;
using threadlight::presentation::Foe;
using threadlight::presentation::Spark;
using threadlight::presentation::DeathEffect;
using Particle=threadlight::presentation::Particle<Color>;
float len(V a){return std::sqrt(a.x*a.x+a.y*a.y);} float dist(V a,V b){return len(a-b);} V norm(V a){float l=len(a);return l>0.001f?a*(1/l):V{};} Vector2 cv(V a){return {a.x,a.y};}
V catmullRom(V p0,V p1,V p2,V p3,float t){const float t2=t*t,t3=t2*t;return (p1*2.f+(p2-p0)*t+(p0*2.f-p1*5.f+p2*4.f-p3)*t2+(p0*-1.f+p1*3.f-p2*3.f+p3)*t3)*.5f;}
Color C(unsigned char r,unsigned char g,unsigned char b,unsigned char a=255){return {r,g,b,a};}
}
int main(int argc, char** argv){
 bool m1Telemetry=false;
 int bossFixture=-1; // Explicit development-only CLI fixture; never a campaign checkpoint.
 std::uint32_t spawnSeed=7382;
 for(int i=1;i<argc;i++){
  const std::string_view arg=argv[i];
  if((arg=="--m1-telemetry"||arg=="--m2-telemetry"||arg=="--m3-telemetry"))m1Telemetry=true;
  else if((arg=="--m1-seed"||arg=="--m2-seed"||arg=="--m3-seed")&&i+1<argc){
   const std::string_view text=argv[++i];std::uint32_t parsed=0;
   const auto result=std::from_chars(text.data(),text.data()+text.size(),parsed);
   if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())return 2;
   spawnSeed=parsed;
  }else if(arg=="--m3-boss-fixture"&&i+1<argc){
   const std::string_view build=argv[++i];
   constexpr std::string_view builds[]={"zero","offense","defense","mobility","mixed"};
   bossFixture=-1;for(int j=0;j<5;++j)if(build==builds[j])bossFixture=j;
   if(bossFixture<0)return 2;
  }else return 2;
 }
 SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);
 InitWindow(threadlight::layout::kInitialScreenWidth,threadlight::layout::kInitialScreenHeight,"THREADLIGHT | A small game of living silk");
 const auto windowCoordinates=threadlight::layout::WindowCoordinates::atStartup(
     GetScreenWidth(),GetScreenHeight(),GetRenderWidth(),GetRenderHeight());
 SetWindowMinSize(windowCoordinates.nativeWidth(960),windowCoordinates.nativeHeight(640));
 int SW=windowCoordinates.width(GetRenderWidth()),SH=windowCoordinates.height(GetRenderHeight());
 auto playArea=threadlight::layout::playBounds(SW,SH);
 SetExitKey(KEY_NULL); SetTargetFPS(60); InitAudioDevice();
 const bool audioReady=IsAudioDeviceReady();
 threadlight::GameAudio sfx;
 if(!sfx.initialize(audioReady))TraceLog(LOG_WARNING,"Audio device unavailable; event sounds are disabled.");
 const auto settingsPath=threadlight::settings::userSettingsPath();
 auto appSettings=threadlight::settings::load(settingsPath);
 sfx.setMasterVolume(appSettings.sfxVolumePercent/100.0f);
 SetTargetFPS(appSettings.frameRateLimit);
 using threadlight::Mode;
 Mode mode=Mode::Title,settingsReturn=Mode::Title; bool exitRequested=false; int mainSelection=0,pauseSelection=0,exitSelection=threadlight::menu::kDefaultExitSelection,settingsTab=0,settingsFocus=0;
 threadlight::settings::PendingEdits settingsEdits;
 namespace foundation=threadlight::foundation;
 namespace battle=threadlight::combat;
 namespace upgrades=threadlight::upgrades;
 namespace skitter=threadlight::skitter;
 namespace upgradeUI=threadlight::upgrade_ui;
 namespace chapter=threadlight::chapter;
 namespace root=threadlight::rootwarden;
 namespace chapterHud=threadlight::chapter_hud;
 foundation::WaveLifecycle lifecycle;
 foundation::RewardLedger rewards;
 chapter::Schedule spawnSchedule;int spawnCursor=0;
 root::State boss;foundation::RewardId bossRewardId=0;
 auto bossArena=[&](){return root::Arena{playArea.left,playArea.right,playArea.top,playArea.bottom};};
 std::uint64_t simulationTick=0;
 battle::PlayerHealth health;
 upgrades::Ranks ranks;
 upgrades::Stats stats=upgrades::derive(ranks);
 upgrades::OfferState offers;offers.rng=spawnSeed^0x9e3779b9u;
 V player{(playArea.left+playArea.right)*.5f,(playArea.top+playArea.bottom)*.5f};
 float dashCd=0,dashCycleDuration=1.85f,dashTime=0,shake=0,pulse=0,damageFeedback=0;
 int wave=0,choice=0;
 const float radius=kPlayerRadius;
 bool dashVisualActive=false,playerWasMoving=false;
 float dashEchoTimer=0,dashEchoFade=0; size_t dashEchoCount=0; std::array<V,6> dashEcho{};
 std::vector<Thread> thread; std::vector<Foe> foes; std::vector<Spark> sparks; std::vector<Particle> particles; std::vector<DeathEffect> deathEffects; std::vector<V> stars;
 thread.reserve(240); foes.reserve(80); sparks.reserve(48); particles.reserve(threadlight::visual::kParticleLimit); deathEffects.reserve(threadlight::visual::kDeathEffectLimit); stars.reserve(110);
 for(int i=0;i<110;i++) stars.push_back({GetRandomValue(0,10000)/10000.0f,GetRandomValue(0,10000)/10000.0f});
 threadlight::timing::FixedStepClock stepClock;
 V previousPlayer=player;
 float previousShake=shake,previousDamageFeedback=damageFeedback,previousDashEchoFade=dashEchoFade;
 std::array<V,6> previousDashEcho=dashEcho;
 size_t previousDashEchoCount=dashEchoCount;
 auto capturePresentation=[&](){
  previousPlayer=player;previousShake=shake;previousDamageFeedback=damageFeedback;previousDashEchoFade=dashEchoFade;
  previousDashEcho=dashEcho;previousDashEchoCount=dashEchoCount;boss.previousP=boss.p;
  for(auto& item:thread)item.capture();
  for(auto& item:foes)item.capture();
  for(auto& item:sparks)item.capture();
  for(auto& item:particles)item.capture();
  for(auto& item:deathEffects)item.capture();
 };
 bool dashPressPending=false,collectionContinuePending=false;
 threadlight::menu::FocusOwner menuFocus;
 float measuredFps=60.0f;
 auto saveSettings=[&](){
  SetTargetFPS(appSettings.frameRateLimit);
  sfx.setMasterVolume(appSettings.sfxVolumePercent/100.0f);
  if(!settingsEdits.flush([&](){return threadlight::settings::save(settingsPath,appSettings);}))
   TraceLog(LOG_WARNING,"Could not save ThreadLight settings to the user config directory.");
 };
 auto burst=[&](V p,Color col,int n,float force){for(int i=0;i<n&&particles.size()<threadlight::visual::kParticleLimit;i++){float a=(float)GetRandomValue(0,6283)/1000.f,s=(float)GetRandomValue(25,(int)force);particles.push_back({p,{std::cos(a)*s,std::sin(a)*s},.65f,.65f,(float)GetRandomValue(2,6),col});}};
 auto addDeathEffect=[&](const Foe& foe){
  if(deathEffects.size()>=threadlight::visual::kDeathEffectLimit)deathEffects.erase(deathEffects.begin());
  const bool heavy=foe.kind==battle::EnemyKind::Brute;
  DeathEffect effect{};effect.p=foe.p;effect.kind=heavy?1:0;
  const int count=heavy?threadlight::visual::kHeavyShardCount:threadlight::visual::kSmallShardCount;
  const float phase=foe.phase*.71f;
  for(int i=0;i<count;i++){const float a=phase+6.2831853f*(float)i/count+(float)GetRandomValue(-20,20)*.01f;const float force=heavy?65.f:160.f;effect.velocity[i]={std::cos(a)*(force+(float)GetRandomValue(-15,15)),std::sin(a)*(force+(float)GetRandomValue(-15,15))};}
  deathEffects.push_back(effect);
 };
 auto traceM1=[&](const char* event,foundation::RewardId id=0,int value=0){
  if(!m1Telemetry)return;
  const auto& c=rewards.counters();
  TraceLog(LOG_INFO,"M1 seed=%u tick=%llu wave=%d phase=%s elapsed=%d event=%s id=%llu value=%d spawned=%llu killed=%llu removed=%llu spark_created=%llu spark_collected=%llu spark_expired=%llu issued=%llu resolved=%llu bank=%lld score=%llu pickups_created=%llu pickups_collected=%llu pickups_expired=%llu",
   spawnSeed,(unsigned long long)simulationTick,wave+1,foundation::phaseName(lifecycle.phase()),lifecycle.elapsedTicks(),event,(unsigned long long)id,value,
   (unsigned long long)c.spawned,(unsigned long long)c.killed,(unsigned long long)c.despawned,
   (unsigned long long)c.sparksCreated,(unsigned long long)c.sparksCollected,(unsigned long long)c.sparksExpired,
   (unsigned long long)c.creditsIssued,(unsigned long long)c.creditsResolved,(long long)rewards.remainder(),(unsigned long long)rewards.score(),
   (unsigned long long)c.pickupsCreated,(unsigned long long)c.pickupsCollected,(unsigned long long)c.pickupsExpired);
 };
 auto traceBoss=[&](const char* event){
  if(!m1Telemetry)return;
  const auto& m=boss.metrics;
  TraceLog(LOG_INFO,"M3 boss event=%s seed=%u tick=%llu stage=%s phase=%d hp=%d duration_ticks=%llu hits=%llu exposure_hits=%llu contact_ticks=%llu cycles=%llu dealt=%d player_damage=%d supports=%d",
   event,spawnSeed,(unsigned long long)simulationTick,root::stageName(boss.stage),boss.phase,boss.health.hp,
   (unsigned long long)m.ticks,(unsigned long long)m.hits,(unsigned long long)m.exposedHits,
   (unsigned long long)m.contactTicks,(unsigned long long)m.cycles,m.silkDamage,m.playerDamage,boss.supportsReleased);
 };
 auto startEncounter=[&](){
  const auto encounter=chapter::encounter(wave);
  lifecycle.start({std::max(1,encounter.seconds*foundation::kTicksPerSecond),encounter.boss});
  spawnSchedule=chapter::schedule(wave,spawnSeed);spawnCursor=0;
  thread.clear();dashTime=0;dashVisualActive=false;dashEchoCount=0;dashEchoFade=0;
  if(encounter.boss){
   const V center{(playArea.left+playArea.right)*.5f,(playArea.top+playArea.bottom)*.5f};
   const V direction=len(center-player)>1?norm(center-player):V{1,0};
   const V start=dist(center,player)>220?center:player+direction*230.f;
   root::start(boss,{start.x,start.y},bossArena());
   bossRewardId=rewards.registerEnemy(root::kCoreValue,250);traceBoss("start");
  }
  traceM1("encounter_start");
 };
 auto reset=[&](){
  player={(playArea.left+playArea.right)*.5f,(playArea.top+playArea.bottom)*.5f};
  health={};playerWasMoving=false;ranks={};
  if(bossFixture==1)ranks.value[upgrades::index(upgrades::Id::Razor)]=3;
  if(bossFixture==2)ranks.value[upgrades::index(upgrades::Id::Heartwood)]=3;
  if(bossFixture==3){ranks.value[upgrades::index(upgrades::Id::Quick)]=2;ranks.value[upgrades::index(upgrades::Id::Rhythm)]=1;}
  if(bossFixture==4){ranks.value[upgrades::index(upgrades::Id::Razor)]=1;ranks.value[upgrades::index(upgrades::Id::Living)]=1;ranks.value[upgrades::index(upgrades::Id::Heartwood)]=1;}
  stats=upgrades::derive(ranks);health.hp=health.maxHp=stats.maxHp;offers={};offers.rng=spawnSeed^0x9e3779b9u;
  dashCd=dashTime=shake=pulse=damageFeedback=0;wave=bossFixture<0?0:6;choice=0;simulationTick=0;
  rewards=foundation::RewardLedger{};boss={};bossRewardId=0;dashCycleDuration=stats.dashCooldownSeconds;dashVisualActive=false;
  dashEchoTimer=dashEchoFade=0;dashEchoCount=0;dashEcho.fill(player);
  thread.clear();foes.clear();sparks.clear();particles.clear();deathEffects.clear();
  stepClock.clear();dashPressPending=collectionContinuePending=false;mode=Mode::Play;startEncounter();
 };
 auto addFoe=[&](battle::EnemyKind kind,float a){
  const auto def=battle::definition(kind);const float r=def.radius;
  const float playWidth=playArea.right-playArea.left,playHeight=playArea.bottom-playArea.top;
  const float padding=r+5.f;
  auto spawn=threadlight::chooseSafeSpawn(a,player.x-playArea.left,player.y-playArea.top,playWidth,playHeight,padding,std::max(padding,playWidth*.53f),std::max(padding,playHeight*.53f),radius+r+70.f);
  V p{spawn.x+playArea.left,spawn.y+playArea.top};
  foes.push_back({p,{},r,{def.hp,def.hp},def.speed,a,0,kind});
  auto& foe=foes.back();foe.rewardId=rewards.registerEnemy(def.sparkValue,def.killScore);
  foe.skitterState.side=(foe.rewardId%2)?1:-1;
  traceM1("spawn",foe.rewardId,def.sparkValue);
  if(m1Telemetry)TraceLog(LOG_INFO,"M3 enemy id=%llu kind=%d hp=%d x=%.1f y=%.1f",(unsigned long long)foe.rewardId,(int)kind,def.hp,p.x,p.y);
 };
 auto releaseSupports=[&](int count){
  for(int i=0;i<count;++i)addFoe(battle::EnemyKind::Hunter,chapter::kPi*.25f+chapter::kPi*(boss.supportsReleased+i)*.5f);
  if(count)traceBoss("supports");
 };
 auto finishEncounter=[&](){
  if(!lifecycle.finishRewards(rewards.pendingCredits()!=0))return;
  traceM1("encounter_complete");
  ++wave;startEncounter();
 };
 auto drawText=[&](const char* s,int x,int y,int size,Color c){DrawText(s,x,y,size,c);};
  auto applyMenuAction=[&](threadlight::menu::Action action){threadlight::menu::apply(mode,settingsReturn,exitRequested,exitSelection,action);};
  auto changeFrameLimit=[&](int value){if(!threadlight::settings::validFrameRateLimit(value)||value==appSettings.frameRateLimit)return;appSettings.frameRateLimit=value;settingsEdits.dirty=true;saveSettings();};
  auto changeShowFps=[&](){appSettings.showFps=!appSettings.showFps;settingsEdits.dirty=true;saveSettings();};
  auto changeSfxVolume=[&](int value,bool persist=true){value=std::clamp(value,0,100);if(value==appSettings.sfxVolumePercent)return;appSettings.sfxVolumePercent=value;settingsEdits.dirty=true;sfx.setMasterVolume(value/100.0f);if(persist)saveSettings();};
  auto contains=[](Rectangle rectangle,Vector2 point){return CheckCollisionPointRec(point,rectangle);};
  auto buttonRect=[&](int index,int count,float centerY,float width=300.f,float height=46.f,float gap=54.f){return Rectangle{(SW-width)*.5f,centerY+(index-(count-1)*.5f)*gap-height*.5f,width,height};};
  auto mainButton=[&](int index){return buttonRect(index,3,SH*.5f+96.f,310.f,46.f,56.f);};
  auto pauseButton=[&](int index){return buttonRect(index,4,SH*.5f+72.f,330.f,44.f,50.f);};

  int loggedWidth=0,loggedHeight=0,loggedRenderWidth=0,loggedRenderHeight=0;
  Vector2 loggedDpi{0.0f,0.0f};
  while(!WindowShouldClose()&&!exitRequested){
   const Mode screenAtFrameStart=mode;
   const double frameSeconds=std::max(0.0,static_cast<double>(GetFrameTime()));
   measuredFps=threadlight::timing::smoothMeasuredFps(measuredFps,static_cast<float>(frameSeconds));
   const int nextSW=windowCoordinates.width(GetRenderWidth()),nextSH=windowCoordinates.height(GetRenderHeight());
   const auto nextPlayArea=threadlight::layout::playBounds(nextSW,nextSH);
   const bool resized=nextSW!=SW||nextSH!=SH;
   if(resized){
    auto remapWorldPoint=[&](V& point){const auto mapped=threadlight::layout::remapPoint({point.x,point.y},playArea,nextPlayArea);point={mapped.x,mapped.y};};
    remapWorldPoint(player);
    for(auto& item:thread)remapWorldPoint(item.p);
    for(auto& foe:foes){
     if(foe.kind==battle::EnemyKind::Skitter){
      foundation::Point p{foe.p.x,foe.p.y},v{foe.vel.x,foe.vel.y};
      skitter::remapBounds(foe.skitterState,p,v,
       {playArea.left+foe.r,playArea.right-foe.r,playArea.top+foe.r,playArea.bottom-foe.r},
       {nextPlayArea.left+foe.r,nextPlayArea.right-foe.r,nextPlayArea.top+foe.r,nextPlayArea.bottom-foe.r});
      foe.p={p.x,p.y};foe.vel={v.x,v.y};
     }else remapWorldPoint(foe.p);
    }
    if(chapter::encounter(wave).boss)root::remap(boss,bossArena(),{nextPlayArea.left,nextPlayArea.right,nextPlayArea.top,nextPlayArea.bottom});
    for(auto& spark:sparks)remapWorldPoint(spark.p);
    for(auto& particle:particles)remapWorldPoint(particle.p);
    for(auto& effect:deathEffects)remapWorldPoint(effect.p);
    for(auto& echo:dashEcho)remapWorldPoint(echo);
   }
   SW=nextSW;SH=nextSH;playArea=nextPlayArea;
   const int renderWidth=GetRenderWidth(),renderHeight=GetRenderHeight();
   const Vector2 dpi=GetWindowScaleDPI();
   if(SW!=loggedWidth||SH!=loggedHeight||renderWidth!=loggedRenderWidth||
      renderHeight!=loggedRenderHeight||std::abs(dpi.x-loggedDpi.x)>.01f||
      std::abs(dpi.y-loggedDpi.y)>.01f){
    TraceLog(LOG_INFO,"ThreadLight logical client %dx%d, framebuffer %dx%d, scale %.2fx%.2f",SW,SH,renderWidth,renderHeight,dpi.x,dpi.y);
    loggedWidth=SW;loggedHeight=SH;loggedRenderWidth=renderWidth;loggedRenderHeight=renderHeight;loggedDpi=dpi;
   }
   player.x=threadlight::layout::clampPlayerX(player.x,playArea);
   player.y=threadlight::layout::clampPlayerY(player.y,playArea);
   if(resized){
    stepClock.clear();dashPressPending=collectionContinuePending=false;capturePresentation();
   }

   const float settingsWidth=std::min(860.f,static_cast<float>(SW-40));
   const float settingsLeft=(SW-settingsWidth)*.5f,settingsTop=SH*.5f-256.f;
   const Rectangle videoTab{settingsLeft+48.f,settingsTop+76.f,165.f,42.f};
   const Rectangle audioTab{settingsLeft+230.f,settingsTop+76.f,165.f,42.f};
   const float fpsChipWidth=(settingsWidth-364.f)/4.f;
   Rectangle fpsChips[4]{};
   for(int i=0;i<4;i++)fpsChips[i]={settingsLeft+292.f+i*(fpsChipWidth+8.f),settingsTop+143.f,fpsChipWidth,38.f};
   const Rectangle showToggle{settingsLeft+settingsWidth-182.f,settingsTop+224.f,132.f,42.f};
   const Rectangle frameRow{settingsLeft+42.f,settingsTop+132.f,settingsWidth-84.f,64.f};
   const Rectangle volumeSlider{settingsLeft+48.f,settingsTop+207.f,settingsWidth-96.f,18.f};
   const Rectangle volumeRow{settingsLeft+42.f,settingsTop+144.f,settingsWidth-84.f,100.f};
   const Rectangle settingsBack{settingsLeft+settingsWidth*.5f-125.f,settingsTop+442.f,250.f,44.f};
   const Vector2 mouse=GetMousePosition();
   const Vector2 mouseDelta=GetMouseDelta();
   const bool menuKey=IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_DOWN)||
       IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_ENTER)||
       IsKeyPressed(KEY_SPACE)||IsKeyPressed(KEY_TAB)||IsKeyPressed(KEY_ESCAPE)||
       IsKeyPressed(KEY_P)||IsKeyPressed(KEY_R)||
       (mode==Mode::Upgrade&&(IsKeyPressed(KEY_ONE)||IsKeyPressed(KEY_TWO)||IsKeyPressed(KEY_THREE)||IsKeyPressed(KEY_A)||IsKeyPressed(KEY_D)||IsKeyPressed(KEY_S)));
   menuFocus.update(mouseDelta.x!=0.f||mouseDelta.y!=0.f,
                    IsMouseButtonPressed(MOUSE_BUTTON_LEFT),menuKey);
   auto hovered=[&](Rectangle rectangle){return menuFocus.mouseOwnsFocus()&&contains(rectangle,mouse);};
   auto sampleVolume=[&](){
    const float position=std::clamp((mouse.x-volumeSlider.x)/std::max(1.f,volumeSlider.width),0.f,1.f);
    changeSfxVolume(threadlight::settings::quantizeSfxVolume(static_cast<int>(std::lround(position*100.f))),false);
   };
   // Capture the release position even outside the slider. Interruptions end
   // the edit before menu navigation can bypass release handling.
   if(settingsEdits.dragging&&IsWindowFocused()&&!menuKey&&
      (IsMouseButtonDown(MOUSE_BUTTON_LEFT)||IsMouseButtonReleased(MOUSE_BUTTON_LEFT)))sampleVolume();
   if(settingsEdits.finishDrag(mode==Mode::Settings&&settingsTab==1,
       IsWindowFocused(),IsMouseButtonDown(MOUSE_BUTTON_LEFT),menuKey))saveSettings();

   if(screenAtFrameStart==Mode::Title){
    if(IsKeyPressed(KEY_ESCAPE))applyMenuAction(threadlight::menu::Action::Escape);
    else if(IsKeyPressed(KEY_UP))mainSelection=threadlight::menu::moveSelection(mainSelection,-1,3);
    else if(IsKeyPressed(KEY_DOWN))mainSelection=threadlight::menu::moveSelection(mainSelection,1,3);
    else if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){
     if(mainSelection==0)reset();
     else if(mainSelection==1)applyMenuAction(threadlight::menu::Action::OpenSettings);
     else applyMenuAction(threadlight::menu::Action::RequestExit);
    }else for(int i=0;i<3;i++)if(hovered(mainButton(i))){
     mainSelection=i;
     if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
      if(i==0)reset();else if(i==1)applyMenuAction(threadlight::menu::Action::OpenSettings);else applyMenuAction(threadlight::menu::Action::RequestExit);
     }
    }
   }else if(screenAtFrameStart==Mode::Pause){
    if(IsKeyPressed(KEY_P))applyMenuAction(threadlight::menu::Action::Resume);
    else if(IsKeyPressed(KEY_ESCAPE))applyMenuAction(threadlight::menu::Action::Escape);
    else if(IsKeyPressed(KEY_R))reset();
    else if(IsKeyPressed(KEY_UP))pauseSelection=threadlight::menu::moveSelection(pauseSelection,-1,4);
    else if(IsKeyPressed(KEY_DOWN))pauseSelection=threadlight::menu::moveSelection(pauseSelection,1,4);
    else if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){
     if(pauseSelection==0)applyMenuAction(threadlight::menu::Action::Resume);
     else if(pauseSelection==1)applyMenuAction(threadlight::menu::Action::OpenSettings);
     else if(pauseSelection==2)reset();
     else applyMenuAction(threadlight::menu::Action::ReturnToMainMenu);
    }else for(int i=0;i<4;i++)if(hovered(pauseButton(i))){
     pauseSelection=i;
     if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
      if(i==0)applyMenuAction(threadlight::menu::Action::Resume);
      else if(i==1)applyMenuAction(threadlight::menu::Action::OpenSettings);
      else if(i==2)reset();
      else applyMenuAction(threadlight::menu::Action::ReturnToMainMenu);
     }
    }
   }else if(screenAtFrameStart==Mode::Settings){
    if(IsKeyPressed(KEY_ESCAPE))applyMenuAction(threadlight::menu::Action::Escape);
    else if(IsKeyPressed(KEY_TAB))threadlight::menu::switchSettingsTab(settingsTab,settingsFocus);
    else {
     if(hovered(videoTab)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){settingsTab=0;settingsFocus=0;}
     else if(hovered(audioTab)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){settingsTab=1;settingsFocus=0;}
     else if(hovered(settingsBack)){settingsFocus=settingsTab==0?2:1;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))applyMenuAction(threadlight::menu::Action::Back);}
     else if(settingsTab==0){
      for(int i=0;i<4;i++)if(hovered(fpsChips[i])){settingsFocus=0;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){constexpr int rates[]={30,60,120,0};changeFrameLimit(rates[i]);}}
      if(hovered(showToggle)){settingsFocus=1;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))changeShowFps();}
     }else if(settingsTab==1&&hovered(volumeRow))settingsFocus=0;
     if(settingsTab==1&&hovered(volumeSlider)&&IsWindowFocused()&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
      settingsFocus=0;
      settingsEdits.dragging=true;
      sampleVolume();
     }
     if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_DOWN)){
      const int direction=IsKeyPressed(KEY_DOWN)?1:-1;
      settingsFocus=threadlight::menu::moveSelection(settingsFocus,direction,threadlight::menu::settingsFocusCount(settingsTab));
     }else if(IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_RIGHT)){
      const int direction=IsKeyPressed(KEY_RIGHT)?1:-1;
      if(settingsTab==0&&settingsFocus==0)changeFrameLimit(threadlight::settings::nextFrameRateLimit(appSettings.frameRateLimit,direction));
      else if(settingsTab==0&&settingsFocus==1)changeShowFps();
      else if(settingsTab==1&&settingsFocus==0)changeSfxVolume(threadlight::settings::adjustSfxVolume(appSettings.sfxVolumePercent,direction));
     }else if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){
      if(settingsTab==0&&settingsFocus==0)changeFrameLimit(threadlight::settings::nextFrameRateLimit(appSettings.frameRateLimit,1));
      else if(settingsTab==0&&settingsFocus==1)changeShowFps();
      else if((settingsTab==0&&settingsFocus==2)||(settingsTab==1&&settingsFocus==1))applyMenuAction(threadlight::menu::Action::Back);
      else if(settingsTab==1&&settingsFocus==0)changeSfxVolume(threadlight::settings::adjustSfxVolume(appSettings.sfxVolumePercent,1));
     }
    }
   }else if(screenAtFrameStart==Mode::ExitConfirm){
    const Rectangle yesButton{SW*.5f-230.f,SH*.5f+52.f,210.f,48.f};
    const Rectangle noButton{SW*.5f+20.f,SH*.5f+52.f,210.f,48.f};
    if(IsKeyPressed(KEY_ESCAPE))applyMenuAction(threadlight::menu::Action::Escape);
    else if(IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_RIGHT))exitSelection=threadlight::menu::moveSelection(exitSelection,1,2);
    else if(IsKeyPressed(KEY_ENTER))applyMenuAction(threadlight::menu::exitSelectionAction(exitSelection));
    else if(hovered(yesButton)){exitSelection=0;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))applyMenuAction(threadlight::menu::Action::ConfirmExit);}
    else if(hovered(noButton)){exitSelection=1;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))applyMenuAction(threadlight::menu::Action::CancelExit);}
   }else if(screenAtFrameStart==Mode::Play){
    if(IsKeyPressed(KEY_P)){
     applyMenuAction(threadlight::menu::Action::Pause);
     stepClock.clear();dashPressPending=collectionContinuePending=false;
    }else{
     dashPressPending=dashPressPending||IsKeyPressed(KEY_SPACE);
     if(lifecycle.phase()==foundation::Phase::Collection)
      collectionContinuePending=collectionContinuePending||IsKeyPressed(KEY_ENTER);
     const int steps=stepClock.advance(frameSeconds);
     for(int tick=0;tick<steps&&mode==Mode::Play;tick++){
      capturePresentation();
      constexpr float dt=static_cast<float>(threadlight::timing::kFixedStepSeconds);
      ++simulationTick;
      const auto tickPhase=lifecycle.phase();
      const int phaseElapsedAtStart=lifecycle.elapsedTicks();
      const bool combat=foundation::isCombat(tickPhase);
      const bool continueCollection=collectionContinuePending;collectionContinuePending=false;
      if(tickPhase==foundation::Phase::Countdown){
       dashPressPending=false;
       lifecycle.tick(foes.size());
       if(lifecycle.phase()!=tickPhase)traceM1("transition");
       continue;
      }
      if(!combat&&tickPhase!=foundation::Phase::Collection)break;
      battle::tick(health);dashCd=std::max(0.f,dashCd-dt);pulse=std::max(0.f,pulse-dt);damageFeedback=std::max(0.f,damageFeedback-dt);shake=std::max(0.f,shake-dt);dashVisualActive=dashTime>0;dashEchoTimer=std::max(0.f,dashEchoTimer-dt);
      V mv{(float)(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT))-(float)(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)),(float)(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN))-(float)(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))}; if(len(mv)>0)mv=norm(mv);playerWasMoving=len(mv)>0;
      const bool dashPressed=dashPressPending;dashPressPending=false;
      if(dashPressed&&dashCd<=0&&len(mv)>0){dashTime=threadlight::kDashDurationSeconds;dashCd=stats.dashCooldownSeconds;dashCycleDuration=dashCd;dashEcho.fill(player);dashEchoCount=0;dashEchoFade=.22f;dashEchoTimer=0;dashVisualActive=true;burst(player,C(132,244,255),10,150);sfx.play(threadlight::audio::Event::Dash);}
      const bool dashActiveThisFrame=threadlight::dashProtectsCollisionThisFrame(dashTime);dashVisualActive=dashVisualActive||dashActiveThisFrame;dashEchoFade=std::max(0.f,dashEchoFade-dt);float mult=dashActiveThisFrame?3.7f:1.f;if(dashActiveThisFrame)dashTime=std::max(0.f,dashTime-dt);player=player+mv*(stats.moveSpeed*mult*dt);player.x=threadlight::layout::clampPlayerX(player.x,playArea);player.y=threadlight::layout::clampPlayerY(player.y,playArea);
      if(dashActiveThisFrame&&dashEchoTimer<=0){const size_t last=std::min(dashEchoCount,dashEcho.size()-1);for(size_t i=last;i>0;i--)dashEcho[i]=dashEcho[i-1];dashEcho[0]=player;dashEchoCount=std::min(dashEchoCount+1,dashEcho.size());dashEchoTimer=.028f;dashEchoFade=std::max(dashEchoFade,.22f);}
      if(combat){
       if(thread.empty()||dist(thread.back().p,player)>9)thread.push_back({player,0});
       for(auto &t:thread)t.age+=dt;
       while(!thread.empty()&&(thread.front().age>threadlight::visual::kSilkLifetimeSeconds||thread.size()>520))thread.erase(thread.begin());
       const bool bossEncounter=chapter::encounter(wave).boss;
       if(tickPhase==foundation::Phase::Spawning&&!bossEncounter){
        while(spawnCursor<spawnSchedule.count&&spawnSchedule.items[spawnCursor].tick<=phaseElapsedAtStart){
         const auto entry=spawnSchedule.items[spawnCursor++];addFoe(entry.kind,entry.angle);
        }
       }
       if(bossEncounter&&root::alive(boss)){
        const auto bossEvents=root::tick(boss,{player.x,player.y},bossArena());
        releaseSupports(bossEvents.supports);
        if(bossEvents.warning){sfx.play(threadlight::audio::Event::BossWarning);traceBoss("warning");}
        bool touched=false;
        for(size_t i=1;i<thread.size();++i){
         if(thread[i].age>stats.activeSilkSeconds||!root::silkContact(boss,{thread[i-1].p.x,thread[i-1].p.y},{thread[i].p.x,thread[i].p.y},stats.silkHalfWidth))continue;
         touched=true;const auto hit=root::applySilk(boss,stats.silkDamage);
         if(hit.hit==battle::SilkHit::None)break;
         burst({boss.p.x,boss.p.y},C(194,255,227),4,80);traceBoss("silk_hit");
         if(hit.events.phaseChanged){sfx.play(threadlight::audio::Event::BossPhase);releaseSupports(hit.events.supports);traceBoss("phase_two");}
         if(hit.events.defeated){
          // Death cancels geometry immediately. Remaining supports use the ledger's
          // rewardless terminal path; already-earned pickups are untouched.
          for(const auto& foe:foes){rewards.despawnEnemy(foe.rewardId);traceM1("boss_support_removal",foe.rewardId);}
          foes.clear();
          if(const auto drop=rewards.defeatEnemy(bossRewardId)){
           Spark core{{threadlight::layout::clampPlayerX(boss.p.x,playArea),threadlight::layout::clampPlayerY(boss.p.y,playArea)},0};
           core.pickup.id=drop->id;core.bossCore=true;sparks.push_back(core);traceM1("boss_core_created",drop->id,drop->sparkValue);
          }
          burst({boss.p.x,boss.p.y},C(255,214,142),32,180);sfx.play(threadlight::audio::Event::BossPhase);traceBoss("defeated");
         }else sfx.play(threadlight::audio::Event::EnemyHit);
         break;
        }
        if(touched)++boss.metrics.contactTicks;
       }
       int commitments=0;
       for(const auto& f:foes)if(f.kind==battle::EnemyKind::Skitter&&skitter::committed(f.skitterState))++commitments;
       battle::IncomingHit incoming;
       if(bossEncounter)incoming.consider(root::incomingDamage(boss,{previousPlayer.x,previousPlayer.y},{player.x,player.y},radius),bossRewardId);
       for(auto &f:foes){
        const V oldPosition=f.p;const bool wasCommitted=f.kind==battle::EnemyKind::Skitter&&skitter::committed(f.skitterState);
        f.phase+=dt;battle::tick(f.health);f.hitFlash=std::max(0.f,f.hitFlash-dt);
        if(f.kind==battle::EnemyKind::Skitter){
         foundation::Point p{f.p.x,f.p.y},v{f.vel.x,f.vel.y};
         const auto result=skitter::tick(f.skitterState,p,v,{player.x,player.y},{player.x-previousPlayer.x,player.y-previousPlayer.y},
          {playArea.left+f.r,playArea.right-f.r,playArea.top+f.r,playArea.bottom-f.r},commitments<2);
         f.p={p.x,p.y};f.vel={v.x,v.y};
         commitments+=(skitter::committed(f.skitterState)?1:0)-(wasCommitted?1:0);
         if(result.telegraphStarted)traceM1("skitter_windup",f.rewardId);
         if(result.burstStarted)traceM1("skitter_burst",f.rewardId);
        }else{
         const V d=norm(player-f.p);const float wob=std::sin(f.phase*3)*.28f;
         f.vel=f.vel+(d+V{-d.y*wob,d.x*wob})*(f.speed*dt*2.5f);
         const float vl=len(f.vel);if(vl>f.speed)f.vel=f.vel*(f.speed/vl);f.p=f.p+f.vel*dt;
        }
        for(size_t i=1;i<thread.size();i++){
         if(thread[i].age>stats.activeSilkSeconds)continue;
         if(!battle::silkContact({oldPosition.x,oldPosition.y},{f.p.x,f.p.y},f.r,
             {thread[i-1].p.x,thread[i-1].p.y},{thread[i].p.x,thread[i].p.y},stats.silkHalfWidth))continue;
         const auto hit=battle::applySilk(f.health,stats.silkDamage);
         if(hit==battle::SilkHit::None)break;
         f.hitFlash=threadlight::visual::kEnemyHitFlashSeconds;burst(f.p,C(83,255,226,220),4,70);
         if(hit==battle::SilkHit::Killed){
          if(const auto drop=rewards.defeatEnemy(f.rewardId)){
           V p{threadlight::layout::clampPlayerX(f.p.x,playArea),threadlight::layout::clampPlayerY(f.p.y,playArea)};
           Spark spark{p,0};spark.pickup.id=drop->id;sparks.push_back(spark);
           addDeathEffect(f);shake=std::max(shake,f.kind==battle::EnemyKind::Brute?.105f:.045f);
           sfx.play(threadlight::audio::Event::EnemyDeath);traceM1("kill",drop->id,drop->sparkValue);
          }
          if(f.kind==battle::EnemyKind::Skitter&&skitter::committed(f.skitterState))--commitments;
         }else sfx.play(threadlight::audio::Event::EnemyHit);
         break;
        }
        if(f.health.alive()&&battle::bodyContact({previousPlayer.x,previousPlayer.y},{player.x,player.y},radius,
             {oldPosition.x,oldPosition.y},{f.p.x,f.p.y},f.r))incoming.consider(battle::definition(f.kind).contactDamage,f.rewardId);
       }
       if(const int damage=battle::applyIncoming(health,incoming,dashActiveThisFrame)){
        if(bossEncounter)boss.metrics.playerDamage+=damage;
        traceM1("player_damage",incoming.sourceId,damage);
        damageFeedback=threadlight::visual::kDamageFeedbackDuration;shake=threadlight::visual::kDamageShakeSeconds;
        sfx.play(threadlight::audio::Event::PlayerDamage);burst(player,C(255,92,119),13,170);
       }
       foes.erase(std::remove_if(foes.begin(),foes.end(),[](const Foe& f){return !f.health.alive();}),foes.end());
      }
      for(auto &s:sparks){
       s.t+=dt;foundation::Point position{s.p.x,s.p.y};
       const auto result=foundation::tickPickup(s.pickup,position,{previousPlayer.x,previousPlayer.y},{player.x,player.y},radius,combat,true,stats.pickupRadius,stats.pickupPullSpeed);
       s.p={position.x,position.y};
       if(result==foundation::PickupResult::Collected){
        const auto before=rewards.counters().creditsIssued;
        if(rewards.collectPickup(s.pickup.id)){sfx.play(threadlight::audio::Event::Spark);burst(s.p,C(255,218,125),8,100);traceM1("pickup",s.pickup.id);}
        if(rewards.counters().creditsIssued!=before)traceM1("credit_issued",rewards.counters().creditsIssued);
       }else if(result==foundation::PickupResult::Expired){rewards.expirePickup(s.pickup.id);traceM1("pickup_expired",s.pickup.id);}
      }
      sparks.erase(std::remove_if(sparks.begin(),sparks.end(),[](const Spark& s){return s.pickup.terminal!=foundation::PickupResult::None;}),sparks.end());
      const float particleDamping=threadlight::timing::dampingMultiplier(.96f,60.f,dt);
      for(auto &p:particles){p.life-=dt;p.p=p.p+p.v*dt;p.v=p.v*particleDamping;}particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Particle& p){return p.life<=0;}),particles.end());
      for(auto &effect:deathEffects)effect.age+=dt;deathEffects.erase(std::remove_if(deathEffects.begin(),deathEffects.end(),[](const DeathEffect& e){return e.age>=(e.kind?threadlight::visual::kHeavyDeathSeconds:threadlight::visual::kSmallDeathSeconds);}),deathEffects.end());
      const auto events=lifecycle.tick(foes.size(),chapter::encounter(wave).boss&&!root::alive(boss),continueCollection,!sparks.empty());
      if(events.timeoutDespawn){
       for(const auto& foe:foes){rewards.despawnEnemy(foe.rewardId);traceM1("timeout_removal",foe.rewardId);}
       foes.clear();traceM1("cleanup_timeout",0,foundation::kCleanupTicks);
      }
      if(events.enteredCollection){thread.clear();dashTime=0;dashVisualActive=false;dashEchoCount=0;dashEchoFade=0;traceM1(events.timeoutDespawn?"collection_timeout":"collection_clear",0,tickPhase==foundation::Phase::Cleanup?phaseElapsedAtStart+1:0);}
      else if(lifecycle.phase()!=tickPhase)traceM1("transition");
      // Complete bookkeeping on a lethal tick, but never open rewards or advance
      // encounters after defeat. Loss retains outcome precedence.
      if(!health.alive()){mode=Mode::Lost;sfx.play(threadlight::audio::Event::Defeat);traceM1("defeat");continue;}
      if(events.enteredPendingRewards){
       traceM1("collection_finished",0,phaseElapsedAtStart+1);
       if(m1Telemetry){const auto& c=rewards.counters();TraceLog(LOG_INFO,"M3 collection wave=%d ticks=%d early=%d live=%llu created=%llu collected=%llu expired=%llu pending=%llu bank=%lld",
        wave+1,phaseElapsedAtStart+1,continueCollection?1:0,(unsigned long long)(c.sparksCreated-c.sparksCollected-c.sparksExpired),
        (unsigned long long)c.sparksCreated,(unsigned long long)c.sparksCollected,(unsigned long long)c.sparksExpired,
        (unsigned long long)rewards.pendingCredits(),(long long)rewards.remainder());}
       if(chapter::encounter(wave).boss){
        // Preserve the pending ledger and live pickups for the eventual Chapter II
        // handoff. This prototype stops without spending unusable evolutions.
        mode=Mode::Won;sfx.play(threadlight::audio::Event::Victory);traceM1("chapter_prototype_complete");
       }else if(rewards.pendingCredits()){
        upgrades::prepareOffer(offers,ranks,health,rewards.nextCredit(),true,chapter::encounter(wave+1).boss);choice=0;mode=Mode::Upgrade;sfx.play(threadlight::audio::Event::Evolution);traceM1("offer_open",offers.current.credit);
       }else finishEncounter();
      }
     }
     if(mode!=Mode::Play)stepClock.clear();
    }
   }else if(screenAtFrameStart==Mode::Upgrade){
    const auto cards=upgradeUI::layout(SW,SH,offers.current.count);
    const int pointed=menuFocus.mouseOwnsFocus()?upgradeUI::cardAt(cards,mouse.x,mouse.y,offers.current.count):-1;
    if(pointed>=0)choice=pointed;
    if(IsKeyPressed(KEY_ONE)||IsKeyPressed(KEY_A))choice=0;
    else if(IsKeyPressed(KEY_TWO)||IsKeyPressed(KEY_D))choice=1;
    else if(IsKeyPressed(KEY_THREE)||IsKeyPressed(KEY_S))choice=2;
    else if(IsKeyPressed(KEY_LEFT))choice=threadlight::menu::moveSelection(choice,-1,offers.current.count);
    else if(IsKeyPressed(KEY_RIGHT))choice=threadlight::menu::moveSelection(choice,1,offers.current.count);
    choice=std::clamp(choice,0,std::max(0,offers.current.count-1));
    const bool confirm=IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)||
     (menuFocus.mouseOwnsFocus()&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&upgradeUI::contains(cards.confirm,mouse.x,mouse.y));
    const auto selected=offers.current.count>0?offers.current.cards[choice]:upgrades::Id::Count;
    if(confirm&&(offers.current.count==0
       ?upgrades::resolveExhaustedCredit(rewards,lifecycle.phase(),offers.current,ranks,health,true)
       :upgrades::resolveChoice(rewards,lifecycle.phase(),offers.current,choice,ranks,health,true))){
     stats=upgrades::derive(ranks);traceM1("credit_resolved",offers.current.credit);
     if(m1Telemetry)TraceLog(LOG_INFO,"M2 evolution credit=%llu upgrade=%d rank=%d damage=%d speed=%.0f cooldown=%.2f hp=%d maxhp=%d",
      (unsigned long long)offers.current.credit,(int)selected,upgrades::rank(ranks,selected),stats.silkDamage,stats.moveSpeed,stats.dashCooldownSeconds,health.hp,health.maxHp);
     choice=0;
     if(!rewards.pendingCredits()){mode=Mode::Play;finishEncounter();}
     else{upgrades::prepareOffer(offers,ranks,health,rewards.nextCredit(),true);traceM1("offer_open",offers.current.credit);}
     stepClock.clear();dashPressPending=collectionContinuePending=false;
    }
   }
   else if(screenAtFrameStart==Mode::Lost||screenAtFrameStart==Mode::Won){if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_R))reset();else if(IsKeyPressed(KEY_ESCAPE))applyMenuAction(threadlight::menu::Action::Escape);}

   if(settingsEdits.finishDrag(mode==Mode::Settings&&settingsTab==1,
       IsWindowFocused(),IsMouseButtonDown(MOUSE_BUTTON_LEFT),false))saveSettings();

   if(mode!=screenAtFrameStart){stepClock.clear();dashPressPending=collectionContinuePending=false;capturePresentation();}
   const float alpha=mode==Mode::Play?stepClock.presentationAlpha():1.f;
   const V shownPlayer=threadlight::presentation::blend(previousPlayer,player,alpha);
   const float shownShake=threadlight::presentation::decay(previousShake,shake,alpha);
   const float shownDamageFeedback=threadlight::presentation::decay(previousDamageFeedback,damageFeedback,alpha);
   const float shownDashEchoFade=threadlight::presentation::decay(previousDashEchoFade,dashEchoFade,alpha);

  BeginDrawing();ClearBackground(C(7,12,27));
  // Own the modelview for the entire frame: raylib's retained screenScale can
  // change independently of its startup mouse scale. Its projection/viewport
  // already match the framebuffer. Apply our startup scale exactly once.
  rlLoadIdentity();
  rlScalef(windowCoordinates.scale.x,windowCoordinates.scale.y,1.f);
  const float time=(float)GetTime();
  const float shakeStrength=threadlight::visual::shakeEnvelope(shownShake,threadlight::visual::kDamageShakeSeconds);
  const float cameraShiftX=std::sin(time*71.f)*shakeStrength*threadlight::layout::kCameraShakeX;
  const float cameraShiftY=std::sin(time*53.f+1.4f)*shakeStrength*threadlight::layout::kCameraShakeY;
  rlPushMatrix();
  rlTranslatef(cameraShiftX,cameraShiftY,0.f);
  const float playWidth=playArea.right-playArea.left,playHeight=playArea.bottom-playArea.top;
  const float glowRadius=std::min(playWidth,playHeight)*.19f;
  for(int i=0;i<5;i++){
   const float x=playArea.left+playWidth*(.12f+.19f*i);
   const float y=playArea.top+playHeight*((i%2==0)?.34f:.68f);
   DrawCircleGradient(x,y,glowRadius,C(13,33,57,43),C(7,12,27,0));
  }
  for(size_t i=0;i<stars.size();i++){
   const float tw=.4f+.6f*(.5f+.5f*std::sin(time*(.6f+(i%7)*.13f)+i));
   const V star{playArea.left+stars[i].x*playWidth,playArea.top+stars[i].y*playHeight};
   DrawCircleV(cv(star),i%11==0?2:1,C(114,183,213,(unsigned char)(40+tw*130)));
  }
  for(int x=32;x<SW;x+=48)DrawLine(x,(int)playArea.top,x,(int)playArea.bottom,C(34,66,83,26));
  for(int y=(int)playArea.top+20;y<playArea.bottom;y+=48)DrawLine(0,y,SW,y,C(34,66,83,22));
  // A short Catmull-Rom subdivision keeps the wake organic without changing
  // the sampled collision segments or allocating temporary geometry.
  // A newly sampled silk endpoint belongs to the current tick. Draw it at
  // the interpolated player instead of letting the silk lead the player.
  auto silkPoint=[&](size_t i){return threadlight::presentation::silkPoint(thread[i],i+1==thread.size(),shownPlayer);};
  auto silkAge=[&](size_t i){return threadlight::presentation::blend(thread[i].previousAge,thread[i].age,alpha);};
  if(thread.size()>1){
   for(size_t i=1;i<thread.size();i++){
    const V p0=silkPoint(i>1?i-2:i-1),p1=silkPoint(i-1),p2=silkPoint(i),p3=silkPoint(i+1<thread.size()?i+1:i);
    for(int step=1;step<=3;step++){
     const float t0=(float)(step-1)/3.f,t1=(float)step/3.f;
     const V a=catmullRom(p0,p1,p2,p3,t0),b=catmullRom(p0,p1,p2,p3,t1);
     const float age=silkAge(i-1)+(silkAge(i)-silkAge(i-1))*(t0+t1)*.5f;
     const float shimmer=.5f+.5f*std::sin(time*5.f-age*1.6f);
     const auto silk=threadlight::visual::silkAppearance(age,shimmer,stats.activeSilkSeconds);
     Color halo=ColorLerp(C(0,222,211),C(42,129,165),silk.ageBlend);
     Color body=ColorLerp(C(14,247,220),C(45,151,184),silk.ageBlend);
     Color core=ColorLerp(C(224,255,246),C(119,184,204),silk.ageBlend);
     halo.a=(unsigned char)std::lround(silk.haloOpacity*255.f);
     body.a=(unsigned char)std::lround(silk.bodyOpacity*(.94f+.06f*shimmer)*255.f);
     core.a=(unsigned char)std::lround(silk.coreOpacity*255.f);
     DrawLineEx(cv(a),cv(b),8.5f+(stats.silkHalfWidth-5.f)*2.f-1.5f*silk.ageBlend,halo);
     DrawLineEx(cv(a),cv(b),4.4f+(stats.silkHalfWidth-5.f)-1.0f*silk.ageBlend,body);
     DrawLineEx(cv(a),cv(b),1.75f-.35f*silk.ageBlend,core);
    }
   }
  }
  // A short, bright trail behind the dashing player communicates direction
  // and distance, then fades quickly without covering enemies or silk.
  if(shownDashEchoFade>0&&previousDashEchoCount>1){
   const float fade=std::clamp(shownDashEchoFade/.22f,0.f,1.f);
   for(size_t i=previousDashEchoCount-1;i>0;i--){
    const float ageWeight=.25f+.75f*(1.f-(float)(i-1)/(float)(previousDashEchoCount-1));
    const unsigned char glow=(unsigned char)(86.f*fade*ageWeight),core=(unsigned char)(230.f*fade*ageWeight);
    DrawLineEx(cv(previousDashEcho[i]),cv(previousDashEcho[i-1]),7.f*fade,C(0,183,229,glow));
    DrawLineEx(cv(previousDashEcho[i]),cv(previousDashEcho[i-1]),2.4f*fade,C(190,248,255,core));
    DrawCircleV(cv(previousDashEcho[i]),radius*.72f,C(67,220,255,(unsigned char)(145.f*fade*ageWeight)));
   }
   DrawLineEx(cv(previousDashEcho[0]),cv(shownPlayer),7.f*fade,C(0,183,229,(unsigned char)(86.f*fade)));
   DrawLineEx(cv(previousDashEcho[0]),cv(shownPlayer),2.4f*fade,C(190,248,255,(unsigned char)(230.f*fade)));
  }
  for(const auto& source:sparks){auto s=source.present(alpha);
   const bool expiring=foundation::isCombat(lifecycle.phase())&&s.pickup.combatAgeTicks>foundation::kPickupLifetimeTicks-360;
   const float warning=expiring?(.62f+.38f*std::sin(s.t*9.f)):1.f;
   float r=(s.bossCore?12.f:8.f)+2*std::sin(time*6+s.t);
   DrawCircleV(cv(s.p),r+8,C(255,185,95,20));DrawPoly(cv(s.p),4,r,45,Fade(C(255,209,115),warning));
   DrawPolyLinesEx(cv(s.p),4,r+2,45,1.1f,Fade(C(255,242,187,190),warning));
   if(s.bossCore)DrawCircleLines((int)s.p.x,(int)s.p.y,r+6,C(255,222,153,190));
  }
  for(const auto& source:deathEffects){const auto effect=source.present(alpha);
   const float duration=effect.kind?threadlight::visual::kHeavyDeathSeconds:threadlight::visual::kSmallDeathSeconds;
   const float progress=std::clamp(effect.age/duration,0.f,1.f),remain=1.f-progress;
   const float eased=threadlight::visual::deathProgress(effect.age,duration);
   if(effect.kind){
    DrawRing(cv(effect.p),8.f+31.f*eased,10.f+31.f*eased,0,360,32,C(255,133,193,(unsigned char)(115.f*remain)));
    DrawCircleV({effect.p.x,effect.p.y+17.f*progress},(1.f-eased)*12.f,C(255,123,184,(unsigned char)(95.f*remain)));
   }else DrawCircleV(cv(effect.p),6.f*(1.f-progress),C(255,179,218,(unsigned char)(125.f*remain)));
   const int shardCount=effect.kind?threadlight::visual::kHeavyShardCount:threadlight::visual::kSmallShardCount;
   for(int i=0;i<shardCount;i++){
    const V velocity=effect.velocity[i];const float fall=effect.kind?150.f*effect.age*effect.age:0.f;
    const V pos=effect.p+velocity*(effect.age*(1.f-.24f*progress))+V{0,fall};
    const float angle=std::atan2(velocity.y,velocity.x)*57.29578f+(effect.kind?time*42.f:0.f);
    const float shardSize=(effect.kind?5.4f:4.2f)*(1.f-.46f*progress);
    const Color shard=effect.kind?C(255,143,197,(unsigned char)(235.f*remain)):C(255,190,228,(unsigned char)(245.f*remain));
    DrawPoly(cv(pos),effect.kind?4:3,shardSize,angle,shard);
    if(i%2==0)DrawCircleV(cv(pos),1.3f*remain,C(186,255,243,(unsigned char)(175.f*remain)));
   }
  }
  // Locked burst lanes use simulation state; they remain visible throughout the burst.
  for(const auto& f:foes)if(f.kind==battle::EnemyKind::Skitter&&skitter::committed(f.skitterState)){
   const auto& state=f.skitterState;
   const Vector2 a{state.burstStart.x,state.burstStart.y},b{state.burstEnd.x,state.burstEnd.y};
   const V direction=norm(V{b.x-a.x,b.y-a.y});const V side{-direction.y*(f.r+radius),direction.x*(f.r+radius)};
   DrawLineEx(a,b,(f.r+radius)*2.f,C(255,157,118,23));
   DrawLineEx({a.x+side.x,a.y+side.y},{b.x+side.x,b.y+side.y},1.f,C(255,177,139,175));
   DrawLineEx({a.x-side.x,a.y-side.y},{b.x-side.x,b.y-side.y},1.f,C(255,177,139,175));
   DrawCircleLines((int)b.x,(int)b.y,f.r+radius,C(255,177,139,130));
  }
  if(chapter::encounter(wave).boss&&root::alive(boss)){
   const bool warning=boss.stage==root::Stage::RootWarning;
   const bool reach=boss.stage==root::Stage::RootActive;
   // Draw the exact stored capsule geometry; growing active branches use the same
   // endpoint helper as collision. Warning shapes never follow the player.
   if(warning||reach)for(int i=0;i<boss.branchCount;++i){
    const auto branch=reach?root::activeBranch(boss,i):boss.branches[i];
    const Vector2 a{branch.a.x,branch.a.y},b{branch.b.x,branch.b.y};
    const V direction=norm(V{b.x-a.x,b.y-a.y}),side{-direction.y*root::kRootHalfWidth,direction.x*root::kRootHalfWidth};
    const Color line=reach?C(255,117,137,235):C(255,163,145,200);
    DrawLineEx(a,b,root::kRootHalfWidth*2.f,reach?C(211,74,105,105):C(223,111,122,22));
    DrawLineEx({a.x+side.x,a.y+side.y},{b.x+side.x,b.y+side.y},1.6f,line);
    DrawLineEx({a.x-side.x,a.y-side.y},{b.x-side.x,b.y-side.y},1.6f,line);
    DrawCircleLines((int)a.x,(int)a.y,root::kRootHalfWidth,line);DrawCircleLines((int)b.x,(int)b.y,root::kRootHalfWidth,line);
   }
   if(boss.stage==root::Stage::StepWarning||boss.stage==root::Stage::StepActive){
    const Vector2 a{boss.stepStart.x,boss.stepStart.y},b{boss.stepEnd.x,boss.stepEnd.y};
    const V direction=norm(V{b.x-a.x,b.y-a.y}),side{-direction.y*root::kContactRadius,direction.x*root::kContactRadius};
    DrawLineEx(a,b,root::kContactRadius*2,C(255,128,155,25));
    DrawLineEx({a.x+side.x,a.y+side.y},{b.x+side.x,b.y+side.y},1.5f,C(255,174,154,205));
    DrawLineEx({a.x-side.x,a.y-side.y},{b.x-side.x,b.y-side.y},1.5f,C(255,174,154,205));
    DrawCircleLines((int)b.x,(int)b.y,root::kContactRadius,C(255,174,154,205));
   }
   const auto shown=root::interpolate(boss.previousP,boss.p,alpha);const Vector2 pos{shown.x,shown.y};
   const bool open=root::exposed(boss),transition=boss.stage==root::Stage::Transition;
   const float breath=1.f+.025f*std::sin((float)boss.metrics.ticks*.025f);
   const Color bark=open?C(244,205,129):transition?C(152,233,221):C(201,137,138);
   DrawCircleV(pos,root::kVulnerableRadius+12,C(160,104,135,13));
   for(int i=0;i<6;++i){
    const float a=i*chapter::kPi/3.f+.13f*boss.phase;const V out{std::cos(a),std::sin(a)};
    const Vector2 end{pos.x+out.x*48.f*breath,pos.y+out.y*48.f*breath};
    DrawLineEx(pos,end,12.f,C(33,36,50));DrawLineEx(pos,end,2.f,bark);
    DrawPoly(end,4,8.f,a*57.2958f+45,C(40,43,54));DrawPolyLinesEx(end,4,8.f,a*57.2958f+45,1.2f,bark);
   }
   DrawCircleLines((int)pos.x,(int)pos.y,root::kVulnerableRadius,Fade(bark,open?.9f:.35f));
   DrawPoly(pos,8,root::kContactRadius,22.5f,C(44,29,47));
   DrawPolyLinesEx(pos,8,root::kContactRadius,22.5f,2.f,transition?C(149,226,213):C(245,116,140));
   DrawLineEx({pos.x-10,pos.y-14},{pos.x+4,pos.y+15},2.f,bark);DrawLineEx({pos.x+11,pos.y-10},{pos.x-7,pos.y+16},2.f,bark);
   if(open)DrawCircleV(pos,7,C(255,223,154,225));
   else DrawPoly(pos,4,6,45,bark);
  }
  for(const auto& source:foes){auto f=source.present(alpha);
   const float flash=threadlight::visual::hitFlashStrength(f.hitFlash);
   const bool heavy=f.kind==battle::EnemyKind::Brute,skitterBody=f.kind==battle::EnemyKind::Skitter;
   const int sides=skitterBody?3:heavy?6:4;
   const Color base=skitterBody?C(255,164,125):heavy?C(255,116,178):C(255,97,132);
   const Color edge=ColorLerp(base,C(207,255,246,255),flash);
   const float bob=std::sin(f.phase*5.f)*2.2f,breath=1.f+std::sin(f.phase*3.2f)*.035f;
   const Vector2 pos={f.p.x,f.p.y+bob};
   DrawCircleV({pos.x,pos.y+f.r*.48f},f.r*.7f,C(3,9,22,95));
   DrawCircleV(pos,f.r+8,C(255,63,123,(unsigned char)(14+flash*30)));
   DrawPoly(pos,sides,f.r*breath,time*40.f,C(25,22,45));
   DrawPolyLinesEx(pos,sides,f.r*breath,time*40.f,1.5f+flash*1.1f,edge);
   if(heavy){const float arc=time*25.f;DrawCircleV(pos,3.f,C(255,116,178,175));DrawRing(pos,f.r*.67f,f.r*.75f,arc,arc+55.f,18,C(255,173,207,120));}
   else DrawLineEx({pos.x-6,pos.y+5},{pos.x+6,pos.y+5},1.f,C(255,141,167,165));
   DrawCircleV({pos.x-4,pos.y-2},2.2f,C(255,238,220));DrawCircleV({pos.x+4,pos.y-2},2.2f,C(255,238,220));
   if(flash>0)DrawCircleV(pos,f.r*.56f,C(219,255,249,(unsigned char)(105.f*flash)));
   if(f.health.hp<f.health.maxHp){DrawRectangle((int)(f.p.x-f.r),(int)(f.p.y-f.r-10),((int)(2*f.r)),3,C(50,48,71));DrawRectangle((int)(f.p.x-f.r),(int)(f.p.y-f.r-10),(int)(2*f.r*std::clamp((float)f.health.hp/f.health.maxHp,0.f,1.f)),3,C(255,118,157));}
  }
  for(const auto& source:particles){auto p=source.present(alpha);float a=std::clamp(p.life/p.maxLife,0.f,1.f);DrawCircleV(cv(p.p),p.size*a,Fade(p.color,a));}
  if(mode!=Mode::Title){
   const float impact=threadlight::visual::damageImpactStrength(shownDamageFeedback);
   const bool dashVisual=dashTime>0||dashVisualActive;
   DrawCircleV(cv(shownPlayer),31,C(45,230,214,18));
   DrawCircleV(cv(shownPlayer),21,C(51,230,221,34));
   if(impact>0)DrawCircleV(cv(shownPlayer),radius+13,C(242,76,101,(unsigned char)(35*impact)));
   const Color rim=dashVisual?C(126,239,255,245):ColorLerp(C(164,255,232,220),C(255,133,151,235),impact*.72f);
   const Color core=dashVisual?C(224,251,255,255):ColorLerp(C(221,255,245,255),C(255,171,181,255),impact*.48f);
   DrawCircleV(cv(shownPlayer),radius+4,rim);
   DrawCircleV(cv(shownPlayer),radius,core);
   if(impact>0){const float ringRadius=threadlight::visual::damageRingRadius(radius,shownDamageFeedback);DrawRing(cv(shownPlayer),ringRadius,ringRadius+1.6f,0,360,48,C(255,118,139,(unsigned char)(100*impact)));}
  }
  rlPopMatrix();
  const bool showRunHud=mode==Mode::Play||mode==Mode::Pause||mode==Mode::Upgrade||
      mode==Mode::Lost||mode==Mode::Won||(mode==Mode::Settings&&settingsReturn==Mode::Pause);
  const auto hud=threadlight::layout::hudLayout(SW,SH);
  if(showRunHud){
   const float hudHeight=threadlight::layout::kHudBottom;
   const int titleSize=SW<1100?22:25;
   DrawRectangle(0,0,SW,(int)hudHeight,C(5,10,24,235));
   DrawLine(0,(int)hudHeight,SW,(int)hudHeight,C(60,152,156,80));
   drawText("THREADLIGHT",26,22,titleSize,C(200,255,243));
   if(bossFixture>=0)drawText("DEV FIXTURE",26,49,10,C(255,213,131));
   else if(SW>=1180)drawText("THE BRIAR CIRCUIT",214,31,13,C(91,167,179));
   DrawText(TextFormat("VITALITY %.0f / %d",health.hp/100.f,health.maxHp/100),(int)hud.healthX,17,11,C(120,164,178));
   DrawRectangle((int)hud.healthX,38,(int)hud.healthWidth,10,C(34,49,67));
   DrawRectangle((int)hud.healthX,38,(int)(hud.healthWidth*std::clamp((float)health.hp/health.maxHp,0.f,1.f)),10,health.hp>health.maxHp*.35f?C(72,225,190):C(255,91,127));
   drawText(TextFormat("WAVE %d / 7",wave+1),(int)hud.waveX,22,18,C(217,239,242));
   drawText(TextFormat("SPARKS %lld / %d",(long long)rewards.remainder(),rewards.nextThreshold()),(int)hud.sparksX,24,15,C(255,213,131));
   drawText(TextFormat("SCORE %06llu",(unsigned long long)rewards.score()),(int)hud.scoreX,24,17,C(177,208,221));
   drawText(chapter::encounter(wave).boss&&foundation::isCombat(lifecycle.phase())?"DEFEAT THE GUARDIAN":TextFormat("%s %.1fs",foundation::phaseName(lifecycle.phase()),lifecycle.remainingTicks()/120.f),(int)hud.waveX,51,11,C(121,196,197));
   drawText(TextFormat("CREDITS %llu",(unsigned long long)rewards.pendingCredits()),(int)hud.sparksX,51,11,C(177,208,221));
  }
  if(showRunHud&&chapter::encounter(wave).boss){
   const auto b=chapterHud::layout(SW,SH).bossBar;
   DrawText("THE ROOTWARDEN",26,60,13,C(226,197,161));
   DrawRectangle((int)b.x,(int)b.y,(int)b.width,(int)b.height,C(50,39,58));
   DrawRectangle((int)b.x,(int)b.y,(int)(b.width*(float)boss.health.hp/boss.health.maxHp),(int)b.height,root::exposed(boss)?C(255,218,141):C(225,121,151));
   DrawText(TextFormat("%s  %.0f/70",!root::alive(boss)?"DEFEATED":root::exposed(boss)?"EXPOSED +1":boss.stage==root::Stage::Transition?"BARK OPENING":boss.phase==1?"PHASE I":"PHASE II",boss.health.hp/100.f),(int)(b.x+b.width+12),60,13,C(223,197,182));
  }
  if(mode==Mode::Play){
   const float footerY=hud.footerY;
   DrawRectangle(12,(int)(footerY+9),SW-24,34,C(5,10,24,185));
   drawText("WASD MOVE",24,(int)(footerY+20),12,C(149,191,199));
   drawText("SPACE DASH",125,(int)(footerY+20),12,C(149,191,199));

   drawText(lifecycle.phase()==foundation::Phase::Collection?TextFormat("COLLECT %.1fs / ENTER",lifecycle.remainingTicks()/120.f):chapter::encounter(wave).name,(int)hud.hintX,(int)(footerY+20),12,C(102,219,207));
   drawText("SFX IN SETTINGS",(int)hud.settingsHintX,(int)(footerY+20),12,C(149,191,199));
   drawText("P  PAUSE",(int)hud.pauseHintX,(int)(footerY+20),12,C(149,191,199));
  }
  if(showRunHud){
   if(mode!=Mode::Play)drawText("SPACE DASH",125,(int)(hud.footerY+20),12,C(84,116,130));
   const auto dash=chapterHud::dashView(dashCd,dashCycleDuration,dashTime>0||dashVisualActive,
    playerWasMoving);
   const auto track=chapterHud::layout(SW,SH).dashTrack;
   const Color activeDashColor=dash.state==chapterHud::DashState::Active?C(148,236,255):dash.state==chapterHud::DashState::Recharging?C(91,156,184):C(112,234,203);
   const Color dashColor=mode==Mode::Play?activeDashColor:Fade(activeDashColor,.45f);
   DrawRectangle((int)track.x,(int)track.y,(int)track.width,2,C(42,64,80));
   DrawRectangle((int)track.x,(int)track.y,(int)(track.width*dash.fill),2,dashColor);
   DrawText(dash.state==chapterHud::DashState::Active?"DASH":dash.state==chapterHud::DashState::Recharging?TextFormat("%.1fs",dash.remainingTenths/10.f):dash.state==chapterHud::DashState::ReadyMove?"READY / MOVE":"READY",(int)track.x,(int)track.y-14,12,dashColor);
  }
  auto drawButton=[&](Rectangle rectangle,const char* label,bool selected,bool hovered,int fontSize=16){
   const bool active=selected||hovered;
   const Color fill=active?C(17,64,74,245):C(12,30,48,238);
   const Color outline=active?C(87,240,211,235):C(68,131,146,150);
   DrawRectangleRounded(rectangle,.16f,8,fill);
   DrawRectangleRoundedLines(rectangle,.16f,8,outline);
   const int textWidth=MeasureText(label,fontSize);
   DrawText(label,(int)(rectangle.x+(rectangle.width-textWidth)*.5f),
            (int)(rectangle.y+(rectangle.height-fontSize)*.5f-1),fontSize,
            active?C(221,255,246):C(157,201,207));
  };
  auto panel=[&](const char* kicker,const char* title,const char* sub){
   const Rectangle bounds{SW*.5f-340.f,SH*.5f-230.f,680.f,460.f};
   DrawRectangle(0,0,SW,SH,C(3,7,18,195));
   DrawRectangleRounded(bounds,.045f,16,C(11,22,40,246));
   DrawRectangleRoundedLines(bounds,.045f,16,C(91,216,197,145));
   DrawText(kicker,(int)bounds.x+52,(int)bounds.y+31,13,C(107,228,206));
   const int titleWidth=MeasureText(title,38);
   DrawText(title,(int)(SW*.5f-titleWidth*.5f),(int)bounds.y+58,38,C(226,255,247));
   const int subWidth=MeasureText(sub,16);
   DrawText(sub,(int)(SW*.5f-subWidth*.5f),(int)bounds.y+112,16,C(162,193,204));
   return bounds;
  };
  if(mode==Mode::Title){
   const Rectangle bounds=panel(bossFixture<0?"CHAPTER I DEVELOPMENT PROTOTYPE":"DEVELOPMENT BOSS FIXTURE","THREADLIGHT",bossFixture<0?"Six encounters. One guardian. Weave your way through.":"Isolated Rootwarden test; no campaign progress or checkpoint.");
   DrawText("Move to weave a fading thread. Circle the hunters; let silk do the cutting.",(int)bounds.x+52,(int)bounds.y+153,15,C(175,211,213));
   DrawText("WASD / arrows move     SPACE dash     P pauses",(int)bounds.x+52,(int)bounds.y+181,15,C(121,196,197));
   DrawText(bossFixture<0?"Collect sparks manually. Spend earned credits between encounters.":"Fixture: 0-3 preset ranks, normal health, attacks, damage and rewards.",(int)bounds.x+52,(int)bounds.y+207,15,C(121,196,197));
   const char* labels[]={"START GAME","SETTINGS","EXIT GAME"};
   for(int i=0;i<3;i++)drawButton(mainButton(i),labels[i],mainSelection==i,hovered(mainButton(i)));
   DrawText("UP / DOWN SELECT     ENTER CONFIRMS     ESC OPENS EXIT CONFIRMATION",(int)bounds.x+52,(int)bounds.y+431,12,C(108,167,178));
  }
  if(mode==Mode::Pause){
   panel("THE GARDEN HOLDS ITS BREATH","PAUSED","Your thread is waiting.");
   const char* labels[]={"RESUME","SETTINGS","RESTART RUN","RETURN TO MAIN MENU"};
   for(int i=0;i<4;i++)drawButton(pauseButton(i),labels[i],pauseSelection==i,hovered(pauseButton(i)),15);
   DrawText("P RESUMES     ESC RETURNS TO TITLE",SW/2-142,SH/2+197,12,C(108,167,178));
  }
  if(mode==Mode::Settings){
   DrawRectangle(0,0,SW,SH,C(3,7,18,205));
   const Rectangle settingsPanel{settingsLeft,settingsTop,settingsWidth,512.f};
   DrawRectangleRounded(settingsPanel,.045f,16,C(11,22,40,250));
   DrawRectangleRoundedLines(settingsPanel,.045f,16,C(91,216,197,155));
   DrawText("SETTINGS",(int)settingsLeft+48,(int)settingsTop+27,28,C(226,255,247));
   drawButton(videoTab,"VIDEO",settingsTab==0,hovered(videoTab));
   drawButton(audioTab,"AUDIO",settingsTab==1,hovered(audioTab));
   DrawLine((int)settingsLeft+42,(int)settingsTop+129,(int)(settingsLeft+settingsWidth-42),(int)settingsTop+129,C(58,115,131,95));
   if(settingsTab==0){
    DrawRectangleRoundedLines(frameRow,.08f,8,settingsFocus==0?C(76,201,197,115):C(47,89,105,75));
    DrawText("FRAME RATE LIMIT",(int)settingsLeft+48,(int)settingsTop+153,16,settingsFocus==0?C(205,243,237):C(145,183,191));
    constexpr int frameRates[]={30,60,120,0};
    const char* rateLabels[]={"30 FPS","60 FPS","120 FPS","UNLIMITED"};
    for(int i=0;i<4;i++){
     const bool configured=appSettings.frameRateLimit==frameRates[i];
     drawButton(fpsChips[i],rateLabels[i],configured,hovered(fpsChips[i]),13);
    }
    DrawText("Unlimited removes the game limiter; display or driver caps may still apply.",(int)settingsLeft+48,(int)settingsTop+195,13,C(111,164,176));
    DrawText("SHOW FPS",(int)settingsLeft+48,(int)settingsTop+236,16,settingsFocus==1?C(205,243,237):C(145,183,191));
    DrawText("Measured output rate",(int)settingsLeft+48,(int)settingsTop+259,12,C(111,164,176));
    drawButton(showToggle,appSettings.showFps?"ON":"OFF",settingsFocus==1,hovered(showToggle),14);
   }else{
    DrawRectangleRoundedLines(volumeRow,.08f,8,settingsFocus==0?C(76,201,197,115):C(47,89,105,75));
    DrawText("SOUND EFFECTS VOLUME",(int)settingsLeft+48,(int)settingsTop+160,16,settingsFocus==0?C(205,243,237):C(145,183,191));
    DrawText(TextFormat("%d%%",appSettings.sfxVolumePercent),(int)(settingsLeft+settingsWidth-100.f),(int)settingsTop+158,18,C(226,255,247));
    const float trackY=volumeSlider.y+9.f;
    DrawLineEx({volumeSlider.x,trackY},{volumeSlider.x+volumeSlider.width,trackY},6.f,C(38,63,79));
    const float volumeFraction=appSettings.sfxVolumePercent/100.f;
    DrawLineEx({volumeSlider.x,trackY},{volumeSlider.x+volumeSlider.width*volumeFraction,trackY},6.f,C(45,215,197,215));
    DrawCircleV({volumeSlider.x+volumeSlider.width*volumeFraction,trackY},9.f,C(207,255,243));
    DrawCircleLines((int)(volumeSlider.x+volumeSlider.width*volumeFraction),(int)trackY,11.f,C(73,230,207));
    DrawText("Drag to set volume. Arrow keys change it in 5% steps; changes apply immediately.",(int)settingsLeft+48,(int)settingsTop+245,13,C(111,164,176));
   }
   drawButton(settingsBack,"BACK",(settingsTab==0&&settingsFocus==2)||(settingsTab==1&&settingsFocus==1),hovered(settingsBack));
   DrawText("TAB SWITCHES SECTIONS     ESC RETURNS",(int)settingsLeft+48,(int)settingsTop+488,11,C(108,167,178));
  }
  if(mode==Mode::ExitConfirm){
   const Rectangle bounds=panel("LEAVE THE GARDEN?","CONFIRM EXIT","Your current run will end.");
   constexpr const char* question="Are you sure you want to exit ThreadLight?";
   DrawText(question,(int)(SW*.5f-MeasureText(question,16)*.5f),(int)bounds.y+190,16,C(188,224,222));
   const Rectangle yesButton{SW*.5f-230.f,SH*.5f+52.f,210.f,48.f};
   const Rectangle noButton{SW*.5f+20.f,SH*.5f+52.f,210.f,48.f};
   drawButton(yesButton,"Yes, Exit Game",exitSelection==0,hovered(yesButton),15);
   drawButton(noButton,"No, Stay",exitSelection==1,hovered(noButton),15);
   DrawText("ESC CANCELS",SW/2-45,SH/2+122,12,C(108,167,178));
  }
  if(mode==Mode::Upgrade){
   const auto ui=upgradeUI::layout(SW,SH,offers.current.count);
   auto rect=[](upgradeUI::Rect r){return Rectangle{r.x,r.y,r.width,r.height};};
   DrawRectangle(0,0,SW,SH,C(3,7,18,205));
   DrawRectangleRounded(rect(ui.panel),.045f,16,C(11,22,40,250));
   DrawRectangleRoundedLines(rect(ui.panel),.045f,16,C(91,216,197,145));
   DrawText("EVOLUTION",(int)ui.panel.x+28,(int)ui.panel.y+25,13,C(107,228,206));
   DrawText("CHOOSE YOUR NEXT THREAD",(int)ui.panel.x+28,(int)ui.panel.y+48,28,C(226,255,247));
   DrawText(TextFormat("%llu credit(s) pending - collect sparks to earn more",(unsigned long long)rewards.pendingCredits()),(int)ui.panel.x+28,(int)ui.panel.y+87,14,C(162,193,204));
   constexpr const char* roman[]={"-","I","II","III","IV","V"};
   auto wrapped=[&](const std::string& text,float x,float y,float width,int size,Color color){
    std::string line;size_t start=0;
    while(start<text.size()){
     const auto end=text.find(' ',start);const auto word=text.substr(start,end==std::string::npos?end:end-start);
     const auto candidate=line.empty()?word:line+" "+word;
     if(!line.empty()&&MeasureText(candidate.c_str(),size)>width){DrawText(line.c_str(),(int)x,(int)y,size,color);y+=size+7;line=word;}else line=candidate;
     if(end==std::string::npos)break;start=end+1;
    }
    if(!line.empty())DrawText(line.c_str(),(int)x,(int)y,size,color);
   };
   for(int i=0;i<offers.current.count;i++){
    const auto r=ui.cards[i];const auto id=offers.current.cards[i];const int current=upgrades::rank(ranks,id),cap=upgrades::maximum(id);
    DrawRectangleRounded(rect(r),.06f,8,choice==i?C(19,55,64,245):C(15,32,48,245));
    DrawRectangleRoundedLines(rect(r),.06f,8,choice==i?C(133,255,221,230):C(67,115,132,125));
    DrawText(TextFormat("0%d",i+1),(int)r.x+16,(int)r.y+15,14,C(255,218,135));
    wrapped(upgrades::name(id),r.x+16,r.y+43,r.width-32,17,C(212,249,231));
    DrawText(cap?TextFormat("RANK %s -> %s / %s",roman[current],roman[current+1],roman[cap]):"REPEATABLE RECOVERY",(int)r.x+16,(int)r.y+96,12,C(137,203,203));
    for(int j=0;j<cap;j++)DrawRectangle((int)r.x+16+j*22,(int)r.y+119,16,3,j<current?C(107,228,206):j==current?C(255,213,131):C(53,83,97));
    wrapped(upgrades::effectText(id,ranks,health),r.x+16,r.y+144,r.width-32,15,C(172,211,211));
    if(cap&&current+1==cap)DrawText("MAX RANK NEXT",(int)r.x+16,(int)r.y+216,11,C(255,218,135));
   }
   if(offers.current.count==0){
    DrawText("ALL AVAILABLE RANKS COMPLETE",(int)ui.panel.x+28,(int)ui.panel.y+166,20,C(212,249,231));
    DrawText("No useful recovery is available. Discard this unused credit to continue.",(int)ui.panel.x+28,(int)ui.panel.y+210,15,C(172,211,211));
   }
   drawButton(rect(ui.confirm),offers.current.count?"CONFIRM EVOLUTION":"DISCARD CREDIT",true,hovered(rect(ui.confirm)),13);
   DrawText("1 / 2 / 3 SELECT    LEFT / RIGHT CYCLE    ENTER CONFIRMS",(int)ui.panel.x+28,(int)(ui.panel.y+ui.panel.height-20),11,C(108,167,178));
  }
  if(mode==Mode::Lost||mode==Mode::Won){panel(mode==Mode::Won?"CHAPTER I PROTOTYPE COMPLETE":"THE THREAD GOES QUIET",mode==Mode::Won?"ROOTWARDEN DEFEATED":"LOST TO THE BRIAR",mode==Mode::Won?"Chapter I Prototype Complete - Chapters II-V are not implemented.":"The hunters remember your path.");DrawText(TextFormat("Score %06llu   Kills %llu   Evolutions %llu",(unsigned long long)rewards.score(),(unsigned long long)rewards.counters().killed,(unsigned long long)rewards.counters().creditsResolved),SW/2-255,SH/2+24,18,C(179,225,216));DrawText("ENTER / R  —  weave again",SW/2-255,SH/2+76,16,C(255,218,135));DrawText("ESC  —  return to title",SW/2-255,SH/2+111,15,C(136,188,192));}
  if(mode==Mode::Won)DrawText(TextFormat("Unspent credits %llu   Uncollected spark value %llu",(unsigned long long)rewards.pendingCredits(),
   (unsigned long long)(rewards.counters().sparksCreated-rewards.counters().sparksCollected-rewards.counters().sparksExpired)),SW/2-255,SH/2+150,14,C(137,203,203));
  if(appSettings.showFps)
   DrawText(TextFormat("FPS %d",(int)std::lround(measuredFps)),8,5,10,Fade(C(174,222,228),.32f));
  sfx.update();
  EndDrawing();
 }
 settingsEdits.dragging=false;
 saveSettings();
 sfx.shutdown();
 if(audioReady)CloseAudioDevice();
 CloseWindow();return 0;
}
