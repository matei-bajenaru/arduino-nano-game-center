#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <EEPROM.h>

// ===== Configurație Pini (identică cu Arduino Nano Game Center) =====
#define TFT_CS 10
#define TFT_DC  8
#define TFT_RST 9
#define B_LEFT  2
#define B_DOWN  3
#define B_RIGHT 4
#define B_UP    7
#define BUZ     6

// ===== Dimensiuni Ecran & Grilă Tetris (10x20) =====
#define SW       320
#define SH       240
#define COLS     10
#define ROWS     20
#define CELL     11          // 10*11 = 110px lățime, 20*11 = 220px înălțime
#define BOARD_X  105         // Centrat perfect între panoul stâng și cel drept
#define BOARD_Y  10          // Margini sus/jos de câte 10px

// ===== Adresă EEPROM dedicată Tetris (nu se suprapune peste Snake la 0) =====
#define EE_MAGIC_ADDR 16
#define EE_SCORE_ADDR 18
#define EE_MAGIC_VAL  0x5454

// ===== Culori UI =====
#define C_BG_OUTER   0x0841
#define C_PANEL_BG   0x1082
#define C_PANEL_HI   0x39E7
#define C_PANEL_SH   0x0000
#define C_FRAME_OUT  0x4A69
#define C_FRAME_IN   0x2104
#define C_TEXT_DIM   0x9CD3
#define C_WHITE      0xFFFF
#define C_GOLD       0xFEE0
#define C_CYAN_TXT   0x3EFF
#define C_RED_TXT    0xF9A6

// ===== Palete Bevel 3D în PROGMEM (0 = dală goală, 1..7 = I, J, L, O, S, T, Z) =====
// Fiecare intrare: { groove, hiBevel, mainFill, shBevel, sheen }
const uint16_t BLOCK_PAL[8][5] PROGMEM = {
  { 0x0000, 0x2965, 0x18E3, 0x0841, 0x18E3 }, // 0: Dală goală reliefată
  { 0x018A, 0x7FFF, 0x067F, 0x0355, 0xDFFF }, // 1: I - Cyan lucios
  { 0x008A, 0x5BDF, 0x19BF, 0x08B3, 0x9DFF }, // 2: J - Albastru cobalt
  { 0x4140, 0xFD88, 0xFA60, 0x99E0, 0xFEB4 }, // 3: L - Portocaliu aprins
  { 0x4200, 0xFFF0, 0xFEE0, 0x9B40, 0xFFFF }, // 4: O - Galben auriu
  { 0x0240, 0x67EC, 0x06E3, 0x0381, 0xB7F5 }, // 5: S - Verde smarald
  { 0x4047, 0xFBBF, 0xF8B8, 0x8830, 0xFDDF }, // 6: T - Magenta / Roz neon
  { 0x4000, 0xFB8E, 0xF082, 0x8800, 0xFD96 }  // 7: Z - Roșu intens
};

// Paletă Ghost Piece (reflexie colorată întunecată pe dalele negre)
const uint16_t GHOST_PAL[8][4] PROGMEM = {
  { 0x0000, 0x2965, 0x18E3, 0x0841 },
  { 0x0043, 0x1B93, 0x0A4B, 0x0105 }, // 1: Ghost Cyan
  { 0x0024, 0x1913, 0x088B, 0x0065 }, // 2: Ghost Albastru
  { 0x1840, 0x5A22, 0x3920, 0x2080 }, // 3: Ghost Portocaliu
  { 0x18A0, 0x5B22, 0x3A00, 0x20E0 }, // 4: Ghost Galben
  { 0x00C0, 0x1B23, 0x09E1, 0x0120 }, // 5: Ghost Verde
  { 0x1802, 0x590B, 0x3846, 0x2003 }, // 6: Ghost Magenta
  { 0x1800, 0x58E3, 0x3820, 0x2000 }  // 7: Ghost Roșu
};

