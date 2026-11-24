#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <EEPROM.h>

// ===== Configurație Pini (Arduino Nano Game Center) =====
#define TFT_CS 10
#define TFT_DC  8
#define TFT_RST 9
#define B_LEFT  2
#define B_DOWN  3
#define B_RIGHT 4
#define B_UP    7
#define BUZ     6

// ===== Dimensiuni Full-Screen (Arenă 240×220 px + Panou dreapta 68 px) =====
#define SW        320
#define SH        240
#define GW        48          // 48 granule lățime (divizibil cu 3 -> 16 bytes/rând)
#define GH        44          // 44 granule înălțime
#define G_PX      5           // 5×5 pixeli per granulă -> 240×220 px full-screen
#define BLOCK_G   4           // Fiecare pătrat dintr-o piesă = 4×4 granule (20×20 px)
#define DANGER_Y  6           // Limită superioară la 6 granule (30 px) -> zonă spawn 0..5
#define BOARD_W   (GW * G_PX) // 240 px
#define BOARD_H   (GH * G_PX) // 220 px
#define BOARD_X   4           // Margine stânga 4 px
#define BOARD_Y   10          // Margini egale sus/jos de câte 10 px
#define BUF_GRAN  12          // 12 granule (60 px) per transfer SPI -> 120 bytes SRAM

// ===== EEPROM dedicat Sand Tetris =====
#define EE_MAGIC_ADDR 32
#define EE_SCORE_ADDR 34
#define EE_MAGIC_VAL  0x534B

// ===== Culori UI Moderne (Obsidian & Slate) =====
#define C_BG_OUTER  0x0842
#define C_BOARD_BG  0x0821
#define C_PANEL_BG  0x10A3
#define C_PANEL_HI  0x4A69
#define C_PANEL_SH  0x0000
#define C_FRAME_OUT 0x5AEB
#define C_FRAME_IN  0x2965
#define C_TEXT_DIM  0x9CD3
#define C_WHITE     0xFFFF
#define C_GOLD      0xFEE0
#define C_CYAN_TXT  0x47FF
#define C_RED_TXT   0xF9A6
#define C_DANGER_LN 0x9124

// ===== Tabele PROGMEM pentru eliminare 100% a împărțirilor (/3, %3, /6, %6) =====
const uint8_t DIV3[GW] PROGMEM = {
  0,0,0, 1,1,1, 2,2,2, 3,3,3, 4,4,4, 5,5,5, 6,6,6, 7,7,7,
  8,8,8, 9,9,9, 10,10,10, 11,11,11, 12,12,12, 13,13,13, 14,14,14, 15,15,15
};

const uint8_t MOD3[GW] PROGMEM = {
  0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2,
  0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2, 0,1,2
};

const uint8_t DIV6[216] PROGMEM = {
  0,0,0,0,0,0, 1,1,1,1,1,1, 2,2,2,2,2,2, 3,3,3,3,3,3, 4,4,4,4,4,4, 5,5,5,5,5,5,
  6,6,6,6,6,6, 7,7,7,7,7,7, 8,8,8,8,8,8, 9,9,9,9,9,9, 10,10,10,10,10,10, 11,11,11,11,11,11,
  12,12,12,12,12,12, 13,13,13,13,13,13, 14,14,14,14,14,14, 15,15,15,15,15,15, 16,16,16,16,16,16, 17,17,17,17,17,17,
  18,18,18,18,18,18, 19,19,19,19,19,19, 20,20,20,20,20,20, 21,21,21,21,21,21, 22,22,22,22,22,22, 23,23,23,23,23,23,
  24,24,24,24,24,24, 25,25,25,25,25,25, 26,26,26,26,26,26, 27,27,27,27,27,27, 28,28,28,28,28,28, 29,29,29,29,29,29,
  30,30,30,30,30,30, 31,31,31,31,31,31, 32,32,32,32,32,32, 33,33,33,33,33,33, 34,34,34,34,34,34, 35,35,35,35,35,35
};

const uint8_t MOD6[216] PROGMEM = {
  0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5,
  0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5,
  0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5,
  0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5,
  0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5,
  0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5, 0,1,2,3,4,5
};

// ===== Paletă Modernă de Nisip (5 nuanțe per culoare: shadow, dark, mid, light, sparkle) =====
const uint16_t SAND_PAL[5][5] PROGMEM = {
  { 0x0821, 0x0821, 0x0821, 0x0821, 0x0821 }, // 0: Fundal obsidian
  { 0x6061, 0xA8C3, 0xD945, 0xF228, 0xFCB2 }, // 1: Coral Rubin
  { 0x09E2, 0x1B84, 0x2DE6, 0x572B, 0x9FF3 }, // 2: Smarald Mint (cu contrast granular clar)
  { 0x0949, 0x1A70, 0x2BD6, 0x557C, 0x9EFF }, // 3: Albastru Cobalt
  { 0x6A00, 0xB3E1, 0xE562, 0xFEE7, 0xFFF4 }  // 4: Chihlimbar Auriu
};

// ===== Paletă Blocuri Moderne 3D Concentrice (20×20 px per pătrat) =====
const uint16_t PIECE_PAL[5][4] PROGMEM = {
  { 0x0821, 0x0821, 0x0821, 0x0821 },
  { 0x5021, 0xFCB2, 0xD945, 0xFA69 }, // 1: Coral Rubin
  { 0x0961, 0x9FF3, 0x2DE6, 0x67EC }, // 2: Smarald Mint
  { 0x08E7, 0x9EFF, 0x2BD6, 0x6DDF }, // 3: Albastru Cobalt
  { 0x59A0, 0xFFF4, 0xE562, 0xFF2B }  // 4: Chihlimbar Auriu
};

