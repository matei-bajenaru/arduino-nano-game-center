#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <math.h>

#define TFT_CS    10
#define TFT_DC     8
#define TFT_RST    9
#define BTN_SPIN   2
#define BTN_BET    3
#define BTN_DOUBLE 4
#define BUZZER     6

#define C_BG      0x000C
#define C_PANEL   0x0812
#define C_BORDER  0x0C29
#define C_GOLD    0xFEA0
#define C_GOLD2   0xC4A0
#define C_RED     0xE000
#define C_DRED    0x8000
#define C_GREEN   0x07A0
#define C_DGREEN  0x0360
#define C_BLUE    0x115F
#define C_LBLUE   0x0ABF
#define C_CYAN    0x07EF
#define C_PURPLE  0x781F
#define C_LPURP   0xAC1F
#define C_ORANGE  0xFB40
#define C_YELLOW  0xFFE0
#define C_DYELLOW 0xCC40
#define C_GREY    0x8410
#define C_DGREY   0x39E7
#define C_PINK    0xF81F
#define C_WHITE   0xFFFF
#define C_LIME    0x87E0

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#define SCR_W  320
#define SCR_H  240
#define HDR_H   40
#define RW      84
#define RH      90
#define RGAP     6
#define RY      50
#define RX0     14
#define RCY    (RY + RH/2)
#define STY    (RY + RH + 10)
#define BTNY   196
#define BTNH    36

const int RCX[3] = { RX0+RW/2, RX0+RW+RGAP+RW/2, RX0+2*(RW+RGAP)+RW/2 };

#define NS 10
const uint8_t SW[NS]  = {14, 5,12, 8,10,14,10,12,10, 5};
const uint8_t PAY[NS] = { 2,15, 5, 8, 6, 2, 4, 5, 5, 3};
#define P7ANY 5

int  reels[3]={0,0,0}, credit=100, bet=10, lastWin=0, doubles=0;
bool gMode=false, lost=false, jack=false, s7any=false, spin=false, stopReq=false;
bool inGamble=false;
uint8_t gHistory[4]={0,0,0,0}; // culoarea cartii per runda: 0=gol, 1=rosu, 2=negru
unsigned long lp[3]={0,0,0};
#define DEB 160UL

const int MWF[]={1047,1319,1568,2093}, MWD[]={70,70,70,180};
const int MJF[]={880,1047,1319,1568,1760,2093,2349}, MJD[]={60,60,60,60,60,60,300};
const int MLF[]={400,280,180}, MLD[]={80,100,160};
const int MDF[]={1400,1800,2200}, MDD[]={50,50,100};

void drawUI(); void drawHdr(); void drawReel(int,uint16_t);
void drawSym(int,int,int); void drawStatus(); void drawPips();
void drawDPips(); void drawBtns(); void drawPL(); void splash();
void doSpin(); void chkWin(); void doGamble(bool); void resetW();
void flashW(bool); void cprint(const char*,int,uint16_t,uint8_t);
void smsg(const char*,const char*,uint16_t);
void drawGambleScreen(); void drawCard(int,int,int,int,bool,bool);
void drawGambleHistory(); void showGambleResult(bool,bool);
void enterGamble(); void exitGamble(bool);
const char* sname(int); int wrand(); void mel(const int*,const int*,int);

void setup(){
  pinMode(BTN_SPIN,INPUT_PULLUP); pinMode(BTN_BET,INPUT_PULLUP);
  pinMode(BTN_DOUBLE,INPUT_PULLUP); pinMode(BUZZER,OUTPUT);
  tft.init(240,320); tft.setRotation(1); tft.fillScreen(C_BG);
  randomSeed(analogRead(A0)^(analogRead(A1)<<4));
  splash(); delay(1500); drawUI();
}