// ===== Formele celor 7 Tetromino-uri × 4 rotații (4 coordonate (x,y) împachetate) =====
// Fiecare octet = (x << 4) | y în grila locală 4x4
const uint8_t TETROMINO[7][4][4] PROGMEM = {
  // 0: I
  {
    { 0x01, 0x11, 0x21, 0x31 },
    { 0x20, 0x21, 0x22, 0x23 },
    { 0x02, 0x12, 0x22, 0x32 },
    { 0x10, 0x11, 0x12, 0x13 }
  },
  // 1: J
  {
    { 0x00, 0x01, 0x11, 0x21 },
    { 0x10, 0x20, 0x11, 0x12 },
    { 0x01, 0x11, 0x21, 0x22 },
    { 0x10, 0x11, 0x02, 0x12 }
  },
  // 2: L
  {
    { 0x20, 0x01, 0x11, 0x21 },
    { 0x10, 0x11, 0x12, 0x22 },
    { 0x01, 0x11, 0x21, 0x02 },
    { 0x00, 0x10, 0x11, 0x12 }
  },
  // 3: O
  {
    { 0x10, 0x20, 0x11, 0x21 },
    { 0x10, 0x20, 0x11, 0x21 },
    { 0x10, 0x20, 0x11, 0x21 },
    { 0x10, 0x20, 0x11, 0x21 }
  },
  // 4: S
  {
    { 0x10, 0x20, 0x01, 0x11 },
    { 0x10, 0x11, 0x21, 0x22 },
    { 0x11, 0x21, 0x02, 0x12 },
    { 0x00, 0x01, 0x11, 0x12 }
  },
  // 5: T
  {
    { 0x10, 0x01, 0x11, 0x21 },
    { 0x10, 0x11, 0x21, 0x12 },
    { 0x01, 0x11, 0x21, 0x12 },
    { 0x10, 0x01, 0x11, 0x12 }
  },
  // 6: Z
  {
    { 0x00, 0x10, 0x11, 0x21 },
    { 0x20, 0x11, 0x21, 0x12 },
    { 0x01, 0x11, 0x12, 0x22 },
    { 0x10, 0x01, 0x11, 0x02 }
  }
};

// Offset-uri Wall Kick pentru rotație lină lângă pereți și podea
const int8_t WALL_KICKS[8][2] PROGMEM = {
  {  0,  0 },
  { -1,  0 },
  {  1,  0 },
  {  0, -1 },
  { -1, -1 },
  {  1, -1 },
  { -2,  0 },
  {  2,  0 }
};

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ===== Buffer atomic 11x11 pixeli (242 bytes SRAM) -> ZERO flickering =====
uint16_t cellBuf[CELL * CELL];

// ===== Stare tablă și randare diferențială =====
uint8_t board[ROWS][COLS];        // 0 = gol, 1..7 = bloc fixat
uint16_t dirtyPrev[ROWS];         // mască de biți cu celulele atinse la cadrul anterior
uint16_t dirtyCurr[ROWS];         // mască de biți cu celulele atinse la cadrul curent

// ===== Stare piesă activă & 7-Bag =====
uint8_t bag[7];
uint8_t bagIdx = 7;
uint8_t curType = 0, curRot = 0;
int8_t curX = 3, curY = 0;
uint8_t nextType = 0;
int8_t ghostY = 0;
int8_t curSubY = 0;               // interpolare verticală 0..CELL-1 pixeli
int8_t prevSubY = -1;

// ===== Scor, nivel, linii & timere =====
uint32_t score = 0, highScore = 0;
uint16_t lines = 0;
uint8_t level = 1;
bool gameRunning = false, isPaused = false, pauseComboLock = false;
unsigned long lastGravity = 0, lastFrame = 0, gameOverAt = 0;
bool waitUpRelease = false;

// Lock delay (permite glisarea piesei pe sol timp de 500ms înainte de blocare)
bool grounded = false;
unsigned long groundStart = 0;
uint8_t lockMoves = 0;
const uint16_t LOCK_DELAY_MS = 500;

// ===== Control ultra-prompt (front căzător + DAS/ARR + dublu-tap Hard Drop) =====
const uint8_t BTN_PINS[4] = { B_UP, B_RIGHT, B_DOWN, B_LEFT };
bool btnState[4] = { HIGH, HIGH, HIGH, HIGH };
unsigned long btnEdgeTime[4] = { 0, 0, 0, 0 };
unsigned long holdStartLeft = 0, holdRepeatLeft = 0;
unsigned long holdStartRight = 0, holdRepeatRight = 0;
unsigned long holdRepeatDown = 0;
unsigned long lastDownTap = 0;

// ===== Sunet non-blocant pe buzzer =====
uint16_t sfxFreq[4];
uint16_t sfxDur[4];
uint8_t sfxCount = 0, sfxIdx = 0;
unsigned long sfxTimer = 0;

void queueTone(uint16_t f, uint16_t d) {
  if (sfxCount < 4) {
    sfxFreq[sfxCount] = f;
    sfxDur[sfxCount] = d;
    sfxCount++;
  }
}

void playSfx1(uint16_t f, uint16_t d) {
  sfxCount = 0; sfxIdx = 0;
  tone(BUZ, f, d);
  sfxTimer = millis() + d + 4;
}

