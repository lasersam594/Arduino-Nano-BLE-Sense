/*
Nano BLE 33 Sense Sensor Test V11.

Copyright (c) 1994-2024 Sam Goldwasser, all rights reserved.  Permission is granted for public use or modification as long
as the Copyright notice is included.

This a simple utility to exercise most of the Nano BLE 33 Sense Ver1 or Ver2 sensors using the on-board LEDs and serial port.
The required Nano BLE 33 libraries are all either built into the Arduino IDE or Arduino Cloud Editor, or readily found via
a Web search.  Note that the primary difference between the Ver1 and Ver2 sketches are the libraries for the IMU and T/H.

Accelerometer (Gs) X, Y, Z; Gyroscope (Degs/s) Roll, Pitch, Yaw; Magnetic Field (Gauss) X, Y, Z; Temperature (DegC),
Pressure (mm/Hg), Humidity (%), Proximity (Prox), RGB Light Detect (R, G, B) values, and peak Mic values are all optionally
sent via the serial port as data-only, or with labels.

In addition, the on-board BUILTIN_LED, PWR_LED, and RGB_LED provide visual output:

1. Gyroscope: Displays the absolute value for Roll, Pitch, and Yaw as the brightness of the RGB leds.
2. Proximity: Displays the distance as the brightness of the BUILTIN_LED (bright is closest).
3. Static Tilt (accelerometer Z value): Turns on the PWR_LED if more than approximately 45 degrees.
4. Microphone: Displays the peak intensity of the audio on a color scale using the RGB leds.
5. Heartbeat: The BUILTIN_LED flashes at an approximately 1 Hz rate if there is no display activity.

To select the Nano BLE 33 Sense board type (Ver1 or Ver2), edit the #define at the beginning of the sketch.
To select whether data is send to the serial port and what type, edit the #defines data1 and verbose1.

Suggestions for (modest!) improvements welcome.
*/

// Select Nano BLE 33 Sense Rev1 or Rev2
#define Rev2        // Select based on specific board  

// Data transmission
#define data1 0        // Sends data if 1, LEDs-only on Nano if 0
#define verbose1 0     // Display labels if 1, data-only if 0
#define senddiag1 0    // Include diagnostic information iff 1.  TBD, currently one integer (diag) is coded.

// Board-specific corrections for possible Gyro offsets
#ifdef Rev1
#define GR_COR   6.5
#define GP_COR   0
#define GY_COR   2.5
#endif

#ifdef Rev2
#define GR_COR   0
#define GP_COR   0
#define GY_COR   0
#endif

// Color palette for audio in RGB_LEDs.
#define BLACK 0,0,0
#define GRAY 7,7,7
#define MAGENTA 15,0,30
#define BLUE 0,0,75
#define CYAN 0,50,50
#define GREEN 0,192,0
#define YELLOW 128,92,0
#define ORANGE 200,40,0
#define RED 255,0,0
#define WHITE 255,255,255

#include <Arduino.h>

// Version
#ifdef Rev1
#include <Arduino_LSM9DS1.h>       // Accelerometer, magnetometer and gyroscope
#include <Arduino_HTS221.h>        // Temperature and humidity
#endif

#ifdef Rev2
#include <Arduino_BMI270_BMM150.h> // Accelerometer, magnetometer and gyroscope
#include <Arduino_HS300x.h>        // Temperature and humidity
#endif

// Common libraries
#include <Arduino_LPS22HB.h>       // Pressure
#include <Arduino_APDS9960.h>      // RGB light and proximity
#include <PDM.h>

int proximity = 0;
int count = 0;
int i = 0;
int timeout = 0;
int sum = 0;

// buffer to read audio samples into, each sample is 16-bits
short sampleBuffer[1024];

// number of samples read
volatile int samplesRead;

void setup() {
  
  // Set the LEDs pins as outputs
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_PWR, OUTPUT);
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);

  // Turn the LED_BUILTIN and LED_PWR on and set the RGB LEDs at low brightness
  analogWrite(LED_BUILTIN, 255);
  digitalWrite(LED_PWR, 1);
  analogWrite(LEDR, 200);
  analogWrite(LEDG, 200);
  analogWrite(LEDB, 200);
  
  if (data1 == 1) {
    Serial.begin(9600);
    while (!Serial);
    Serial.println();
    Serial.println();
    Serial.println("Arduino Nano 33 BLE Sense Sensor Exerciser.");
  }

  // configure the data receive callback
  PDM.onReceive(onPDMdata);

