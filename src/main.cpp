#include "raylib.h"
#include "rlgl.h"
#include "game_logic.h"
#include "game_layout.h"
#include "app_settings.h"
#include "cosmetic_palette.h"
#include "menu_state.h"
#include "game_timing.h"
#include "game_presentation.h"
#include "game_audio.h"
#include "visual_feedback.h"
#include "visual_effects.h"
#include "player_upgrades.h"
#include "upgrade_layout.h"
#include "upgrade_text_cache.h"
#include "chapter_one.h"
#include "rootwarden.h"
#include "chapter_hud.h"
#include "afterimage.h"
#include "signature_reward.h"
#include "attack_coordinator.h"
#include "chapter_boss.h"
#include "briar_caller.h"
#include "crossweave.h"
#include "cocoon.h"
#include "resonant_loom.h"
#include "signature_catalog.h"
#include "signature_effects.h"
#include "silk_utilities.h"
#include "needle_interception.h"
#include "bestiary.h"
#include "silk_collision.h"
#include "performance_metrics.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include <charconv>
#include <random>
#include <string_view>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

#if defined(_WIN32)
extern "C" unsigned int __stdcall timeBeginPeriod(unsigned int);
extern "C" unsigned int __stdcall timeEndPeriod(unsigned int);
#endif

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
const char* modeName(threadlight::Mode mode){
 using threadlight::Mode;
 switch(mode){
 case Mode::Title:return "title";case Mode::Play:return "play";case Mode::Pause:return "pause";
 case Mode::Settings:return "settings";case Mode::ExitConfirm:return "exit_confirm";
 case Mode::Upgrade:return "upgrade";case Mode::Signature:return "signature";
 case Mode::Lost:return "lost";case Mode::Won:return "won";
 }
 return "unknown";
}
}
int main(int argc, char** argv){
 using PerfClock=std::chrono::steady_clock;
 const auto bootTime=PerfClock::now();
 bool m1Telemetry=false;
 bool m6RenderCheck=false;
 bool pacingCheck=false;
 bool settingsRenderCheck=false;
 int personalizationRenderCheck=-1; // 0 locked, 1 unlocked cyan, 2 unlocked violet.
 int pacingRate=60,pacingVsync=-1;
 int bossFixture=-1; // Explicit development-only CLI fixture; never a campaign checkpoint.
 int waveFixture=-1;
 int waveFixtureWave=7;
 int bossFixtureWave=6;
 int fixtureOptions=0;
 int rewardFixture=0;bool utilityFixture=false,minimumQaWindow=false;
 std::uint32_t spawnSeed=7382;
 std::string perfLogPath;
 for(int i=1;i<argc;i++){
  const std::string_view arg=argv[i];
   if(arg=="--perf-log"&&i+1<argc){if(!perfLogPath.empty())return 2;perfLogPath=argv[++i];if(perfLogPath.empty())return 2;}
   else if(arg=="--settings-render-check"){
    if(++fixtureOptions>1)return 2;
    settingsRenderCheck=true;m6RenderCheck=true;utilityFixture=true;waveFixture=4;waveFixtureWave=7;
   }else if(arg=="--personalization-render-check"&&i+1<argc){
    if(++fixtureOptions>1)return 2;
    const std::string_view value=argv[++i];
    if(value=="locked")personalizationRenderCheck=0;
    else if(value=="cyan")personalizationRenderCheck=1;
    else if(value=="violet")personalizationRenderCheck=2;
    else return 2;
    m6RenderCheck=true;utilityFixture=true;waveFixture=4;waveFixtureWave=7;
   }else if(arg=="--pacing-check"&&i+1<argc){
    if(++fixtureOptions>1)return 2;
    const std::string_view text=argv[++i];const auto parsed=std::from_chars(text.data(),text.data()+text.size(),pacingRate);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||(pacingRate!=30&&pacingRate!=60&&pacingRate!=120&&pacingRate!=240))return 2;
    pacingCheck=true;utilityFixture=true;waveFixture=4;waveFixtureWave=7;
   }else if(arg=="--pacing-vsync"&&i+1<argc){
    if(pacingVsync!=-1)return 2;
    const std::string_view value=argv[++i];if(value=="on")pacingVsync=1;else if(value=="off")pacingVsync=0;else return 2;
   }
   else if(arg=="--m6-render-check"||arg=="--m7-render-check")m6RenderCheck=true;
  else if(arg=="--m7-min-window")minimumQaWindow=true;
  else if(arg=="--m7-utility-fixture"){
   if(++fixtureOptions>1)return 2;utilityFixture=true;waveFixture=4;waveFixtureWave=7;
  }else if(arg=="--m7-reward-fixture"&&i+1<argc){
   if(++fixtureOptions>1)return 2;
   const std::string_view value=argv[++i];const auto parsed=std::from_chars(value.data(),value.data()+value.size(),rewardFixture);
   if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||rewardFixture<1||rewardFixture>4)return 2;
   bossFixture=4;bossFixtureWave=rewardFixture*7-1;
  }
  else if((arg=="--m1-telemetry"||arg=="--m2-telemetry"||arg=="--m3-telemetry"||arg=="--m4-telemetry"||arg=="--m5-telemetry"||arg=="--m6-telemetry"||arg=="--m7-telemetry"))m1Telemetry=true;
  else if((arg=="--m1-seed"||arg=="--m2-seed"||arg=="--m3-seed"||arg=="--m4-seed"||arg=="--m5-seed"||arg=="--m6-seed"||arg=="--m7-seed")&&i+1<argc){
   const std::string_view text=argv[++i];std::uint32_t parsed=0;
   const auto result=std::from_chars(text.data(),text.data()+text.size(),parsed);
   if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())return 2;
   spawnSeed=parsed;
  }else if((arg=="--m3-boss-fixture"||arg=="--m4-wave-fixture"||arg=="--m6-sower-fixture")&&i+1<argc){
   if(++fixtureOptions>1)return 2;
   const std::string_view build=argv[++i];
   constexpr std::string_view builds[]={"zero","offense","defense","mobility","mixed"};
   int selected=-1;for(int j=0;j<5;++j)if(build==builds[j])selected=j;
   if(selected<0)return 2;
   if(arg=="--m4-wave-fixture"||arg=="--m6-sower-fixture"){waveFixture=selected;waveFixtureWave=arg=="--m6-sower-fixture"?21:7;}else bossFixture=selected;
  }else if((arg=="--m5-dancer-fixture"||arg=="--m5-conductor-fixture"||arg=="--m6-hollow-fixture"||arg=="--m6-heart-fixture")&&i+1<argc){
   if(++fixtureOptions>1)return 2;
   const std::string_view build=argv[++i];constexpr std::string_view builds[]={"zero","offense","defense","mobility","mixed"};
   bossFixture=-1;for(int j=0;j<5;++j)if(build==builds[j])bossFixture=j;
   if(bossFixture<0)return 2;
   bossFixtureWave=arg=="--m5-dancer-fixture"?13:arg=="--m5-conductor-fixture"?20:arg=="--m6-hollow-fixture"?27:34;
  }else return 2;
 }
 if(bossFixture>=0&&waveFixture>=0)return 2;
 if(m6RenderCheck&&rewardFixture==0&&!utilityFixture&&(bossFixture<0||bossFixtureWave<27))return 2;
 if(minimumQaWindow&&!m6RenderCheck)return 2;
 if((pacingVsync!=-1&&!pacingCheck)||(pacingCheck&&m6RenderCheck))return 2;
 std::unique_ptr<threadlight::performance::Recorder> perf;
 if(!perfLogPath.empty()){
  perf=std::make_unique<threadlight::performance::Recorder>();
  if(!perf->open(std::filesystem::u8path(perfLogPath))){std::fprintf(stderr,"Could not create performance log (path must be new and writable).\n");return 3;}
 }
 SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT|(m6RenderCheck?FLAG_WINDOW_HIDDEN:0));
 InitWindow(threadlight::layout::kInitialScreenWidth,threadlight::layout::kInitialScreenHeight,"THREADLIGHT | A small game of living silk");
 const auto windowCoordinates=threadlight::layout::WindowCoordinates::atStartup(
     GetScreenWidth(),GetScreenHeight(),GetRenderWidth(),GetRenderHeight());
 SetWindowMinSize(windowCoordinates.nativeWidth(960),windowCoordinates.nativeHeight(640));
 if(minimumQaWindow)SetWindowSize(windowCoordinates.nativeWidth(960),windowCoordinates.nativeHeight(640));
 int SW=windowCoordinates.width(GetRenderWidth()),SH=windowCoordinates.height(GetRenderHeight());
 auto playArea=threadlight::layout::playBounds(SW,SH);
 SetExitKey(KEY_NULL); SetTargetFPS(pacingCheck?pacingRate:(m6RenderCheck?60:30)); if(!m6RenderCheck&&!pacingCheck)InitAudioDevice();
 const bool audioReady=!m6RenderCheck&&!pacingCheck&&IsAudioDeviceReady();
 threadlight::GameAudio sfx;
 if(!sfx.initialize(audioReady))TraceLog(LOG_WARNING,"Audio device unavailable; event sounds are disabled.");
 const auto settingsPath=threadlight::settings::userSettingsPath();
 auto appSettings=threadlight::settings::load(settingsPath);
 if(personalizationRenderCheck>=0){
  appSettings.violetAscendantUnlocked=personalizationRenderCheck!=0;
  appSettings.selectedPalette=personalizationRenderCheck==2?threadlight::cosmetics::PaletteId::VioletAscendant:threadlight::cosmetics::PaletteId::OriginalCyan;
 }
 if(pacingCheck){appSettings.frameRateLimit=pacingRate;if(pacingVsync!=-1)appSettings.vsync=pacingVsync==1;}
 auto applyVsync=[&](){if(appSettings.vsync)SetWindowState(FLAG_VSYNC_HINT);else ClearWindowState(FLAG_VSYNC_HINT);};
 applyVsync();
 TraceLog(LOG_INFO,"PRESENTATION vsync_requested=%d display_hz=%d pacing_check=%d requested_fps=%d",appSettings.vsync,GetMonitorRefreshRate(GetCurrentMonitor()),pacingCheck,appSettings.frameRateLimit);
 bool highResolutionTimer=false;
 auto updateTimerResolution=[&](bool capped){
#if defined(_WIN32)
  if(capped&&!highResolutionTimer)highResolutionTimer=timeBeginPeriod(1)==0;
  else if(!capped&&highResolutionTimer){timeEndPeriod(1);highResolutionTimer=false;}
#else
  (void)capped;
#endif
 };
 updateTimerResolution(!m6RenderCheck);
 sfx.setMasterVolume(appSettings.sfxVolumePercent/100.0f);
 int appliedFrameRateLimit=pacingCheck?pacingRate:(m6RenderCheck?60:30);
 using threadlight::Mode;
 Mode mode=Mode::Title,settingsReturn=Mode::Title; bool exitRequested=false; int mainSelection=0,pauseSelection=0,exitSelection=threadlight::menu::kDefaultExitSelection,settingsTab=0,settingsFocus=0;
 bool tutorial=false,tutorialMoved=false,tutorialDashed=false,inventoryOpen=false;
 bool violetUnlockedThisRun=false;
 int titleView=0,codexSelection=0;
 float tutorialSkipHold=0.f, bossPhaseNotice=0.f;
 int pauseConfirm=0,confirmSelection=1,tutorialSpawnStart=-1;
 threadlight::settings::PendingEdits settingsEdits;
 namespace foundation=threadlight::foundation;
 namespace cosmetics=threadlight::cosmetics;
 namespace battle=threadlight::combat;
 namespace upgrades=threadlight::upgrades;
 namespace skitter=threadlight::skitter;
 namespace upgradeUI=threadlight::upgrade_ui;
 namespace chapter=threadlight::chapter;
 namespace root=threadlight::chapter_boss;
 namespace chapterHud=threadlight::chapter_hud;
 namespace afterimage=threadlight::afterimage;
 namespace signatures=threadlight::signature_reward;
 namespace ranged=threadlight::ranged;
 namespace attacks=threadlight::attacks;
 namespace cross=threadlight::crossweave;
 namespace caller=threadlight::briar_caller;
 namespace field=threadlight::area_hazards;
 namespace sower=threadlight::sower;
 namespace cocoon=threadlight::cocoon;
 namespace loom=threadlight::resonant_loom;
 namespace catalog=threadlight::signature_catalog;
 namespace effects=threadlight::signature_effects;
 namespace utilities=threadlight::silk_utilities;
 namespace needle=threadlight::needle_interception;
 foundation::WaveLifecycle lifecycle;
 foundation::RewardLedger rewards;
 chapter::Schedule spawnSchedule;int spawnCursor=0;
 root::State boss;foundation::RewardId bossRewardId=0;
 signatures::RewardState signatureReward;
 signatures::RewardState crossReward;
 signatures::RewardState cocoonReward,loomReward;
 signatures::RewardState giftReward;
 catalog::State signatureCatalog;
 effects::AnchorState anchorState;
 effects::SlowState bossSlow;
 utilities::BloodstitchState bloodstitch;
 utilities::NeedlecatchState needlecatch;
 foundation::Point needleFlash{};int needleFlashTicks=0;
 std::uint64_t needleCatches=0,anchorProcs=0;int bloodHealing=0;
 std::array<int,4> runDamageTaken{};
 cocoon::State cocoonState;
 loom::State loomState;
 loom::Target loomSource{};bool loomPending=false,finalBossDefeated=false;
 field::Pool ground;
 cross::State crossState;
 cross::Pulse crossPulse;
 foundation::Point pulseCenter{};int pulseTicks=0;
 float pulseRadius=cross::kPulseRadius;
 std::uint64_t cocoonsFormed=0,loomBursts=0;int barrierAbsorbed=0,loomDamage=0;
 std::uint64_t nextThreadId=0,crossPulses=0,crossHits=0;int crossDamage=0;
 int supportQueued=0,supportSpawned=0;
 afterimage::State offensiveEcho;
 ranged::Pool projectiles;
 attacks::Coordinator attackCoordinator;
 std::uint64_t echoCount=0,echoHitCount=0;int echoDamage=0;
 auto bossArena=[&](){return root::Arena{playArea.left,playArea.right,playArea.top,playArea.bottom};};
 std::uint64_t simulationTick=0;
 battle::PlayerHealth health;
 upgrades::Ranks ranks;
 upgrades::Stats stats=upgrades::derive(ranks);
 upgrades::OfferState offers;offers.rng=spawnSeed^0x9e3779b9u;
 V player{(playArea.left+playArea.right)*.5f,(playArea.top+playArea.bottom)*.5f};
 float dashCd=0,dashCycleDuration=1.85f,dashTime=0,shake=0,pulse=0,damageFeedback=0;
 int wave=0,choice=0;
 auto currentSignature=[&]() -> signatures::RewardState& {return giftReward;};
 const float radius=kPlayerRadius;
 bool dashVisualActive=false,playerWasMoving=false;
 float dashEchoTimer=0,dashEchoFade=0; size_t dashEchoCount=0; std::array<V,6> dashEcho{};
 std::vector<Thread> thread; std::vector<Foe> foes; std::vector<Spark> sparks; std::vector<Particle> particles; std::vector<DeathEffect> deathEffects; std::vector<V> stars;
 // One extra slot allows append-before-trim without growing the bounded trail.
 thread.reserve(521); foes.reserve(80); sparks.reserve(48); particles.reserve(threadlight::visual::kParticleLimit); deathEffects.reserve(threadlight::visual::kDeathEffectLimit); stars.reserve(110);
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
 threadlight::timing::MovementPressBuffer movementPressPending;
 threadlight::menu::FocusOwner menuFocus;
 float measuredFps=60.0f,lastFrameSeconds=0.0f;
 upgradeUI::OfferTextCache offerTextCache;
 auto saveSettings=[&](){
  if(pacingCheck)return; // A diagnostic run never writes the owner's profile.
  sfx.setMasterVolume(appSettings.sfxVolumePercent/100.0f);
  if(!settingsEdits.flush([&](){return threadlight::settings::save(settingsPath,appSettings);}))
   TraceLog(LOG_WARNING,"Could not save ThreadLight settings to the user config directory.");
 };
 auto burst=[&](V p,Color col,int n,float force){for(int i=0;i<n&&particles.size()<threadlight::visual::kParticleLimit;i++){float a=(float)GetRandomValue(0,6283)/1000.f,s=(float)GetRandomValue(25,(int)force);particles.push_back({p,{std::cos(a)*s,std::sin(a)*s},.65f,.65f,(float)GetRandomValue(2,6),col});}};
 auto addDeathEffect=[&](const Foe& foe){
  if(deathEffects.size()>=threadlight::visual::kDeathEffectLimit)deathEffects.erase(deathEffects.begin());
  const bool heavy=foe.kind==battle::EnemyKind::Brute||
   (foe.kind==battle::EnemyKind::BriarCaller&&foe.briarCaller.members>=4);
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
  if(boss.kind!=root::Kind::Rootwarden)TraceLog(LOG_INFO,"M5 boss=%s event=%s ticks=%llu hits=%llu exposure=%llu silk=%d signature=%d shots=%llu buds_cut=%llu support_spawned=%d queued=%d",
   root::name(boss),event,(unsigned long long)m.ticks,(unsigned long long)m.hits,(unsigned long long)m.exposedHits,m.silkDamage,m.signatureDamage,(unsigned long long)boss.shotsFired,(unsigned long long)boss.budsCut,supportSpawned,supportQueued);
  TraceLog(LOG_INFO,"M3 boss event=%s seed=%u tick=%llu stage=%s phase=%d hp=%d duration_ticks=%llu hits=%llu exposure_hits=%llu contact_ticks=%llu cycles=%llu dealt=%d player_damage=%d supports=%d",
   event,spawnSeed,(unsigned long long)simulationTick,root::stageName(boss.stage),boss.phase,boss.health.hp,
   (unsigned long long)m.ticks,(unsigned long long)m.hits,(unsigned long long)m.exposedHits,
   (unsigned long long)m.contactTicks,(unsigned long long)m.cycles,m.silkDamage,m.playerDamage,boss.supportsReleased);
 };
 auto unlockCodex=[&](int index){
  if(bossFixture>=0||waveFixture>=0||m6RenderCheck)return;
  const int bit=1<<index;if(!(appSettings.codexSeen&bit)){appSettings.codexSeen|=bit;settingsEdits.dirty=true;}
 };
 auto startEncounter=[&](){
  const auto encounter=chapter::encounter(wave);
  giftReward={};anchorState={};bossSlow={};bloodstitch={};needlecatch={};needleFlashTicks=0;
  lifecycle.start({std::max(1,encounter.seconds*foundation::kTicksPerSecond),encounter.boss});
  spawnSchedule=chapter::schedule(wave,spawnSeed);spawnCursor=0;
  thread.clear();cross::clear(crossState);cocoon::clear(cocoonState);loom::clear(loomState);loomPending=false;ground.clear();crossPulse={};pulseTicks=0;afterimage::clear(offensiveEcho);projectiles.clear();attackCoordinator.clear();dashTime=0;dashVisualActive=false;dashEchoCount=0;dashEchoFade=0;
  if(encounter.boss){
   const V center{(playArea.left+playArea.right)*.5f,(playArea.top+playArea.bottom)*.5f};
   const V direction=len(center-player)>1?norm(center-player):V{1,0};
   const V start=dist(center,player)>220?center:player+direction*230.f;
   supportQueued=supportSpawned=0;
   root::start(boss,{start.x,start.y},bossArena(),wave==13?root::Kind::Dancer:wave==20?root::Kind::Conductor:wave==27?root::Kind::Hollow:wave==34?root::Kind::Heart:root::Kind::Rootwarden);
   unlockCodex(6+static_cast<int>(boss.kind));
   bossRewardId=rewards.registerEnemy(root::kCoreValue,250);boss.hazardOwner=bossRewardId;traceBoss("start");
  }
  traceM1("encounter_start");
 };
 auto reset=[&](){
  violetUnlockedThisRun=false;
  tutorial=false;inventoryOpen=false;pauseConfirm=0;bossPhaseNotice=0;runDamageTaken={};
  player={(playArea.left+playArea.right)*.5f,(playArea.top+playArea.bottom)*.5f};
  health={};playerWasMoving=false;ranks={};
  const int fixture=waveFixture>=0?waveFixture:bossFixture;
  if(fixture==1)ranks.value[upgrades::index(upgrades::Id::Razor)]=3;
  if(fixture==2)ranks.value[upgrades::index(upgrades::Id::Heartwood)]=3;
  if(fixture==3){ranks.value[upgrades::index(upgrades::Id::Quick)]=2;ranks.value[upgrades::index(upgrades::Id::Rhythm)]=1;}
  if(fixture==4){ranks.value[upgrades::index(upgrades::Id::Razor)]=1;ranks.value[upgrades::index(upgrades::Id::Living)]=1;ranks.value[upgrades::index(upgrades::Id::Heartwood)]=1;}
  if(waveFixture>0)ranks.value[upgrades::index(fixture==3?upgrades::Id::Rhythm:upgrades::Id::Living)]+=1;
  if(bossFixture>=0&&bossFixtureWave>=13){
   ranks={};const int budget=bossFixtureWave==13?6:10;
   if(fixture==1){ranks.value[upgrades::index(upgrades::Id::Razor)]=bossFixtureWave==13?3:5;ranks.value[upgrades::index(upgrades::Id::Living)]=2;ranks.value[upgrades::index(upgrades::Id::Broad)]=budget-(bossFixtureWave==13?5:7);}
   if(fixture==2){ranks.value[upgrades::index(upgrades::Id::Heartwood)]=bossFixtureWave==13?3:5;ranks.value[upgrades::index(upgrades::Id::Living)]=bossFixtureWave==13?2:3;ranks.value[upgrades::index(upgrades::Id::Broad)]=bossFixtureWave==13?1:2;}
   if(fixture==3){ranks.value[upgrades::index(upgrades::Id::Quick)]=bossFixtureWave==13?3:5;ranks.value[upgrades::index(upgrades::Id::Rhythm)]=bossFixtureWave==13?2:3;ranks.value[upgrades::index(upgrades::Id::Living)]=bossFixtureWave==13?1:2;}
   if(fixture==4){ranks.value={bossFixtureWave==13?2:3,2,1,0,0,bossFixtureWave==13?1:4,0};}
  }
  if((bossFixture>=0&&bossFixtureWave>=27)||(waveFixture>=0&&waveFixtureWave==21)){
   ranks={};int remaining=fixture==0?0:bossFixtureWave==34&&bossFixture>=0?16:12;
   constexpr upgrades::Id priority[4][7]={
    {upgrades::Id::Razor,upgrades::Id::Living,upgrades::Id::Broad,upgrades::Id::Quick,upgrades::Id::Heartwood,upgrades::Id::Rhythm,upgrades::Id::Magnet},
    {upgrades::Id::Heartwood,upgrades::Id::Living,upgrades::Id::Broad,upgrades::Id::Rhythm,upgrades::Id::Quick,upgrades::Id::Magnet,upgrades::Id::Razor},
    {upgrades::Id::Quick,upgrades::Id::Rhythm,upgrades::Id::Living,upgrades::Id::Broad,upgrades::Id::Heartwood,upgrades::Id::Magnet,upgrades::Id::Razor},
    {upgrades::Id::Razor,upgrades::Id::Living,upgrades::Id::Heartwood,upgrades::Id::Quick,upgrades::Id::Broad,upgrades::Id::Rhythm,upgrades::Id::Magnet}};
   const auto& order=priority[std::clamp(fixture-1,0,3)];
   while(remaining>0)for(auto id:order){
    auto& value=ranks.value[upgrades::index(id)];
    if(fixture==4){if(remaining&&value<upgrades::maximum(id)){++value;--remaining;}}
    else while(remaining&&value<upgrades::maximum(id)){++value;--remaining;}
   }
  }
  if(utilityFixture){ranks={};ranks.value[upgrades::index(upgrades::Id::Needlecatch)]=3;ranks.value[upgrades::index(upgrades::Id::Bloodstitch)]=3;ranks.value[upgrades::index(upgrades::Id::Razor)]=1;ranks.value[upgrades::index(upgrades::Id::Living)]=1;}
  stats=upgrades::derive(ranks);health.hp=health.maxHp=stats.maxHp;offers={};offers.rng=spawnSeed^0x9e3779b9u;
  if(utilityFixture)health.hp=5500; // Explicit injured utility QA fixture only.
  dashCd=dashTime=shake=pulse=damageFeedback=0;wave=waveFixture>=0?waveFixtureWave:bossFixture<0?0:bossFixtureWave;choice=0;simulationTick=0;
  rewards=foundation::RewardLedger{};signatureReward={};signatureCatalog={};signatureCatalog.rng=spawnSeed^0x51f15e5du;giftReward={};offensiveEcho={};projectiles={};attackCoordinator.clear();echoCount=echoHitCount=0;echoDamage=0;boss={};bossRewardId=0;dashCycleDuration=stats.dashCooldownSeconds;dashVisualActive=false;
  if(waveFixture>=0)signatureReward.owned=true; // Explicit QA fixture, never normal progression.
  crossReward={};crossState={};nextThreadId=crossPulses=crossHits=0;crossDamage=0;pulseTicks=0;
  cocoonReward={};loomReward={};cocoonState={};loomState={};loomPending=finalBossDefeated=false;ground.clear();cocoonsFormed=loomBursts=0;barrierAbsorbed=loomDamage=0;
  if(bossFixture>=0&&bossFixtureWave>=13)signatureReward.owned=true;
  if(bossFixture>=0&&bossFixtureWave>=20)crossReward.owned=true;
  if((bossFixture>=0&&bossFixtureWave>=27)||(waveFixture>=0&&waveFixtureWave==21)){signatureReward.owned=crossReward.owned=cocoonReward.owned=true;}
  if(bossFixture>=0&&bossFixtureWave==34)loomReward.owned=true;
  signatureCatalog.owned[catalog::index(catalog::Id::Afterimage)]=signatureReward.owned;
  signatureCatalog.owned[catalog::index(catalog::Id::Crossweave)]=crossReward.owned;
  signatureCatalog.owned[catalog::index(catalog::Id::Cocoon)]=cocoonReward.owned;
  signatureCatalog.owned[catalog::index(catalog::Id::ResonantLoom)]=loomReward.owned;
  needleCatches=anchorProcs=0;bloodHealing=0;
  dashEchoTimer=dashEchoFade=0;dashEchoCount=0;dashEcho.fill(player);
  thread.clear();foes.clear();sparks.clear();particles.clear();deathEffects.clear();
  stepClock.clear();dashPressPending=collectionContinuePending=false;mode=Mode::Play;startEncounter();
  if(rewardFixture){
   // Isolated reward/UI fixture, not an earned boss victory or checkpoint.
   root::clearThreats(boss);boss.health.hp=0;lifecycle.start({1,true,0});
   lifecycle.tick(0,true);lifecycle.tick(0,false,true,false);
   signatures::queue(giftReward,bossRewardId,true);catalog::prepare(signatureCatalog,bossRewardId);mode=Mode::Signature;
  }
 };
 auto addFoe=[&](battle::EnemyKind kind,float a){
  unlockCodex(static_cast<int>(kind));
  const auto def=battle::definition(kind,chapter::encounter(wave).chapter);const float r=def.radius;
  const float playWidth=playArea.right-playArea.left,playHeight=playArea.bottom-playArea.top;
  const float padding=r+5.f;
  auto spawn=threadlight::chooseSafeSpawn(a,player.x-playArea.left,player.y-playArea.top,playWidth,playHeight,padding,std::max(padding,playWidth*.53f),std::max(padding,playHeight*.53f),radius+r+70.f);
  V p{spawn.x+playArea.left,spawn.y+playArea.top};
  foes.push_back({p,{},r,{def.hp,def.hp},def.speed,a,0,kind});
  auto& foe=foes.back();foe.rewardId=rewards.registerEnemy(def.sparkValue,def.killScore);
  if(kind==battle::EnemyKind::Sower)sower::start(foe.sowerState,foe.rewardId,sower::configForChapter(chapter::encounter(wave).chapter));
  foe.skitterState.side=(foe.rewardId%2)?1:-1;
  traceM1("spawn",foe.rewardId,def.sparkValue);
  if(m1Telemetry)TraceLog(LOG_INFO,"M3 enemy id=%llu kind=%d hp=%d x=%.1f y=%.1f",(unsigned long long)foe.rewardId,(int)kind,def.hp,p.x,p.y);
 };
 auto mergeBriarCallers=[&](){
  const int chapterNumber=chapter::encounter(wave).chapter;
  for(size_t i=0;i<foes.size();++i){
   auto& keeper=foes[i];
   if(!keeper.health.alive()||keeper.kind!=battle::EnemyKind::BriarCaller)continue;
   for(size_t j=i+1;j<foes.size();++j){
    auto& absorbed=foes[j];
    if(!absorbed.health.alive()||absorbed.kind!=battle::EnemyKind::BriarCaller||
       !caller::readyToFuse({keeper.p.x,keeper.p.y},keeper.briarCaller,
                            {absorbed.p.x,absorbed.p.y},absorbed.briarCaller))continue;
    const int members=caller::fusedMembers(keeper.briarCaller,absorbed.briarCaller);
    const auto definition=battle::definition(battle::EnemyKind::BriarCaller,chapterNumber);
    const int rewardSparkValue=definition.sparkValue*members;
    const int rewardScore=caller::scale(definition.killScore,members);
    if(!rewards.mergeAliveEnemies(keeper.rewardId,absorbed.rewardId,
                                  rewardSparkValue,rewardScore))continue;

    const int maxHp=caller::scale(definition.hp,members);
    const int hp=caller::fusedHealth(keeper.health.hp,keeper.health.maxHp,keeper.briarCaller,
                                    absorbed.health.hp,absorbed.health.maxHp,
                                    absorbed.briarCaller,maxHp);
    keeper.health.maxHp=maxHp;
    keeper.health.hp=hp;
    keeper.r=caller::scale(definition.radius,members);
    keeper.briarCaller.members=members;
    if(anchorState.targetId==absorbed.rewardId){anchorState.targetId=0;anchorState.windowTicks=0;}
    absorbed.health.hp=0;
    burst(keeper.p,C(189,231,139),14,105.f);
    traceM1("briar_caller_merge",keeper.rewardId,members);
   }
  }
 };
 auto releaseSupports=[&](int count){
  if(boss.kind!=root::Kind::Rootwarden){supportQueued+=count;return;}
  root::visitSupportBatch(boss,count,[&](battle::EnemyKind supportKind,int offset,int){
   addFoe(supportKind,chapter::kPi*.25f+chapter::kPi*(boss.supportsReleased+offset)*.5f);
  });
  if(count)traceBoss("supports");
 };
 auto budHit=[&](int index,int damage,bool ordinary){
  if(index<0||index>=boss.budCount)return battle::SilkHit::None;
  const auto p=boss.buds[index].p;const auto hit=root::damageBud(boss,index,damage,ordinary);
  if(hit!=battle::SilkHit::None){burst({p.x,p.y},C(168,230,211),hit==battle::SilkHit::Killed?6:3,70);sfx.play(threadlight::audio::Event::EnemyHit);}
  if(hit==battle::SilkHit::Killed)traceBoss("bud_cut");
  return hit;
 };
 auto foeHit=[&](Foe& f,battle::SilkHit hit,bool echo){
  if(hit==battle::SilkHit::None)return;
  if(!echo&&loom::onHit(loomState,loomReward.owned,true,loom::HitKind::OrdinarySilk)){
   loomPending=true;loomSource={f.rewardId,{f.p.x,f.p.y},0,0};
  }
  f.hitFlash=threadlight::visual::kEnemyHitFlashSeconds;
  burst(f.p,echo?C(183,232,255,225):C(83,255,226,220),echo?6:4,echo?90:70);
  if(hit==battle::SilkHit::Killed){
   if(anchorState.targetId==f.rewardId){anchorState.targetId=0;anchorState.windowTicks=0;}
   if(f.kind==battle::EnemyKind::Sower)ground.cancelOwner(f.rewardId);
   projectiles.cancelReservation(f.projectileTicket);f.projectileTicket=0;
   if(const auto drop=rewards.defeatEnemy(f.rewardId)){
    if(!echo){
     const int healed=utilities::onOrdinaryKill(bloodstitch,upgrades::rank(ranks,upgrades::Id::Bloodstitch),health);
     if(healed){bloodHealing+=healed;burst(player,C(224,163,176),6,65);traceM1("bloodstitch_heal",drop->id,healed);}
    }
    V p{threadlight::layout::clampPlayerX(f.p.x,playArea),threadlight::layout::clampPlayerY(f.p.y,playArea)};
    Spark spark{p,0};spark.pickup.id=drop->id;sparks.push_back(spark);
    addDeathEffect(f);shake=std::max(shake,(f.kind==battle::EnemyKind::Brute||
     (f.kind==battle::EnemyKind::BriarCaller&&f.briarCaller.members>=4))?.105f:.045f);
    sfx.play(threadlight::audio::Event::EnemyDeath);traceM1("kill",drop->id,drop->sparkValue);
   }
  }else sfx.play(threadlight::audio::Event::EnemyHit);
 };
 auto bossHit=[&](const root::DamageResult& hit,bool ordinary=false){
  if(hit.hit==battle::SilkHit::None)return;
  burst({boss.p.x,boss.p.y},C(194,255,227),4,80);traceBoss("silk_or_echo_hit");
  if(hit.events.phaseChanged){bossPhaseNotice=2.5f;sfx.play(threadlight::audio::Event::BossPhase);releaseSupports(hit.events.supports);traceBoss("phase_two");}
  if(hit.events.defeated){
   anchorState.targetId=0;anchorState.windowTicks=0;
   for(const auto& foe:foes)if(rewards.despawnEnemy(foe.rewardId))traceM1("boss_support_removal",foe.rewardId);
   foes.clear();projectiles.clear();ground.clear();attackCoordinator.clear();
   if(wave==34){rewards.defeatEnemyWithoutPickup(bossRewardId);finalBossDefeated=true;loomPending=false;}
   else if(const auto drop=rewards.defeatEnemy(bossRewardId)){
    Spark core{{threadlight::layout::clampPlayerX(boss.p.x,playArea),threadlight::layout::clampPlayerY(boss.p.y,playArea)},0};
    core.pickup.id=drop->id;core.bossCore=true;sparks.push_back(core);traceM1("boss_core_created",drop->id,drop->sparkValue);
   }
   supportQueued=0;
   auto& reward=currentSignature();
   if(wave<34&&signatures::queue(reward,bossRewardId,health.alive()))traceM1("signature_queued",bossRewardId);
   if(ordinary){const int healed=utilities::onOrdinaryKill(bloodstitch,upgrades::rank(ranks,upgrades::Id::Bloodstitch),health);bloodHealing+=healed;if(healed)traceM1("bloodstitch_heal",bossRewardId,healed);}
   burst({boss.p.x,boss.p.y},C(255,214,142),32,180);sfx.play(threadlight::audio::Event::BossPhase);traceBoss("defeated");
  }else sfx.play(threadlight::audio::Event::EnemyHit);
 };
 auto finishEncounter=[&](){
  if(!health.alive())return;
  if(chapter::encounter(wave).boss&&currentSignature().pending&&rewards.pendingCredits()==0){
   catalog::prepare(signatureCatalog,currentSignature().source);
   mode=Mode::Signature;choice=0;traceM1("signature_offer",currentSignature().source);return;
  }
  if(!lifecycle.finishRewards(rewards.pendingCredits()!=0))return;
  traceM1("encounter_complete");
  ++wave;startEncounter();
 };
  auto drawText=[&](const char* s,int x,int y,int size,Color c){DrawText(s,x,y,size,c);};
  auto paletteColor=[](cosmetics::Rgba color){return C(color.r,color.g,color.b,color.a);};
  const auto& playerPalette=cosmetics::palette(cosmetics::normalizePalette(
      static_cast<int>(appSettings.selectedPalette),appSettings.violetAscendantUnlocked));
  auto applyMenuAction=[&](threadlight::menu::Action action){threadlight::menu::apply(mode,settingsReturn,exitRequested,exitSelection,action);};
  auto changeFrameLimit=[&](int value){if(!threadlight::settings::validFrameRateLimit(value)||value==appSettings.frameRateLimit)return;appSettings.frameRateLimit=value;settingsEdits.dirty=true;saveSettings();};
  auto changeShowFps=[&](){appSettings.showFps=!appSettings.showFps;if(appSettings.showFps)measuredFps=threadlight::timing::initialMeasuredFps(lastFrameSeconds);settingsEdits.dirty=true;saveSettings();};
  auto changeVsync=[&](){appSettings.vsync=!appSettings.vsync;applyVsync();settingsEdits.dirty=true;saveSettings();};
  auto changeSfxVolume=[&](int value,bool persist=true){value=std::clamp(value,0,100);if(value==appSettings.sfxVolumePercent)return;appSettings.sfxVolumePercent=value;settingsEdits.dirty=true;sfx.setMasterVolume(value/100.0f);if(persist)saveSettings();};
  auto changePalette=[&](cosmetics::PaletteId palette){if(appSettings.selectedPalette==palette)return;if(!threadlight::settings::selectPalette(appSettings,palette))return;settingsEdits.dirty=true;saveSettings();};
  auto contains=[](Rectangle rectangle,Vector2 point){return CheckCollisionPointRec(point,rectangle);};
  auto buttonRect=[&](int index,int count,float centerY,float width=300.f,float height=46.f,float gap=54.f){return Rectangle{(SW-width)*.5f,centerY+(index-(count-1)*.5f)*gap-height*.5f,width,height};};
  auto mainButton=[&](int index){return buttonRect(index,5,SH*.5f+86.f,310.f,38.f,43.f);};
  auto codexButton=[&](int index){return Rectangle{SW*.5f-390.f,SH*.5f-205.f+index*29.f,240.f,26.f};};
  auto titleBack=[&](){return Rectangle{SW*.5f-90.f,SH*.5f+190.f,180.f,38.f};};
  auto pauseButton=[&](int index){return buttonRect(index,5,SH*.5f+72.f,330.f,38.f,43.f);};
  auto confirmButton=[&](int index){return Rectangle{SW*.5f-160.f+index*165.f,SH*.5f+25.f,155.f,42.f};};
  auto startGame=[&](){
   reset();
   if(!appSettings.tutorialCompleted&&bossFixture<0&&waveFixture<0){
    tutorial=true;tutorialMoved=tutorialDashed=false;tutorialSkipHold=0.f;tutorialSpawnStart=-1;
    lifecycle.start({24*60*60*foundation::kTicksPerSecond,false});
    spawnSchedule={};spawnSchedule.count=10;
    for(int i=0;i<10;++i)spawnSchedule.items[i]={i*180,battle::EnemyKind::Hunter,(i%2)*chapter::kPi};
    spawnCursor=0;
   }
  };
  auto finishTutorial=[&](){
   tutorial=false;appSettings.tutorialCompleted=true;settingsEdits.dirty=true;saveSettings();reset();
  };

  int loggedWidth=0,loggedHeight=0,loggedRenderWidth=0,loggedRenderHeight=0;
  Vector2 loggedDpi{0.0f,0.0f};
  int renderCheckFrames=0;bool capturedWarning=false,capturedActive=false;
   auto previousPerfFrame=PerfClock::time_point{};
   auto lastMemorySample=PerfClock::time_point{};
   threadlight::performance::MemorySample memorySample{};
   std::uint64_t perfFrameNumber=0;
  if(m6RenderCheck||pacingCheck)reset();
  if(settingsRenderCheck)mode=Mode::Settings;
  if(personalizationRenderCheck>=0){mode=Mode::Settings;settingsTab=2;}
  const auto pacingStart=PerfClock::now();
  while(!WindowShouldClose()&&!exitRequested){
    if(pacingCheck&&PerfClock::now()-pacingStart>=std::chrono::seconds(5))break;
    const auto frameStart=perf?PerfClock::now():PerfClock::time_point{};
    const double frameQpcMs=perf?threadlight::performance::queryClockMilliseconds():0;
    double simulationMs=0,discardedMs=0;int frameSteps=0;
   const Mode screenAtFrameStart=mode;
   const double frameSeconds=std::max(0.0,static_cast<double>(GetFrameTime()));
   lastFrameSeconds=static_cast<float>(frameSeconds);
   if(appSettings.showFps)measuredFps=threadlight::timing::smoothMeasuredFps(measuredFps,lastFrameSeconds);
   const int nextSW=windowCoordinates.width(GetRenderWidth()),nextSH=windowCoordinates.height(GetRenderHeight());
   const auto nextPlayArea=threadlight::layout::playBounds(nextSW,nextSH);
   const bool resized=nextSW!=SW||nextSH!=SH;
   if(resized){
    auto remapWorldPoint=[&](V& point){const auto mapped=threadlight::layout::remapPoint({point.x,point.y},playArea,nextPlayArea);point={mapped.x,mapped.y};};
    remapWorldPoint(player);
    {V p{pulseCenter.x,pulseCenter.y};remapWorldPoint(p);pulseCenter={p.x,p.y};}
    for(auto* detector:{&crossState,&cocoonState.detector})for(size_t i=0;i<detector->pendingCount;++i){auto& contact=detector->pendingCrossings[i];V p{contact.lastOnLine.x,contact.lastOnLine.y};remapWorldPoint(p);contact.lastOnLine={p.x,p.y};}
    for(auto& item:thread)remapWorldPoint(item.p);
    for(auto& foe:foes){
     if(foe.kind==battle::EnemyKind::Skitter){
      foundation::Point p{foe.p.x,foe.p.y},v{foe.vel.x,foe.vel.y};
      skitter::remapBounds(foe.skitterState,p,v,
       {playArea.left+foe.r,playArea.right-foe.r,playArea.top+foe.r,playArea.bottom-foe.r},
       {nextPlayArea.left+foe.r,nextPlayArea.right-foe.r,nextPlayArea.top+foe.r,nextPlayArea.bottom-foe.r});
      foe.p={p.x,p.y};foe.vel={v.x,v.y};
     }else if(foe.kind==battle::EnemyKind::Spitter){
      foundation::Point p{foe.p.x,foe.p.y},v{foe.vel.x,foe.vel.y};
      ranged::remap(foe.rangedState,p,v,
       {playArea.left+foe.r,playArea.right-foe.r,playArea.top+foe.r,playArea.bottom-foe.r},
       {nextPlayArea.left+foe.r,nextPlayArea.right-foe.r,nextPlayArea.top+foe.r,nextPlayArea.bottom-foe.r});
      foe.p={p.x,p.y};foe.vel={v.x,v.y};
     }else if(foe.kind==battle::EnemyKind::Sower){
      foundation::Point p{foe.p.x,foe.p.y},v{foe.vel.x,foe.vel.y};
      sower::remap(foe.sowerState,p,v,{playArea.left,playArea.right,playArea.top,playArea.bottom},{nextPlayArea.left,nextPlayArea.right,nextPlayArea.top,nextPlayArea.bottom});
      foe.p={p.x,p.y};foe.vel={v.x,v.y};
     }else remapWorldPoint(foe.p);
    }
    {V p{needleFlash.x,needleFlash.y};remapWorldPoint(p);needleFlash={p.x,p.y};}
    ground.remap({playArea.left,playArea.right,playArea.top,playArea.bottom},{nextPlayArea.left,nextPlayArea.right,nextPlayArea.top,nextPlayArea.bottom},boss.fields.areaOccupied({playArea.left,playArea.right,playArea.top,playArea.bottom}));
    if(chapter::encounter(wave).boss)root::remap(boss,bossArena(),{nextPlayArea.left,nextPlayArea.right,nextPlayArea.top,nextPlayArea.bottom},ground.areaOccupied({nextPlayArea.left,nextPlayArea.right,nextPlayArea.top,nextPlayArea.bottom}));
    if(root::alive(boss)&&boss.corridor.enabled)ground.cancelUnsafeCorridors(boss.corridor);
    afterimage::remap(offensiveEcho,{playArea.left,playArea.right,playArea.top,playArea.bottom},
     {nextPlayArea.left,nextPlayArea.right,nextPlayArea.top,nextPlayArea.bottom});
    projectiles.remap({playArea.left,playArea.right,playArea.top,playArea.bottom},
     {nextPlayArea.left,nextPlayArea.right,nextPlayArea.top,nextPlayArea.bottom});
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
    if(offensiveEcho.recording&&offensiveEcho.count){
     const auto last=offensiveEcho.path[offensiveEcho.count-1];
     const foundation::Point shift{player.x-last.x,player.y-last.y};
     for(size_t i=0;i<offensiveEcho.count;++i){offensiveEcho.path[i].x+=shift.x;offensiveEcho.path[i].y+=shift.y;}
     offensiveEcho.path[offensiveEcho.count-1]={player.x,player.y};
    }
    stepClock.clear();dashPressPending=collectionContinuePending=false;movementPressPending.clear();capturePresentation();
   }

   const float settingsWidth=std::min(860.f,static_cast<float>(SW-40));
   const float settingsLeft=(SW-settingsWidth)*.5f,settingsTop=SH*.5f-256.f;
   const Rectangle videoTab{settingsLeft+48.f,settingsTop+76.f,165.f,42.f};
   const Rectangle audioTab{settingsLeft+230.f,settingsTop+76.f,165.f,42.f};
   const Rectangle personalizationTab{settingsLeft+412.f,settingsTop+76.f,220.f,42.f};
   constexpr int frameRateChoiceCount=static_cast<int>(threadlight::settings::kFrameRateLimits.size());
   const float fpsChipWidth=(settingsWidth-340.f-8.f*(frameRateChoiceCount-1))/frameRateChoiceCount;
   std::array<Rectangle,frameRateChoiceCount> fpsChips{};
   for(int i=0;i<frameRateChoiceCount;i++)fpsChips[i]={settingsLeft+292.f+i*(fpsChipWidth+8.f),settingsTop+143.f,fpsChipWidth,38.f};
   const Rectangle showToggle{settingsLeft+settingsWidth-182.f,settingsTop+224.f,132.f,42.f};
   const Rectangle vsyncToggle{settingsLeft+settingsWidth-182.f,settingsTop+306.f,132.f,42.f};
   const Rectangle frameRow{settingsLeft+42.f,settingsTop+132.f,settingsWidth-84.f,64.f};
   const Rectangle volumeSlider{settingsLeft+48.f,settingsTop+207.f,settingsWidth-96.f,18.f};
   const Rectangle volumeRow{settingsLeft+42.f,settingsTop+144.f,settingsWidth-84.f,100.f};
   const float paletteCardGap=16.f;
   const float paletteCardWidth=(settingsWidth-84.f-paletteCardGap)*.5f;
   const std::array<Rectangle,2> paletteCards{{
    {settingsLeft+42.f,settingsTop+144.f,paletteCardWidth,268.f},
    {settingsLeft+42.f+paletteCardWidth+paletteCardGap,settingsTop+144.f,paletteCardWidth,268.f}
   }};
   const Rectangle settingsBack{settingsLeft+settingsWidth*.5f-125.f,settingsTop+442.f,250.f,44.f};
   const Vector2 mouse=GetMousePosition();
   const Vector2 mouseDelta=GetMouseDelta();
   const bool menuKey=IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_DOWN)||
       IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_ENTER)||
       IsKeyPressed(KEY_SPACE)||IsKeyPressed(KEY_TAB)||IsKeyPressed(KEY_ESCAPE)||
       IsKeyPressed(KEY_P)||IsKeyPressed(KEY_R)||
       ((mode==Mode::Upgrade||mode==Mode::Signature)&&(IsKeyPressed(KEY_ONE)||IsKeyPressed(KEY_TWO)||IsKeyPressed(KEY_THREE)||IsKeyPressed(KEY_A)||IsKeyPressed(KEY_D)||IsKeyPressed(KEY_S)));
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

   const bool focused=IsWindowFocused(),minimized=IsWindowMinimized();
   if(threadlight::menu::shouldAutoPause(screenAtFrameStart,focused,minimized,m6RenderCheck||pacingCheck)){
    applyMenuAction(threadlight::menu::Action::Pause);
   }else if(screenAtFrameStart==Mode::Pause&&!m6RenderCheck&&(!focused||minimized)){
    // Wait for focus and an intentional resume; do not consume stale input.
   }else if(screenAtFrameStart==Mode::Title&&titleView==1){
    if(IsKeyPressed(KEY_ESCAPE))titleView=0;
    else if(IsKeyPressed(KEY_UP))codexSelection=threadlight::menu::moveSelection(codexSelection,-1,11);
    else if(IsKeyPressed(KEY_DOWN))codexSelection=threadlight::menu::moveSelection(codexSelection,1,11);
    else if(hovered(titleBack())&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))titleView=0;
    else for(int i=0;i<11;++i)if(hovered(codexButton(i))&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))codexSelection=i;
   }else if(screenAtFrameStart==Mode::Title&&titleView==2){
    if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)||(hovered(titleBack())&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))titleView=0;
   }else if(screenAtFrameStart==Mode::Title){
    if(IsKeyPressed(KEY_ESCAPE))applyMenuAction(threadlight::menu::Action::Escape);
    else if(IsKeyPressed(KEY_UP))mainSelection=threadlight::menu::moveSelection(mainSelection,-1,5);
    else if(IsKeyPressed(KEY_DOWN))mainSelection=threadlight::menu::moveSelection(mainSelection,1,5);
    else if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){
     if(mainSelection==0)startGame();
     else if(mainSelection==1)titleView=1;
     else if(mainSelection==2)titleView=2;
     else if(mainSelection==3)applyMenuAction(threadlight::menu::Action::OpenSettings);
     else applyMenuAction(threadlight::menu::Action::RequestExit);
    }else for(int i=0;i<5;i++)if(hovered(mainButton(i))){
     mainSelection=i;
     if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
      if(i==0)startGame();else if(i==1)titleView=1;else if(i==2)titleView=2;else if(i==3)applyMenuAction(threadlight::menu::Action::OpenSettings);else applyMenuAction(threadlight::menu::Action::RequestExit);
     }
    }
   }else if(screenAtFrameStart==Mode::Pause){
    if(pauseConfirm){
     if(IsKeyPressed(KEY_ESCAPE)){pauseConfirm=0;}
     else if(IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_DOWN))confirmSelection=1-confirmSelection;
     else if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){
      if(confirmSelection==0){if(pauseConfirm==1)startGame();else {tutorial=false;applyMenuAction(threadlight::menu::Action::ReturnToMainMenu);}}
      pauseConfirm=0;
     }else for(int i=0;i<2;++i)if(hovered(confirmButton(i))){confirmSelection=i;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){if(i==0){if(pauseConfirm==1)startGame();else {tutorial=false;applyMenuAction(threadlight::menu::Action::ReturnToMainMenu);}}pauseConfirm=0;}}
    }else if(inventoryOpen){if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_I)||IsKeyPressed(KEY_P)||(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&hovered(Rectangle{SW*.5f-90.f,SH*.5f+235.f,180.f,38.f})))inventoryOpen=false;}
    else if(IsKeyPressed(KEY_P))applyMenuAction(threadlight::menu::Action::Resume);
    else if(IsKeyPressed(KEY_ESCAPE)){pauseConfirm=2;confirmSelection=1;}
    else if(IsKeyPressed(KEY_R)){pauseConfirm=1;confirmSelection=1;}
    else if(IsKeyPressed(KEY_I))inventoryOpen=true;
    else if(IsKeyPressed(KEY_UP))pauseSelection=threadlight::menu::moveSelection(pauseSelection,-1,5);
    else if(IsKeyPressed(KEY_DOWN))pauseSelection=threadlight::menu::moveSelection(pauseSelection,1,5);
    else if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){
     if(pauseSelection==0)applyMenuAction(threadlight::menu::Action::Resume);
     else if(pauseSelection==1)applyMenuAction(threadlight::menu::Action::OpenSettings);
     else if(pauseSelection==2)inventoryOpen=true;
     else {pauseConfirm=pauseSelection==3?1:2;confirmSelection=1;}
    }else for(int i=0;i<5;i++)if(hovered(pauseButton(i))){
     pauseSelection=i;
     if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
      if(i==0)applyMenuAction(threadlight::menu::Action::Resume);
      else if(i==1)applyMenuAction(threadlight::menu::Action::OpenSettings);
      else if(i==2)inventoryOpen=true;
      else {pauseConfirm=i==3?1:2;confirmSelection=1;}
     }
    }
   }else if(screenAtFrameStart==Mode::Settings){
    if(IsKeyPressed(KEY_ESCAPE))applyMenuAction(threadlight::menu::Action::Escape);
    else if(IsKeyPressed(KEY_TAB))threadlight::menu::switchSettingsTab(settingsTab,settingsFocus);
    else {
     if(hovered(videoTab)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){settingsTab=0;settingsFocus=0;}
     else if(hovered(audioTab)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){settingsTab=1;settingsFocus=0;}
     else if(hovered(personalizationTab)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){settingsTab=2;settingsFocus=0;}
     else if(hovered(settingsBack)){settingsFocus=settingsTab==0?3:(settingsTab==1?1:2);if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))applyMenuAction(threadlight::menu::Action::Back);}
     else if(settingsTab==0){
      for(int i=0;i<frameRateChoiceCount;i++)if(hovered(fpsChips[i])){settingsFocus=0;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))changeFrameLimit(threadlight::settings::kFrameRateLimits[i]);}
      if(hovered(showToggle)){settingsFocus=1;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))changeShowFps();}
      if(hovered(vsyncToggle)){settingsFocus=2;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))changeVsync();}
     }else if(settingsTab==1&&hovered(volumeRow))settingsFocus=0;
     else if(settingsTab==2){
      for(int i=0;i<2;++i)if(hovered(paletteCards[i])){
       settingsFocus=i;
       if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&i==0)changePalette(cosmetics::PaletteId::OriginalCyan);
       if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&i==1)changePalette(cosmetics::PaletteId::VioletAscendant);
      }
     }
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
      else if(settingsTab==0&&settingsFocus==2)changeVsync();
      else if(settingsTab==1&&settingsFocus==0)changeSfxVolume(threadlight::settings::adjustSfxVolume(appSettings.sfxVolumePercent,direction));
     }else if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){
      if(settingsTab==0&&settingsFocus==0)changeFrameLimit(threadlight::settings::nextFrameRateLimit(appSettings.frameRateLimit,1));
      else if(settingsTab==0&&settingsFocus==1)changeShowFps();
      else if(settingsTab==0&&settingsFocus==2)changeVsync();
      else if((settingsTab==0&&settingsFocus==3)||(settingsTab==1&&settingsFocus==1))applyMenuAction(threadlight::menu::Action::Back);
      else if(settingsTab==1&&settingsFocus==0)changeSfxVolume(threadlight::settings::adjustSfxVolume(appSettings.sfxVolumePercent,1));
      else if(settingsTab==2&&settingsFocus==0)changePalette(cosmetics::PaletteId::OriginalCyan);
      else if(settingsTab==2&&settingsFocus==1)changePalette(cosmetics::PaletteId::VioletAscendant);
      else if(settingsTab==2&&settingsFocus==2)applyMenuAction(threadlight::menu::Action::Back);
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
    if(tutorial){tutorialSkipHold=IsWindowFocused()&&IsKeyDown(KEY_B)?std::min(3.f,tutorialSkipHold+(float)frameSeconds):0.f;}
    if(tutorial&&(tutorialSkipHold>=3.f||(rewards.counters().killed>=10&&IsKeyPressed(KEY_ENTER)))){
     finishTutorial();
    }else if(!pacingCheck&&IsKeyPressed(KEY_P)){
     applyMenuAction(threadlight::menu::Action::Pause);
     stepClock.clear();dashPressPending=collectionContinuePending=false;
    }else{
     dashPressPending=dashPressPending||IsKeyPressed(KEY_SPACE);
     movementPressPending.add((IsKeyPressed(KEY_A)||IsKeyPressed(KEY_LEFT)?1u:0u)|(IsKeyPressed(KEY_D)||IsKeyPressed(KEY_RIGHT)?2u:0u)|(IsKeyPressed(KEY_W)||IsKeyPressed(KEY_UP)?4u:0u)|(IsKeyPressed(KEY_S)||IsKeyPressed(KEY_DOWN)?8u:0u));
     if(lifecycle.phase()==foundation::Phase::Collection)
      collectionContinuePending=collectionContinuePending||IsKeyPressed(KEY_ENTER);
     const auto simulationStart=perf?PerfClock::now():PerfClock::time_point{};
     const int steps=stepClock.advance(frameSeconds);
     if(perf)discardedMs=stepClock.discardedSeconds()*1000.0;
     for(int tick=0;tick<steps&&mode==Mode::Play;tick++){
      if(perf)++frameSteps;
      capturePresentation();
      constexpr float dt=static_cast<float>(threadlight::timing::kFixedStepSeconds);
      ++simulationTick;
      const auto tickPhase=lifecycle.phase();
      const int phaseElapsedAtStart=lifecycle.elapsedTicks();
      const bool combat=foundation::isCombat(tickPhase);
      const bool continueCollection=collectionContinuePending;collectionContinuePending=false;
      if(tickPhase==foundation::Phase::Countdown){
       dashPressPending=false;movementPressPending.clear();
       lifecycle.tick(foes.size());
       if(lifecycle.phase()!=tickPhase)traceM1("transition");
       continue;
      }
      if(!combat&&tickPhase!=foundation::Phase::Collection)break;
      battle::tick(health);dashCd=std::max(0.f,dashCd-dt);pulse=std::max(0.f,pulse-dt);damageFeedback=std::max(0.f,damageFeedback-dt);shake=std::max(0.f,shake-dt);dashVisualActive=dashTime>0;dashEchoTimer=std::max(0.f,dashEchoTimer-dt);
      const unsigned pressedDirections=movementPressPending.consume();
      V mv{(float)(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT)||(pressedDirections&2))-(float)(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)||(pressedDirections&1)),(float)(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN)||(pressedDirections&8))-(float)(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP)||(pressedDirections&4))}; if(len(mv)>0)mv=norm(mv);playerWasMoving=len(mv)>0;
      if(pacingCheck){const float angle=static_cast<float>(simulationTick)*dt*1.5f;mv={std::cos(angle),std::sin(angle)};playerWasMoving=true;}
      if(tutorial&&playerWasMoving)tutorialMoved=true;
      const bool dashPressed=pacingCheck?simulationTick%180==60:dashPressPending;dashPressPending=false;
      if(dashPressed&&dashCd<=0&&len(mv)>0){afterimage::beginDash(offensiveEcho,{player.x,player.y},combat,signatureReward.owned);dashTime=threadlight::kDashDurationSeconds;dashCd=stats.dashCooldownSeconds;dashCycleDuration=dashCd;dashEcho.fill(player);dashEchoCount=0;dashEchoFade=.22f;dashEchoTimer=0;dashVisualActive=true;burst(player,C(132,244,255),10,150);sfx.play(threadlight::audio::Event::Dash);}
      if(tutorial&&dashTime>0&&!tutorialDashed){tutorialDashed=true;tutorialSpawnStart=phaseElapsedAtStart;}
      bossPhaseNotice=std::max(0.f,bossPhaseNotice-dt);
      const bool dashActiveThisFrame=threadlight::dashProtectsCollisionThisFrame(dashTime);dashVisualActive=dashVisualActive||dashActiveThisFrame;dashEchoFade=std::max(0.f,dashEchoFade-dt);float mult=dashActiveThisFrame?stats.dashMultiplier:1.f;if(dashActiveThisFrame)dashTime=std::max(0.f,dashTime-dt);player=player+mv*(stats.moveSpeed*mult*dt);player.x=threadlight::layout::clampPlayerX(player.x,playArea);player.y=threadlight::layout::clampPlayerY(player.y,playArea);
      if(dashActiveThisFrame){
       afterimage::appendDash(offensiveEcho,{previousPlayer.x,previousPlayer.y},{player.x,player.y},combat);
       if(dashTime<=0){
        afterimage::finishDash(offensiveEcho,combat,signatureReward.owned);
        if(offensiveEcho.active()){++echoCount;sfx.play(threadlight::audio::Event::Afterimage);traceM1("echo_created",echoCount,(int)offensiveEcho.count);}
       }
      }
      if(dashActiveThisFrame&&dashEchoTimer<=0){const size_t last=std::min(dashEchoCount,dashEcho.size()-1);for(size_t i=last;i>0;i--)dashEcho[i]=dashEcho[i-1];dashEcho[0]=player;dashEchoCount=std::min(dashEchoCount+1,dashEcho.size());dashEchoTimer=.028f;dashEchoFade=std::max(dashEchoFade,.22f);}
      if(combat){
       utilities::tick(needlecatch,true);if(needleFlashTicks>0)--needleFlashTicks;
       const float bossPursuitFactor=effects::factor(bossSlow,true);
       effects::tick(anchorState,true);effects::tick(bossSlow,true);
       // Interception can use only strands already present at tick start.
       // A strand laid during this movement must not block a shot in its past.
       std::array<needle::Segment,520> blockingSilk{};size_t blockingCount=0;
       for(size_t i=1;utilities::ready(needlecatch,upgrades::rank(ranks,upgrades::Id::Needlecatch))&&i<thread.size();++i)if(thread[i].age+dt<=stats.activeSilkSeconds)
        blockingSilk[blockingCount++]={{thread[i-1].p.x,thread[i-1].p.y},{thread[i].p.x,thread[i].p.y}};
       loom::tick(loomState,true,loomReward.owned);
       std::array<cross::Segment,520> loopHistory{};size_t loopCount=0;
       if(cocoonReward.owned&&!thread.empty()){
        for(size_t i=1;i<thread.size();++i)loopHistory[loopCount++]={thread[i].id,{thread[i-1].p.x,thread[i-1].p.y},{thread[i].p.x,thread[i].p.y},(int)std::lround(thread[i].age*120.f)+1};
        if(dist(thread.back().p,previousPlayer)>.05f&&loopCount<loopHistory.size())loopHistory[loopCount++]={nextThreadId+1,{thread.back().p.x,thread.back().p.y},{previousPlayer.x,previousPlayer.y},0};
       }
       const auto formation=cocoon::tick(cocoonState,{previousPlayer.x,previousPlayer.y},{player.x,player.y},std::span<const cross::Segment>(loopHistory.data(),loopCount),true,cocoonReward.owned);
       if(formation.formed){++cocoonsFormed;burst(player,C(208,241,193),12,100);sfx.play(threadlight::audio::Event::Afterimage);traceM1("cocoon_formed",formation.segmentId,(int)formation.area);}
       if(pulseTicks>0)--pulseTicks;
       if(thread.empty()||threadlight::silk::shouldAppendKnot(thread.back().p,player)){thread.push_back({player,0});thread.back().id=++nextThreadId;}
       for(auto &t:thread)t.age+=dt;
       while(!thread.empty()&&(thread.front().age>threadlight::visual::kSilkLifetimeSeconds||thread.size()>520))thread.erase(thread.begin());
       const size_t firstActiveSilk=threadlight::silk::firstActiveSegment(std::span<const Thread>(thread),stats.activeSilkSeconds);
       std::array<cross::Segment,520> history{};size_t historyCount=0;
       if(crossReward.owned)for(size_t i=1;i<thread.size();++i)
        history[historyCount++]={thread[i].id,{thread[i-1].p.x,thread[i-1].p.y},{thread[i].p.x,thread[i].p.y},(int)std::lround(thread[i].age*120.f)};
       crossPulse=cross::tick(crossState,{previousPlayer.x,previousPlayer.y},{player.x,player.y},std::span<const cross::Segment>(history.data(),historyCount),true,crossReward.owned);
       if(crossPulse.triggered){pulseCenter=crossPulse.center;pulseRadius=cross::kPulseRadius;pulseTicks=24;++crossPulses;sfx.play(threadlight::audio::Event::Afterimage);burst({pulseCenter.x,pulseCenter.y},C(170,237,226),12,110);traceM1("crossweave_pulse",crossPulses);}
       const bool bossEncounter=chapter::encounter(wave).boss;
       if(tickPhase==foundation::Phase::Spawning&&!bossEncounter){
        while((!tutorial||tutorialSpawnStart>=0)&&spawnCursor<spawnSchedule.count&&spawnSchedule.items[spawnCursor].tick<=phaseElapsedAtStart-(tutorial?tutorialSpawnStart:0)){
         const auto entry=spawnSchedule.items[spawnCursor++];addFoe(entry.kind,entry.angle);
        }
       }
       if(bossEncounter&&root::alive(boss)){
        if(boss.kind!=root::Kind::Rootwarden&&boss.stage==root::Stage::Stalk&&!boss.counterstepping&&boss.remaining<=1){
         int supportCommits=0;for(const auto& f:foes)if((f.kind==battle::EnemyKind::Skitter&&skitter::committed(f.skitterState))||(f.kind==battle::EnemyKind::Spitter&&ranged::committed(f.rangedState))||(f.kind==battle::EnemyKind::Sower&&sower::committed(f.sowerState)))++supportCommits;
         root::waitForSupports(boss,supportCommits,projectiles.flyingCount());
        }
        const auto stageBefore=boss.stage;const bool bossWasStalking=stageBefore==root::Stage::Stalk&&!boss.counterstepping;
        const auto bossEvents=root::tick(boss,{player.x,player.y},bossArena(),ground.areaOccupied({playArea.left,playArea.right,playArea.top,playArea.bottom}));
        if(bossWasStalking&&boss.stage==root::Stage::Stalk&&!boss.counterstepping)
         boss.p=root::add(boss.previousP,root::scale(root::sub(boss.p,boss.previousP),bossPursuitFactor));
        releaseSupports(bossEvents.supports);
        if(bossEvents.warning){if(boss.corridor.enabled)ground.cancelUnsafeCorridors(boss.corridor);sfx.play(threadlight::audio::Event::BossWarning);traceBoss("warning");}
        bool touched=false;
        for(size_t i=firstActiveSilk;i<thread.size();++i){
         if(!root::silkContact(boss,{thread[i-1].p.x,thread[i-1].p.y},{thread[i].p.x,thread[i].p.y},stats.silkHalfWidth))continue;
         touched=true;const auto hit=root::applySilk(boss,stats.silkDamage);
         if(hit.hit==battle::SilkHit::None)break;
         if(hit.hit!=battle::SilkHit::None&&loom::onHit(loomState,loomReward.owned,true,loom::HitKind::OrdinarySilk)){loomPending=true;loomSource={bossRewardId,boss.p,0,1};}
         bossHit(hit,true);
         if(root::alive(boss))effects::onOrdinaryHit(bossSlow,catalog::owns(signatureCatalog,catalog::Id::Slipknot));
         if(root::alive(boss)&&effects::onOrdinaryHit(anchorState,catalog::owns(signatureCatalog,catalog::Id::AnchorStitch),bossRewardId,hit.damage)){
          ++anchorProcs;bossHit(root::applySignature(boss,effects::damage(stats.silkDamage,true)));traceM1("anchor_stitch",bossRewardId);
         }
         break;
        }
        if(touched)++boss.metrics.contactTicks;
        if(root::alive(boss)&&afterimage::claimContact(offensiveEcho,bossRewardId,boss.previousP,boss.p,root::vulnerableRadius(boss))){
         const auto hit=root::applySignature(boss,afterimage::damage(stats.silkDamage,true));
         echoDamage+=hit.damage;++echoHitCount;bossHit(hit);traceM1("echo_hit",bossRewardId,hit.damage);
        }
        for(int i=0;i<boss.budCount;++i)if(boss.buds[i].health.alive()){
         const auto& bud=boss.buds[i];
         for(size_t t=firstActiveSilk;t<thread.size();++t)if(battle::silkContact(bud.p,bud.p,16,{thread[t-1].p.x,thread[t-1].p.y},{thread[t].p.x,thread[t].p.y},stats.silkHalfWidth)){
          budHit(i,stats.silkDamage,true);break;
         }
         if(bud.health.alive()&&afterimage::claimContact(offensiveEcho,bud.id,bud.p,bud.p,16)){
          const int hp=bud.health.hp;if(budHit(i,afterimage::damage(stats.silkDamage),false)!=battle::SilkHit::None){++echoHitCount;echoDamage+=hp-bud.health.hp;}
         }
        }
       }
       if(bossEncounter&&root::alive(boss)&&supportQueued){
        int available=std::max(0,root::supportCapacity(boss)-(int)foes.size());
        while(supportQueued&&available--){
         const auto kind=root::supportKind(boss,supportSpawned);
         addFoe(kind,chapter::kPi*.25f+supportSpawned*chapter::kPi*.5f);--supportQueued;++supportSpawned;
        }
       }
       mergeBriarCallers();
       for(auto& foe:foes){
        if(!foe.health.alive()||foe.kind==battle::EnemyKind::BriarCaller)continue;
        const caller::State* strongestAura=nullptr;
        int strongestBoost=0;
        for(const auto& source:foes){
         if(!source.health.alive()||source.kind!=battle::EnemyKind::BriarCaller||
            !caller::insideAura({source.p.x,source.p.y},{foe.p.x,foe.p.y},source.briarCaller))continue;
         const int boost=caller::speedBoostPercent(source.briarCaller);
         if(boost>strongestBoost){strongestBoost=boost;strongestAura=&source.briarCaller;}
        }
        if(strongestAura)caller::applyAura(foe.callerSpeedBoost,*strongestAura);
        else caller::tick(foe.callerSpeedBoost);
       }
       int commitments=0;
       for(const auto& f:foes){
        if(f.kind==battle::EnemyKind::Skitter&&skitter::committed(f.skitterState))++commitments;
        if(f.kind==battle::EnemyKind::Spitter&&ranged::committed(f.rangedState))++commitments;
        if(f.kind==battle::EnemyKind::Sower&&sower::committed(f.sowerState))++commitments;
       }
       attacks::Grants grants;
       if(chapter::encounter(wave).chapter>=2){
        std::array<attacks::Request,64> ready{};int readyCount=0;
        for(const auto& f:foes){
         if(readyCount==(int)ready.size())break;
         if(f.kind==battle::EnemyKind::Skitter&&f.skitterState.phase==skitter::Phase::Flank&&f.skitterState.ticksRemaining<=1)
          ready[readyCount++]={f.rewardId,attacks::Kind::Skitter};
         else if(f.kind==battle::EnemyKind::Spitter&&ranged::wantsCommit(f.rangedState))
          ready[readyCount++]={f.rewardId,attacks::Kind::Spitter};
         else if(f.kind==battle::EnemyKind::Sower&&sower::wantsCommit(f.sowerState))
          ready[readyCount++]={f.rewardId,attacks::Kind::Skitter};
        }
        const int bossSlots=bossEncounter&&root::alive(boss)?root::supportSlots(boss):0;
        grants=attackCoordinator.choose(std::span<const attacks::Request>(ready.data(),readyCount),commitments+bossSlots,(int)projectiles.freeSlots(),simulationTick);
        for(int i=0;i<grants.count;++i)if(grants.items[i].kind==attacks::Kind::Spitter)
         for(auto& f:foes)if(f.rewardId==grants.items[i].id)f.projectileTicket=projectiles.reserve(f.rewardId);
       }
       // The boss pool is stable during this enemy loop, after boss updates and
       // defeat handling. Compute its occupancy only if a Sower needs it.
       float bossFieldArea=0.f;bool bossFieldAreaKnown=false;
       battle::IncomingHit incoming;
       int incomingKind=0;
       auto considerDamage=[&](int amount,std::uint64_t id,int kind){const auto before=incoming;incoming.consider(amount,id);if(before.damage!=incoming.damage||before.sourceId!=incoming.sourceId)incomingKind=kind;};
       for(auto &f:foes){
        if(!f.health.alive())continue;
        const V oldPosition=f.p;
        const float pursuitFactor=effects::factor(f.slow,false);
        effects::tick(f.slow,true);
        const bool wasCommitted=f.kind==battle::EnemyKind::Skitter?skitter::committed(f.skitterState):f.kind==battle::EnemyKind::Sower?sower::committed(f.sowerState):f.kind==battle::EnemyKind::Spitter&&ranged::committed(f.rangedState);
        f.phase+=dt;battle::tick(f.health);f.hitFlash=std::max(0.f,f.hitFlash-dt);
        if(f.kind==battle::EnemyKind::Skitter){
         foundation::Point p{f.p.x,f.p.y},v{f.vel.x,f.vel.y};
         const bool canCommit=chapter::encounter(wave).chapter>=2?grants.contains(f.rewardId):commitments<2;
         const auto result=skitter::tick(f.skitterState,p,v,{player.x,player.y},{player.x-previousPlayer.x,player.y-previousPlayer.y},
          {playArea.left+f.r,playArea.right-f.r,playArea.top+f.r,playArea.bottom-f.r},canCommit,
          caller::boostedSpeed(f.speed,f.callerSpeedBoost));
         f.p={p.x,p.y};f.vel={v.x,v.y};
         if(result.telegraphStarted)traceM1("skitter_windup",f.rewardId);
         if(result.burstStarted)traceM1("skitter_burst",f.rewardId);
        }else if(f.kind==battle::EnemyKind::Spitter){
         foundation::Point p{f.p.x,f.p.y},v{f.vel.x,f.vel.y};
         const auto result=ranged::tick(f.rangedState,p,v,{player.x,player.y},
          {playArea.left+f.r,playArea.right-f.r,playArea.top+f.r,playArea.bottom-f.r},grants.contains(f.rewardId)&&f.projectileTicket!=0,
          caller::boostedSpeed(f.speed,f.callerSpeedBoost));
         f.p={p.x,p.y};f.vel={v.x,v.y};
         if(result.warningStarted)traceM1("spitter_warning",f.rewardId,(int)f.projectileTicket);
         if(result.aimLocked)traceM1("spitter_locked",f.rewardId);
         if(result.shotReady){
          const auto muzzle=ranged::muzzle(p,f.rangedState.aimDirection);
          if(projectiles.launch(f.projectileTicket,muzzle,f.rangedState.aimDirection)){
           traceM1("projectile_launched",f.projectileTicket,(int)f.rewardId);sfx.play(threadlight::audio::Event::SpitterShot);
          }else{projectiles.cancelReservation(f.projectileTicket);TraceLog(LOG_ERROR,"M4: reserved shot could not launch");}
          f.projectileTicket=0;
         }
        }else if(f.kind==battle::EnemyKind::Sower){
         foundation::Point p{f.p.x,f.p.y},v{f.vel.x,f.vel.y};
         if(!bossFieldAreaKnown){bossFieldArea=boss.fields.areaOccupied({playArea.left,playArea.right,playArea.top,playArea.bottom});bossFieldAreaKnown=true;}
         const auto event=sower::tick(f.sowerState,p,v,{player.x,player.y},{(player.x-previousPlayer.x)*120,(player.y-previousPlayer.y)*120},
          {playArea.left,playArea.right,playArea.top,playArea.bottom},ground,bossEncounter?boss.corridor:field::Corridor{},
          bossFieldArea,grants.contains(f.rewardId),true,
          caller::boostedSpeed(1.f,f.callerSpeedBoost));
         f.p={p.x,p.y};f.vel={v.x,v.y};
         if(event.warningStarted){traceM1("sower_warning",f.rewardId,(int)event.ticket);sfx.play(threadlight::audio::Event::BossWarning);}
        }else{
         V target=player;
         float movementSpeed=caller::boostedSpeed(f.speed,f.callerSpeedBoost);
         if(f.kind==battle::EnemyKind::BriarCaller){
          const auto targetPoint=caller::movementTarget({f.p.x,f.p.y},{player.x,player.y},foes,&f,
           [](const Foe& other){return other.health.alive();},
           [](const Foe& other){return other.kind==battle::EnemyKind::BriarCaller;},
           [](const Foe& other){return caller::Point{other.p.x,other.p.y};});
          target={targetPoint.x,targetPoint.y};
          movementSpeed=caller::movementSpeed(f.speed,f.briarCaller);
         }
         const V d=norm(target-f.p);const float wob=std::sin(f.phase*3)*.28f;
         f.vel=f.vel+(d+V{-d.y*wob,d.x*wob})*(movementSpeed*dt*2.5f);
         const float vl=len(f.vel);if(vl>movementSpeed)f.vel=f.vel*(movementSpeed/vl);f.p=f.p+f.vel*dt;
        }
        const bool nowCommitted=f.kind==battle::EnemyKind::Skitter?skitter::committed(f.skitterState):f.kind==battle::EnemyKind::Sower?sower::committed(f.sowerState):f.kind==battle::EnemyKind::Spitter&&ranged::committed(f.rangedState);
        if(!wasCommitted&&!nowCommitted)f.p=oldPosition+(f.p-oldPosition)*pursuitFactor;
        commitments+=(nowCommitted?1:0)-(wasCommitted?1:0);
        if(f.health.silkCooldownTicks==0)for(size_t i=firstActiveSilk;i<thread.size();i++){
         if(!battle::silkContact({oldPosition.x,oldPosition.y},{f.p.x,f.p.y},f.r,
             {thread[i-1].p.x,thread[i-1].p.y},{thread[i].p.x,thread[i].p.y},stats.silkHalfWidth))continue;
         const int hpBefore=f.health.hp;
         const auto ordinaryHit=battle::applySilk(f.health,stats.silkDamage);
         const int ordinaryDamage=hpBefore-f.health.hp;
         foeHit(f,ordinaryHit,false);
         if(ordinaryDamage>0&&f.health.alive()){
          effects::onOrdinaryHit(f.slow,catalog::owns(signatureCatalog,catalog::Id::Slipknot));
          const bool anchor=effects::onOrdinaryHit(anchorState,catalog::owns(signatureCatalog,catalog::Id::AnchorStitch),f.rewardId,ordinaryDamage);
          if(anchor&&f.health.alive()){
           const int bonus=effects::damage(stats.silkDamage,false);
           foeHit(f,battle::applyDamage(f.health,bonus),true);++anchorProcs;traceM1("anchor_stitch",f.rewardId);
          }
         }
         break;
        }
        if(f.health.alive()&&afterimage::claimContact(offensiveEcho,f.rewardId,{oldPosition.x,oldPosition.y},{f.p.x,f.p.y},f.r)){
         const int before=f.health.hp;const auto hit=battle::applyDamage(f.health,afterimage::damage(stats.silkDamage));
         ++echoHitCount;echoDamage+=before-f.health.hp;traceM1("echo_hit",f.rewardId,before-f.health.hp);foeHit(f,hit,true);
        }
        if(!f.health.alive()&&nowCommitted)--commitments;
       }
       if(crossPulse.triggered){
        std::array<cross::Target,128> targets{};size_t count=0;
        auto candidate=[&](foundation::Point p,float r,foundation::RewardId id,int index,int kind){
         if(count<targets.size())targets[count++]={id,p,r,index,kind};
        };
        for(size_t i=0;i<foes.size();++i)if(foes[i].health.alive())candidate({foes[i].p.x,foes[i].p.y},foes[i].r,foes[i].rewardId,(int)i,0);
        if(bossEncounter&&root::alive(boss)){
         candidate(boss.p,root::vulnerableRadius(boss),bossRewardId,0,1);
         for(int i=0;i<boss.budCount;++i)if(boss.buds[i].health.alive())candidate(boss.buds[i].p,16,boss.buds[i].id,i,2);
        }
        const auto selected=cross::selectTargets(std::span<const cross::Target>(targets.data(),count),crossPulse.center);
        for(size_t i=0;i<selected.count;++i){const auto target=selected.items[i];
         if(target.kind==0){auto& f=foes[target.index];const int hp=f.health.hp;
          const auto hit=battle::applyDamage(f.health,cross::damage(stats.silkDamage));
          crossDamage+=hp-f.health.hp;foeHit(f,hit,true);++crossHits;
         }else if(target.kind==1){const auto hit=root::applySignature(boss,cross::damage(stats.silkDamage,true));crossDamage+=hit.damage;++crossHits;bossHit(hit);if(hit.events.defeated)break;}
         else if(target.index<boss.budCount){auto& bud=boss.buds[target.index];const int hp=bud.health.hp;
          if(budHit(target.index,cross::damage(stats.silkDamage),false)!=battle::SilkHit::None){++crossHits;crossDamage+=hp-bud.health.hp;}
         }
        }
       }
       if(loomPending&&!finalBossDefeated){
        loomPending=false;++loomBursts;pulseCenter=loomSource.position;pulseRadius=loom::kNeighborRadius;pulseTicks=24;
        std::array<loom::Candidate,128> candidates{};size_t count=0;
        for(size_t i=0;i<foes.size();++i)if(foes[i].health.alive())candidates[count++]={foes[i].rewardId,{foes[i].p.x,foes[i].p.y},(int)i,0};
        if(bossEncounter&&root::alive(boss)){
         candidates[count++]={bossRewardId,boss.p,0,1};
         for(int i=0;i<boss.budCount;++i)if(boss.buds[i].health.alive())candidates[count++]={boss.buds[i].id,boss.buds[i].p,i,2};
        }
        const auto selected=loom::selectTargets(loomSource,std::span<const loom::Candidate>(candidates.data(),count));
        for(size_t i=0;i<selected.count;++i){const auto target=selected.items[i];
         if(target.kind==0){
          auto found=std::find_if(foes.begin(),foes.end(),[&](const Foe& f){return f.rewardId==target.id&&f.health.alive();});
          if(found!=foes.end()){auto& f=*found;const int hp=f.health.hp;
           const auto hit=battle::applyDamage(f.health,loom::damage(stats.silkDamage));
           loomDamage+=hp-f.health.hp;foeHit(f,hit,true);
          }
         }else if(target.kind==1&&root::alive(boss)&&target.id==bossRewardId){
          const auto hit=root::applySignature(boss,loom::damage(stats.silkDamage,true));loomDamage+=hit.damage;bossHit(hit);if(hit.events.defeated)break;
         }else if(target.kind==2&&target.index<boss.budCount&&boss.buds[target.index].id==target.id){
          const int hp=boss.buds[target.index].health.hp;budHit(target.index,loom::damage(stats.silkDamage),false);loomDamage+=hp-boss.buds[target.index].health.hp;
         }
        }
        sfx.play(threadlight::audio::Event::Afterimage);traceM1("resonant_burst",loomSource.id,loomDamage);
       }
       if(utilities::ready(needlecatch,upgrades::rank(ranks,upgrades::Id::Needlecatch))){
        const auto caught=needle::find(projectiles,boss.bullets,std::span<const needle::Segment>(blockingSilk.data(),blockingCount),stats.silkHalfWidth,
         {previousPlayer.x,previousPlayer.y},{player.x,player.y},radius,{playArea.left,playArea.right,playArea.top,playArea.bottom});
        if(caught.id&&(caught.pool==0?projectiles:boss.bullets).intercept(caught.id)){
         utilities::trigger(needlecatch,upgrades::rank(ranks,upgrades::Id::Needlecatch));
         needleFlash=caught.position;needleFlashTicks=24;++needleCatches;
         burst({needleFlash.x,needleFlash.y},C(218,235,180),8,70);sfx.play(threadlight::audio::Event::EnemyHit);
         traceM1("needlecatch",caught.id,caught.pool);
        }
       }
       const bool bodyCanHurt=battle::canReceiveIncoming(health,dashActiveThisFrame);
       if(bodyCanHurt)for(const auto& f:foes)if(f.health.alive()&&battle::bodyContact({previousPlayer.x,previousPlayer.y},{player.x,player.y},radius,
          {f.previousP.x,f.previousP.y},{f.p.x,f.p.y},f.r))considerDamage(battle::definition(f.kind,chapter::encounter(wave).chapter).contactDamage,f.rewardId,0);
       if(bossEncounter&&root::alive(boss)){
        if(bodyCanHurt)considerDamage(root::incomingDamage(boss,{previousPlayer.x,previousPlayer.y},{player.x,player.y},radius),bossRewardId,1);
        const auto bossImpacts=root::projectileTick(boss,{previousPlayer.x,previousPlayer.y},{player.x,player.y},radius,bossArena());
        for(int i=0;i<bossImpacts.count;++i)considerDamage(bossImpacts.items[i].damage,bossRewardId,1);
        const auto fieldHit=boss.fields.tick({previousPlayer.x,previousPlayer.y},{player.x,player.y},radius);
        considerDamage(fieldHit.damage,fieldHit.sourceId,1);
       }
       const auto groundHit=ground.tick({previousPlayer.x,previousPlayer.y},{player.x,player.y},radius);
       considerDamage(groundHit.damage,groundHit.sourceId,3);
       const auto impacts=projectiles.tick({previousPlayer.x,previousPlayer.y},{player.x,player.y},radius,
        {playArea.left,playArea.right,playArea.top,playArea.bottom});
       for(int i=0;i<impacts.count;++i){
        considerDamage(impacts.items[i].damage,impacts.items[i].sourceId,2);
        traceM1("projectile_contact",impacts.items[i].projectileId,(int)impacts.items[i].sourceId);
       }
       afterimage::advance(offensiveEcho,true);
       const int barrierBefore=cocoonState.barrierCenti;
       const int damage=battle::applyIncoming(health,incoming,dashActiveThisFrame,cocoon::barrierActive(cocoonState)?&cocoonState.barrierCenti:nullptr,stats.damageReductionPercent);
       if(barrierBefore>cocoonState.barrierCenti){barrierAbsorbed+=barrierBefore-cocoonState.barrierCenti;burst(player,C(221,239,185),8,90);traceM1("cocoon_absorbed",incoming.sourceId,barrierBefore-cocoonState.barrierCenti);}
       if(damage){
        runDamageTaken[incomingKind]+=damage;
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
       for(const auto& foe:foes)if(rewards.despawnEnemy(foe.rewardId))traceM1("timeout_removal",foe.rewardId);
       foes.clear();traceM1("cleanup_timeout",0,foundation::kCleanupTicks);
      }
      if(events.enteredCollection){anchorState={};bossSlow={};needleFlashTicks=0;thread.clear();cross::clear(crossState);cocoon::clear(cocoonState);loom::clear(loomState);loomPending=false;ground.clear();boss.fields.clear();pulseTicks=0;afterimage::clear(offensiveEcho);projectiles.clear();attackCoordinator.clear();dashTime=0;dashVisualActive=false;dashEchoCount=0;dashEchoFade=0;traceM1(events.timeoutDespawn?"collection_timeout":"collection_clear",0,tickPhase==foundation::Phase::Cleanup?phaseElapsedAtStart+1:0);}
      else if(lifecycle.phase()!=tickPhase)traceM1("transition");
      // Complete bookkeeping on a lethal tick, but never open rewards or advance
      // encounters after defeat. Loss retains outcome precedence.
      if(tutorial&&!health.alive())health.hp=health.maxHp;
      if(!health.alive()){giftReward={};anchorState={};bossSlow={};needleFlashTicks=0;signatureReward={};crossReward={};cocoonReward={};loomReward={};cross::clear(crossState);cocoon::clear(cocoonState);loom::clear(loomState);loomPending=false;ground.clear();pulseTicks=0;root::clearThreats(boss);afterimage::clear(offensiveEcho);projectiles.clear();attackCoordinator.clear();mode=Mode::Lost;sfx.play(threadlight::audio::Event::Defeat);traceM1("defeat");continue;}
      if(finalBossDefeated){mode=Mode::Won;sfx.play(threadlight::audio::Event::Victory);traceM1("campaign_victory");continue;}
      if(events.enteredPendingRewards){
       if(m1Telemetry)TraceLog(LOG_INFO,"M5 wave=%d cross_pulses=%llu cross_hits=%llu cross_damage=%d",wave+1,(unsigned long long)crossPulses,(unsigned long long)crossHits,crossDamage);
       if(m1Telemetry)TraceLog(LOG_INFO,"M4 summary wave=%d echoes=%llu echo_hits=%llu echo_damage_centi=%d projectiles=%llu reserved=%llu queue=%d",
        wave+1,(unsigned long long)echoCount,(unsigned long long)echoHitCount,echoDamage,
        (unsigned long long)projectiles.flyingCount(),(unsigned long long)projectiles.reservedCount(),attackCoordinator.waiting());
       traceM1("collection_finished",0,phaseElapsedAtStart+1);
       if(m1Telemetry){const auto& c=rewards.counters();TraceLog(LOG_INFO,"M3 collection wave=%d ticks=%d early=%d live=%llu created=%llu collected=%llu expired=%llu pending=%llu bank=%lld",
        wave+1,phaseElapsedAtStart+1,continueCollection?1:0,(unsigned long long)(c.sparksCreated-c.sparksCollected-c.sparksExpired),
        (unsigned long long)c.sparksCreated,(unsigned long long)c.sparksCollected,(unsigned long long)c.sparksExpired,
        (unsigned long long)rewards.pendingCredits(),(long long)rewards.remainder());}
       if(wave+1==chapter::kEncounterCount){
        mode=Mode::Won;sfx.play(threadlight::audio::Event::Victory);traceM1("campaign_victory");
       }else if(rewards.pendingCredits()){
        upgrades::prepareOffer(offers,ranks,health,rewards.nextCredit(),wave+2<chapter::kEncounterCount,chapter::encounter(wave+1).boss,wave+1>=7);choice=0;mode=Mode::Upgrade;sfx.play(threadlight::audio::Event::Evolution);traceM1("offer_open",offers.current.credit);
       }else finishEncounter();
      }
     }
     if(perf)simulationMs=std::chrono::duration<double,std::milli>(PerfClock::now()-simulationStart).count();
     if(mode!=Mode::Play)stepClock.clear();
    }
   }else if(screenAtFrameStart==Mode::Upgrade){
    const auto cards=upgradeUI::layout(SW,SH,offers.current.count);
    const int pointed=menuFocus.mouseOwnsFocus()?upgradeUI::cardAt(cards,mouse.x,mouse.y,offers.current.count):-1;
    if(pointed>=0)choice=pointed;
    if(IsKeyPressed(KEY_ONE))choice=0;
    else if(IsKeyPressed(KEY_TWO)||IsKeyPressed(KEY_S))choice=1;
    else if(IsKeyPressed(KEY_THREE))choice=2;
    else if(IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_A))choice=threadlight::menu::moveSelection(choice,-1,offers.current.count);
    else if(IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_D))choice=threadlight::menu::moveSelection(choice,1,offers.current.count);
    choice=std::clamp(choice,0,std::max(0,offers.current.count-1));
    const bool confirm=IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)||
     (menuFocus.mouseOwnsFocus()&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&upgradeUI::contains(cards.confirm,mouse.x,mouse.y));
    const auto selected=offers.current.count>0?offers.current.cards[choice]:upgrades::Id::Count;
    const bool economyUseful=wave+2<chapter::kEncounterCount;
    if(confirm&&(offers.current.count==0
       ?upgrades::resolveExhaustedCredit(rewards,lifecycle.phase(),offers.current,ranks,health,economyUseful,wave+1>=7)
       :upgrades::resolveChoice(rewards,lifecycle.phase(),offers.current,choice,ranks,health,economyUseful,wave+1>=7))){
     stats=upgrades::derive(ranks);traceM1("credit_resolved",offers.current.credit);
     if(m1Telemetry)TraceLog(LOG_INFO,"M2 evolution credit=%llu upgrade=%d rank=%d damage=%d speed=%.0f cooldown=%.2f hp=%d maxhp=%d",
      (unsigned long long)offers.current.credit,(int)selected,upgrades::rank(ranks,selected),stats.silkDamage,stats.moveSpeed,stats.dashCooldownSeconds,health.hp,health.maxHp);
     choice=0;
     if(!rewards.pendingCredits()){mode=Mode::Play;finishEncounter();}
     else{upgrades::prepareOffer(offers,ranks,health,rewards.nextCredit(),wave+2<chapter::kEncounterCount,false,wave+1>=7);traceM1("offer_open",offers.current.credit);}
     stepClock.clear();dashPressPending=collectionContinuePending=false;
    }
   }
   else if(screenAtFrameStart==Mode::Signature){
    const int count=signatureCatalog.current.count;
    const auto ui=upgradeUI::layout(SW,SH,count);
    const int pointed=menuFocus.mouseOwnsFocus()?upgradeUI::cardAt(ui,mouse.x,mouse.y,count):-1;
    if(pointed>=0)choice=pointed;
    if(IsKeyPressed(KEY_ONE))choice=0;
    else if(IsKeyPressed(KEY_TWO)||IsKeyPressed(KEY_S))choice=1;
    else if(IsKeyPressed(KEY_THREE))choice=2;
    else if(IsKeyPressed(KEY_LEFT)||IsKeyPressed(KEY_A))choice=threadlight::menu::moveSelection(choice,-1,count);
    else if(IsKeyPressed(KEY_RIGHT)||IsKeyPressed(KEY_D))choice=threadlight::menu::moveSelection(choice,1,count);
    choice=std::clamp(choice,0,std::max(0,count-1));
    const bool confirm=IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)||
     (menuFocus.mouseOwnsFocus()&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&upgradeUI::contains(ui.confirm,mouse.x,mouse.y));
    auto& offered=currentSignature();
    if(confirm&&catalog::accept(signatureCatalog,offered,lifecycle.phase(),health.alive(),rewards.pendingCredits(),choice)){
     signatureReward.owned=catalog::owns(signatureCatalog,catalog::Id::Afterimage);
     crossReward.owned=catalog::owns(signatureCatalog,catalog::Id::Crossweave);
     cocoonReward.owned=catalog::owns(signatureCatalog,catalog::Id::Cocoon);
     loomReward.owned=catalog::owns(signatureCatalog,catalog::Id::ResonantLoom);
     traceM1("signature_acquired",offered.source,(int)signatureCatalog.current.choices[choice]);sfx.play(threadlight::audio::Event::Evolution);
     stepClock.clear();dashPressPending=collectionContinuePending=false;mode=Mode::Play;finishEncounter();
    }
   }
   else if(screenAtFrameStart==Mode::Lost||screenAtFrameStart==Mode::Won){if(inventoryOpen){if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_I)||(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)&&hovered(Rectangle{SW*.5f-90.f,SH*.5f+235.f,180.f,38.f})))inventoryOpen=false;}else if(IsKeyPressed(KEY_I))inventoryOpen=true;else if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_R))reset();else if(IsKeyPressed(KEY_ESCAPE))applyMenuAction(threadlight::menu::Action::Escape);}

   if(settingsEdits.finishDrag(mode==Mode::Settings&&settingsTab==1,
       IsWindowFocused(),IsMouseButtonDown(MOUSE_BUTTON_LEFT),false))saveSettings();

   if(mode!=screenAtFrameStart){
    if((mode==Mode::Lost||mode==Mode::Won)&&!tutorial&&bossFixture<0&&waveFixture<0){
     const bool won=mode==Mode::Won;const bool better=threadlight::settings::isBetterRun(appSettings,won,wave+1,rewards.score());
     if(threadlight::settings::grantVioletAscendant(appSettings,won,wave+1,false,false)){
      violetUnlockedThisRun=true;settingsEdits.dirty=true;
     }
     if(better){appSettings.bestWave=wave+1;appSettings.bestWon=won;appSettings.bestScore=rewards.score();appSettings.bestKills=rewards.counters().killed;settingsEdits.dirty=true;}
    }
    if(settingsEdits.dirty&&!m6RenderCheck)saveSettings();
    stepClock.clear();dashPressPending=collectionContinuePending=false;movementPressPending.clear();capturePresentation();
   }
   const float alpha=mode==Mode::Play?stepClock.presentationAlpha():1.f;
   const V shownPlayer=threadlight::presentation::blend(previousPlayer,player,alpha);
   const float shownShake=threadlight::presentation::decay(previousShake,shake,alpha);
   const float shownDamageFeedback=threadlight::presentation::decay(previousDamageFeedback,damageFeedback,alpha);
   const float shownDashEchoFade=threadlight::presentation::decay(previousDashEchoFade,dashEchoFade,alpha);

  const auto drawStart=perf?PerfClock::now():PerfClock::time_point{};
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
  auto drawFields=[&](const field::Pool& pool){
   for(const auto& patch:pool.items()){
    if(patch.state==field::SlotState::Empty)continue;
    const auto& spec=patch.spec;const bool warning=patch.state==field::SlotState::Warning,fade=patch.state==field::SlotState::Fading;
    const float opacity=fade?patch.fadeRemaining/30.f:1.f;
    const Color baseLine=fade?C(120,182,177,100):warning?C(242,155,143,190):C(255,112,140,225);
    const Color line=Fade(baseLine,opacity*baseLine.a/255.f);
    const Color fill=fade?C(0,0,0,0):warning?C(218,113,137,15):C(190,63,100,48);
    if(spec.shape==field::Shape::Disk){
     const Vector2 p{spec.center.x,spec.center.y};DrawCircleV(p,spec.radius,fill);DrawCircleLines((int)p.x,(int)p.y,spec.radius,line);
     if(warning)DrawCircleLines((int)p.x,(int)p.y,4,line);
    }else if(spec.shape==field::Shape::Capsule){
     const Vector2 a{spec.a.x,spec.a.y},b{spec.b.x,spec.b.y};const auto d=root::unit(root::sub(spec.b,spec.a));const Vector2 n{-d.y*spec.halfWidth,d.x*spec.halfWidth};
     DrawLineEx(a,b,spec.halfWidth*2,fill);DrawLineEx({a.x+n.x,a.y+n.y},{b.x+n.x,b.y+n.y},1.2f,line);DrawLineEx({a.x-n.x,a.y-n.y},{b.x-n.x,b.y-n.y},1.2f,line);
     DrawCircleLines((int)a.x,(int)a.y,spec.halfWidth,line);DrawCircleLines((int)b.x,(int)b.y,spec.halfWidth,line);
    }else{
     const float radius=field::activeRadius(patch);
     const float inner=std::max(0.f,(warning?std::min(spec.radiusStart,spec.radiusEnd):radius)-spec.ringHalfWidth);
     const float outer=(warning?std::max(spec.radiusStart,spec.radiusEnd):radius)+spec.ringHalfWidth;
     const float angle=std::atan2(spec.gapDirection.y,spec.gapDirection.x)*57.2957795f,gap=spec.gapHalfAngleRadians*57.2957795f;
     for(int side=0;side<2;++side){const float a=angle+side*180+gap,b=angle+(side+1)*180-gap;
      DrawRing({spec.center.x,spec.center.y},inner,outer,a,b,48,fill);DrawRingLines({spec.center.x,spec.center.y},inner,outer,a,b,48,line);
     }
    }
   }
  };
  drawFields(ground);drawFields(boss.fields);
  if(needleFlashTicks>0)DrawCircleLines((int)needleFlash.x,(int)needleFlash.y,8.f+(24-needleFlashTicks)*.6f,C(217,234,174,(unsigned char)(needleFlashTicks*8)));
  if(chapter::encounter(wave).boss&&boss.corridor.enabled&&root::alive(boss)){
   DrawLineEx({boss.corridor.a.x,boss.corridor.a.y},{boss.corridor.b.x,boss.corridor.b.y},112,C(90,190,177,9));
  }
  // A short Catmull-Rom subdivision keeps the wake organic without changing
  // the sampled collision segments or allocating temporary geometry.
  // A newly sampled silk endpoint belongs to the current tick. Draw it at
  // the interpolated player instead of letting the silk lead the player.
  auto silkPoint=[&](size_t i){return threadlight::presentation::silkPoint(thread[i],i+1==thread.size(),shownPlayer);};
  auto silkAge=[&](size_t i){return threadlight::presentation::blend(thread[i].previousAge,thread[i].age,alpha);};
  if(thread.size()>1){
   for(size_t i=1;i<thread.size();i++){
    const V p0=silkPoint(i>1?i-2:i-1),p1=silkPoint(i-1),p2=silkPoint(i),p3=silkPoint(i+1<thread.size()?i+1:i);
    V a=catmullRom(p0,p1,p2,p3,0.f);
    const float age0=silkAge(i-1),age1=silkAge(i);
    for(int step=1;step<=3;step++){
     const float t0=(float)(step-1)/3.f,t1=(float)step/3.f;
     const V b=catmullRom(p0,p1,p2,p3,t1);
     const float age=age0+(age1-age0)*(t0+t1)*.5f;
     const float shimmer=.5f+.5f*std::sin(time*5.f-age*1.6f);
     const auto silk=threadlight::visual::silkAppearance(age,shimmer,stats.activeSilkSeconds);
     Color halo=ColorLerp(paletteColor(playerPalette.silkHaloFresh),paletteColor(playerPalette.silkHaloAged),silk.ageBlend);
     Color body=ColorLerp(paletteColor(playerPalette.silkBodyFresh),paletteColor(playerPalette.silkBodyAged),silk.ageBlend);
     Color core=ColorLerp(paletteColor(playerPalette.silkCoreFresh),paletteColor(playerPalette.silkCoreAged),silk.ageBlend);
     halo.a=(unsigned char)std::lround(silk.haloOpacity*255.f);
     body.a=(unsigned char)std::lround(silk.bodyOpacity*(.94f+.06f*shimmer)*255.f);
     core.a=(unsigned char)std::lround(silk.coreOpacity*255.f);
     DrawLineEx(cv(a),cv(b),8.5f+(stats.silkHalfWidth-5.f)*2.f-1.5f*silk.ageBlend,halo);
     DrawLineEx(cv(a),cv(b),4.4f+(stats.silkHalfWidth-5.f)-1.0f*silk.ageBlend,body);
     DrawLineEx(cv(a),cv(b),1.75f-.35f*silk.ageBlend,core);
     a=b; // Adjacent subdivisions share the exact same evaluated endpoint.
    }
   }
  }
  if(offensiveEcho.active()&&!offensiveEcho.recording){
   const float fade=.35f+.65f*(float)offensiveEcho.remainingTicks/afterimage::kEchoTicks;
   for(size_t i=1;i<offensiveEcho.count;++i){
    const Vector2 a{offensiveEcho.path[i-1].x,offensiveEcho.path[i-1].y},b{offensiveEcho.path[i].x,offensiveEcho.path[i].y};
    DrawLineEx(a,b,afterimage::kHalfWidth*2,paletteColor(cosmetics::withAlpha(playerPalette.afterimageHalo,(unsigned char)(30*fade))));
    DrawLineEx(a,b,3.f,paletteColor(cosmetics::withAlpha(playerPalette.afterimageCore,(unsigned char)(190*fade))));
    if(i%4==0)DrawCircleV(a,2.4f,paletteColor(cosmetics::withAlpha(playerPalette.afterimageSpark,(unsigned char)(160*fade))));
   }
  }
  // A short, bright trail behind the dashing player communicates direction
  // and distance, then fades quickly without covering enemies or silk.
  if(shownDashEchoFade>0&&previousDashEchoCount>1){
   const float fade=std::clamp(shownDashEchoFade/.22f,0.f,1.f);
   for(size_t i=previousDashEchoCount-1;i>0;i--){
    const float ageWeight=.25f+.75f*(1.f-(float)(i-1)/(float)(previousDashEchoCount-1));
    const unsigned char glow=(unsigned char)(86.f*fade*ageWeight),core=(unsigned char)(230.f*fade*ageWeight);
    DrawLineEx(cv(previousDashEcho[i]),cv(previousDashEcho[i-1]),7.f*fade,paletteColor(cosmetics::withAlpha(playerPalette.dashHalo,glow)));
    DrawLineEx(cv(previousDashEcho[i]),cv(previousDashEcho[i-1]),2.4f*fade,paletteColor(cosmetics::withAlpha(playerPalette.dashCore,core)));
    DrawCircleV(cv(previousDashEcho[i]),radius*.72f,paletteColor(cosmetics::withAlpha(playerPalette.dashSpark,(unsigned char)(145.f*fade*ageWeight))));
   }
   DrawLineEx(cv(previousDashEcho[0]),cv(shownPlayer),7.f*fade,paletteColor(cosmetics::withAlpha(playerPalette.dashHalo,(unsigned char)(86.f*fade))));
   DrawLineEx(cv(previousDashEcho[0]),cv(shownPlayer),2.4f*fade,paletteColor(cosmetics::withAlpha(playerPalette.dashCore,(unsigned char)(230.f*fade))));
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
  for(const auto& f:foes)if(f.kind==battle::EnemyKind::Spitter&&ranged::committed(f.rangedState)){
   const auto source=ranged::muzzle({f.p.x,f.p.y},f.rangedState.aimDirection);
   const auto end=root::rayEnd(source,f.rangedState.aimDirection,ranged::kProjectileSpeed*3.f,bossArena(),ranged::kProjectileRadius);
   const Vector2 a{source.x,source.y},b{end.x,end.y};
   const Color line=f.rangedState.aimLocked?C(241,147,187,195):C(202,141,180,110);
   if(f.rangedState.aimLocked){DrawLineEx(a,b,1.2f,line);DrawCircleLines((int)a.x,(int)a.y,6.f,line);}
   else for(int dash=0;dash<12;dash+=2){
    const auto p0=root::interpolate(source,end,dash/12.f),p1=root::interpolate(source,end,(dash+1)/12.f);
    DrawLineEx({p0.x,p0.y},{p1.x,p1.y},1.f,line);
   }
  }
  if(pulseTicks>0){const float t=1-pulseTicks/24.f;DrawCircleLines((int)pulseCenter.x,(int)pulseCenter.y,pulseRadius*(.8f+.2f*t),C(160,239,215,(unsigned char)(190*(1-t))));}
  if(chapter::encounter(wave).boss&&root::alive(boss)&&boss.kind!=root::Kind::Rootwarden){
   const auto shown=root::interpolate(boss.previousP,boss.p,alpha);const Vector2 p{shown.x,shown.y};
   const bool open=root::exposed(boss),transition=boss.stage==root::Stage::Transition;
   const Color edge=open?C(255,224,151):transition?C(150,238,225):boss.kind==root::Kind::Dancer?C(225,166,231):C(244,156,129);
   if(root::spatial(boss)){
    const bool heart=boss.kind==root::Kind::Heart;const float beat=1.f+(heart?.045f:.02f)*std::sin((float)boss.metrics.ticks*.055f);
    if(boss.stage==root::Stage::StepWarning||boss.stage==root::Stage::StepActive){
     const Vector2 a{boss.stepStart.x,boss.stepStart.y},b{boss.stepEnd.x,boss.stepEnd.y};const float r=root::contactRadius(boss);
     const auto d=root::unit(root::sub(boss.stepEnd,boss.stepStart));const Vector2 n{-d.y*r,d.x*r};
     DrawLineEx(a,b,2*r,C(235,130,161,25));DrawLineEx({a.x+n.x,a.y+n.y},{b.x+n.x,b.y+n.y},1.6f,edge);DrawLineEx({a.x-n.x,a.y-n.y},{b.x-n.x,b.y-n.y},1.6f,edge);DrawCircleLines((int)b.x,(int)b.y,r,edge);
    }
    if(!heart&&boss.stage==root::Stage::RootWarning){
     DrawLineEx({boss.patternOrigin.x,boss.patternOrigin.y},{boss.relocateEnd.x,boss.relocateEnd.y},2,C(145,222,205,130));
     DrawCircleLines((int)boss.relocateEnd.x,(int)boss.relocateEnd.y,root::contactRadius(boss),C(145,222,205,130));
    }
    DrawPoly(p,heart?12:6,root::vulnerableRadius(boss)*beat,heart?15.f:30.f,C(28,26,48));
    DrawPolyLinesEx(p,heart?12:6,root::vulnerableRadius(boss)*beat,heart?15.f:30.f,2,edge);
    for(int i=0;i<(heart?8:6);++i){const float a=i*chapter::kPi*2/(heart?8:6);const Vector2 out{p.x+std::cos(a)*(heart?61.f:57.f),p.y+std::sin(a)*(heart?61.f:57.f)};
     DrawLineEx(p,out,heart?3.f:1.5f,edge);DrawPoly(out,heart?3:4,heart?7.f:6.f,a*57.3f,edge);
    }
   }else if(boss.kind==root::Kind::Dancer){
    if(boss.stage==root::Stage::StepWarning||boss.stage==root::Stage::StepActive){
     const Vector2 a{boss.stepStart.x,boss.stepStart.y},b{boss.stepEnd.x,boss.stepEnd.y};
     const float r=root::contactRadius(boss);const auto d=root::unit(root::sub(boss.stepEnd,boss.stepStart));
     const Vector2 n{-d.y*r,d.x*r};
     DrawLineEx(a,b,2*r,C(232,133,164,24));
     DrawLineEx({a.x+n.x,a.y+n.y},{b.x+n.x,b.y+n.y},1.5f,C(252,177,177,210));
     DrawLineEx({a.x-n.x,a.y-n.y},{b.x-n.x,b.y-n.y},1.5f,C(252,177,177,210));
     DrawCircleLines((int)b.x,(int)b.y,r,C(252,177,177,210));
    }
    if(boss.counterstepping)DrawLineEx({boss.counterStart.x,boss.counterStart.y},{boss.counterEnd.x,boss.counterEnd.y},2,C(140,219,212,110));
    DrawPoly(p,4,root::vulnerableRadius(boss),45,C(33,29,52));
    DrawPolyLinesEx(p,4,root::vulnerableRadius(boss),45,2,edge);
    DrawLineEx({p.x-44,p.y},{p.x+44,p.y},3,edge);DrawLineEx({p.x,p.y-44},{p.x,p.y+44},3,edge);
   }else{
    if(boss.stage==root::Stage::RootWarning||boss.stage==root::Stage::RootActive)
     for(int i=boss.shotCursor;i<boss.shotCount;++i){const auto& shot=boss.shots[i];
      if(shot.bud>=0&&!boss.buds[shot.bud].health.alive())continue;
      const auto end=root::rayEnd(shot.p,shot.direction,540,bossArena(),5);
      DrawLineEx({shot.p.x,shot.p.y},{end.x,end.y},10,C(238,142,156,18));
      DrawLineEx({shot.p.x,shot.p.y},{end.x,end.y},1.2f,C(246,165,150,175));
     }
    for(int i=0;i<boss.budCount;++i)if(boss.buds[i].health.alive()){
     const auto& b= boss.buds[i];DrawCircleV({b.p.x,b.p.y},16,C(26,51,56));DrawCircleLines((int)b.p.x,(int)b.p.y,16,C(168,230,211));
     DrawText(TextFormat("%d",(b.health.hp+99)/100),(int)b.p.x-4,(int)b.p.y-5,10,C(211,244,223));
    }
    DrawPoly(p,10,root::vulnerableRadius(boss),0,C(40,30,40));
    DrawPolyLinesEx(p,10,root::vulnerableRadius(boss),0,2,edge);
    for(int i=0;i<6;++i){float a=i*chapter::kPi/3.f;DrawLineEx(p,{p.x+std::cos(a)*56,p.y+std::sin(a)*56},2,edge);}
   }
   DrawCircleLines((int)p.x,(int)p.y,root::contactRadius(boss),C(255,125,145));
   DrawCircleLines((int)p.x,(int)p.y,root::vulnerableRadius(boss),Fade(edge,.28f));
   DrawCircleV(p,open?8.f:5.f,edge);
   for(const auto& shot:boss.bullets.items())if(shot.state==ranged::SlotState::Flying){
    const auto q=root::interpolate(shot.previousP,shot.p,alpha);DrawPoly({q.x,q.y},3,shot.spec.radius,20,C(255,175,128));DrawCircleV({q.x,q.y},2,C(255,232,193));
   }
  }
  if(chapter::encounter(wave).boss&&root::alive(boss)&&anchorState.targetId==bossRewardId&&anchorState.windowTicks>0){
   const auto marked=root::interpolate(boss.previousP,boss.p,alpha);
   DrawPolyLinesEx({marked.x,marked.y},4,root::vulnerableRadius(boss)+8,45,1.4f,C(239,214,148,210));
  }
  if(chapter::encounter(wave).boss&&root::alive(boss)&&boss.kind==root::Kind::Rootwarden){
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
  if(chapter::encounter(wave).boss&&root::alive(boss)){
   const auto shown=root::interpolate(boss.previousP,boss.p,alpha);const Vector2 pos{shown.x,shown.y};
   const bool open=root::exposed(boss);
   const float barWidth=160.f,barX=std::clamp(pos.x-barWidth*.5f,8.f,(float)SW-barWidth-8.f);
   const float offset=root::vulnerableRadius(boss)+22.f;
   const float barY=pos.y-offset<80.f?pos.y+offset:pos.y-offset;
   DrawRectangle((int)barX-2,(int)barY-17,(int)barWidth+4,26,C(5,10,24,220));
   DrawText(TextFormat("%s / PHASE %d",root::name(boss),boss.phase),(int)barX,(int)barY-14,10,C(239,216,192));
   DrawRectangle((int)barX,(int)barY,(int)barWidth,5,C(61,45,62));
   DrawRectangle((int)barX,(int)barY,(int)(barWidth*std::clamp((float)boss.health.hp/boss.health.maxHp,0.f,1.f)),5,open?C(255,218,141):C(225,121,151));
   const float threshold1=boss.kind==root::Kind::Rootwarden?.55f:boss.kind==root::Kind::Heart?.75f:boss.kind==root::Kind::Dancer?.70f:.65f;
   const float threshold2=boss.kind==root::Kind::Heart?.50f:boss.kind==root::Kind::Dancer?.35f:.30f;
   DrawRectangle((int)(barX+barWidth*threshold1)-1,(int)barY,2,5,C(255,239,210));
   if(boss.kind!=root::Kind::Rootwarden)DrawRectangle((int)(barX+barWidth*threshold2)-1,(int)barY,2,5,C(255,239,210));
   if(boss.kind==root::Kind::Heart)DrawRectangle((int)(barX+barWidth*.25f)-1,(int)barY,2,5,C(255,239,210));
  }
  for(const auto& f:foes){const auto shown=f.present(alpha);
   const float flash=threadlight::visual::hitFlashStrength(shown.hitFlash);
   const bool heavy=f.kind==battle::EnemyKind::Brute,skitterBody=f.kind==battle::EnemyKind::Skitter,spitterBody=f.kind==battle::EnemyKind::Spitter,callerBody=f.kind==battle::EnemyKind::BriarCaller,sowerBody=f.kind==battle::EnemyKind::Sower;
   const int sides=sowerBody?6:callerBody?5:spitterBody?5:skitterBody?3:heavy?6:4;
   const Color base=sowerBody?C(177,217,144):callerBody?C(222,185,111):spitterBody?C(226,139,194):skitterBody?C(255,164,125):heavy?C(255,116,178):C(255,97,132);
   const Color edge=ColorLerp(base,C(207,255,246,255),flash);
   const float bob=std::sin(shown.phase*5.f)*2.2f,breath=1.f+std::sin(shown.phase*3.2f)*.035f;
   const Vector2 pos={shown.p.x,shown.p.y+bob};
   if(callerBody){
    const float aura=caller::auraRadius(f.briarCaller);
    DrawCircleV(pos,aura,C(141,207,111,8));
    DrawCircleLines((int)pos.x,(int)pos.y,aura,C(168,231,142,105));
   }
   if(f.slow.remainingTicks>0)DrawCircleLines((int)pos.x,(int)pos.y,f.r+5,C(124,210,215,155));
   if(anchorState.targetId==f.rewardId&&anchorState.windowTicks>0)DrawPolyLinesEx(pos,4,f.r+8,45,1.4f,C(239,214,148,210));
   DrawCircleV({pos.x,pos.y+f.r*.48f},f.r*.7f,C(3,9,22,95));
   DrawCircleV(pos,f.r+8,C(255,63,123,(unsigned char)(14+flash*30)));
   DrawPoly(pos,sides,f.r*breath,time*40.f,C(25,22,45));
   DrawPolyLinesEx(pos,sides,f.r*breath,time*40.f,1.5f+flash*1.1f,edge);
   if(sowerBody){DrawCircleLines((int)pos.x,(int)pos.y,f.r*.55f,edge);DrawCircleV(pos,3,C(234,232,168));}
   if(spitterBody){
    const float charge=ranged::committed(f.rangedState)?1.f-(float)f.rangedState.remaining/ranged::kWindupTicks:0.f;
    DrawCircleV(pos,4.f+charge*3.f,C(246,174,211,(unsigned char)(120+100*charge)));
    const auto direction=f.rangedState.aimDirection;
    DrawLineEx(pos,{pos.x+direction.x*12.f,pos.y+direction.y*12.f},2.f,C(255,217,230));
   }else if(heavy){const float arc=time*25.f;DrawCircleV(pos,3.f,C(255,116,178,175));DrawRing(pos,f.r*.67f,f.r*.75f,arc,arc+55.f,18,C(255,173,207,120));}
   else if(callerBody){
    const int petals=std::clamp(f.briarCaller.members,1,5);
    for(int i=0;i<petals;++i){const float a=time*.45f+6.2831853f*i/petals;DrawCircleV({pos.x+std::cos(a)*f.r*.52f,pos.y+std::sin(a)*f.r*.52f},std::max(2.2f,f.r*.12f),C(255,229,159,235));}
    if(f.briarCaller.members>1)DrawText(TextFormat("x%d",f.briarCaller.members),(int)pos.x-8,(int)(pos.y-f.r-13),9,C(231,251,200));
   }
   else DrawLineEx({pos.x-6,pos.y+5},{pos.x+6,pos.y+5},1.f,C(255,141,167,165));
   DrawCircleV({pos.x-4,pos.y-2},2.2f,C(255,238,220));DrawCircleV({pos.x+4,pos.y-2},2.2f,C(255,238,220));
   if(flash>0)DrawCircleV(pos,f.r*.56f,C(219,255,249,(unsigned char)(105.f*flash)));
   if(f.health.hp<f.health.maxHp){DrawRectangle((int)(shown.p.x-f.r),(int)(shown.p.y-f.r-10),((int)(2*f.r)),3,C(50,48,71));DrawRectangle((int)(shown.p.x-f.r),(int)(shown.p.y-f.r-10),(int)(2*f.r*std::clamp((float)f.health.hp/f.health.maxHp,0.f,1.f)),3,C(255,118,157));}
  }
  for(const auto& source:particles){auto p=source.present(alpha);float a=std::clamp(p.life/p.maxLife,0.f,1.f);DrawCircleV(cv(p.p),p.size*a,Fade(p.color,a));}
  for(const auto& shot:projectiles.items())if(shot.state==ranged::SlotState::Flying){
   const auto pos=root::interpolate(shot.previousP,shot.p,alpha);
   const float angle=std::atan2(shot.velocity.y,shot.velocity.x);
   const Vector2 head{pos.x,pos.y},tail{pos.x-std::cos(angle)*11.f,pos.y-std::sin(angle)*11.f};
   DrawLineEx(tail,head,2.f,C(229,114,172,140));
   DrawPoly(head,3,ranged::kProjectileRadius,angle*57.29578f,C(255,156,194));
   DrawCircleV(head,2.f,C(255,232,222));
  }
  if(mode!=Mode::Title){
   const float impact=threadlight::visual::damageImpactStrength(shownDamageFeedback);
   const bool dashVisual=dashTime>0||dashVisualActive;
   DrawCircleV(cv(shownPlayer),31,paletteColor(cosmetics::withAlpha(playerPalette.playerAuraOuter,18)));
   DrawCircleV(cv(shownPlayer),21,paletteColor(cosmetics::withAlpha(playerPalette.playerAuraInner,34)));
   if(impact>0)DrawCircleV(cv(shownPlayer),radius+13,C(242,76,101,(unsigned char)(35*impact)));
   const Color rim=dashVisual?paletteColor(playerPalette.playerDashRim):ColorLerp(paletteColor(playerPalette.playerRim),C(255,133,151,235),impact*.72f);
   const Color core=dashVisual?paletteColor(playerPalette.playerDashRim):ColorLerp(paletteColor(playerPalette.playerCore),C(255,171,181,255),impact*.48f);
   DrawCircleV(cv(shownPlayer),radius+4,rim);
   DrawCircleV(cv(shownPlayer),radius,core);
   if(dashVisual)DrawCircleLines((int)shownPlayer.x,(int)shownPlayer.y,radius+8,paletteColor(playerPalette.playerDashRim));
   if(cocoon::barrierActive(cocoonState))DrawCircleLines((int)shownPlayer.x,(int)shownPlayer.y,radius+9,C(223,241,188,215));
   if(impact>0){const float ringRadius=threadlight::visual::damageRingRadius(radius,shownDamageFeedback);DrawRing(cv(shownPlayer),ringRadius,ringRadius+1.6f,0,360,48,C(255,118,139,(unsigned char)(100*impact)));}
  }
  rlPopMatrix();
  const bool showRunHud=mode==Mode::Play||mode==Mode::Pause||mode==Mode::Upgrade||mode==Mode::Signature||
      mode==Mode::Lost||mode==Mode::Won||(mode==Mode::Settings&&settingsReturn==Mode::Pause);
  const auto hud=threadlight::layout::hudLayout(SW,SH);
  if(showRunHud){
   const float hudHeight=threadlight::layout::kHudBottom;
   const int titleSize=SW<1100?22:25;
   DrawRectangle(0,0,SW,(int)hudHeight,C(5,10,24,235));
   DrawLine(0,(int)hudHeight,SW,(int)hudHeight,C(60,152,156,80));
   drawText("THREADLIGHT",26,22,titleSize,C(200,255,243));
   if(bossFixture>=0||waveFixture>=0)drawText("DEV FIXTURE",26,49,10,C(255,213,131));
   else if(SW>=1180)drawText("THE BRIAR CIRCUIT",214,31,13,C(91,167,179));
   DrawText(TextFormat("VITALITY %.0f / %d",health.hp/100.f,health.maxHp/100),(int)hud.healthX,17,11,C(120,164,178));
   DrawRectangle((int)hud.healthX,38,(int)hud.healthWidth,10,C(34,49,67));
   DrawRectangle((int)hud.healthX,38,(int)(hud.healthWidth*std::clamp((float)health.hp/health.maxHp,0.f,1.f)),10,health.hp>health.maxHp*.35f?C(72,225,190):C(255,91,127));
   drawText(tutorial?"TUTORIAL":TextFormat("WAVE %d / %d",wave+1,chapter::kEncounterCount),(int)hud.waveX,22,18,C(217,239,242));
   drawText(TextFormat("SPARKS %lld / %d",(long long)rewards.remainder(),rewards.nextThreshold()),(int)hud.sparksX,24,15,C(255,213,131));
   drawText(TextFormat("SCORE %06llu",(unsigned long long)rewards.score()),(int)hud.scoreX,24,17,C(177,208,221));
   drawText(tutorial?"PRACTICE":chapter::encounter(wave).boss&&foundation::isCombat(lifecycle.phase())?"DEFEAT THE GUARDIAN":TextFormat("%s %.1fs",foundation::phaseName(lifecycle.phase()),lifecycle.remainingTicks()/120.f),(int)hud.waveX,51,11,C(121,196,197));
   drawText(TextFormat("CREDITS %llu",(unsigned long long)rewards.pendingCredits()),(int)hud.sparksX,51,11,C(177,208,221));
  }
  if(showRunHud&&catalog::ownedCount(signatureCatalog)&&!chapter::encounter(wave).boss)
   DrawText(TextFormat("SIGNATURES %d / 4",catalog::ownedCount(signatureCatalog)),26,60,11,C(228,216,151));
  if(showRunHud&&cocoonReward.owned)DrawText(cocoon::barrierActive(cocoonState)?TextFormat("COCOON %.1f HP / %.1fs",cocoonState.barrierCenti/100.f,cocoonState.remainingTicks/120.f):cocoonState.cooldownTicks?TextFormat("COCOON %.1fs",cocoonState.cooldownTicks/120.f):"COCOON / LOOP READY",(int)hud.healthX,55,10,C(193,224,174));
  if(showRunHud&&bossPhaseNotice>0&&chapter::encounter(wave).boss){
   const char* notice=TextFormat("%s / PHASE %d",root::name(boss),boss.phase);
   const int width=MeasureText(notice,18);DrawRectangle(SW/2-width/2-12,85,width+24,30,C(5,10,24,225));
   DrawText(notice,SW/2-width/2,91,18,C(255,218,161));
  }
  if(showRunHud&&upgrades::rank(ranks,upgrades::Id::Needlecatch)>0)
   DrawText(needlecatch.cooldownTicks?TextFormat("NEEDLECATCH %.1fs",needlecatch.cooldownTicks/120.f):"NEEDLECATCH / READY",24,(int)hud.footerY-8,10,C(200,218,168));
  if(showRunHud&&upgrades::rank(ranks,upgrades::Id::Bloodstitch)>0){
   const auto track=chapterHud::layout(SW,SH).dashTrack;
   DrawText(TextFormat("BLOODSTITCH %d/%d  HEALED %.0f/%.0f",bloodstitch.kills,stats.bloodKillsPerHeal,bloodstitch.healedCenti/100.f,stats.bloodWaveCap/100.f),SW-285,(int)track.y-14,10,C(218,163,180));
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
  if(tutorial&&mode==Mode::Play){
   DrawRectangle(SW/2-260,112,520,91,C(5,10,24,225));
   const char* instruction=!tutorialMoved?"MOVE WITH WASD OR ARROW KEYS":!tutorialDashed?"MOVE AND PRESS SPACE TO DASH":rewards.counters().killed<10?"WEAVE SILK THROUGH THE HUNTERS":"PRACTICE COMPLETE / ENTER TO START";
   DrawText(instruction,SW/2-MeasureText(instruction,17)/2,122,17,C(217,250,236));
   DrawText(tutorialDashed?TextFormat("Hunters defeated: %llu / 10   -   Walk over sparks to collect them",(unsigned long long)rewards.counters().killed):"Your movement leaves a damaging silk trail.",SW/2-240,148,13,C(165,211,207));
   DrawText("P: pause   |   Hold B for 3 seconds to skip",SW/2-240,173,13,C(255,213,131));
   DrawRectangle(SW/2+155,191,85,3,C(48,70,78));DrawRectangle(SW/2+155,191,(int)(85*tutorialSkipHold/3.f),3,C(255,213,131));
  }
  if(showRunHud){
   const auto dash=chapterHud::dashView(dashCd,dashCycleDuration,dashTime>0||dashVisualActive,
    playerWasMoving);
   const auto track=chapterHud::layout(SW,SH).dashTrack;
   const Color activeDashColor=dash.state==chapterHud::DashState::Active?C(148,236,255):dash.state==chapterHud::DashState::Recharging?C(91,156,184):C(112,234,203);
   const Color dashColor=mode==Mode::Play?activeDashColor:Fade(activeDashColor,.45f);
   DrawRectangle((int)track.x,(int)track.y,(int)track.width,2,C(42,64,80));
   DrawRectangle((int)track.x,(int)track.y,(int)(track.width*dash.fill),2,dashColor);
   DrawText(dash.state==chapterHud::DashState::Active?"DASH / ACTIVE":dash.state==chapterHud::DashState::Recharging?TextFormat("DASH / %.1fs",dash.remainingTenths/10.f):dash.state==chapterHud::DashState::ReadyMove?"DASH / MOVE":"DASH / READY",(int)track.x,(int)track.y-14,12,dashColor);
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
  if(mode==Mode::Title&&titleView==0){
   const Rectangle bounds=panel(waveFixture>=0?(waveFixtureWave==21?"M7 / SOWER FIXTURE":"M7 / WAVE 8 FIXTURE"):bossFixture<0?"M7 / 35-WAVE DEVELOPMENT CAMPAIGN":bossFixtureWave==34?"M7 / BRIAR HEART FIXTURE":bossFixtureWave==27?"M7 / HOLLOW WEAVER FIXTURE":bossFixtureWave==13?"M7 / NEEDLE DANCER FIXTURE":bossFixtureWave==20?"M7 / THORN CONDUCTOR FIXTURE":"M7 / ROOTWARDEN FIXTURE","THREADLIGHT",waveFixture>=0?"Isolated encounter fixture; no campaign checkpoint.":bossFixture<0?"Weave through five chapters. Complete the Briar Circuit.":"Isolated boss test; no campaign progress or checkpoint.");
   DrawText("Move to weave a fading thread. Circle the hunters; let silk do the cutting.",(int)bounds.x+52,(int)bounds.y+153,15,C(175,211,213));
   DrawText("WASD / arrows move     SPACE dash     P pauses",(int)bounds.x+52,(int)bounds.y+181,15,C(121,196,197));
   const char* labels[]={"START GAME","ENEMY CODEX","BEST RUN","SETTINGS","EXIT GAME"};
   for(int i=0;i<5;i++)drawButton(mainButton(i),labels[i],mainSelection==i,hovered(mainButton(i)),15);
   DrawText("UP / DOWN SELECT     ENTER CONFIRMS     ESC OPENS EXIT CONFIRMATION",(int)bounds.x+52,(int)bounds.y+431,12,C(108,167,178));
  }
  if(mode==Mode::Title&&titleView==1){
   DrawRectangle(0,0,SW,SH,C(3,7,18,215));Rectangle box{SW*.5f-420,SH*.5f-260,840,520};
   DrawRectangleRounded(box,.035f,12,C(11,22,40,250));DrawRectangleRoundedLines(box,.035f,12,C(91,216,197,155));
   DrawText("ENEMY CODEX",(int)box.x+30,(int)box.y+22,26,C(226,255,247));
   DrawText("Encounter an enemy to unlock its entry.",(int)box.x+30,(int)box.y+55,13,C(165,201,209));
   for(int i=0;i<11;++i)drawButton(codexButton(i),threadlight::bestiary::unlocked(appSettings.codexSeen,i)?threadlight::bestiary::entries[i].name:"???",codexSelection==i,hovered(codexButton(i)),12);
   const bool unlocked=threadlight::bestiary::unlocked(appSettings.codexSeen,codexSelection);
   const auto& entry=threadlight::bestiary::entries[codexSelection];
   const int x=(int)box.x+300,y=(int)box.y+92;
   DrawText(unlocked?entry.name:"UNKNOWN",x,y,21,C(255,218,161));
   if(unlocked){
    DrawText("BEHAVIOR",x,y+47,12,C(107,228,206));DrawText(entry.behavior,x,y+68,14,C(211,240,225));
    DrawText("WATCH FOR",x,y+112,12,C(107,228,206));DrawText(entry.tell,x,y+133,14,C(211,240,225));
    DrawText("COUNTER",x,y+177,12,C(107,228,206));DrawText(entry.counter,x,y+198,14,C(211,240,225));
   }else DrawText("Encounter this enemy in a normal run to learn more.",x,y+60,14,C(165,201,209));
   drawButton(titleBack(),"BACK / ESC",true,hovered(titleBack()),13);
   DrawText("UP / DOWN SELECT",(int)box.x+30,(int)box.y+485,11,C(108,167,178));
  }
  if(mode==Mode::Title&&titleView==2){
   panel("PERSONAL RECORD","BEST RUN","Ranked by victory, wave reached, then score.");
   if(appSettings.bestWave){
    DrawText(appSettings.bestWon?"CAMPAIGN CLEARED":"FURTHEST RUN",SW/2-220,SH/2-30,16,C(255,213,131));
    DrawText(TextFormat("WAVE %d / %d",appSettings.bestWave,chapter::kEncounterCount),SW/2-220,SH/2+5,21,C(217,250,236));
    DrawText(TextFormat("SCORE %06llu     KILLS %llu",(unsigned long long)appSettings.bestScore,(unsigned long long)appSettings.bestKills),SW/2-220,SH/2+44,17,C(179,225,216));
   }else DrawText("Finish a normal run to set your first record.",SW/2-220,SH/2+20,16,C(179,225,216));
   drawButton(titleBack(),"BACK / ESC",true,hovered(titleBack()),13);
  }
  if(mode==Mode::Pause&&!inventoryOpen&&!pauseConfirm){
   panel("THE GARDEN HOLDS ITS BREATH","PAUSED","Your thread is waiting.");
   const char* labels[]={"RESUME","SETTINGS","UPGRADES & SIGNATURES","RESTART RUN","RETURN TO MAIN MENU"};
   for(int i=0;i<5;i++)drawButton(pauseButton(i),labels[i],pauseSelection==i,hovered(pauseButton(i)),15);
   DrawText("P RESUMES     I INVENTORY     ESC LEAVES",SW/2-160,SH/2+197,12,C(108,167,178));
  }
  if((mode==Mode::Pause||mode==Mode::Lost||mode==Mode::Won)&&inventoryOpen){
   DrawRectangle(0,0,SW,SH,C(3,7,18,215));
   Rectangle box{SW*.5f-420,SH*.5f-270,840,540};DrawRectangleRounded(box,.035f,12,C(11,22,40,250));DrawRectangleRoundedLines(box,.035f,12,C(91,216,197,155));
   DrawText("YOUR BUILD",(int)box.x+30,(int)box.y+22,26,C(226,255,247));
   DrawText("UPGRADES / RANK",(int)box.x+30,(int)box.y+62,13,C(107,228,206));
   for(int i=0;i<upgrades::kPermanentCount;++i){const auto id=(upgrades::Id)i;const int col=i/6,row=i%6;
    DrawText(TextFormat("%s  %d / %d",upgrades::name(id),upgrades::rank(ranks,id),upgrades::maximum(id)),(int)box.x+30+col*245,(int)box.y+88+row*23,13,upgrades::rank(ranks,id)?C(211,240,225):C(110,145,155));}
   DrawText("SIGNATURES",(int)box.x+525,(int)box.y+62,13,C(255,213,131));
   int row=0;for(size_t i=0;i<catalog::kCount;++i)if(signatureCatalog.owned[i]){const auto id=(catalog::Id)i;const auto lines=catalog::descriptionLines(id);
    DrawText(catalog::name(id).data(),(int)box.x+525,(int)box.y+88+row*57,15,C(239,224,175));
    DrawText(lines[0].data(),(int)box.x+525,(int)box.y+108+row*57,11,C(165,201,209));++row;}
   if(!row)DrawText("None yet",(int)box.x+525,(int)box.y+88,14,C(110,145,155));
   DrawText(TextFormat("Silk %.2f   Speed %.0f   Dash %.2fs   Max HP %d",stats.silkDamage/100.f,stats.moveSpeed,stats.dashCooldownSeconds,stats.maxHp/100),(int)box.x+30,(int)box.y+258,15,C(172,211,211));
   DrawText(TextFormat("Silk duration %.1fs   Width %.0f   Damage reduction %d%%",stats.activeSilkSeconds,stats.silkHalfWidth*2,stats.damageReductionPercent),(int)box.x+30,(int)box.y+286,15,C(172,211,211));
   DrawText(TextFormat("Pickup radius %.0f   Dash speed %.1fx",stats.pickupRadius,stats.dashMultiplier),(int)box.x+30,(int)box.y+314,15,C(172,211,211));
   if(stats.needleCooldownTicks)DrawText(TextFormat("Needlecatch cooldown %.1fs",stats.needleCooldownTicks/120.f),(int)box.x+30,(int)box.y+342,14,C(172,211,211));
   if(stats.bloodKillsPerHeal)DrawText(TextFormat("Bloodstitch: heal every %d kills, %.0f HP cap per wave",stats.bloodKillsPerHeal,stats.bloodWaveCap/100.f),(int)box.x+30,(int)box.y+370,14,C(172,211,211));
   drawButton(Rectangle{SW*.5f-90.f,SH*.5f+235.f,180.f,38.f},"BACK / ESC",true,false,13);
  }
  if(mode==Mode::Pause&&pauseConfirm){
   DrawRectangle(0,0,SW,SH,C(3,7,18,220));Rectangle box{SW*.5f-300,SH*.5f-100,600,205};
   DrawRectangleRounded(box,.06f,12,C(11,22,40,250));DrawRectangleRoundedLines(box,.06f,12,C(91,216,197,155));
   const char* prompt=pauseConfirm==1?"Restart the current match?":"Leave the current match?";
   DrawText(prompt,SW/2-MeasureText(prompt,23)/2,SH/2-65,23,C(226,255,247));
   DrawText("Current progress will be lost.",SW/2-130,SH/2-25,16,C(165,201,209));
   drawButton(confirmButton(0),"YES",confirmSelection==0,hovered(confirmButton(0)));
   drawButton(confirmButton(1),"NO",confirmSelection==1,hovered(confirmButton(1)));
  }
  if(mode==Mode::Settings){
   DrawRectangle(0,0,SW,SH,C(3,7,18,205));
   const Rectangle settingsPanel{settingsLeft,settingsTop,settingsWidth,512.f};
   DrawRectangleRounded(settingsPanel,.045f,16,C(11,22,40,250));
   DrawRectangleRoundedLines(settingsPanel,.045f,16,C(91,216,197,155));
   DrawText("SETTINGS",(int)settingsLeft+48,(int)settingsTop+27,28,C(226,255,247));
   drawButton(videoTab,"VIDEO",settingsTab==0,hovered(videoTab));
   drawButton(audioTab,"AUDIO",settingsTab==1,hovered(audioTab));
   drawButton(personalizationTab,"PERSONALIZATION",settingsTab==2,hovered(personalizationTab),13);
   DrawLine((int)settingsLeft+42,(int)settingsTop+129,(int)(settingsLeft+settingsWidth-42),(int)settingsTop+129,C(58,115,131,95));
   if(settingsTab==0){
    DrawRectangleRoundedLines(frameRow,.08f,8,settingsFocus==0?C(76,201,197,115):C(47,89,105,75));
    DrawText("FRAME RATE LIMIT",(int)settingsLeft+48,(int)settingsTop+153,16,settingsFocus==0?C(205,243,237):C(145,183,191));
    const char* rateLabels[]={"30 FPS","60 FPS","120 FPS","240 FPS","UNLIMITED"};
    for(int i=0;i<frameRateChoiceCount;i++){
     const bool configured=appSettings.frameRateLimit==threadlight::settings::kFrameRateLimits[i];
     drawButton(fpsChips[i],rateLabels[i],configured,hovered(fpsChips[i]),13);
    }
    DrawText("Gameplay limit. Menus and unfocused windows use 30 FPS.",(int)settingsLeft+48,(int)settingsTop+204,13,C(111,164,176));
    DrawText("SHOW FPS",(int)settingsLeft+48,(int)settingsTop+236,16,settingsFocus==1?C(205,243,237):C(145,183,191));
    DrawText("Measured output rate",(int)settingsLeft+48,(int)settingsTop+259,12,C(111,164,176));
    drawButton(showToggle,appSettings.showFps?"ON":"OFF",settingsFocus==1,hovered(showToggle),14);
    DrawText("VSYNC",(int)settingsLeft+48,(int)settingsTop+318,16,settingsFocus==2?C(205,243,237):C(145,183,191));
    DrawText("Match display refresh; may limit output even with Unlimited.",(int)settingsLeft+48,(int)settingsTop+341,12,C(111,164,176));
    drawButton(vsyncToggle,appSettings.vsync?"ON":"OFF",settingsFocus==2,hovered(vsyncToggle),14);
   }else if(settingsTab==1){
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
   }else{
    for(int i=0;i<2;++i){
     const Rectangle card=paletteCards[i];
     const auto id=i==0?cosmetics::PaletteId::OriginalCyan:cosmetics::PaletteId::VioletAscendant;
     const auto& preview=cosmetics::palette(id);
     const bool unlocked=i==0||appSettings.violetAscendantUnlocked;
     const bool selected=unlocked&&appSettings.selectedPalette==id;
     const bool focusedCard=settingsFocus==i;
     DrawRectangleRounded(card,.055f,12,C(8,17,34,248));
     DrawRectangleRoundedLines(card,.055f,12,selected?paletteColor(preview.accent):focusedCard?C(125,161,233,185):C(57,86,116,145));
     DrawText(i==0?"ORIGINAL CYAN":"VIOLET ASCENDANT",(int)card.x+20,(int)card.y+18,15,C(226,245,249));
     const char* status=i==0?(selected?"EQUIPPED":"AVAILABLE"):(selected?"EQUIPPED":(unlocked?"UNLOCKED":"LOCKED"));
     const Color statusColor=selected?paletteColor(preview.highlight):(unlocked?C(126,210,190):C(180,164,217));
     DrawText(status,(int)(card.x+card.width-MeasureText(status,10)-20),(int)card.y+21,10,statusColor);
     const float centerX=card.x+card.width*.5f,playerY=card.y+124.f;
     const Color halo=paletteColor(cosmetics::withAlpha(preview.silkHaloFresh,90));
     const Color body=paletteColor(cosmetics::withAlpha(preview.silkBodyFresh,220));
     const Color core=paletteColor(preview.silkCoreFresh);
     DrawLineEx({card.x+35.f,playerY+23.f},{centerX-15.f,playerY+5.f},8.f,halo);
     DrawLineEx({card.x+35.f,playerY+23.f},{centerX-15.f,playerY+5.f},3.5f,body);
     DrawLineEx({centerX-15.f,playerY+5.f},{card.x+card.width-35.f,playerY-18.f},8.f,halo);
     DrawLineEx({centerX-15.f,playerY+5.f},{card.x+card.width-35.f,playerY-18.f},3.5f,body);
     DrawLineEx({card.x+35.f,playerY+23.f},{centerX-15.f,playerY+5.f},1.5f,core);
     DrawLineEx({centerX-15.f,playerY+5.f},{card.x+card.width-35.f,playerY-18.f},1.5f,core);
     DrawCircleV({centerX-15.f,playerY+5.f},17.f,paletteColor(cosmetics::withAlpha(preview.playerAuraOuter,44)));
     DrawCircleV({centerX-15.f,playerY+5.f},9.f,paletteColor(preview.playerRim));
     DrawCircleV({centerX-15.f,playerY+5.f},5.5f,paletteColor(preview.playerCore));
     DrawLine((int)card.x+20,(int)card.y+165,(int)(card.x+card.width-20),(int)card.y+165,C(53,77,102,130));
     if(i==1&&!unlocked){
      DrawCircleLines((int)(card.x+card.width*.5f),(int)card.y+196,7.f,C(198,179,239,235));
      DrawRectangleRounded({centerX-9.f,card.y+195.f,18.f,15.f},.2f,4,C(198,179,239,235));
      DrawText("Win Wave 35 to unlock",(int)(centerX-MeasureText("Win Wave 35 to unlock",13)*.5f),(int)card.y+220,13,C(194,185,220));
     }else{
      const char* note=selected?"This color is active":"Select to equip immediately";
      DrawText(note,(int)(centerX-MeasureText(note,12)*.5f),(int)card.y+205,12,C(145,181,197));
     }
    }
   }
   drawButton(settingsBack,"BACK",(settingsTab==0&&settingsFocus==3)||(settingsTab==1&&settingsFocus==1)||(settingsTab==2&&settingsFocus==2),hovered(settingsBack));
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
   if(offers.current.count>0)offerTextCache.update(offers.current,ranks,health,ui.cards[0].width-32,
    [](const char* text,int size){return MeasureText(text,size);});
   auto wrapped=[](const std::vector<std::string>& lines,float x,float y,int size,Color color){
    for(const auto& line:lines){DrawText(line.c_str(),(int)x,(int)y,size,color);y+=size+7;}
   };
   for(int i=0;i<offers.current.count;i++){
    const auto r=ui.cards[i];const auto id=offers.current.cards[i];const int current=upgrades::rank(ranks,id),cap=upgrades::maximum(id);
    DrawRectangleRounded(rect(r),.06f,8,choice==i?C(19,55,64,245):C(15,32,48,245));
    DrawRectangleRoundedLines(rect(r),.06f,8,choice==i?C(133,255,221,230):C(67,115,132,125));
    DrawText(TextFormat("0%d",i+1),(int)r.x+16,(int)r.y+15,14,C(255,218,135));
    wrapped(offerTextCache.card(i).name,r.x+16,r.y+43,17,C(212,249,231));
    DrawText(cap?TextFormat("RANK %s -> %s / %s",roman[current],roman[current+1],roman[cap]):"REPEATABLE RECOVERY",(int)r.x+16,(int)r.y+96,12,C(137,203,203));
    for(int j=0;j<cap;j++)DrawRectangle((int)r.x+16+j*22,(int)r.y+119,16,3,j<current?C(107,228,206):j==current?C(255,213,131):C(53,83,97));
    wrapped(offerTextCache.card(i).effect,r.x+16,r.y+144,15,C(172,211,211));
    if(cap&&current+1==cap)DrawText("MAX RANK NEXT",(int)r.x+16,(int)r.y+216,11,C(255,218,135));
   }
   if(offers.current.count==0){
    DrawText("ALL AVAILABLE RANKS COMPLETE",(int)ui.panel.x+28,(int)ui.panel.y+166,20,C(212,249,231));
    DrawText("No useful recovery is available. Discard this unused credit to continue.",(int)ui.panel.x+28,(int)ui.panel.y+210,15,C(172,211,211));
   }
   drawButton(rect(ui.confirm),offers.current.count?"CONFIRM EVOLUTION":"DISCARD CREDIT",true,hovered(rect(ui.confirm)),13);
   DrawText("A / D OR ARROWS MOVE    S MIDDLE    1 / 2 / 3 SELECT    ENTER CONFIRMS",(int)ui.panel.x+28,(int)(ui.panel.y+ui.panel.height-20),11,C(108,167,178));
  }
  if(mode==Mode::Signature){
   const auto ui=upgradeUI::layout(SW,SH,signatureCatalog.current.count);
   auto rect=[](upgradeUI::Rect r){return Rectangle{r.x,r.y,r.width,r.height};};
   DrawRectangle(0,0,SW,SH,C(3,7,18,205));
   DrawRectangleRounded(rect(ui.panel),.045f,16,C(11,22,40,250));
   DrawRectangleRoundedLines(rect(ui.panel),.045f,16,C(224,196,129,170));
   DrawText("CHOOSE A SIGNATURE",(int)ui.panel.x+28,(int)ui.panel.y+28,28,C(249,228,180));
   DrawText(TextFormat("Guardian's gift %d / 4. Choose one; each can be acquired once.",(wave+1)/7),(int)ui.panel.x+28,(int)ui.panel.y+82,15,C(165,201,209));
   for(int i=0;i<signatureCatalog.current.count;++i){
    const auto id=signatureCatalog.current.choices[i];const auto card=ui.cards[i];const bool selected=choice==i;
    DrawRectangleRounded(rect(card),.06f,8,selected?C(25,50,61,250):C(15,32,48,245));
    DrawRectangleRoundedLines(rect(card),.06f,8,selected?C(233,209,151,230):C(93,112,121,140));
    DrawText(TextFormat("0%d / SIGNATURE I",i+1),(int)card.x+16,(int)card.y+15,12,C(224,196,129));
    DrawText(catalog::name(id).data(),(int)card.x+16,(int)card.y+42,18,C(244,230,191));
    DrawLineEx({card.x+18,card.y+79},{card.x+card.width-18,card.y+79},1.f,C(141,161,154,100));
    const auto lines=catalog::descriptionLines(id);
    for(int line=0;line<4;++line)DrawText(lines[line].data(),(int)card.x+16,(int)card.y+94+line*23,13,C(180,216,215));
    const bool damage=id!=catalog::Id::Cocoon&&id!=catalog::Id::Slipknot;
    if(damage){const int ordinary=id==catalog::Id::Afterimage?afterimage::damage(stats.silkDamage):id==catalog::Id::Crossweave?cross::damage(stats.silkDamage):id==catalog::Id::ResonantLoom?loom::damage(stats.silkDamage):effects::damage(stats.silkDamage,false);
     const int bossDamage=id==catalog::Id::Afterimage?afterimage::damage(stats.silkDamage,true):id==catalog::Id::Crossweave?cross::damage(stats.silkDamage,true):id==catalog::Id::ResonantLoom?loom::damage(stats.silkDamage,true):effects::damage(stats.silkDamage,true);
     DrawText(TextFormat("Damage %.2f / boss %.2f",ordinary/100.f,bossDamage/100.f),(int)card.x+16,(int)card.y+203,12,C(229,210,160));
    }else DrawText("UTILITY / NO EXTRA CONTROLS",(int)card.x+16,(int)card.y+203,11,C(229,210,160));
   }
   drawButton(rect(ui.confirm),"CLAIM SIGNATURE",true,hovered(rect(ui.confirm)),13);
   DrawText("A / D OR ARROWS MOVE    S MIDDLE    1 / 2 / 3 SELECT    ENTER CONFIRMS",(int)ui.panel.x+28,(int)(ui.panel.y+ui.panel.height-20),11,C(146,191,198));
  }
  if((mode==Mode::Lost||mode==Mode::Won)&&!inventoryOpen){
   panel(mode==Mode::Won?"VICTORY":"RUN COMPLETE",mode==Mode::Won?"THE BRIAR HEART FALLS":"LOST TO THE BRIAR","Your run at a glance.");
   if(mode==Mode::Won&&violetUnlockedThisRun){
    const Rectangle award{SW*.5f-220.f,SH*.5f-86.f,440.f,40.f};
    const float reveal=std::clamp((float)GetTime()*.7f,0.f,1.f);
    const float pulse=.72f+.28f*std::sin((float)GetTime()*3.2f);
    DrawRectangleRounded(award,.25f,12,C(35,24,70,235));
    DrawRectangleRoundedLines(award,.25f,12,C(174,127,255,(unsigned char)(175*pulse)));
    const Vector2 crest{award.x+29.f,award.y+20.f};
    DrawRing(crest,11.f,12.5f,18.f+(float)GetTime()*48.f,236.f+(float)GetTime()*48.f,36,C(192,154,255,(unsigned char)(155*reveal)));
    DrawCircleV(crest,5.f,C(227,205,255,(unsigned char)(255*reveal)));
    DrawText("VIOLET ASCENDANT UNLOCKED",(int)award.x+58,(int)award.y+12,14,C(239,224,255));
    DrawText("A new color awaits in Settings  >  Personalization",(int)award.x+58,(int)award.y+26,9,C(191,174,224));
   }
   DrawText(TextFormat("Wave %d / %d   Score %06llu   Kills %llu",wave+1,chapter::kEncounterCount,(unsigned long long)rewards.score(),(unsigned long long)rewards.counters().killed),SW/2-255,SH/2-36,17,C(179,225,216));
   DrawText(TextFormat("Evolutions %llu   Signatures %d   Sparks collected %llu",(unsigned long long)rewards.counters().creditsResolved,catalog::ownedCount(signatureCatalog),(unsigned long long)rewards.counters().sparksCollected),SW/2-255,SH/2-5,15,C(179,225,216));
   DrawText("DAMAGE TAKEN / HP",SW/2-255,SH/2+30,12,C(255,213,131));
   DrawText(TextFormat("Contact %.1f   Boss %.1f   Shots %.1f   Ground %.1f",runDamageTaken[0]/100.f,runDamageTaken[1]/100.f,runDamageTaken[2]/100.f,runDamageTaken[3]/100.f),SW/2-255,SH/2+51,15,C(172,211,211));
   DrawText("I  VIEW BUILD     ENTER / R  RETRY",SW/2-255,SH/2+103,16,C(255,218,135));
   DrawText("ESC  RETURN TO TITLE",SW/2-255,SH/2+135,14,C(136,188,192));
  }
  if(appSettings.showFps)
   DrawText(TextFormat("FPS %d",(int)std::lround(measuredFps)),8,5,10,Fade(C(174,222,228),.32f));
  sfx.update();
  if(m6RenderCheck){
   ++renderCheckFrames;
   rlDrawRenderBatchActive(); // Screenshot must include this frame's queued draws.
   const auto capture=[](const char* path){
    // raylib's screenshot helper multiplies the framebuffer by DPI again on
    // this backend. Read its actual dimensions for the isolated QA artifact.
    Image frame{rlReadScreenPixels(GetRenderWidth(),GetRenderHeight()),GetRenderWidth(),GetRenderHeight(),1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    if(frame.data){ExportImage(frame,path);UnloadImage(frame);}
   };
   if(mode==Mode::Signature){capture("m7-signature.png");exitRequested=true;}
   if(settingsRenderCheck){capture("settings-vsync.png");exitRequested=true;}
   if(personalizationRenderCheck>=0){capture(personalizationRenderCheck==0?"personalization-locked.png":personalizationRenderCheck==1?"personalization-cyan.png":"personalization-violet.png");exitRequested=true;}
   if(utilityFixture&&simulationTick>=240){capture("m7-utilities.png");exitRequested=true;}
   if(!capturedWarning&&(boss.stage==root::Stage::RootWarning||boss.stage==root::Stage::StepWarning)){
    capture("m6-warning.png");capturedWarning=true;
   }
   if(!capturedActive&&(boss.stage==root::Stage::RootActive||boss.stage==root::Stage::StepActive)){
    capture("m6-active.png");capturedActive=true;
   }
   if((capturedWarning&&capturedActive)||renderCheckFrames>=1200)exitRequested=true;
  }
  const int effectiveFrameRateLimit=pacingCheck?pacingRate:threadlight::timing::effectiveFrameRateLimit(
   appSettings.frameRateLimit,mode==Mode::Play,IsWindowFocused(),IsWindowMinimized(),m6RenderCheck);
  if(effectiveFrameRateLimit!=appliedFrameRateLimit){
   updateTimerResolution(!m6RenderCheck&&effectiveFrameRateLimit!=0);
   SetTargetFPS(effectiveFrameRateLimit);appliedFrameRateLimit=effectiveFrameRateLimit;
  }
  const auto presentStart=perf?PerfClock::now():PerfClock::time_point{};
  EndDrawing();
  if(perf){
   const auto frameEnd=PerfClock::now();
   auto ms=[](auto duration){return std::chrono::duration<double,std::milli>(duration).count();};
   if(lastMemorySample==PerfClock::time_point{}||frameEnd-lastMemorySample>=std::chrono::seconds(1)){
    memorySample=threadlight::performance::processMemory();lastMemorySample=frameEnd;
   }
   const size_t first=threadlight::silk::firstActiveSegment(std::span<const Thread>(thread),stats.activeSilkSeconds);
   threadlight::performance::Frame sample;
   sample.number=++perfFrameNumber;sample.elapsedSeconds=std::chrono::duration<double>(frameStart-bootTime).count();
   sample.intervalMs=previousPerfFrame==PerfClock::time_point{}?0:ms(frameStart-previousPerfFrame);previousPerfFrame=frameStart;
   sample.raylibFrameMs=frameSeconds*1000;sample.updateMs=ms(drawStart-frameStart);sample.simulationMs=simulationMs;
   sample.drawMs=ms(presentStart-drawStart);sample.presentMs=ms(frameEnd-presentStart);
   sample.discardedMs=discardedMs;sample.steps=frameSteps;sample.mode=modeName(screenAtFrameStart);
   sample.phase=foundation::phaseName(lifecycle.phase());sample.wave=wave+1;sample.width=GetRenderWidth();sample.height=GetRenderHeight();
   sample.enemies=foes.size();sample.silk=thread.empty()?0:thread.size()-1;sample.activeSilk=thread.size()-first;
   sample.sparks=sparks.size();sample.particles=particles.size();sample.projectiles=projectiles.flyingCount()+boss.bullets.flyingCount();
   sample.groundHazards=ground.liveCount();sample.bossHazards=boss.fields.liveCount();sample.memory=memorySample;
   sample.configuredFps=appSettings.frameRateLimit;sample.effectiveFps=effectiveFrameRateLimit;
   sample.vsyncRequested=appSettings.vsync;sample.monitorHz=GetMonitorRefreshRate(GetCurrentMonitor());sample.focused=focused;sample.frameQpcMs=frameQpcMs;
   if(!perf->record(sample)){TraceLog(LOG_WARNING,"Performance log write failed; stopping diagnostics.");perf.reset();}
  }
 }
 settingsEdits.dragging=false;
 if(!m6RenderCheck)saveSettings();
 updateTimerResolution(false);
 if(perf){
  perf->finish();const auto& h=perf->intervals();
  TraceLog(LOG_INFO,"PERF intervals=%llu p50<=%.1fms p95<=%.1fms p99<=%.1fms max=%.2fms discarded=%.2fms frames=%llu%s",
   (unsigned long long)h.count(),h.percentile(.50),h.percentile(.95),h.percentile(.99),h.maxMs(),perf->discardedMs(),
   (unsigned long long)perf->discardedFrames(),perf->healthy()?"":" (log flush failed)");
 }
 sfx.shutdown();
 if(audioReady)CloseAudioDevice();
 CloseWindow();return 0;
}