void updateAudio(unsigned long now) {
  if (sfxCount > 0 && now >= sfxTimer) {
    if (sfxIdx < sfxCount) {
      tone(BUZ, sfxFreq[sfxIdx], sfxDur[sfxIdx]);
      sfxTimer = now + sfxDur[sfxIdx] + 8;
      sfxIdx++;
    } else {
      noTone(BUZ);
      sfxCount = 0;
      sfxIdx = 0;
    }
  }
}

// ===== Încărcare / Salvare High Score în EEPROM =====
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

// ===== Transfer atomic SPI pentru o celulă 11x11 =====
inline void blitBuffer(int16_t px, int16_t py, int16_t w, int16_t h) {
  tft.startWrite();
  tft.setAddrWindow(px, py, w, h);
  tft.writePixels(cellBuf, (uint32_t)w * (uint32_t)h);
  tft.endWrite();
}

// Desenează în cellBuf o dală teșită 11x11 (fie dală goală 0, fie bloc colorat 1..7)
void paintBevelTileToBuf(uint8_t id) {
  uint16_t cGroove = pgm_read_word(&BLOCK_PAL[id][0]);
  uint16_t cHi     = pgm_read_word(&BLOCK_PAL[id][1]);
  uint16_t cMain   = pgm_read_word(&BLOCK_PAL[id][2]);
  uint16_t cSh     = pgm_read_word(&BLOCK_PAL[id][3]);
  uint16_t cSheen  = pgm_read_word(&BLOCK_PAL[id][4]);

  uint16_t *dst = cellBuf;
  for (int8_t ly = 0; ly < CELL; ly++) {
    for (int8_t lx = 0; lx < CELL; lx++) {
      if (lx == 0 || ly == 0 || lx == CELL - 1 || ly == CELL - 1) {
        *dst++ = cGroove;
        continue;
      }

      if (id == 0) {
        if (lx == 1 || ly == 1) {
          *dst++ = (lx == CELL - 2 || ly == CELL - 2) ? cMain : cHi;
        } else if (lx == CELL - 2 || ly == CELL - 2) {
          *dst++ = cSh;
        } else {
          *dst++ = cMain;
        }
      } else {
        int8_t ix = lx - 1;
        int8_t iy = ly - 1;
        if (ix < 2 && iy < 9 - ix) {
          *dst++ = cHi;
        } else if (iy < 2 && ix < 9 - iy) {
          *dst++ = cHi;
        } else if (ix >= 7 || iy >= 7) {
          *dst++ = cSh;
        } else if (ix >= 2 && ix <= 3 && iy >= 2 && iy <= 3) {
          *dst++ = cSheen;
        } else {
          *dst++ = cMain;
        }
      }
    }
  }
}

// Desenează în cellBuf o dală Ghost Piece (reflexie colorată întunecată pe fundal negru)
void paintGhostTileToBuf(uint8_t id) {
  uint16_t cGroove = pgm_read_word(&GHOST_PAL[id][0]);
  uint16_t cHi     = pgm_read_word(&GHOST_PAL[id][1]);
  uint16_t cMain   = pgm_read_word(&GHOST_PAL[id][2]);
  uint16_t cSh     = pgm_read_word(&GHOST_PAL[id][3]);

  uint16_t *dst = cellBuf;
  for (int8_t ly = 0; ly < CELL; ly++) {
    for (int8_t lx = 0; lx < CELL; lx++) {
      if (lx == 0 || ly == 0 || lx == CELL - 1 || ly == CELL - 1) {
        *dst++ = cGroove;
        continue;
      }
      int8_t ix = lx - 1;
      int8_t iy = ly - 1;
      if ((ix < 2 && iy < 9 - ix) || (iy < 2 && ix < 9 - iy)) {
        *dst++ = cHi;
      } else if (ix >= 7 || iy >= 7) {
        *dst++ = cSh;
      } else {
        *dst++ = cMain;
      }
    }
  }
}

// Suprapune o fâșie dintr-un bloc activ peste cellBuf (pentru cădere fluidă sub-pixel)
void overlayActiveSlice(uint8_t id, int8_t dstYStart, int8_t dstYEnd, int8_t srcYOffset) {
  uint16_t cGroove = pgm_read_word(&BLOCK_PAL[id][0]);
  uint16_t cHi     = pgm_read_word(&BLOCK_PAL[id][1]);
  uint16_t cMain   = pgm_read_word(&BLOCK_PAL[id][2]);
  uint16_t cSh     = pgm_read_word(&BLOCK_PAL[id][3]);
  uint16_t cSheen  = pgm_read_word(&BLOCK_PAL[id][4]);

  for (int8_t ly = dstYStart; ly < dstYEnd; ly++) {
    int8_t sly = srcYOffset + (ly - dstYStart);
    uint16_t *rowPtr = &cellBuf[ly * CELL];
    for (int8_t lx = 0; lx < CELL; lx++) {
      if (lx == 0 || sly == 0 || lx == CELL - 1 || sly == CELL - 1) {
        rowPtr[lx] = cGroove;
        continue;
      }
      int8_t ix = lx - 1;
      int8_t iy = sly - 1;
      if ((ix < 2 && iy < 9 - ix) || (iy < 2 && ix < 9 - iy)) {
        rowPtr[lx] = cHi;
      } else if (ix >= 7 || iy >= 7) {
        rowPtr[lx] = cSh;
      } else if (ix >= 2 && ix <= 3 && iy >= 2 && iy <= 3) {
        rowPtr[lx] = cSheen;
      } else {
        rowPtr[lx] = cMain;
      }
    }
  }
}

