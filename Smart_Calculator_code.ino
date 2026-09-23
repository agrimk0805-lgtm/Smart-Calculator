#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// XIAO ESP32-S3 Pin Config
#define OLED_SDA 5   // D4 pin
#define OLED_SCL 6   // D5 pin
#define VBAT_PIN 10  // Battery voltage measurement pin (GPIO 10 / D6)

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// XIAO ESP32-S3 Pins
byte rowPins[ROWS] = {1, 2, 3, 4};    // D0, D1, D2, D3
byte colPins[COLS] = {44, 7, 8, 9};   // D7, D8, D9, D10

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Security & Password System
const String CORRECT_PIN = "1388"; //Add Your Password
String enteredPin = "";
bool isLocked = true;

// Operating Modes
enum OperatingMode { MODE_NORMAL, MODE_CI, MODE_SI, MODE_GST, MODE_PL };
OperatingMode currentMode = MODE_NORMAL;

// Multi-step Input Enums
enum Step { STEP_1, STEP_2, STEP_3, STEP_4, STEP_RESULT };
Step currentStep = STEP_1;

// General Calculator Variables
String inputString = "";
String expressionTape = "";  
double firstNum = 0.0;
double secondNum = 0.0;
char op = ' ';
bool isResultShown = false;
bool isShiftActive = false;

// Financial & P&L Variables
double pVal = 0.0, rVal = 0.0, nVal = 0.0, tVal = 0.0;
double cpVal = 0.0, spVal = 0.0; 
double baseVal = 0.0, pctVal = 0.0; 
int plSubMode = 0; // 0 = P&L Mode, 1 = Percentage Mode

// GST & Discount Variables
double gstBasePrice = 0.0;
double gstDiscountVal = 0.0;
double gstTaxRate = 18.0; // Default GST Rate
int gstSubMode = 0;       // 0 = Add GST + Discount, 1 = Extract GST
bool isDiscountPct = true;// true = % discount, false = flat amount

// Timers & Animations
unsigned long lastKeyPressTime = 0;
const unsigned long DEBOUNCE_DELAY = 150;
const unsigned long IDLE_TIMEOUT = 10000; 
bool inEyeMode = false;
unsigned long lastAnimTime = 0;
int animState = 0;

// Function Declarations
void handlePasswordKey(char key);
void displayPasswordScreen();
void handleNormalModeKey(char key);
void handleCIModeKey(char key);
void handleSIModeKey(char key);
void handleGSTModeKey(char key);
void handlePLModeKey(char key);
void cycleMode();
void advanceCIStep();
void displayCIMenu();
void advanceSIStep();
void displaySIMenu();
void displayGSTMenu();
void displayPLMenu();
void setBinaryOp(char newOp);
void calculateResult();
void showUnaryResult(String label, double result);
void resetCalculator();
void refreshScreen();
void showError(String msg);
void updateDisplay(String text);

void setup() {
  Wire.begin(OLED_SDA, OLED_SCL);
  pinMode(VBAT_PIN, INPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println("XIAO ESP32-S3");
  display.println("Advanced Calculator");
  display.println("Initializing...");
  display.display();
  delay(1200);

  lastKeyPressTime = millis();
  displayPasswordScreen();
}

int getBatteryPercentage() {
  int raw = analogRead(VBAT_PIN);
  float voltage = (raw / 4095.0) * 3.3 * 2.0; 
  int pct = (int)((voltage - 3.2) / (4.2 - 3.2) * 100.0);
  return constrain(pct, 0, 100);
}

void drawBatteryIcon() {
  int pct = getBatteryPercentage();
  display.setTextSize(1);
  display.setCursor(95, 0);
  display.print(pct);
  display.print("%");
}

void handleEyeAnimation() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastAnimTime > 2000) {
    lastAnimTime = currentMillis;
    animState = (animState + 1) % 3;
  }

  display.clearDisplay();
  int eyeRadius = 16;
  int leftEyeX = 40, rightEyeX = 88, eyeY = 32;

  if (animState == 0) {
    display.drawLine(leftEyeX - 12, eyeY, leftEyeX + 12, eyeY, SSD1306_WHITE);
    display.drawLine(rightEyeX - 12, eyeY, rightEyeX + 12, eyeY, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(100, 15); display.print("z");
    display.setCursor(108, 10); display.print("Z");
    display.setCursor(116, 5);  display.print("Z");
  } 
  else if (animState == 1) {
    display.fillCircle(leftEyeX, eyeY, eyeRadius, SSD1306_WHITE);
    display.fillCircle(leftEyeX + 2, eyeY, 6, SSD1306_BLACK);
    display.drawLine(rightEyeX - 12, eyeY, rightEyeX + 12, eyeY, SSD1306_WHITE);
  } 
  else {
    display.fillCircle(leftEyeX, eyeY, eyeRadius, SSD1306_WHITE);
    display.fillCircle(rightEyeX, eyeY, eyeRadius, SSD1306_WHITE);
    display.fillCircle(leftEyeX - 2, eyeY - 2, 6, SSD1306_BLACK);
    display.fillCircle(rightEyeX - 2, eyeY - 2, 6, SSD1306_BLACK);
  }
  display.display();
}

