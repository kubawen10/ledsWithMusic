#include <arduinoFFT.h>
#include <FastLED.h>
#include <EasyButton.h>

//pins
#define LED_PIN         4             // LED strip data
#define AUDIO_IN_PIN    35            // Signal in on this pin
#define BTN_PIN         13            // Button pin

//analyse
#define SAMPLES         512           // Must be a power of 2
#define SAMPLING_FREQ   9000          // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AMPLITUDE       1000          // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.            
#define NUM_BANDS       8             // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NOISE           750           // Used as a crude noise filter, values below this are ignored

//leds
#define NUM_LEDS        300           // Number of leds
#define PATTERN_CHANGE_TIME 120       // Duration of every pattern in auto changing mode in seconds 
//button
#define LONG_PRESS_MS   500           // Number of ms to count as a long press

// FastLED stuff
CRGB source1[NUM_LEDS];
CRGB source2 [NUM_LEDS];
CRGB leds[NUM_LEDS];
int BRIGHTNESS_SETTINGS[]={0,30,50,100,200,255};



// Button stuff
EasyButton modeBtn(BTN_PIN);
bool music = false;                   //defines current mode, music/noMusic
bool autoChangePatterns = true;
int ButtonPushCounter = 0;  // Changing patterns

// Sampling and FFT stuff
unsigned int sampling_period_us;
int bandTopValues[]={60, 20, 50, 50, 60, 90, 130, 35};
int oldBarHeights[] = {0,0,0,0,0,0,0,0};  // The length of these arrays must be >= NUM_BANDS
int bandValues[] = {0,0,0,0,0,0,0,0};
double vReal[SAMPLES];
double vImag[SAMPLES];
unsigned long newTime;
arduinoFFT FFT = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// Used for effects
uint8_t hue =0;
uint8_t hue1 =0;
uint8_t hueForBreath=0;
uint8_t paletteIndex = 0;
unsigned long then=0;
unsigned long now;
int ammount=0;
uint8_t blendAmount = 0;
uint8_t source1Pattern = 0;
uint8_t source2Pattern = 1;
bool useSource1 = false;

//gradients
DEFINE_GRADIENT_PALETTE(yellowredpurple_gp){
  0, 252, 176, 69,
  82, 131, 58, 180,
  186, 253, 29, 29,
  255, 252, 176, 69
};
DEFINE_GRADIENT_PALETTE(red_gp){
  0, 255, 0, 0,
  66, 245, 81, 81,
  170, 142, 0, 0,
  255, 255, 0, 0
};
DEFINE_GRADIENT_PALETTE(bluepink_gp){
  0, 232, 0, 112,
  127, 0, 2, 172,
  255, 232, 0, 112
};
DEFINE_GRADIENT_PALETTE( purple_gp ) {
  0,   0, 212, 255,   //blue
255, 179,   0, 255 }; //purple
DEFINE_GRADIENT_PALETTE( outrun_gp ) {
  0, 141,   0, 100,   //purple
127, 255, 192,   0,   //yellow
255,   0,   5, 255 };  //blue
DEFINE_GRADIENT_PALETTE( greenblue_gp ) {
  0,   0, 255,  60,   //green
 64,   0, 236, 255,   //cyan
128,   0,   5, 255,   //blue
192,   0, 236, 255,   //cyan
255,   0, 255,  60 }; //green
DEFINE_GRADIENT_PALETTE (heatmap_gp) {
    0,   0,   0,   0,   //black
  128, 255,   0,   0,   //red
  200, 255, 255,   0,   //bright yellow
  255, 255, 255, 255    //full white 
};
CRGBPalette16 yellowredpurple = yellowredpurple_gp;
CRGBPalette16 redpalette = red_gp; 
CRGBPalette16 bluepink = bluepink_gp;
CRGBPalette16 purplePal = purple_gp;
CRGBPalette16 outrunPal = outrun_gp;
CRGBPalette16 greenbluePal = greenblue_gp;
CRGBPalette16 heatPal = heatmap_gp;