// ===== Helpers pentru coordonatele celor 4 blocuri ale piesei =====
inline void getPieceCell(uint8_t type, uint8_t rot, uint8_t i, int8_t &ox, int8_t &oy) {
  uint8_t packed = pgm_read_byte(&TETROMINO[type][rot][i]);
  ox = (int8_t)(packed >> 4);
  oy = (int8_t)(packed & 0x0F);
}

bool isValidPos(uint8_t type, uint8_t rot, int8_t px, int8_t py) {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t ox, oy;
    getPieceCell(type, rot, i, ox, oy);
    int8_t gx = px + ox;
    int8_t gy = py + oy;
    if (gx < 0 || gx >= COLS || gy >= ROWS) return false;
    if (gy >= 0 && board[gy][gx] != 0) return false;
  }
  return true;
}

void computeGhostY() {
  ghostY = curY;
  while (isValidPos(curType, curRot, curX, ghostY + 1)) {
    ghostY++;
  }
}

// ===== Randare atomică a unei singure celule (c, r) cu fundal + ghost + piesă activă =====
void renderBoardCell(int8_t c, int8_t r, int8_t subY) {
  if (c < 0 || c >= COLS || r < 0 || r >= ROWS) return;

  uint8_t baseId = board[r][c];
  if (baseId > 0) {
    paintBevelTileToBuf(baseId);
  } else {
    bool isGhost = false;
    if (gameRunning && ghostY > curY) {
      for (uint8_t i = 0; i < 4; i++) {
        int8_t ox, oy;
        getPieceCell(curType, curRot, i, ox, oy);
        if (curX + ox == c && ghostY + oy == r) {
          isGhost = true;
          break;
        }
      }
    }
    if (isGhost) {
      paintGhostTileToBuf(curType + 1);
    } else {
      paintBevelTileToBuf(0);
    }
  }

  if (gameRunning) {
    uint8_t activeId = curType + 1;
    for (uint8_t i = 0; i < 4; i++) {
      int8_t ox, oy;
      getPieceCell(curType, curRot, i, ox, oy);
      int8_t bx = curX + ox;
      int8_t by = curY + oy;
      if (bx == c) {
        if (by == r) {
          overlayActiveSlice(activeId, subY, CELL, 0);
        } else if (subY > 0 && by + 1 == r) {
          overlayActiveSlice(activeId, 0, subY, CELL - subY);
        }
      }
    }
  }

  blitBuffer(BOARD_X + c * CELL, BOARD_Y + r * CELL, CELL, CELL);
}

void buildDirtyMask(int8_t subY) {
  for (int8_t r = 0; r < ROWS; r++) dirtyCurr[r] = 0;
  if (!gameRunning) return;

  for (uint8_t i = 0; i < 4; i++) {
    int8_t ox, oy;
    getPieceCell(curType, curRot, i, ox, oy);
    int8_t gx = curX + ox;
    if (gx < 0 || gx >= COLS) continue;

    int8_t gy = curY + oy;
    if (gy >= 0 && gy < ROWS) dirtyCurr[gy] |= (uint16_t)(1U << gx);
    if (subY > 0 && gy + 1 >= 0 && gy + 1 < ROWS) {
      dirtyCurr[gy + 1] |= (uint16_t)(1U << gx);
    }

    int8_t ghy = ghostY + oy;
    if (ghy >= 0 && ghy < ROWS) dirtyCurr[ghy] |= (uint16_t)(1U << gx);
  }
}

void syncDynamicBoard(bool forceAllDynamic) {
  buildDirtyMask(curSubY);

  for (int8_t r = 0; r < ROWS; r++) {
    uint16_t combined = dirtyPrev[r] | dirtyCurr[r];
    if (combined == 0) continue;
    for (int8_t c = 0; c < COLS; c++) {
      uint16_t bit = (uint16_t)(1U << c);
      if ((combined & bit) || (forceAllDynamic && (dirtyCurr[r] & bit))) {
        renderBoardCell(c, r, curSubY);
      }
    }
    dirtyPrev[r] = dirtyCurr[r];
  }
  prevSubY = curSubY;
}

