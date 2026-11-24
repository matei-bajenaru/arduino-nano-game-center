#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <EEPROM.h>

// ===== Configurație Pini =====
#define TFT_CS 10
#define TFT_DC  8
#define TFT_RST 9
#define B_LEFT  2
#define B_DOWN  3
#define B_RIGHT 4
#define B_UP    7
#define BUZ     6

// ===== Paletă =====
#define C_BG_LIGHT 0xA7E7
#define C_BG_DARK  0x9DE6
#define C_HEADER   0x4C68
#define C_BORDER   0x3BE5
#define C_SNAKE_H  0x23DF    // Albastru cap
#define C_SNAKE_B  0x34BF    // Albastru corp
#define C_SNAKE_HL 0x5DFF    // Highlight lucios (constant stânga-sus)
#define C_SNAKE_SH 0x1170    // Umbră subtilă (constant dreapta-jos)
#define C_SNAKE_M  0x1193    // Interior gură
#define C_APPLE    0xF1A4
#define C_LEAF     0x2D44
#define C_GOLD     0xFEE0
#define C_WHITE    0xFFFF
#define C_BLACK    0x0000

// ===== Dimensiuni grilă =====
#define SC      20
#define SCO     15
#define SRO     10
#define SOX     10
#define SOY     35
#define SW      320
#define SH      240
#define SMX     150
#define EE_ADDR 0

// ===== Randare =====
#define FRAME_MS 16   // ~60 fps

struct Pt { int8_t x, y; };
Pt sb[SMX];   // poziții curente ale șarpelui (grilă, întregi) - SURSA UNICĂ de adevăr

// Buffer atomic 20x20 pixeli (800 bytes SRAM) -> zero flickering
uint16_t spriteBuf[SC * SC];

// Tabelă rapidă în PROGMEM pentru interpolarea curbelor din coadă fără împărțiri
const uint8_t INV_DENOM[40] PROGMEM = {
  0, 255, 128, 85, 64, 51, 43, 37, 32, 28,
  26, 23, 21, 20, 18, 17, 16, 15, 14, 13,
  13, 12, 12, 11, 11, 10, 10, 9, 9, 9,
  9, 8, 8, 8, 8, 7, 7, 7, 7, 7
};

int sl = 0, sd = 1, snd = 1, ss = 0, shi = 0;
Pt sf2;
bool sa = false, sgo = false, spau = false, spx = false, fpS = false;
unsigned long slm = 0, fpt = 0, ppb = 0, lastFrame = 0;
uint16_t ssp = 220;
uint8_t ftype = 0, fbon = 10;

// ---- Input rapid pe front căzător independent pentru fiecare buton ----
const uint8_t BTN_PINS[4] = { B_UP, B_RIGHT, B_DOWN, B_LEFT };
bool btnStable[4] = { HIGH, HIGH, HIGH, HIGH };
unsigned long btnLastEdge[4] = { 0, 0, 0, 0 };

// ---- Stare restart Game Over ----
unsigned long gameOverAt = 0;
bool waitUpRelease = false;

// ---- Buffer input pentru viraje rapide consecutive ----
int8_t dirQueue[3];
uint8_t dirQueueLen = 0;

// ---- Sunet non-blocant la mâncat ----
uint8_t eatSoundStep = 0;
unsigned long eatSoundTimer = 0;

// ---- Stare animație (per pas curent, progres exact 0..SC pixeli) ----
Pt animHeadFrom;             // celula din care avansează capul în acest pas
Pt animTailFrom;             // celula din care se retrage coada în acest pas
bool tailMoving = false;     // false când șarpele tocmai a crescut (coada stă pe loc)
int16_t prevStepAmt = 0;     // câți pixeli (0..SC) au fost deja randați în pasul curent

// ---- Stare impact la perete ----
bool sImpact = false;
unsigned long impactStart = 0;
Pt impactBase;
const uint16_t IMPACT_MS = 130;

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

const int MLF[] = {450, 350, 250}, MLD[] = {70, 90, 150};

inline int16_t floorDiv20(int16_t v) {
  return (v >= 0) ? (v / SC) : ((v - (SC - 1)) / SC);
}

inline uint16_t getTileColor(int8_t x, int8_t y) {
  if (x < 0 || x >= SCO || y < 0 || y >= SRO) return C_BG_DARK;
  return ((x + y) & 1) ? C_BG_DARK : C_BG_LIGHT;
}

inline bool isOppositeDir(int8_t a, int8_t b) {
  return ((a == 0 && b == 2) || (a == 2 && b == 0) ||
          (a == 1 && b == 3) || (a == 3 && b == 1));
}

void playTone(int freq, int duration) {
  tone(BUZ, freq, duration);
  delay(duration + 5);
  noTone(BUZ);
}

inline void triggerEatSound() {
  tone(BUZ, 950, 20);
  eatSoundStep = 1;
  eatSoundTimer = millis();
}