void setup() {
  //setting up the leds
  pinMode(AUDIO_IN_PIN, INPUT);
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(BRIGHTNESS_SETTINGS[3]);
  FastLED.clear();
  FastLED.show();

  //setting up the button functions
  modeBtn.begin();
  modeBtn.onPressed(changePattern);                                 // short click  -> change pattern
  modeBtn.onPressedFor(LONG_PRESS_MS, brightnessButton);            // long click   -> adjust the brightness
  modeBtn.onSequence(3, 2000, startAutoMode);                       // triple click -> starts auto pattern changing
  modeBtn.onSequence(5, 2000, musicNoMusic);                        // five clicks  -> changes mode 

  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));
  delay(500);
}

// changing pattern and disabling auto pattern changing
void changePattern(){
  Serial.println("change");
  autoChangePatterns = false;
  nextPattern();
}

// adjusting the brightness
void brightnessButton(){
  Serial.println("brightness");
  if (FastLED.getBrightness() == BRIGHTNESS_SETTINGS[5])  FastLED.setBrightness(BRIGHTNESS_SETTINGS[0]);
  else if (FastLED.getBrightness() == BRIGHTNESS_SETTINGS[4]) FastLED.setBrightness(BRIGHTNESS_SETTINGS[5]);
  else if (FastLED.getBrightness() == BRIGHTNESS_SETTINGS[3]) FastLED.setBrightness(BRIGHTNESS_SETTINGS[4]);
  else if (FastLED.getBrightness() == BRIGHTNESS_SETTINGS[2]) FastLED.setBrightness(BRIGHTNESS_SETTINGS[3]);
  else if (FastLED.getBrightness() == BRIGHTNESS_SETTINGS[1]) FastLED.setBrightness(BRIGHTNESS_SETTINGS[2]);
  else if (FastLED.getBrightness() == BRIGHTNESS_SETTINGS[0]) FastLED.setBrightness(BRIGHTNESS_SETTINGS[1]);
}

// enables auto pattern changing
void startAutoMode() {
  Serial.println("auto change");
  autoChangePatterns = true;
}

// changes the mode and resets to default values
void musicNoMusic(){
  Serial.println("music noMusic");
  music = !music;
  startAutoMode();
  ButtonPushCounter = 0;
  blendAmount = 0;
  source1Pattern = 0;
  source2Pattern = 1;
  useSource1 = false;
}