void drawFullBoard() {
  buildDirtyMask(curSubY);
  for (int8_t r = 0; r < ROWS; r++) {
    for (int8_t c = 0; c < COLS; c++) {
      renderBoardCell(c, r, curSubY);
    }
    dirtyPrev[r] = dirtyCurr[r];
  }
  prevSubY = curSubY;
}

// ===== Desenare panouri HUD (stânga: scor/nivel, dreapta: NEXT + controale) =====
void drawBevelBox(int16_t x, int16_t y, int16_t w, int16_t h) {
  tft.fillRect(x, y, w, h, C_PANEL_BG);
  tft.drawFastHLine(x, y, w, C_PANEL_HI);
  tft.drawFastVLine(x, y, h, C_PANEL_HI);
  tft.drawFastHLine(x, y + h - 1, w, C_PANEL_SH);
  tft.drawFastVLine(x + w - 1, y, h, C_PANEL_SH);
}

void updateHudValues() {
  char buf[12];
  tft.setTextSize(1);

  tft.fillRect(14, 30, 76, 12, C_PANEL_BG);
  tft.setTextColor(C_WHITE);
  sprintf(buf, "%lu", (unsigned long)score);
  tft.setCursor(16, 32);
  tft.print(buf);

  tft.fillRect(14, 80, 76, 12, C_PANEL_BG);
  tft.setTextColor(C_GOLD);
  sprintf(buf, "%lu", (unsigned long)highScore);
  tft.setCursor(16, 82);
  tft.print(buf);

  tft.fillRect(14, 130, 76, 12, C_PANEL_BG);
  tft.setTextColor(C_CYAN_TXT);
  sprintf(buf, "%u", lines);
  tft.setCursor(16, 132);
  tft.print(buf);

  tft.fillRect(14, 180, 76, 12, C_PANEL_BG);
  tft.setTextColor(C_WHITE);
  sprintf(buf, "%u", level);
  tft.setCursor(16, 182);
  tft.print(buf);
}

void drawNextPiecePreview() {
  int16_t gx0 = 246;
  int16_t gy0 = 36;

  int8_t minX = 4, maxX = 0, minY = 4, maxY = 0;
  for (uint8_t i = 0; i < 4; i++) {
    int8_t ox, oy;
    getPieceCell(nextType, 0, i, ox, oy);
    if (ox < minX) minX = ox;
    if (ox > maxX) maxX = ox;
    if (oy < minY) minY = oy;
    if (oy > maxY) maxY = oy;
  }
  int16_t offPxX = ((4 - (maxX - minX + 1)) * CELL) / 2 - minX * CELL;
  int16_t offPxY = ((4 - (maxY - minY + 1)) * CELL) / 2 - minY * CELL;

  paintBevelTileToBuf(0);
  for (int8_t r = 0; r < 4; r++) {
    for (int8_t c = 0; c < 4; c++) {
      blitBuffer(gx0 + c * CELL, gy0 + r * CELL, CELL, CELL);
    }
  }

  paintBevelTileToBuf(nextType + 1);
  for (uint8_t i = 0; i < 4; i++) {
    int8_t ox, oy;
    getPieceCell(nextType, 0, i, ox, oy);
    blitBuffer(gx0 + ox * CELL + offPxX, gy0 + oy * CELL + offPxY, CELL, CELL);
  }
}

void drawStaticInterface() {
  tft.fillScreen(C_BG_OUTER);

  tft.drawRect(BOARD_X - 3, BOARD_Y - 3, COLS * CELL + 6, ROWS * CELL + 6, C_FRAME_OUT);
  tft.drawRect(BOARD_X - 2, BOARD_Y - 2, COLS * CELL + 4, ROWS * CELL + 4, C_FRAME_IN);
  tft.drawRect(BOARD_X - 1, BOARD_Y - 1, COLS * CELL + 2, ROWS * CELL + 2, C_PANEL_SH);

  const char* labels[4] = { "SCORE", "HIGH", "LINES", "LEVEL" };
  for (uint8_t i = 0; i < 4; i++) {
    int16_t by = 10 + i * 50;
    drawBevelBox(8, by, 88, 42);
    tft.setTextSize(1);
    tft.setTextColor(C_TEXT_DIM);
    tft.setCursor(16, by + 6);
    tft.print(labels[i]);
  }

  drawBevelBox(224, 10, 88, 82);
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT_DIM);
  tft.setCursor(256, 16);
  tft.print("NEXT");

  drawBevelBox(224, 102, 88, 128);
  tft.setTextColor(C_GOLD);
  tft.setCursor(234, 110);
  tft.print("CONTROLS");

  tft.setTextColor(C_TEXT_DIM);
  tft.setCursor(232, 130); tft.print("UP : ROTATE");
  tft.setCursor(232, 148); tft.print("L/R: MOVE");
  tft.setCursor(232, 166); tft.print("DN : SOFT");
  tft.setCursor(232, 184); tft.print("2xDN: DROP");
  tft.setCursor(232, 202); tft.print("L+R: PAUSE");

  updateHudValues();
}