void loop(){
  uint32_t now=millis();

  // Ecran gamble activ — BET=iesi, SPIN=alege negru, DOUBLE=alege rosu
  if(inGamble){
    if(!digitalRead(BTN_BET)&&now-lp[1]>DEB){
      // Iesi din gamble, colecteaza
      exitGamble(false); lp[1]=now;
    }
    if(!digitalRead(BTN_SPIN)&&now-lp[0]>DEB){
      doGamble(false); lp[0]=now;  // alege NEGRU
    }
    if(!digitalRead(BTN_DOUBLE)&&now-lp[2]>DEB){
      doGamble(true); lp[2]=now;   // alege ROSU
    }
    return;
  }

  if(!digitalRead(BTN_BET)&&!gMode&&!spin&&now-lp[1]>DEB){
    bet=(bet>=50)?10:bet+10; drawPips(); drawHdr(); drawBtns();
    tone(BUZZER,500+bet*10,25); lp[1]=now;
  }
  if(!digitalRead(BTN_SPIN)&&now-lp[0]>DEB){
    if(spin){ stopReq=true; lp[0]=now; }
    else if(gMode){ credit+=lastWin; resetW(); drawUI(); tone(BUZZER,900,60); lp[0]=now; }
    else if(credit>=bet){ doSpin(); lp[0]=now; }
    else{ smsg("CREDIT","INSUFICIENT",C_RED); tone(BUZZER,200,300); lp[0]=now; }
  }
  if(!digitalRead(BTN_DOUBLE)&&gMode&&!spin&&doubles<4&&now-lp[2]>DEB){
    enterGamble(); lp[2]=now;
  }
}

void splash(){
  tft.fillScreen(C_BG);
  tft.drawRect(3,3,SCR_W-6,SCR_H-6,C_GOLD);
  tft.drawRect(6,6,SCR_W-12,SCR_H-12,C_GOLD2);
  for(int i=0;i<18;i++) tft.fillCircle(15+random(290),15+random(210),1,C_GOLD);
  tft.setTextSize(4);
  tft.setTextColor(C_GOLD);  tft.setCursor(38,48);  tft.print(F("LUCKY"));
  tft.setTextColor(C_GOLD2); tft.setCursor(38,92);  tft.print(F("REELS"));
  cprint("v2.1",142,C_GREY,1);
  int px=30; for(int s=0;s<5;s++){drawSym(s,px,182);px+=56;}
  cprint("PRESS SPIN TO START",218,C_LBLUE,1);
}

void drawUI(){
  tft.fillScreen(C_BG); drawHdr();
  for(int i=0;i<3;i++) drawReel(i,C_BORDER);
  drawPL(); drawPips(); drawStatus(); drawDPips(); drawBtns();
}

void drawHdr(){
  tft.fillRect(0,0,SCR_W,HDR_H,C_PANEL);
  tft.drawFastHLine(0,HDR_H,SCR_W,C_BORDER);
  tft.setTextColor(C_DGREY); tft.setTextSize(1);
  tft.setCursor(8,7); tft.print(F("CREDIT"));
  tft.setTextColor(C_CYAN); tft.setTextSize(2);
  char b[8]; sprintf(b,"%5d",credit); tft.setCursor(8,20); tft.print(b);
  tft.setTextColor(C_GOLD);  tft.setTextSize(1); tft.setCursor(128,10); tft.print(F("LUCKY"));
  tft.setTextColor(C_GOLD2);                     tft.setCursor(128,22); tft.print(F("REELS"));
  tft.setTextColor(C_DGREY); tft.setCursor(248,7); tft.print(F("BET"));
  tft.setTextColor(C_YELLOW); tft.setTextSize(2);
  sprintf(b,"%2d",bet); tft.setCursor(252,20); tft.print(b);
}

void drawReel(int i,uint16_t bc){
  int lx=RX0+i*(RW+RGAP);
  tft.fillRect(lx+1,RY+1,RW-2,RH-2,C_BG);
  tft.drawRect(lx,RY,RW,RH,bc);
  tft.fillRect(lx+1,RY+1,RW-2,3,C_PANEL);
  tft.fillRect(lx+1,RY+RH-4,RW-2,3,C_PANEL);
  drawSym(reels[i],RCX[i],RCY);
}

void drawPL(){
  int my=RCY;
  tft.drawFastHLine(0,my,RX0-4,C_GOLD); tft.fillCircle(RX0-4,my,2,C_GOLD);
  int rx=RX0+3*RW+2*RGAP;
  tft.fillCircle(rx+4,my,2,C_GOLD); tft.drawFastHLine(rx+4,my,SCR_W-rx-4,C_GOLD);
}

void drawPips(){
  int y=RY+RH+6; tft.fillRect(0,y,SCR_W,8,C_BG);
  int lv=bet/10, pw=44, gap=5, sx=(SCR_W-(5*pw+4*gap))/2;
  for(int i=0;i<5;i++){
    uint16_t c=(i<lv)?C_PURPLE:C_BORDER;
    tft.fillRect(sx+i*(pw+gap),y+1,pw,5,c);
    if(i<lv) tft.drawFastHLine(sx+i*(pw+gap),y+1,pw,C_LPURP);
  }
}

