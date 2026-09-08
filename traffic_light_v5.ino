// ============================================================
// HE THONG DEN GIAO THONG BAT DOI XUNG - v5 (MAX7219 + 74HC165)
// Arduino Uno R3 + 2x 74HC595 (U3-U4, den tin hieu) 
//                + 2x MAX7219 (man hinh)
//                + 1x 74HC165 (doc nut bam Manual mode)
// Thay the hoan toan U5 (segment) + U6 (LEFT digit) + U7 (LIGHT digit)
// + 16x BC558 + 16x Rb bang 2 IC MAX7219 dieu khien man hinh 2821AS (CC)
// ============================================================
//
// THU VIEN CAN CAI DAT TRUOC KHI BIEN DICH:
//   Arduino IDE > Sketch > Include Library > Manage Libraries
//   Tim va cai: "LedControl" (tac gia: Eberhard Fahle)
//
// TAI SAO DOI SANG MAX7219:
//   - Bo hoan toan 16x BC558, 16x Rb (2.2k), 32x tro 220 ohm segment
//   - Khong con phai tu viet ham chong ghosting (BLANK_US, delayMicroseconds)
//   - MAX7219 tu quet da hop noi bo (~800Hz voi 8 digit), khong bi anh huong
//     neu Arduino ban xu ly cam bien (vi du pulseIn cho HC-SR04) mat vai chuc ms
//   - Chi con 3 day tin hieu (DIN, CLK, LOAD) cho toan bo 16 digit
//
// TAI SAO DUNG 74HC165 CHO MANUAL MODE:
//   - Tiet kiem chan Arduino khi co nhieu nut bam
//   - Chi can 3 chan (DATA, CLK, LOAD) de doc 8 nut (hoac nhieu hon neu cascade)
//   - Ly tuong cho cac he thong co nhieu lenh manual (Emergency, A Go, B Go, ...)
//
// BA CHUOI RIENG BIET:
//   Chuoi 1: 74HC595 (U3, U4) dieu khien den tin hieu mau (do/vang/xanh/re trai)
//            Chan: D11=DATA, D12=CLK, D13=LATCH
//   Chuoi 2: 2x MAX7219 cascade dieu khien 16 digit man hinh 7-doan
//            Chan: D7=DIN, D8=CLK, D9=LOAD
//   Chuoi 3: 74HC165 doc cac nut bam Manual mode
//            Chan: D4=DATA, D5=CLK, D6=LOAD
//
// SO DO CASCADE 2 MAX7219:
//   Arduino DIN -> MAX7219 #0 (DIN) -> DOUT -> MAX7219 #1 (DIN)
//   Ca hai LOAD va CLK noi chung (bus song song, khong noi tiep)
//   Trong thu vien LedControl: addr=0 la chip gan Arduino nhat (nhan DIN truc tiep)
//
// PHAN CONG DIGIT (dua theo cau truc LEFT/LIGHT da co):
//   MAX7219 addr=0 (MAX_LIGHT, nhan DIN truc tiep tu Arduino)
//     -> 4 man hinh LIGHT (A1, A2, B1, B2), moi man hinh 2 digit
//     digit 0 = A1 LIGHT DIG1 (hang chuc)  digit 1 = A1 LIGHT DIG2 (hang don vi)
//     digit 2 = A2 LIGHT DIG1              digit 3 = A2 LIGHT DIG2
//     digit 4 = B1 LIGHT DIG1              digit 5 = B1 LIGHT DIG2
//     digit 6 = B2 LIGHT DIG1              digit 7 = B2 LIGHT DIG2
//   MAX7219 addr=1 (MAX_LEFT, DOUT cua chip #0 noi vao DIN chip nay)
//     -> 4 man hinh LEFT (A1, A2, B1, B2)
//     digit 0 = A1 LEFT DIG1               digit 1 = A1 LEFT DIG2
//     digit 2 = A2 LEFT DIG1               digit 3 = A2 LEFT DIG2
//     digit 4 = B1 LEFT DIG1               digit 5 = B1 LEFT DIG2
//     digit 6 = B2 LEFT DIG1               digit 7 = B2 LEFT DIG2
//
// RSET CHO MOI CHIP MAX7219 (theo datasheet 2821AS: VF=1.8V, IF max=30mA):
//   Chon RSET = 15 kOhm -> ISEG khoang 20-25mA, an toan duoi muc max 30mA
//   MOI CHIP CAN 1 RSET RIENG (khong dung chung 1 dien tro cho ca 2 chip)
//
// 74HC165 BIT MAPPING (Q0-Q7 noi vao Arduino DATA pin):
//   Q0 = PIN_AUTO (chuyen Auto/Manual)
//   Q1 = PIN_MANUAL (chuyen Manual/Auto)
//   Q2 = PIN_BTN_ALLRED (Emergency Stop)
//   Q3 = PIN_BTN_AGO (Route A Go)
//   Q4 = PIN_BTN_BGO (Route B Go)
//   Q5-Q7 = du phong cho nut bo sung
//   Luu y: 74HC165 output active HIGH khi button pressed (noi mass qua button)
//
// U3 BIT MAPPING (khong doi so voi ban truoc):
//   Q0=red A1, Q1=yellow A1, Q2=green A1, Q3=left A1,
//   Q4=red A2, Q5=yellow A2, Q6=green A2, Q7=left A2
// U4 BIT MAPPING: mirror cua U3 cho Route B