// ===== Generator 7-Bag =====
uint8_t drawFromBag() {
  if (bagIdx >= 7) {
    for (uint8_t i = 0; i < 7; i++) bag[i] = i;
    for (int8_t i = 6; i > 0; i--) {
      uint8_t j = (uint8_t)random(i + 1);
      uint8_t tmp = bag[i];
      bag[i] = bag[j];
      bag[j] = tmp;
    }
    bagIdx = 0;
  }
  return bag[bagIdx++];
}

uint16_t getGravityInterval() {
  int32_t ms = 680 - (int32_t)(level - 1) * 52;
  if (ms < 75) ms = 75;
  return (uint16_t)ms;
}

void triggerGameOver() {
  gameRunning = false;
  saveHighScore();
  updateHudValues();

  for (int8_t r = ROWS - 1; r >= 0; r--) {
    for (int8_t c = 0; c < COLS; c++) {
      if (board[r][c] > 0) {
        paintGhostTileToBuf(7);
        blitBuffer(BOARD_X + c * CELL, BOARD_Y + r * CELL, CELL, CELL);
      }
    }
    if (r % 2 == 0) {
      tone(BUZ, 180 + r * 12, 25);
      delay(28);
    }
  }
  noTone(BUZ);

  int16_t bx = BOARD_X + 4;
  int16_t by = 68;
  int16_t bw = COLS * CELL - 8;
  int16_t bh = 96;
  drawBevelBox(bx, by, bw, bh);
  tft.drawRect(bx, by, bw, bh, C_GOLD);

  tft.setTextSize(2);
  tft.setTextColor(C_RED_TXT);
  tft.setCursor(bx + 26, by + 12); tft.print("GAME");
  tft.setCursor(bx + 26, by + 32); tft.print("OVER");

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(bx + 14, by + 60); tft.print("APASA SUS");
  tft.setCursor(bx + 10, by + 74); tft.print("PT. RESTART");

  const uint16_t gf[3] = { 330, 277, 220 };
  for (uint8_t i = 0; i < 3; i++) {
    tone(BUZ, gf[i], 95);
    delay(105);
  }
  noTone(BUZ);

  gameOverAt = millis();
  waitUpRelease = (digitalRead(B_UP) == LOW);
}

void spawnPiece() {
  curType = nextType;
  nextType = drawFromBag();
  curRot = 0;
  curX = 3;
  curY = 0;
  curSubY = 0;
  prevSubY = -1;
  grounded = false;
  lockMoves = 0;
  lastGravity = millis();

  drawNextPiecePreview();

  if (!isValidPos(curType, curRot, curX, curY)) {
    triggerGameOver();
    return;
  }

  computeGhostY();
  syncDynamicBoard(true);
}