// ===== Cele 7 Tetromino-uri × 4 rotații =====
const uint8_t TETROMINO[7][4][4] PROGMEM = {
  { { 0x01, 0x11, 0x21, 0x31 }, { 0x10, 0x11, 0x12, 0x13 }, { 0x01, 0x11, 0x21, 0x31 }, { 0x10, 0x11, 0x12, 0x13 } },
  { { 0x00, 0x01, 0x11, 0x21 }, { 0x10, 0x20, 0x11, 0x12 }, { 0x01, 0x11, 0x21, 0x22 }, { 0x10, 0x11, 0x02, 0x12 } },
  { { 0x20, 0x01, 0x11, 0x21 }, { 0x10, 0x11, 0x12, 0x22 }, { 0x01, 0x11, 0x21, 0x02 }, { 0x00, 0x10, 0x11, 0x12 } },
  { { 0x00, 0x10, 0x01, 0x11 }, { 0x00, 0x10, 0x01, 0x11 }, { 0x00, 0x10, 0x01, 0x11 }, { 0x00, 0x10, 0x01, 0x11 } },
  { { 0x10, 0x20, 0x01, 0x11 }, { 0x10, 0x11, 0x21, 0x22 }, { 0x10, 0x20, 0x01, 0x11 }, { 0x10, 0x11, 0x21, 0x22 } },
  { { 0x10, 0x01, 0x11, 0x21 }, { 0x10, 0x11, 0x21, 0x12 }, { 0x01, 0x11, 0x21, 0x12 }, { 0x10, 0x01, 0x11, 0x12 } },
  { { 0x00, 0x10, 0x11, 0x21 }, { 0x20, 0x11, 0x21, 0x12 }, { 0x00, 0x10, 0x11, 0x21 }, { 0x20, 0x11, 0x21, 0x12 } }
};

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ===== Memorie Ultra-Economă (1176 bytes total -> >870 bytes liberi pentru stivă!) =====
uint8_t grid[GH][GW / 3];         // 704 bytes: granule nisip (doar valori 0..4!)
uint8_t dirtyMinX[GH];            // 44 bytes: 255 = rând curat, 0..47 = marginea stângă dirty
uint8_t dirtyMaxX[GH];            // 44 bytes: 0..47 = marginea dreaptă dirty
uint16_t visMask[GH][3];          // 264 bytes: bitset separat pentru flood-fill (nu atinge grid!)
uint16_t lineBuf[BUF_GRAN * G_PX]; // 120 bytes: buffer SPI (60 px per transfer)

// ===== Stare Piesă Activă =====
uint8_t curType = 0, curRot = 0, curColor = 1;
int8_t curGX = 16, curGY = 0;
int8_t prevGX = -1, prevGY = -99;
uint8_t prevType = 0, prevRot = 0;
uint8_t nextType = 0, nextColor = 1;

// ===== Stare Joc, Scor & Combo =====
uint32_t score = 0, highScore = 0;
uint16_t clearedLines = 0;
uint8_t comboMult = 1;
bool gameRunning = false, isPaused = false, pauseLock = false;
bool scanDir = false, needSettleCheck = false;
unsigned long lastSandTick = 0, lastFallTick = 0, lastFrame = 0, gameOverAt = 0;
bool waitUpRelease = false;

// ===== Input & Dublu-Click pe JOS =====
const uint8_t BTN_PINS[4] = { B_UP, B_RIGHT, B_DOWN, B_LEFT };
bool btnState[4] = { HIGH, HIGH, HIGH, HIGH };
unsigned long btnEdgeTime[4] = { 0, 0, 0, 0 };
unsigned long holdStartL = 0, holdRepL = 0;
unsigned long holdStartR = 0, holdRepR = 0;
unsigned long holdRepD = 0, lastDownTap = 0;

// ===== Sunet Non-Blocant =====
unsigned long sfxTimer = 0;

void playSfx1(uint16_t f, uint16_t d) {
  tone(BUZ, f, d);
  sfxTimer = millis() + d + 4;
}

void updateAudio(unsigned long now) {
  if (sfxTimer > 0 && now >= sfxTimer) {
    noTone(BUZ);
    sfxTimer = 0;
  }
}

// ===== Helpers Dirty Bounding Box per Rând =====
inline void markSpanDirty(int8_t r, int8_t x0, int8_t x1) {
  if (r < 0 || r >= GH) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= GW) x1 = GW - 1;
  if (x0 > x1) return;
  if ((uint8_t)x0 < dirtyMinX[r]) dirtyMinX[r] = (uint8_t)x0;
  if ((uint8_t)x1 > dirtyMaxX[r]) dirtyMaxX[r] = (uint8_t)x1;
}

inline void markAllDirty() {
  for (int8_t r = 0; r < GH; r++) {
    dirtyMinX[r] = 0;
    dirtyMaxX[r] = GW - 1;
  }
}

// Reîmprospătează textura de nisip pe toate celulele ocupate (fără să șteargă fundalul)
void refreshAllSandCells() {
  for (int8_t y = 0; y < GH; y++) {
    int8_t minX = GW, maxX = -1;
    for (int8_t x = 0; x < GW; x++) {
      if (getCell(x, y) != 0) {
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
      }
    }
    if (maxX >= minX) {
      markSpanDirty(y, minX, maxX);
    }
  }
}

// ===== Acces Ultra-Rapid Fără Împărțiri (Tabele PROGMEM) =====
inline uint8_t getCell(int8_t x, int8_t y) {
  uint8_t packed = grid[y][pgm_read_byte(&DIV3[x])];
  uint8_t rem = pgm_read_byte(&MOD3[x]);
  if (rem == 0) return pgm_read_byte(&MOD6[packed]);
  uint8_t q1 = pgm_read_byte(&DIV6[packed]);
  if (rem == 1) return pgm_read_byte(&MOD6[q1]);
  return pgm_read_byte(&DIV6[q1]);
}

inline void setCell(int8_t x, int8_t y, uint8_t val) {
  uint8_t idx = pgm_read_byte(&DIV3[x]);
  uint8_t packed = grid[y][idx];
  uint8_t c0 = pgm_read_byte(&MOD6[packed]);
  uint8_t q1 = pgm_read_byte(&DIV6[packed]);
  uint8_t c1 = pgm_read_byte(&MOD6[q1]);
  uint8_t c2 = pgm_read_byte(&DIV6[q1]);
  uint8_t rem = pgm_read_byte(&MOD3[x]);
  if (rem == 0) c0 = val;
  else if (rem == 1) c1 = val;
  else c2 = val;
  grid[y][idx] = (uint8_t)(c0 + (c1 << 2) + (c1 << 1) + (c2 << 5) + (c2 << 2));
}

