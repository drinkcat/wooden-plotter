// Wooden Plotter
// Copyright (C) 2015 Nicolas Boichat
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <Servo.h>
#include <avr/pgmspace.h>
#include "calib.h"
#include "glyphs.h"

#undef DEBUG_SERIAL
//#define DEBUG_SERIAL 1

#ifdef DEBUG_SERIAL
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTln(x) Serial.println(x)
#else
#define DEBUG_PRINT(x) do {} while(0)
#define DEBUG_PRINTln(x) do {} while(0)
#endif

#define STATUS_PRINT(x) do { if (Serial) Serial.print(x); } while(0)
#define STATUS_PRINTln(x) do { if (Serial) Serial.println(x); } while(0)

/* Globals */
Servo servo[3]; /* Left, right, cradle */
#define wifiSerial Serial1
char last[64] = "-1"; /* last wifi time */
unsigned long lastplot = 0; /* Last time anything was plotted */

/* These should be configuration items */
/* Compensation for incorrect servo angles, in deci-degrees */
const unsigned long plotdelay = 5*60; /* Plot time every 5 minutes */

#include "config.h"

void setup() {
    Serial.begin(9600);
#ifdef DEBUG_SERIAL
    while (!Serial) {
      ; // wait for serial port to connect. Needed for Leonardo only
    }
#endif
  
  plotter_setup();
  
  if (!wifi_setup()) {
    STATUS_PRINTln("wifi error");
  }
  
  /* Draw surrounding rectangle */
  rect(0, (LEN_X-1)*STEP, 0, (LEN_Y-1)*STEP);
}

void loop() {
  STATUS_PRINTln("loop");
  
  if (!wifi_fetch()) {
    STATUS_PRINTln("fetch error");
    if (!wifi_setup())
      STATUS_PRINTln("setup error");    
  }
  for (int i = 0; i < 30; i++)
    delay(1000);
}

/** WIFI commands **/

bool wifi_cmdok(const char* cmd, long timeout=1000) {
    wifiSerial.println(cmd);
    long start = millis();
    while (millis() - start < timeout) {
      if (wifiSerial.find("OK")) {
        /* Trim new lines */
        return wifiSerial.find("\n");
      }
    }
    return false;
}

int wifi_cmdstring(const char* cmd, char* buffer, int len, long timeout=3000) {
  int pos = 0;
  wifiSerial.println(cmd);
  long start = millis();
  while (millis() - start < timeout) {
    if (pos+1 >= len) {
      buffer[pos] = 0;
      return pos;
    }
    int cnt = wifiSerial.readBytesUntil('\n', buffer+pos, len-pos-1);
    if (cnt <= 0)
      continue;

    if (!strncmp(buffer+pos, "OK", 2)) {
      buffer[pos] = 0;
      return pos;
    }

    buffer[pos+cnt] = '\n';
    pos += cnt+1;
/*    DEBUG_PRINT("[");
    DEBUG_PRINT(pos);
    DEBUG_PRINT("/");
    DEBUG_PRINT(cnt);
    DEBUG_PRINT("]");
    DEBUG_PRINTln(buffer);*/
  }
  return -1;
}

void wifi_flush() {
    /* Flush buffer */
    while (wifiSerial.available() > 0) {
      wifiSerial.read();
    }
}

