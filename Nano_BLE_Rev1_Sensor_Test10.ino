/*
  Arduino Nano 33 BLE Sense Rev1: Simple Accelerometer, Gyroscope, Magnetic Field, Environmental, Proximity, and Light Sensor Test

  Copyright 1994-2024 Sam Goldwasser

  This example reads the values from the sensors and optionally sends them to the Serial Monitor or Serial Plotter.
  It also shows Roll, Pitch, and Yaw as the intensities of the RGB LED, as well as proximity in the intensity of the
  BUILTIN LED, and turns the PWR_LED on if tilt (az) is more than ~45 degrees.  The BUILTIN_LED pulses at a roughly
  1 Hz rate as a heartbeat if gyro and proximity are not active.  And reads data from the built-in microphone and
  sends peak-detected audio to the RGB LED iff there is no activity on the Gyro and vice-versa.

  V10: 2-Dec-2024.
*/

#define verbose1 1                 // Display labels if 1, data-only if 0
#define data1 1                     // Sends data if 1, LEDs-only on Nano if 0

#define BLACK 0,0,0
#define VIOLET 25,0,50
#define BLUE 0,0,255
#define CYAN 0,255,255
#define GREEN 0,0,255
#define YELLOW 255,50,0
#define ORANGE 255,125,0
#define RED 255,0,0

#include <Arduino_LSM9DS1.h>       // Accelerometer, magnetometer and gyroscope
#include <Arduino_HTS221.h>        // Temperature and humidity
#include <Arduino_LPS22HB.h>       // Pressure
#include <Arduino_APDS9960.h>      // RGB light and proximity
#include <PDM.h>

int proximity = 0;
int count = 0;
int i = 0;
int sum = 0;

// buffer to read audio samples into, each sample is 16-bits
short sampleBuffer[256];

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
     Serial.println("Started.");
     }

  // configure the data receive callback
  PDM.onReceive(onPDMdata);

  if (!IMU.begin()) {
    if (data1 == 1) Serial.println("Failed to initialize IMU!");
    while (1);
    }  
  if (!HTS.begin()) {
    if (data1 == 1) Serial.println("Failed to initialize humidity temperature sensor!");
    while (1);  
    }

  if (!BARO.begin()) {
    if (data1 == 1) Serial.println("Failed to initialize pressure sensor!");
    while (1);
    }
 
  if (!APDS.begin()) {
    if (data1 == 1) Serial.println("Error initializing APDS9960 sensor!");
    }

  // optionally set the gain, defaults to 20
  // PDM.setGain(30);
  // initialize PDM with:
  // - one channel (mono mode)
  // - a 16 kHz sample rate
  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }

  if ((verbose1 == 1) && (data1 == 1)) {
    Serial.println("Acceleration in Gs.");
    Serial.println("Angle in degrees/second. LED threshold 25.");  
    Serial.println("Magnetic field in Gauss.");
    Serial.println("Temperature in degrees Centigrade.");
    Serial.println("Pressure in mm/Hg.");
    Serial.println("Humidity in rel %.");
    Serial.println("Soundlevel.");

    Serial.println();
  }
}

void loop() {
  float ax, ay, az, gr, gp, gy, mx, my, mz;
  char buffer[40];
  int led, ledr, ledp, ledy;

// Accelerometer

  while (!IMU.accelerationAvailable()) {}
    {
      IMU.readAcceleration(ax, ay, az);
 
      if (data1 == 1) {
        if (verbose1 == 1) Serial.print("Axl (Gs) X: ");
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
      else digitalWrite(LED_PWR, LOW);
     
     // analogWrite(LED_PWR, led); // Using analogWrite hangs here, even with a cosntant???
    }

// Gyroscope

#define GR_COR   6.5   // These are board-specific offset nulls
#define GP_COR   0
#define GY_COR   2.5

  while (!IMU.gyroscopeAvailable()) {}
    {
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

      ledr = abs(gr - GR_COR)/8;
      ledp = abs(gp - GP_COR)/8;
      ledy = abs(gy - GY_COR)/8;
      if ((ledr > 8) || (ledp > 8) || (ledy > 8)) RGB_LED_Color(ledr, ledp, ledy);

    }

// Magnetic field

  while (!IMU.magneticFieldAvailable()) {}
    {
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
    }

// Temperature, humidity, and pressure

  float temperature = HTS.readTemperature();
  float humidity    = HTS.readHumidity();
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
/*
  if (r > 16) analogWrite(LEDR, 255-(r/16)); // Removed due to useless behavior. ;-)
  if (g > 16) analogWrite(LEDG, 255-(g/16)); 
  if (b > 16) analogWrite(LEDB, 255-(b/16)); 
*/
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
  }

 if (data1 == 1) {
     if (verbose1 == 1) Serial.print(" | Mic: ");
        sprintf(buffer, "%4d", sum / 8 );
        Serial.print(buffer);
        Serial.println("");
   }


// Display the peak sound value in RGB_LED
    if (((abs(gr - GR_COR) < 2) && (abs(gp - GP_COR) < 2) && (abs(gy - GY_COR)) < 2)) { // Only if no Gyro activity
      if (sum >= 600) RGB_LED_Color(RED);         // Red
      else if (sum >= 500) RGB_LED_Color(ORANGE); // Orange
      else if (sum >= 300) RGB_LED_Color(YELLOW); // Yellow
      else if (sum >= 200) RGB_LED_Color(GREEN);  // Green
      else if (sum >= 150) RGB_LED_Color(CYAN);   // Cyan
      else if (sum >= 100) RGB_LED_Color(BLUE);   // Blue
      else if (sum >= 50)  RGB_LED_Color(VIOLET); // Violet
      else if (sum >= 0)   RGB_LED_Color(BLACK);  // Black
    }
  
// clear the read count
  samplesRead = 0;
  count++;
  if (count >= 15) {
    if((proximity > 230) && (ledr == 0) && (ledp == 0) &&  (ledy == 0)) analogWrite(LED_BUILTIN,255);
    count = 0;
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