void drawStatus(){
  int y=STY+4;
  tft.fillRect(0,STY,SCR_W,BTNY-STY-12,C_BG);
  if(lost){ cprint("AI PIERDUT!",y,C_RED,2); cprint("JOACA DIN NOU",y+20,C_DGREY,1); }
  else if(gMode){
    char b[24];
    bool triple=(reels[0]==reels[1]&&reels[1]==reels[2]);
    if(jack)        cprint(triple?"JACKPOT TRIPLE!":"JACKPOT! 7+7",y,C_GOLD,2);
    else if(s7any)  cprint("SEVEN + ORICE!",y,C_ORANGE,2);
    else if(triple){ sprintf(b,"TRIPLE %s!",sname(reels[0])); cprint(b,y,C_GOLD,1); }
    else{ sprintf(b,"%s + %s",sname(reels[0]),sname(reels[0])); cprint(b,y,C_GREEN,1); }
    sprintf(b,"WIN: %d  GAMBLE?",lastWin); cprint(b,y+18,C_GOLD,1);
  } else {
    cprint("APASA SPIN",y+4,C_LBLUE,2);
  }
}

void drawDPips(){
  int y=BTNY-15; tft.fillRect(0,y,SCR_W,12,C_BG);
  if(!gMode) return;
  int dw=18,dgap=6,sx=(SCR_W-(4*dw+3*dgap))/2;
  for(int i=0;i<4;i++){
    uint16_t c=(i<doubles)?C_ORANGE:C_BORDER;
    tft.fillRect(sx+i*(dw+dgap),y,dw,10,c);
    if(i<doubles) tft.drawFastHLine(sx+i*(dw+dgap),y,dw,0xFBA0);
  }
}

void drawBtns(){
  tft.fillRect(0,BTNY-1,SCR_W,SCR_H-BTNY+1,C_PANEL);
  tft.drawFastHLine(0,BTNY-1,SCR_W,C_BORDER);
  int bw=88,gap=8,y=BTNY+3,x0=(SCR_W-3*bw-2*gap)/2;
  uint16_t bc=gMode?C_GREEN:C_BLUE, tc=gMode?C_GREEN:C_CYAN;
  tft.fillRect(x0,y,bw,BTNH,C_BG); tft.drawRect(x0,y,bw,BTNH,bc);
  tft.setTextColor(tc); tft.setTextSize(1);
  tft.setCursor(x0+8,y+12); tft.print(gMode?F(" COLLECT"):F("  SPIN"));
  int x1=x0+bw+gap;
  tft.fillRect(x1,y,bw,BTNH,C_BG); tft.drawRect(x1,y,bw,BTNH,C_PURPLE);
  tft.setTextColor(C_LPURP); tft.setCursor(x1+8,y+12); tft.print(F(" BET +10"));
  int x2=x1+bw+gap; bool cd=gMode&&doubles<4; uint16_t dc=cd?C_ORANGE:C_BORDER;
  tft.fillRect(x2,y,bw,BTNH,C_BG); tft.drawRect(x2,y,bw,BTNH,dc);
  tft.setTextColor(dc); tft.setCursor(x2+6,y+12); tft.print(F("DOUBLE x2"));
}