void loop() {
  char key = keypad.getKey();

  if (millis() - lastKeyPressTime > IDLE_TIMEOUT) {
    inEyeMode = true;
    handleEyeAnimation();
  }

  if (key && (millis() - lastKeyPressTime > DEBOUNCE_DELAY)) {
    lastKeyPressTime = millis();

    if (inEyeMode) {
      inEyeMode = false;
      if (isLocked) displayPasswordScreen();
      else refreshScreen();
      return;
    }

    if (isLocked) {
      handlePasswordKey(key);
      return;
    }

    if (currentMode == MODE_NORMAL)      handleNormalModeKey(key);
    else if (currentMode == MODE_CI)     handleCIModeKey(key);
    else if (currentMode == MODE_SI)     handleSIModeKey(key);
    else if (currentMode == MODE_GST)    handleGSTModeKey(key);
    else if (currentMode == MODE_PL)     handlePLModeKey(key);
  }
}

// Password Verification Logic
void handlePasswordKey(char key) {
  if (key >= '0' && key <= '9') {
    if (enteredPin.length() < 8) {
      enteredPin += key;
      displayPasswordScreen();
    }
  } 
  else if (key == '#') { // Backspace / Clear
    if (enteredPin.length() > 0) {
      enteredPin.remove(enteredPin.length() - 1);
      displayPasswordScreen();
    }
  } 
  else if (key == '*') { // Enter / Submit
    if (enteredPin == CORRECT_PIN) {
      isLocked = false;
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(10, 20);
      display.println("UNLOCKED");
      display.display();
      delay(800);
      updateDisplay("0");
    } else {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 15);
      display.println("ACCESS");
      display.println("DENIED!");
      display.display();
      delay(1200);
      enteredPin = "";
      displayPasswordScreen();
    }
  }
}

void displayPasswordScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("[LOCKED]");
  drawBatteryIcon();

  display.setCursor(0, 15);
  display.println("Enter PIN:");

  // Mask PIN with asterisks
  String maskedPin = "";
  for (int i = 0; i < enteredPin.length(); i++) {
    maskedPin += "*";
  }

  display.setTextSize(2);
  display.setCursor(0, 32);
  display.print(maskedPin);

  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print("[*]=Enter  [#]=Del");
  display.display();
}

// 1. Normal Mode Key Handler
void handleNormalModeKey(char key) {
  if (key >= '0' && key <= '9') {
    if (isResultShown) {
      inputString = "";
      expressionTape = "";
      isResultShown = false;
    }
    if (inputString.length() < 10) {
      inputString += key;
      updateDisplay(inputString);
    }
  } 
  else if (key == '#') {
    if (isShiftActive) {
      if (inputString.length() > 0) inputString.remove(inputString.length() - 1);
      isShiftActive = false;
      updateDisplay(inputString.length() > 0 ? inputString : "0");
    } else {
      if (isResultShown) { inputString = "0"; expressionTape = ""; isResultShown = false; }
      if (inputString.indexOf('.') == -1) {
        if (inputString.length() == 0) inputString = "0";
        inputString += ".";
        updateDisplay(inputString);
      }
    }
  } 
  else if (key == 'A') {
    if (isShiftActive) {
      if (inputString.length() > 0) {
        double val = inputString.toDouble();
        if (val < 0) {
          showError("Err: Domain");
        } else {
          showUnaryResult("Sqrt", sqrt(val));
        }
      }
      isShiftActive = false;
    } else setBinaryOp('+');
  } 
  else if (key == 'B') {
    if (isShiftActive) {
      if (inputString.length() > 0) showUnaryResult("Cbrt", cbrt(inputString.toDouble()));
      isShiftActive = false;
    } else setBinaryOp('-');
  } 
  else if (key == 'C') {
    if (isShiftActive) { resetCalculator(); isShiftActive = false; }
    else setBinaryOp('*');
  } 
  else if (key == 'D') {
    if (isShiftActive) { setBinaryOp('/'); isShiftActive = false; }
    else setBinaryOp('^');
  } 
  else if (key == '*') {
    if (op != ' ' && inputString.length() > 0) {
      calculateResult();
      isShiftActive = false;
    } else {
      if (isShiftActive) {
        isShiftActive = false;
        cycleMode();
      } else {
        isShiftActive = true;
        updateDisplay(inputString.length() > 0 ? inputString : "0");
      }
    }
  }
}