bool wifi_setup() {
    STATUS_PRINTln("wifi setup");
  
    char buffer[128] = {0};
    wifiSerial.begin(115200);
    wifiSerial.setTimeout(1000);
    
    STATUS_PRINTln("WiFi setup");
    
    wifi_flush();
    STATUS_PRINTln("Restart");    
    if (!wifi_cmdok("AT+RST")) {
      return false;
    }
    delay(2000);
    wifi_flush();
    
    STATUS_PRINTln("Kick");
    if (!wifi_cmdok("AT")) {
      return false;
    }
    
    if (wifi_cmdstring("AT+GMR", buffer, sizeof(buffer)) < 0) {
      return false;
    }
    STATUS_PRINT("Version");
    STATUS_PRINTln(buffer);

    STATUS_PRINTln("Sta mode");
    if (!wifi_cmdok("AT+CWMODE=1")) {
      return false;
    }

    STATUS_PRINTln("Connecting...");
    snprintf(buffer, sizeof(buffer), "AT+CWJAP=\"%s\",\"%s\"", SSID, PASSWORD);
    DEBUG_PRINTln(buffer);

    if (!wifi_cmdok(buffer, 10000)) {
      return false;
    }

    STATUS_PRINTln("Connected!");
    return true;
}

bool wifi_fetch() {
  char buffer[512] = {0};
  char buffercmd[16] = {0};
  
  STATUS_PRINTln("Fetching data...");
  snprintf(buffer, sizeof(buffer), "AT+CIPSTART=\"TCP\",\"%s\",80", HOST);
  if (!wifi_cmdok(buffer, 5000))
    return false;
  
  DEBUG_PRINTln("Sending HTTP");
  snprintf(buffer, sizeof(buffer), "GET /raw?tz=%s&device=%s&last=%s HTTP/1.0\r\nHost: %s\r\n\r\n", TZ, DEVICE, last, HOST);
  snprintf(buffercmd, sizeof(buffercmd), "AT+CIPSEND=%d", strlen(buffer));  
  if (!wifi_cmdok(buffercmd, 5000))
    return false;
  DEBUG_PRINTln(buffer);
  wifiSerial.print(buffer);

  long timeout = 3000;
  long start = millis();
  int len;
  while (true) {
    if (millis() - start >= timeout)
      return false;
      
    if (wifiSerial.find("+IPD,"))
      break;
  }

  len = wifiSerial.readBytesUntil(':', buffer, sizeof(buffer));
  if (len < 1)
    return false;
  buffer[len] = 0;
  int totlen = atoi(buffer);
  
  len = wifiSerial.readBytesUntil('\n', buffer, sizeof(buffer));
  if (len <= 8 || strncmp(buffer+len-3, "OK", 2)) {
    DEBUG_PRINTln(buffer);
    DEBUG_PRINTln("Not HTTP OK");
    return false;
  }
  totlen -= len+1;

  while (totlen > 0) {
    len = wifiSerial.readBytesUntil('\n', buffer, sizeof(buffer));
    if (len <= 0)
      return false;
    totlen -= len+1;
    buffer[len] = 0;
    DEBUG_PRINT("data/");
    DEBUG_PRINT(len);
    DEBUG_PRINT("/");
    DEBUG_PRINTln(buffer);
    if (len == 1 && buffer[len-1] == '\r') {
      break;
    }
  }
  
  /* Parse command */
  /* Time */
  len = wifiSerial.readBytesUntil(';', buffer, sizeof(buffer));
  if (len > 0) {
    buffer[len] = 0;
    strncpy(last, buffer, sizeof(last));
    STATUS_PRINT("Time:");
    STATUS_PRINTln(last);
  }
  
  /* Type */
  len = wifiSerial.readBytesUntil(';', buffer, sizeof(buffer));
  if (len <= 0) return false;
  buffer[len] = 0;

  if (!strcmp(buffer, "clock")) {
    STATUS_PRINT("clock:");
    len = wifiSerial.readBytesUntil('\n', buffer, sizeof(buffer));
    if (len <= 5) return false;
    buffer[5] = 0;
    STATUS_PRINT(buffer);
    if (lastplot == 0 || (millis()-lastplot)/1000 > plotdelay) {
      STATUS_PRINTln(" => Plotting...");
      text(buffer, 0, (LEN_X-1)*STEP, 0, (LEN_Y-1)*STEP);
      lastplot = millis();
    } else {
      STATUS_PRINT(" => Not plotting (");
      STATUS_PRINT((millis()-lastplot)/1000);    
      STATUS_PRINTln(")");
    }
    return true;
  } else if (!strcmp(buffer, "text")) {
    STATUS_PRINT("text:");
    len = wifiSerial.readBytesUntil('\n', buffer, sizeof(buffer));
    if (len <= 0) return false;
    buffer[len] = 0;
    STATUS_PRINTln(buffer);
    /* Plot the text, as big as possible */
    text(buffer, 0, (LEN_X-1)*STEP, 0, (LEN_Y-1)*STEP);
    lastplot = millis();
    return true;
  } else {
    STATUS_PRINT("Invalid command: ");
    STATUS_PRINTln(buffer);
  }
  return false;
}