void drawSym(int s,int cx,int cy){
  switch(s){
    case 0:
      tft.fillCircle(cx-11,cy+4,11,C_RED);   tft.fillCircle(cx+11,cy+4,11,C_RED);
      tft.fillCircle(cx-11,cy+7,6,C_DRED);   tft.fillCircle(cx+11,cy+7,6,C_DRED);
      tft.fillCircle(cx-14,cy,3,C_PINK);      tft.fillCircle(cx+8,cy,3,C_PINK);
      tft.drawLine(cx-11,cy-7,cx,cy-20,C_DGREEN);
      tft.drawLine(cx+11,cy-7,cx,cy-20,C_DGREEN);
      tft.drawLine(cx,cy-20,cx,cy-28,C_DGREEN);
      tft.fillCircle(cx+4,cy-20,4,C_GREEN); break;
    case 1:
      tft.setTextSize(5);
      tft.setTextColor(C_GOLD2); tft.setCursor(cx-16,cy-28); tft.print('7');
      tft.setTextColor(C_RED);   tft.setCursor(cx-17,cy-29); tft.print('7');
      tft.setTextColor(C_GOLD);  tft.setCursor(cx-18,cy-30); tft.print('7');
      tft.setTextSize(1); tft.setTextColor(C_YELLOW);
      tft.setCursor(cx-14,cy+18); tft.print(F("LUCKY")); break;
    case 2:
      tft.fillCircle(cx,cy-10,16,C_YELLOW);
      tft.fillRect(cx-16,cy-10,32,16,C_YELLOW);
      tft.drawCircle(cx,cy-10,16,C_GOLD);
      tft.drawFastHLine(cx-16,cy+6,32,C_GOLD);
      tft.fillRect(cx-19,cy+5,38,5,C_YELLOW);
      tft.drawRect(cx-19,cy+5,38,5,C_GOLD);
      tft.fillCircle(cx,cy+14,5,C_GOLD);    tft.fillCircle(cx,cy+14,3,C_DYELLOW);
      tft.fillCircle(cx-6,cy-17,4,C_WHITE); break;
    case 3:{
      // Forma clasica diamant: coroana sus (trapez) + pavilion jos (triunghi)
      // Coroana — trapez sus, albastru deschis
      tft.fillTriangle(cx-22,cy-4, cx-10,cy-22, cx+10,cy-22, C_LBLUE);
      tft.fillTriangle(cx-22,cy-4, cx+10,cy-22, cx+22,cy-4,  C_LBLUE);
      // Fateta centrala sus — mai deschisa
      tft.fillTriangle(cx,cy-22, cx-10,cy-22, cx-4,cy-4, C_CYAN);
      tft.fillTriangle(cx,cy-22, cx+10,cy-22, cx+4,cy-4, 0x4FFF);
      // Fateta tabla (centru coroana)
      tft.fillTriangle(cx-4,cy-4, cx+4,cy-4, cx,cy-14, C_WHITE);
      // Pavilion — triunghi jos, albastru mai inchis
      tft.fillTriangle(cx-22,cy-4, cx+22,cy-4, cx,cy+26, C_BLUE);
      // Fatete pavilion stanga/dreapta
      tft.fillTriangle(cx-22,cy-4, cx,cy-4, cx,cy+26, 0x0C5F);
      tft.fillTriangle(cx+22,cy-4, cx,cy-4, cx,cy+26, C_LBLUE);
      // Contur
      tft.drawTriangle(cx-22,cy-4, cx+22,cy-4, cx,cy+26, C_WHITE);
      tft.drawLine(cx-22,cy-4, cx-10,cy-22, C_WHITE);
      tft.drawLine(cx-10,cy-22, cx+10,cy-22, C_WHITE);
      tft.drawLine(cx+10,cy-22, cx+22,cy-4, C_WHITE);
      tft.drawLine(cx-22,cy-4, cx+22,cy-4, C_WHITE);
      // Scantei
      tft.fillCircle(cx-8,cy-16,2,C_WHITE);
      tft.fillCircle(cx+6,cy-10,2,C_WHITE); break;}
    case 4:{
      float da=TWO_PI/5.0f, off=-HALF_PI;
      for(int p=0;p<5;p++){
        float a=off+p*da, al=a-da/2.0f, ar=a+da/2.0f;
        tft.fillTriangle(cx+(int)(24*cos(a)),cy+(int)(24*sin(a)),
                         cx+(int)(10*cos(al)),cy+(int)(10*sin(al)),
                         cx+(int)(10*cos(ar)),cy+(int)(10*sin(ar)),C_YELLOW);
      }
      tft.fillCircle(cx,cy,10,C_YELLOW); tft.fillCircle(cx,cy,5,C_DYELLOW);
      tft.fillCircle(cx-4,cy-4,3,C_WHITE); break;}
    case 5:
      tft.fillCircle(cx,cy,16,C_YELLOW);
      tft.fillCircle(cx-6,cy-4,12,C_YELLOW);
      tft.fillCircle(cx+6,cy-4,12,C_YELLOW);
      tft.drawCircle(cx,cy,16,C_DGREEN);
      tft.drawCircle(cx-6,cy-4,12,C_DGREEN);
      tft.drawCircle(cx+6,cy-4,12,C_DGREEN);
      tft.fillCircle(cx-18,cy-2,4,C_YELLOW); tft.drawCircle(cx-18,cy-2,4,C_DGREEN);
      tft.fillCircle(cx+18,cy-2,4,C_YELLOW); tft.drawCircle(cx+18,cy-2,4,C_DGREEN);
      tft.fillRect(cx-2,cy-24,4,8,C_DGREEN); tft.fillCircle(cx,cy-25,3,C_GREEN);
      tft.fillCircle(cx-7,cy-8,4,C_WHITE); break;
    case 6:
      tft.fillRect(cx-26,cy-16,52,32,C_DGREY);
      tft.drawRect(cx-26,cy-16,52,32,C_GOLD);
      tft.drawRect(cx-24,cy-14,48,28,C_GOLD2);
      tft.setTextColor(C_GOLD); tft.setTextSize(2);
      tft.setCursor(cx-18,cy-8); tft.print(F("BAR")); break;
    case 7:
      // Felie pepene rotita 90 stanga: verde stanga, rosu dreapta
      // Coaja verde (semicerc stang)
      tft.fillCircle(cx-2,cy,24,C_DGREEN);
      // Banda verde deschis
      tft.fillCircle(cx-2,cy,22,C_GREEN);
      // Miez rosu
      tft.fillCircle(cx-2,cy,19,C_RED);
      // Taie jumatatea dreapta — linie verticala de separatie
      tft.fillRect(cx-2,cy-26,30,52,C_BG);
      tft.drawFastVLine(cx-2,cy-24,48,C_DGREEN);
      // Seminte in miezul rosu (stanga de linie)
      tft.fillCircle(cx-10,cy-8,2,C_DGREY);
      tft.fillCircle(cx-16,cy+2,2,C_DGREY);
      tft.fillCircle(cx-8,cy+10,2,C_DGREY);
      tft.fillCircle(cx-18,cy-8,2,C_DGREY); break;
    case 8:{
      // Strugure realist: coditsa sus, ciorchine piramidal cu baza jos
      // Coditsa subtire sus
      tft.fillRect(cx-1,cy-30,3,10,C_DGREEN);
      // Frunzulita mica laterala
      tft.fillCircle(cx+5,cy-28,4,C_GREEN);
      tft.fillCircle(cx-5,cy-29,3,C_DGREEN);

      // Boabe mov inchis cu highlight — piramida: 1 sus, 2, 3, 4 jos
      // Rand 1 — 1 bob (varf)
      tft.fillCircle(cx,      cy-20, 9, 0x4008);
      tft.fillCircle(cx,      cy-20, 7, C_PURPLE);
      tft.fillCircle(cx-3,    cy-23, 3, C_LPURP);

      // Rand 2 — 2 boabe
      tft.fillCircle(cx-10,   cy-6,  9, 0x4008);
      tft.fillCircle(cx-10,   cy-6,  7, C_PURPLE);
      tft.fillCircle(cx-13,   cy-9,  3, C_LPURP);
      tft.fillCircle(cx+10,   cy-6,  9, 0x4008);
      tft.fillCircle(cx+10,   cy-6,  7, C_PURPLE);
      tft.fillCircle(cx+7,    cy-9,  3, C_LPURP);

      // Rand 3 — 3 boabe
      tft.fillCircle(cx-20,   cy+8,  9, 0x4008);
      tft.fillCircle(cx-20,   cy+8,  7, C_PURPLE);
      tft.fillCircle(cx-23,   cy+5,  3, C_LPURP);
      tft.fillCircle(cx,      cy+8,  9, 0x4008);
      tft.fillCircle(cx,      cy+8,  7, C_PURPLE);
      tft.fillCircle(cx-3,    cy+5,  3, C_LPURP);
      tft.fillCircle(cx+20,   cy+8,  9, 0x4008);
      tft.fillCircle(cx+20,   cy+8,  7, C_PURPLE);
      tft.fillCircle(cx+17,   cy+5,  3, C_LPURP);

      // Rand 4 — 2 boabe jos (baza mai lata)
      tft.fillCircle(cx-10,   cy+22, 9, 0x4008);
      tft.fillCircle(cx-10,   cy+22, 7, C_PURPLE);
      tft.fillCircle(cx-13,   cy+19, 3, C_LPURP);
      tft.fillCircle(cx+10,   cy+22, 9, 0x4008);
      tft.fillCircle(cx+10,   cy+22, 7, C_PURPLE);
      tft.fillCircle(cx+7,    cy+19, 3, C_LPURP);
      break;}
    case 9:
      // Corp pruna — cerc mare mov inchis
      tft.fillCircle(cx,cy+2,22,0x4008);
      tft.fillCircle(cx,cy+2,20,C_PURPLE);
      // Santulet vertical caracteristic
      tft.fillRect(cx-1,cy-18,3,42,0x4008);
      // Highlight alb-albastru
      tft.fillCircle(cx-9,cy-8,6,C_LPURP);
      tft.fillCircle(cx-9,cy-8,3,0xDEFF);
      // Coditsa verde
      tft.fillRect(cx-1,cy-20,3,7,C_DGREEN);
      tft.fillCircle(cx+5,cy-22,4,C_GREEN);
      tft.fillCircle(cx-5,cy-23,3,C_DGREEN); break;
  }
}