void unpackRow(int8_t y, uint8_t *outRow) {
  const uint8_t *src = grid[y];
  uint8_t x = 0;
  for (uint8_t i = 0; i < GW / 3; i++) {
    uint8_t p = src[i];
    outRow[x++] = pgm_read_byte(&MOD6[p]);
    uint8_t q1 = pgm_read_byte(&DIV6[p]);
    outRow[x++] = pgm_read_byte(&MOD6[q1]);
    outRow[x++] = pgm_read_byte(&DIV6[q1]);
  }
}

void packRow(int8_t y, const uint8_t *inRow) {
  uint8_t *dst = grid[y];
  uint8_t x = 0;
  for (uint8_t i = 0; i < GW / 3; i++) {
    uint8_t c0 = inRow[x++];
    uint8_t c1 = inRow[x++];
    uint8_t c2 = inRow[x++];
    dst[i] = (uint8_t)(c0 + (c1 << 2) + (c1 << 1) + (c2 << 5) + (c2 << 2));
  }
}

// ===== Helpers Bitset pentru Detecția Podurilor (0% modificări în grid) =====
inline bool getVis(int8_t x, int8_t y) {
  return (visMask[y][(uint8_t)x >> 4] & (1U << ((uint8_t)x & 15))) != 0;
}

inline void setVis(int8_t x, int8_t y) {
  visMask[y][(uint8_t)x >> 4] |= (1U << ((uint8_t)x & 15));
}

// ===== EEPROM High Score =====
void loadHighScore() {
  uint16_t magic = 0;
  EEPROM.get(EE_MAGIC_ADDR, magic);
  if (magic == EE_MAGIC_VAL) {
    EEPROM.get(EE_SCORE_ADDR, highScore);
    if (highScore > 999999UL) highScore = 0;
  } else {
    highScore = 0;
    EEPROM.put(EE_MAGIC_ADDR, (uint16_t)EE_MAGIC_VAL);
    EEPROM.put(EE_SCORE_ADDR, highScore);
  }
}

void saveHighScore() {
  if (score > highScore) {
    highScore = score;
    EEPROM.put(EE_MAGIC_ADDR, (uint16_t)EE_MAGIC_VAL);
    EEPROM.put(EE_SCORE_ADDR, highScore);
  }
}

// ===== Helpers Piesă Activă =====
inline void getBlockOffset(uint8_t type, uint8_t rot, uint8_t i, int8_t &bx, int8_t &by) {
  uint8_t p = pgm_read_byte(&TETROMINO[type][rot][i]);
  bx = (int8_t)(p >> 4);
  by = (int8_t)(p & 0x0F);
}

bool canPlacePiece(uint8_t type, uint8_t rot, int8_t gx, int8_t gy) {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx, by;
    getBlockOffset(type, rot, i, bx, by);
    int8_t x0 = gx + bx * BLOCK_G;
    int8_t y0 = gy + by * BLOCK_G;
    if (x0 < 0 || x0 + BLOCK_G > GW || y0 < 0 || y0 + BLOCK_G > GH) return false;
    for (int8_t dy = 0; dy < BLOCK_G; dy++) {
      int8_t cy = y0 + dy;
      for (int8_t dx = 0; dx < BLOCK_G; dx++) {
        if (getCell(x0 + dx, cy) != 0) return false;
      }
    }
  }
  return true;
}

void markPieceDirty(uint8_t type, uint8_t rot, int8_t gx, int8_t gy) {
  if (gy < 0) return;
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx, by;
    getBlockOffset(type, rot, i, bx, by);
    int8_t x0 = gx + bx * BLOCK_G;
    int8_t y0 = gy + by * BLOCK_G;
    for (int8_t dy = 0; dy < BLOCK_G; dy++) {
      markSpanDirty(y0 + dy, x0, x0 + BLOCK_G - 1);
    }
  }
}

// Hash pseudo-aleator pe 8 biți per granulă -> textură de nisip uniformă pe toate culorile
inline uint8_t hashSand(int8_t x, int8_t r) {
  uint8_t h = (uint8_t)(x * 73 + r * 137);
  h ^= (uint8_t)(h >> 3);
  h = (uint8_t)(h * 39);
  h ^= (uint8_t)(h >> 4);
  return h;
}

// ===== Randare Modernă Zero-Flicker =====
uint16_t getModernBlockPixel(uint8_t colId, int8_t lx, int8_t ly) {
  uint8_t dEdge = (uint8_t)lx;
  if ((uint8_t)ly < dEdge) dEdge = (uint8_t)ly;
  if ((uint8_t)(19 - lx) < dEdge) dEdge = (uint8_t)(19 - lx);
  if ((uint8_t)(19 - ly) < dEdge) dEdge = (uint8_t)(19 - ly);

  bool topLeft = (lx + ly < 19);
  if (dEdge == 0) return pgm_read_word(&PIECE_PAL[colId][0]);
  if (dEdge <= 2) return pgm_read_word(&PIECE_PAL[colId][topLeft ? 1 : 2]);
  if (dEdge == 3) return pgm_read_word(&PIECE_PAL[colId][2]);
  if (dEdge == 4) return pgm_read_word(&PIECE_PAL[colId][0]);
  if (dEdge == 5) return pgm_read_word(&PIECE_PAL[colId][1]);
  return pgm_read_word(&PIECE_PAL[colId][3]);
}