/* Plotter functions */

void plotter_setup() {
  servo[0].attach(8); //Left
  servo[1].attach(9); //Right
  servo[2].attach(7); //Cradle
/*
  while(true) {
    servo[0].write(0); delay(200);
    servo[1].write(0); delay(200);
    servo[2].write(0); delay(200);
    delay(2000);
    servo[0].write(180); delay(200);
    servo[1].write(180); delay(200);
    servo[2].write(180); delay(200);
    delay(2000);
    servo[0].write(90); delay(200);
    servo[1].write(90); delay(200);
    servo[2].write(90); delay(200);
    delay(2000);
  }*/

  lift(true);
  delay(1000);
  servoWrite(0, 900);
  servoWrite(1, 900);
  delay(2000);
  servo_rest(true);
}

void servoWrite(int index, int value) {
  if(value < 0) value = 0;
  if(value > 1800) value = 1800;
  value = map(value, 0-servoComp[index], 1800-servoComp[index], 544, 2400);      
  servo[index].writeMicroseconds(value);
}

float posx = 0.0, posy = 0.0;

const int stepdelay=10; /* 10ms delay between steps */

float interpolate(const int angle[LEN_Y][LEN_X], float nx, float ny) {
  int ix1 = floor(nx/STEP);
  int iy1 = floor(ny/STEP);
  ix1 = min(max(0, ix1), LEN_X-2);
  iy1 = min(max(0, iy1), LEN_Y-2);
  int ix2 = ix1+1;
  int iy2 = iy1+1;
  
  float dx = nx/STEP-ix1;
  float dy = ny/STEP-iy1;
  /* Sanity */
  dx = min(max(0.0, dx), 1.0);
  dy = min(max(0.0, dy), 1.0);
  
#ifdef DEBUG_SERIAL 
/*  DEBUG_PRINT("/ix:");
  DEBUG_PRINT(ix1);
  DEBUG_PRINT("-");
  DEBUG_PRINT(iy1);
  DEBUG_PRINT("/");
  DEBUG_PRINT(ix2);
  DEBUG_PRINT("-");
  DEBUG_PRINT(iy2);
  DEBUG_PRINT("/");
  DEBUG_PRINT(dx);
  DEBUG_PRINT("-");
  DEBUG_PRINT(dy);
  DEBUG_PRINTln();*/
#endif

  int x1y1 = pgm_read_word_near(&(angle[iy1][ix1]));
  int x1y2 = pgm_read_word_near(&(angle[iy2][ix1]));
  int x2y1 = pgm_read_word_near(&(angle[iy1][ix2]));
  int x2y2 = pgm_read_word_near(&(angle[iy2][ix2]));
  
  /* Double interpolation for constant y */
  float a1 = (1.0-dx)*x1y1 + dx*x2y1;
  float a2 = (1.0-dx)*x1y2 + dx*x2y2;
  float a  = (1.0-dy)*a1 + dy*a2;

#ifdef DEBUG_SERIAL 
/*  DEBUG_PRINT("///");
  DEBUG_PRINT(x1y1);
  DEBUG_PRINT("-");
  DEBUG_PRINT(x1y2);
  DEBUG_PRINT("/");
  DEBUG_PRINT(x2y1);
  DEBUG_PRINT("-");
  DEBUG_PRINT(x2y2);
  DEBUG_PRINT("=");
  DEBUG_PRINT(a1);
  DEBUG_PRINT("/");
  DEBUG_PRINT(a2);
  DEBUG_PRINT("/");
  DEBUG_PRINT(a);
  DEBUG_PRINTln();*/
#endif
 
  return round(a);
}