void doSpin(){
  spin=true; lost=false; jack=false; stopReq=false;
  credit-=bet; drawHdr();
  int fr[3]; for(int i=0;i<3;i++) fr[i]=wrand();
  for(int i=0;i<3;i++) drawReel(i,C_BLUE);
  for(int f=0;f<16;f++){
    // verifica buton stop
    if(!stopReq && !digitalRead(BTN_SPIN)) stopReq=true;
    if(stopReq){
      // skip: afiseaza 2 frame-uri rapide si iese
      for(int i=0;i<3;i++){ reels[i]=wrand(); drawReel(i,C_BLUE); }
      tone(BUZZER,1400,15); delay(40);
      for(int i=0;i<3;i++){ reels[i]=fr[i]; drawReel(i,C_BORDER); }
      tone(BUZZER,1800,20); delay(40);
      break;
    }
    for(int i=0;i<3;i++){
      int st=9+i*2;
      reels[i]=(f<st)?wrand():fr[i];
      drawReel(i,(f<st)?C_BLUE:C_BORDER);
    }
    if(f%2==0) tone(BUZZER,900+f*22,8);
    delay((f<6)?22:(f<11)?36:55);
  }
  for(int i=0;i<3;i++){reels[i]=fr[i]; drawReel(i,C_BORDER);}
  chkWin(); spin=false; stopReq=false;
}