#include <LedControl.h>

// ── 74HC595 pins (den tin hieu, U3-U4) ────────────────────────────────────────
#define DATA_PIN    11
#define CLK_PIN     12
#define LATCH_PIN   13

// ── MAX7219 pins (man hinh 7-doan, 2 chip cascade) ────────────────────────────
#define MAX_DIN     7
#define MAX_CLK     8
#define MAX_LOAD    9
#define MAX_NUM_DEV 2      // tong so chip MAX7219 trong chuoi

LedControl lc = LedControl(MAX_DIN, MAX_CLK, MAX_LOAD, MAX_NUM_DEV);

#define MAX_LIGHT  0   // dia chi chip dieu khien 4 man hinh LIGHT (nhan DIN truc tiep tu Arduino)
#define MAX_LEFT   1   // dia chi chip dieu khien 4 man hinh LEFT (DOUT cua chip #0 noi vao DIN chip nay)

// ── 74HC165 pins (doc nut bam Manual mode) ────────────────────────────────────
#define HC165_DATA  4    // Q7 output tu 74HC165
#define HC165_CLK   5    // CLK (CP)
#define HC165_LOAD  6    // LOAD (/PL)

// Bit mask cho 74HC165
#define HC165_AUTO       (1 << 0)   // Q0: chuyen Auto
#define HC165_MANUAL     (1 << 1)   // Q1: chuyen Manual
#define HC165_ALLRED     (1 << 2)   // Q2: Emergency Stop
#define HC165_AGO        (1 << 3)   // Q3: Route A Go
#define HC165_BGO        (1 << 4)   // Q4: Route B Go

// ── Indicator LED pins ────────────────────────────────────────────────────────
#define LED_AUTO_IND    2
#define LED_MANUAL_IND  3
#define LED_ALLRED_IND  A0   // Re-purpose analog pins as digital outputs
#define LED_AGO_IND     A1
#define LED_BGO_IND     A2

// ── Phase timing (giay) ────────────────────────────────────────────────────────
#define A_L2_CUTOFF  30
#define A_L1_CUTOFF  35
#define A_STR_END    40
#define A_YEL_END    45
#define B_L2_CUTOFF  75
#define B_L1_CUTOFF  80
#define B_STR_END    85
#define CYCLE_TOTAL  90

#define DEBOUNCE_MS     20


// ── U3: Route A traffic LEDs ─────────────────────────────────────────────────
#define U3_RED_A1    (1 << 0)
#define U3_YEL_A1    (1 << 1)
#define U3_GRN_A1    (1 << 2)
#define U3_LEFT_A1   (1 << 3)
#define U3_RED_A2    (1 << 4)
#define U3_YEL_A2    (1 << 5)
#define U3_GRN_A2    (1 << 6)
#define U3_LEFT_A2   (1 << 7)
#define U3_RED_BOTH  (U3_RED_A1  | U3_RED_A2)
#define U3_YEL_BOTH  (U3_YEL_A1  | U3_YEL_A2)
#define U3_GRN_BOTH  (U3_GRN_A1  | U3_GRN_A2)