void blitRowChunk(int8_t r, int8_t xStart, int8_t xEnd, const uint8_t *rowCells,
                  const int8_t *actX, const int8_t *actY, bool drawAct) {
  int16_t spanGran = xEnd - xStart + 1;
  int16_t spanPx = spanGran * G_PX;
  bool isDangerLineRow = (r == DANGER_Y);

  tft.startWrite();
  tft.setAddrWindow(BOARD_X + xStart * G_PX, BOARD_Y + r * G_PX, spanPx, G_PX);

  for (int8_t subY = 0; subY < G_PX; subY++) {
    uint16_t *dst = lineBuf;
    bool drawLimitDash = (isDangerLineRow && subY == 0);

    for (int8_t x = xStart; x <= xEnd; x++) {
      uint8_t v = rowCells[x];
      if (v == 0 || v > 4) {
        if (drawLimitDash && (x & 1) == 0) {
          *dst++ = C_DANGER_LN;
          *dst++ = C_DANGER_LN;
          *dst++ = C_DANGER_LN;
          *dst++ = C_BOARD_BG;
          *dst++ = C_BOARD_BG;
        } else {
          *dst++ = C_BOARD_BG;
          *dst++ = C_BOARD_BG;
          *dst++ = C_BOARD_BG;
          *dst++ = C_BOARD_BG;
          *dst++ = C_BOARD_BG;
        }
      } else {
        uint8_t h = hashSand(x, r);
        uint8_t bodyIdx = (uint8_t)(1 + (h % 3)); // 1=dark, 2=mid, 3=light
        uint16_t cBody   = pgm_read_word(&SAND_PAL[v][bodyIdx]);
        uint16_t cShade  = pgm_read_word(&SAND_PAL[v][0]);
        uint16_t cBright = pgm_read_word(&SAND_PAL[v][bodyIdx + 1]);

        if (subY == 0) {
          *dst++ = ((h & 0x07) == 0) ? pgm_read_word(&SAND_PAL[v][4]) : cBright;
          *dst++ = cBright;
          *dst++ = cBody;
          *dst++ = ((h & 0x18) == 0) ? cBright : cBody;
          *dst++ = cShade;
        } else if (subY < 4) {
          *dst++ = ((subY == 2 && (h & 0x04)) ? cBody : cBright);
          *dst++ = cBody;
          *dst++ = ((subY == 2 && (h & 0x02)) ? cBright : cBody);
          *dst++ = ((subY == 1 && (h & 0x20)) ? cBright : cBody);
          *dst++ = cShade;
        } else {
          *dst++ = ((h & 0x40) ? cShade : cBody);
          *dst++ = cShade;
          *dst++ = cShade;
          *dst++ = cShade;
          *dst++ = cShade;
        }
      }
    }

    if (drawAct) {
      for (uint8_t i = 0; i < 4; i++) {
        if (r >= actY[i] && r < actY[i] + BLOCK_G) {
          int8_t bx0 = actX[i];
          int8_t bx1 = bx0 + BLOCK_G - 1;
          if (bx1 < xStart || bx0 > xEnd) continue;
          int8_t ly = (r - actY[i]) * G_PX + subY;
          for (int8_t gx = bx0; gx <= bx1; gx++) {
            if (gx < xStart || gx > xEnd) continue;
            int16_t dstBase = (gx - xStart) * G_PX;
            int8_t srcBase = (gx - bx0) * G_PX;
            lineBuf[dstBase]     = getModernBlockPixel(curColor, srcBase,     ly);
            lineBuf[dstBase + 1] = getModernBlockPixel(curColor, srcBase + 1, ly);
            lineBuf[dstBase + 2] = getModernBlockPixel(curColor, srcBase + 2, ly);
            lineBuf[dstBase + 3] = getModernBlockPixel(curColor, srcBase + 3, ly);
            lineBuf[dstBase + 4] = getModernBlockPixel(curColor, srcBase + 4, ly);
          }
        }
      }
    }

    tft.writePixels(lineBuf, spanPx);
  }
  tft.endWrite();
}

void renderDirtyRows() {
  int8_t actX[4], actY[4];
  bool activeVisible = (gameRunning && curGY >= 0);
  if (activeVisible) {
    for (uint8_t i = 0; i < 4; i++) {
      int8_t bx, by;
      getBlockOffset(curType, curRot, i, bx, by);
      actX[i] = curGX + bx * BLOCK_G;
      actY[i] = curGY + by * BLOCK_G;
    }
  }

  uint8_t rowCells[GW];

  for (int8_t r = 0; r < GH; r++) {
    uint8_t x0 = dirtyMinX[r];
    if (x0 == 255) continue;
    uint8_t x1 = dirtyMaxX[r];
    dirtyMinX[r] = 255;
    dirtyMaxX[r] = 0;

    unpackRow(r, rowCells);

    for (int8_t cx = (int8_t)x0; cx <= (int8_t)x1; cx += BUF_GRAN) {
      int8_t ce = cx + BUF_GRAN - 1;
      if (ce > (int8_t)x1) ce = (int8_t)x1;
      blitRowChunk(r, cx, ce, rowCells, actX, actY, activeVisible);
    }
  }

  prevGX = curGX;
  prevGY = curGY;
  prevType = curType;
  prevRot = curRot;
}