inline void updateSound(unsigned long now) {
  if (eatSoundStep == 1 && (now - eatSoundTimer >= 22)) {
    tone(BUZ, 1550, 25);
    eatSoundStep = 0;
  }
}

void centerText(const char* t, int y, uint16_t c, uint8_t sz) {
  tft.setTextSize(sz);
  tft.setTextColor(c);
  tft.setCursor((SW - (int)strlen(t) * 6 * sz) / 2, y);
  tft.print(t);
}

// Trimite atomic o zonă din spriteBuf pe display
inline void blitRect(int16_t px, int16_t py, int16_t w, int16_t h) {
  if (w <= 0 || h <= 0) return;
  tft.startWrite();
  tft.setAddrWindow(px, py, w, h);
  tft.writePixels(spriteBuf, (uint32_t)w * (uint32_t)h);
  tft.endWrite();
}

inline void blitTile(int16_t px, int16_t py) {
  blitRect(px, py, SC, SC);
}

// ===== Coadă multi-segment ("rezoluție" mare: până la 6 segmente în funcție de lungime) =====
inline int16_t getDynamicTailSpan() {
  int16_t span = 1;
  if (sl >= 14) span = 6;
  else if (sl >= 10) span = 5;
  else if (sl >= 7) span = 4;
  else if (sl >= 5) span = 3;
  else if (sl >= 4) span = 2;

  if (span > sl - 2) span = sl - 2; // păstrăm cel puțin capul (0) și gâtul (1) pline
  if (span < 1) span = 1;
  return span;
}

// ===== Pixel din celulă de corp cu iluminare constantă STÂNGA-SUS -> DREAPTA-JOS =====
uint16_t evalBodyCellPixel(int16_t idx, int16_t lx, int16_t ly) {
  Pt p = sb[idx];
  uint16_t bg = getTileColor(p.x, p.y);
  if (idx < 1 || idx >= sl) return bg;

  Pt ahead = sb[idx - 1];
  bool hasBehind = (idx + 1 < sl);
  Pt behind = hasBehind ? sb[idx + 1] : p;

  bool connUp    = (ahead.y < p.y) || (hasBehind && behind.y < p.y);
  bool connDown  = (ahead.y > p.y) || (hasBehind && behind.y > p.y);
  bool connLeft  = (ahead.x < p.x) || (hasBehind && behind.x < p.x);
  bool connRight = (ahead.x > p.x) || (hasBehind && behind.x > p.x);

  // 1. Segment orizontal drept: reflexie sus (y=3..5), umbră jos (y=19)
  if ((connLeft || connRight) && !connUp && !connDown) {
    if (ly >= 3 && ly <= 5) return C_SNAKE_HL;
    if (ly == SC - 1) return C_SNAKE_SH;
    return C_SNAKE_B;
  }

  // 2. Segment vertical drept: reflexie stânga (x=3..5), umbră dreapta (x=19)
  if ((connUp || connDown) && !connLeft && !connRight) {
    if (lx >= 3 && lx <= 5) return C_SNAKE_HL;
    if (lx == SC - 1) return C_SNAKE_SH;
    return C_SNAKE_B;
  }

  // 3. Curbă la 90 de grade (toate cele 4 orientări aliniate la nivel de pixel)
  if ((connLeft || connRight) && (connUp || connDown)) {
    const int16_t rOut = 9;

    // Cazul A: DREAPTA + JOS (colț exterior STÂNGA-SUS decupat)
    if (connRight && connDown) {
      if (lx < rOut && ly < rOut) {
        int16_t dx = rOut - lx;
        int16_t dy = rOut - ly;
        int16_t d2 = dx * dx + dy * dy;
        if (d2 > rOut * rOut) return bg;
        if (d2 >= 16 && d2 <= 36) return C_SNAKE_HL;
      } else if (lx >= rOut && ly >= 3 && ly <= 5) {
        return C_SNAKE_HL;
      } else if (ly >= rOut && lx >= 3 && lx <= 5) {
        return C_SNAKE_HL;
      }
      if (ly == SC - 1 && lx == SC - 1) return C_SNAKE_SH;
      return C_SNAKE_B;
    }

    // Cazul B: STÂNGA + SUS (colț exterior DREAPTA-JOS decupat)
    if (connLeft && connUp) {
      if (lx >= SC - rOut && ly >= SC - rOut) {
        int16_t dx = lx - (SC - 1 - rOut);
        int16_t dy = ly - (SC - 1 - rOut);
        int16_t d2 = dx * dx + dy * dy;
        if (d2 > rOut * rOut) return bg;
        if (d2 >= (rOut - 1) * (rOut - 1)) return C_SNAKE_SH;
      }
      if ((ly == SC - 1 && lx < SC - rOut) || (lx == SC - 1 && ly < SC - rOut)) {
        return C_SNAKE_SH;
      }
      if ((ly >= 3 && ly <= 5 && lx <= 5) || (lx >= 3 && lx <= 5 && ly <= 5)) {
        return C_SNAKE_HL;
      }
      return C_SNAKE_B;
    }

    // Cazul C: STÂNGA + JOS (colț exterior DREAPTA-SUS decupat)
    if (connLeft && connDown) {
      if (lx >= SC - rOut && ly < rOut) {
        int16_t dx = lx - (SC - 1 - rOut);
        int16_t dy = rOut - ly;
        int16_t d2 = dx * dx + dy * dy;
        if (d2 > rOut * rOut) return bg;
        if (d2 >= (rOut - 1) * (rOut - 1) && dx >= dy) return C_SNAKE_SH;
      }
      if ((lx == SC - 1 && ly >= rOut) || (lx == 0 && ly == SC - 1)) {
        return C_SNAKE_SH;
      }
      if ((ly >= 3 && ly <= 5 && lx <= 5) || (lx >= 3 && lx <= 5 && ly >= 3)) {
        return C_SNAKE_HL;
      }
      return C_SNAKE_B;
    }

    // Cazul D: DREAPTA + SUS (colț exterior STÂNGA-JOS decupat)
    if (connRight && connUp) {
      if (lx < rOut && ly >= SC - rOut) {
        int16_t dx = rOut - lx;
        int16_t dy = ly - (SC - 1 - rOut);
        int16_t d2 = dx * dx + dy * dy;
        if (d2 > rOut * rOut) return bg;
        if (d2 >= (rOut - 1) * (rOut - 1) && dy >= dx) return C_SNAKE_SH;
      }
      if ((ly == SC - 1 && lx >= rOut) || (lx == SC - 1 && ly == 0)) {
        return C_SNAKE_SH;
      }
      if ((lx >= 3 && lx <= 5 && ly <= 5) || (ly >= 3 && ly <= 5 && lx >= 3)) {
        return C_SNAKE_HL;
      }
      return C_SNAKE_B;
    }
  }

  return C_SNAKE_B;
}