// ===== Ștergerea liniilor complete cu animație luminoasă =====
void checkAndClearLines() {
  uint8_t fullRows[4];
  uint8_t cleared = 0;

  for (int8_t r = ROWS - 1; r >= 0; r--) {
    bool full = true;
    for (int8_t c = 0; c < COLS; c++) {
      if (board[r][c] == 0) { full = false; break; }
    }
    if (full && cleared < 4) {
      fullRows[cleared++] = (uint8_t)r;
    }
  }

  if (cleared == 0) return;

  for (uint8_t step = 0; step < 2; step++) {
    uint16_t flashCol = (step == 0) ? C_WHITE : C_GOLD;
    for (int i = 0; i < CELL * CELL; i++) cellBuf[i] = flashCol;
    for (uint8_t k = 0; k < cleared; k++) {
      uint8_t r = fullRows[k];
      for (int8_t c = 0; c < COLS; c++) {
        blitBuffer(BOARD_X + c * CELL, BOARD_Y + r * CELL, CELL, CELL);
      }
    }
    tone(BUZ, (cleared == 4) ? (900 + step * 450) : (650 + step * 250), 40);
    delay(45);
  }
  noTone(BUZ);

  int8_t writeRow = ROWS - 1;
  for (int8_t readRow = ROWS - 1; readRow >= 0; readRow--) {
    bool isCleared = false;
    for (uint8_t k = 0; k < cleared; k++) {
      if (fullRows[k] == (uint8_t)readRow) { isCleared = true; break; }
    }
    if (!isCleared) {
      if (writeRow != readRow) {
        for (int8_t c = 0; c < COLS; c++) board[writeRow][c] = board[readRow][c];
      }
      writeRow--;
    }
  }
  while (writeRow >= 0) {
    for (int8_t c = 0; c < COLS; c++) board[writeRow][c] = 0;
    writeRow--;
  }

  static const uint16_t LINE_PTS[5] = { 0, 100, 300, 500, 800 };
  score += (uint32_t)LINE_PTS[cleared] * (uint32_t)level;
  lines += cleared;
  level = (uint8_t)(lines / 10) + 1;

  if (score > highScore) highScore = score;
  updateHudValues();

  if (cleared == 4) {
    queueTone(1046, 55);
    queueTone(1318, 55);
    queueTone(1568, 90);
  } else {
    queueTone(784, 45);
    queueTone(1046, 65);
  }

  for (int8_t r = 0; r < ROWS; r++) {
    dirtyPrev[r] = 0;
    for (int8_t c = 0; c < COLS; c++) {
      paintBevelTileToBuf(board[r][c]);
      blitBuffer(BOARD_X + c * CELL, BOARD_Y + r * CELL, CELL, CELL);
    }
  }
}

void lockCurrentPiece() {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t ox, oy;
    getPieceCell(curType, curRot, i, ox, oy);
    int8_t gx = curX + ox;
    int8_t gy = curY + oy;
    if (gy >= 0 && gy < ROWS && gx >= 0 && gx < COLS) {
      board[gy][gx] = curType + 1;
    }
  }

  curSubY = 0;
  buildDirtyMask(0);
  for (int8_t r = 0; r < ROWS; r++) {
    uint16_t combined = dirtyPrev[r] | dirtyCurr[r];
    for (int8_t c = 0; c < COLS; c++) {
      if (combined & (uint16_t)(1U << c)) {
        paintBevelTileToBuf(board[r][c]);
        blitBuffer(BOARD_X + c * CELL, BOARD_Y + r * CELL, CELL, CELL);
      }
    }
    dirtyPrev[r] = 0;
  }

  playSfx1(220, 22);
  checkAndClearLines();
  spawnPiece();
}

// ===== Mișcări și rotație cu Wall Kicks =====
bool tryMoveHorizontal(int8_t dx) {
  if (isValidPos(curType, curRot, curX + dx, curY)) {
    curX += dx;
    computeGhostY();
    if (grounded && lockMoves < 15) {
      groundStart = millis();
      lockMoves++;
    }
    if (!isValidPos(curType, curRot, curX, curY + 1)) {
      curSubY = 0;
    }
    syncDynamicBoard(false);
    playSfx1(520, 12);
    return true;
  }
  return false;
}

bool tryRotate() {
  uint8_t nextRot = (curRot + 1) & 0x03;
  for (uint8_t k = 0; k < 8; k++) {
    int8_t kx = (int8_t)pgm_read_byte(&WALL_KICKS[k][0]);
    int8_t ky = (int8_t)pgm_read_byte(&WALL_KICKS[k][1]);
    if (isValidPos(curType, nextRot, curX + kx, curY + ky)) {
      curX += kx;
      curY += ky;
      curRot = nextRot;
      computeGhostY();
      if (grounded && lockMoves < 15) {
        groundStart = millis();
        lockMoves++;
      }
      if (!isValidPos(curType, curRot, curX, curY + 1)) {
        curSubY = 0;
      }
      syncDynamicBoard(false);
      playSfx1(740, 16);
      return true;
    }
  }
  return false;
}

void performHardDrop() {
  int8_t dropped = ghostY - curY;
  if (dropped > 0) {
    score += (uint32_t)dropped * 2UL;
    if (score > highScore) highScore = score;
    updateHudValues();
  }
  curY = ghostY;
  curSubY = 0;
  playSfx1(160, 30);
  lockCurrentPiece();
}

void startNewGame() {
  for (int8_t r = 0; r < ROWS; r++) {
    dirtyPrev[r] = 0;
    for (int8_t c = 0; c < COLS; c++) board[r][c] = 0;
  }

  score = 0;
  lines = 0;
  level = 1;
  bagIdx = 7;
  isPaused = false;
  pauseComboLock = false;
  gameRunning = true;
  waitUpRelease = false;

  for (uint8_t b = 0; b < 4; b++) {
    btnState[b] = digitalRead(BTN_PINS[b]);
    btnEdgeTime[b] = 0;
  }

  drawStaticInterface();
  nextType = drawFromBag();
  drawFullBoard();
  spawnPiece();
}