// ===== Fizică Ultra-Rapidă (zero lag chiar și când tot ecranul este plin!) =====
bool stepSandPhysics() {
  bool anyMoved = false;
  scanDir = !scanDir;

  uint8_t rowAbove[GW];
  uint8_t rowBelow[GW];

  unpackRow(GH - 1, rowBelow);
  bool belowDirty = false;

  for (int8_t y = GH - 2; y >= 0; y--) {
    unpackRow(y, rowAbove);

    int8_t minDirty = GW, maxDirty = -1;
    bool dir = scanDir ^ (y & 1);

    for (int8_t step = 0; step < GW; step++) {
      int8_t x = dir ? step : (GW - 1 - step);
      uint8_t c = rowAbove[x];
      if (c == 0) continue;

      if (rowBelow[x] == 0) {
        rowAbove[x] = 0;
        rowBelow[x] = c;
        if (x < minDirty) minDirty = x;
        if (x > maxDirty) maxDirty = x;
        continue;
      }

      bool canL1 = (x > 0) && (rowBelow[x - 1] == 0);
      bool canR1 = (x < GW - 1) && (rowBelow[x + 1] == 0);

      if (canL1 || canR1) {
        int8_t nx;
        if (canL1 && canR1) {
          nx = (((x + y + (scanDir ? 1 : 0)) & 1) == 0) ? (x - 1) : (x + 1);
        } else {
          nx = canL1 ? (x - 1) : (x + 1);
        }
        rowAbove[x] = 0;
        rowBelow[nx] = c;
        int8_t lo = (x < nx) ? x : nx;
        int8_t hi = (x > nx) ? x : nx;
        if (lo < minDirty) minDirty = lo;
        if (hi > maxDirty) maxDirty = hi;
        continue;
      }

      bool canL2 = (x > 1) && (rowAbove[x - 1] == 0) && (rowBelow[x - 2] == 0);
      bool canR2 = (x < GW - 2) && (rowAbove[x + 1] == 0) && (rowBelow[x + 2] == 0);

      if (canL2 || canR2) {
        int8_t nx;
        if (canL2 && canR2) {
          nx = (((x + y + (scanDir ? 1 : 0)) & 1) == 0) ? (x - 2) : (x + 2);
        } else {
          nx = canL2 ? (x - 2) : (x + 2);
        }
        rowAbove[x] = 0;
        rowBelow[nx] = c;
        int8_t lo = (x < nx) ? x : nx;
        int8_t hi = (x > nx) ? x : nx;
        if (lo < minDirty) minDirty = lo;
        if (hi > maxDirty) maxDirty = hi;
      }
    }

    bool aboveDirty = (maxDirty >= minDirty);
    if (aboveDirty) {
      markSpanDirty(y, minDirty, maxDirty);
      markSpanDirty(y + 1, minDirty, maxDirty);
      anyMoved = true;
    }

    if (belowDirty || aboveDirty) {
      packRow(y + 1, rowBelow);
    }

    for (uint8_t i = 0; i < GW; i++) rowBelow[i] = rowAbove[i];
    belowDirty = aboveDirty;
  }

  if (belowDirty) {
    packRow(0, rowBelow);
  }

  return anyMoved;
}