void loop() {
  modeBtn.read();
  
  //-----MUSIC DATA-----
  if (music){
    // Reset bandValues[]
    for (int i = 0; i<NUM_BANDS; i++){
      bandValues[i] = 0;
    }
              
    // Sample the audio pin
    for (int i = 0; i < SAMPLES; i++) {
      newTime = micros();
      vReal[i] = analogRead(AUDIO_IN_PIN); // A conversion takes about 9.7uS on an ESP32
      vImag[i] = 0;
      while ((micros() - newTime) < sampling_period_us) { /* chill */ }
    }
              
    // Compute FFT
    FFT.DCRemoval();
    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();
              
    // Analyse FFT results
    for (int i = 2; i < (SAMPLES/2); i++){       // Don't use sample 0 and only first SAMPLES/2 are usable. Each array element represents a frequency bin and its value the amplitude.
      if (vReal[i] > NOISE) {                    // Add a crude noise filter
        //8 bands, 12kHz top band
        if (i<=4 )           bandValues[0]  += (int)vReal[i];
        if (i>4   && i<=6  ) bandValues[1]  += (int)vReal[i];
        if (i>6   && i<=13 ) bandValues[2]  += (int)vReal[i];
        if (i>13  && i<=27 ) bandValues[3]  += (int)vReal[i];
        if (i>27  && i<=55 ) bandValues[4]  += (int)vReal[i];
        if (i>55  && i<=112) bandValues[5]  += (int)vReal[i];
        if (i>112 && i<=229) bandValues[6]  += (int)vReal[i];
        if (i>229          ) bandValues[7]  += (int)vReal[i];
      }
    }
              
    // Process the FFT data into bar heights
    for (byte band = 0; band < NUM_BANDS; band++) {
      int barHeight = bandValues[band] / AMPLITUDE;                             // Scale the bars
      barHeight = ((oldBarHeights[band] * 1) + barHeight) / 2;                  // Small amount of averaging between frames      
      if (barHeight > bandTopValues[band]) {barHeight = bandTopValues[band];}   // Defines the maximum value of each bar     
      bandValues[band]=map(barHeight,0,bandTopValues[band],0,255);              // Mapping bar heights to be between 0 and 255      
      oldBarHeights[band] = barHeight;                                          // Save oldBarHeights for averaging later
    }
    //all bandValues are between 0 and 255 now
  }

////used for debugging
//  for (int i=0;i<8;i++){
//      
//      Serial.print(bandValues[i]);
//      Serial.print("    ");
//    }
//  Serial.println();

  //^^-----END OF MUSIC DATA-----^^

  //-----LED SETTING-----
  
  //used for blending patterns (smooth transitions)
  EVERY_N_MILLISECONDS(10) {
    if (!music){
      blend(source1, source2, leds, NUM_LEDS, blendAmount);   // Blend between the two sources
    
      if (useSource1) {
        if (blendAmount < 255) blendAmount++;                   // Blend 'up' to source 2
      } 
      else {
        if (blendAmount > 0) blendAmount--;                     // Blend 'down' to source 1
      }
    }
  }

  //chosing between running music or noMusic pattern
  if (music){
    runMusicPattern(ButtonPushCounter, leds);
  }  
  else{ 
    runNoMusicPattern(source1Pattern, source1);
    runNoMusicPattern(source2Pattern, source2);
  }
 
  // Used in some of the patterns
  EVERY_N_MILLISECONDS(25) {
    hue++;
    paletteIndex++;
  }
  //used in function "sameChangingColor"
  EVERY_N_MILLISECONDS(200) {
    hue1++;
  }
  //time between every pattern in auto changing mode
  EVERY_N_SECONDS(PATTERN_CHANGE_TIME) {
    if (autoChangePatterns) nextPattern();
  }
  FastLED.show();
}

//FUNCTIONS

// changes the pattern based on current mode
void nextPattern(){
  // defines the ammount of patterns for each mode
  if (autoChangePatterns && !music) ammount=10;  //skip red when auto changing
  else if (!autoChangePatterns && !music) ammount = 11;
  else if (music) ammount = 7;
  
  ButtonPushCounter = (ButtonPushCounter + 1) % ammount;
  
  // Determine which source array for new pattern
  if (useSource1) source1Pattern = ButtonPushCounter;    
  else source2Pattern = ButtonPushCounter;

  useSource1 = !useSource1;                           // Swap source array for next time around
}

// running patterns without music input
void runNoMusicPattern(uint8_t pattern, CRGB *LEDArray) {
  switch (pattern) {
    case 0:
      rainbow(LEDArray);
      break;
    case 1:
      sameChangingColor(LEDArray);
      break;
    case 2:
      redColorPalette(LEDArray);
      break;
    case 3:
      yellowredpurplePalette(LEDArray);
      break;
    case 4:
      bluepinkPalette(LEDArray);
      break;
    case 5:
      longRaingbow(LEDArray);
      break;
    case 6:
      purplePalette(LEDArray);
      break;
    case 7:
      outrunPalette(LEDArray);
      break;
    case 8:
      greenbluePalette(LEDArray);
      break;
    case 9:
      heatPalette(LEDArray);
      break;  
    case 10:
      solidRed(LEDArray);
      break;  
  }
}