void chkWin(){
  lastWin=0; jack=false; s7any=false; gMode=false;
  int r0=reels[0], r1=reels[1], r2=reels[2];
  bool triple=(r0==r1&&r1==r2);
  bool pair=(r0==r1);
  if(triple){
    // Triple: plata dubla fata de pereche
    lastWin=bet*PAY[r0]*2; gMode=true;
    if(r0==1){jack=true; flashW(true); mel(MJF,MJD,7);}
    else{flashW(false); mel(MWF,MWD,4);}
  } else if(pair){
    lastWin=bet*PAY[r0]; gMode=true;
    if(r0==1){jack=true; flashW(true); mel(MJF,MJD,7);}
    else{flashW(false); mel(MWF,MWD,4);}
  } else if(r0==1){
    lastWin=bet*P7ANY; s7any=true; gMode=true;
    flashW(false); mel(MWF,MWD,4);
  } else {
    mel(MLF,MLD,3);
  }
  doubles=0; drawUI();
}

void flashW(bool j){
  uint16_t c=j?C_GOLD:C_GREEN;
  bool triple=(reels[0]==reels[1]&&reels[1]==reels[2]);
  for(int f=0;f<6;f++){
    drawReel(0,(f%2==0)?c:C_BORDER);
    drawReel(1,(f%2==0)?c:C_BORDER);
    drawReel(2,triple?(f%2==0?c:C_BORDER):C_DGREY);
    delay(70);
  }
  drawReel(0,c); drawReel(1,c);
  drawReel(2,triple?c:C_DGREY);
}

// ── Deseneaza o carte de joc ──────────────────────────────────
void drawCard(int x, int y, int w, int h, bool red, bool faceDown){
  tft.fillRect(x,y,w,h,C_WHITE);
  tft.drawRect(x,y,w,h,C_DGREY);
  tft.drawRect(x+1,y+1,w-2,h-2,C_DGREY);
  if(faceDown){
    // Dos — pattern geometric albastru inchis
    tft.fillRect(x+3,y+3,w-6,h-6,C_BLUE);
    for(int dy=y+6;dy<y+h-6;dy+=7)
      tft.drawFastHLine(x+4,dy,w-8,C_LBLUE);
    for(int dx=x+6;dx<x+w-6;dx+=7)
      tft.drawFastVLine(dx,y+4,h-8,C_LBLUE);
    return;
  }
  uint16_t col = red ? C_RED : C_BG;
  int cx=x+w/2, cy=y+h/2;
  // Simbol mare central
  tft.setTextSize(4); tft.setTextColor(col);
  if(red){
    tft.setCursor(cx-14,cy-22); tft.print(F("<3")); // inima
  } else {
    tft.setCursor(cx-14,cy-22); tft.print(F("**")); // pica
  }
  // Colturi
  tft.setTextSize(1); tft.setTextColor(col);
  tft.setCursor(x+4,y+4);
  tft.print(red?F("R"):F("N"));
  tft.setCursor(x+w-10,y+h-14);
  tft.print(red?F("R"):F("N"));
}