void drawBodyCell(int16_t idx) {
  if (idx < 1 || idx >= sl) return;
  Pt p = sb[idx];
  uint16_t *dst = spriteBuf;
  for (int16_t ly = 0; ly < SC; ly++) {
    for (int16_t lx = 0; lx < SC; lx++) {
      *dst++ = evalBodyCellPixel(idx, lx, ly);
    }
  }
  blitTile(SOX + p.x * SC, SOY + p.y * SC);
}

// ===== Reacoperă instant fâșia din spatele capului =====
void uncoverNeckStrip(int16_t fromAmt, int16_t toAmt) {
  if (toAmt <= fromAmt || sl < 2) return;
  int16_t hdx = sb[0].x - animHeadFrom.x;
  int16_t hdy = sb[0].y - animHeadFrom.y;
  int16_t ax = SOX + animHeadFrom.x * SC;
  int16_t ay = SOY + animHeadFrom.y * SC;
  int16_t delta = toAmt - fromAmt;

  uint16_t *dst = spriteBuf;

  if (hdx > 0) {
    for (int16_t ly = 0; ly < SC; ly++) {
      for (int16_t lx = fromAmt; lx < toAmt; lx++) {
        *dst++ = evalBodyCellPixel(1, lx, ly);
      }
    }
    blitRect(ax + fromAmt, ay, delta, SC);
  } else if (hdx < 0) {
    int16_t xStart = SC - toAmt;
    int16_t xEnd   = SC - fromAmt;
    for (int16_t ly = 0; ly < SC; ly++) {
      for (int16_t lx = xStart; lx < xEnd; lx++) {
        *dst++ = evalBodyCellPixel(1, lx, ly);
      }
    }
    blitRect(ax + xStart, ay, delta, SC);
  } else if (hdy > 0) {
    for (int16_t ly = fromAmt; ly < toAmt; ly++) {
      for (int16_t lx = 0; lx < SC; lx++) {
        *dst++ = evalBodyCellPixel(1, lx, ly);
      }
    }
    blitRect(ax, ay + fromAmt, SC, delta);
  } else if (hdy < 0) {
    int16_t yStart = SC - toAmt;
    int16_t yEnd   = SC - fromAmt;
    for (int16_t ly = yStart; ly < yEnd; ly++) {
      for (int16_t lx = 0; lx < SC; lx++) {
        *dst++ = evalBodyCellPixel(1, lx, ly);
      }
    }
    blitRect(ax, ay + yStart, SC, delta);
  }
}