// ── U4: Route B traffic LEDs ─────────────────────────────────────────────────
#define U4_RED_B1    (1 << 0)
#define U4_YEL_B1    (1 << 1)
#define U4_GRN_B1    (1 << 2)
#define U4_LEFT_B1   (1 << 3)
#define U4_RED_B2    (1 << 4)
#define U4_YEL_B2    (1 << 5)
#define U4_GRN_B2    (1 << 6)
#define U4_LEFT_B2   (1 << 7)
#define U4_RED_BOTH  (U4_RED_B1  | U4_RED_B2)
#define U4_YEL_BOTH  (U4_YEL_B1  | U4_YEL_B2)
#define U4_GRN_BOTH  (U4_GRN_B1  | U4_GRN_B2)


// ── Global state ───────────────────────────────────────────────────────────────
byte allData[2];   // [0]=U4 (gui truoc), [1]=U3 (gui sau) -- xem ham commit()

bool isAutoMode = true;
int  mainTime   = 0;
int  lastPhase  = -1;

enum ManualCmd { CMD_ALLRED, CMD_AGO, CMD_BGO };
ManualCmd currentCmd = CMD_ALLRED;

// Gia tri hien thi (khong doi logic tinh toan so voi ban truoc)
int cntLeftA  = 0;
int cntLeftB  = 0;
int cntLightA = 0;
int cntLightB = 0;

// Luu gia tri da hien thi lan truoc de tranh ghi lai MAX7219 khi khong doi
// (giam so lan goi SPI khong can thiet, khong bat buoc nhung toi uu nhe)
int lastShownLeftA  = -1;
int lastShownLeftB  = -1;
int lastShownLightA = -1;
int lastShownLightB = -1;

unsigned long prevSecond = 0;

// Bien luu trang thai input tu 74HC165
byte hc165Data = 0;
bool lastRawAuto   = false;
bool lastRawManual = false;
bool lastRawAllRed = false;
bool lastRawAGo    = false;
bool lastRawBGo    = false;
unsigned long lastDebounceTime = 0;


// ── Setup ──────────────────────────────────────────────────────────────────────
void setup() {
  pinMode(DATA_PIN,  OUTPUT);
  pinMode(CLK_PIN,   OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  // 74HC165 pins
  pinMode(HC165_DATA, INPUT);
  pinMode(HC165_CLK,  OUTPUT);
  pinMode(HC165_LOAD, OUTPUT);

  pinMode(LED_AUTO_IND,   OUTPUT);
  pinMode(LED_MANUAL_IND, OUTPUT);
  pinMode(LED_ALLRED_IND, OUTPUT);
  pinMode(LED_AGO_IND,    OUTPUT);
  pinMode(LED_BGO_IND,    OUTPUT);

  // Khoi tao 2 chip MAX7219
  // Moi chip mac dinh o che do shutdown khi vua cap nguon -> phai wake up
  for (int addr = 0; addr < MAX_NUM_DEV; addr++) {
    lc.shutdown(addr, false);        // thoat che do shutdown
    lc.setIntensity(addr, 8);        // do sang 0-15, chon giua (~50%)
    lc.setScanLimit(addr, 7);        // quet du 8 digit (0-7)
    lc.clearDisplay(addr);           // xoa man hinh luc khoi dong
  }

  // Trang thai khoi dong an toan cho den tin hieu: tat ca do
  allData[1] = U3_RED_BOTH;   // U3 (Route A)
  allData[0] = U4_RED_BOTH;   // U4 (Route B)
  commit();

  Serial.begin(9600);
  Serial.println("He thong den giao thong - v5 (MAX7219 + 74HC165)");
  Serial.println("U3-U4 (74HC595) + 2x MAX7219 (man hinh) + 74HC165 (nut bam)");

  prevSecond = millis();
}


// ── Main loop ──────────────────────────────────────────────────────────────────
void loop() {
  readInputs();

  if (isAutoMode) {
    runAutoMode();
  } else {
    runManualMode();
  }

  updateIndicatorLEDs();
  updateDisplays();   // thay the hoan toan runDisplayMux() cua ban cu
}


// ── Doc 74HC165 va debounce input ─────────────────────────────────────────────
void readInputs() {
  // Xung LOAD de nap du lieu tu cac nut bam vao thanh ghi dich
  digitalWrite(HC165_LOAD, LOW);
  digitalWrite(HC165_LOAD, HIGH);
  
  // Doc 8 bit tu 74HC165 (MSB first)
  byte inputData = 0;
  for (int i = 7; i >= 0; i--) {
    inputData |= (digitalRead(HC165_DATA) << i);
    digitalWrite(HC165_CLK, HIGH);
    digitalWrite(HC165_CLK, LOW);
  }
  
  hc165Data = inputData;
  
  // Trich xuat cac bit rieng le (active HIGH khi button pressed)
  bool rawAuto    = (hc165Data & HC165_AUTO)   != 0;
  bool rawManual  = (hc165Data & HC165_MANUAL) != 0;
  bool rawAllRed  = (hc165Data & HC165_ALLRED) != 0;
  bool rawAGo     = (hc165Data & HC165_AGO)    != 0;
  bool rawBGo     = (hc165Data & HC165_BGO)    != 0;

  bool anyChange = (rawAuto   != lastRawAuto)   ||
                   (rawManual != lastRawManual)  ||
                   (rawAllRed != lastRawAllRed)  ||
                   (rawAGo    != lastRawAGo)     ||
                   (rawBGo    != lastRawBGo);

  if (anyChange) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime < DEBOUNCE_MS) {
    return;
  }

  // Xu ly chuyen che do Auto <-> Manual
  if (rawAuto == true && rawManual == false && !isAutoMode) {
    isAutoMode  = true;
    mainTime    = 0;
    prevSecond  = millis();
    lastPhase   = -1;
    Serial.println();
    Serial.println("[CHE DO] Manual -> Auto | Chu ky khoi dong lai tu Phase A1");
  }

  if (rawManual == true && rawAuto == false && isAutoMode) {
    isAutoMode  = false;
    currentCmd  = CMD_ALLRED;
    Serial.println();
    Serial.println("[CHE DO] Auto -> Manual | Mac dinh: ALL RED");
  }

  // Xu ly nut bam trong Manual mode
  if (!isAutoMode) {
    if (rawAllRed == true && lastRawAllRed == false) {
      currentCmd = CMD_ALLRED;
      Serial.println("[MANUAL] Lenh: ALL RED");
    }
    if (rawAGo == true && lastRawAGo == false) {
      currentCmd = CMD_AGO;
      Serial.println("[MANUAL] Lenh: ROUTE A GO");
    }
    if (rawBGo == true && lastRawBGo == false) {
      currentCmd = CMD_BGO;
      Serial.println("[MANUAL] Lenh: ROUTE B GO");
    }
  }

  lastRawAuto   = rawAuto;
  lastRawManual = rawManual;
  lastRawAllRed = rawAllRed;
  lastRawAGo    = rawAGo;
  lastRawBGo    = rawBGo;
}