// running patterns with music input
void runMusicPattern(uint8_t pattern, CRGB *LEDArray){
  switch (pattern){
    case 0:
      heatBarsOnBass(LEDArray);
      break;
    case 1:
      rainbowBarsOnBass(LEDArray);
      break;   
    case 2: 
      breathOnBass(LEDArray);
      break;
    case 3:
      frequencySprinkle(LEDArray);
      break;
    case 4:
      sameColorBars(LEDArray);
      break; 
    case 5:
      meteors(LEDArray);
      break;  
    case 6:
      randomStrips(LEDArray);
      break;
  }
}

//-----PALETTES WITH MUSIC-----

//heat bars every some ammount of leds  20 leds = 1 segment 
void heatBarsOnBass(CRGB *LEDArray){
  int height = map(bandValues[0],0,255,0,10);
  
  for(int j = 9; j < 300; j+=20){
    for (int i = 0; i < height; i++){
      LEDArray[j-i]   = CRGB(255,i*20,0);
      LEDArray[j+i+1] = CRGB(255,i*20,0);  
    }
    for (int i = height; i < 10; i ++){
      LEDArray[j-i]   = CRGB(0,0,0);
      LEDArray[j+i+1] = CRGB(0,0,0);
    }
  } 
}

// rainbow bars, like "heatBarsOnBass"
void rainbowBarsOnBass(CRGB *LEDArray){
  int height = map(bandValues[0],0,255,0,10);
  for(int j = 9; j < 300; j+=20){
    for (int i = 0; i < height; i++){
      int huee = map(i,0,9,0,255);
      LEDArray[j-i]   = CHSV(huee,255,255);
      LEDArray[j+i+1] = CHSV(huee,255,255);  
    }
    for (int i = height; i < 10; i ++){
      LEDArray[j-i]   = CRGB(0,0,0);
      LEDArray[j+i+1] = CRGB(0,0,0);
    }
  }  
}

// breathing effect on bass
void breathOnBass(CRGB *LEDArray){
  fill_solid(LEDArray, NUM_LEDS, CHSV(hueForBreath,255,bandValues[0]));
  if (bandValues[0]>100){
    hueForBreath+=10;
  }
  EVERY_N_MILLISECONDS(25){
    fadeToBlackBy(LEDArray,NUM_LEDS,1);
  }
}

// sprinkles some color based on frequency
void frequencySprinkle(CRGB *LEDArray){
  int bigBass     = map(bandValues[0],0,255,0,20);
  int Bass        = map(bandValues[1],0,255,0,5);
  int smallBass   = map(bandValues[2],0,255,0,5);
  int bigMid      = map(bandValues[3],0,255,0,5);
  int Mid         = map(bandValues[4],0,255,0,5);
  int smallMid    = map(bandValues[5],0,255,0,5);
  int High        = map(bandValues[6],0,255,0,5);
  int bigHigh     = map(bandValues[7],0,255,0,5);
  
  for(int i=0;i<Bass;i++){
    LEDArray[random16(0,NUM_LEDS-1)] = CHSV(31,255,255);
  }
  for(int i=0;i<smallBass;i++){
    LEDArray[random16(0,NUM_LEDS-1)] = CHSV(45,255,255); 
  }
  for(int i=0;i<bigMid;i++){
    LEDArray[random16(0,NUM_LEDS-1)] = CHSV(55,255,255); 
  }
  for(int i=0;i<Mid;i++){
    LEDArray[random16(0,NUM_LEDS-1)] = CHSV(90,255,255); 
  }
  for(int i=0;i<smallMid;i++){
    LEDArray[random16(0,NUM_LEDS-1)] = CHSV(220,255,255);
  }
  for(int i=0;i<High;i++){
    LEDArray[random16(0,NUM_LEDS-1)] = CHSV(175,255,255); 
  }
  for(int i=0;i<bigHigh;i++){
    LEDArray[random16(0,NUM_LEDS-1)] = CHSV(140,255,255); 
  }
  for(int i=0;i<bigBass;i++){
    LEDArray[random16(0,NUM_LEDS-1)] = CHSV(0,255,255);
  }
  fadeToBlackBy(LEDArray,NUM_LEDS,50); 
}