// ===== Coadă multi-segment de rezoluție înaltă =====
void drawDynamicTailCell(Pt cell, Pt towardBody, Pt awayFromBody, bool isMovingTipCell,
                         int16_t distBase, int16_t scaleFP, int16_t tipR, int16_t stepAmt) {
  if (cell.x < 0 || cell.x >= SCO || cell.y < 0 || cell.y >= SRO) return;

  uint16_t bg = getTileColor(cell.x, cell.y);
  int8_t inDx = towardBody.x - cell.x;
  int8_t inDy = towardBody.y - cell.y;
  int8_t outDx = awayFromBody.x - cell.x;
  int8_t outDy = awayFromBody.y - cell.y;

  bool isCorner = ((inDx != 0 && outDy != 0) || (inDy != 0 && outDx != 0));
  bool isHoriz  = (inDx != 0 || outDx != 0) && !isCorner;

  int16_t tipR2 = tipR * tipR;
  uint16_t *dst = spriteBuf;

  if (!isCorner) {
    uint8_t halfWArr[SC];
    uint8_t hlPosArr[SC];
    int16_t distArr[SC];

    for (int16_t a = 0; a < SC; a++) {
      int16_t d = isMovingTipCell ? (a - stepAmt) : (distBase + a);
      distArr[a] = d;
      if (d < 0) {
        halfWArr[a] = 0;
        hlPosArr[a] = 0;
      } else {
        int16_t hw = tipR + ((d * scaleFP) >> 8);
        if (hw > 10) hw = 10;
        halfWArr[a] = (uint8_t)hw;
        hlPosArr[a] = (uint8_t)(10 - ((hw * 154) >> 8));
      }
    }

    for (int16_t ly = 0; ly < SC; ly++) {
      for (int16_t lx = 0; lx < SC; lx++) {
        int16_t along, perp;
        if (inDx > 0 || outDx < 0)      { along = lx;            perp = abs(ly - 10); }
        else if (inDx < 0 || outDx > 0) { along = (SC - 1) - lx; perp = abs(ly - 10); }
        else if (inDy > 0 || outDy < 0) { along = ly;            perp = abs(lx - 10); }
        else                            { along = (SC - 1) - ly; perp = abs(lx - 10); }

        int16_t d = distArr[along];
        if (d < 0) { *dst++ = bg; continue; }

        int16_t hw = halfWArr[along];
        if (d < tipR) {
          int16_t dt = tipR - d;
          if (dt * dt + perp * perp > tipR2) { *dst++ = bg; continue; }
        } else if (perp >= hw) {
          *dst++ = bg; continue;
        }

        uint8_t hlp = hlPosArr[along];
        if (isHoriz) {
          if (ly >= hlp && ly <= hlp + 2 && d > tipR) *dst++ = C_SNAKE_HL;
          else if (ly == 10 + hw - 1) *dst++ = C_SNAKE_SH;
          else *dst++ = C_SNAKE_B;
        } else {
          if (lx >= hlp && lx <= hlp + 2 && d > tipR) *dst++ = C_SNAKE_HL;
          else if (lx == 10 + hw - 1) *dst++ = C_SNAKE_SH;
          else *dst++ = C_SNAKE_B;
        }
      }
    }
  } else {
    int16_t cx = (inDx > 0 || outDx > 0) ? (SC - 1) : 0;
    int16_t cy = (inDy > 0 || outDy > 0) ? (SC - 1) : 0;

    for (int16_t ly = 0; ly < SC; ly++) {
      int16_t dy = abs(ly - cy);
      for (int16_t lx = 0; lx < SC; lx++) {
        int16_t dx = abs(lx - cx);

        int16_t maxD = (dx > dy) ? dx : dy;
        int16_t minD = (dx < dy) ? dx : dy;
        int16_t approxR = maxD + ((minD * 3) >> 3);
        int16_t perp = abs(approxR - 10);

        int16_t exitCoord  = (outDx != 0) ? dx : dy;
        int16_t entryCoord = (inDx  != 0) ? dx : dy;
        int16_t denom = exitCoord + entryCoord;
        uint8_t inv = pgm_read_byte(&INV_DENOM[denom]);
        int16_t along = (denom > 0) ? (((exitCoord * 19) * inv) >> 8) : 10;

        int16_t d = distBase + along;
        int16_t hw = tipR + ((d * scaleFP) >> 8);
        if (hw > 10) hw = 10;

        if (d < tipR) {
          int16_t dt = tipR - d;
          if (dt * dt + perp * perp > tipR2) { *dst++ = bg; continue; }
        } else if (perp >= hw) {
          *dst++ = bg; continue;
        }

        int16_t signedR = ((cx == 0) ? lx : (19 - lx)) + ((cy == 0) ? ly : (19 - ly));
        bool isLitSide = (cx == 0 && cy == 0) ? (approxR < 10) :
                         (cx != 0 && cy != 0) ? (approxR > 10) : (signedR < 19);

        int16_t hlOffset = (hw * 154) >> 8;
        if (isLitSide && perp >= hlOffset - 2 && perp <= hlOffset && d > tipR) {
          *dst++ = C_SNAKE_HL;
        } else if (!isLitSide && perp == hw - 1) {
          *dst++ = C_SNAKE_SH;
        } else {
          *dst++ = C_SNAKE_B;
        }
      }
    }
  }

  blitTile(SOX + cell.x * SC, SOY + cell.y * SC);
}