// Startup

  if (!IMU.begin()) {
    if (data1 == 1) Serial.println("Failed to initialize IMU!");
    while (1);
  }  

// Version-specific temperature and humidity sensor libraries

#ifdef Rev1
  if (!HTS.begin()) {
#endif
 
#ifdef Rev2   
  if (!HS300x.begin()) {
#endif

  if (data1 == 1) Serial.println("Failed to initialize temperature and humidity sensor!");
    while (1);  
  }

  if (!BARO.begin()) {
    if (data1 == 1) Serial.println("Failed to initialize pressure sensor!");
    while (1);
  }
 
  if (!APDS.begin()) {
    if (data1 == 1) Serial.println("Error initializing APDS9960 sensor!");
  }
 
  // Microphone (one channel, mono mode. The only sample rates that work so far are 16.000 kHz and 41.667 kHz.  Go figure. ;-)
  if (!PDM.begin(1, 41667)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }

  PDM.setBufferSize(1024);  // 512 is default; 1024 works but 2048 hangs
  PDM.setGain(30);          // Optionally set gain, defaults to 20 

// Banner blurb
  if ((verbose1 == 1) && (data1 == 1)) {
    Serial.println();
    Serial.println("Functions:");
    Serial.println();
    Serial.println("  - Acceleration in Gs.");
    Serial.println("  - Angle in degrees/second. LED threshold 25.");  
    Serial.println("  - Magnetic field in Gauss.");
    Serial.println("  - Temperature in degrees Centigrade.");
    Serial.println("  - Pressure in mm/Hg.");
    Serial.println("  - Humidity in rel %.");
    Serial.println("  - Proximity in arbitrary units.");
    Serial.println("  - RGB light intensity in arbitrary units.");
    Serial.println("  - Peak soundlevel in arbitrary units."); 
    Serial.println();
    Serial.println("Data:"); 
    Serial.println("");
  }
}