/* feed in mm/sec */
void gopos(const float x, const float y, const float feed=50.0) {
  float distance = sqrt((x-posx)*(x-posx) + (y-posy)*(y-posy));
  int steps = (1000*distance/feed)/stepdelay;
  if (steps < 1) steps = 1;
  
#ifdef DEBUG_SERIAL
/*  DEBUG_PRINT("gopos:");
  DEBUG_PRINT(x);
  DEBUG_PRINT("/");
  DEBUG_PRINT(y);
  DEBUG_PRINT("///distance:");
  DEBUG_PRINT(distance);
  DEBUG_PRINT("/");
  DEBUG_PRINT(steps);
  DEBUG_PRINTln();*/
#endif
  
  for (int i = 1; i <= steps; i++) {
    float nx = (posx*(steps-i) + x*i) / steps;
    float ny = (posy*(steps-i) + y*i) / steps;

    /*DEBUG_PRINT("n:");
    DEBUG_PRINT(nx);
    DEBUG_PRINT("/");
    DEBUG_PRINT(ny);*/
    
    /* Interpolate */
    int a1 = interpolate(ANGLE1, nx, ny);
    int a2 = interpolate(ANGLE2, nx, ny);
    servoWrite(0, a1);
    servoWrite(1, a2);
    delay(stepdelay);
  }
    
  posx = x;
  posy = y;
  
  /*DEBUG_PRINT("gopos:");
  DEBUG_PRINT(x);
  DEBUG_PRINT("/");
  DEBUG_PRINT(y);
  DEBUG_PRINT("---");
  DEBUG_PRINT(alpha1);
  DEBUG_PRINT("/");
  DEBUG_PRINT(alpha2);
  DEBUG_PRINTln();*/
  //servo[0].write((+5)/10);
  //servo[1].write((+5)/10);
  //delay(500);
}

boolean lifted = true;

void lift(boolean yes) {
   if (yes) {
       int base = lifted ? 90 : 180;
       servo[2].write(base);
       servo[2].attach(7);
       for (; base > 90; base--) {
         servo[2].write(base);
         delay(5);
       }
       servo[2].write(90);
       delay(500);
   } else {
       int base = lifted ? 90 : 180;
       for (; base < 180; base++) {
         servo[2].write(base);
         delay(5);
       }
       servo[2].write(180);
       delay(500);
       servo[2].detach();
   }
   lifted = yes;
}

boolean resting = false;

void servo_rest(boolean yes) {
  if ((yes && resting) || (!yes && !resting)) {
    return;
  }
  
  if (yes) {
    /* Start by going to left-top corner */
    lift(true);
    delay(500);
    gopos(0, 0);
    int a1 = interpolate(ANGLE1, 0, 0);
    int a2 = interpolate(ANGLE2, 0, 0);
    
    servo[0].write(5);
    servo[1].write(60);
    delay(3000);
    lift(false);
    
    servo[0].detach();
    servo[1].detach();
  } else {
    servo[0].attach(8); //Left
    servo[1].attach(9); //Right
    delay(500);
    lift(true);
    delay(500);
    gopos(0.1, 0.1);   
  }
  
  resting = yes;
}

/* Draw rectangle, then rest */
void rect(float x1, float x2, float y1, float y2) {
  int i, j;
  i = x1;
  servo_rest(false);
  lift(true);
  gopos(x1, y1);
  delay(100);
  lift(false);
  gopos(x1, y2);
  gopos(x2, y2);
  gopos(x2, y1);
  gopos(x1, y1);
  servo_rest(true);
}