void cycleMode() {
  if (currentMode == MODE_NORMAL)      currentMode = MODE_CI;
  else if (currentMode == MODE_CI)     currentMode = MODE_SI;
  else if (currentMode == MODE_SI)     currentMode = MODE_GST;
  else if (currentMode == MODE_GST)    currentMode = MODE_PL;
  else currentMode = MODE_NORMAL;
  
  currentStep = STEP_1;
  gstSubMode = 0;
  plSubMode = 0;
  isDiscountPct = true;
  inputString = "";
  expressionTape = "";
  isShiftActive = false;
  refreshScreen();
}

// 2. Compound Interest Mode Key Handler
void handleCIModeKey(char key) {
  if (key >= '0' && key <= '9') {
    if (inputString.length() < 10) { inputString += key; displayCIMenu(); }
  } else if (key == '#') {
    if (inputString.indexOf('.') == -1) { if (inputString.length() == 0) inputString = "0"; inputString += "."; displayCIMenu(); }
  } else if (key == 'A' || key == 'B' || key == 'D') {
    advanceCIStep();
  } else if (key == 'C') {
    currentStep = STEP_1; inputString = ""; displayCIMenu();
  } else if (key == '*') {
    cycleMode();
  }
}

void advanceCIStep() {
  double val = inputString.toDouble();
  inputString = "";
  if (currentStep == STEP_1) { pVal = val; currentStep = STEP_2; }
  else if (currentStep == STEP_2) { rVal = val; currentStep = STEP_3; }
  else if (currentStep == STEP_3) { 
    if (val == 0.0) val = 1.0;
    nVal = val; 
    currentStep = STEP_4; 
  }
  else if (currentStep == STEP_4) { tVal = val; currentStep = STEP_RESULT; }
  else if (currentStep == STEP_RESULT) { currentStep = STEP_1; }
  displayCIMenu();
}

void displayCIMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); display.print("[CI Mode]");
  drawBatteryIcon();
  display.setCursor(0, 12);

  if (currentStep == STEP_1) { display.println("Principal (P):"); display.setTextSize(2); display.setCursor(0, 30); display.print(inputString); }
  else if (currentStep == STEP_2) { display.println("Rate % (r):"); display.setTextSize(2); display.setCursor(0, 30); display.print(inputString); }
  else if (currentStep == STEP_3) { display.println("Periods/Yr (n):"); display.setTextSize(2); display.setCursor(0, 30); display.print(inputString); }
  else if (currentStep == STEP_4) { display.println("Years (t):"); display.setTextSize(2); display.setCursor(0, 30); display.print(inputString); }
  else if (currentStep == STEP_RESULT) {
    if (nVal == 0.0) nVal = 1.0;
    double amt = pVal * pow(1.0 + ((rVal / 100.0) / nVal), nVal * tVal);
    display.println("Total Amount (A):");
    display.setTextSize(2); display.setCursor(0, 26); display.print(amt, 2);
    display.setTextSize(1); display.setCursor(0, 50); display.print("Interest: "); display.print(amt - pVal, 2);
  }
  display.display();
}

// 3. Simple Interest Mode Key Handler
void handleSIModeKey(char key) {
  if (key >= '0' && key <= '9') {
    if (inputString.length() < 10) { inputString += key; displaySIMenu(); }
  } else if (key == '#') {
    if (inputString.indexOf('.') == -1) { if (inputString.length() == 0) inputString = "0"; inputString += "."; displaySIMenu(); }
  } else if (key == 'A' || key == 'B' || key == 'D') {
    advanceSIStep();
  } else if (key == 'C') {
    currentStep = STEP_1; inputString = ""; displaySIMenu();
  } else if (key == '*') {
    cycleMode();
  }
}