// ===== Detecție și Ștergere Poduri de Culoare prin Bitset =====
bool checkAndClearColorBridges() {
  bool clearedAny = false;
  uint8_t rowCurr[GW];

  for (uint8_t targetCol = 1; targetCol <= 4; targetCol++) {
    int8_t minY = GH, maxY = -1;
    bool hasLeft = false, hasRight = false;

    for (int8_t y = 0; y < GH; y++) {
      unpackRow(y, rowCurr);
      bool rowHasCol = false;
      for (int8_t x = 0; x < GW; x++) {
        if (rowCurr[x] == targetCol) {
          rowHasCol = true;
          if (x == 0) hasLeft = true;
          if (x == GW - 1) hasRight = true;
        }
      }
      if (rowHasCol) {
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
      visMask[y][0] = 0;
      visMask[y][1] = 0;
      visMask[y][2] = 0;
    }

    if (!hasLeft || !hasRight) continue;

    for (int8_t y = minY; y <= maxY; y++) {
      if (getCell(0, y) == targetCol) setVis(0, y);
    }

    bool changed = true;
    while (changed) {
      changed = false;

      for (int8_t y = maxY; y >= minY; y--) {
        unpackRow(y, rowCurr);
        int8_t x = 0;
        while (x < GW) {
          if (rowCurr[x] != targetCol) { x++; continue; }
          int8_t segStart = x;
          bool connected = false;
          while (x < GW && rowCurr[x] == targetCol) {
            if (!connected) {
              if (getVis(x, y)) connected = true;
              else if (y < maxY && ((x > 0 && getVis(x - 1, y + 1)) || getVis(x, y + 1) || (x < GW - 1 && getVis(x + 1, y + 1)))) connected = true;
              else if (y > minY && ((x > 0 && getVis(x - 1, y - 1)) || getVis(x, y - 1) || (x < GW - 1 && getVis(x + 1, y - 1)))) connected = true;
            }
            x++;
          }
          if (connected) {
            for (int8_t k = segStart; k < x; k++) {
              if (!getVis(k, y)) {
                setVis(k, y);
                changed = true;
              }
            }
          }
        }
      }

      if (!changed) break;
      changed = false;

      for (int8_t y = minY; y <= maxY; y++) {
        unpackRow(y, rowCurr);
        int8_t x = 0;
        while (x < GW) {
          if (rowCurr[x] != targetCol) { x++; continue; }
          int8_t segStart = x;
          bool connected = false;
          while (x < GW && rowCurr[x] == targetCol) {
            if (!connected) {
              if (getVis(x, y)) connected = true;
              else if (y > minY && ((x > 0 && getVis(x - 1, y - 1)) || getVis(x, y - 1) || (x < GW - 1 && getVis(x + 1, y - 1)))) connected = true;
              else if (y < maxY && ((x > 0 && getVis(x - 1, y + 1)) || getVis(x, y + 1) || (x < GW - 1 && getVis(x + 1, y + 1)))) connected = true;
            }
            x++;
          }
          if (connected) {
            for (int8_t k = segStart; k < x; k++) {
              if (!getVis(k, y)) {
                setVis(k, y);
                changed = true;
              }
            }
          }
        }
      }
    }

    bool reachedRight = false;
    for (int8_t y = minY; y <= maxY; y++) {
      if (getVis(GW - 1, y)) { reachedRight = true; break; }
    }

    if (!reachedRight) continue;

    clearedAny = true;
    uint16_t count = 0;

    for (uint8_t flash = 0; flash < 2; flash++) {
      uint16_t fCol = (flash == 0) ? C_WHITE : C_GOLD;
      for (int8_t y = minY; y <= maxY; y++) {
        if ((visMask[y][0] | visMask[y][1] | visMask[y][2]) == 0) continue;
        unpackRow(y, rowCurr);

        for (int8_t x0 = 0; x0 < GW; x0 += BUF_GRAN) {
          tft.startWrite();
          tft.setAddrWindow(BOARD_X + x0 * G_PX, BOARD_Y + y * G_PX, BUF_GRAN * G_PX, G_PX);
          for (int8_t sub = 0; sub < G_PX; sub++) {
            uint16_t *dst = lineBuf;
            for (int8_t x = x0; x < x0 + BUF_GRAN; x++) {
              uint8_t v = rowCurr[x];
              uint16_t pxCol = getVis(x, y) ? fCol : ((v == 0) ? C_BOARD_BG : pgm_read_word(&SAND_PAL[v][2]));
              *dst++ = pxCol; *dst++ = pxCol; *dst++ = pxCol; *dst++ = pxCol; *dst++ = pxCol;
            }
            tft.writePixels(lineBuf, BUF_GRAN * G_PX);
          }
          tft.endWrite();
        }
      }
      tone(BUZ, 740 + flash * 370 + comboMult * 110, 34);
      delay(38);
    }
    noTone(BUZ);

    for (int8_t y = minY; y <= maxY; y++) {
      if ((visMask[y][0] | visMask[y][1] | visMask[y][2]) == 0) continue;
      unpackRow(y, rowCurr);
      int8_t mMinX = GW, mMaxX = -1;
      for (int8_t x = 0; x < GW; x++) {
        if (getVis(x, y)) {
          rowCurr[x] = 0;
          if (x < mMinX) mMinX = x;
          if (x > mMaxX) mMaxX = x;
          count++;
        }
      }
      if (mMaxX >= mMinX) {
        packRow(y, rowCurr);
        markSpanDirty(y, mMinX, mMaxX);
      }
    }

    clearedLines++;
    score += (uint32_t)count * 4UL * (uint32_t)comboMult;
    if (score > highScore) highScore = score;
    if (comboMult < 12) comboMult++;
    needSettleCheck = true;
  }

  if (clearedAny) {
    refreshAllSandCells();
  }

  return clearedAny;
}

bool isCeilingBreached() {
  uint8_t rowTemp[GW];
  for (int8_t y = 0; y < DANGER_Y; y++) {
    unpackRow(y, rowTemp);
    for (int8_t x = 0; x < GW; x++) {
      if (rowTemp[x] >= 1 && rowTemp[x] <= 4) return true;
    }
  }
  return false;
}

// ===== Interfață HUD Laterală (248..316 px) =====
void drawBevelBox(int16_t x, int16_t y, int16_t w, int16_t h) {
  tft.fillRect(x, y, w, h, C_PANEL_BG);
  tft.drawFastHLine(x, y, w, C_PANEL_HI);
  tft.drawFastVLine(x, y, h, C_PANEL_HI);
  tft.drawFastHLine(x, y + h - 1, w, C_PANEL_SH);
  tft.drawFastVLine(x + w - 1, y, h, C_PANEL_SH);
}

void updateHudValues() {
  tft.setTextSize(1);

  tft.fillRect(252, 91, 60, 10, C_PANEL_BG);
  tft.setTextColor(C_WHITE);
  tft.setCursor(253, 92);
  tft.print(score);

  tft.fillRect(252, 122, 60, 10, C_PANEL_BG);
  tft.setTextColor(C_GOLD);
  tft.setCursor(253, 123);
  tft.print(highScore);

  tft.fillRect(252, 153, 60, 10, C_PANEL_BG);
  tft.setTextColor(C_CYAN_TXT);
  tft.setCursor(253, 154);
  tft.print(clearedLines);

  tft.fillRect(252, 184, 60, 10, C_PANEL_BG);
  tft.setTextColor(comboMult > 1 ? C_GOLD : C_WHITE);
  tft.setCursor(253, 185);
  tft.print(F("X"));
  tft.print(comboMult);
}

void drawNextPreview() {
  tft.fillRect(252, 24, 60, 44, C_BOARD_BG);

  int8_t minX = 4, maxX = 0, minY = 4, maxY = 0;
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx, by;
    getBlockOffset(nextType, 0, i, bx, by);
    if (bx < minX) minX = bx;
    if (bx > maxX) maxX = bx;
    if (by < minY) minY = by;
    if (by > maxY) maxY = by;
  }

  const int16_t BSZ = 11;
  int16_t ox = 252 + (60 - (maxX - minX + 1) * BSZ) / 2 - minX * BSZ;
  int16_t oy = 24 + (44 - (maxY - minY + 1) * BSZ) / 2 - minY * BSZ;

  uint16_t c0 = pgm_read_word(&PIECE_PAL[nextColor][0]);
  uint16_t c1 = pgm_read_word(&PIECE_PAL[nextColor][1]);
  uint16_t c2 = pgm_read_word(&PIECE_PAL[nextColor][2]);
  uint16_t c3 = pgm_read_word(&PIECE_PAL[nextColor][3]);

  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx, by;
    getBlockOffset(nextType, 0, i, bx, by);
    int16_t x = ox + bx * BSZ;
    int16_t y = oy + by * BSZ;
    tft.fillRect(x, y, BSZ, BSZ, c2);
    tft.drawRect(x, y, BSZ, BSZ, c0);
    tft.drawFastHLine(x + 1, y + 1, BSZ - 2, c1);
    tft.drawFastVLine(x + 1, y + 1, BSZ - 2, c1);
    tft.drawRect(x + 2, y + 2, BSZ - 4, BSZ - 4, c0);
    tft.fillRect(x + 3, y + 3, BSZ - 6, BSZ - 6, c3);
  }
}