void loop() {
  float ax, ay, az, gr, gp, gy, mx, my, mz;
  char buffer[40];
  int led, ledr, ledp, ledy;
  int diag = 0;

// Accelerometer

  while (!IMU.accelerationAvailable()) {}
  IMU.readAcceleration(ax, ay, az);
  if (data1 == 1) {
    if (verbose1 == 1) Serial.print("  Axl (Gs) X: ");
    sprintf(buffer, "%5.2f", ax );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print(" Y: ");
    sprintf(buffer, "%5.2f", ay );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print(" Z: ");
    sprintf(buffer, "%5.2f"  , az );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print(" | ");
  }

  led = (az * 255);
  if (led < 180) digitalWrite(LED_PWR, HIGH); // Turn on LED_PWR if tilt is more than ~45 degrees
    else digitalWrite(LED_PWR, LOW);          // analogWrite(LED_PWR, led); // Using analogWrite hangs here, even with a cosntant???
    
// Gyroscope

  while (!IMU.gyroscopeAvailable()) {}
  IMU.readGyroscope(gr, gp, gy);
  if (data1 == 1) {
    if (verbose1 == 1) Serial.print("Gyro (Degs/s) R: ");
    sprintf(buffer, "%8.2f", gr - GR_COR );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print( "  P: ");
    sprintf(buffer, "%8.2f", gp - GP_COR );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print( "  Y: ");
    sprintf(buffer, "%8.2f"  , gy - GY_COR);
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print(" | ");
  }

  ledr = abs(gr - GR_COR)/2;
  ledp = abs(gp - GP_COR)/2;
  ledy = abs(gy - GY_COR)/2;

  if ((ledr > 8) || (ledp > 8) || (ledy > 8)) {
    RGB_LED_Color(ledr, ledp, ledy);
    timeout = 16;
    }
  else if (timeout > 0) timeout --;

// Magnetic field

  while (!IMU.magneticFieldAvailable()) {}    
  IMU.readMagneticField(mx, my, mz);
  if (data1 == 1) {
    if (verbose1 == 1) Serial.print("Field (Gauss) X: ");
    sprintf(buffer, "%5.2f", mx/100 );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print( "  Y: ");
    sprintf(buffer, "%5.2f", my/100 );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print( "  Z: ");
    sprintf(buffer, "%5.2f", mz/100 );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print(" |");
  }

// Temperature, humidity, and pressure

#ifdef Rev1
  float temperature = HTS.readTemperature();
  float humidity    = HTS.readHumidity();
#endif

#ifdef Rev2
  float temperature = HS300x.readTemperature();
  float humidity    = HS300x.readHumidity();
#endif

  float pressure    = BARO.readPressure();

  if (data1 == 1) { 
    if (verbose1 == 1) Serial.print(" T:");
    sprintf(buffer, " %5.2f", temperature );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print(" DegC; P: "); 
    sprintf(buffer, "%6.2f", pressure*7.50062 );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print(" mm/Hg; H: ");
    sprintf(buffer, "%5.2f", humidity );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print("% | Prox:");  
  }

// Proximity

  if (APDS.proximityAvailable()) proximity = APDS.readProximity();
  analogWrite(LED_BUILTIN, 230-proximity); 

  if (data1 == 1) {
    sprintf(buffer, " %3d", proximity );
    Serial.print(buffer);
  }

// RGB light detect

  int r, g, b;

  while (!APDS.colorAvailable()) delay(5);
  APDS.readColor(r, g, b);

  if (data1 == 1) {
    if (verbose1 == 1) Serial.print(" | Light R: ");
    sprintf(buffer, "%3d", r/16 );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print(" G: ");
    sprintf(buffer, "%3d", g/16 );
    Serial.print(buffer);
    if (verbose1 == 1) Serial.print(" B: ");
    sprintf(buffer, "%3d", b/16 );
      Serial.print(buffer);
  }

// Microphone

  // wait for samples to be read
  if (samplesRead) {
    int i = 0;
    sum = 0;

    for (i = 0; i < samplesRead; i++) if (abs(sampleBuffer[i]) > sum) sum = abs(sampleBuffer[i]); // Peak detect

  // Display the peak sound value in RGB_LED
    if (((abs(gr - GR_COR) < 1) && (abs(gp - GP_COR) < 1) && (abs(gy - GY_COR)) < 1)) { // Only if no Gyro activity
      if (sum >= 1000) RGB_LED_Color(WHITE);
      else if (sum >= 600) RGB_LED_Color(RED);
      else if (sum >= 400) RGB_LED_Color(ORANGE);
      else if (sum >= 325) RGB_LED_Color(YELLOW);
      else if (sum >= 250) RGB_LED_Color(GREEN);
      else if (sum >= 175) RGB_LED_Color(CYAN);
      else if (sum >= 100) RGB_LED_Color(BLUE);
      else if (sum >= 50)  RGB_LED_Color(MAGENTA);
      else if (sum >= 25)  RGB_LED_Color(GRAY);
      else if (sum >= 0)   RGB_LED_Color(BLACK); 
    }
    if (sum >= 25) timeout = 16;
  }

  if (data1 == 1) {
    if (verbose1 == 1) Serial.print(" | Mic: ");
    sprintf(buffer, "%4d", sum  );
    Serial.print(buffer);
    if (senddiag1 == 0) Serial.println("");
  }

  // clear the read count
  samplesRead = 0; // Clear sample buffer

  // Optional diagnostic field
  if (senddiag1 == 1) {
    if (data1 == 1) {
      if (verbose1 == 1) Serial.print(" | Diag: ");
      sprintf(buffer, "%4d", diag  );
      Serial.print(buffer);
      Serial.println("");
    }
  }

  // Heartbeat
  count++;
  if (count >= 8) {
    if((proximity > 230) && (ledr <= 8) && (ledp <= 8) && (ledy <= 8) && (sum < 25) && (timeout == 0))
    {
      analogWrite(LED_BUILTIN,255); // Pulse BUILTIN led
      count = 0;
    }
  }
  delay(25); 
}

void RGB_LED_Color(int r, int g, int b) {
  analogWrite(LEDR,255-r);
  analogWrite(LEDG,255-g);
  analogWrite(LEDB,255-b);
}

void onPDMdata() {
  // query the number of bytes available
  int bytesAvailable = PDM.available();

  // read into the sample buffer
  PDM.read(sampleBuffer, bytesAvailable);

  // 16-bit, 2 bytes per sample
  samplesRead = bytesAvailable / 2;
}