void updateDynamicTail(int16_t stepAmt, bool fullTailRefresh) {
  if (sl < 2) return;

  int16_t span = getDynamicTailSpan();
  int16_t firstTailIdx = sl - span;
  if (firstTailIdx < 1) firstTailIdx = 1;

  int16_t rem = tailMoving ? (SC - stepAmt) : 0;
  int16_t totalTailPx = span * SC + rem;
  if (totalTailPx < SC) totalTailPx = SC;

  // Vârful devine din ce în ce mai fin pe măsură ce coada câștigă mai multe segmente (5px -> 2px)
  int16_t tipR = (span >= 5) ? 2 : ((span >= 3) ? 3 : ((span == 2) ? 4 : 5));
  int16_t scaleFP = ((10 - tipR) << 8) / totalTailPx;

  // 1. Celula veche din care iese vârful cozii
  if (tailMoving && (animTailFrom.x != sb[sl - 1].x || animTailFrom.y != sb[sl - 1].y)) {
    if (stepAmt < SC) {
      drawDynamicTailCell(animTailFrom, sb[sl - 1], animTailFrom, true,
                          0, scaleFP, tipR, stepAmt);
    } else {
      tft.fillRect(SOX + animTailFrom.x * SC, SOY + animTailFrom.y * SC,
                   SC, SC, getTileColor(animTailFrom.x, animTailFrom.y));
    }
  }

  // 2. La cadre intermediare actualizăm ultimele 2 celule active; la final de pas toate cele `span` celule
  int16_t startIdx = fullTailRefresh ? firstTailIdx : max((int16_t)(sl - 2), firstTailIdx);
  for (int16_t i = sl - 1; i >= startIdx; i--) {
    int16_t cellsBehind = (sl - 1) - i;
    int16_t distBase = rem + cellsBehind * SC;
    Pt towardBody = sb[i - 1];
    Pt awayFromBody = (i + 1 < sl) ? sb[i + 1] : (tailMoving ? animTailFrom : sb[i]);

    drawDynamicTailCell(sb[i], towardBody, awayFromBody, false,
                        distBase, scaleFP, tipR, stepAmt);
  }
}

// ===== Randare atomică a capului (cu mulare pe curba din sb[1] -> zero X / ieșire în contur) =====
void drawHeadSprite(int16_t px, int16_t py) {
  int16_t relX = px - SOX;
  int16_t relY = py - SOY;
  int8_t gx0 = (int8_t)floorDiv20(relX);
  int8_t gy0 = (int8_t)floorDiv20(relY);
  int16_t remX = relX - gx0 * SC;
  int16_t remY = relY - gy0 * SC;

  uint16_t *dst = spriteBuf;

  for (int16_t ly = 0; ly < SC; ly++) {
    int8_t gy = (ly + remY < SC) ? gy0 : (gy0 + 1);
    int16_t cellLy = (ly + remY < SC) ? (ly + remY) : (ly + remY - SC);

    for (int16_t lx = 0; lx < SC; lx++) {
      int8_t gx = (lx + remX < SC) ? gx0 : (gx0 + 1);
      int16_t cellLx = (lx + remX < SC) ? (lx + remX) : (lx + remX - SC);

      bool onNeckCell = (sl >= 2 && gx == sb[1].x && gy == sb[1].y);
      uint16_t tileBg = getTileColor(gx, gy);
      uint16_t neckBg = onNeckCell ? evalBodyCellPixel(1, cellLx, cellLy) : tileBg;

      int16_t u, v;
      if (sd == 1)      { u = lx;            v = ly; }
      else if (sd == 3) { u = (SC - 1) - lx; v = ly; }
      else if (sd == 2) { u = ly;            v = lx; }
      else              { u = (SC - 1) - ly; v = lx; }

      // Dacă spatele capului (u < 10) trece peste o celulă de curbă (sb[1]) al cărei colț exterior
      // este decupat (neckBg == tileBg), nu lăsăm spatele capului să iasă în afara curbei!
      if (onNeckCell && u < 10 && neckBg == tileBg) {
        *dst++ = tileBg;
        continue;
      }

      // Silueta capului (față rotunjită cu rază 8px)
      const int16_t rHead = 8;
      if (u >= SC - rHead) {
        int16_t du = u - (SC - 1 - rHead);
        if (v < rHead) {
          int16_t dv = rHead - v;
          if (du * du + dv * dv > rHead * rHead) { *dst++ = neckBg; continue; }
        } else if (v >= SC - rHead) {
          int16_t dv = v - (SC - 1 - rHead);
          if (du * du + dv * dv > rHead * rHead) { *dst++ = neckBg; continue; }
        }
      }

      uint16_t col = C_SNAKE_H;

      // Gură în față (u = 15..18, v = 6..13)
      if (u >= 15 && u <= 18 && v >= 6 && v <= 13) {
        if (!((u == 18) && (v == 6 || v == 13))) {
          col = C_SNAKE_M;
        }
      }

      // Ochi identici și simetrici pe toate cele 4 direcții
      int16_t duEye = u - 9;
      int16_t dvEye1 = v - 5;
      int16_t dvEye2 = v - 14;
      if ((duEye * duEye + dvEye1 * dvEye1 <= 12) ||
          (duEye * duEye + dvEye2 * dvEye2 <= 12)) {
        col = C_WHITE;
        int16_t duPup = u - 10;
        if ((duPup * duPup + dvEye1 * dvEye1 <= 3) ||
            (duPup * duPup + dvEye2 * dvEye2 <= 3)) {
          col = C_BLACK;
        }
      }

      *dst++ = col;
    }
  }

  blitTile(px, py);
}