// ── Deseneaza istoricul rundelor (cartonase mici sus) ─────────
// Stocheaza culoarea cartii pentru fiecare runda: 0=necunoscut, 1=rosu, 2=negru

void drawGambleHistory(){
  int cw=26, ch=36, gap=6;
  int total=4*cw+3*gap;
  int sx=(SCR_W-total)/2, sy=46;
  for(int i=0;i<4;i++){
    int x=sx+i*(cw+gap);
    if(i<doubles){
      // Runda castigata — cartonas cu culoarea cartii
      bool wasRed=(gHistory[i]==1);
      tft.fillRect(x,sy,cw,ch,wasRed?C_RED:C_BG);
      tft.drawRect(x,sy,cw,ch,C_GOLD);
      tft.drawRect(x+1,sy+1,cw-2,ch-2,C_GOLD2);
      tft.setTextColor(C_WHITE); tft.setTextSize(1);
      tft.setCursor(x+4,sy+6);  tft.print(wasRed?F("R"):F("N"));
      tft.setCursor(x+4,sy+18); tft.print(F("W"));
    } else if(gHistory[i]==0){
      // Slot gol — cartonas gri
      tft.fillRect(x,sy,cw,ch,C_PANEL);
      tft.drawRect(x,sy,cw,ch,C_DGREY);
      tft.setTextColor(C_DGREY); tft.setTextSize(2);
      tft.setCursor(x+8,sy+10); tft.print(i+1);
    }
  }
}

// ── Ecranul principal de gamble (carta cu fata in jos) ────────
void drawGambleScreen(){
  tft.fillScreen(C_BG);

  // Header
  tft.fillRect(0,0,SCR_W,44,C_PANEL);
  tft.drawFastHLine(0,44,SCR_W,C_BORDER);
  tft.setTextColor(C_GOLD); tft.setTextSize(1);
  tft.setCursor(8,6); tft.print(F("DUBLAJ"));
  tft.setTextColor(C_CYAN); tft.setTextSize(2);
  char b[16]; sprintf(b,"WIN:%d",lastWin);
  tft.setCursor(8,20); tft.print(b);
  tft.setTextColor(C_GOLD); tft.setTextSize(1);
  tft.setCursor(210,6); tft.print(F("DUBLU:"));
  tft.setTextColor(C_GREEN); tft.setTextSize(2);
  sprintf(b,"%d",lastWin*2);
  tft.setCursor(210,20); tft.print(b);

  // Istoricul cartonaselor
  drawGambleHistory();

  // Carta centrala cu fata in jos
  int cw=80,ch=110,cx=(SCR_W-cw)/2,cy=88;
  drawCard(cx,cy,cw,ch,false,true);

  // Linie separator
  tft.drawFastHLine(20,186,SCR_W-40,C_BORDER);

  // Instructiune colectare
  cprint("BET = COLECTEAZA",190,C_DGREY,1);

  // Butoane NEGRU si ROSU
  int bw=100,bh=36,by=200,gap2=20;
  int bx0=(SCR_W-2*bw-gap2)/2, bx1=bx0+bw+gap2;

  tft.fillRect(bx0,by,bw,bh,C_BG);
  tft.drawRect(bx0,by,bw,bh,C_WHITE);
  tft.setTextColor(C_WHITE); tft.setTextSize(1);
  tft.setCursor(bx0+12,by+8);  tft.print(F("NEGRU"));
  tft.setCursor(bx0+10,by+20); tft.print(F("[SPIN]"));

  tft.fillRect(bx1,by,bw,bh,C_BG);
  tft.drawRect(bx1,by,bw,bh,C_RED);
  tft.setTextColor(C_RED); tft.setTextSize(1);
  tft.setCursor(bx1+18,by+8);  tft.print(F("ROSU"));
  tft.setCursor(bx1+10,by+20); tft.print(F("[DOUBLE]"));
}

