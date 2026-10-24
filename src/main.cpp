#include "raylib.h"
#include "game_logic.h"
#include "game_layout.h"
#include "app_settings.h"
#include "menu_state.h"
#include "game_timing.h"
#include "game_audio.h"
#include "visual_feedback.h"
#include "visual_effects.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {
constexpr float kPlayerRadius=threadlight::layout::kPlayerRadius;
struct V { float x{},y{}; };
V operator+(V a,V b){return {a.x+b.x,a.y+b.y};} V operator-(V a,V b){return {a.x-b.x,a.y-b.y};} V operator*(V a,float s){return {a.x*s,a.y*s};}
float len(V a){return std::sqrt(a.x*a.x+a.y*a.y);} float dist(V a,V b){return len(a-b);} V norm(V a){float l=len(a);return l>0.001f?a*(1/l):V{};} Vector2 cv(V a){return {a.x,a.y};}
V catmullRom(V p0,V p1,V p2,V p3,float t){const float t2=t*t,t3=t2*t;return (p1*2.f+(p2-p0)*t+(p0*2.f-p1*5.f+p2*4.f-p3)*t2+(p0*-1.f+p1*3.f-p2*3.f+p3)*t3)*.5f;}
Color C(unsigned char r,unsigned char g,unsigned char b,unsigned char a=255){return {r,g,b,a};}
struct Thread {V p; float age;};
struct Foe {V p,vel; float r,hp,maxhp,speed,hitCd,phase,hitFlash; int kind;};
struct Spark {V p; float t;};
struct Particle {V p,v;float life,maxLife,size;Color color;};
struct DeathEffect {V p; std::array<V,threadlight::visual::kHeavyShardCount> velocity; float age{}; int kind{};};
}
int main(){
 SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_WINDOW_RESIZABLE|FLAG_MSAA_4X_HINT);
 InitWindow(threadlight::layout::kInitialScreenWidth,threadlight::layout::kInitialScreenHeight,"THREADLIGHT | A small game of living silk");
 SetWindowMinSize(960,640);
 // GetScreenWidth/Height are logical client dimensions. GetRenderWidth/Height
 // are DPI-scaled framebuffer dimensions and must never drive world geometry.
 int SW=GetScreenWidth(),SH=GetScreenHeight();
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
 Mode mode=Mode::Title,settingsReturn=Mode::Title; bool exitRequested=false,sliderDragging=false; int mainSelection=0,pauseSelection=0,exitSelection=threadlight::menu::kDefaultExitSelection,settingsTab=0,settingsFocus=0;
 V player{(playArea.left+playArea.right)*.5f,(playArea.top+playArea.bottom)*.5f}; float hp=100, energy=0, hitInvuln=0,dashCd=0,dashTime=0, spawnT=0, waveT=0, shake=0, pulse=0, damageFeedback=0; int wave=0, score=0, kills=0, levelN=1, choice=0; float speed=255, trailDmg=1, radius=kPlayerRadius; bool dashUpgrade=false, dashVisualActive=false;
 float dashEchoTimer=0,dashEchoFade=0; size_t dashEchoCount=0; std::array<V,6> dashEcho{};
 std::vector<Thread> thread; std::vector<Foe> foes; std::vector<Spark> sparks; std::vector<Particle> particles; std::vector<DeathEffect> deathEffects; std::vector<V> stars;
 thread.reserve(240); foes.reserve(80); sparks.reserve(48); particles.reserve(threadlight::visual::kParticleLimit); deathEffects.reserve(threadlight::visual::kDeathEffectLimit); stars.reserve(110);
 for(int i=0;i<110;i++) stars.push_back({GetRandomValue(0,10000)/10000.0f,GetRandomValue(0,10000)/10000.0f});
 threadlight::timing::FixedStepClock stepClock;
 bool dashPressPending=false;
 float measuredFps=60.0f;
 auto saveSettings=[&](){
  SetTargetFPS(appSettings.frameRateLimit);
  sfx.setMasterVolume(appSettings.sfxVolumePercent/100.0f);
  if(!threadlight::settings::save(settingsPath,appSettings))
   TraceLog(LOG_WARNING,"Could not save ThreadLight settings to the user config directory.");
 };
 auto burst=[&](V p,Color col,int n,float force){for(int i=0;i<n&&particles.size()<threadlight::visual::kParticleLimit;i++){float a=(float)GetRandomValue(0,6283)/1000.f,s=(float)GetRandomValue(25,(int)force);particles.push_back({p,{std::cos(a)*s,std::sin(a)*s},.65f,.65f,(float)GetRandomValue(2,6),col});}};
 auto addDeathEffect=[&](const Foe& foe){
  if(deathEffects.size()>=threadlight::visual::kDeathEffectLimit)deathEffects.erase(deathEffects.begin());
  DeathEffect effect{};effect.p=foe.p;effect.kind=foe.kind;
  const int count=foe.kind?threadlight::visual::kHeavyShardCount:threadlight::visual::kSmallShardCount;
  const float phase=foe.phase*.71f;
  for(int i=0;i<count;i++){const float a=phase+6.2831853f*(float)i/count+(float)GetRandomValue(-20,20)*.01f;const float force=foe.kind?65.f:160.f;effect.velocity[i]={std::cos(a)*(force+(float)GetRandomValue(-15,15)),std::sin(a)*(force+(float)GetRandomValue(-15,15))};}
  deathEffects.push_back(effect);
 };
 auto reset=[&](){player={(playArea.left+playArea.right)*.5f,(playArea.top+playArea.bottom)*.5f};hp=100;energy=0;hitInvuln=dashCd=dashTime=spawnT=waveT=shake=pulse=damageFeedback=0;wave=0;score=kills=0;levelN=1;speed=255;trailDmg=1;radius=kPlayerRadius;dashUpgrade=false;dashVisualActive=false;dashEchoTimer=dashEchoFade=0;dashEchoCount=0;dashEcho.fill(player);thread.clear();foes.clear();sparks.clear();particles.clear();deathEffects.clear();stepClock.clear();dashPressPending=false;mode=Mode::Play;};
 auto addFoe=[&](int w){float a=(float)GetRandomValue(0,6283)/1000.f;int k=(GetRandomValue(0,9)<(w>2?3:2))?1:0;float r=k?20:14;const float playWidth=playArea.right-playArea.left,playHeight=playArea.bottom-playArea.top;const float padding=r+5.f;auto spawn=threadlight::chooseSafeSpawn(a,player.x-playArea.left,player.y-playArea.top,playWidth,playHeight,padding,std::max(padding,playWidth*.53f),std::max(padding,playHeight*.53f),radius+r+70.f);V p{spawn.x+playArea.left,spawn.y+playArea.top};foes.push_back({p,{},r,k?3.f:1.f,k?3.f:1.f,k?66.f:95.f,0,a,0,k});};
 auto segDist=[](V p,V a,V b){V ab=b-a;float q=ab.x*ab.x+ab.y*ab.y;float t=q>0?std::clamp(((p.x-a.x)*ab.x+(p.y-a.y)*ab.y)/q,0.f,1.f):0;return dist(p,a+ab*t);};
 auto drawText=[&](const char* s,int x,int y,int size,Color c){DrawText(s,x,y,size,c);};
  auto applyMenuAction=[&](threadlight::menu::Action action){threadlight::menu::apply(mode,settingsReturn,exitRequested,exitSelection,action);};
  auto changeFrameLimit=[&](int value){if(!threadlight::settings::validFrameRateLimit(value)||value==appSettings.frameRateLimit)return;appSettings.frameRateLimit=value;saveSettings();};
  auto changeShowFps=[&](){appSettings.showFps=!appSettings.showFps;saveSettings();};
  auto changeSfxVolume=[&](int value,bool persist=true){value=std::clamp(value,0,100);if(value==appSettings.sfxVolumePercent)return;appSettings.sfxVolumePercent=value;sfx.setMasterVolume(value/100.0f);if(persist)saveSettings();};
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
   const int nextSW=std::max(1,GetScreenWidth()),nextSH=std::max(1,GetScreenHeight());
   const auto nextPlayArea=threadlight::layout::playBounds(nextSW,nextSH);
   if(nextSW!=SW||nextSH!=SH){
    auto remapWorldPoint=[&](V& point){const auto mapped=threadlight::layout::remapPoint({point.x,point.y},playArea,nextPlayArea);point={mapped.x,mapped.y};};
    remapWorldPoint(player);
    for(auto& item:thread)remapWorldPoint(item.p);
    for(auto& foe:foes)remapWorldPoint(foe.p);
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

   if(screenAtFrameStart==Mode::Title){
    if(IsKeyPressed(KEY_ESCAPE))applyMenuAction(threadlight::menu::Action::Escape);
    else if(IsKeyPressed(KEY_UP))mainSelection=threadlight::menu::moveSelection(mainSelection,-1,3);
    else if(IsKeyPressed(KEY_DOWN))mainSelection=threadlight::menu::moveSelection(mainSelection,1,3);
    else if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){
     if(mainSelection==0)reset();
     else if(mainSelection==1)applyMenuAction(threadlight::menu::Action::OpenSettings);
     else applyMenuAction(threadlight::menu::Action::RequestExit);
    }else for(int i=0;i<3;i++)if(contains(mainButton(i),mouse)){
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
    }else for(int i=0;i<4;i++)if(contains(pauseButton(i),mouse)){
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
     if(contains(videoTab,mouse)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){settingsTab=0;settingsFocus=0;}
     else if(contains(audioTab,mouse)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){settingsTab=1;settingsFocus=0;}
     else if(contains(settingsBack,mouse)){settingsFocus=settingsTab==0?2:1;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))applyMenuAction(threadlight::menu::Action::Back);}
     else if(settingsTab==0){
      for(int i=0;i<4;i++)if(contains(fpsChips[i],mouse)){settingsFocus=0;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){constexpr int rates[]={30,60,120,0};changeFrameLimit(rates[i]);}}
      if(contains(showToggle,mouse)){settingsFocus=1;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))changeShowFps();}
     }else if(settingsTab==1&&contains(volumeRow,mouse))settingsFocus=0;
     if(settingsTab==1&&(contains(volumeSlider,mouse)||
         (sliderDragging&&(IsMouseButtonDown(MOUSE_BUTTON_LEFT)||IsMouseButtonReleased(MOUSE_BUTTON_LEFT))))){
      settingsFocus=0;
      if(contains(volumeSlider,mouse)&&IsMouseButtonPressed(MOUSE_BUTTON_LEFT))sliderDragging=true;
      if(sliderDragging&&IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
       const float position=std::clamp((mouse.x-volumeSlider.x)/std::max(1.f,volumeSlider.width),0.f,1.f);
       const int percent=static_cast<int>(std::lround(position*100.f));
       changeSfxVolume(threadlight::settings::quantizeSfxVolume(percent),false);
      }
      if(sliderDragging&&IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){sliderDragging=false;saveSettings();}
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
    else if(contains(yesButton,mouse)){exitSelection=0;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))applyMenuAction(threadlight::menu::Action::ConfirmExit);}
    else if(contains(noButton,mouse)){exitSelection=1;if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))applyMenuAction(threadlight::menu::Action::CancelExit);}
   }else if(screenAtFrameStart==Mode::Play){
    if(IsKeyPressed(KEY_P)){
     applyMenuAction(threadlight::menu::Action::Pause);
     stepClock.clear();dashPressPending=false;
    }else{
     dashPressPending=dashPressPending||IsKeyPressed(KEY_SPACE);
     const int steps=stepClock.advance(frameSeconds);
     for(int tick=0;tick<steps&&mode==Mode::Play;tick++){
      constexpr float dt=static_cast<float>(threadlight::timing::kFixedStepSeconds);
      hitInvuln=std::max(0.f,hitInvuln-dt);dashCd=std::max(0.f,dashCd-dt);pulse=std::max(0.f,pulse-dt);damageFeedback=std::max(0.f,damageFeedback-dt);shake=std::max(0.f,shake-dt);dashVisualActive=dashTime>0;dashEchoTimer=std::max(0.f,dashEchoTimer-dt);
      V mv{(float)(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT))-(float)(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)),(float)(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN))-(float)(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))}; if(len(mv)>0)mv=norm(mv);
      const bool dashPressed=dashPressPending;dashPressPending=false;
      if(dashPressed&&dashCd<=0&&len(mv)>0){dashTime=threadlight::kDashDurationSeconds;dashCd=dashUpgrade?threadlight::kUpgradedDashCooldownSeconds:threadlight::kDashCooldownSeconds;dashEcho.fill(player);dashEchoCount=0;dashEchoFade=.22f;dashEchoTimer=0;dashVisualActive=true;burst(player,C(132,244,255),10,150);sfx.play(threadlight::audio::Event::Dash);}
      const bool dashActiveThisFrame=threadlight::dashProtectsCollisionThisFrame(dashTime);dashVisualActive=dashVisualActive||dashActiveThisFrame;dashEchoFade=std::max(0.f,dashEchoFade-dt);float mult=dashActiveThisFrame?3.7f:1.f;if(dashActiveThisFrame)dashTime=std::max(0.f,dashTime-dt);player=player+mv*(speed*mult*dt);player.x=threadlight::layout::clampPlayerX(player.x,playArea);player.y=threadlight::layout::clampPlayerY(player.y,playArea);
      if(dashActiveThisFrame&&dashEchoTimer<=0){const size_t last=std::min(dashEchoCount,dashEcho.size()-1);for(size_t i=last;i>0;i--)dashEcho[i]=dashEcho[i-1];dashEcho[0]=player;dashEchoCount=std::min(dashEchoCount+1,dashEcho.size());dashEchoTimer=.028f;dashEchoFade=std::max(dashEchoFade,.22f);}
      if(thread.empty()||dist(thread.back().p,player)>9)thread.push_back({player,0});
      for(auto &t:thread)t.age+=dt;while(!thread.empty()&&(thread.front().age>threadlight::visual::kSilkLifetimeSeconds||thread.size()>520))thread.erase(thread.begin());
      waveT+=dt;spawnT-=dt;const bool spawnDue=spawnT<=0;if(spawnDue&&wave<threadlight::kWaveCount){addFoe(wave);spawnT=std::max(.30f,1.08f-wave*.09f);}
      for(auto &f:foes){V d=norm(player-f.p);f.phase+=dt;f.hitCd=std::max(0.f,f.hitCd-dt);f.hitFlash=std::max(0.f,f.hitFlash-dt);float wob=std::sin(f.phase*3)*.28f;f.vel=f.vel+(d+V{-d.y*wob,d.x*wob})*(f.speed*dt*2.5f);float vl=len(f.vel);if(vl>f.speed)f.vel=f.vel*(f.speed/vl);f.p=f.p+f.vel*dt;
       for(size_t i=1;i<thread.size();i++){if(thread[i].age>threadlight::visual::kActiveSilkSeconds)continue;if(segDist(f.p,thread[i-1].p,thread[i].p)<f.r+5&&f.hitCd<=0){f.hp-=trailDmg;f.hitCd=.27f;f.hitFlash=threadlight::visual::kEnemyHitFlashSeconds;burst(f.p,C(83,255,226,220),4,70);if(f.hp<=0){sparks.push_back({f.p,0});score+=f.kind?35:10;kills++;addDeathEffect(f);shake=std::max(shake,f.kind?.105f:.045f);f.r=0;sfx.play(threadlight::audio::Event::EnemyDeath);}else sfx.play(threadlight::audio::Event::EnemyHit);break;}}
       if(f.r>0&&dist(f.p,player)<f.r+radius&&!dashActiveThisFrame&&hitInvuln<=0){hp-=f.kind?16:10;hitInvuln=.78f;damageFeedback=threadlight::visual::kDamageFeedbackDuration;shake=threadlight::visual::kDamageShakeSeconds;sfx.play(threadlight::audio::Event::PlayerDamage);burst(player,C(255,92,119),13,170);}
      }
      foes.erase(std::remove_if(foes.begin(),foes.end(),[](Foe f){return f.r<=0;}),foes.end());
      for(auto &s:sparks){s.t+=dt;if(dist(s.p,player)<110){V v=norm(player-s.p);s.p=s.p+v*(220*dt);}if(dist(s.p,player)<radius+12){energy+=1;score+=5;s.t=-1;sfx.play(threadlight::audio::Event::Spark);burst(s.p,C(255,218,125),8,100);}}
      sparks.erase(std::remove_if(sparks.begin(),sparks.end(),[](Spark s){return s.t<0||s.t>12;}),sparks.end());
      const float particleDamping=threadlight::timing::dampingMultiplier(.96f,60.f,dt);
      for(auto &p:particles){p.life-=dt;p.p=p.p+p.v*dt;p.v=p.v*particleDamping;}particles.erase(std::remove_if(particles.begin(),particles.end(),[](const Particle& p){return p.life<=0;}),particles.end());
      for(auto &effect:deathEffects)effect.age+=dt;deathEffects.erase(std::remove_if(deathEffects.begin(),deathEffects.end(),[](const DeathEffect& e){return e.age>=(e.kind?threadlight::visual::kHeavyDeathSeconds:threadlight::visual::kSmallDeathSeconds);}),deathEffects.end());
      const bool waveComplete=threadlight::waveIsComplete(wave,waveT,spawnDue);const auto outcome=threadlight::resolvePlayOutcome(wave,hp,waveComplete,energy>=8);if(waveComplete)waveT=0;threadlight::applyProgress(outcome,wave,energy,levelN,choice);
      const Mode priorMode=mode;mode=outcome.mode;if(mode!=priorMode){if(mode==Mode::Upgrade)sfx.play(threadlight::audio::Event::Evolution);else if(mode==Mode::Won)sfx.play(threadlight::audio::Event::Victory);else if(mode==Mode::Lost)sfx.play(threadlight::audio::Event::Defeat);}
     }
     if(mode!=Mode::Play)stepClock.clear();
    }
   }else if(screenAtFrameStart==Mode::Upgrade){if(IsKeyPressed(KEY_ONE)||IsKeyPressed(KEY_A))choice=0;else if(IsKeyPressed(KEY_TWO)||IsKeyPressed(KEY_D))choice=1;else if(IsKeyPressed(KEY_THREE)||IsKeyPressed(KEY_S))choice=2;else if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){if(choice==0)hp=std::min(100.f,hp+35);if(choice==1)trailDmg+=.65f;if(choice==2){speed+=24;dashUpgrade=true;}mode=Mode::Play;stepClock.clear();}}
   else if(screenAtFrameStart==Mode::Lost||screenAtFrameStart==Mode::Won){if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_R))reset();else if(IsKeyPressed(KEY_ESCAPE))applyMenuAction(threadlight::menu::Action::Escape);}

  BeginDrawing();ClearBackground(C(7,12,27));
  const float time=(float)GetTime();
  const float shakeStrength=threadlight::visual::shakeEnvelope(shake,threadlight::visual::kDamageShakeSeconds);
  const float cameraShiftX=std::sin(time*71.f)*shakeStrength*threadlight::layout::kCameraShakeX;
  const float cameraShiftY=std::sin(time*53.f+1.4f)*shakeStrength*threadlight::layout::kCameraShakeY;
  const auto cameraTransform=threadlight::layout::cameraTransform(SW,SH,renderWidth,renderHeight,cameraShiftX,cameraShiftY);
  Camera2D worldCamera{};worldCamera.offset={cameraTransform.offset.x,cameraTransform.offset.y};worldCamera.target={cameraTransform.target.x,cameraTransform.target.y};worldCamera.zoom=cameraTransform.zoom;
  BeginMode2D(worldCamera);
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
  if(thread.size()>1){
   for(size_t i=1;i<thread.size();i++){
    const V p0=thread[i>1?i-2:i-1].p,p1=thread[i-1].p,p2=thread[i].p,p3=thread[i+1<thread.size()?i+1:i].p;
    for(int step=1;step<=3;step++){
     const float t0=(float)(step-1)/3.f,t1=(float)step/3.f;
     const V a=catmullRom(p0,p1,p2,p3,t0),b=catmullRom(p0,p1,p2,p3,t1);
     const float age=thread[i-1].age+(thread[i].age-thread[i-1].age)*(t0+t1)*.5f;
     const float shimmer=.5f+.5f*std::sin(time*5.f-age*1.6f);
     const auto silk=threadlight::visual::silkAppearance(age,shimmer);
     Color halo=ColorLerp(C(0,222,211),C(42,129,165),silk.ageBlend);
     Color body=ColorLerp(C(14,247,220),C(45,151,184),silk.ageBlend);
     Color core=ColorLerp(C(224,255,246),C(119,184,204),silk.ageBlend);
     halo.a=(unsigned char)std::lround(silk.haloOpacity*255.f);
     body.a=(unsigned char)std::lround(silk.bodyOpacity*(.94f+.06f*shimmer)*255.f);
     core.a=(unsigned char)std::lround(silk.coreOpacity*255.f);
     DrawLineEx(cv(a),cv(b),8.5f-1.5f*silk.ageBlend,halo);
     DrawLineEx(cv(a),cv(b),4.4f-1.0f*silk.ageBlend,body);
     DrawLineEx(cv(a),cv(b),1.75f-.35f*silk.ageBlend,core);
    }
   }
  }
  // A short, bright trail behind the dashing player communicates direction
  // and distance, then fades quickly without covering enemies or silk.
  if(dashEchoFade>0&&dashEchoCount>1){
   const float fade=std::clamp(dashEchoFade/.22f,0.f,1.f);
   for(size_t i=dashEchoCount-1;i>0;i--){
    const float ageWeight=.25f+.75f*(1.f-(float)(i-1)/(float)(dashEchoCount-1));
    const unsigned char glow=(unsigned char)(86.f*fade*ageWeight),core=(unsigned char)(230.f*fade*ageWeight);
    DrawLineEx(cv(dashEcho[i]),cv(dashEcho[i-1]),7.f*fade,C(0,183,229,glow));
    DrawLineEx(cv(dashEcho[i]),cv(dashEcho[i-1]),2.4f*fade,C(190,248,255,core));
    DrawCircleV(cv(dashEcho[i]),radius*.72f,C(67,220,255,(unsigned char)(145.f*fade*ageWeight)));
   }
   DrawLineEx(cv(dashEcho[0]),cv(player),7.f*fade,C(0,183,229,(unsigned char)(86.f*fade)));
   DrawLineEx(cv(dashEcho[0]),cv(player),2.4f*fade,C(190,248,255,(unsigned char)(230.f*fade)));
  }
  for(auto s:sparks){float r=8+2*std::sin(time*6+s.t);DrawCircleV(cv(s.p),r+8,C(255,185,95,20));DrawPoly(cv(s.p),4,r,45,C(255,209,115));DrawPolyLinesEx(cv(s.p),4,r+2,45,1.1f,C(255,242,187,190));}
  for(const auto& effect:deathEffects){
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
  for(auto f:foes){
   const float flash=threadlight::visual::hitFlashStrength(f.hitFlash);
   const Color base=f.kind?C(255,116,178):C(255,97,132);
   const Color edge=ColorLerp(base,C(207,255,246,255),flash);
   const float bob=std::sin(f.phase*5.f)*2.2f,breath=1.f+std::sin(f.phase*3.2f)*.035f;
   const Vector2 pos={f.p.x,f.p.y+bob};
   DrawCircleV({pos.x,pos.y+f.r*.48f},f.r*.7f,C(3,9,22,95));
   DrawCircleV(pos,f.r+8,C(255,63,123,(unsigned char)(14+flash*30)));
   DrawPoly(pos,f.kind?6:4,f.r*breath,time*40.f,C(25,22,45));
   DrawPolyLinesEx(pos,f.kind?6:4,f.r*breath,time*40.f,1.5f+flash*1.1f,edge);
   if(f.kind){const float arc=time*25.f;DrawCircleV(pos,3.f,C(255,116,178,175));DrawRing(pos,f.r*.67f,f.r*.75f,arc,arc+55.f,18,C(255,173,207,120));}
   else DrawLineEx({pos.x-6,pos.y+5},{pos.x+6,pos.y+5},1.f,C(255,141,167,165));
   DrawCircleV({pos.x-4,pos.y-2},2.2f,C(255,238,220));DrawCircleV({pos.x+4,pos.y-2},2.2f,C(255,238,220));
   if(flash>0)DrawCircleV(pos,f.r*.56f,C(219,255,249,(unsigned char)(105.f*flash)));
   if(f.hp<f.maxhp){DrawRectangle((int)(f.p.x-f.r),(int)(f.p.y-f.r-10),((int)(2*f.r)),3,C(50,48,71));DrawRectangle((int)(f.p.x-f.r),(int)(f.p.y-f.r-10),(int)(2*f.r*std::clamp(f.hp/f.maxhp,0.f,1.f)),3,C(255,118,157));}
  }
  for(auto p:particles){float a=std::clamp(p.life/p.maxLife,0.f,1.f);DrawCircleV(cv(p.p),p.size*a,Fade(p.color,a));}
  if(mode!=Mode::Title){
   const float impact=threadlight::visual::damageImpactStrength(damageFeedback);
   const bool dashVisual=dashTime>0||dashVisualActive;
   DrawCircleV(cv(player),31,C(45,230,214,18));
   DrawCircleV(cv(player),21,C(51,230,221,34));
   if(impact>0)DrawCircleV(cv(player),radius+13,C(242,76,101,(unsigned char)(35*impact)));
   const Color rim=dashVisual?C(126,239,255,245):ColorLerp(C(164,255,232,220),C(255,133,151,235),impact*.72f);
   const Color core=dashVisual?C(224,251,255,255):ColorLerp(C(221,255,245,255),C(255,171,181,255),impact*.48f);
   DrawCircleV(cv(player),radius+4,rim);
   DrawCircleV(cv(player),radius,core);
   if(impact>0){const float ringRadius=threadlight::visual::damageRingRadius(radius,damageFeedback);DrawRing(cv(player),ringRadius,ringRadius+1.6f,0,360,48,C(255,118,139,(unsigned char)(100*impact)));}
  }
  EndMode2D();
  const bool showRunHud=mode==Mode::Play||mode==Mode::Pause||mode==Mode::Upgrade||
      mode==Mode::Lost||mode==Mode::Won||(mode==Mode::Settings&&settingsReturn==Mode::Pause);
  const auto hud=threadlight::layout::hudLayout(SW,SH);
  if(showRunHud){
   const float hudHeight=threadlight::layout::kHudBottom;
   const int titleSize=SW<1100?22:25;
   DrawRectangle(0,0,SW,(int)hudHeight,C(5,10,24,235));
   DrawLine(0,(int)hudHeight,SW,(int)hudHeight,C(60,152,156,80));
   drawText("THREADLIGHT",26,22,titleSize,C(200,255,243));
   if(SW>=1180)drawText("THE BRIAR CIRCUIT",214,31,13,C(91,167,179));
   DrawText("VITALITY",(int)hud.healthX,17,11,C(120,164,178));
   DrawRectangle((int)hud.healthX,38,(int)hud.healthWidth,10,C(34,49,67));
   DrawRectangle((int)hud.healthX,38,(int)(hud.healthWidth*std::clamp(hp/100.f,0.f,1.f)),10,hp>35?C(72,225,190):C(255,91,127));
   drawText(TextFormat("WAVE %d / 5",std::min(wave+1,threadlight::kWaveCount)),(int)hud.waveX,22,18,C(217,239,242));
   drawText(TextFormat("SPARKS  %d / 8",(int)energy),(int)hud.sparksX,24,15,C(255,213,131));
   drawText(TextFormat("SCORE  %06d",score),(int)hud.scoreX,24,17,C(177,208,221));
  }
  if(mode==Mode::Play){
   const float footerY=hud.footerY;
   DrawRectangle(12,(int)(footerY+9),SW-24,34,C(5,10,24,185));
   drawText("WASD MOVE",24,(int)(footerY+20),12,C(149,191,199));
   drawText("SPACE DASH",125,(int)(footerY+20),12,C(149,191,199));
   drawText("SILK CUTS HUNTERS",(int)hud.hintX,(int)(footerY+20),12,C(102,219,207));
   drawText("SFX IN SETTINGS",(int)hud.settingsHintX,(int)(footerY+20),12,C(149,191,199));
   drawText("P  PAUSE",(int)hud.pauseHintX,(int)(footerY+20),12,C(149,191,199));
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
   const Rectangle bounds=panel("A LUMINOUS SURVIVAL STORY","THREADLIGHT","A small game of living silk");
   DrawText("Move to weave a fading thread. Circle the hunters; let silk do the cutting.",(int)bounds.x+52,(int)bounds.y+153,15,C(175,211,213));
   DrawText("WASD / arrows move     SPACE dash     P pauses",(int)bounds.x+52,(int)bounds.y+181,15,C(121,196,197));
   DrawText("Collect 8 sparks to evolve. Survive five waves.",(int)bounds.x+52,(int)bounds.y+207,15,C(121,196,197));
   const char* labels[]={"START GAME","SETTINGS","EXIT GAME"};
   for(int i=0;i<3;i++)drawButton(mainButton(i),labels[i],mainSelection==i,contains(mainButton(i),mouse));
   DrawText("UP / DOWN SELECT     ENTER CONFIRMS     ESC OPENS EXIT CONFIRMATION",(int)bounds.x+52,(int)bounds.y+431,12,C(108,167,178));
  }
  if(mode==Mode::Pause){
   panel("THE GARDEN HOLDS ITS BREATH","PAUSED","Your thread is waiting.");
   const char* labels[]={"RESUME","SETTINGS","RESTART RUN","RETURN TO MAIN MENU"};
   for(int i=0;i<4;i++)drawButton(pauseButton(i),labels[i],pauseSelection==i,contains(pauseButton(i),mouse),15);
   DrawText("P RESUMES     ESC RETURNS TO TITLE",SW/2-142,SH/2+197,12,C(108,167,178));
  }
  if(mode==Mode::Settings){
   DrawRectangle(0,0,SW,SH,C(3,7,18,205));
   const Rectangle settingsPanel{settingsLeft,settingsTop,settingsWidth,512.f};
   DrawRectangleRounded(settingsPanel,.045f,16,C(11,22,40,250));
   DrawRectangleRoundedLines(settingsPanel,.045f,16,C(91,216,197,155));
   DrawText("SETTINGS",(int)settingsLeft+48,(int)settingsTop+27,28,C(226,255,247));
   drawButton(videoTab,"VIDEO",settingsTab==0,contains(videoTab,mouse));
   drawButton(audioTab,"AUDIO",settingsTab==1,contains(audioTab,mouse));
   DrawLine((int)settingsLeft+42,(int)settingsTop+129,(int)(settingsLeft+settingsWidth-42),(int)settingsTop+129,C(58,115,131,95));
   if(settingsTab==0){
    DrawRectangleRoundedLines(frameRow,.08f,8,settingsFocus==0?C(76,201,197,115):C(47,89,105,75));
    DrawText("FRAME RATE LIMIT",(int)settingsLeft+48,(int)settingsTop+153,16,settingsFocus==0?C(205,243,237):C(145,183,191));
    constexpr int frameRates[]={30,60,120,0};
    const char* rateLabels[]={"30 FPS","60 FPS","120 FPS","UNLIMITED"};
    for(int i=0;i<4;i++){
     const bool configured=appSettings.frameRateLimit==frameRates[i];
     drawButton(fpsChips[i],rateLabels[i],configured,contains(fpsChips[i],mouse),13);
    }
    DrawText("Unlimited removes the game limiter; display or driver caps may still apply.",(int)settingsLeft+48,(int)settingsTop+195,13,C(111,164,176));
    DrawText("SHOW FPS",(int)settingsLeft+48,(int)settingsTop+236,16,settingsFocus==1?C(205,243,237):C(145,183,191));
    DrawText("Measured output rate",(int)settingsLeft+48,(int)settingsTop+259,12,C(111,164,176));
    drawButton(showToggle,appSettings.showFps?"ON":"OFF",settingsFocus==1,contains(showToggle,mouse),14);
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
   drawButton(settingsBack,"BACK",(settingsTab==0&&settingsFocus==2)||(settingsTab==1&&settingsFocus==1),contains(settingsBack,mouse));
   DrawText("TAB SWITCHES SECTIONS     ESC RETURNS",(int)settingsLeft+48,(int)settingsTop+488,11,C(108,167,178));
  }
  if(mode==Mode::ExitConfirm){
   const Rectangle bounds=panel("LEAVE THE GARDEN?","CONFIRM EXIT","Your current run will end.");
   constexpr const char* question="Are you sure you want to exit ThreadLight?";
   DrawText(question,(int)(SW*.5f-MeasureText(question,16)*.5f),(int)bounds.y+190,16,C(188,224,222));
   const Rectangle yesButton{SW*.5f-230.f,SH*.5f+52.f,210.f,48.f};
   const Rectangle noButton{SW*.5f+20.f,SH*.5f+52.f,210.f,48.f};
   drawButton(yesButton,"Yes, Exit Game",exitSelection==0,contains(yesButton,mouse),15);
   drawButton(noButton,"No, Stay",exitSelection==1,contains(noButton,mouse),15);
   DrawText("ESC CANCELS",SW/2-45,SH/2+122,12,C(108,167,178));
  }
  if(mode==Mode::Upgrade){panel("A NEW PATTERN TAKES SHAPE","CHOOSE AN EVOLUTION","1 / A, 2 / D, 3 / S  then  ENTER");const char* names[3]={"01   HEARTWOOD","02   RAZOR FILAMENT","03   QUICKSTEP"};const char* desc[3]={"Restore 35 vitality","Silk deals more damage","Move faster; dash recharges sooner"};for(int i=0;i<3;i++){int x=SW/2-260+i*176;Color outline=choice==i?C(133,255,221,220):C(67,115,132,100);DrawRectangleRounded({(float)x,(float)(SH/2+5),160,126},.08f,8,C(20,42,57,230));DrawRectangleRoundedLines({(float)x,(float)(SH/2+5),160,126},.08f,8,outline);DrawText(names[i],x+12,SH/2+23,13,C(212,249,231));DrawText(desc[i],x+12,SH/2+57,13,C(139,191,193));}DrawText("SELECT WITH 1 / 2 / 3, OR A / D / S",SW/2-185,SH/2+159,14,C(255,218,135));}
  if(mode==Mode::Lost||mode==Mode::Won){panel(mode==Mode::Won?"DAWN RECLAIMS THE GARDEN":"THE THREAD GOES QUIET",mode==Mode::Won?"YOU HELD THE NIGHT":"LOST TO THE BRIAR","The hunters remember your path.");DrawText(TextFormat("Score %06d     Hunters cut %d     Evolutions %d",score,kills,levelN-1),SW/2-255,SH/2+24,18,C(179,225,216));DrawText("ENTER / R  —  weave again",SW/2-255,SH/2+76,16,C(255,218,135));DrawText("ESC  —  return to title",SW/2-255,SH/2+111,15,C(136,188,192));}
  if(appSettings.showFps)
   DrawText(TextFormat("FPS %d",(int)std::lround(measuredFps)),8,5,10,Fade(C(174,222,228),.32f));
  sfx.update();
  EndDrawing();
 }
 sfx.shutdown();
 if(audioReady)CloseAudioDevice();
 CloseWindow();return 0;
}