void drawStaticUi() {
  tft.fillScreen(C_BG_OUTER);

  tft.drawRect(BOARD_X - 3, BOARD_Y - 3, BOARD_W + 6, BOARD_H + 6, C_FRAME_OUT);
  tft.drawRect(BOARD_X - 2, BOARD_Y - 2, BOARD_W + 4, BOARD_H + 4, C_FRAME_IN);
  tft.drawRect(BOARD_X - 1, BOARD_Y - 1, BOARD_W + 2, BOARD_H + 2, C_PANEL_SH);

  int16_t limPy = BOARD_Y + DANGER_Y * G_PX;
  tft.drawFastHLine(BOARD_X - 3, limPy, 3, C_RED_TXT);
  tft.drawFastHLine(BOARD_X + BOARD_W, limPy, 3, C_RED_TXT);

  drawBevelBox(248, 10, 68, 62);
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT_DIM);
  tft.setCursor(270, 14);
  tft.print(F("NEXT"));

  drawBevelBox(248, 76, 68, 126);
  tft.setTextColor(C_TEXT_DIM);
  tft.setCursor(253, 80);  tft.print(F("SCORE"));
  tft.setCursor(253, 111); tft.print(F("BEST"));
  tft.setCursor(253, 142); tft.print(F("LINES"));
  tft.setCursor(253, 173); tft.print(F("COMBO"));

  drawBevelBox(248, 206, 68, 24);
  for (uint8_t c = 1; c <= 4; c++) {
    int16_t sx = 253 + (c - 1) * 15;
    tft.fillRect(sx, 212, 12, 12, pgm_read_word(&SAND_PAL[c][2]));
    tft.drawRect(sx, 212, 12, 12, pgm_read_word(&SAND_PAL[c][0]));
  }

  updateHudValues();
}

void triggerGameOver() {
  gameRunning = false;
  if (prevGY >= 0) markPieceDirty(prevType, prevRot, prevGX, prevGY);
  curGY = -99;
  renderDirtyRows();

  saveHighScore();
  updateHudValues();

  int16_t bx = BOARD_X + 42;
  int16_t by = 66;
  int16_t bw = 156;
  int16_t bh = 104;

  tft.fillRect(bx - 3, by - 3, bw + 6, bh + 6, C_PANEL_SH);
  drawBevelBox(bx, by, bw, bh);
  tft.drawRect(bx, by, bw, bh, C_GOLD);
  tft.drawRect(bx + 2, by + 2, bw - 4, bh - 4, C_FRAME_IN);

  tft.setTextSize(2);
  tft.setTextColor(C_RED_TXT);
  tft.setCursor(bx + 24, by + 14);
  tft.print(F("GAME OVER"));

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD);
  tft.setCursor(bx + 24, by + 42);
  tft.print(F("SCOR: "));
  tft.setTextColor(C_WHITE);
  tft.print(score);

  tft.setTextColor(C_TEXT_DIM);
  tft.setCursor(bx + 27, by + 68);
  tft.print(F("APASA SUS PENTRU"));
  tft.setTextColor(C_CYAN_TXT);
  tft.setCursor(bx + 54, by + 82);
  tft.print(F("RESTART"));

  const uint16_t gf[3] = { 330, 277, 220 };
  for (uint8_t i = 0; i < 3; i++) {
    tone(BUZ, gf[i], 90);
    delay(100);
  }
  noTone(BUZ);

  gameOverAt = millis();
  waitUpRelease = (digitalRead(B_UP) == LOW);
}

void spawnPiece() {
  curType = nextType;
  curColor = nextColor;
  nextType = (uint8_t)random(7);
  nextColor = (uint8_t)(random(4) + 1);

  curRot = 0;
  curGX = 16;
  curGY = 0;
  prevGY = -99;
  lastFallTick = millis();

  drawNextPreview();

  if (!canPlacePiece(curType, curRot, curGX, curGY)) {
    triggerGameOver();
    return;
  }

  markPieceDirty(curType, curRot, curGX, curGY);
  renderDirtyRows();
}

void dissolvePieceToSand() {
  if (prevGY >= 0) markPieceDirty(prevType, prevRot, prevGX, prevGY);

  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx, by;
    getBlockOffset(curType, curRot, i, bx, by);
    int8_t x0 = curGX + bx * BLOCK_G;
    int8_t y0 = curGY + by * BLOCK_G;
    for (int8_t dy = 0; dy < BLOCK_G; dy++) {
      int8_t cy = y0 + dy;
      if (cy < 0 || cy >= GH) continue;
      markSpanDirty(cy, x0, x0 + BLOCK_G - 1);
      for (int8_t dx = 0; dx < BLOCK_G; dx++) {
        int8_t cx = x0 + dx;
        if (cx >= 0 && cx < GW && getCell(cx, cy) == 0) {
          setCell(cx, cy, curColor);
        }
      }
    }
  }

  comboMult = 1;
  needSettleCheck = true;
  prevGY = -99;
  playSfx1(230, 16);

  if (isCeilingBreached() && !stepSandPhysics()) {
    if (checkAndClearColorBridges()) {
      updateHudValues();
      renderDirtyRows();
    }
    if (isCeilingBreached()) {
      triggerGameOver();
      return;
    }
  }

  spawnPiece();
}

void redrawActiveTransition() {
  if (prevGY >= 0) markPieceDirty(prevType, prevRot, prevGX, prevGY);
  markPieceDirty(curType, curRot, curGX, curGY);
  renderDirtyRows();
}

bool moveHorizontal(int8_t stepG) {
  int8_t dir = (stepG > 0) ? 1 : -1;
  int8_t mag = abs(stepG);
  for (int8_t s = mag; s >= 1; s--) {
    if (canPlacePiece(curType, curRot, curGX + dir * s, curGY)) {
      curGX += dir * s;
      redrawActiveTransition();
      playSfx1(520, 8);
      return true;
    }
  }
  return false;
}

bool rotatePiece() {
  uint8_t nextRot = (curRot + 1) & 0x03;
  static const int8_t KICKS[9][2] = {
    { 0, 0 }, { -1, 0 }, { 1, 0 }, { -2, 0 }, { 2, 0 },
    { -4, 0 }, { 4, 0 }, { 0, -2 }, { 0, -4 }
  };
  for (uint8_t k = 0; k < 9; k++) {
    int8_t nx = curGX + KICKS[k][0];
    int8_t ny = curGY + KICKS[k][1];
    if (canPlacePiece(curType, nextRot, nx, ny)) {
      curGX = nx;
      curGY = ny;
      curRot = nextRot;
      redrawActiveTransition();
      playSfx1(740, 12);
      return true;
    }
  }
  return false;
}