/* Draw circle, then rest */
void circle(float x, float y, float r) {
  int angle = 0;
  servo_rest(false);
  lift(true);
  gopos(x, y+r);
  delay(100);
  lift(false);
  for (float angle = 0; angle < 360; angle++) {
    float x1 = x+r*sin(angle*PI/180);
    float y1 = y+r*cos(angle*PI/180);
    gopos(x1, y1);
  }
  servo_rest(true);
}

int chrtopos(char chr) {
  int pos = chr-32;
  if (pos > 95 || pos < 0) pos = '?'-32;
  return pos;
}

/* Draw letter (no rest) */
/* h is maximum height, i.e. letter writes from y-h to y+h */
float letter(char chr, float x, float y, float h) {
  int pos = chrtopos(chr);
  const signed char* glyph = GLYPHS[pos];
  
  float scale = h/16.0;
  
  signed char nx1 = pgm_read_byte(glyph);
  signed char nx2 = pgm_read_byte(glyph+1);
  
  glyph += 2;
  
  float bx = -nx1*scale;
  
#ifdef DEBUG_SERIAL 
  DEBUG_PRINT("/");
  DEBUG_PRINT(nx1);
  DEBUG_PRINT("-");
  DEBUG_PRINT(nx2);
  DEBUG_PRINT("///");
  DEBUG_PRINT(scale);
  DEBUG_PRINTln();
#endif
  
  boolean lifted = false;
  
  while (true) {
    signed char nx = pgm_read_byte(glyph);
    signed char ny = pgm_read_byte(glyph+1);
    
    glyph += 2;

#ifdef DEBUG_SERIAL 
  DEBUG_PRINT("/");
  DEBUG_PRINT(nx);
  DEBUG_PRINT("-");
  DEBUG_PRINT(ny);
  DEBUG_PRINTln();
#endif
    
    if (nx == GLYPH_END || ny == GLYPH_END)
      break;
      
    if (nx == GLYPH_LIFT || ny == GLYPH_LIFT) {
      lift(true);
      lifted = true;
      continue;
    } 
    
    gopos(x+bx+nx*scale, y+ny*scale);
    
    if (lifted) {
      lifted = false;
      delay(100);
      lift(false);
    }
  }
  
  return (nx2-nx1)*scale;
}

/* Draw text, then rest */
void text(const char* str, float x1, float x2, float y1, float y2) {
  if (x1 >= x2 || y1 >= y2) return;
  
  float maxwidth = x2-x1;
  float maxheight = y2-y1;
  
  float genwidth = 0;
  float genheight = 32;
  
  for (const char* ptr = str; *ptr; ptr++) {
    int pos = chrtopos(*ptr);
    const signed char* glyph = GLYPHS[pos];
    signed char nx1 = pgm_read_byte(glyph);
    signed char nx2 = pgm_read_byte(glyph+1);
    genwidth += (nx2-nx1);
  }
  
  DEBUG_PRINT("str:");
  DEBUG_PRINT(str);
  DEBUG_PRINT("/");
  DEBUG_PRINT(genwidth);
  DEBUG_PRINT("-");
  DEBUG_PRINT(genheight);
  DEBUG_PRINT("//");
  DEBUG_PRINT(maxwidth);
  DEBUG_PRINT("-");
  DEBUG_PRINT(maxheight);
  DEBUG_PRINTln();
  
  float r1 = maxwidth/genwidth;
  float r2 = maxheight/genheight;
  float r = min(r1,r2);
  
  float x = x1;
  float height = 16*r;
  float basey = y2-height;

  DEBUG_PRINT(r1);
  DEBUG_PRINT("-");
  DEBUG_PRINT(r2);
  DEBUG_PRINT("//");
  DEBUG_PRINT(height);
  DEBUG_PRINTln();
  
  servo_rest(false);
  for (const char* ptr = str; *ptr; ptr++) {
    x += letter(*ptr, x, basey, height);
  }
  servo_rest(true);
}