// ── Auto mode: 8 pha, chu ky 90 giay (logic khong doi so voi ban truoc) ──────
void runAutoMode() {
  if (millis() - prevSecond >= 1000UL) {
    prevSecond += 1000UL;
    mainTime++;
    if (mainTime >= CYCLE_TOTAL) {
      mainTime = 0;
      Serial.println("--- chu ky moi ---");
    }
    printStatus();
  }

  updateCountdowns();
  setPhaseOutputs();
  commit();
}


// ── Tinh gia tri dem nguoc (giu nguyen logic da kiem chung) ──────────────────
void updateCountdowns() {

  if (mainTime < A_L2_CUTOFF) {
    cntLeftA = A_L2_CUTOFF - mainTime;
  } else if (mainTime < A_L1_CUTOFF) {
    cntLeftA = A_L1_CUTOFF - mainTime;
  } else {
    cntLeftA = 0;
  }

  if (mainTime >= A_YEL_END && mainTime < B_L2_CUTOFF) {
    cntLeftB = B_L2_CUTOFF - mainTime;
  } else if (mainTime >= B_L2_CUTOFF && mainTime < B_L1_CUTOFF) {
    cntLeftB = B_L1_CUTOFF - mainTime;
  } else {
    cntLeftB = 0;
  }

  if (mainTime < A_L2_CUTOFF) {
    cntLightA = A_L2_CUTOFF - mainTime;
  } else if (mainTime < A_L1_CUTOFF) {
    cntLightA = A_L1_CUTOFF - mainTime;
  } else if (mainTime < A_STR_END) {
    cntLightA = A_STR_END - mainTime;
  } else if (mainTime < A_YEL_END) {
    cntLightA = A_YEL_END - mainTime;
  } else {
    cntLightA = CYCLE_TOTAL - mainTime;
  }

  if (mainTime < A_YEL_END) {
    cntLightB = A_YEL_END - mainTime;
  } else if (mainTime < B_L2_CUTOFF) {
    cntLightB = B_L2_CUTOFF - mainTime;
  } else if (mainTime < B_L1_CUTOFF) {
    cntLightB = B_L1_CUTOFF - mainTime;
  } else if (mainTime < B_STR_END) {
    cntLightB = B_STR_END - mainTime;
  } else {
    cntLightB = CYCLE_TOTAL - mainTime;
  }
}