// ===== Panel Status =====
void drawHeader() {
  tft.fillRect(0, 0, SW, SOY - 2, C_HEADER);
  tft.drawFastHLine(0, SOY - 2, SW, C_BORDER);

  tft.fillCircle(25, 16, 7, C_APPLE);
  tft.fillRoundRect(25, 8, 2, 4, 1, C_LEAF);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  char buf[8]; sprintf(buf, "%d", ss);
  tft.setCursor(42, 9); tft.print(buf);

  int tx = SW - 90;
  tft.fillRoundRect(tx, 9, 14, 10, 3, C_GOLD);
  tft.fillRect(tx + 5, 19, 4, 5, C_GOLD);
  tft.fillRect(tx + 2, 24, 10, 2, C_GOLD);
  sprintf(buf, "%d", shi);
  tft.setCursor(tx + 22, 9); tft.print(buf);
}

void updateHeaderScore() {
  char buf[8];
  tft.fillRect(42, 8, 56, 18, C_HEADER);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  sprintf(buf, "%d", ss);
  tft.setCursor(42, 9);
  tft.print(buf);

  if (ss > shi) {
    shi = ss;
    int tx = SW - 90;
    tft.fillRect(tx + 22, 8, 56, 18, C_HEADER);
    sprintf(buf, "%d", shi);
    tft.setCursor(tx + 22, 9);
    tft.print(buf);
  }
}

void drawFruit(bool pulse) {
  int px = SOX + sf2.x * SC;
  int py = SOY + sf2.y * SC;
  tft.fillRect(px, py, SC, SC, getTileColor(sf2.x, sf2.y));

  int cx = px + SC / 2;
  int cy = py + SC / 2;
  int r = pulse ? 8 : 7;

  if (ftype == 0) {
    tft.fillCircle(cx, cy + 1, r, C_APPLE);
    tft.fillCircle(cx - 2, cy - 2, 2, C_WHITE);
    tft.fillRoundRect(cx, py + 2, 3, 5, 1, C_LEAF);
  } else {
    tft.fillCircle(cx, cy, r, C_GOLD);
    tft.fillCircle(cx - 2, cy - 2, 2, C_WHITE);
  }
}

void generateFruit() {
  uint16_t occ[SRO];
  for (int8_t y = 0; y < SRO; y++) occ[y] = 0;
  for (int i = 0; i < sl; i++) {
    if (sb[i].y >= 0 && sb[i].y < SRO && sb[i].x >= 0 && sb[i].x < SCO) {
      occ[sb[i].y] |= (uint16_t)(1U << sb[i].x);
    }
  }

  int16_t freeCount = (SCO * SRO) - sl;
  if (freeCount <= 0) return;

  int16_t pick = (int16_t)random(freeCount);
  int16_t idx = 0;
  for (int8_t y = 0; y < SRO; y++) {
    uint16_t rowMask = occ[y];
    for (int8_t x = 0; x < SCO; x++) {
      if ((rowMask & (uint16_t)(1U << x)) == 0) {
        if (idx == pick) {
          sf2.x = x;
          sf2.y = y;
          ftype = (random(100) < 20) ? 1 : 0;
          fbon = (ftype == 1) ? 30 : 10;
          fpS = true;
          fpt = millis();
          drawFruit(true);
          return;
        }
        idx++;
      }
    }
  }
}

void drawField() {
  for (int y = 0; y < SRO; y++) {
    for (int x = 0; x < SCO; x++) {
      tft.fillRect(SOX + x * SC, SOY + y * SC, SC, SC, getTileColor(x, y));
    }
  }
  tft.drawRect(SOX - 1, SOY - 1, SCO * SC + 2, SRO * SC + 2, C_BORDER);
}

void redrawFullSnake() {
  int16_t span = getDynamicTailSpan();
  int16_t bodyEnd = sl - span;
  for (int16_t i = 1; i < bodyEnd; i++) {
    drawBodyCell(i);
  }
  updateDynamicTail(SC, true);
  drawHeadSprite(SOX + sb[0].x * SC, SOY + sb[0].y * SC);
}

