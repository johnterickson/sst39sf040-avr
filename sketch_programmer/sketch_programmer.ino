
/*
 * 19 Address lines A0-A18
 * 7 data lines D0-D7
 * CE# always LOW
 * OE#
 * WE#
 * 
 * Total: 28 wires
 * 
 * Arduino 1 (control): 
 *  D0,D1 UART
 *  D2-D17 data in and out
 *  D18, D19 I2C
 *  
 * Arduino 2 (extra io):
 *  D0,D1 UART
 *  D2-D17 data-outputs -> A0->A15
 *  D18, D19 I2C
 */

const int
  DQ0 = 2,
  DQ7 = 9,
  CLK = 10,
  ADDR0=11,
  ADDR8=12,
  ADDR16=13,
  CE = 14,
  OE = 15,
  WE = 16;

void step() {
  //delay(1);
  //Serial.print("waiting...");
  //while('\n' != Serial.read());
  //Serial.println("resuming.");
}

void initOutput(int pin, int initValue) {
  if (initValue != 0) {
    pinMode(pin, INPUT_PULLUP);
  } else {
    pinMode(pin, INPUT);
  }

  pinMode(pin, OUTPUT);
}

unsigned long lastAddr = -1;

void setAddr(unsigned long addr) {
  if (addr == lastAddr) return;
  
  digitalWrite(CLK, LOW);
  for (int i=0; i<8; i++) {
    digitalWrite(ADDR0, (addr >> (7-i)) & 0x1);
    digitalWrite(ADDR8, (addr >> (8+(7-i))) & 0x1);
    digitalWrite(ADDR16, (addr >> (16+(7-i))) & 0x1);
    digitalWrite(CLK, HIGH) ;
    //delay(1);
    digitalWrite(CLK, LOW);
  }
  
}

void inputMode() {
  for (int i=0; i<8; i++) {
    pinMode(DQ0+i, INPUT);
  }
}

byte readDQ() {
  inputMode();
  byte b = 0;
  for (int i=0; i<8; i++) {
    b |= (digitalRead(DQ0+i) << i);
  }
  return b;
}

void setDQ(byte b) {
  for (int i=0; i<8; i++) {
    initOutput(DQ0+i, (b>>i) & 0x1);
  }
}

byte readByte(unsigned long a) {
  inputMode();
  digitalWrite(OE, HIGH);
  digitalWrite(WE, HIGH);
  digitalWrite(CE, LOW);
  delayMicroseconds(1);
  setAddr(a);

  //delay(2000);

  step();
  digitalWrite(OE, LOW);
  delayMicroseconds(1); // 70 ns
  
  //delay(2000);

  step();
  byte b = readDQ();

  step();
  digitalWrite(OE, HIGH);
  step();
  
  //delay(2000);
  
  delayMicroseconds(1); // 70 ns
  //setAddr(0);
  digitalWrite(CE, HIGH);
  delayMicroseconds(1);
  return b;
}

void writeByte(unsigned long a, byte b) {
  digitalWrite(OE, HIGH);
  digitalWrite(WE, HIGH);
  setAddr(a);
  setDQ(b);
  delayMicroseconds(1); // 40 ns

  //delay(2000);

  step();
  digitalWrite(WE, LOW);
  step();
  
  //delay(2000);
  
  delayMicroseconds(1); // 40 ns
  digitalWrite(WE, HIGH);
  step();

  //delay(2000);
  
  //setAddr(0);
  delayMicroseconds(1); // TDH 0 ns
}

void programByte(unsigned long a, byte b) {
  digitalWrite(CE, HIGH);
  digitalWrite(OE, HIGH);
  digitalWrite(WE, HIGH);
  digitalWrite(CE, LOW);
  delayMicroseconds(1);
  writeByte(0x5555UL, 0xAA);
  writeByte(0x2AAAUL, 0x55); 
  writeByte(0x5555UL, 0xA0);
  writeByte(a, b);
  delayMicroseconds(30); // TBP 20 us
  inputMode();
  //setAddr(0);
  digitalWrite(CE, HIGH);
  delayMicroseconds(1);
}

void erase() {
  digitalWrite(OE, HIGH);
  digitalWrite(WE, HIGH);
  digitalWrite(CE, LOW);
  delayMicroseconds(1);
  writeByte(0x5555UL, 0xAA);
  writeByte(0x2AAAUL, 0x55);
  writeByte(0x5555UL, 0x80);
  writeByte(0x5555UL, 0xAA);
  writeByte(0x2AAAUL, 0x55);
  writeByte(0x5555UL, 0x10);
  delay(150); // TSCE 100 ms
  inputMode();
  //setAddr(0);
  digitalWrite(CE, HIGH);
  delayMicroseconds(1);
}

void enterId() {
  step();
  digitalWrite(CE, HIGH);
  digitalWrite(OE, HIGH);
  digitalWrite(WE, HIGH);
  step();
  digitalWrite(CE, LOW);
  delayMicroseconds(1);
  writeByte(0x5555UL, 0xAA);
  writeByte(0x2AAAUL, 0x55);
  //writeByte(0xAAA2UL, 0x55);
  writeByte(0x5555UL, 0x90);
  //writeByte(0x5555UL, 0x09);
  digitalWrite(CE, HIGH);
  delayMicroseconds(1); // TIDA 150 ns
  inputMode();
  //setAddr(0);
}

void exitId() {
  digitalWrite(CE, HIGH);
  digitalWrite(OE, HIGH);
  digitalWrite(WE, HIGH);
  digitalWrite(CE, LOW);
  delayMicroseconds(1);
  writeByte(0x5555UL, 0xF0);
  delayMicroseconds(1); // TIDA 150 ns
  inputMode();
  //setAddr(0);
  digitalWrite(CE, HIGH);
  delayMicroseconds(1);
}

void setup() {
  Serial.begin(115200);
  inputMode();

  initOutput(CE, HIGH);
  initOutput(OE, HIGH);
  initOutput(WE, HIGH);

  initOutput(CLK, LOW);
  initOutput(ADDR0, LOW);
  initOutput(ADDR8, LOW);
  initOutput(ADDR16, LOW);
  setAddr(0);

  delay(1000);
  
  Serial.println("READY");
  Serial.flush();
}

unsigned long addr = 0x0;
char line[10] = {0};
int charIndex = 0;

void processLine() {
  //Serial.print(line);
  //Serial.print(' ');
  switch (line[0]) {
    case 'i':
      {
        enterId();
        Serial.print("ManufacturerId:");
        byte manId = readByte(0);
        Serial.print(manId, HEX);
        Serial.print(" DeviceId:");
        byte devId = readByte(1);
        Serial.println(devId, HEX);
        exitId();
      }
      break;
    case 'a':
      Serial.println(addr, HEX);
      break;
    case 'e':
      erase();
      addr = 0;
      Serial.println(readByte(addr), HEX);
      break;
    case 'r':
      Serial.println(readByte(addr), HEX);
      break;
    case 'w':
      int b;
      sscanf(&line[1], "%x", &b);
      programByte(addr,(byte)b);
      Serial.println(readByte(addr), HEX);
      addr += 1;
      break;
  }
  Serial.flush();
}

void loop()
{  
  while(Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line[charIndex] = '\0';
      processLine();
      line[0] = '\0';
      charIndex = 0;
    } else {
      line[charIndex] = c;
      charIndex += 1;
    }
  }
  delay(1);
}