// ── Dat trang thai den theo pha (giong het ban truoc, chi con U3/U4) ─────────
void setPhaseOutputs() {

  if (mainTime < A_L2_CUTOFF) {
    if (lastPhase != 1) { lastPhase = 1;
      Serial.println("[A1] A: xanh + re trai A1&A2 | B: do"); }
    allData[1] = U3_GRN_BOTH | U3_LEFT_A1 | U3_LEFT_A2;
    allData[0] = U4_RED_BOTH;
  }
  else if (mainTime < A_L1_CUTOFF) {
    if (lastPhase != 2) { lastPhase = 2;
      Serial.println("[A2] A: xanh + re trai A1 (A2 da tat) | B: do"); }
    allData[1] = U3_GRN_BOTH | U3_LEFT_A1;
    allData[0] = U4_RED_BOTH;
  }
  else if (mainTime < A_STR_END) {
    if (lastPhase != 3) { lastPhase = 3;
      Serial.println("[A3] A: xanh thang clearance | B: do"); }
    allData[1] = U3_GRN_BOTH;
    allData[0] = U4_RED_BOTH;
  }
  else if (mainTime < A_YEL_END) {
    if (lastPhase != 4) { lastPhase = 4;
      Serial.println("[A4] A: vang | B: do"); }
    allData[1] = U3_YEL_BOTH;
    allData[0] = U4_RED_BOTH;
  }
  else if (mainTime < B_L2_CUTOFF) {
    if (lastPhase != 5) { lastPhase = 5;
      Serial.println("[B1] B: xanh + re trai B1&B2 | A: do"); }
    allData[1] = U3_RED_BOTH;
    allData[0] = U4_GRN_BOTH | U4_LEFT_B1 | U4_LEFT_B2;
  }
  else if (mainTime < B_L1_CUTOFF) {
    if (lastPhase != 6) { lastPhase = 6;
      Serial.println("[B2] B: xanh + re trai B1 (B2 da tat) | A: do"); }
    allData[1] = U3_RED_BOTH;
    allData[0] = U4_GRN_BOTH | U4_LEFT_B1;
  }
  else if (mainTime < B_STR_END) {
    if (lastPhase != 7) { lastPhase = 7;
      Serial.println("[B3] B: xanh thang clearance | A: do"); }
    allData[1] = U3_RED_BOTH;
    allData[0] = U4_GRN_BOTH;
  }
  else {
    if (lastPhase != 8) { lastPhase = 8;
      Serial.println("[B4] B: vang | A: do"); }
    allData[1] = U3_RED_BOTH;
    allData[0] = U4_YEL_BOTH;
  }
}


// ── Manual mode (khong doi so voi ban truoc) ──────────────────────────────────
void runManualMode() {
  if (currentCmd == CMD_ALLRED) {
    allData[1] = U3_RED_BOTH;
    allData[0] = U4_RED_BOTH;
    cntLeftA = cntLeftB = cntLightA = cntLightB = 0;
  }
  else if (currentCmd == CMD_AGO) {
    allData[1] = U3_GRN_BOTH | U3_LEFT_A1 | U3_LEFT_A2;
    allData[0] = U4_RED_BOTH;
    cntLeftA = cntLeftB = cntLightA = cntLightB = 0;
  }
  else if (currentCmd == CMD_BGO) {
    allData[1] = U3_RED_BOTH;
    allData[0] = U4_GRN_BOTH | U4_LEFT_B1 | U4_LEFT_B2;
    cntLeftA = cntLeftB = cntLightA = cntLightB = 0;
  }

  commit();
}