void hardDrop() {
  int8_t startY = curGY;
  while (canPlacePiece(curType, curRot, curGX, curGY + 1)) {
    curGY++;
  }
  if (curGY != startY) {
    markPieceDirty(prevType, prevRot, prevGX, prevGY);
    markPieceDirty(curType, curRot, curGX, curGY);
  }
  playSfx1(340, 18);
  dissolvePieceToSand();
}

void startNewGame() {
  markAllDirty();
  for (int8_t y = 0; y < GH; y++) {
    for (int8_t x = 0; x < GW / 3; x++) grid[y][x] = 0;
  }

  score = 0;
  clearedLines = 0;
  comboMult = 1;
  isPaused = false;
  pauseLock = false;
  gameRunning = true;
  waitUpRelease = false;
  needSettleCheck = false;
  prevGY = -99;
  lastDownTap = 0;

  for (uint8_t b = 0; b < 4; b++) {
    btnState[b] = digitalRead(BTN_PINS[b]);
    btnEdgeTime[b] = 0;
  }

  drawStaticUi();
  nextType = (uint8_t)random(7);
  nextColor = (uint8_t)(random(4) + 1);
  renderDirtyRows();
  spawnPiece();
}

void processInputs(unsigned long now) {
  if (!gameRunning) {
    bool upLow = (digitalRead(B_UP) == LOW);
    if (!upLow) {
      waitUpRelease = false;
    } else if (!waitUpRelease && (now - gameOverAt > 250)) {
      startNewGame();
    }
    return;
  }

  bool comboPause = (digitalRead(B_LEFT) == LOW) && (digitalRead(B_RIGHT) == LOW);
  if (comboPause && !pauseLock) {
    pauseLock = true;
    isPaused = !isPaused;
    if (isPaused) {
      int16_t bx = BOARD_X + 66;
      int16_t by = 98;
      drawBevelBox(bx, by, 108, 38);
      tft.setTextSize(2);
      tft.setTextColor(C_GOLD);
      tft.setCursor(bx + 24, by + 11);
      tft.print(F("PAUZA"));
    } else {
      markAllDirty();
      renderDirtyRows();
      lastFallTick = now;
    }
    return;
  }
  if (!comboPause) pauseLock = false;
  if (isPaused) return;

  for (uint8_t b = 0; b < 4; b++) {
    bool raw = digitalRead(BTN_PINS[b]);
    if (raw != btnState[b] && (now - btnEdgeTime[b] >= 12)) {
      btnState[b] = raw;
      btnEdgeTime[b] = now;
      if (raw == LOW) {
        if (b == 0) {
          rotatePiece();
        } else if (b == 1) {
          moveHorizontal(1);
          holdStartR = now;
          holdRepR = now;
        } else if (b == 3) {
          moveHorizontal(-1);
          holdStartL = now;
          holdRepL = now;
        } else if (b == 2) {
          if (lastDownTap > 0 && (now - lastDownTap <= 280)) {
            lastDownTap = 0;
            hardDrop();
            return;
          }
          lastDownTap = now;
          holdRepD = now;
          if (canPlacePiece(curType, curRot, curGX, curGY + 1)) {
            curGY++;
            redrawActiveTransition();
          } else {
            dissolvePieceToSand();
            return;
          }
        }
      }
    }
  }

  if (btnState[3] == LOW && btnState[1] == HIGH) {
    if (now - holdStartL >= 95 && now - holdRepL >= 18) {
      holdRepL = now;
      moveHorizontal(-1);
    }
  } else if (btnState[1] == LOW && btnState[3] == HIGH) {
    if (now - holdStartR >= 95 && now - holdRepR >= 18) {
      holdRepR = now;
      moveHorizontal(1);
    }
  }

  if (btnState[2] == LOW && (now - holdRepD >= 18)) {
    holdRepD = now;
    if (canPlacePiece(curType, curRot, curGX, curGY + 1)) {
      curGY++;
      redrawActiveTransition();
    } else {
      dissolvePieceToSand();
    }
  }
}

void setup() {
  pinMode(B_LEFT, INPUT_PULLUP);
  pinMode(B_DOWN, INPUT_PULLUP);
  pinMode(B_RIGHT, INPUT_PULLUP);
  pinMode(B_UP, INPUT_PULLUP);
  pinMode(BUZ, OUTPUT);

  tft.init(240, 320);
  tft.setSPISpeed(8000000);
  tft.setRotation(1);
  tft.fillScreen(C_BG_OUTER);

  randomSeed(analogRead(A0) ^ (analogRead(A1) << 4) ^ micros());
  loadHighScore();
  startNewGame();
}

void loop() {
  unsigned long now = millis();
  updateAudio(now);
  processInputs(now);

  if (!gameRunning || isPaused) return;

  if (now - lastSandTick >= 12) {
    lastSandTick = now;
    bool moved = stepSandPhysics();
    if (moved) {
      needSettleCheck = true;
    } else if (needSettleCheck) {
      needSettleCheck = false;
      if (checkAndClearColorBridges()) {
        updateHudValues();
        renderDirtyRows();
      } else if (isCeilingBreached()) {
        triggerGameOver();
        return;
      }
    }
  }

  uint16_t fallMs = (clearedLines > 60) ? 52 : (82 - (clearedLines >> 1));

  if (now - lastFallTick >= fallMs) {
    lastFallTick = now;
    if (canPlacePiece(curType, curRot, curGX, curGY + 1)) {
      curGY++;
      markPieceDirty(prevType, prevRot, prevGX, prevGY);
      markPieceDirty(curType, curRot, curGX, curGY);
    } else {
      dissolvePieceToSand();
      if (!gameRunning) return;
    }
  }

  if (now - lastFrame >= 16) {
    lastFrame = now;
    renderDirtyRows();
  }
}