void initGame() {
  sl = 3; sd = 1; snd = 1; dirQueueLen = 0; ss = 0; ssp = 220;
  sa = true; sgo = false; spau = false; spx = false;
  slm = 0; lastFrame = 0;
  sImpact = false; tailMoving = false; prevStepAmt = SC;
  eatSoundStep = 0; waitUpRelease = false;

  for (uint8_t b = 0; b < 4; b++) {
    btnStable[b] = digitalRead(BTN_PINS[b]);
    btnLastEdge[b] = 0;
  }

  for (int i = 0; i < sl; i++) { sb[i].x = 4 - i; sb[i].y = SRO / 2; }
  animHeadFrom = sb[0];
  animTailFrom = sb[sl - 1];

  tft.fillScreen(C_BG_DARK);
  drawHeader();
  drawField();
  redrawFullSnake();

  generateFruit();
  centerText("APASA O DIRECTIE", SOY + 75, C_HEADER, 2);
}

void gameOver() {
  sa = false;
  if (ss >= shi) { shi = ss; EEPROM.put(EE_ADDR, shi); }

  for (int i = 0; i < sl; i++) {
    tft.fillRect(SOX + sb[i].x * SC, SOY + sb[i].y * SC, SC, SC, getTileColor(sb[i].x, sb[i].y));
    if (i % 2 == 0) playTone(280 - (i * 12), 35);
  }

  delay(150);
  tft.fillRoundRect(30, 60, SW - 60, 125, 8, C_HEADER);
  tft.drawRoundRect(29, 59, SW - 58, 127, 8, C_GOLD);

  centerText("GAME OVER", 75, C_APPLE, 3);
  char buf[20]; sprintf(buf, "SCOR: %d", ss);
  centerText(buf, 115, C_WHITE, 2);
  centerText("APASA SUS PENTRU RESTART", 155, C_BG_LIGHT, 1);

  for (int i = 0; i < 3; i++) playTone(MLF[i], MLD[i]);

  gameOverAt = millis();
  waitUpRelease = (digitalRead(B_UP) == LOW);
}

// ===== Randare fluidă =====
void renderStepTo(int16_t targetAmt, bool stepFinished) {
  if (targetAmt < 0) targetAmt = 0;
  if (targetAmt > SC) targetAmt = SC;

  if (targetAmt > prevStepAmt) {
    uncoverNeckStrip(prevStepAmt, targetAmt);
  }

  if (tailMoving || stepFinished) {
    updateDynamicTail(targetAmt, stepFinished);
  }

  if (stepFinished && sl >= 2) {
    int16_t span = getDynamicTailSpan();
    int16_t outOfTailIdx = sl - span - 1;
    if (outOfTailIdx >= 2) {
      drawBodyCell(outOfTailIdx);
    }
  }

  int16_t hdx = sb[0].x - animHeadFrom.x;
  int16_t hdy = sb[0].y - animHeadFrom.y;
  int16_t px = SOX + animHeadFrom.x * SC + hdx * targetAmt;
  int16_t py = SOY + animHeadFrom.y * SC + hdy * targetAmt;
  drawHeadSprite(px, py);

  prevStepAmt = targetAmt;
}

void renderSnakeFrame() {
  unsigned long elapsed = millis() - slm;
  int16_t targetAmt = (ssp > 0) ? (int16_t)((elapsed * (unsigned long)SC) / (unsigned long)ssp) : SC;
  if (targetAmt > SC) targetAmt = SC;
  if (targetAmt != prevStepAmt) {
    renderStepTo(targetAmt, targetAmt == SC);
  }
}

// ===== Animație de impact la perete =====
void renderImpact() {
  unsigned long now = millis();
  unsigned long el = now - impactStart;
  if (el > IMPACT_MS) el = IMPACT_MS;

  int16_t offsetPx = (int16_t)((el * 8UL) / IMPACT_MS);
  int16_t dx = 0, dy = 0;
  if (sd == 0) dy = -1; else if (sd == 2) dy = 1; else if (sd == 3) dx = -1; else dx = 1;

  if (sl >= 2) {
    drawBodyCell(1);
  } else {
    tft.fillRect(SOX + impactBase.x * SC, SOY + impactBase.y * SC, SC, SC, C_SNAKE_B);
  }

  int16_t px = SOX + impactBase.x * SC + dx * offsetPx;
  int16_t py = SOY + impactBase.y * SC + dy * offsetPx;

  drawHeadSprite(px, py);
  tft.drawRect(SOX - 1, SOY - 1, SCO * SC + 2, SRO * SC + 2, C_BORDER);

  if (el >= IMPACT_MS) { sImpact = false; gameOver(); }
}

void pushDirection(int8_t newDir) {
  int8_t refDir = (dirQueueLen == 0) ? snd : dirQueue[dirQueueLen - 1];
  if (newDir == refDir || isOppositeDir(newDir, refDir)) return;
  if (dirQueueLen < 3) {
    dirQueue[dirQueueLen++] = newDir;
  }
}