// same color bars like "heatBarsOnBass"
void sameColorBars(CRGB *LEDArray){
  int height = map(bandValues[0],0,255,0,10);
  
  for(int j = 9; j < 300; j+=20){
    for (int i = 0; i < height; i++){
      LEDArray[j-i]   = CHSV(hue,255,255);
      LEDArray[j+i+1] = CHSV(hue,255,255);  
    }
    for (int i = height; i < 10; i ++){
      LEDArray[j-i]   = CRGB(0,0,0);
      LEDArray[j+i+1] = CRGB(0,0,0);
    }
  }  
}

// meteors (could be better) on bass
void meteors(CRGB *LEDArray){
  if (bandValues[0]>100){
    LEDArray[0]=CHSV(hue,255,255);
  }

  for(int i=NUM_LEDS-1;i>0;i--){
    LEDArray[i]=LEDArray[i-1];
    LEDArray[i-1]=CRGB(0,0,0);
  }
  
}

// random color strips on bars
void randomStrips(CRGB *LEDArray){
  now=millis();
  if ((bandValues[0]>80) && (now-then>300)){
    
    FastLED.clear();
    int from=random(0,25);
    int to=random(from+1,30);
    int color=random(0,255);
    while(to<NUM_LEDS){
      for (int i=from;i<to;i++){ 
        LEDArray[i]=CHSV(color,255,255);
      
      
      }
      from=random(to+1,to+25);
      to=random(from+1,from+30);
    }
   then=now;
  }
  fadeToBlackBy(LEDArray,NUM_LEDS,50);
  
}

// ^-----END OF PATTERNS WITH MUSIC-----^

// -----PATTERNS WITHOUT MUSIC-----

// slow rainbow
void rainbow(CRGB *LEDArray){
  for (int i = 0; i < NUM_LEDS; i++){
        LEDArray[i] = CHSV(hue + (i * 3),255,255);
      }
}

// all leds same color, changing
void sameChangingColor(CRGB *LEDArray){
  for (int i = 0; i < NUM_LEDS; i++){
        LEDArray[i] = CHSV(hue1,255,255);
      }
}

void redColorPalette(CRGB *LEDArray){
  fill_palette(LEDArray, NUM_LEDS, paletteIndex, 3, redpalette, 255, LINEARBLEND);
}

void yellowredpurplePalette(CRGB *LEDArray){
  fill_palette(LEDArray, NUM_LEDS, paletteIndex, 3, yellowredpurple, 255, LINEARBLEND);
}

void bluepinkPalette(CRGB *LEDArray){
  fill_palette(LEDArray, NUM_LEDS, paletteIndex, 3, bluepink, 255, LINEARBLEND);
}

// slow rainbow but less ammount of colors 
void longRaingbow(CRGB *LEDArray){
  for (int i = 0; i < NUM_LEDS; i++){
        LEDArray[i] = CHSV(hue + (i * 0.3),255,255);
      }
}

void purplePalette(CRGB *LEDArray){
  fill_palette(LEDArray, NUM_LEDS, paletteIndex, 3, purplePal, 255, LINEARBLEND);
}

void outrunPalette(CRGB *LEDArray){
  fill_palette(LEDArray, NUM_LEDS, paletteIndex, 3, outrunPal, 255, LINEARBLEND);
}

void greenbluePalette(CRGB *LEDArray){
  fill_palette(LEDArray, NUM_LEDS, paletteIndex, 3, greenbluePal, 255, LINEARBLEND);
}

void heatPalette(CRGB *LEDArray){
  fill_palette(LEDArray, NUM_LEDS, paletteIndex, 3, heatPal, 255, LINEARBLEND);
}

void solidRed(CRGB *LEDArray){
  fill_solid(LEDArray, NUM_LEDS, CRGB::Red);
}