// ── Cap nhat 5 LED chi thi ────────────────────────────────────────────────────
void updateIndicatorLEDs() {
  digitalWrite(LED_AUTO_IND,   isAutoMode ? HIGH : LOW);
  digitalWrite(LED_MANUAL_IND, isAutoMode ? LOW  : HIGH);

  if (!isAutoMode) {
    digitalWrite(LED_ALLRED_IND, currentCmd == CMD_ALLRED ? HIGH : LOW);
    digitalWrite(LED_AGO_IND,    currentCmd == CMD_AGO    ? HIGH : LOW);
    digitalWrite(LED_BGO_IND,    currentCmd == CMD_BGO    ? HIGH : LOW);
  } else {
    digitalWrite(LED_ALLRED_IND, LOW);
    digitalWrite(LED_AGO_IND,    LOW);
    digitalWrite(LED_BGO_IND,    LOW);
  }
}


// ── Gui allData[] ra U3/U4 (74HC595, khong lien quan MAX7219) ────────────────
// allData[0]=U4 gui truoc (xa hon), allData[1]=U3 gui sau (gan Arduino hon)
void commit() {
  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLK_PIN, MSBFIRST, allData[0]);
  shiftOut(DATA_PIN, CLK_PIN, MSBFIRST, allData[1]);
  digitalWrite(LATCH_PIN, HIGH);
}


// ── Cap nhat man hinh qua MAX7219 (THAY THE HOAN TOAN ham runDisplayMux cu) ──
// Khong can tinh toan blank period, khong can bang ma CA[], khong can BC558.
// MAX7219 tu quet noi bo lien tuc sau khi ghi du lieu, khong bi anh huong
// du Arduino ban viec khac (vi du doc cam bien HC-SR04) trong vai chuc ms.
//
// lc.setDigit(addr, digitIndex, value, showDecimalPoint)
//   addr = 0 (MAX_LIGHT) hoac 1 (MAX_LEFT)
//   digitIndex = 0-7 (vi tri digit trong chip do)
//   value = 0-9 (MAX7219 tu giai ma BCD, khong can bang tra CA[])
//
// lc.setChar(addr, digitIndex, ' ', false) -> dung de hien thi trang (an so 0)
void updateDisplays() {
  int vLA = constrain(cntLeftA,  0, 99);
  int vLB = constrain(cntLeftB,  0, 99);
  int vCA = constrain(cntLightA, 0, 99);
  int vCB = constrain(cntLightB, 0, 99);

  // Chi ghi lai MAX7219 khi gia tri thay doi (giam SPI traffic khong can thiet)
  if (vLA != lastShownLeftA) {
    writeTwoDigit(MAX_LEFT, 0, 1, vLA);   // A1 LEFT & A2 LEFT dung chung digit 0,1
    writeTwoDigit(MAX_LEFT, 2, 3, vLA);
    lastShownLeftA = vLA;
  }
  if (vLB != lastShownLeftB) {
    writeTwoDigit(MAX_LEFT, 4, 5, vLB);   // B1 LEFT
    writeTwoDigit(MAX_LEFT, 6, 7, vLB);   // B2 LEFT
    lastShownLeftB = vLB;
  }
  if (vCA != lastShownLightA) {
    writeTwoDigit(MAX_LIGHT, 0, 1, vCA);  // A1 LIGHT
    writeTwoDigit(MAX_LIGHT, 2, 3, vCA);  // A2 LIGHT
    lastShownLightA = vCA;
  }
  if (vCB != lastShownLightB) {
    writeTwoDigit(MAX_LIGHT, 4, 5, vCB);  // B1 LIGHT
    writeTwoDigit(MAX_LIGHT, 6, 7, vCB);  // B2 LIGHT
    lastShownLightB = vCB;
  }
}


// ── Ham phu: ghi 1 gia tri 2 chu so vao 2 vi tri digit lien tiep ─────────────
// digitTens: vi tri hang chuc | digitUnits: vi tri hang don vi
// An so 0 o hang chuc neu gia tri < 10 (leading-zero suppression)
void writeTwoDigit(int addr, int digitTens, int digitUnits, int value) {
  int tens  = value / 10;
  int units = value % 10;

  if (value < 10) {
    lc.setChar(addr, digitTens, ' ', false);   // an hang chuc
  } else {
    lc.setDigit(addr, digitTens, tens, false);
  }
  lc.setDigit(addr, digitUnits, units, false);
}


// ── Serial status moi giay (giong het ban truoc) ──────────────────────────────
void printStatus() {
  Serial.print(mainTime);
  Serial.print("s | LA="); Serial.print(cntLeftA);
  Serial.print(" LB=");    Serial.print(cntLeftB);
  Serial.print(" | CA=");  Serial.print(cntLightA);
  Serial.print(" CB=");    Serial.println(cntLightB);
}