// Citire ultra-promptă pe front căzător independent pentru fiecare buton (debounce 10 ms per buton)
inline void readInputs(uint32_t now) {
  for (uint8_t b = 0; b < 4; b++) {
    bool raw = digitalRead(BTN_PINS[b]);
    if (raw != btnStable[b] && (now - btnLastEdge[b] >= 10)) {
      btnStable[b] = raw;
      btnLastEdge[b] = now;
      if (raw == LOW) {
        pushDirection((int8_t)b);
      }
    }
  }
}

void updateGame() {
  uint32_t now = millis();
  updateSound(now);

  if (!sa) {
    bool upPressed = (digitalRead(B_UP) == LOW);
    if (!upPressed) {
      waitUpRelease = false;
    } else if (!waitUpRelease && (now - gameOverAt > 200)) {
      initGame();
    }
    return;
  }

  if (sgo) {
    bool pauseCombo = !digitalRead(B_LEFT) && !digitalRead(B_RIGHT);
    if (pauseCombo && !spx && now - ppb > 300) {
      spau = !spau; spx = true; ppb = now;
      if (spau) {
        centerText("PAUZA", SOY + 75, C_WHITE, 3);
      } else {
        drawField();
        redrawFullSnake();
        drawFruit(true);
        slm = now;
        prevStepAmt = SC;
      }
      return;
    }
    if (!pauseCombo) spx = false;
    if (spau) return;
  }

  readInputs(now);

  if (fpS && now - fpt > 350) { fpt = now; fpS = !fpS; drawFruit(fpS); }

  if (!sgo) {
    if (dirQueueLen == 0 &&
        digitalRead(B_UP) && digitalRead(B_DOWN) &&
        digitalRead(B_LEFT) && digitalRead(B_RIGHT)) {
      return;
    }
    if (dirQueueLen > 0) {
      snd = dirQueue[0];
      dirQueue[0] = dirQueue[1];
      dirQueue[1] = dirQueue[2];
      dirQueueLen--;
    }
    sd = snd;
    sgo = true;
    drawField();
    redrawFullSnake();
    drawFruit(true);
    slm = now;
    prevStepAmt = SC;
    return;
  }

  if (now - slm < ssp) return;

  if (prevStepAmt < SC) {
    renderStepTo(SC, true);
  }

  Pt oldHead = sb[0];
  if (dirQueueLen > 0) {
    snd = dirQueue[0];
    dirQueue[0] = dirQueue[1];
    dirQueue[1] = dirQueue[2];
    dirQueueLen--;
  }
  sd = snd;

  Pt nh = oldHead;
  if (sd == 0) nh.y--; else if (sd == 2) nh.y++; else if (sd == 3) nh.x--; else nh.x++;

  if (nh.x < 0 || nh.x >= SCO || nh.y < 0 || nh.y >= SRO) {
    sImpact = true; impactStart = now; impactBase = oldHead;
    return;
  }

  bool ate = (nh.x == sf2.x && nh.y == sf2.y);
  int checkLimit = ate ? sl : (sl - 1);
  for (int i = 0; i < checkLimit; i++) {
    if (sb[i].x == nh.x && sb[i].y == nh.y) { gameOver(); return; }
  }

  Pt oldTail = sb[sl - 1];
  bool grew = false;

  if (ate && sl < SMX) { sl++; grew = true; }

  for (int i = sl - 1; i > 0; i--) sb[i] = sb[i - 1];
  sb[0] = nh;

  animHeadFrom = oldHead;
  prevStepAmt = 0;

  if (grew) {
    tailMoving = false;
    animTailFrom = sb[sl - 1];
  } else {
    tailMoving = true;
    animTailFrom = oldTail;
  }

  drawHeadSprite(SOX + animHeadFrom.x * SC, SOY + animHeadFrom.y * SC);

  slm += ssp;
  if (now - slm >= ssp) slm = now;

  if (ate) {
    ss += fbon;
    triggerEatSound();
    if (ss % 40 == 0 && ssp > 100) ssp -= 15;
    generateFruit();
    updateHeaderScore();
  }
}

void setup() {
  pinMode(B_LEFT, INPUT_PULLUP);  pinMode(B_DOWN, INPUT_PULLUP);
  pinMode(B_RIGHT, INPUT_PULLUP); pinMode(B_UP, INPUT_PULLUP);
  pinMode(BUZ, OUTPUT);

  tft.init(240, 320);
  tft.setSPISpeed(8000000);
  tft.setRotation(1);
  tft.fillScreen(C_BLACK);

  randomSeed(analogRead(A0) ^ (analogRead(A1) << 3));
  EEPROM.get(EE_ADDR, shi);
  if (shi < 0 || shi > 30000) shi = 0;

  delay(50);
  initGame();
}

void loop() {
  readInputs(millis());

  if (sImpact) {
    unsigned long now = millis();
    if (now - lastFrame >= FRAME_MS) { lastFrame = now; renderImpact(); }
    return;
  }

  updateGame();

  unsigned long now = millis();
  if (sa && sgo && !spau && now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    renderSnakeFrame();
    readInputs(millis());
  }
}
