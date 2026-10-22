#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
constexpr int SW=1280, SH=800;
struct V { float x{},y{}; };
V operator+(V a,V b){return {a.x+b.x,a.y+b.y};} V operator-(V a,V b){return {a.x-b.x,a.y-b.y};} V operator*(V a,float s){return {a.x*s,a.y*s};}
float len(V a){return std::sqrt(a.x*a.x+a.y*a.y);} float dist(V a,V b){return len(a-b);} V norm(V a){float l=len(a);return l>0.001f?a*(1/l):V{};} Vector2 cv(V a){return {a.x,a.y};}
Color C(unsigned char r,unsigned char g,unsigned char b,unsigned char a=255){return {r,g,b,a};}
struct Thread {V p; float age;}; struct Foe {V p,vel; float r,hp,maxhp,speed,hitCd,phase; int kind;}; struct Spark {V p; float t;}; struct Particle {V p,v;float life,maxLife,size;Color color;};
enum class Mode{Title,Play,Pause,Upgrade,Lost,Won};
}
int main(){
 SetConfigFlags(FLAG_WINDOW_HIGHDPI|FLAG_MSAA_4X_HINT); InitWindow(SW,SH,"THREADLIGHT | A small game of living silk"); SetExitKey(KEY_NULL); SetTargetFPS(60); InitAudioDevice();
 constexpr int SR=22050; auto tone=[&](float hz,float sec,float vol){int n=(int)(SR*sec); short* d=(short*)MemAlloc(n*sizeof(short)); for(int i=0;i<n;i++){float t=(float)i/SR, env=std::exp(-t*7.0f);d[i]=(short)(std::sin(2*PI*hz*t)*env*vol*32767);} Wave w{1,SR,16,(unsigned int)n,d}; Sound s=LoadSoundFromWave(w); UnloadWave(w); return s;};
 Sound ping=tone(880,.10f,.20f), cut=tone(180,.18f,.26f), level=tone(520,.28f,.18f), hurt=tone(110,.22f,.22f);
 Mode mode=Mode::Title; V player{SW/2.f,SH/2.f}, aim{1,0}; float hp=100, energy=0, invuln=0,dashCd=0,dashTime=0, spawnT=0, waveT=0, shake=0, pulse=0; int wave=0, score=0, kills=0, levelN=1, choice=0; float speed=255, trailDmg=1, radius=12; bool dashUpgrade=false;
 std::vector<Thread> thread; std::vector<Foe> foes; std::vector<Spark> sparks; std::vector<Particle> particles; std::vector<V> stars;
 for(int i=0;i<110;i++) stars.push_back({(float)GetRandomValue(20,SW-20),(float)GetRandomValue(20,SH-20)});
 auto burst=[&](V p,Color col,int n,float force){for(int i=0;i<n;i++){float a=(float)GetRandomValue(0,6283)/1000.f,s=(float)GetRandomValue(25,(int)force);particles.push_back({p,{std::cos(a)*s,std::sin(a)*s},.65f,.65f,(float)GetRandomValue(2,6),col});}};
 auto reset=[&](){player={SW/2.f,SH/2.f};hp=100;energy=0;invuln=dashCd=dashTime=spawnT=waveT=shake=pulse=0;wave=0;score=kills=0;levelN=1;speed=255;trailDmg=1;radius=12;dashUpgrade=false;thread.clear();foes.clear();sparks.clear();particles.clear();mode=Mode::Play;};
 auto addFoe=[&](int w){float a=(float)GetRandomValue(0,6283)/1000.f; V p{SW/2.f+std::cos(a)*680,SH/2.f+std::sin(a)*480};p.x=std::clamp(p.x,25.f,SW-25.f);p.y=std::clamp(p.y,25.f,SH-25.f);int k=(GetRandomValue(0,9)<(w>2?3:2))?1:0;float r=k?20:14;foes.push_back({p,{},r,k?3.f:1.f,k?3.f:1.f,k?66.f:95.f,0,(float)GetRandomValue(0,6283)/1000.f,k});};
 auto segDist=[](V p,V a,V b){V ab=b-a;float q=ab.x*ab.x+ab.y*ab.y;float t=q>0?std::clamp(((p.x-a.x)*ab.x+(p.y-a.y)*ab.y)/q,0.f,1.f):0;return dist(p,a+ab*t);};
 auto drawText=[&](const char* s,int x,int y,int size,Color c){DrawText(s,x,y,size,c);};
 while(!WindowShouldClose()){
  float dt=std::min(GetFrameTime(),.033f); Vector2 m=GetMousePosition();
  if(mode==Mode::Title){if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE))reset();}
  else if(mode==Mode::Play){
   if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_P))mode=Mode::Pause;
   aim=norm(V{m.x-player.x,m.y-player.y}); invuln=std::max(0.f,invuln-dt);dashCd=std::max(0.f,dashCd-dt);pulse=std::max(0.f,pulse-dt);shake=std::max(0.f,shake-dt*22);
   V mv{(float)(IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT))-(float)(IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT)),(float)(IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN))-(float)(IsKeyDown(KEY_W)||IsKeyDown(KEY_UP))}; if(len(mv)>0)mv=norm(mv);
   if(IsKeyPressed(KEY_SPACE)&&dashCd<=0&&len(mv)>0){dashTime=.19f;dashCd=dashUpgrade?1.05f:1.55f;invuln=.28f;burst(player,C(132,244,255),10,150);PlaySound(cut);}
   float mult=dashTime>0?3.7f:1.f; if(dashTime>0)dashTime-=dt;player=player+mv*(speed*mult*dt);player.x=std::clamp(player.x,40.f,SW-40.f);player.y=std::clamp(player.y,95.f,SH-40.f);
   if(thread.empty()||dist(thread.back().p,player)>9)thread.push_back({player,0});for(auto &t:thread)t.age+=dt;while(!thread.empty()&&(thread.front().age>5.5f||thread.size()>520))thread.erase(thread.begin());
   waveT+=dt;spawnT-=dt;if(spawnT<=0&&wave<6){addFoe(wave);spawnT=std::max(.30f,1.08f-wave*.09f);if(waveT>17+wave*3){wave++;waveT=0;mode=wave<=5?Mode::Upgrade:Mode::Won;PlaySound(level);}}
   for(auto &f:foes){V d=norm(player-f.p);f.phase+=dt;f.hitCd=std::max(0.f,f.hitCd-dt);float wob=std::sin(f.phase*3)*.28f;f.vel=f.vel+(d+V{-d.y*wob,d.x*wob})*(f.speed*dt*2.5f);float vl=len(f.vel);if(vl>f.speed)f.vel=f.vel*(f.speed/vl);f.p=f.p+f.vel*dt;
    for(size_t i=1;i<thread.size();i++){if(thread[i].age>1.2f)continue;if(segDist(f.p,thread[i-1].p,thread[i].p)<f.r+5&&f.hitCd<=0){f.hp-=trailDmg;f.hitCd=.27f;burst(f.p,C(83,255,226,220),4,70);if(f.hp<=0){sparks.push_back({f.p,0});score+=f.kind?35:10;kills++;burst(f.p,C(255,123,186),15,180);f.r=0;PlaySound(ping);}break;}}
    if(f.r>0&&dist(f.p,player)<f.r+radius&&invuln<=0){hp-=f.kind?16:10;invuln=.78f;shake=7;PlaySound(hurt);burst(player,C(255,92,119),13,170);}
   }
   foes.erase(std::remove_if(foes.begin(),foes.end(),[](Foe f){return f.r<=0;}),foes.end());
   for(auto &s:sparks){s.t+=dt;if(dist(s.p,player)<110){V v=norm(player-s.p);s.p=s.p+v*(220*dt);}if(dist(s.p,player)<radius+12){energy+=1;score+=5;s.t=-1;PlaySound(ping);burst(s.p,C(255,218,125),8,100);}}
   sparks.erase(std::remove_if(sparks.begin(),sparks.end(),[](Spark s){return s.t<0||s.t>12;}),sparks.end());
   for(auto &p:particles){p.life-=dt;p.p=p.p+p.v*dt;p.v=p.v*.96f;}particles.erase(std::remove_if(particles.begin(),particles.end(),[](Particle p){return p.life<=0;}),particles.end());
   if(energy>=8){energy-=8;levelN++;mode=Mode::Upgrade;PlaySound(level);}if(hp<=0)mode=Mode::Lost;
  } else if(mode==Mode::Pause){if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_P))mode=Mode::Play;if(IsKeyPressed(KEY_R))reset();}
  else if(mode==Mode::Upgrade){if(IsKeyPressed(KEY_ONE)||IsKeyPressed(KEY_A))choice=0; if(IsKeyPressed(KEY_TWO)||IsKeyPressed(KEY_D))choice=1;if(IsKeyPressed(KEY_THREE)||IsKeyPressed(KEY_S))choice=2;if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_SPACE)){if(choice==0){hp=std::min(100.f,hp+35);}if(choice==1){trailDmg+=.65f;}if(choice==2){speed+=24;dashUpgrade=true;}mode=Mode::Play;}}
  else if(mode==Mode::Lost||mode==Mode::Won){if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_R))reset();if(IsKeyPressed(KEY_ESCAPE))mode=Mode::Title;}

  BeginDrawing();ClearBackground(C(7,12,27));
  for(int i=0;i<4;i++){float x=200+i*310;DrawCircleGradient(x,310+(i%2)*180,210,C(13,33,57,45),C(7,12,27,0));}
  for(size_t i=0;i<stars.size();i++){float tw=.4f+.6f*(.5f+.5f*std::sin((float)GetTime()*(.6f+(i%7)*.13f)+i));DrawCircleV(cv(stars[i]),i%11==0?2:1,C(114,183,213,(unsigned char)(40+tw*130)));}
  for(int x=32;x<SW;x+=48)DrawLine(x,82,x,SH,C(34,66,83,26));for(int y=96;y<SH;y+=48)DrawLine(0,y,SW,y,C(34,66,83,22));
  for(int x=80;x<SW;x+=120){float b=.5f+.5f*std::sin(GetTime()*1.7+x);DrawCircle(x,88,3,C(68,226,207,(unsigned char)(90+b*120)));DrawCircle(x,88,9,C(45,194,198,18));}
  if(thread.size()>1){for(int pass=0;pass<3;pass++){for(size_t i=1;i<thread.size();i++){float life=1-thread[i].age/5.5f;Color c=pass==0?C(20,244,216,(unsigned char)(life*50)):pass==1?C(45,239,228,(unsigned char)(life*100)):C(204,255,235,(unsigned char)(life*210));DrawLineEx(cv(thread[i-1].p),cv(thread[i].p),pass==0?15:pass==1?7:2,c);}}}
  for(auto s:sparks){float r=8+2*std::sin(GetTime()*6+s.t);DrawPoly(cv(s.p),4,r,45,C(255,209,115));DrawCircleV(cv(s.p),r+8,C(255,185,95,25));}
  for(auto f:foes){Color c=f.kind?C(255,116,178):C(255,97,132);float bob=std::sin(f.phase*5)*3;DrawCircleV({f.p.x,f.p.y+bob},f.r+10,C(255,63,123,20));DrawPoly({f.p.x,f.p.y+bob},f.kind?6:4,f.r,GetTime()*40,C(25,22,45));DrawPolyLinesEx({f.p.x,f.p.y+bob},f.kind?6:4,f.r,GetTime()*40,2,c);DrawCircleV({f.p.x-4,f.p.y+bob-2},2,C(255,224,201));DrawCircleV({f.p.x+4,f.p.y+bob-2},2,C(255,224,201));if(f.hp<f.maxhp){DrawRectangle((int)(f.p.x-f.r),(int)(f.p.y-f.r-9),(int)(2*f.r),3,C(50,48,71));DrawRectangle((int)(f.p.x-f.r),(int)(f.p.y-f.r-9),(int)(2*f.r*f.hp/f.maxhp),3,C(255,118,157));}}
  for(auto p:particles){float a=std::clamp(p.life/p.maxLife,0.f,1.f);DrawCircleV(cv(p.p),p.size*a,Fade(p.color,a));}
  if(mode!=Mode::Title){float flash=invuln>0?(.45f+.45f*std::sin(GetTime()*24)):1;DrawCircleV(cv(player),31,C(45,230,214,22));DrawCircleV(cv(player),21,C(51,230,221,40));DrawCircleV(cv(player),radius+4,C(164,255,232,220));DrawCircleV(cv(player),radius,C(221,255,245,(unsigned char)(flash*255)));DrawLineEx(cv(player),cv(player+aim*25),3,C(255,252,221));}
  DrawRectangle(0,0,SW,76,C(5,10,24,235));DrawLine(0,76,SW,76,C(60,152,156,80));drawText("THREADLIGHT",34,22,25,C(200,255,243));drawText("THE BRIAR CIRCUIT",224,31,13,C(91,167,179));
  DrawText("VITALITY",390,17,11,C(120,164,178));DrawRectangle(390,38,180,10,C(34,49,67));DrawRectangle(390,38,(int)(180*std::clamp(hp/100,0.f,1.f)),10,hp>35?C(72,225,190):C(255,91,127));
  drawText(TextFormat("WAVE %d / 5",std::min(wave+1,5)),615,22,18,C(217,239,242));drawText(TextFormat("SPARKS  %d / 8",(int)energy),800,24,15,C(255,213,131));drawText(TextFormat("SCORE  %06d",score),1033,24,17,C(177,208,221));
  if(mode==Mode::Play){DrawRectangle(20,SH-52,SW-40,32,C(5,10,24,185));drawText("WASD MOVE",36,SH-43,12,C(149,191,199));drawText("SPACE DASH",185,SH-43,12,C(149,191,199));drawText("YOUR SILK CUTS THE HUNTERS. LOOP BACK THROUGH YOUR WAKE.",364,SH-43,12,C(102,219,207));drawText("P / ESC PAUSE",1055,SH-43,12,C(149,191,199));}
  auto panel=[&](const char* kicker,const char* title,const char* sub){DrawRectangle(0,0,SW,SH,C(3,7,18,195));DrawRectangleRounded({SW/2.f-310,SH/2.f-186,620,372},.04f,12,C(11,22,40,245));DrawRectangleRoundedLines({SW/2.f-310,SH/2.f-186,620,372},.04f,12,C(91,216,197,130));DrawText(kicker,SW/2-255,SH/2-139,13,C(107,228,206));DrawText(title,SW/2-255,SH/2-101,42,C(226,255,247));DrawText(sub,SW/2-255,SH/2-43,17,C(162,193,204));};
  if(mode==Mode::Title){panel("A LUMINOUS SURVIVAL STORY","THREADLIGHT","A small game of living silk");DrawText("Move to weave a fading thread. Circle the hunters; let the silk do the cutting.",SW/2-255,SH/2+15,16,C(175,211,213));DrawText("WASD move     SPACE dash     P pause",SW/2-255,SH/2+53,16,C(121,196,197));DrawText("Collect 8 sparks to evolve. Survive five waves.",SW/2-255,SH/2+84,16,C(121,196,197));DrawText("PRESS ENTER TO ENTER THE GARDEN",SW/2-255,SH/2+137,16,C(255,218,135));}
  if(mode==Mode::Pause){panel("THE GARDEN HOLDS ITS BREATH","PAUSED","Your thread is waiting.");DrawText("P or ESC  —  resume",SW/2-255,SH/2+32,18,C(181,224,219));DrawText("R  —  begin again",SW/2-255,SH/2+68,18,C(181,224,219));}
  if(mode==Mode::Upgrade){panel("A NEW PATTERN TAKES SHAPE","CHOOSE AN EVOLUTION","1 / A, 2 / D, 3 / S  then  ENTER");const char* names[3]={"01   HEARTWOOD","02   RAZOR FILAMENT","03   QUICKSTEP"};const char* desc[3]={"Restore 35 vitality","Silk deals more damage","Move faster; dash recharges sooner"};for(int i=0;i<3;i++){int x=SW/2-260+i*176;Color outline=choice==i?C(133,255,221,220):C(67,115,132,100);DrawRectangleRounded({(float)x,SH/2+5,160,126},.08f,8,C(20,42,57,230));DrawRectangleRoundedLines({(float)x,SH/2+5,160,126},.08f,8,outline);DrawText(names[i],x+12,SH/2+23,13,C(212,249,231));DrawText(desc[i],x+12,SH/2+57,13,C(139,191,193));}DrawText("SELECT WITH 1 / 2 / 3, OR A / D / S",SW/2-185,SH/2+159,14,C(255,218,135));}
  if(mode==Mode::Lost||mode==Mode::Won){panel(mode==Mode::Won?"DAWN RECLAIMS THE GARDEN":"THE THREAD GOES QUIET",mode==Mode::Won?"YOU HELD THE NIGHT":"LOST TO THE BRIAR","The hunters remember your path.");DrawText(TextFormat("Score %06d     Hunters cut %d     Evolutions %d",score,kills,levelN-1),SW/2-255,SH/2+24,18,C(179,225,216));DrawText("ENTER / R  —  weave again",SW/2-255,SH/2+76,16,C(255,218,135));DrawText("ESC  —  return to title",SW/2-255,SH/2+111,15,C(136,188,192));}
  EndDrawing();
 }
 UnloadSound(ping);UnloadSound(cut);UnloadSound(level);UnloadSound(hurt);CloseAudioDevice();CloseWindow();return 0;
}