// ===== Citire butoane cu răspuns instant + DAS/ARR =====
void processInputs(unsigned long now) {
  if (!gameRunning) {
    bool upLow = (digitalRead(B_UP) == LOW);
    if (!upLow) {
      waitUpRelease = false;
    } else if (!waitUpRelease && (now - gameOverAt > 220)) {
      startNewGame();
    }
    return;
  }

  bool comboPause = (digitalRead(B_LEFT) == LOW) && (digitalRead(B_RIGHT) == LOW);
  if (comboPause && !pauseComboLock) {
    pauseComboLock = true;
    isPaused = !isPaused;
    if (isPaused) {
      int16_t bx = BOARD_X + 10;
      int16_t by = 96;
      drawBevelBox(bx, by, COLS * CELL - 20, 38);
      tft.setTextSize(2);
      tft.setTextColor(C_GOLD);
      tft.setCursor(bx + 15, by + 11);
      tft.print("PAUZA");
    } else {
      drawFullBoard();
      lastGravity = now;
    }
    return;
  }
  if (!comboPause) pauseComboLock = false;
  if (isPaused) return;

  for (uint8_t b = 0; b < 4; b++) {
    bool raw = digitalRead(BTN_PINS[b]);
    if (raw != btnState[b] && (now - btnEdgeTime[b] >= 12)) {
      btnState[b] = raw;
      btnEdgeTime[b] = now;
      if (raw == LOW) {
        if (b == 0) {
          tryRotate();
        } else if (b == 1) {
          tryMoveHorizontal(1);
          holdStartRight = now;
          holdRepeatRight = now;
        } else if (b == 3) {
          tryMoveHorizontal(-1);
          holdStartLeft = now;
          holdRepeatLeft = now;
        } else if (b == 2) {
          if (now - lastDownTap < 210) {
            lastDownTap = 0;
            performHardDrop();
            return;
          }
          lastDownTap = now;
          holdRepeatDown = now;
          if (isValidPos(curType, curRot, curX, curY + 1)) {
            curY++;
            curSubY = 0;
            lastGravity = now;
            score += 1;
            if (score > highScore) highScore = score;
            updateHudValues();
            syncDynamicBoard(false);
          }
        }
      }
    }
  }

  if (btnState[3] == LOW && btnState[1] == HIGH) {
    if (now - holdStartLeft >= 155 && now - holdRepeatLeft >= 42) {
      holdRepeatLeft = now;
      tryMoveHorizontal(-1);
    }
  } else if (btnState[1] == LOW && btnState[3] == HIGH) {
    if (now - holdStartRight >= 155 && now - holdRepeatRight >= 42) {
      holdRepeatRight = now;
      tryMoveHorizontal(1);
    }
  }

  if (btnState[2] == LOW && (now - holdRepeatDown >= 45)) {
    holdRepeatDown = now;
    if (isValidPos(curType, curRot, curX, curY + 1)) {
      curY++;
      curSubY = 0;
      lastGravity = now;
      score += 1;
      if (score > highScore) highScore = score;
      updateHudValues();
      syncDynamicBoard(false);
    }
  }
}

void updateSimulation(unsigned long now) {
  if (!gameRunning || isPaused) return;

  bool canFall = isValidPos(curType, curRot, curX, curY + 1);
  uint16_t gravMs = getGravityInterval();

  if (canFall) {
    grounded = false;
    unsigned long el = now - lastGravity;
    if (el >= gravMs) {
      curY++;
      curSubY = 0;
      lastGravity += gravMs;
      if (now - lastGravity >= gravMs) lastGravity = now;

      if (!isValidPos(curType, curRot, curX, curY + 1)) {
        grounded = true;
        groundStart = now;
      }
      syncDynamicBoard(false);
    } else {
      int8_t targetSubY = (int8_t)((el * (unsigned long)CELL) / (unsigned long)gravMs);
      if (targetSubY >= CELL) targetSubY = CELL - 1;
      if (targetSubY != prevSubY) {
        curSubY = targetSubY;
        syncDynamicBoard(false);
      }
    }
  } else {
    curSubY = 0;
    if (!grounded) {
      grounded = true;
      groundStart = now;
      syncDynamicBoard(false);
    } else if (now - groundStart >= LOCK_DELAY_MS) {
      lockCurrentPiece();
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

  if (now - lastFrame >= 16) {
    lastFrame = now;
    updateSimulation(now);
  }
}