// ── Arata rezultatul dupa alegere ─────────────────────────────
void showGambleResult(bool cardRed, bool won){
  // Redeseneaza ecranul cu carta intoarsa (fata in sus)
  tft.fillScreen(C_BG);
  tft.fillRect(0,0,SCR_W,44,C_PANEL);
  tft.drawFastHLine(0,44,SCR_W,C_BORDER);
  tft.setTextColor(C_GOLD); tft.setTextSize(1);
  tft.setCursor(8,6); tft.print(F("REZULTAT"));
  tft.setTextColor(won?C_GREEN:C_RED); tft.setTextSize(2);
  char b[20];
  sprintf(b,won?"WIN: %d":"PIERDUT",lastWin);
  tft.setCursor(8,20); tft.print(b);

  drawGambleHistory();

  // Carta dezvaluita
  int cw=80,ch=110,cx=(SCR_W-cw)/2,cy=88;
  drawCard(cx,cy,cw,ch,cardRed,false);

  // Mesaj castig/pierdut
  if(won){
    cprint("AI GHICIT!",188,C_GREEN,1);
  } else {
    cprint("GRESIT!",188,C_RED,1);
  }
}

// ── Intra in modul gamble ─────────────────────────────────────
void enterGamble(){
  if(doubles>=4) return;
  inGamble=true;
  tone(BUZZER,700,40); delay(80); tone(BUZZER,900,40);
  drawGambleScreen();
}

// ── Joaca o runda — chooseRed=alegerea jucatorului ───────────
void doGamble(bool chooseRed){
  // Genereaza culoarea cartii (55% sansa de castig)
  bool cardRed = chooseRed ? (random(100)<55) : (random(100)>=55);
  bool won = (chooseRed==cardRed);

  // Inregistreaza in istoric
  gHistory[doubles] = cardRed ? 1 : 2;

  // Arata rezultatul (carta intoarsa)
  showGambleResult(cardRed, won);
  delay(900);

  if(won){
    lastWin*=2; doubles++;
    tone(BUZZER,1600,60); delay(80); tone(BUZZER,2000,80); delay(100); tone(BUZZER,2400,150);
    delay(300);
    if(doubles>=4){
      exitGamble(true);
    } else {
      drawGambleScreen(); // noua runda — carta cu fata in jos
    }
  } else {
    mel(MLF,MLD,3);
    delay(600);
    lastWin=0; lost=true; gMode=false;
    inGamble=false;
    for(int i=0;i<4;i++) gHistory[i]=0;
    drawUI();
    delay(600); lost=false; drawStatus();
  }
}

// ── Iesi din gamble cu colectare ─────────────────────────────
void exitGamble(bool autoExit){
  inGamble=false;
  credit+=lastWin;
  if(autoExit){
    tft.fillScreen(C_BG);
    cprint("MAX DOUBLES!",90,C_GOLD,2);
    char b[20]; sprintf(b,"TOTAL: %d",lastWin);
    cprint(b,120,C_GREEN,2);
    mel(MWF,MWD,4); delay(1200);
  }
  for(int i=0;i<4;i++) gHistory[i]=0;
  resetW(); drawUI();
}

void resetW(){
  lastWin=0; doubles=0; gMode=false; lost=false; jack=false; s7any=false;
  inGamble=false; for(int i=0;i<4;i++) gHistory[i]=0;
}

const char* sname(int s){
  switch(s){
    case 0: return "CHERRY"; case 1: return "SEVEN";
    case 2: return "BELL";   case 3: return "DIAMOND";
    case 4: return "STAR";   case 5: return "LEMON";
    case 6: return "BAR";    case 7: return "PEPENE";
    case 8: return "STRUGURE"; case 9: return "PRUNA";
  } return "?";
}

int wrand(){
  int r=random(100),c=0;
  for(int i=0;i<NS;i++){c+=SW[i];if(r<c)return i;}
  return 0;
}

void mel(const int*f,const int*d,int n){
  for(int i=0;i<n;i++){tone(BUZZER,f[i],d[i]);delay(d[i]+12);}
  noTone(BUZZER);
}

void cprint(const char*t,int y,uint16_t c,uint8_t sz){
  tft.setTextSize(sz); tft.setTextColor(c);
  tft.setCursor((SCR_W-(int)strlen(t)*6*sz)/2,y); tft.print(t);
}

void smsg(const char*l1,const char*l2,uint16_t c){
  int y=STY+8; tft.fillRect(0,STY,SCR_W,BTNY-STY-14,C_BG);
  cprint(l1,y,c,2); cprint(l2,y+22,C_DGREY,1);
}