void advanceSIStep() {
  double val = inputString.toDouble();
  inputString = "";
  if (currentStep == STEP_1) { pVal = val; currentStep = STEP_2; }
  else if (currentStep == STEP_2) { rVal = val; currentStep = STEP_3; }
  else if (currentStep == STEP_3) { tVal = val; currentStep = STEP_RESULT; }
  else if (currentStep == STEP_RESULT) { currentStep = STEP_1; }
  displaySIMenu();
}

void displaySIMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); display.print("[SI Mode]");
  drawBatteryIcon();
  display.setCursor(0, 12);

  if (currentStep == STEP_1) { display.println("Principal (P):"); display.setTextSize(2); display.setCursor(0, 30); display.print(inputString); }
  else if (currentStep == STEP_2) { display.println("Rate % (r):"); display.setTextSize(2); display.setCursor(0, 30); display.print(inputString); }
  else if (currentStep == STEP_3) { display.println("Years (t):"); display.setTextSize(2); display.setCursor(0, 30); display.print(inputString); }
  else if (currentStep == STEP_RESULT) {
    double interest = (pVal * rVal * tVal) / 100.0;
    display.println("Simple Interest:");
    display.setTextSize(2); display.setCursor(0, 26); display.print(interest, 2);
    display.setTextSize(1); display.setCursor(0, 50); display.print("Total: "); display.print(pVal + interest, 2);
  }
  display.display();
}

// 4. GST & Discount Mode Key Handler
void handleGSTModeKey(char key) {
  if (key >= '0' && key <= '9') {
    if (inputString.length() < 10) { inputString += key; displayGSTMenu(); }
  } else if (key == '#') {
    if (inputString.indexOf('.') == -1) { if (inputString.length() == 0) inputString = "0"; inputString += "."; displayGSTMenu(); }
  } else if (key == 'A') {
    gstSubMode = 0;
    double val = inputString.toDouble();
    inputString = "";
    if (currentStep == STEP_1) { gstBasePrice = val; currentStep = STEP_2; }
    else if (currentStep == STEP_2) { gstDiscountVal = val; currentStep = STEP_3; }
    else if (currentStep == STEP_3) { 
      if (val > 0) gstTaxRate = val; 
      currentStep = STEP_RESULT; 
    }
    else if (currentStep == STEP_RESULT) { currentStep = STEP_1; }
    displayGSTMenu();
  } else if (key == 'B') {
    gstSubMode = 1;
    double val = inputString.toDouble();
    inputString = "";
    if (currentStep == STEP_1) { gstBasePrice = val; currentStep = STEP_2; }
    else if (currentStep == STEP_2) { 
      if (val > 0) gstTaxRate = val; 
      currentStep = STEP_RESULT; 
    }
    else if (currentStep == STEP_RESULT) { currentStep = STEP_1; }
    displayGSTMenu();
  } else if (key == 'D') {
    if (currentStep == STEP_2 && gstSubMode == 0) {
      isDiscountPct = !isDiscountPct;
      displayGSTMenu();
    }
  } else if (key == 'C') {
    currentStep = STEP_1; inputString = ""; displayGSTMenu();
  } else if (key == '*') {
    cycleMode();
  }
}

void displayGSTMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); 
  display.print(gstSubMode == 0 ? "[GST + Disc]" : "[Extract GST]");
  drawBatteryIcon();
  display.setCursor(0, 12);

  if (gstSubMode == 0) {
    if (currentStep == STEP_1) {
      display.println("Enter Base Price:");
      display.setTextSize(2); display.setCursor(0, 30); display.print(inputString);
      display.setTextSize(1); display.setCursor(0, 52); display.print("Press [A] for Next");
    } else if (currentStep == STEP_2) {
      display.print("Discount ("); display.print(isDiscountPct ? "%" : "Amt"); display.print("):");
      display.setTextSize(2); display.setCursor(0, 30); display.print(inputString);
      display.setTextSize(1); display.setCursor(0, 52); display.print("[D]Toggle %/Amt [A]>");
    } else if (currentStep == STEP_3) {
      display.println("Enter GST %:");
      display.setTextSize(2); display.setCursor(0, 30); display.print(inputString.length() > 0 ? inputString : String(gstTaxRate, 1)); display.print("%");
      display.setTextSize(1); display.setCursor(0, 52); display.print("Press [A] for Result");
    } else if (currentStep == STEP_RESULT) {
      double discAmount = isDiscountPct ? (gstBasePrice * (gstDiscountVal / 100.0)) : gstDiscountVal;
      double discountedPrice = gstBasePrice - discAmount;
      if (discountedPrice < 0) discountedPrice = 0;
      double gstAmount = discountedPrice * (gstTaxRate / 100.0);
      double finalTotal = discountedPrice + gstAmount;

      display.print("Net: "); display.print(discountedPrice, 1);
      display.print(" Tax: "); display.print(gstAmount, 1);
      display.setTextSize(2); display.setCursor(0, 26); display.print(finalTotal, 2);
      display.setTextSize(1); display.setCursor(0, 50); 
      display.print("Disc: -"); display.print(discAmount, 1);
      display.print(" ("); display.print(gstTaxRate, 1); display.print("% GST)");
    }
  } else {
    if (currentStep == STEP_1) {
      display.println("Inclusive Price:");
      display.setTextSize(2); display.setCursor(0, 30); display.print(inputString);
      display.setTextSize(1); display.setCursor(0, 52); display.print("Press [B] for Next");
    } else if (currentStep == STEP_2) {
      display.println("Enter GST %:");
      display.setTextSize(2); display.setCursor(0, 30); display.print(inputString.length() > 0 ? inputString : String(gstTaxRate, 1)); display.print("%");
      display.setTextSize(1); display.setCursor(0, 52); display.print("Press [B] for Result");
    } else if (currentStep == STEP_RESULT) {
      double basePrice = gstBasePrice / (1.0 + (gstTaxRate / 100.0));
      double gstPortion = gstBasePrice - basePrice;

      display.println("Base (Excl Tax):");
      display.setTextSize(2); display.setCursor(0, 26); display.print(basePrice, 2);
      display.setTextSize(1); display.setCursor(0, 50); display.print("GST Portion: "); display.print(gstPortion, 2);
    }
  }
  display.display();
}

// 5. Profit, Loss & Percentage Mode Key Handler
void handlePLModeKey(char key) {
  if (key >= '0' && key <= '9') {
    if (inputString.length() < 10) { inputString += key; displayPLMenu(); }
  } else if (key == '#') {
    if (inputString.indexOf('.') == -1) { if (inputString.length() == 0) inputString = "0"; inputString += "."; displayPLMenu(); }
  } else if (key == 'A') { 
    plSubMode = 0;
    double val = inputString.toDouble();
    inputString = "";
    if (currentStep == STEP_1) { cpVal = val; currentStep = STEP_2; }
    else if (currentStep == STEP_2) { spVal = val; currentStep = STEP_RESULT; }
    else if (currentStep == STEP_RESULT) { currentStep = STEP_1; }
    displayPLMenu();
  } else if (key == 'B') { 
    plSubMode = 1;
    double val = inputString.toDouble();
    inputString = "";
    if (currentStep == STEP_1) { baseVal = val; currentStep = STEP_2; }
    else if (currentStep == STEP_2) { pctVal = val; currentStep = STEP_RESULT; }
    else if (currentStep == STEP_RESULT) { currentStep = STEP_1; }
    displayPLMenu();
  } else if (key == 'C') { 
    currentStep = STEP_1; inputString = ""; displayPLMenu();
  } else if (key == '*') {
    cycleMode();
  }
}

void displayPLMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); 
  display.print(plSubMode == 0 ? "[P&L Mode]" : "[Pct % Mode]");
  drawBatteryIcon();
  display.setCursor(0, 12);

  if (plSubMode == 0) { 
    if (currentStep == STEP_1) {
      display.println("Cost Price (CP):");
      display.setTextSize(2); display.setCursor(0, 30); display.print(inputString);
      display.setTextSize(1); display.setCursor(0, 52); display.print("Press [A] for Next");
    } else if (currentStep == STEP_2) {
      display.println("Selling Price (SP):");
      display.setTextSize(2); display.setCursor(0, 30); display.print(inputString);
      display.setTextSize(1); display.setCursor(0, 52); display.print("Press [A] for Result");
    } else if (currentStep == STEP_RESULT) {
      double diff = spVal - cpVal;
      if (diff >= 0) {
        double profitPct = (cpVal != 0) ? (diff / cpVal) * 100.0 : 0.0;
        display.println("PROFIT:");
        display.setTextSize(2); display.setCursor(0, 26); display.print("+"); display.print(diff, 2);
        display.setTextSize(1); display.setCursor(0, 50); display.print("Profit %: "); display.print(profitPct, 2); display.print("%");
      } else {
        double lossAmt = abs(diff);
        double lossPct = (cpVal != 0) ? (lossAmt / cpVal) * 100.0 : 0.0;
        display.println("LOSS:");
        display.setTextSize(2); display.setCursor(0, 26); display.print("-"); display.print(lossAmt, 2);
        display.setTextSize(1); display.setCursor(0, 50); display.print("Loss %: "); display.print(lossPct, 2); display.print("%");
      }
    }
  } else { 
    if (currentStep == STEP_1) {
      display.println("Enter Base Number:");
      display.setTextSize(2); display.setCursor(0, 30); display.print(inputString);
      display.setTextSize(1); display.setCursor(0, 52); display.print("Press [B] for Next");
    } else if (currentStep == STEP_2) {
      display.println("Enter Percent (%):");
      display.setTextSize(2); display.setCursor(0, 30); display.print(inputString); display.print("%");
      display.setTextSize(1); display.setCursor(0, 52); display.print("Press [B] for Result");
    } else if (currentStep == STEP_RESULT) {
      double resultPct = (baseVal * pctVal) / 100.0;
      display.print(pctVal, 1); display.print("% of "); display.print(baseVal, 1);
      display.setTextSize(2); display.setCursor(0, 28); display.print("="); display.print(resultPct, 2);
    }
  }
  display.display();
}

void setBinaryOp(char newOp) {
  if (inputString.length() > 0) {
    firstNum = inputString.toDouble();
    op = newOp;
    expressionTape = inputString + op;
    inputString = "";
    updateDisplay(String(op));
  }
}

void calculateResult() {
  if (inputString.length() > 0 && op != ' ') {
    secondNum = inputString.toDouble();
    expressionTape += inputString;
    double result = 0.0;

    if (op == '+') result = firstNum + secondNum;
    else if (op == '-') result = firstNum - secondNum;
    else if (op == '*') result = firstNum * secondNum;
    else if (op == '^') result = pow(firstNum, secondNum);
    else if (op == '/') {
      if (secondNum != 0.0) result = firstNum / secondNum;
      else { showError("Err: Div by 0"); return; }
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0); display.print(expressionTape);
    drawBatteryIcon();

    display.setTextSize(2); display.setCursor(0, 30); display.print("="); display.print(result, 2);
    display.display();

    inputString = String(result, 4);
    expressionTape = "";
    op = ' ';
    isResultShown = true;
  }
}

void showUnaryResult(String label, double result) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); display.print(label + "(" + inputString + ")");
  drawBatteryIcon();

  display.setTextSize(2); display.setCursor(0, 30); display.print("="); display.print(result, 2);
  display.display();

  inputString = String(result, 4);
  isResultShown = true;
}

void resetCalculator() {
  inputString = "";
  expressionTape = "";
  firstNum = 0.0; 
  secondNum = 0.0; 
  op = ' ';
  isResultShown = false;
  isShiftActive = false;
  refreshScreen();
}

void refreshScreen() {
  if (currentMode == MODE_NORMAL) updateDisplay(inputString.length() > 0 ? inputString : "0");
  else if (currentMode == MODE_CI)  displayCIMenu();
  else if (currentMode == MODE_SI)  displaySIMenu();
  else if (currentMode == MODE_GST) displayGSTMenu();
  else if (currentMode == MODE_PL)  displayPLMenu();
}

void showError(String msg) {
  display.clearDisplay();
  display.setTextSize(2); 
  display.setCursor(0, 20); 
  display.println(msg);
  display.display();
  
  inputString = ""; 
  expressionTape = "";
  firstNum = 0.0;
  secondNum = 0.0;
  op = ' ';
  isResultShown = true;
  delay(1200);
  refreshScreen();
}

void updateDisplay(String text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  if (expressionTape.length() > 0) display.print(expressionTape);
  else display.print(isShiftActive ? "[S]" : "[NORM]");

  drawBatteryIcon();

  display.setTextSize(2);
  display.setCursor(0, 30);
  display.print(text);
  display.display();
}
