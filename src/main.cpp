// Version history moved to CHANGELOG.md (see project root).

// esp32 by Espressif Systems --> v2.0.12
// ESP32 Dev Module
// Events:  Core 0
// Arduino: Core 1
// Flash Size: 4MB (32Mb)
// PARTITION: Minimal SPIFFS (1.9Mb app with OTA/190Kb SPIFFS)
// Sketchbook Location: /Users/phillipcarlson/Documents/Arduino/SLS/Test

// Single source of truth for the version. Auto-incremented by +0.01 on every
// successful build by scripts/merge_firmware.py; see CHANGELOG.md for history.
float ver = 3.01;


/* #################### To add a new screen (example screen6) ####################
  
  # In your main .ino file, add the new function void Screen6().
  # Change "const int NUM_CLOCK_SCREENS = 5;" to 6
  # Add Screen6 to "void (*clockScreenFunctions[NUM_CLOCK_SCREENS])"
  # Add void Screen6(); { add new screen to // Forward declarations
  # in "WEB_Settings_HTML.h" find
        <select id="screenSelect" onchange="updateScreen(this.value)">
          <option value="1">Screen 1 - Classic Digital</option>
          <option value="2">Screen 2 - Alt. Digital</option>
          <option value="3">Screen 3 - Analog</option>
          <option value="4">Screen 4 - World Map</option>
          <option value="5">Screen 5 - Moon Phase</option>
          <option value="6">Screen 6 - Gradient Clock</option>   <-- ADD THIS LINE
*/

// Screen1 = Info Clock
// Screen2 = Large Weather Clock
// Screen3 = Analogue & calendar Clock
// Screen4 = Day/night terminator map
// Screen5 = Moon Phase
// Screen6 = Gradient Clock
// Screen7 = Digital Watch
// Screen8 = Full Analog Clock
// Screen9 = Nixie Tube Clock

// Screen90 = Setup Clock QR code page
// Screen91 = Setup Wifi (when no credentials saved or connection failed)
// Screen92 = Matrix Button Settings Menu




// Converted from MyClock_2.89.ino -- the Arduino IDE added this include
// (and the forward declarations further down) behind the scenes.
#include <Arduino.h>
#include "esp_system.h"
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <ArduinoOTA.h>
#include <RTClib.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager           v2.0.17
#include <EasyButton.h>
#include <WebServer.h>
#include <Update.h> // browser-based OTA (POST /update)
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <qrcoderm.h> 
#include <SPIFFS.h>
#include <math.h> // Required for sin, cos, asin, atan2, sqrt, fmaxf, fminf
#include <algorithm> // Required for std::min, std::max
#include "WebPage_gz.h" // generated from web/index.html by scripts/build_web.py
#include "Maps.h"
#include "Clock_Faces.h"
#include "WeatherIcons.h"
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"
#include "time.h"   
// https://werner.rothschopf.net/microcontroller/202103_arduino_esp32_ntp_en.htm
// https://www.tutorialspoint.com/c_standard_library/c_function_strftime.htm
// %l = 12 Hour time with no leading 0's 

#include "Fonts/Font_5x7_practical8pt7b.h"
#include "Fonts/Font_5x7_practical8pt7c.h"
#include "Fonts/TomThumb.h" 
#include "Fonts/Org_01.h"
#include "Fonts/Tiny_Phil.h"
#include "Fonts/Aclonica_Regular_12.h"
#include "Fonts/DS_DIGI12pt7b.h"
#include "Fonts/Pixel_Bold10pt7b.h"
#include "Fonts/Tidbyt_Numbers1.h"
GFXfont timeFont = Tidbyt_Numbers1;
int timeFontOffset = -1;

#define PANEL_RES_X 64      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32      // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1       // Total number of panels chained one to another
#define SDA_PIN 21          // i2c pins for AHT10 and DS3231 RTC clock
#define SCL_PIN 19          //



char current_hoursmins[64];  // Declare globally 

// Call this after time synchronization or when switches change
//updateCurrentHoursMins();


// Define a struct to hold RGB values
struct RGBColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

GFXcanvas16 dma_canvas(PANEL_RES_X, PANEL_RES_Y); // Create Canvas
MatrixPanel_I2S_DMA *dma_display = nullptr;  // Declare globally
// A dedicated buffer to hold the last completed frame for web screenshots.
// This prevents sending partially-drawn frames (race condition).
uint8_t screenshotBuffer[PANEL_RES_X * PANEL_RES_Y * 2]; 

// Global variables to store web-selected settings for each screen
// To add a new screen, you just increment this number and add the new screen function.
const int NUM_CLOCK_SCREENS = 9;
const int SCREEN_ID_SETUP_QR    = 90;
const int SCREEN_ID_WIFI_PORTAL = 91;
const int SCREEN_ID_MENU        = 92;


int timezoneOffset = 11; // Default to Sydney AEDT (UTC+11); change this based on your selector
int terminatorOffset = 0; // Global variable to shift terminator line (pixels)
bool ntpPaused = false;           // Flag to track NTP pause state
unsigned long pauseStartTime = 0; // Store the start time of the pause
bool yearAnimationActive = false; // Flag to indicate year animation is running
unsigned long animationStartTime = 0; // Start time of the animation
const unsigned long ANIMATION_DURATION = 15000; // 15 seconds in milliseconds
const int DAYS_PER_YEAR = 365;     // Number of days in a non-leap year
const unsigned long DISPLAY_TIME_PER_DAY = 15000 / 365; // ~41 ms per day
int currentAnimationDay = 1;       // Current day in the animation
String animationDateStr = ""; // Store the formatted date (e.g., "15-MAY")
unsigned long lastSaveTime = 0;
const unsigned long saveDelay = 1000; // 1 second delay
const unsigned long SETUP_PORTAL_TIMEOUT = 5000; // 10 minutes in milliseconds
// Star related
struct Star {float x;float y;float speed;uint8_t brightness;}; 
int NUM_STARS = 25; 
const int MAX_STAR_CAPACITY = 255;
Star stars[MAX_STAR_CAPACITY];
bool stars_initialized = false;
float AA_INTENSITY_MIN_FACTOR = 0.3f;

// Sun Object related
struct SunObject {float x;float y;float speed;};
SunObject screen4_sun_obj;
// bool screen4_sun_initialized = false; // No longer strictly needed
bool is_screen4_sun_active = false; 

// Speed related
float EARTH_ROTATION_SPEED_DEG_PER_FRAME = 1.0f; // was 0.37
const float BASELINE_EARTH_ROTATION_FOR_STAR_SPEED = 0.37f;
float screen4_sun_calculated_speed = 0.0f; 

// Sun Visuals
uint16_t sun_color_global; 
const float SUN_Y_OFFSET_RANGE = 12.0f; // How high and low the sun reaches on the screen
//const int SUN_VISUAL_RADIUS = 1;

// NEW constants for Screen4 fuzzy sun appearance
const int SUN_CORE_RADIUS_S4 = 1;     // Example: Radius of the solid inner core (0 for pixel, 1 for 3x3-ish)
const int SUN_FUZZ_LAYERS_S4 = 3;     // Example: Number of additional fuzzy layers (e.g., 2 layers)
                                      // Total visual extent will be roughly SUN_CORE_RADIUS_S4 + SUN_FUZZ_LAYERS_S4

// NEW: A struct to hold ALL settings for a single screen.
struct ScreenSettings {
  // Colors
  RGBColor time_col       = {118, 251, 78};
  RGBColor date_col       = {214, 214, 214};
  RGBColor dateBG_col     = {71, 36, 171};
  RGBColor temp_col       = {255, 98, 80};
  RGBColor humidity_col   = {40, 95, 244};
  RGBColor day_col        = {245, 236, 0};
  RGBColor month_col      = {214, 214, 214};
  RGBColor ampm_col       = {100, 100, 100};
  RGBColor seconds_col    = {100, 100, 100};
  RGBColor land_col       = {119, 187, 65};
  RGBColor water_col      = {0, 66, 170};
  RGBColor ice_col        = {192, 192, 192};
  
  // Switches
  bool ampmSwitch         = false;
  bool secondsSwitch      = false;
  bool twentyFourHourSwitch = false;
  bool iconsSwitch        = true;
  bool temperatureSwitch  = true;
  bool minMaxTempsSwitch  = true;
  bool daySwitch          = true;
  bool dateSwitch         = true;
  bool monthSwitch        = true;
  bool humiditySwitch     = true;
  bool landSwitch         = true;
  bool waterSwitch        = true;
  bool iceSwitch          = true;
  bool clockSwitch        = true;
  bool backgroundSwitch   = false;
  bool SpareSwitch        = false;
  bool SpareSwitch2       = false;
  bool SpareSwitch3       = false;

  // Sliders
  int pageSlider          = 128;
  int pageSlider2         = 128;
  int pageSlider3         = 128;

  // Image/Colour Options
  bool land_use_image     = false;
  bool water_use_image    = false;
  bool ice_use_image      = false;
  bool clock_use_image    = false;
  int land_bitmap_index   = 0;
  int water_bitmap_index  = 0;
  int ice_bitmap_index    = 0;
  int clock_bitmap_index  = 1;
  String clockDisplayMode = "4 numbers";

  // NEW MEMBERS FOR SCREEN 8
  String markerDisplayMode = "4 Numbers + Stars"; // New setting for marker style
  String numberColorMode = "Rainbow";             // "Rainbow" or "Colour"
  String starColorMode = "Rainbow";               // "Rainbow" or "Colour"
  RGBColor number_color = {200, 200, 200};        // Custom color for numbers
  RGBColor star_color = {150, 150, 150};          // Custom color for stars
  bool hourHandSwitch = true;                     // Switch for hour hand
  bool minuteHandSwitch = true;                   // Switch for minute hand
  bool secondHandSwitch = true;                   // Switch for second hand
};
// NEW: A single vector to hold the settings for all clock screens.
std::vector<ScreenSettings> allScreenSettings;

  // --- NEW: Schedule-related variables ---
  struct Schedule {
  int screen;
  int start_hour;
  int start_min;
  int end_hour;
  int end_min;
  };

std::vector<Schedule> schedules; // A dynamic list to hold all schedule entries
bool schedulesEnabled = false;
int defaultScreen = 1;

// Forward Declarations (add these near the top of your sketch)
void handleSchedulesEnabled();
void handleUpdateSchedules();
void updateCurrentHoursMins();
void dayOfYearToDate(int dayOfYear, int year, int &month, int &day);
float calculateMoonPhase(struct tm timeinfo);
void syncRTCtoESP();
void syncESPtoRTC();
void MenuButtonPressed();
void MenuButtonHeld();
void choosescreen();

void Screen1(); // Info Clock
void Screen2(); // Large Weather Clock
void Screen3(); // Analogue & calendar Clock
void Screen4(); // Day/night terminator map
void Screen5(); // Moon Phase
void Screen6(); // Gradient Clock
void Screen7(); // Digital Watch
void Screen8(); // Full Analog Clock
void Screen9(); // Nixie Tube Clock

void Screen90(); // Setup Clock QR code page
void Screen91(); // Setup Wifi (when no credentials saved or connection failed)
void Screen92(); // Matrix Settings Menu


// NEW: An array of function pointers. This is our "dispatch table" for screens.
void (*clockScreenFunctions[NUM_CLOCK_SCREENS])() = {
  Screen1, 
  Screen2, 
  Screen3, 
  Screen4, 
  Screen5,
  Screen6,
  Screen7,
  Screen8,
  Screen9
  // To add Screen6 (as a clock face), add it here and increase NUM_CLOCK_SCREENS.
};



// BITMAP List for Screen4
  enum BitmapType {       
  BITMAP_BLUE_MARBLE,
  BITMAP_Continents_1,
  BITMAP_Continents_2,
  BITMAP_GOLDEN,
  BITMAP_Grassy,
  BITMAP_Icy,
  BITMAP_Light_Marble,
  BITMAP_Nasa,
  BITMAP_Neon_1,
  BITMAP_Neon_2,
  BITMAP_Neon_3,
  BITMAP_Neon_4,
  BITMAP_Night,
  // Add more bitmaps here as needed (up to 10+)
  BITMAP_COUNT
  };

  const unsigned short *bitmaps[BITMAP_COUNT] PROGMEM = {
  Blue_Marble,
  Continents_1,
  Continents_2,
  Golden,
  Grassy,
  Icy,
  Light_Marble,
  Nasa,
  Neon_1,
  Neon_2,
  Neon_3,
  Neon_4,
  Night
}; // Last one has no ,

RGBColor web_time_col[6] = {
  {118, 251, 78},  // Screen 1
  {118, 251, 78},  // Screen 2
  {214, 214, 214},  // Screen 3
  {118, 251, 78},  // Screen 4
  {100, 100, 100},  // Screen 5
  {100, 100, 100}   // Screen 6
};    // For time color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_date_col[6] = {
  {214, 214, 214},  // Screen 1
  {214, 214, 214},  // Screen 2
  {0, 0, 0},  // Screen 3
  {100, 100, 100},  // Screen 4
  {100, 100, 100},  // Screen 5
  {100, 100, 100}   // Screen 6
};    // For date color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_dateBG_col[6] = {
  {71, 36, 171},       // Screen 1
  {214, 214, 214},       // Screen 2
  {214, 214, 214},       // Screen 3
  {80, 0, 0},       // Screen 4
  {80, 0, 0},       // Screen 5
  {80, 0, 0}        // Screen 6
};                  // For date background color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_temp_col[6] = {
  {255, 98, 80},       // Screen 1
  {255, 98, 80},       // Screen 2
  {181, 26, 0},       // Screen 3
  {80, 0, 0},       // Screen 4
  {80, 0, 0},       // Screen 5
  {80, 0, 0}        // Screen 6
};                    // For temperature color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_humidity_col[6] = {
  {40, 95, 244},       // Screen 1
  {40, 95, 244},       // Screen 2
  {0, 0, 80},       // Screen 3
  {0, 0, 80},       // Screen 4
  {0, 0, 80},       // Screen 5
  {0, 0, 80}        // Screen 6
};               // For humidity color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_day_col[6] = {
  {245, 236, 0},  // Screen 1
  {245, 236, 0},  // Screen 2
  {211, 87, 254},  // Screen 3
  {100, 100, 100},  // Screen 4
  {100, 100, 100},  // Screen 5
  {100, 100, 100}   // Screen 6
};     // For day color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_month_col[6] = {
  {214, 214, 214},  // Screen 1
  {214, 214, 214},  // Screen 2
  {255, 255, 255},  // Screen 3
  {0, 0, 0},  // Screen 4
  {100, 100, 100},  // Screen 5
  {100, 100, 100}   // Screen 6
};   // For month color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_ampm_col[6] = {
  {100, 100, 100},  // Screen 1
  {181, 26, 0},  // Screen 2
  {255, 98, 80},  // Screen 3
  {100, 100, 100},  // Screen 4
  {100, 100, 100},  // Screen 5
  {100, 100, 100}   // Screen 6
};    // For AM/PM color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_seconds_col[6] = {
  {100, 100, 100},  // Screen 1
  {40, 95, 244},  // Screen 2
  {40, 95, 244},  // Screen 3
  {100, 100, 100},  // Screen 4
  {100, 100, 100},  // Screen 5
  {100, 100, 100}   // Screen 6
}; // For seconds color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_land_col[6] = {
  {100, 100, 100},  // Screen 1
  {100, 100, 100},  // Screen 2
  {100, 100, 100},  // Screen 3
  {119, 187, 65},  // Screen 4
  {100, 100, 100},  // Screen 5
  {100, 100, 100}   // Screen 6
}; // For seconds color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_water_col[6] = {
  {100, 100, 100},  // Screen 1
  {100, 100, 100},  // Screen 2
  {100, 100, 100},  // Screen 3
  {0, 66, 170},  // Screen 4
  {100, 100, 100},  // Screen 5
  {100, 100, 100}   // Screen 6
}; // For seconds color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_ice_col[6] = {
  {100, 100, 100},  // Screen 1
  {100, 100, 100},  // Screen 2
  {100, 100, 100},  // Screen 3
  {192, 192, 192},  // Screen 4
  {100, 100, 100},  // Screen 5
  {100, 100, 100}   // Screen 6
}; // For seconds color (Screen 1, 2, 3, 4, 5, 6)

RGBColor web_background_col[6] = {
  {100, 100, 100},  // Screen 1
  {100, 100, 100},  // Screen 2
  {100, 100, 100},  // Screen 3
  {100, 100, 100},  // Screen 4
  {100, 100, 100},  // Screen 5
  {100, 100, 100}   // Screen 6
}; // For seconds color (Screen 1, 2, 3, 4, 5, 6)

volatile bool networkServicesStarted = false;
bool ampmSwitch[6] = {false, true, false, false, false, false};        // AM/PM display toggle for each screen
bool secondsSwitch[6] = {false, true, false, false, false, false};     // Seconds display toggle for each screen
bool twentyFourHourSwitch[6] = {false, false, false, false, false, false}; // 24-hour format toggle for each screen
bool iconsSwitch[6] = {true, true, false, false, false, false};       // Weather icons toggle for each screen
int pageSlider[6] = {128, 128, 128, 128, 128, 128};                  // Page slider for each screen
int pageSlider2[6] = {128, 128, 128, 128, 128, 128};                  // Page slider for each screen
int pageSlider3[6] = {128, 128, 128, 128, 128, 128};                  // Page slider for each screen
bool temperatureSwitch[6] = {true, true, true, false, false, false}; // Temperature display toggle for each screen
bool minMaxTempsSwitch[6] = {true, true, false, false, false, false}; // Min/Max temperature display toggle for each screen
bool daySwitch[6] = {true, true, true, false, false, false};         // Day of week display toggle for each screen
bool dateSwitch[6] = {true, true, true, false, false, false};        // Date display toggle for each screen
bool monthSwitch[6] = {true, true, true, true, false, false};       // Month display toggle for each screen
bool humiditySwitch[6] = {true, true, false, false, false, false};    // Humidity display toggle for each screen
bool landSwitch[6] = {true, true, true, true, true, true}; 
bool waterSwitch[6] {true, true, true, true, true, true}; 
bool iceSwitch[6] {true, true, true, true, true, true}; 
bool backgroundSwitch[6] = {false, false, false, false, false, false}; 
bool clockSwitch[6] = {true, true, true, true, true, true}; // Optional 
bool SpareSwitch[6] = {false, false, false, false, false, false};
bool SpareSwitch2[6] = {false, false, false, false, false, false};
bool SpareSwitch3[6] = {false, false, false, false, false, false};

bool land_use_image[6] = {false, false, false, false, false, false}; 
bool water_use_image[6] = {false, false, false, false, false, false}; 
bool ice_use_image[6] = {false, false, false, false, false, false}; 
bool clock_use_image[6] = {false, false, false, false, false, false};
int land_bitmap_index[6] = {0, 0, 0, 0, 0, 0}; 
int water_bitmap_index[6] = {0, 0, 0, 0, 0, 0}; 
int ice_bitmap_index[6] = {0, 0, 0, 0, 0, 0}; 
int clock_bitmap_index[6] = {1, 1, 1, 1, 1, 1};
String clockDisplayMode[6] = {"4 numbers", "4 numbers", "4 ticks", "4 numbers", "4 numbers", "4 numbers"};

String selectedTimezone = "UTC0";      // Default to UTC
String selectedWeatherService = "pirateweather"; // Can be "pirateweather", "openweathermap", or "weatherapi"
String pirateWeatherAPI = "";         // Store Pirate Weather API key
String openWeatherMapAPI = "";        // Store OpenWeatherMap API key
String weatherAPI_API = "";           // Store WeatherAPI.com API key
String tempUnits = "celsius";       // To store "celsius" or "fahrenheit"
String tempType = "actual";         // To store "actual" or "feels_like"
float gpsLat = 0.0;                   // Store latitude
float gpsLon = 0.0;                   // Store longitude
float moonPhase = 0.125f; // Default to a 25% illuminated waxing crescent
float moonPercentage = 25.0f; // Default to 25% illuminated
bool useCalculatedMoonPhase = false; // True during year animation or day change
float calculatedMoonPhase = 0.0f;   // Store computed phase
uint8_t brightness = 150;  // Default brightness value (0-255)
uint16_t darkRoomLDRValue = 50;    // Renamed from minLDRValue
uint16_t brightRoomLDRValue = 4095; // Renamed from maxLDRValue
uint8_t darkRoomBrightness = 20;   // Renamed from minBrightness
uint8_t brightRoomBrightness = 255; // Renamed from maxBrightness
bool autoBrightnessEnabled = false; // variable to toggle auto-brightness
bool settingsChanged = false; // Flag to track user-initiated changes
uint8_t targetBrightness = 0; // Target brightness for smooth transition
uint8_t currentBrightness = 200; // Current brightness 200 is for bootup brightness
unsigned long lastBrightnessUpdate = 0; // For smooth transition timing
const unsigned long brightnessUpdateInterval = 50; // 50ms for smooth steps
const int MenuButtonPin = 18;
const int LDR_PIN = 32;  // LDR connected to GPIO 2
int FirstBoot = 1;  
int Screen_Refresh = 0; 
int Date_Background_Refresh = 0; 
int Override = 1;
int Menu_Page = 1;
int Menu_Select = 0;
int NTP_Allow = 0;  
int eeprom_TZ = 6;
int Prog_X = 0;
int Prog_Y = 20;
float internalTemp = 0.0;
float internalHumid = 0.0;
unsigned long lastNTPSync = 0;
const unsigned long ntpSyncInterval = 240 * 60 * 1000; // 4 hours =  240 minutes in milliseconds
String panelType = "P5.0"; // Can be "P5.0" or "P2.5"
float indoorTempOffset = 0.0; // To store the temperature offset from the user



// --- NEW --- Language setting and translation arrays
String currentLanguage = "en"; // "en" for English, "de" for German, "sv" for Swedish
const char* days_DE_short[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
const char* days_DE_long[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
const char* months_DE_short[] = {"Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez"};
const char* months_DE_long[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};
const char* days_SE_short[] = {"Sön", "Mån", "Tis", "Ons", "Tor", "Fre", "Lör"};
const char* days_SE_long[] = {"Söndag", "Måndag", "Tisdag", "Onsdag", "Torsdag", "Fredag", "Lördag"};
const char* months_SE_short[] = {"Jan", "Feb", "Mar", "Apr", "Maj", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dec"};
const char* months_SE_long[] = {"Januari", "Februari", "Mars", "April", "Maj", "Juni", "Juli", "Augusti", "September", "Oktober", "November", "December"};

Adafruit_AHTX0 aht;
RTC_DS3231 rtc;

// Define PI if not already defined
#ifndef PI
#define PI 3.1415926535f
#endif
// choose your time zone from this list
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
//String MY_TZ = "AEST-10AEDT,M10.1.0,M4.1.0/3"; 
// String MY_TZ = String('AEST-10AEDT,M10.1.0,M4.1.0/3'); 
time_t now;   
struct tm timeinfo;
#define MY_TZ "AEST-10AEDT,M10.1.0,M4.1.0/3"
const char* ntpServer = "pool.ntp.org";

// Global variables for weather data (not displayed on web page but logged to Serial)
String currentTemp = "";              // Store current temperature (1 decimal place)
String currentApparentTemp = "";
String currentHumidity = "";          // Store current humidity (0 decimal places)
String currentConditions = "";        // Store current conditions
String dailyMinMaxTemps = "";         // Store daily min/max temperatures (0 decimal places)
String weatherIcon = "";              // Store weather icon (e.g., "clear-day", "rain")
float currentTemperature = 0.0;  // Store as float for precision
float currentApparentTemperature = 0.0;
float currentHumidityFloat = 0.0;     // Store as float for precision
int todayMinTemp = 0;  // Initialize to 0, will be updated by fetchWeather()
int todayMaxTemp = 0;  // Initialize to 0, will be updated by fetchWeather()
int currentScreen = 1;  // Tracks the current screen (1, 2, or 3)
int prevScreen = 1;  // Tracks the current screen (1, 2, or 3)


// Rate limiting for API calls
unsigned long lastApiCallTime = 0;    // Timestamp of last API call
int apiCallCount = 0;                 // Number of API calls in the current hour
const int MAX_API_CALLS = 10;         // Maximum calls per hour
const long HOUR_MS = 3600000;         // 1 hour in milliseconds
int lastHttpCode = 0;  // Track last HTTP response code from fetchWeather()

// Define bitmap list for Screen3
  enum Screen3BitmapType {
  BITMAP_FRAME_NEON_BRICK,
  BITMAP_FRAME_NEON_FOG,
  BITMAP_FRAME_TRON,
  BITMAP_RING_BLUE,
  BITMAP_RING_CYBERPUNK,
  BITMAP_RING_NEON_CLOCK_1,
  BITMAP_RING_NEON_CLOCK_2,
  BITMAP_RING_PINK_AND_BLUE,
  BITMAP_RING_STARGATE,
  BITMAP_SQUARE_PINK,
  BITMAP_SQUARE_PINK_2,
  SCREEN3_BITMAP_COUNT
  };

  const unsigned short *screen3_bitmaps[SCREEN3_BITMAP_COUNT] PROGMEM = {
  Frame_Neon_Brick,
  Frame_Neon_Fog,
  Frame_Tron,
  Ring_Blue,
  Ring_Cyberpunk,
  Ring_Neon_Clock_1,
  Ring_Neon_Clock_2,
  Ring_Pink_and_Blue,
  Ring_Stargate,
  Square_Pink,
  Square_Pink_2
};



WebServer server(80);


struct TimeDateComponents {
    String hoursMins;
    String seconds;
    String ampm;
    String day;
    String date;
    String month;
    String apparentTemp;
    String humidity;
    String minTemp;
    String maxTemp;
    String weatherIcon;
};



enum SystemState {
  STATE_WIFI_NO_CREDENTIALS, // No stored credentials, show setup screen
  STATE_WIFI_SETUP,          // Initial WiFi setup/checking
  STATE_WIFI_CONNECTING,     // Waiting for connection and NTP sync
  STATE_RUNNING,             // Normal operation after NTP sync
  STATE_WIFI_DISCONNECTED,    // New state for handling lost connection
};
volatile SystemState currentState = STATE_WIFI_NO_CREDENTIALS; // Volatile for cross-core access
static unsigned long stateStartTime = 0;
static unsigned long stateNOWIFITime = 0;
volatile bool needWeatherUpdate = false; // Already volatile if added earlier
SemaphoreHandle_t settingsMutex = NULL; // Guards /settings.json + the settings globals serialized into it
// True when the Reboot Guard fired (3+ reboots inside 60s) and forced safe
// brightness settings. Published to the web UI as doc["safe_mode"] so the
// settings page can back off to its most conservative live-refresh rate.
bool safeModeActive = false;



// Function definition

// ===========================================================================
//  Forward declarations
// ---------------------------------------------------------------------------
//  The Arduino IDE auto-generated these prototypes during its .ino -> .cpp
//  preprocessing step. Plain C++ has no such step, so they are listed here.
//  Add a line here whenever you add a new function.
// ===========================================================================
void saveSettings();
void printSpiffsFile();
void ClearWifi();
float lerp(float a, float b, float t);
float clamp(float val, float minVal, float maxVal);
void solarPosition(time_t time, float &longitude, float &latitude);
TimeDateComponents getTimeDateString();
float clampValue(float value, float minVal, float maxVal);
void markSettingsChanged();
void handleWeatherService();
void handlePirateWeatherAPI();
void handleOpenWeatherMapAPI();
void handleWeatherAPI_API();
void handleLandSwitch();
void handleWaterSwitch();
void handleIceSwitch();
void handleLandBitmap();
void handleWaterBitmap();
void handleIceBitmap();
void handleLandOption();
void handleWaterOption();
void handleIceOption();
void handleScreenSelection();
void handleMarkerDisplayMode();
void handleNumberColorMode();
void handleStarColorMode();
void handleNumberColor();
void handleStarColor();
void handleHourHandSwitch();
void handleMinuteHandSwitch();
void handleSecondHandSwitch();
void handleBackgroundSwitch();
void handleGPS();
void fetchWeather();
void handleAMPM();
void handleSeconds();
void handle24Hour();
void handleIcons();
void handleTemperature();
void handleMinMaxTemps();
void handleDay();
void handleDate();
void handleMonth();
void handleHumidity();
void handleTimeColor();
void handleDateColor();
void handleDateBGColor();
void handleTempColor();
void handleHumidityColor();
void handleDayColor();
void handleMonthColor();
void handleAMPMColor();
void handleSecondsColor();
void handleBrightness();
void handleDarkRoomBrightness();
void handleBrightRoomBrightness();
void handleSetDarkRoomLDR();
void handleSetBrightRoomLDR();
void handleAutoBrightness();
void handleCurrentBrightness();
void handleTimezone();
void handleLandColor();
void handleWaterColor();
void handleIceColor();
void handleClockSwitch();
void handleClockOption();
void handleClockBitmap();
void handlePageSlider();
void handlePageSlider2();
void handlePageSlider3();
void handleSpareSwitch();
void handleSpareSwitch2();
void handleSpareSwitch3();
void handleFullYearAnimation();
void fetchWeatherTask(void *pvParameters);
void webServerTask(void *pvParameters);
void getInternalAHT10();
void handleScreenshot();
void handleUnits();
void handleTempType();
void displayWeatherIcon(const String& weatherIcon);
void displayLargeWeatherIcon(const String& weatherIcon);
void WIFI_SETUP();
bool dma_display_is_valid();
bool dma_canvas_is_valid();
void Screen1();
void draw_aa_line_simple(GFXcanvas16 &canvas, float x0, float y0, float x1, float y1, uint16_t color, float fuzz_factor);
void Screen3();
float map_float(float x, float in_min, float in_max, float out_min, float out_max);
void init_star(Star& s, bool initial_setup);
void init_sun_for_sky(SunObject& sun_param, float current_delta_sun);
void Screen4();
void Screen5();
void Screen6();
void drawGlowString(String text, int x, int y, uint16_t core_color, uint16_t glow_color, float glow_intensity);
void Screen7();
void drawHourMarker(int hour, const String& mode, uint16_t num_color, uint16_t star_color);
void Screen8();
void Screen9();
void Screen90();
void dayOfYearToDate(int dayOfYear, int year, int &month, int &day);
void handleSerialInput();
void handleLanguage();
void handleIndoorTempOffset();
void handleSchedulesEnabled();
void handleUpdateSchedules();
void handlePanelType();
void drawYearAnimationDate();
void updateCurrentHoursMins();
void choosescreen();
void synchroniseWith_NTP_Time();
void syncRTCtoESP();
void syncESPtoRTC();
void checkNTPSync();
void drawCentreTime(const String &buf, int x, int y);
void drawCentreString(const String &buf, int x, int y);
void drawCentreThirdLeftString(const String &buf, int x, int y);
void drawRightString(const String &buf, int x, int y);
void drawRightCenterString(const String &buf, int x, int y);
void drawLeftString(const String &buf, int x, int y);
void drawCentreChar(const char *buf, int x, int y);
void MenuButtonPressed();


TimeDateComponents getTimeDateString() {
    TimeDateComponents components;
    
    getLocalTime(&timeinfo);
    updateCurrentHoursMins();

    // Split time string into parts
    String timeStr = String(current_hoursmins);
    if (twentyFourHourSwitch[currentScreen - 1]) {
        if (secondsSwitch[currentScreen - 1]) {
            components.hoursMins = timeStr.substring(0, 5);  // e.g., "14:30"
            components.seconds = timeStr.substring(6);       // e.g., "45"
        } else {
            components.hoursMins = timeStr.substring(0, 5);  // e.g., "14:30"
        }
    } else {
        if (ampmSwitch[currentScreen - 1]) {
            if (secondsSwitch[currentScreen - 1]) {
                components.hoursMins = timeStr.substring(0, 5);  // e.g., "8:30"
                components.seconds = timeStr.substring(6, 8);    // e.g., "45"
                components.ampm = timeStr.substring(9);          // e.g., "PM"
            } else {
                components.hoursMins = timeStr.substring(0, 5);  // e.g., "8:30"
                components.ampm = timeStr.substring(6);          // e.g., "PM"
            }
        } else {
            if (secondsSwitch[currentScreen - 1]) {
                components.hoursMins = timeStr.substring(0, 5);  // e.g., "8:30"
                components.seconds = timeStr.substring(6);       // e.g., "45"
            } else {
                components.hoursMins = timeStr.substring(0, 5);  // e.g., "8:30"
            }
        }
    }

    // Build date components
    char dayBuf[16] = "";
    char monthBuf[16] = "";
    if (daySwitch[currentScreen - 1]) {
        if (currentLanguage == "de") {
            const char* format = (daySwitch[currentScreen - 1] && !dateSwitch[currentScreen - 1] && !monthSwitch[currentScreen - 1]) ? days_DE_long[timeinfo.tm_wday] : days_DE_short[timeinfo.tm_wday];
            strncpy(dayBuf, format, sizeof(dayBuf));
        } else if (currentLanguage == "sv") { // ADDED SWEDISH
            const char* format = (daySwitch[currentScreen - 1] && !dateSwitch[currentScreen - 1] && !monthSwitch[currentScreen - 1]) ? days_SE_long[timeinfo.tm_wday] : days_SE_short[timeinfo.tm_wday];
            strncpy(dayBuf, format, sizeof(dayBuf));
        } else {
            strftime(dayBuf, sizeof(dayBuf), (daySwitch[currentScreen - 1] && !dateSwitch[currentScreen - 1] && !monthSwitch[currentScreen - 1]) ? "%A" : "%a", &timeinfo);
        }
        components.day = String(dayBuf);
    }
    if (dateSwitch[currentScreen - 1]) {
        components.date = String(timeinfo.tm_mday);
    }
    if (monthSwitch[currentScreen - 1]) {
        if (currentLanguage == "de") {
            const char* format = (monthSwitch[currentScreen - 1] && !daySwitch[currentScreen - 1] && !dateSwitch[currentScreen - 1]) ? months_DE_long[timeinfo.tm_mon] : months_DE_short[timeinfo.tm_mon];
            strncpy(monthBuf, format, sizeof(monthBuf));
        } else if (currentLanguage == "sv") { // ADDED SWEDISH
            const char* format = (monthSwitch[currentScreen - 1] && !daySwitch[currentScreen - 1] && !dateSwitch[currentScreen - 1]) ? months_SE_long[timeinfo.tm_mon] : months_SE_short[timeinfo.tm_mon];
            strncpy(monthBuf, format, sizeof(monthBuf));
        } else {
            strftime(monthBuf, sizeof(monthBuf), (monthSwitch[currentScreen - 1] && !daySwitch[currentScreen - 1] && !dateSwitch[currentScreen - 1]) ? "%B" : "%b", &timeinfo);
        }
        components.month = String(monthBuf);
    }

    // Add weather components based on switches
    if (temperatureSwitch[currentScreen - 1]) {
        components.apparentTemp = currentApparentTemp + "c";
    }
    if (humiditySwitch[currentScreen - 1]) {
        components.humidity = currentHumidity + "%";
    }
    if (minMaxTempsSwitch[currentScreen - 1]) {
        components.maxTemp = String(todayMaxTemp) + "c";
        components.minTemp = String(todayMinTemp) + "c";
    }
    if (iconsSwitch[currentScreen - 1]) {
        components.weatherIcon = weatherIcon;
    }

    return components;
}







void debounceSaveSettings() {
  if (!settingsChanged) return;
  unsigned long currentTime = millis();
  if (currentTime - lastSaveTime >= saveDelay) {
    saveSettings();
    lastSaveTime = currentTime;
    settingsChanged = false;
    //Serial.println("Settings saved to SPIFFS (debounced)");
  }
}

// This function ensures a value stays within a specified min/max range.
float clampValue(float value, float minVal, float maxVal) {
  // Use the standard min/max functions to clamp the value.
  // It's equivalent to: 
  // if (value < minVal) return minVal;
  // if (value > maxVal) return maxVal;
  // return value;
  return max(minVal, min(value, maxVal));
}

void markSettingsChanged() {
  settingsChanged = true;
}

void saveSettings() {
  if (settingsMutex && xSemaphoreTakeRecursive(settingsMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("[Settings] Could not acquire settingsMutex — skipping save this cycle.");
    return;
  }

  const char* finalPath = "/settings.json";
  const char* tmpPath   = "/settings.json.tmp";
  File file = SPIFFS.open(tmpPath, "w");
  if (!file) {
    Serial.println("Failed to open settings tmp file for writing");
    if (settingsMutex) xSemaphoreGiveRecursive(settingsMutex);
    return;
  }

  DynamicJsonDocument doc(8192);

  JsonArray screensArray = doc.createNestedArray("screens");

  for (const auto& settings : allScreenSettings) {
    JsonObject screenObj = screensArray.createNestedObject();
    screenObj["time_col_r"] = settings.time_col.r;
    screenObj["time_col_g"] = settings.time_col.g;
    screenObj["time_col_b"] = settings.time_col.b;
    screenObj["date_col_r"] = settings.date_col.r;
    screenObj["date_col_g"] = settings.date_col.g;
    screenObj["date_col_b"] = settings.date_col.b;
    screenObj["dateBG_col_r"] = settings.dateBG_col.r;
    screenObj["dateBG_col_g"] = settings.dateBG_col.g;
    screenObj["dateBG_col_b"] = settings.dateBG_col.b;
    screenObj["temp_col_r"] = settings.temp_col.r;
    screenObj["temp_col_g"] = settings.temp_col.g;
    screenObj["temp_col_b"] = settings.temp_col.b;
    screenObj["humidity_col_r"] = settings.humidity_col.r;
    screenObj["humidity_col_g"] = settings.humidity_col.g;
    screenObj["humidity_col_b"] = settings.humidity_col.b;
    screenObj["day_col_r"] = settings.day_col.r;
    screenObj["day_col_g"] = settings.day_col.g;
    screenObj["day_col_b"] = settings.day_col.b;
    screenObj["month_col_r"] = settings.month_col.r;
    screenObj["month_col_g"] = settings.month_col.g;
    screenObj["month_col_b"] = settings.month_col.b;
    screenObj["ampm_col_r"] = settings.ampm_col.r;
    screenObj["ampm_col_g"] = settings.ampm_col.g;
    screenObj["ampm_col_b"] = settings.ampm_col.b;
    screenObj["seconds_col_r"] = settings.seconds_col.r;
    screenObj["seconds_col_g"] = settings.seconds_col.g;
    screenObj["seconds_col_b"] = settings.seconds_col.b;
    screenObj["land_col_r"] = settings.land_col.r;
    screenObj["land_col_g"] = settings.land_col.g;
    screenObj["land_col_b"] = settings.land_col.b;
    screenObj["water_col_r"] = settings.water_col.r;
    screenObj["water_col_g"] = settings.water_col.g;
    screenObj["water_col_b"] = settings.water_col.b;
    screenObj["ice_col_r"] = settings.ice_col.r;
    screenObj["ice_col_g"] = settings.ice_col.g;
    screenObj["ice_col_b"] = settings.ice_col.b;
    screenObj["ampmSwitch"] = settings.ampmSwitch;
    screenObj["secondsSwitch"] = settings.secondsSwitch;
    screenObj["twentyFourHourSwitch"] = settings.twentyFourHourSwitch;
    screenObj["iconsSwitch"] = settings.iconsSwitch;
    screenObj["temperatureSwitch"] = settings.temperatureSwitch;
    screenObj["minMaxTempsSwitch"] = settings.minMaxTempsSwitch;
    screenObj["daySwitch"] = settings.daySwitch;
    screenObj["dateSwitch"] = settings.dateSwitch;
    screenObj["monthSwitch"] = settings.monthSwitch;
    screenObj["humiditySwitch"] = settings.humiditySwitch;
    screenObj["landSwitch"] = settings.landSwitch;
    screenObj["waterSwitch"] = settings.waterSwitch;
    screenObj["iceSwitch"] = settings.iceSwitch;
    screenObj["clockSwitch"] = settings.clockSwitch;
    screenObj["backgroundSwitch"] = settings.backgroundSwitch;
    screenObj["SpareSwitch"] = settings.SpareSwitch;
    screenObj["SpareSwitch2"] = settings.SpareSwitch2;
    screenObj["SpareSwitch3"] = settings.SpareSwitch3;
    screenObj["pageSlider"] = settings.pageSlider;
    screenObj["pageSlider2"] = settings.pageSlider2;
    screenObj["pageSlider3"] = settings.pageSlider3;
    screenObj["land_use_image"] = settings.land_use_image;
    screenObj["water_use_image"] = settings.water_use_image;
    screenObj["ice_use_image"] = settings.ice_use_image;
    screenObj["clock_use_image"] = settings.clock_use_image;
    screenObj["land_bitmap_index"] = settings.land_bitmap_index;
    screenObj["water_bitmap_index"] = settings.water_bitmap_index;
    screenObj["ice_bitmap_index"] = settings.ice_bitmap_index;
    screenObj["clock_bitmap_index"] = settings.clock_bitmap_index;
    screenObj["clockDisplayMode"] = settings.clockDisplayMode;
    screenObj["markerDisplayMode"] = settings.markerDisplayMode;
    screenObj["numberColorMode"] = settings.numberColorMode;
    screenObj["starColorMode"] = settings.starColorMode;
    screenObj["number_color_r"] = settings.number_color.r;
    screenObj["number_color_g"] = settings.number_color.g;
    screenObj["number_color_b"] = settings.number_color.b;
    screenObj["star_color_r"] = settings.star_color.r;
    screenObj["star_color_g"] = settings.star_color.g;
    screenObj["star_color_b"] = settings.star_color.b;
    screenObj["hourHandSwitch"] = settings.hourHandSwitch;
    screenObj["minuteHandSwitch"] = settings.minuteHandSwitch;
    screenObj["secondHandSwitch"] = settings.secondHandSwitch;
  }

  doc["auto_brightness"] = autoBrightnessEnabled;
  doc["timezone"] = selectedTimezone;
  doc["weather_service"] = selectedWeatherService;
  doc["pirate_weather_api"] = pirateWeatherAPI;
  doc["openweathermap_api"] = openWeatherMapAPI;
  doc["weatherapi_api"] = weatherAPI_API;
  doc["gps_lat"] = gpsLat;
  doc["gps_lon"] = gpsLon;
  doc["brightness"] = brightness;
  doc["dark_room_brightness"] = darkRoomBrightness;
  doc["bright_room_brightness"] = brightRoomBrightness;
  doc["dark_room_ldr_value"] = darkRoomLDRValue;
  doc["bright_room_ldr_value"] = brightRoomLDRValue;
  doc["current_screen"] = currentScreen;
  doc["units"] = tempUnits;
  doc["temp_type"] = tempType;
  doc["indoor_temp_offset"] = indoorTempOffset; // NEW: Save the offset

  doc["temperature"] = currentTemperature;
  doc["apparent_temperature"] = currentApparentTemperature;
  doc["humidity"] = currentHumidityFloat;
  doc["conditions"] = currentConditions;
  doc["min_temp"] = todayMinTemp;
  doc["max_temp"] = todayMaxTemp;
  doc["weather_icon"] = weatherIcon;
  doc["moon_Phase"] = moonPhase;
  doc["moon_Percentage"] = moonPercentage;
  doc["language"] = currentLanguage; 
  doc["schedulesEnabled"] = schedulesEnabled;
  doc["defaultScreen"] = defaultScreen;
  JsonArray schedulesJson = doc.createNestedArray("schedules");
  for (const auto& sched : schedules) {
    JsonObject s = schedulesJson.createNestedObject();
    s["screen"] = sched.screen;
    s["sh"] = sched.start_hour;
    s["sm"] = sched.start_min;
    s["eh"] = sched.end_hour;
    s["em"] = sched.end_min;
  }
  doc["panel_type"] = panelType;
  //Serial.println("[Save Settings] Saving panel_type as: " + panelType); 

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to settings tmp file");
    file.close();
    SPIFFS.remove(tmpPath);
    if (settingsMutex) xSemaphoreGiveRecursive(settingsMutex);
    return;
  }
  file.close();

  SPIFFS.remove(finalPath);           // SPIFFS.rename() refuses to overwrite an existing dest
  if (!SPIFFS.rename(tmpPath, finalPath)) {
    Serial.println("CRITICAL: failed to rename settings tmp file into place!");
  } else {
    Serial.println("Settings saved to SPIFFS (atomic)");
  }
  if (settingsMutex) xSemaphoreGiveRecursive(settingsMutex);
}

void loadSettings() {
  if (settingsMutex) xSemaphoreTakeRecursive(settingsMutex, pdMS_TO_TICKS(5000));

  if (!SPIFFS.exists("/settings.json") && SPIFFS.exists("/settings.json.tmp")) {
    Serial.println("[Settings] Recovering orphaned .tmp left by an interrupted save.");
    SPIFFS.rename("/settings.json.tmp", "/settings.json");
  }

  File file = SPIFFS.open("/settings.json", "r");
  if (!file) {
    Serial.println("Failed to open settings file for reading, using defaults");
    if (settingsMutex) xSemaphoreGiveRecursive(settingsMutex);
    return;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Failed to parse settings file: " + String(error.c_str()));
    file.close();
    if (settingsMutex) xSemaphoreGiveRecursive(settingsMutex);
    return;
  }

  JsonArray screensArray = doc["screens"];
  if (screensArray) {
    size_t screensToLoad = min((size_t)screensArray.size(), (size_t)NUM_CLOCK_SCREENS);
    allScreenSettings.resize(NUM_CLOCK_SCREENS);

    for (size_t i = 0; i < screensToLoad; ++i) {
      JsonObject screenObj = screensArray[i];
      if (screenObj) {
        allScreenSettings[i].time_col.r = screenObj["time_col_r"] | 118;
        allScreenSettings[i].time_col.g = screenObj["time_col_g"] | 251;
        allScreenSettings[i].time_col.b = screenObj["time_col_b"] | 78;
        allScreenSettings[i].date_col.r = screenObj["date_col_r"] | 214;
        allScreenSettings[i].date_col.g = screenObj["date_col_g"] | 214;
        allScreenSettings[i].date_col.b = screenObj["date_col_b"] | 214;
        allScreenSettings[i].dateBG_col.r = screenObj["dateBG_col_r"] | 71;
        allScreenSettings[i].dateBG_col.g = screenObj["dateBG_col_g"] | 36;
        allScreenSettings[i].dateBG_col.b = screenObj["dateBG_col_b"] | 171;
        allScreenSettings[i].temp_col.r = screenObj["temp_col_r"] | 255;
        allScreenSettings[i].temp_col.g = screenObj["temp_col_g"] | 98;
        allScreenSettings[i].temp_col.b = screenObj["temp_col_b"] | 80;
        allScreenSettings[i].humidity_col.r = screenObj["humidity_col_r"] | 40;
        allScreenSettings[i].humidity_col.g = screenObj["humidity_col_g"] | 95;
        allScreenSettings[i].humidity_col.b = screenObj["humidity_col_b"] | 244;
        allScreenSettings[i].day_col.r = screenObj["day_col_r"] | 245;
        allScreenSettings[i].day_col.g = screenObj["day_col_g"] | 236;
        allScreenSettings[i].day_col.b = screenObj["day_col_b"] | 0;
        allScreenSettings[i].month_col.r = screenObj["month_col_r"] | 214;
        allScreenSettings[i].month_col.g = screenObj["month_col_g"] | 214;
        allScreenSettings[i].month_col.b = screenObj["month_col_b"] | 214;
        allScreenSettings[i].ampm_col.r = screenObj["ampm_col_r"] | 100;
        allScreenSettings[i].ampm_col.g = screenObj["ampm_col_g"] | 100;
        allScreenSettings[i].ampm_col.b = screenObj["ampm_col_b"] | 100;
        allScreenSettings[i].seconds_col.r = screenObj["seconds_col_r"] | 100;
        allScreenSettings[i].seconds_col.g = screenObj["seconds_col_g"] | 100;
        allScreenSettings[i].seconds_col.b = screenObj["seconds_col_b"] | 100;
        allScreenSettings[i].land_col.r = screenObj["land_col_r"] | 119;
        allScreenSettings[i].land_col.g = screenObj["land_col_g"] | 187;
        allScreenSettings[i].land_col.b = screenObj["land_col_b"] | 65;
        allScreenSettings[i].water_col.r = screenObj["water_col_r"] | 0;
        allScreenSettings[i].water_col.g = screenObj["water_col_g"] | 66;
        allScreenSettings[i].water_col.b = screenObj["water_col_b"] | 170;
        allScreenSettings[i].ice_col.r = screenObj["ice_col_r"] | 192;
        allScreenSettings[i].ice_col.g = screenObj["ice_col_g"] | 192;
        allScreenSettings[i].ice_col.b = screenObj["ice_col_b"] | 192;
        allScreenSettings[i].ampmSwitch = screenObj["ampmSwitch"] | false;
        allScreenSettings[i].secondsSwitch = screenObj["secondsSwitch"] | false;
        allScreenSettings[i].twentyFourHourSwitch = screenObj["twentyFourHourSwitch"] | false;
        allScreenSettings[i].iconsSwitch = screenObj["iconsSwitch"] | true;
        allScreenSettings[i].temperatureSwitch = screenObj["temperatureSwitch"] | true;
        allScreenSettings[i].minMaxTempsSwitch = screenObj["minMaxTempsSwitch"] | true;
        allScreenSettings[i].daySwitch = screenObj["daySwitch"] | true;
        allScreenSettings[i].dateSwitch = screenObj["dateSwitch"] | true;
        allScreenSettings[i].monthSwitch = screenObj["monthSwitch"] | true;
        allScreenSettings[i].humiditySwitch = screenObj["humiditySwitch"] | true;
        allScreenSettings[i].landSwitch = screenObj["landSwitch"] | true;
        allScreenSettings[i].waterSwitch = screenObj["waterSwitch"] | true;
        allScreenSettings[i].iceSwitch = screenObj["iceSwitch"] | true;
        allScreenSettings[i].clockSwitch = screenObj["clockSwitch"] | true;
        allScreenSettings[i].backgroundSwitch = screenObj["backgroundSwitch"] | false;
        allScreenSettings[i].SpareSwitch = screenObj["SpareSwitch"] | false;
        allScreenSettings[i].SpareSwitch2 = screenObj["SpareSwitch2"] | false;
        allScreenSettings[i].SpareSwitch3 = screenObj["SpareSwitch3"] | false;
        allScreenSettings[i].pageSlider = screenObj["pageSlider"] | 128;
        allScreenSettings[i].pageSlider2 = screenObj["pageSlider2"] | 128;
        allScreenSettings[i].pageSlider3 = screenObj["pageSlider3"] | 128;
        allScreenSettings[i].land_use_image = screenObj["land_use_image"] | false;
        allScreenSettings[i].water_use_image = screenObj["water_use_image"] | false;
        allScreenSettings[i].ice_use_image = screenObj["ice_use_image"] | false;
        allScreenSettings[i].clock_use_image = screenObj["clock_use_image"] | false;
        allScreenSettings[i].land_bitmap_index = screenObj["land_bitmap_index"] | 0;
        allScreenSettings[i].water_bitmap_index = screenObj["water_bitmap_index"] | 0;
        allScreenSettings[i].ice_bitmap_index = screenObj["ice_bitmap_index"] | 0;
        allScreenSettings[i].clock_bitmap_index = screenObj["clock_bitmap_index"] | 1;
        allScreenSettings[i].clockDisplayMode = screenObj["clockDisplayMode"] | "4 numbers";
        allScreenSettings[i].markerDisplayMode = screenObj["markerDisplayMode"] | "4 Numbers + Stars";
        allScreenSettings[i].numberColorMode = screenObj["numberColorMode"] | "Rainbow";
        allScreenSettings[i].starColorMode = screenObj["starColorMode"] | "Rainbow";
        allScreenSettings[i].number_color.r = screenObj["number_color_r"] | 200;
        allScreenSettings[i].number_color.g = screenObj["number_color_g"] | 200;
        allScreenSettings[i].number_color.b = screenObj["number_color_b"] | 200;
        allScreenSettings[i].star_color.r = screenObj["star_color_r"] | 150;
        allScreenSettings[i].star_color.g = screenObj["star_color_g"] | 150;
        allScreenSettings[i].star_color.b = screenObj["star_color_b"] | 150;
        allScreenSettings[i].hourHandSwitch = screenObj["hourHandSwitch"] | true;
        allScreenSettings[i].minuteHandSwitch = screenObj["minuteHandSwitch"] | true;
        allScreenSettings[i].secondHandSwitch = screenObj["secondHandSwitch"] | true;
      }
    }
  }

  autoBrightnessEnabled = doc["auto_brightness"] | false;
  selectedTimezone = doc["timezone"] | String("UTC0");
  selectedWeatherService = doc["weather_service"] | String("pirateweather");
  pirateWeatherAPI = doc["pirate_weather_api"] | String("");
  openWeatherMapAPI = doc["openweathermap_api"] | String("");
  weatherAPI_API = doc["weatherapi_api"] | String("");
  gpsLat = doc["gps_lat"] | 0.0;
  gpsLon = doc["gps_lon"] | 0.0;
  brightness = doc["brightness"] | 150;
  darkRoomBrightness = doc["dark_room_brightness"] | 20;
  brightRoomBrightness = doc["bright_room_brightness"] | 255;
  darkRoomLDRValue = doc["dark_room_ldr_value"] | 50;
  brightRoomLDRValue = doc["bright_room_ldr_value"] | 4095;
  currentScreen = doc["current_screen"] | 1;
  tempUnits = doc["units"] | String("celsius");
  tempType = doc["temp_type"] | String("actual");
  indoorTempOffset = doc["indoor_temp_offset"] | 0.0; // NEW: Load the offset
  currentLanguage = doc["language"] | String("en");
  schedulesEnabled = doc["schedulesEnabled"] | false;
  defaultScreen = doc["defaultScreen"] | 1;
  schedules.clear(); // Clear existing schedules before loading
  JsonArray schedulesJson = doc["schedules"];
  if (schedulesJson) {
    for (JsonObject s : schedulesJson) {
      Schedule newSched;
      newSched.screen = s["screen"];
      newSched.start_hour = s["sh"];
      newSched.start_min = s["sm"];
      newSched.end_hour = s["eh"];
      newSched.end_min = s["em"];
      schedules.push_back(newSched);
    }
  }
  Serial.printf("Loaded %d schedules.\n", schedules.size());
  panelType = doc["panel_type"] | String("P5.0");

  currentTemperature = doc["temperature"] | 0.0;
  currentApparentTemperature = doc["apparent_temperature"] | 0.0;
  currentHumidityFloat = doc["humidity"] | 0.0;
  currentConditions = doc["conditions"] | String("Unknown");
  todayMinTemp = doc["min_temp"] | 0;
  todayMaxTemp = doc["max_temp"] | 0;
  weatherIcon = doc["weather_icon"] | String("none");
  moonPhase = doc["moon_Phase"] | 0.125;
  moonPercentage = doc["moon_Percentage"] | 25.0;

  currentTemp = String(currentTemperature, 1);
  currentApparentTemp = String(currentApparentTemperature, 1);
  currentHumidity = String((int)currentHumidityFloat);

  file.close();
  Serial.println("Settings loaded from SPIFFS into new structure");
  if (settingsMutex) xSemaphoreGiveRecursive(settingsMutex);
}

void handleSettings() {
  DynamicJsonDocument doc(4096);
  
  int screenIndex = currentScreen - 1;

  if (screenIndex < 0 || screenIndex >= allScreenSettings.size()) {
    screenIndex = 0;
  }

  const auto& settings = allScreenSettings[screenIndex];

  doc["version"] = ver;
  doc["safe_mode"] = safeModeActive; // Reboot Guard tripped; UI backs off to its slowest refresh
  doc["ampm_switch"] = settings.ampmSwitch;
  doc["seconds_switch"] = settings.secondsSwitch;
  doc["24hour_switch"] = settings.twentyFourHourSwitch;
  doc["icons_switch"] = settings.iconsSwitch;
  doc["temperature_switch"] = settings.temperatureSwitch;
  doc["minmax_temps_switch"] = settings.minMaxTempsSwitch;
  doc["day_switch"] = settings.daySwitch;
  doc["date_switch"] = settings.dateSwitch;
  doc["month_switch"] = settings.monthSwitch;
  doc["humidity_switch"] = settings.humiditySwitch;
  doc["spare_switch"] = settings.SpareSwitch;
  doc["spare_switch2"] = settings.SpareSwitch2;
  doc["spare_switch3"] = settings.SpareSwitch3;
  doc["land_switch"] = settings.landSwitch;
  doc["water_switch"] = settings.waterSwitch;
  doc["ice_switch"] = settings.iceSwitch;
  doc["clock_switch"] = settings.clockSwitch;
  doc["backgroundSwitch"] = settings.backgroundSwitch;
  doc["land_option"] = settings.land_use_image ? "image" : "colour";
  doc["water_option"] = settings.water_use_image ? "image" : "colour";
  doc["ice_option"] = settings.ice_use_image ? "image" : "colour";
  doc["clock_option"] = settings.clock_use_image ? "image" : "colour";
  doc["land_bitmap_index"] = settings.land_bitmap_index;
  doc["water_bitmap_index"] = settings.water_bitmap_index;
  doc["ice_bitmap_index"] = settings.ice_bitmap_index;
  doc["clock_bitmap_index"] = settings.clock_bitmap_index;
  doc["clockDisplayMode"] = settings.clockDisplayMode;
  doc["page_slider"] = settings.pageSlider;
  doc["page_slider2"] = settings.pageSlider2;
  doc["page_slider3"] = settings.pageSlider3;
  doc["markerDisplayMode"] = settings.markerDisplayMode;
  doc["numberColorMode"] = settings.numberColorMode;
  doc["starColorMode"] = settings.starColorMode;
  doc["hourHandSwitch"] = settings.hourHandSwitch;
  doc["minuteHandSwitch"] = settings.minuteHandSwitch;
  doc["secondHandSwitch"] = settings.secondHandSwitch;
  
  JsonObject timeCol = doc.createNestedObject("time_color");
  timeCol["r"] = settings.time_col.r; timeCol["g"] = settings.time_col.g; timeCol["b"] = settings.time_col.b;
  JsonObject ampmCol = doc.createNestedObject("ampm_color");
  ampmCol["r"] = settings.ampm_col.r; ampmCol["g"] = settings.ampm_col.g; ampmCol["b"] = settings.ampm_col.b;
  JsonObject secondsCol = doc.createNestedObject("seconds_color");
  secondsCol["r"] = settings.seconds_col.r; secondsCol["g"] = settings.seconds_col.g; secondsCol["b"] = settings.seconds_col.b;
  JsonObject dayCol = doc.createNestedObject("day_color");
  dayCol["r"] = settings.day_col.r; dayCol["g"] = settings.day_col.g; dayCol["b"] = settings.day_col.b;
  JsonObject dateCol = doc.createNestedObject("date_color");
  dateCol["r"] = settings.date_col.r; dateCol["g"] = settings.date_col.g; dateCol["b"] = settings.date_col.b;
  JsonObject monthCol = doc.createNestedObject("month_color");
  monthCol["r"] = settings.month_col.r; monthCol["g"] = settings.month_col.g; monthCol["b"] = settings.month_col.b;
  JsonObject dateBGCol = doc.createNestedObject("date_bg_color");
  dateBGCol["r"] = settings.dateBG_col.r; dateBGCol["g"] = settings.dateBG_col.g; dateBGCol["b"] = settings.dateBG_col.b;
  JsonObject tempCol = doc.createNestedObject("temp_color");
  tempCol["r"] = settings.temp_col.r; tempCol["g"] = settings.temp_col.g; tempCol["b"] = settings.temp_col.b;
  JsonObject humidityCol = doc.createNestedObject("humidity_color");
  humidityCol["r"] = settings.humidity_col.r; humidityCol["g"] = settings.humidity_col.g; humidityCol["b"] = settings.humidity_col.b;
  JsonObject landCol = doc.createNestedObject("land_color");
  landCol["r"] = settings.land_col.r; landCol["g"] = settings.land_col.g; landCol["b"] = settings.land_col.b;
  JsonObject waterCol = doc.createNestedObject("water_color");
  waterCol["r"] = settings.water_col.r; waterCol["g"] = settings.water_col.g; waterCol["b"] = settings.water_col.b;
  JsonObject iceCol = doc.createNestedObject("ice_color");
  iceCol["r"] = settings.ice_col.r; iceCol["g"] = settings.ice_col.g; iceCol["b"] = settings.ice_col.b;
  JsonObject numberCol = doc.createNestedObject("number_color");
  numberCol["r"] = settings.number_color.r; numberCol["g"] = settings.number_color.g; numberCol["b"] = settings.number_color.b;
  JsonObject starCol = doc.createNestedObject("star_color");
  starCol["r"] = settings.star_color.r; starCol["g"] = settings.star_color.g; starCol["b"] = settings.star_color.b;

  doc["auto_brightness"] = autoBrightnessEnabled;
  doc["timezone"] = selectedTimezone;
  doc["weather_service"] = selectedWeatherService;
  doc["pirate_weather_api"] = pirateWeatherAPI;
  doc["openweathermap_api"] = openWeatherMapAPI;
  doc["weatherapi_api"] = weatherAPI_API;
  doc["gps_lat"] = gpsLat;
  doc["gps_lon"] = gpsLon;
  doc["brightness"] = brightness;
  doc["dark_room_brightness"] = darkRoomBrightness;
  doc["bright_room_brightness"] = brightRoomBrightness;
  doc["dark_room_ldr_value"] = darkRoomLDRValue;
  doc["bright_room_ldr_value"] = brightRoomLDRValue;
  doc["current_screen"] = currentScreen;
  doc["temperature"] = currentTemp;
  doc["apparent_temperature"] = currentApparentTemp;
  doc["units"] = tempUnits;
  doc["temp_type"] = tempType;
  doc["indoor_temp_offset"] = indoorTempOffset; // NEW: Send offset to webpage
  doc["humidity"] = currentHumidity;
  doc["conditions"] = currentConditions;
  doc["min_temp"] = todayMinTemp;
  doc["max_temp"] = todayMaxTemp;
  doc["weather_icon"] = weatherIcon;
  doc["language"] = currentLanguage; 
  doc["schedulesEnabled"] = schedulesEnabled;
  doc["defaultScreen"] = defaultScreen;
  JsonArray schedulesJson = doc.createNestedArray("schedules");
  for (const auto& sched : schedules) {
    JsonObject s = schedulesJson.createNestedObject();
    s["screen"] = sched.screen;
    // Format time as "HH:MM" for easy use in JavaScript
    char startTime[6], endTime[6];
    sprintf(startTime, "%02d:%02d", sched.start_hour, sched.start_min);
    sprintf(endTime, "%02d:%02d", sched.end_hour, sched.end_min);
    s["start"] = startTime;
    s["end"] = endTime;
  }
  doc["panel_type"] = panelType;
  //Serial.println("[Load Settings] Loaded panel_type from file: " + panelType);

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}
 
void handleWeatherService() {
  if (server.hasArg("service")) {
    selectedWeatherService = server.arg("service");
    Serial.println("Weather Service set to: " + selectedWeatherService);
    apiCallCount = 0; // RESET API CALL COUNT
    needWeatherUpdate = true; // was: fetchWeather(); — that blocked the whole web server for up to 15s
    saveSettings();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing service parameter");
  }
}

void handlePirateWeatherAPI() {
  if (server.hasArg("api")) {
    String apiKey = server.arg("api");
    pirateWeatherAPI = apiKey;
    Serial.println("Pirate Weather API Key updated.");
    apiCallCount = 0; // RESET API CALL COUNT
    needWeatherUpdate = true; // was: fetchWeather() — blocked the whole web server for up to 15s
    saveSettings();
    server.send(200, "text/plain", "API Key OK: " + apiKey);
  } else {
    server.send(400, "text/plain", "Missing API key parameter");
  }
}

void handleOpenWeatherMapAPI() {
  if (server.hasArg("api")) {
    openWeatherMapAPI = server.arg("api");
    Serial.println("OpenWeatherMap API Key updated.");
    apiCallCount = 0; // RESET API CALL COUNT
    needWeatherUpdate = true; // was: fetchWeather() — blocked the whole web server for up to 15s
    saveSettings();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing API key parameter");
  }
}

void handleWeatherAPI_API() {
  if (server.hasArg("api")) {
    weatherAPI_API = server.arg("api");
    Serial.println("WeatherAPI.com API Key updated.");
    apiCallCount = 0; // RESET API CALL COUNT
    needWeatherUpdate = true; // was: fetchWeather() — blocked the whole web server for up to 15s
    saveSettings();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing API key parameter");
  }
}

void handleLandSwitch() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].landSwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleWaterSwitch() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].waterSwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleIceSwitch() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].iceSwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleLandBitmap() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    int value = server.arg("value").toInt();
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      if (value >= 0 && value < BITMAP_COUNT) {
        allScreenSettings[screenIndex].land_bitmap_index = value;
        saveSettings();
        server.send(200, "text/plain", "OK");
      } else {
        server.send(400, "text/plain", "Invalid bitmap index");
      }
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleWaterBitmap() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    int value = server.arg("value").toInt();
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      if (value >= 0 && value < BITMAP_COUNT) {
        allScreenSettings[screenIndex].water_bitmap_index = value;
        saveSettings();
        server.send(200, "text/plain", "OK");
      } else {
        server.send(400, "text/plain", "Invalid bitmap index");
      }
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleIceBitmap() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    int value = server.arg("value").toInt();
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      if (value >= 0 && value < BITMAP_COUNT) {
        allScreenSettings[screenIndex].ice_bitmap_index = value;
        saveSettings();
        server.send(200, "text/plain", "OK");
      } else {
        server.send(400, "text/plain", "Invalid bitmap index");
      }
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleLandOption() {
  // This handler is called when the user selects 'Colour' or 'Image' from the dropdown
  // in the expandable "Land" section of the web UI.
  
  // Check if the required 'value' and 'screen' arguments were provided.
  if (server.hasArg("value") && server.hasArg("screen")) {
    
    // Get the arguments from the request.
    String value = server.arg("value"); // Will be "colour" or "image"
    int screen = server.arg("screen").toInt();
    
    // Convert to 0-based index.
    int screenIndex = screen - 1;

    // --- Safety Check: Validate the screenIndex ---
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      
      // ** THE CORRECTION **
      // Update the 'land_use_image' boolean for the correct screen in our settings vector.
      allScreenSettings[screenIndex].land_use_image = (value == "image");
      
      Serial.println("Set land_use_image[" + String(screenIndex) + "] to " + String(allScreenSettings[screenIndex].land_use_image));
      
      // Save all settings to SPIFFS.
      saveSettings();
      
      // Send a success response.
      server.send(200, "text/plain", "OK");

    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleWaterOption() {
  // This handler is called when the user selects 'Colour' or 'Image' from the "Water" dropdown.
  
  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    // --- Safety Check: Validate the screenIndex ---
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      
      // ** THE CORRECTION **
      // Update the 'water_use_image' boolean for the correct screen.
      allScreenSettings[screenIndex].water_use_image = (value == "image");
      
      Serial.println("Set water_use_image[" + String(screenIndex) + "] to " + String(allScreenSettings[screenIndex].water_use_image));
      
      saveSettings();
      server.send(200, "text/plain", "OK");

    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleIceOption() {
  // This handler is called when the user selects 'Colour' or 'Image' from the "Ice" dropdown.

  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    // --- Safety Check: Validate the screenIndex ---
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      
      // ** THE CORRECTION **
      // Update the 'ice_use_image' boolean for the correct screen.
      allScreenSettings[screenIndex].ice_use_image = (value == "image");
      
      Serial.println("Set ice_use_image[" + String(screenIndex) + "] to " + String(allScreenSettings[screenIndex].ice_use_image));
      
      saveSettings();
      server.send(200, "text/plain", "OK");

    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleScreenSelection() {
  // This handler is called when the user selects a screen from the main dropdown on the web UI.
  
  // Check if the required 'value' argument was provided.
  if (server.hasArg("value")) {
    
    // Get the value from the request and convert it to an integer.
    int newScreen = server.arg("value").toInt();
    
    // ** THE CORRECTION **
    // Validate the new screen number against our dynamic NUM_CLOCK_SCREENS constant.
    // This ensures that only valid, existing clock screens can be selected.
    if (newScreen >= 1 && newScreen <= NUM_CLOCK_SCREENS) {
      
      // Set the global currentScreen variable.
      currentScreen = newScreen;
      
      // Save the setting so it's remembered on the next boot.
      saveSettings();
      
      Serial.println("Screen set to: " + String(currentScreen));
      
      // Send a success response.
      server.send(200, "text/plain", "Screen set to " + String(currentScreen));

    } else {
      // The screen number was invalid (e.g., 0, or greater than NUM_CLOCK_SCREENS).
      server.send(400, "text/plain", "Invalid screen number");
    }
  } else {
    // The request was missing the 'value' argument.
    server.send(400, "text/plain", "No screen value provided");
  }
}

void handleMarkerDisplayMode() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    int screenIndex = server.arg("screen").toInt() - 1;
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].markerDisplayMode = server.arg("value");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleNumberColorMode() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    int screenIndex = server.arg("screen").toInt() - 1;
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].numberColorMode = server.arg("value");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleStarColorMode() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    int screenIndex = server.arg("screen").toInt() - 1;
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].starColorMode = server.arg("value");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleNumberColor() {
    if (server.hasArg("value") && server.hasArg("screen")) {
        int screenIndex = server.arg("screen").toInt() - 1;
        if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
            unsigned long color = strtoul(server.arg("value").c_str(), NULL, 16);
            allScreenSettings[screenIndex].number_color.r = (color >> 16) & 0xFF;
            allScreenSettings[screenIndex].number_color.g = (color >> 8) & 0xFF;
            allScreenSettings[screenIndex].number_color.b = color & 0xFF;
            saveSettings();
            server.send(200, "text/plain", "OK");
        }
    }
}

void handleStarColor() {
    if (server.hasArg("value") && server.hasArg("screen")) {
        int screenIndex = server.arg("screen").toInt() - 1;
        if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
            unsigned long color = strtoul(server.arg("value").c_str(), NULL, 16);
            allScreenSettings[screenIndex].star_color.r = (color >> 16) & 0xFF;
            allScreenSettings[screenIndex].star_color.g = (color >> 8) & 0xFF;
            allScreenSettings[screenIndex].star_color.b = color & 0xFF;
            saveSettings();
            server.send(200, "text/plain", "OK");
        }
    }
}

void handleHourHandSwitch() {
    if (server.hasArg("state") && server.hasArg("screen")) {
        int screenIndex = server.arg("screen").toInt() - 1;
        if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
            allScreenSettings[screenIndex].hourHandSwitch = (server.arg("state") == "true");
            saveSettings();
            server.send(200, "text/plain", "OK");
        }
    }
}

void handleMinuteHandSwitch() {
    if (server.hasArg("state") && server.hasArg("screen")) {
        int screenIndex = server.arg("screen").toInt() - 1;
        if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
            allScreenSettings[screenIndex].minuteHandSwitch = (server.arg("state") == "true");
            saveSettings();
            server.send(200, "text/plain", "OK");
        }
    }
}

void handleSecondHandSwitch() {
    if (server.hasArg("state") && server.hasArg("screen")) {
        int screenIndex = server.arg("screen").toInt() - 1;
        if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
            allScreenSettings[screenIndex].secondHandSwitch = (server.arg("state") == "true");
            saveSettings();
            server.send(200, "text/plain", "OK");
        }
    }
}

void handleBackgroundSwitch() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      // This correctly updates the backgroundSwitch member for the specified screen
      allScreenSettings[screenIndex].backgroundSwitch = (state == "true");
      
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleGPS() {
  if (server.hasArg("lat") && server.hasArg("lon")) {
    String latStr = server.arg("lat");
    String lonStr = server.arg("lon");
    gpsLat = latStr.toFloat();
    gpsLon = lonStr.toFloat();
    Serial.print("GPS Coordinates Updated - Latitude: ");
    Serial.print(gpsLat);
    Serial.print(", Longitude: ");
    Serial.println(gpsLon);
    apiCallCount = 0; // RESET API CALL COUNT
    fetchWeather();
    saveSettings();
    server.send(200, "text/plain", "GPS OK - Lat: " + latStr + ", Lon: " + lonStr);
  } else {
    server.send(400, "text/plain", "Missing GPS parameters");
  }
}

// Helper function to map OpenWeatherMap icon codes to your standard icon names
String mapOwmIconToStandard(String owmIcon, bool isDay) {
    if (owmIcon == "01d" || owmIcon == "01n") return isDay ? "clear-day" : "clear-night";
    if (owmIcon == "02d" || owmIcon == "02n") return isDay ? "partly-cloudy-day" : "partly-cloudy-night";
    if (owmIcon == "03d" || owmIcon == "03n" || owmIcon == "04d" || owmIcon == "04n") return "cloudy";
    if (owmIcon == "09d" || owmIcon == "09n") return "rain";
    if (owmIcon == "10d" || owmIcon == "10n") return "rain";
    if (owmIcon == "11d" || owmIcon == "11n") return "thunderstorm";
    if (owmIcon == "13d" || owmIcon == "13n") return "snow";
    if (owmIcon == "50d" || owmIcon == "50n") return "fog";
    return "none"; // Default fallback
}

// Helper function to map WeatherAPI.com condition codes to your standard icon names
String mapWeatherApiCodeToStandard(int code, bool isDay) {
    if (code == 1000) return isDay ? "clear-day" : "clear-night";
    if (code == 1003) return isDay ? "partly-cloudy-day" : "partly-cloudy-night";
    if (code == 1006 || code == 1009) return "cloudy";
    if (code == 1030 || code == 1135 || code == 1147) return "fog";
    if ((code >= 1180 && code <= 1201) || (code >= 1240 && code <= 1246)) return "rain";
    if (code == 1063 || code == 1069 || code == 1072 || (code >= 1150 && code <= 1171)) return "rain";
    if (code == 1204 || code == 1207 || code == 1249 || code == 1252) return "sleet";
    if (code >= 1273 && code <= 1276) return "thunderstorm";
    if (code == 1279 || code == 1282) return "thunderstorm";
    if (code == 1066 || code == 1114 || code == 1117 || (code >= 1210 && code <= 1225) || (code >= 1255 && code <= 1264)) return "snow";
    return "none"; // Default fallback
}

// Helper function to map WeatherAPI.com text moon phase to a numeric value (0.0-1.0)
float mapWeatherApiMoonPhase(String phase) {
    phase.toLowerCase();
    if (phase == "new moon") return 0.0;
    if (phase == "waxing crescent") return 0.125;
    if (phase == "first quarter") return 0.25;
    if (phase == "waxing gibbous") return 0.375;
    if (phase == "full moon") return 0.5;
    if (phase == "waning gibbous") return 0.625;
    if (phase == "last quarter" || phase == "third quarter") return 0.75;
    if (phase == "waning crescent") return 0.875;
    return 0.0; // Default
}

void fetchWeather() {
  // === Handle "None" service option =========================
  if (selectedWeatherService == "none") {
    Serial.println("Weather service is set to 'None'. Clearing weather data.");
    if (settingsMutex) xSemaphoreTakeRecursive(settingsMutex, pdMS_TO_TICKS(5000));
    currentTemp = "";
    currentApparentTemp = "";
    currentHumidity = "";
    currentConditions = "N/A";
    dailyMinMaxTemps = "";
    weatherIcon = "none";
    currentTemperature = 0.0;
    currentApparentTemperature = 0.0;
    currentHumidityFloat = 0.0;
    todayMinTemp = 0;
    todayMaxTemp = 0;
    moonPhase = 0.125f; // This value correctly represents a 25% illuminated waxing crescent
    moonPercentage = 25.0f; // Set the default percentage to match
    saveSettings(); // Save the cleared values (re-takes the same recursive mutex internally — safe)
    if (settingsMutex) xSemaphoreGiveRecursive(settingsMutex);
    return; // Exit the function immediately
  }
  
  unsigned long currentTime = millis();

  // --- SYSTEM / CONFIG CHECKS ---
  // These deliberately run BEFORE any rate-limit accounting: an attempt that
  // never reaches the network (no GPS set, no API key, unknown service) must
  // not consume the hourly API quota. It used to, which is why an
  // unconfigured clock burned all 10 "calls" without making a single HTTP
  // request, then reported a count that climbed forever (15/10, 21/10, ...).
  if (currentState != STATE_RUNNING) {
    Serial.println("Skipping fetchWeather() - System not in STATE_RUNNING.");
    lastHttpCode = -99; return;
  }
  if (gpsLat == 0.0 || gpsLon == 0.0) {
    Serial.println("Missing GPS coordinates.");
    lastHttpCode = -1; return;
  }

  HTTPClient http;
  String url = "";
  
  // ===================================================================
  // === SERVICE SELECTION AND URL CONSTRUCTION (ALWAYS CELSIUS) =====
  // ===================================================================

  if (selectedWeatherService == "pirateweather") {
    if (pirateWeatherAPI.isEmpty()) { Serial.println("Missing PirateWeather API key."); lastHttpCode = -1; return; }
    String apiUnits = "si"; // ALWAYS "si" for Celsius
    url = "https://api.pirateweather.net/forecast/" + pirateWeatherAPI + "/" + String(gpsLat, 6) + "," + String(gpsLon, 6) + "?units=" + apiUnits;
  } 
  else if (selectedWeatherService == "openweathermap") {
    if (openWeatherMapAPI.isEmpty()) { Serial.println("Missing OpenWeatherMap API key."); lastHttpCode = -1; return; }
    String apiUnits = "metric"; // ALWAYS "metric" for Celsius
    url = "https://api.openweathermap.org/data/3.0/onecall?lat=" + String(gpsLat, 6) + "&lon=" + String(gpsLon, 6) + "&appid=" + openWeatherMapAPI + "&units=" + apiUnits + "&exclude=minutely,alerts";
  } 
  else if (selectedWeatherService == "weatherapi") {
    if (weatherAPI_API.isEmpty()) { Serial.println("Missing WeatherAPI.com API key."); lastHttpCode = -1; return; }
    // WeatherAPI provides both C and F, so no unit parameter is needed in the URL. We will parse the Celsius fields.
    url = "http://api.weatherapi.com/v1/forecast.json?key=" + weatherAPI_API + "&q=" + String(gpsLat, 6) + "," + String(gpsLon, 6) + "&days=1&aqi=no&alerts=no";
  }
  else {
    Serial.println("Unknown weather service selected: " + selectedWeatherService);
    lastHttpCode = -1;
    return;
  }

  // --- RATE LIMITING (only genuine outbound API calls are counted) ---
  if (apiCallCount > 0 && currentTime - lastApiCallTime >= HOUR_MS) {
    apiCallCount = 0; // hourly window elapsed - fresh allowance
  }
  int minutesRemaining = (apiCallCount > 0 && (currentTime - lastApiCallTime < HOUR_MS))
                           ? (HOUR_MS - (currentTime - lastApiCallTime)) / 60000
                           : 60;
  if (apiCallCount >= MAX_API_CALLS) {
    // Checked BEFORE incrementing so the count stops at the limit (10/10)
    // instead of climbing forever. fetchWeatherTask also stops retrying on
    // -2, so we genuinely back off until the window resets.
    static unsigned long lastLimitPrint = 0;
    if (currentTime - lastLimitPrint >= 60000) {
      Serial.printf("API rate limit reached (%d/%d calls this hour). Paused until reset in %d minutes.\n",
                    apiCallCount, MAX_API_CALLS, minutesRemaining);
      lastLimitPrint = currentTime;
    }
    lastHttpCode = -2;
    return;
  }
  apiCallCount++;
  if (apiCallCount == 1) {
    lastApiCallTime = currentTime; // start of this hour's window
  }
  Serial.printf("Starting fetchWeather() for service '%s' (requesting Celsius)... (Attempt %d of %d)\n",
                selectedWeatherService.c_str(), apiCallCount, MAX_API_CALLS);

  http.setTimeout(15000);
  http.begin(url);
  int httpCode = http.GET();
  lastHttpCode = httpCode;
  Serial.println(selectedWeatherService + " API Response Code: " + String(httpCode));

  // ===================================================================
  // === JSON PARSING (ALWAYS PARSING CELSIUS VALUES) ==================
  // ===================================================================
  // Mutex is taken here (after the blocking network call above has already
  // completed) and held across the global writes + final save below, so it's
  // never held across the slow HTTPClient GET itself.
  if (settingsMutex) xSemaphoreTakeRecursive(settingsMutex, pdMS_TO_TICKS(5000));

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048); 
    DeserializationError error;

    if (selectedWeatherService == "pirateweather") {
      StaticJsonDocument<512> filter;
      filter["currently"]["temperature"] = true;
      filter["currently"]["humidity"] = true;
      filter["currently"]["summary"] = true;
      filter["currently"]["icon"] = true;
      filter["currently"]["apparentTemperature"] = true;
      filter["daily"]["data"][0]["temperatureMin"] = true;
      filter["daily"]["data"][0]["temperatureMax"] = true;
      filter["daily"]["data"][0]["moonPhase"] = true;
      error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
      if (!error) {
        currentTemperature = clampValue(doc["currently"]["temperature"], -99.0, 99.0);
        currentApparentTemperature = clampValue(doc["currently"]["apparentTemperature"], -99.0, 99.0);
        todayMinTemp = clampValue(doc["daily"]["data"][0]["temperatureMin"], -99.0, 99.0);
        todayMaxTemp = clampValue(doc["daily"]["data"][0]["temperatureMax"], -99.0, 99.0);
        currentHumidityFloat = clampValue(doc["currently"]["humidity"].as<float>() * 100, 0.0, 100.0);
        currentConditions = doc["currently"]["summary"].as<String>();
        weatherIcon = doc["currently"]["icon"].as<String>();
        moonPhase = doc["daily"]["data"][0]["moonPhase"].as<float>();
      }
    } 
    else if (selectedWeatherService == "openweathermap") {
      StaticJsonDocument<512> filter;
      filter["current"]["temp"] = true;
      filter["current"]["feels_like"] = true;
      filter["current"]["humidity"] = true;
      filter["current"]["weather"][0]["description"] = true;
      filter["current"]["weather"][0]["icon"] = true;
      filter["current"]["sunrise"] = true;
      filter["current"]["sunset"] = true;
      filter["current"]["dt"] = true;
      filter["daily"][0]["temp"]["min"] = true;
      filter["daily"][0]["temp"]["max"] = true;
      filter["daily"][0]["moon_phase"] = true;
      error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
      if (!error) {
        currentTemperature = clampValue(doc["current"]["temp"], -99.0, 99.0);
        currentApparentTemperature = clampValue(doc["current"]["feels_like"], -99.0, 99.0);
        todayMinTemp = clampValue(doc["daily"][0]["temp"]["min"], -99.0, 99.0);
        todayMaxTemp = clampValue(doc["daily"][0]["temp"]["max"], -99.0, 99.0);
        currentHumidityFloat = clampValue(doc["current"]["humidity"].as<float>(), 0.0, 100.0);
        currentConditions = doc["current"]["weather"][0]["description"].as<String>();
        bool isDay = (doc["current"]["dt"] > doc["current"]["sunrise"] && doc["current"]["dt"] < doc["current"]["sunset"]);
        weatherIcon = mapOwmIconToStandard(doc["current"]["weather"][0]["icon"].as<String>(), isDay);
        moonPhase = doc["daily"][0]["moon_phase"].as<float>();
      }
    } 
    else if (selectedWeatherService == "weatherapi") {
      StaticJsonDocument<512> filter;
      // Request Celsius fields specifically
      filter["current"]["temp_c"] = true;
      filter["current"]["feelslike_c"] = true;
      filter["current"]["humidity"] = true;
      filter["current"]["condition"]["text"] = true;
      filter["current"]["condition"]["code"] = true;
      filter["current"]["is_day"] = true;
      filter["forecast"]["forecastday"][0]["day"]["mintemp_c"] = true;
      filter["forecast"]["forecastday"][0]["day"]["maxtemp_c"] = true;
      filter["forecast"]["forecastday"][0]["astro"]["moon_phase"] = true;
      error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
      if (!error) {
        // Always parse the Celsius fields. The conversion to Fahrenheit will happen in the display functions.
        currentTemperature = clampValue(doc["current"]["temp_c"], -99.0, 99.0);
        currentApparentTemperature = clampValue(doc["current"]["feelslike_c"], -99.0, 99.0);
        todayMinTemp = clampValue(doc["forecast"]["forecastday"][0]["day"]["mintemp_c"], -99.0, 99.0);
        todayMaxTemp = clampValue(doc["forecast"]["forecastday"][0]["day"]["maxtemp_c"], -99.0, 99.0);

        currentHumidityFloat = clampValue(doc["current"]["humidity"].as<float>(), 0.0, 100.0);
        currentConditions = doc["current"]["condition"]["text"].as<String>();
        bool isDay = doc["current"]["is_day"] == 1;
        weatherIcon = mapWeatherApiCodeToStandard(doc["current"]["condition"]["code"], isDay);
        moonPhase = mapWeatherApiMoonPhase(doc["forecast"]["forecastday"][0]["astro"]["moon_phase"].as<String>());
      }
    }

    if (error) {
      Serial.println("JSON parsing failed: " + String(error.c_str()));
      lastHttpCode = -3;
    } else {
      Serial.printf("JSON parsed successfully for %s. Free heap: %u\n", selectedWeatherService.c_str(), ESP.getFreeHeap());
      
      // Store the fetched Celsius values as strings
      currentTemp = String(currentTemperature, 1);
      currentApparentTemp = String(currentApparentTemperature, 1);
      currentHumidity = String((int)currentHumidityFloat);

      float illumFraction = 1.0f - fabs(2.0f * moonPhase - 1.0f);
      moonPercentage = roundf(illumFraction * 100.0f * 10.0f) / 10.0f;

      // Log the fetched Celsius values for clarity
      Serial.println("Fetched Temp (Celsius): " + currentTemp + "°C");
      Serial.println("Fetched Apparent Temp (Celsius): " + currentApparentTemp + "°C");
      Serial.println("Fetched Humidity: " + currentHumidity + "%");
      Serial.println("Fetched Min/Max Temp (Celsius): " + String(todayMinTemp) + "°C / " + String(todayMaxTemp) + "°C");
      Serial.println("Fetched Weather Icon: " + weatherIcon);
      Serial.println("Fetched Moon Phase: " + String(moonPhase, 2) + " (" + String(moonPercentage, 1) + "%)");
    }
  } else {
    Serial.println("HTTP Request failed, error: " + String(httpCode));
  }

  http.end();
  delay(10);
  saveSettings(); // re-takes the same recursive mutex internally — safe, no deadlock
  if (settingsMutex) xSemaphoreGiveRecursive(settingsMutex);
}

void printSettings(){
  File file = SPIFFS.open("/settings.json", "r");
  if (file) {
    while (file.available()) {
    Serial.write((char)file.read());
    }
  file.close();}
}



void handleRoot() {
  // The page is stored gzip-compressed in flash (src/WebPage_gz.h, generated
  // from web/index.html by scripts/build_web.py at build time). We hand the
  // compressed bytes straight to the browser and let it inflate them.
  //
  // This replaced a chunked, uncompressed send of ~93 KB. That was the cause
  // of the settings page loading slowly or arriving half-rendered over a weak
  // WiFi link: chunked encoding has no up-front length, so a stall partway
  // through just left the browser drawing a partial page. Now it's ~16 KB
  // with a known Content-Length - roughly 5.8x fewer bytes over the air, in
  // one response.
  Serial.printf("Serving gzipped webpage (%u bytes)...\n", WEBPAGE_GZ_LEN);

  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html", (const char*)WEBPAGE_GZ, WEBPAGE_GZ_LEN);
}


void handleAMPM() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].ampmSwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleSeconds() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].secondsSwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handle24Hour() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].twentyFourHourSwitch = (state == "true");
      updateCurrentHoursMins();
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleIcons() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].iconsSwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleTemperature() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].temperatureSwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleMinMaxTemps() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].minMaxTempsSwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleDay() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].daySwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleDate() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].dateSwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleMonth() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].monthSwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleHumidity() {
  if (server.hasArg("state") && server.hasArg("screen")) {
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      allScreenSettings[screenIndex].humiditySwitch = (state == "true");
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleTimeColor() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      unsigned long color = strtoul(value.c_str(), NULL, 16);
      allScreenSettings[screenIndex].time_col.r = (color >> 16) & 0xFF;
      allScreenSettings[screenIndex].time_col.g = (color >> 8) & 0xFF;
      allScreenSettings[screenIndex].time_col.b = color & 0xFF;
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleDateColor() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      unsigned long color = strtoul(value.c_str(), NULL, 16);
      allScreenSettings[screenIndex].date_col.r = (color >> 16) & 0xFF;
      allScreenSettings[screenIndex].date_col.g = (color >> 8) & 0xFF;
      allScreenSettings[screenIndex].date_col.b = color & 0xFF;
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleDateBGColor() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      unsigned long color = strtoul(value.c_str(), NULL, 16);
      allScreenSettings[screenIndex].dateBG_col.r = (color >> 16) & 0xFF;
      allScreenSettings[screenIndex].dateBG_col.g = (color >> 8) & 0xFF;
      allScreenSettings[screenIndex].dateBG_col.b = color & 0xFF;
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleTempColor() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      unsigned long color = strtoul(value.c_str(), NULL, 16);
      allScreenSettings[screenIndex].temp_col.r = (color >> 16) & 0xFF;
      allScreenSettings[screenIndex].temp_col.g = (color >> 8) & 0xFF;
      allScreenSettings[screenIndex].temp_col.b = color & 0xFF;
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleHumidityColor() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      unsigned long color = strtoul(value.c_str(), NULL, 16);
      allScreenSettings[screenIndex].humidity_col.r = (color >> 16) & 0xFF;
      allScreenSettings[screenIndex].humidity_col.g = (color >> 8) & 0xFF;
      allScreenSettings[screenIndex].humidity_col.b = color & 0xFF;
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleDayColor() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      unsigned long color = strtoul(value.c_str(), NULL, 16);
      
      // This correctly modifies the DAY color
      allScreenSettings[screenIndex].day_col.r = (color >> 16) & 0xFF;
      allScreenSettings[screenIndex].day_col.g = (color >> 8) & 0xFF;
      allScreenSettings[screenIndex].day_col.b = color & 0xFF;
      
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleMonthColor() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      unsigned long color = strtoul(value.c_str(), NULL, 16);
      
      // **THE FIX**: This now correctly modifies the MONTH color
      allScreenSettings[screenIndex].month_col.r = (color >> 16) & 0xFF;
      allScreenSettings[screenIndex].month_col.g = (color >> 8) & 0xFF;
      allScreenSettings[screenIndex].month_col.b = color & 0xFF;
      
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleAMPMColor() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      unsigned long color = strtoul(value.c_str(), NULL, 16);
      allScreenSettings[screenIndex].ampm_col.r = (color >> 16) & 0xFF;
      allScreenSettings[screenIndex].ampm_col.g = (color >> 8) & 0xFF;
      allScreenSettings[screenIndex].ampm_col.b = color & 0xFF;
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleSecondsColor() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      unsigned long color = strtoul(value.c_str(), NULL, 16);
      allScreenSettings[screenIndex].seconds_col.r = (color >> 16) & 0xFF;
      allScreenSettings[screenIndex].seconds_col.g = (color >> 8) & 0xFF;
      allScreenSettings[screenIndex].seconds_col.b = color & 0xFF;
      saveSettings();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleBrightness() {
  // This handler is called when the main "Brightness" slider is adjusted on the web UI.
  // It sets the manual brightness level when Auto Brightness is disabled.

  // Check if the required 'value' argument was provided.
  if (server.hasArg("value")) {
    
    // Get the value from the request and convert it to an integer.
    String value = server.arg("value");
    brightness = value.toInt();
    
    // --- Safety Check: Constrain the value ---
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    
    // Immediately apply the new brightness to the display if Auto Brightness is off.
    // The main loop() handles this logic, but applying it here gives instant feedback.
    if (!autoBrightnessEnabled) {
      dma_display->setBrightness8(brightness);
    }
    
    // Save the new setting to SPIFFS.
    saveSettings();
    
    Serial.println("Brightness set to: " + String(brightness));
    
    // Send a success response.
    server.send(200, "text/plain", "Brightness OK: " + String(brightness));
    
  } else {
    // The request was missing the 'value' argument.
    server.send(400, "text/plain", "Missing brightness value");
  }
}

void handleDarkRoomBrightness() {
  // This handler is called when the "Dark Room Brightness" slider is adjusted on the web UI.
  // It sets the minimum brightness level for the Auto Brightness feature.

  // Check if the required 'value' argument was provided in the request.
  if (server.hasArg("value")) {
    
    // Get the value from the request and convert it to an integer.
    darkRoomBrightness = server.arg("value").toInt();
    
    // --- Safety Check: Constrain the value ---
    // Ensure the brightness value stays within the valid 8-bit range (0 to 255).
    if (darkRoomBrightness > 255) darkRoomBrightness = 255;
    if (darkRoomBrightness < 0) darkRoomBrightness = 0;
    
    // Save the new setting to SPIFFS.
    saveSettings();
    
    Serial.println("Dark Room Brightness set to: " + String(darkRoomBrightness));
    
    // Send a success response back to the web page.
    server.send(200, "text/plain", "Dark Room Brightness OK: " + String(darkRoomBrightness));

  } else {
    // The request was missing the required 'value' argument.
    server.send(400, "text/plain", "Missing dark room brightness value");
  }
}

void handleBrightRoomBrightness() {
  // This handler sets the maximum brightness level for Auto Brightness mode.
  if (server.hasArg("value")) {
    brightRoomBrightness = server.arg("value").toInt();
    // Constrain the value to the valid 8-bit range.
    if (brightRoomBrightness > 255) brightRoomBrightness = 255;
    if (brightRoomBrightness < 0) brightRoomBrightness = 0;
    
    saveSettings();
    Serial.println("Bright Room Brightness set to: " + String(brightRoomBrightness));
    server.send(200, "text/plain", "Bright Room Brightness OK: " + String(brightRoomBrightness));
  } else {
    server.send(400, "text/plain", "Missing bright room brightness value");
  }
}

void handleSetDarkRoomLDR() {
  // This handler calibrates the LDR reading for a "dark room".
  uint32_t ldrSum = 0;
  const int samples = 10;
  for (int i = 0; i < samples; i++) {
    ldrSum += analogRead(LDR_PIN);
    delay(50); // Small delay between readings
  }
  darkRoomLDRValue = ldrSum / samples; // Average the readings

  // Convert raw LDR value to an approximate Lux value for the UI.
  int luxValue = map(darkRoomLDRValue, 0, 4095, 40, 400);

  saveSettings();
  Serial.println("Dark Room LDR Value set to: " + String(darkRoomLDRValue) + " (" + String(luxValue) + " lux)");
  // Send the calculated lux value back to the web page to update the display.
  server.send(200, "text/plain", String(luxValue));
}

void handleSetBrightRoomLDR() {
  // This handler calibrates the LDR reading for a "bright room".
  uint32_t ldrSum = 0;
  const int samples = 10;
  for (int i = 0; i < samples; i++) {
    ldrSum += analogRead(LDR_PIN);
    delay(50);
  }
  brightRoomLDRValue = ldrSum / samples;

  int luxValue = map(brightRoomLDRValue, 0, 4095, 40, 400);

  saveSettings();
  Serial.println("Bright Room LDR Value set to: " + String(brightRoomLDRValue) + " (" + String(luxValue) + " lux)");
  server.send(200, "text/plain", String(luxValue));
}

void handleAutoBrightness() {
  // This handler enables or disables the Auto Brightness feature.
  if (server.hasArg("state")) {
    String state = server.arg("state");
    autoBrightnessEnabled = (state == "true");
    
    saveSettings();
    Serial.println("Auto Brightness: " + state);
    server.send(200, "text/plain", "Auto Brightness OK: " + state);
  } else {
    server.send(400, "text/plain", "Missing state parameter");
  }
}

void handleCurrentBrightness() {
  // This handler is polled by the web UI to display the live brightness level.
  // It maps the current 0-255 brightness value to a 0-100 percentage.
  int brightnessPercent = map(currentBrightness, 0, 255, 0, 100);
  server.send(200, "text/plain", String(brightnessPercent));
}

void handleTimezone() {
  // This handler updates the global timezone setting. It is not screen-specific.
  if (server.hasArg("tz")) {
    String tz = server.arg("tz");
    selectedTimezone = tz;
    Serial.println("Timezone selected: " + tz);

    // Set the timezone environment variable for the C time functions.
    // The format is "TZ=<POSIX_STRING>"
    String tzEnv = "TZ=" + tz;
    char tzChar[64];
    tzEnv.toCharArray(tzChar, 64);
    setenv("TZ", tzChar + 3, 1);  // Skip the "TZ=" part
    tzset(); // Apply the new timezone setting.

    saveSettings();
    server.send(200, "text/plain", "Timezone OK: " + tz);
    syncESPtoRTC(); // Sync the new local time to the RTC module.
  } else {
    server.send(400, "text/plain", "Missing timezone parameter");
  }
}

void handleLandColor() {
  // This handler is called when the user picks a color from the "Land" color picker.
  
  // Check if the required 'value' (hex color) and 'screen' arguments were provided.
  if (server.hasArg("value") && server.hasArg("screen")) {
    
    // Get the arguments from the request.
    String hexColor = server.arg("value"); // e.g., "77BB41"
    int screen = server.arg("screen").toInt();
    
    // Convert to 0-based index.
    int screenIndex = screen - 1;

    // --- Safety Check: Validate the screenIndex ---
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      
      // Convert the hex color string to an unsigned long integer.
      unsigned long colorValue = strtoul(hexColor.c_str(), NULL, 16);
      
      // ** THE CORRECTION **
      // Use bitwise operations to extract the R, G, and B components and
      // update the 'land_col' struct for the correct screen in our settings vector.
      allScreenSettings[screenIndex].land_col.r = (colorValue >> 16) & 0xFF;
      allScreenSettings[screenIndex].land_col.g = (colorValue >> 8) & 0xFF;
      allScreenSettings[screenIndex].land_col.b = colorValue & 0xFF;
      
      Serial.println("Set land_color[" + String(screenIndex) + "] to #" + hexColor);
      
      // Save all settings to SPIFFS.
      saveSettings();
      
      // Send a success response.
      server.send(200, "text/plain", "OK");

    } else {
      // The screen number was invalid.
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    // The request was missing arguments.
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleWaterColor() {
  // This handler is called when the user picks a color from the "Water" color picker.
  
  if (server.hasArg("value") && server.hasArg("screen")) {
    String hexColor = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    // --- Safety Check: Validate the screenIndex ---
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      
      unsigned long colorValue = strtoul(hexColor.c_str(), NULL, 16);
      
      // ** THE CORRECTION **
      // Update the 'water_col' struct for the correct screen in our settings vector.
      allScreenSettings[screenIndex].water_col.r = (colorValue >> 16) & 0xFF;
      allScreenSettings[screenIndex].water_col.g = (colorValue >> 8) & 0xFF;
      allScreenSettings[screenIndex].water_col.b = colorValue & 0xFF;
      
      Serial.println("Set water_color[" + String(screenIndex) + "] to #" + hexColor);
      
      saveSettings();
      server.send(200, "text/plain", "OK");

    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleIceColor() {
  // This handler is called when the user picks a color from the "Ice" color picker.
  
  if (server.hasArg("value") && server.hasArg("screen")) {
    String hexColor = server.arg("value");
    int screen = server.arg("screen").toInt();
    int screenIndex = screen - 1;

    // --- Safety Check: Validate the screenIndex ---
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      
      unsigned long colorValue = strtoul(hexColor.c_str(), NULL, 16);
      
      // ** THE CORRECTION **
      // Update the 'ice_col' struct for the correct screen in our settings vector.
      allScreenSettings[screenIndex].ice_col.r = (colorValue >> 16) & 0xFF;
      allScreenSettings[screenIndex].ice_col.g = (colorValue >> 8) & 0xFF;
      allScreenSettings[screenIndex].ice_col.b = colorValue & 0xFF;
      
      Serial.println("Set ice_color[" + String(screenIndex) + "] to #" + hexColor);
      
      saveSettings();
      server.send(200, "text/plain", "OK");

    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleClockSwitch() {
  // This handler is called when the "Clock Face" master switch is toggled on the web UI.
  
  // Check if the required 'state' and 'screen' arguments were provided in the request.
  if (server.hasArg("state") && server.hasArg("screen")) {
    
    // Get the arguments from the request.
    String state = server.arg("state");
    int screen = server.arg("screen").toInt();
    
    // Convert the 1-based screen number to a 0-based index.
    int screenIndex = screen - 1;

    // --- Safety Check: Validate the screenIndex ---
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      
      // ** THE CORRECTION **
      // Update the 'clockSwitch' boolean for the correct screen in our settings vector.
      allScreenSettings[screenIndex].clockSwitch = (state == "true");
      
      Serial.println("Set clockSwitch[" + String(screenIndex) + "] to " + state);
      
      // Save all settings to SPIFFS.
      saveSettings();
      
      // Send a success response.
      server.send(200, "text/plain", "OK");

    } else {
      // The screen number was invalid. Send an error response.
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    // The request was missing one or both of the required arguments.
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleClockOption() {
  // This handler is called when the user selects 'Colour' or 'Image' from the dropdown
  // in the expandable Clock Face section of the web UI.
  
  // Check if the required 'value' and 'screen' arguments were provided.
  if (server.hasArg("value") && server.hasArg("screen")) {
    
    // Get the arguments from the request.
    String value = server.arg("value");
    int screen = server.arg("screen").toInt();
    
    // Convert to 0-based index.
    int screenIndex = screen - 1;

    // --- Safety Check: Validate the screenIndex ---
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      
      // ** THE CORRECTION **
      // Update the 'clock_use_image' boolean for the correct screen in our settings vector.
      // If the value from the web page is "image", set the boolean to true. Otherwise, false.
      allScreenSettings[screenIndex].clock_use_image = (value == "image");
      
      Serial.println("Set clock_use_image[" + String(screenIndex) + "] to " + String(allScreenSettings[screenIndex].clock_use_image));
      
      // Save all settings to SPIFFS.
      saveSettings();
      
      // Send a success response.
      server.send(200, "text/plain", "OK");

    } else {
      // The screen number was invalid.
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    // The request was missing arguments.
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleClockBitmap() {
  // This handler is called when the user selects a clock face image from the dropdown on the web UI.
  // It receives the selected bitmap index and the screen it applies to.
  
  // Check if the required 'value' and 'screen' arguments were provided in the request.
  if (server.hasArg("value") && server.hasArg("screen")) {
    
    // Convert the string arguments from the request into integers.
    int value = server.arg("value").toInt();
    int screen = server.arg("screen").toInt();
    
    // Convert the 1-based screen number to a 0-based index for our vector.
    int screenIndex = screen - 1;

    // --- Safety Check 1: Validate the screenIndex ---
    // Make sure the screen number is within the valid range of our settings vector.
    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      
      // --- Safety Check 2: Validate the bitmap index ---
      // Make sure the selected index is within the bounds of our available bitmaps.
      // SCREEN3_BITMAP_COUNT is the total number of images in the screen3_bitmaps array.
      if (value >= 0 && value < SCREEN3_BITMAP_COUNT) {
        
        // ** THE CORRECTION **
        // Update the 'clock_bitmap_index' for the correct screen in our settings vector.
        allScreenSettings[screenIndex].clock_bitmap_index = value;
        
        Serial.println("Set clock_bitmap_index[" + String(screenIndex) + "] to " + String(value));
        
        // Save all settings to SPIFFS to persist this change.
        saveSettings();
        
        // Send a success response back to the web page.
        server.send(200, "text/plain", "OK");

      } else {
        // The bitmap index was out of range. Send an error response.
        server.send(400, "text/plain", "Invalid bitmap index");
      }
    } else {
      // The screen number was invalid. Send an error response.
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    // The request was missing one or both of the required arguments. Send an error response.
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handlePageSlider() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    int value_arg = server.arg("value").toInt();
    int screen_arg = server.arg("screen").toInt();
    int screenIndex = screen_arg - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      if (value_arg >= 0 && value_arg <= 255) {
        allScreenSettings[screenIndex].pageSlider = (uint8_t)value_arg;
        saveSettings();
        server.send(200, "text/plain", "OK");
      } else {
        server.send(400, "text/plain", "Invalid slider value");
      }
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handlePageSlider2() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    int value_arg = server.arg("value").toInt();
    int screen_arg = server.arg("screen").toInt();
    int screenIndex = screen_arg - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      if (value_arg >= 0 && value_arg <= 255) {
        allScreenSettings[screenIndex].pageSlider2 = (uint8_t)value_arg;
        saveSettings();
        server.send(200, "text/plain", "OK");
      } else {
        server.send(400, "text/plain", "Invalid slider value");
      }
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handlePageSlider3() {
  if (server.hasArg("value") && server.hasArg("screen")) {
    int value_arg = server.arg("value").toInt();
    int screen_arg = server.arg("screen").toInt();
    int screenIndex = screen_arg - 1;

    if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
      if (value_arg >= 0 && value_arg <= 255) {
        allScreenSettings[screenIndex].pageSlider3 = (uint8_t)value_arg;
        saveSettings();
        server.send(200, "text/plain", "OK");
      } else {
        server.send(400, "text/plain", "Invalid slider value");
      }
    } else {
      server.send(400, "text/plain", "Invalid screen");
    }
  } else {
    server.send(400, "text/plain", "Missing arguments");
  }
}

void handleSpareSwitch() {
    if (server.hasArg("state") && server.hasArg("screen")) {
        String state = server.arg("state");
        int screen = server.arg("screen").toInt();
        int screenIndex = screen - 1;
        if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
            allScreenSettings[screenIndex].SpareSwitch = (state == "true");
            saveSettings();
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "Invalid screen");
        }
    } else {
        server.send(400, "text/plain", "Missing parameters");
    }
}

void handleSpareSwitch2() {
    if (server.hasArg("state") && server.hasArg("screen")) {
        String state = server.arg("state");
        int screen = server.arg("screen").toInt();
        int screenIndex = screen - 1;
        if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
            allScreenSettings[screenIndex].SpareSwitch2 = (state == "true");
            saveSettings();
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "Invalid screen");
        }
    } else {
        server.send(400, "text/plain", "Missing parameters");
    }
}

void handleSpareSwitch3() {
    if (server.hasArg("state") && server.hasArg("screen")) {
        String state = server.arg("state");
        int screen = server.arg("screen").toInt();
        int screenIndex = screen - 1;
        if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) {
            allScreenSettings[screenIndex].SpareSwitch3 = (state == "true");
            saveSettings();
            server.send(200, "text/plain", "OK");
        } else {
            server.send(400, "text/plain", "Invalid screen");
        }
    } else {
        server.send(400, "text/plain", "Missing parameters");
    }
}

void handleFullYearAnimation() {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          int currentYear = timeinfo.tm_year + 1900;
          yearAnimationActive = true;
          animationStartTime = millis();
          currentAnimationDay = 1;
          ntpPaused = true;
          pauseStartTime = millis();
          useCalculatedMoonPhase = true; // Use calculated phase during animation

          int month, day;
          dayOfYearToDate(1, currentYear, month, day);
          timeinfo.tm_year = currentYear - 1900;
          timeinfo.tm_mon = month;
          timeinfo.tm_mday = day;
          timeinfo.tm_hour = 12;
          timeinfo.tm_min = 0;
          timeinfo.tm_sec = 0;
          timeinfo.tm_isdst = -1;

          time_t newTime = mktime(&timeinfo);
          struct timeval tv = {newTime, 0};
          settimeofday(&tv, NULL);

          // Calculate initial moon phase
          calculatedMoonPhase = calculateMoonPhase(timeinfo);

          // Format initial date based on language
          char dateStr[8];
          if (currentLanguage == "de") {
            strncpy(dateStr, months_DE_short[timeinfo.tm_mon], sizeof(dateStr));
          } else {
            strftime(dateStr, sizeof(dateStr), "%b", &timeinfo);
          }
          animationDateStr = String(dateStr);

          Serial.printf("Year animation started from %d-01-01, running for 15 seconds. Moon phase: %.3f\n", currentYear, calculatedMoonPhase);
        } else {
          Serial.println("Failed to get current time to start animation.");
        }
}



void handleFormatSSD() {
  Serial.println("Formatting SPIFFS...");
  if (SPIFFS.format()) {
    Serial.println("SPIFFS formatted successfully");
    server.send(200, "text/plain", "SPIFFS formatted. Rebooting...");
    delay(1000); // Give time for the response to be sent
    ESP.restart(); // Reboot the ESP32
  } else {
    Serial.println("SPIFFS formatting failed");
    server.send(500, "text/plain", "Failed to format SPIFFS");
  }
}

void fetchWeatherTask(void *pvParameters) {
  static unsigned long lastWeatherSyncTime = 0;
  static unsigned long lastRetryTime = 0;
  const unsigned long weatherSyncInterval = 420000; // 7 minutes = 420000
  const unsigned long retryInterval = 30000;        // 30 seconds for retries

  for (;;) { // Infinite loop for the task
    // Only attempt fetch if in RUNNING state and WiFi is connected
    if (currentState == STATE_RUNNING) {
      unsigned long currentTime = millis();

      // Scheduled sync
      if (currentTime - lastWeatherSyncTime >= weatherSyncInterval) {
        //Serial.println("Scheduled weather sync attempt from Core 1...");
        fetchWeather();
        lastWeatherSyncTime = currentTime; // Update sync time regardless of success
        lastRetryTime = currentTime;     // Reset retry timer on scheduled attempt
      }
      // Retry only on genuinely transient failures (HTTP/network errors, bad
      // JSON). These codes are deliberately excluded because retrying every
      // 30s achieves nothing but log spam:
      //   -99 = not in RUNNING state (the outer `if` already covers this)
      //   -1  = missing/invalid config (no GPS, no API key, unknown service).
      //         Nothing changes until the user edits settings - and when they
      //         do, the web handlers set needWeatherUpdate, which fires an
      //         immediate attempt below.
      //   -2  = hourly rate limit hit. Back off until the window resets; the
      //         7-minute scheduled sync will pick it up again on its own.
      else if (lastHttpCode != 200 && lastHttpCode != -99 &&
               lastHttpCode != -1 && lastHttpCode != -2 &&
               currentTime - lastRetryTime >= retryInterval) {
         Serial.println("Retrying weather fetch attempt from Core 1 due to previous failure...");
         fetchWeather();
         lastRetryTime = currentTime; // Update retry time
      }
      // Manual update request from web server
      else if (needWeatherUpdate) {
         Serial.println("Manual weather update requested from Core 1...");
         fetchWeather();
         needWeatherUpdate = false; // Clear the flag
         lastWeatherSyncTime = currentTime; // Update sync time
         lastRetryTime = currentTime;     // Reset retry timer
      }
    }
    
    // Log stack high water mark periodically (optional)
    static unsigned long lastStackLog = 0;
    if (millis() - lastStackLog > 60000) {
       UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
       //Serial.printf("FetchWeatherTask Stack remaining: %u bytes\n", stackHighWaterMark);
       lastStackLog = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // Check every 1 seconds
  }
}

void webServerTask(void *pvParameters) {
  for (;;) { // Infinite loop for the task
    // Handle web server clients only if WiFi is connected
    // WiFiManager handles its portal clients via myWM.process() in loop()
    if (currentState >= STATE_WIFI_CONNECTING) { // Only handle web server once we have a potential IP
       server.handleClient();
    } else {
        // Optional: Log if not handling clients because WiFi is down
        // static bool logged = false;
        // if (!logged) {
        //     Serial.println("Web server not handling clients - WiFi not connected.");
        //     logged = true;
        // }
        // logged = (WiFi.status() == WL_CONNECTED && currentState >= STATE_WIFI_CONNECTING);
    }

    vTaskDelay(pdMS_TO_TICKS(1)); // Yield frequently to other tasks on Core 0
  }
}


void getInternalAHT10() {
  sensors_event_t humidity, temp;
  if (aht.getEvent(&humidity, &temp)) {
    // Store temperature rounded to 1 decimal place
    internalTemp = round(temp.temperature * 10) / 10.0; // Store humidity rounded to 0 decimal places
    internalHumid = round(humidity.relative_humidity);
    // Serial.println("****************************************");
    // Serial.print("AHT10 Temperature: "); Serial.print(internalTemp, 1); Serial.println(" °C");  
    // Serial.print("AHT10 Humidity: "); Serial.print(internalHumid, 1); Serial.println(" %");
    // Serial.println("****************************************");
    // Print RTC time  
    DateTime now = rtc.now(); // Use DateTime, not time_t
    //  Serial.print("RTC Date: "); Serial.print(now.day(), DEC); Serial.print('/'); Serial.print(now.month(), DEC); Serial.print('/'); Serial.println(now.year(), DEC);
    //  Serial.print("RTC Time: "); Serial.print(now.hour(), DEC); Serial.print(':'); Serial.print(now.minute(), DEC); Serial.print(':'); Serial.println(now.second(), DEC);
    //  Serial.println("****************************************");
    // Serial.println();
  } else {
    // In case of failure, set to invalid values
    internalTemp = -99.0;
    internalHumid = -99.0;
  }
}


void handleScreenshot() {
  if (!dma_display || !dma_canvas_is_valid()) {
    server.send(500, "text/plain", "Display not initialized");
    return;
  }
  
  server.sendHeader("Content-Type", "application/octet-stream");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  server.send_P(200, "application/octet-stream", (const char*)screenshotBuffer, sizeof(screenshotBuffer)); // Send the SAFE screenshot buffer, not the live dma_canvas buffer.
}


void handleUnits() {
  if (server.hasArg("value")) {
    tempUnits = server.arg("value");
    saveSettings();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing value");
  }
}

void handleTempType() {
  if (server.hasArg("value")) {
    tempType = server.arg("value");
    saveSettings();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing value");
  }
}







char AutoChipID[11];  // CHAR Array MUST be 11 or LESS!!!!!!!!!!!
WiFiManager myWM;
 
int Wifi_Status = -1;
int Wifi_Flag = 0;
unsigned long time_now = 0;

//MatrixPanel_I2S_DMA *dma_display = nullptr;


uint16_t cc_blk = dma_display->color565(0, 0, 0);         // black
uint16_t cc_wht = dma_display->color565(100, 100, 100);      // white
uint16_t cc_bwht = dma_display->color565(255, 255, 255);  // bright white
uint16_t cc_red = dma_display->color565(150, 0, 0);        // red
uint16_t cc_bred = dma_display->color565(255, 0, 0);      // bright red
uint16_t cc_org = dma_display->color565(100, 40, 0);       // orange
uint16_t cc_borg = dma_display->color565(255, 165, 0);    // bright orange
uint16_t cc_grn = dma_display->color565(0, 160, 0);        // green
uint16_t cc_bgrn = dma_display->color565(0, 255, 0);      // bright green
uint16_t cc_blu = dma_display->color565(0, 0, 180);       // blue
uint16_t cc_bblu = dma_display->color565(0, 128, 255);    // bright blue
uint16_t cc_ylw = dma_display->color565(45, 45, 0);       // yellow
uint16_t cc_bylw = dma_display->color565(255, 255, 0);    // bright yellow
uint16_t cc_gry = dma_display->color565(10, 10, 10);      // gray
uint16_t cc_bgry = dma_display->color565(128, 128, 128);  // bright gray
uint16_t cc_dgr = dma_display->color565(3, 3, 3);         // dark grey
uint16_t cc_cyan = dma_display->color565(0, 30, 30);      // cyan
uint16_t cc_bcyan = dma_display->color565(0, 255, 255);   // bright cyan
uint16_t cc_ppl = dma_display->color565(100, 0, 100);       // purple
uint16_t cc_bppl = dma_display->color565(255, 0, 255);    // bright purple

// Instance of the button.
EasyButton button(MenuButtonPin);

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("\n\n--- Booting Up ---");

  esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.print("[Boot] Reset reason: ");
  switch (resetReason) {
    case ESP_RST_POWERON:   Serial.println("Power-on"); break;
    case ESP_RST_SW:        Serial.println("Software (esp_restart)"); break;
    case ESP_RST_PANIC:     Serial.println("PANIC / exception"); break;
    case ESP_RST_INT_WDT:   Serial.println("Interrupt watchdog"); break;
    case ESP_RST_TASK_WDT:  Serial.println("Task watchdog"); break;
    case ESP_RST_WDT:       Serial.println("Other watchdog"); break;
    case ESP_RST_BROWNOUT:  Serial.println("Brownout"); break;
    case ESP_RST_DEEPSLEEP: Serial.println("Deep sleep wake"); break;
    default:                Serial.printf("Other (%d)\n", (int)resetReason); break;
  }

  settingsMutex = xSemaphoreCreateRecursiveMutex();

  safeModeActive = false; // global, published to the web UI as doc["safe_mode"]
  int rebootCount = 1; // Default to 1 for a normal boot

  // --- REBOOT GUARD V3: Using External RTC and SPIFFS ---
  // This method is robust against both warm reboots (resets) and cold reboots (power loss).

  // 1. Mount SPIFFS unconditionally — must not depend on RTC presence.
  bool spiffsOk = SPIFFS.begin(false); // try a clean mount first, no silent format
  if (!spiffsOk) {
    Serial.println("[SPIFFS] Mount FAILED (missing/corrupt filesystem).");
    Serial.println("[SPIFFS] Attempting recovery format — ALL saved settings/WiFi creds/cache will be lost.");
    spiffsOk = SPIFFS.begin(true);
    Serial.println(spiffsOk ? "[SPIFFS] Reformat succeeded." : "[SPIFFS] Reformat FAILED — unusable this boot.");
  }

  // 2. Reboot guard now gates only on RTC presence + the SPIFFS mount state established above.
  if (!rtc.begin()) {
    Serial.println("[Reboot Guard] DS3231 RTC not found! Guard is DISABLED.");
  } else if (!spiffsOk) {
    Serial.println("[Reboot Guard] SPIFFS not mounted! Guard is DISABLED.");
  } else {
    // Both RTC and SPIFFS are working.
    const char* guardFile = "/reboot_guard.json";
    uint32_t currentTimestamp = rtc.now().unixtime();
    
    File file = SPIFFS.open(guardFile, "r");
    if (!file) {
      // File doesn't exist. This is the first boot after a clean shutdown or format.
      Serial.println("[Reboot Guard] Log file not found. Creating new sequence.");
      rebootCount = 1;
    } else {
      // File exists, let's read it.
      DynamicJsonDocument doc(96); // Small doc for the log file
      DeserializationError error = deserializeJson(doc, file);
      file.close(); // Close the file immediately after reading

      if (error) {
        Serial.println("[Reboot Guard] Failed to parse log file. Starting new sequence.");
        rebootCount = 1;
      } else {
        uint32_t lastTimestamp = doc["timestamp"] | 0;
        int lastCount = doc["count"] | 0;
        Serial.printf("[Reboot Guard] Read Log: Last boot was at %u with count %d.\n", lastTimestamp, lastCount);
        
        // The core logic: Compare current time to the last saved time.
        if ((currentTimestamp - lastTimestamp) <= 60) {
          // Rapid reboot detected!
          rebootCount = lastCount + 1;
          Serial.printf("[Reboot Guard] Rapid reboot detected! New count is %d.\n", rebootCount);
        } else {
          // Normal boot (more than 60s has passed).
          Serial.println("[Reboot Guard] Normal boot detected. Resetting sequence.");
          rebootCount = 1;
        }
      }
    }

    // Now, check if the trigger condition is met.
    if (rebootCount >= 3) {
      Serial.println("!!! [Reboot Guard] TRIGGERED: 3+ reboots in under 60 seconds!");
      safeModeActive = true;
      // We delete the file so the next boot is clean.
      SPIFFS.remove(guardFile); 
      Serial.println("[Reboot Guard] Log file deleted to prevent re-triggering.");
    } else {
      // If not triggered, we update the log file for the *next* boot to read.
      DynamicJsonDocument doc(96);
      doc["timestamp"] = currentTimestamp;
      doc["count"] = rebootCount;
      
      File file = SPIFFS.open(guardFile, "w");
      if(serializeJson(doc, file) == 0) {
        Serial.println("[Reboot Guard] Failed to write updated log file.");
      } else {
        Serial.println("[Reboot Guard] Wrote updated log file for next boot.");
      }
      file.close();
    }
  }
  // --- END OF REBOOT GUARD LOGIC ---


  // Load settings from the main settings file
  allScreenSettings.resize(NUM_CLOCK_SCREENS);
  loadSettings(); 
  
  // If the guard was triggered, OVERWRITE the loaded settings with safe ones and SAVE them.
  if (safeModeActive) {
    autoBrightnessEnabled = false;
    brightness = 40; // Approx 15% (40/255)
    
    Serial.println("*************************************************");
    Serial.println("[Reboot Guard] EMERGENCY SETTINGS APPLIED:");
    Serial.println("--> Auto Brightness turned OFF.");
    Serial.printf("--> Manual Brightness set to %d (15%%).\n", brightness);
    Serial.println("*************************************************");

    saveSettings(); // Save the new "safe" settings to the main settings file
  }

  // --- NOW, INITIALIZE THE POWER-HUNGRY DISPLAY ---
  Serial.println("[Setup] Checking panelType BEFORE pin config. Value is: " + panelType);
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  mxconfig.clkphase = false;
  mxconfig.driver = HUB75_I2S_CFG::FM6047;

  if (panelType == "P2.5") {
    Serial.println("Configuring pins for P2.5 panel.");
    mxconfig.gpio.g1 = 26; mxconfig.gpio.b1 = 27; mxconfig.gpio.g2 = 12; mxconfig.gpio.b2 = 13;
  } else {
    Serial.println("Configuring pins for P5.0 panel (default).");
    mxconfig.gpio.g1 = 27; mxconfig.gpio.b1 = 26; mxconfig.gpio.g2 = 13; mxconfig.gpio.b2 = 12;
  }

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display) {
    Serial.println("!!! FAILED to allocate dma_display !!! Halting.");
    while(1);
  }
  
  dma_display->begin();
  dma_display->setBrightness8(brightness);
  currentBrightness = brightness;
  dma_canvas.setRotation(0);
  dma_canvas.fillScreen(0);
  
  // --- CONTINUE WITH THE REST OF THE SETUP ---
  
  // Splash screen
  dma_canvas.fillScreen(0);
  dma_canvas.setFont(&Font_5x7_practical8pt7b);
  dma_canvas.drawRGBBitmap(0, 0, (const uint16_t *)Wificlock, 64, 32);
  dma_canvas.setCursor(33, 8);  dma_canvas.setTextColor(cc_bcyan);  dma_canvas.print("Wifi");
  dma_canvas.setCursor(37, 17); dma_canvas.setTextColor(cc_bcyan); dma_canvas.print("Clock");
  dma_canvas.setFont(&TomThumb); dma_canvas.setTextColor(cc_bgrn); drawRightString("v" + String(ver,2), 0, 32);

  // Display Reboot Count on Splash Screen
  // dma_canvas.setFont(&TomThumb);
  // dma_canvas.setTextColor(cc_bylw); // Yellow for visibility
  // String bootCountMsg = "Boot: " + String(rebootCount);
  // dma_canvas.setCursor(1, 6); // Positioned in the top-left corner
  // dma_canvas.print(bootCountMsg);

  // Display Safe Mode message on splash screen if triggered
  if (safeModeActive) {
      dma_canvas.setFont(&TomThumb);
      dma_canvas.setTextColor(cc_bred);
      drawCentreString("Safe Mode On", 0, 25);
  }

  dma_display->drawRGBBitmap(0, 0, dma_canvas.getBuffer(), dma_canvas.width(), dma_canvas.height());
  unsigned long splashStartTime = millis();
  stateStartTime = millis();

  // Initialize Sensors
  if (!aht.begin()) Serial.println("Could not find AHT10 sensor! Check wiring.");
  getInternalAHT10();

  // Set timezone and sync time
  setenv("TZ", selectedTimezone.c_str(), 1);
  tzset(); 
  syncRTCtoESP(); 
  getLocalTime(&timeinfo);

  // Button Setup
  pinMode(MenuButtonPin, INPUT_PULLUP);
  button.begin();
  button.onPressed(MenuButtonPressed);
  button.onPressedFor(2000, MenuButtonHeld);

  // --- Setup Web Server endpoints ---
  server.on("/", handleRoot);
  server.on("/settings", handleSettings);
  server.on("/paneltype", handlePanelType);
  server.on("/ampm", handleAMPM);
  server.on("/seconds", handleSeconds);
  server.on("/24hour", handle24Hour);
  server.on("/icons", handleIcons);
  server.on("/temperature", handleTemperature);
  server.on("/units", handleUnits);
  server.on("/temp_type", handleTempType);
  server.on("/minmaxtemps", handleMinMaxTemps);
  server.on("/day", handleDay);
  server.on("/date", handleDate);
  server.on("/month", handleMonth);
  server.on("/humidity", handleHumidity);
  server.on("/timecolor", handleTimeColor);
  server.on("/datecolor", handleDateColor);
  server.on("/ampmcolor", handleAMPMColor);
  server.on("/secondscolor", handleSecondsColor);
  server.on("/datebgcolor", handleDateBGColor);
  server.on("/tempcolor", handleTempColor);
  server.on("/humiditycolor", handleHumidityColor);
  server.on("/daycolor", handleDayColor);
  server.on("/monthcolor", handleMonthColor);
  server.on("/timezone", handleTimezone);
  server.on("/pirateweatherapi", handlePirateWeatherAPI);
  server.on("/gps", handleGPS);
  server.on("/brightness", handleBrightness);
  server.on("/darkRoomBrightness", handleDarkRoomBrightness);
  server.on("/brightRoomBrightness", handleBrightRoomBrightness);
  server.on("/setDarkRoomLDR", handleSetDarkRoomLDR);
  server.on("/setBrightRoomLDR", handleSetBrightRoomLDR);
  server.on("/autoBrightness", handleAutoBrightness);
  server.on("/currentBrightness", handleCurrentBrightness);
  server.on("/formatSSD", handleFormatSSD);
  server.on("/landcolor", handleLandColor);
  server.on("/watercolor", handleWaterColor);
  server.on("/icecolor", handleIceColor);
  server.on("/fullYearAnimation", handleFullYearAnimation);
  server.on("/clock", HTTP_GET, handleClockSwitch);
  server.on("/clock_option", HTTP_GET, handleClockOption);
  server.on("/clock_bitmap", HTTP_GET, handleClockBitmap);
  server.on("/pageslider", HTTP_GET, handlePageSlider);
  server.on("/pageslider2", HTTP_GET, handlePageSlider2);
  server.on("/pageslider3", HTTP_GET, handlePageSlider3);
  server.on("/spare_switch", HTTP_GET, handleSpareSwitch);
  server.on("/spare_switch2", HTTP_GET, handleSpareSwitch2);
  server.on("/spare_switch3", HTTP_GET, handleSpareSwitch3);
  server.on("/screenshot.bin", handleScreenshot);
  server.on("/weather_service", handleWeatherService);
  server.on("/openweathermapapi", handleOpenWeatherMapAPI);
  server.on("/weatherapi_api", handleWeatherAPI_API);
  server.on("/land", handleLandSwitch);
  server.on("/water", handleWaterSwitch);
  server.on("/ice", handleIceSwitch);
  server.on("/land_bitmap", handleLandBitmap);
  server.on("/water_bitmap", handleWaterBitmap);
  server.on("/ice_bitmap", handleIceBitmap);
  server.on("/land_option", handleLandOption);
  server.on("/water_option", handleWaterOption);
  server.on("/ice_option", handleIceOption);
  server.on("/screen", handleScreenSelection);
  server.on("/markerDisplayMode", handleMarkerDisplayMode);
  server.on("/numberColorMode", handleNumberColorMode);
  server.on("/starColorMode", handleStarColorMode);
  server.on("/number_color", handleNumberColor);
  server.on("/star_color", handleStarColor);
  server.on("/hourHandSwitch", handleHourHandSwitch);
  server.on("/minuteHandSwitch", handleMinuteHandSwitch);
  server.on("/secondHandSwitch", handleSecondHandSwitch);
  server.on("/backgroundSwitch", handleBackgroundSwitch);
  server.on("/language", handleLanguage);
  server.on("/indoortempoffset", handleIndoorTempOffset);
  server.on("/schedulesenabled", handleSchedulesEnabled);
  server.on("/updateschedules", HTTP_POST, handleUpdateSchedules);
  server.on("/weather", []() { server.send(200, "text/plain", "Weather Updated: Temp=" + currentTemp + ", Humidity=" + currentHumidity + ", Conditions=" + currentConditions + ", Daily Min/Max=" + dailyMinMaxTemps + ", Icon=" + weatherIcon); });
  server.on("/weatherupdate", [](){ if (currentState == STATE_RUNNING) { needWeatherUpdate = true; server.send(200, "text/plain", "Weather update requested."); } else { server.send(503, "text/plain", "Cannot update weather: Not connected or not in running state."); } });
  server.on("/reboot", []() { server.send(200, "text/plain", "Rebooting..."); delay(1000); ESP.restart(); });
  server.on("/clearWifi", []() { server.send(200, "text/plain", "Clearing WiFi settings & Rebooting..."); myWM.resetSettings(); delay(1000); ESP.restart(); });

  // --- Browser-based OTA: POST a firmware image to /update -----------------
  // NOTE: this expects the APP-ONLY image (.pio/build/esp32dev/firmware.bin),
  // NOT the merged SmartClock_vX.XX.bin in ../BIN/ (that one also contains the
  // bootloader + partition table and is only valid written to offset 0x0 over
  // USB). Uploading the merged image here would brick the app partition, so
  // the size is sanity-checked against the free OTA partition below.
  server.on("/update", HTTP_POST,
    []() { // called once the whole upload has been received
      bool ok = !Update.hasError();
      server.sendHeader("Connection", "close");
      server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "FAIL");
      if (ok) {
        Serial.println("[OTA] Update accepted - rebooting into new firmware.");
        delay(500);
        ESP.restart();
      }
    },
    []() { // streamed while the upload is arriving
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] Receiving %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          Serial.printf("[OTA] Success: %u bytes written.\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        Serial.println("[OTA] Upload aborted.");
      }
    });
  server.on("/clockdisplaymode", HTTP_GET, []() { if (server.hasArg("screen") && server.hasArg("value")) { int screenIndex = server.arg("screen").toInt() - 1; String value = server.arg("value"); if (screenIndex >= 0 && screenIndex < allScreenSettings.size()) { if (value == "4 numbers" || value == "All numbers" || value == "4 ticks" || value == "All ticks") { allScreenSettings[screenIndex].clockDisplayMode = value; saveSettings(); server.send(200, "text/plain", "OK"); } else { server.send(400, "text/plain", "Invalid display mode value"); } } else { server.send(400, "text/plain", "Invalid screen number"); } } else { server.send(400, "text/plain", "Missing parameters"); } });

  // Configure WiFiManager
  WIFI_SETUP();

  // Wait for Splash Screen to finish
  while (millis() - splashStartTime < 5000) {
    delay(10); 
  }

  // Initial State Determination
  if (WiFi.status() == WL_CONNECTED) {
      currentState = STATE_WIFI_CONNECTING;
      Serial.println("WiFi connected quickly (from setup). Entering STATE_WIFI_CONNECTING.");
  } else if (myWM.getWiFiSSID(true) != "") {
      currentState = STATE_WIFI_SETUP;
      Serial.println("Saved WiFi credentials exist, but not connected. Entering STATE_WIFI_SETUP.");
  } else {
      currentState = STATE_WIFI_NO_CREDENTIALS;
      stateNOWIFITime = millis();
      Serial.println("No WiFi credentials or initial connection failed. Entering STATE_WIFI_NO_CREDENTIALS.");
  }
  stateStartTime = millis(); 

  Serial.println("Setup complete. Initial state: " + String(currentState));
}



void displayWeatherIcon(const String& weatherIcon) {
  // Define the position and size for the icon
  int x = 27;  // Same x position as your original example
  int y = 0;  // Same y position as your original example
  int width = 10;  // Same width as your original example
  int height = 10;  // Same height as your original example

  // Map the weatherIcon string to the corresponding bitmap and draw it
  if (weatherIcon == "clear-day") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)Sunny, 11, 11);  //  ClearDay later
  }
  else if (weatherIcon == "clear-night") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)ClearNight, width, height);  //  ClearNight later
  }
  else if (weatherIcon == "rain") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)CloudyRain, width, height);  //  Rain later
  }
  else if (weatherIcon == "snow") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)CloudyRain, width, height);  //  Snow later
  }
  else if (weatherIcon == "sleet") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)CloudyRain, width, height);  //  Sleet later
  }
  else if (weatherIcon == "wind") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)Sunny, 11, 11);  //  Wind later
  }
  else if (weatherIcon == "fog") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)CloudyRain, width, height);  //  Fog later
  }
  else if (weatherIcon == "cloudy") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)Cloudy, width, height);  //  Cloudy later
  }
  else if (weatherIcon == "partly-cloudy-day") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)PartlyCloudyDay, width, height);  //  PartlyCloudyDay later
  }
  else if (weatherIcon == "partly-cloudy-night") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)PartlyCloudyNight, width, height);  //  PartlyCloudyNight later
  }
  else if (weatherIcon == "thunderstorm") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)Stormy, width, height);  //  Thunderstorm later (future)
  }
  else if (weatherIcon == "hail") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)Stormy, width, height);  // Hail later (future)
  }
  else if (weatherIcon == "none" || weatherIcon == "N/A") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)Crossmark, 12, 12);  // None or a default icon later
  }
  else {
    // Fallback for unrecognized icons
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)Crossmark, 12, 12);  //  default icon later
    //Serial.println("Unrecognized weather icon: " + weatherIcon);
  }
}

void displayLargeWeatherIcon(const String& weatherIcon) {
  // Define the position and size for the icon
  int x = 34;  // Same x position as your original example
  int y = 0;  // Same y position as your original example
  int Largewidth = 29;  // Same width as your original example
  int Largeheight = 21;  // Same height as your original example

  // Map the weatherIcon string to the corresponding bitmap and draw it
  if (weatherIcon == "clear-day") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)SunnyLarge, Largewidth, Largeheight);  //  ClearDay later
  }
  else if (weatherIcon == "clear-night") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)ClearNightLarge, Largewidth, Largeheight);  //  ClearNight later
  }
  else if (weatherIcon == "rain") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)CloudyRainLarge, Largewidth, Largeheight);  //  Rain later
  }
  else if (weatherIcon == "snow") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)CloudyRainLarge, Largewidth, Largeheight);  //  Snow later
  }
  else if (weatherIcon == "sleet") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)CloudyRainLarge, Largewidth, Largeheight);  //  Sleet later
  }
  else if (weatherIcon == "wind") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)SunnyLarge, Largewidth, Largeheight);  //  Wind later
  }
  else if (weatherIcon == "fog") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)CloudyRainLarge, Largewidth, Largeheight);  //  Fog later
  }
  else if (weatherIcon == "cloudy") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)CloudyLarge, Largewidth, Largeheight);  //  Cloudy later
  }
  else if (weatherIcon == "partly-cloudy-day") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)PartlyCloudyDayLarge, Largewidth, Largeheight);  //  PartlyCloudyDay later
  }
  else if (weatherIcon == "partly-cloudy-night") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)PartlyCloudyNightLarge, Largewidth, Largeheight);  //  PartlyCloudyNight later
  }
  else if (weatherIcon == "thunderstorm") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)StormyLarge, Largewidth, Largeheight);  //  Thunderstorm later (future)
  }
  else if (weatherIcon == "hail") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)StormyLarge, Largewidth, Largeheight);  // Hail later (future)
  }
  else if (weatherIcon == "none" || weatherIcon == "N/A") {
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)Crossmark, 12, 12);  // None or a default icon later
  }
  else {
    // Fallback for unrecognized icons
    dma_canvas.drawRGBBitmap(x, y, (const uint16_t *)Crossmark, 12, 12);  //  default icon later
    //Serial.println("Unrecognized weather icon: " + weatherIcon);
  }
}

void WIFI_SETUP() {
  // This function now primarily configures WiFiManager but doesn't block waiting
  // Connection attempts and state transitions are handled in loop()

  WiFi.mode(WIFI_STA); // Start in station mode; WiFiManager will switch to AP if needed
  myWM.setConfigPortalBlocking(false); // Keep non-blocking
  myWM.setConfigPortalTimeout(0);      // Portal timeout can be set here if desired

  uint32_t id = 0;
  for(int i=0; i<17; i=i+8) {
    id |= ((ESP.getEfuseMac() >> (50 - i)) & 0xff) << i;
  }
  snprintf(AutoChipID, 10, "Clock-%4X", id);
  Serial.println("Starting WiFiManager autoConnect in non-blocking mode...");
  myWM.autoConnect(AutoChipID); // This will try saved creds or start AP if none/failed
  WiFi.setAutoReconnect(true); // Let the WiFi driver retry immediately on drop, not just our own polling
  myWM.setSaveConfigCallback([]() {
    Serial.println("WiFi credentials saved.");
  });



}



void loop() {
  myWM.process(); // Keep WiFiManager alive. This is critical for the portal.
  button.read();  // Check button state

  static uint32_t lastDebugPrintTime = 0;
  if (millis() - lastDebugPrintTime > 1000) { // Print state once per second
    // Serial.print("Current State: "); Serial.print(currentState);
    // Serial.print(", WiFi Status: "); Serial.print(WiFi.status());
    // Serial.print(", Current Screen Variable: "); Serial.println(currentScreen);
    // Serial.println("Apparent Temperature: " + currentApparentTemp + "°C");
    lastDebugPrintTime = millis();
  }

  // Call getInternalAHT10 every 30 seconds (quick, non-blocking)
  static uint32_t lastAHT10Update = 0;
  const uint32_t aht10UpdateInterval = 30000; 
  if (millis() - lastAHT10Update >= aht10UpdateInterval) {
    getInternalAHT10(); // Uncomment if you want this active
    lastAHT10Update = millis();
  }

// --- Auto-brightness Logic with Flicker Protection ---
  static uint32_t lastAutoBrightnessUpdate = 0;
  static bool isLockedAtMin = false;            // Tracks if we are "locked" at user minimum
  static unsigned long lightThresholdStartTime = 0; // Timer for the 1-second unlock rule
  const uint32_t autoBrightnessUpdateInterval = 50; 
  
  // Define 11% and 12% thresholds (11% of 255 ≈ 28, 12% ≈ 31)
  const uint8_t FLICKER_LIMIT = 28; 
  const uint8_t UNLOCK_THRESHOLD = 31;

  if (autoBrightnessEnabled) {
    uint32_t ldrValue = analogRead(LDR_PIN);
    ldrValue = min(ldrValue, (uint32_t)brightRoomLDRValue);
    
    // Calculate what the sensor "wants" the brightness to be
    long rawMap = map(ldrValue, darkRoomLDRValue, brightRoomLDRValue, darkRoomBrightness, brightRoomBrightness);
    uint8_t sensorTarget = constrain(rawMap, darkRoomBrightness, brightRoomBrightness);
    
    uint8_t finalTarget;

    if (isLockedAtMin) {
      // If we are locked at minimum, check if light level is high enough to reach 12%
      if (sensorTarget >= UNLOCK_THRESHOLD) {
        if (lightThresholdStartTime == 0) lightThresholdStartTime = millis();
        
        // Only unlock if light has been consistently high for 1 second
        if (millis() - lightThresholdStartTime >= 1000) {
          isLockedAtMin = false;
          finalTarget = sensorTarget;
          lightThresholdStartTime = 0;
        } else {
          finalTarget = darkRoomBrightness; // Stay locked until 1s passes
        }
      } else {
        // Light dropped back down or stayed low, reset timer
        lightThresholdStartTime = 0;
        finalTarget = darkRoomBrightness;
      }
    } else {
      // Normal Operation Mode
      if (sensorTarget <= FLICKER_LIMIT && darkRoomBrightness < FLICKER_LIMIT) {
        // We have entered the "Flicker Zone". Target the user's absolute minimum.
        finalTarget = darkRoomBrightness;
        
        // Once the smooth transition reaches the floor, lock it there.
        if (currentBrightness == darkRoomBrightness) {
          isLockedAtMin = true;
        }
      } else {
        // We are above 11%, use the sensor target directly
        finalTarget = sensorTarget;
        lightThresholdStartTime = 0;
      }
    }

    // Apply the smooth transition (+1 or -1 every 50ms)
    if (currentBrightness != finalTarget && millis() - lastAutoBrightnessUpdate >= autoBrightnessUpdateInterval) {
      currentBrightness += (currentBrightness < finalTarget) ? 1 : -1;
      if (dma_display) dma_display->setBrightness8(currentBrightness);
      lastAutoBrightnessUpdate = millis();
    }
  } else if (currentBrightness != brightness) {
    // Manual Mode
    currentBrightness = brightness;
    if (dma_display) dma_display->setBrightness8(currentBrightness);
    isLockedAtMin = false; // Reset lock if auto-brightness is disabled
  }

  bool displayClockFace = false; // Flag to decide if we draw a clock screen
  static bool ntpSyncAttemptedThisConnection = false; // Moved here for better scope visibility across states

switch (currentState) {
    case STATE_WIFI_NO_CREDENTIALS: { // State 0
      // This state means either no creds, or previous attempts failed and we need the portal.
      // WiFiManager's process() should be starting/running the config portal.
      if (SETUP_PORTAL_TIMEOUT == 0 || millis() - stateNOWIFITime < SETUP_PORTAL_TIMEOUT) {
        Screen91(); // Show "Connect to AP" QR screen
      } else { 
        displayClockFace = true; // Fallback to clock display
      }
      
      if (WiFi.status() == WL_CONNECTED) {
          Serial.println("WiFi connected (likely via portal). Transitioning to STATE_WIFI_CONNECTING.");
          currentState = STATE_WIFI_CONNECTING;
          stateStartTime = millis();
          ntpSyncAttemptedThisConnection = false; 
      }
      // If WiFiManager saves new credentials, it usually reboots or calls a save callback.
      // If it connects without saving new ones (e.g., user selected existing network from portal),
      // the WiFi.status() check above handles it.
      break;
    }

    case STATE_WIFI_SETUP: { // State 1: Has saved credentials, trying to connect
      displayClockFace = true;
      static uint8_t setupFailureCount = 0;
      const unsigned long setupAttemptTimeoutMs = 20000;       // unchanged: this attempt failed after 20s
      const uint8_t maxSetupRetriesBeforePortal = 15;          // ~15 * 20s = 5 min of quiet retrying first

      if (WiFi.status() == WL_CONNECTED) {
          Serial.println("WiFi connected (STATE_WIFI_SETUP). Transitioning to STATE_WIFI_CONNECTING.");
          currentState = STATE_WIFI_CONNECTING;
          stateStartTime = millis();
          ntpSyncAttemptedThisConnection = false;
          setupFailureCount = 0;
      } else if (millis() - stateStartTime > setupAttemptTimeoutMs) {
          setupFailureCount++;
          if (setupFailureCount >= maxSetupRetriesBeforePortal) {
              Serial.println("Sustained WiFi failure (~5 min). Falling back to config portal.");
              currentState = STATE_WIFI_NO_CREDENTIALS;
              stateStartTime = millis();
              stateNOWIFITime = millis(); // Reset portal timeout timer for Screen91
              setupFailureCount = 0;
              myWM.startConfigPortal(AutoChipID); // Explicitly start portal
          } else {
              Serial.printf("STATE_WIFI_SETUP retry %u/%u failed; retrying with saved creds (no portal yet).\n",
                             setupFailureCount, maxSetupRetriesBeforePortal);
              WiFi.reconnect();
              stateStartTime = millis();
          }
      }
      break;
    }

    case STATE_WIFI_CONNECTING: { // State 2
      displayClockFace = true; 

      if (WiFi.status() != WL_CONNECTED) {
          Serial.println("WiFi disconnected during STATE_WIFI_CONNECTING. Transitioning to STATE_WIFI_DISCONNECTED.");
          currentState = STATE_WIFI_DISCONNECTED;
          stateStartTime = millis();
          if (networkServicesStarted) {
            // Optionally stop tasks here if desired, or let them fail gracefully
            // For simplicity, just flag that they aren't "running" in a connected sense
            networkServicesStarted = false; // To allow re-init on next connect
          }
          ntpSyncAttemptedThisConnection = false; 
          break; 
      }
      
      if (!networkServicesStarted) {
          Serial.println("STATE_WIFI_CONNECTING: Starting network services...");
          server.begin();
          Serial.println("Web server started.");
          ArduinoOTA.begin();
          Serial.println("Arduino OTA initialized.");

          // Task creation should be idempotent or handles should be checked if tasks can persist
          // For simplicity, assume they are created once when networkServicesStarted is false
          BaseType_t webServerTaskStatus = xTaskCreatePinnedToCore(webServerTask, "WebServerTask", 16384, NULL, 1, NULL, 0);
          BaseType_t fetchWeatherTaskStatus = xTaskCreatePinnedToCore(fetchWeatherTask, "FetchWeatherTask", 8192, NULL, 1, NULL, 1);

          if (webServerTaskStatus != pdPASS || fetchWeatherTaskStatus != pdPASS) {
              Serial.println("Error: Failed to create one or more tasks!");
          } else {
              Serial.println("WebServerTask and FetchWeatherTask created.");
          }
          networkServicesStarted = true;
      }
      
      if (!ntpSyncAttemptedThisConnection) {
          Serial.println ("Getting time from NTP...");
          synchroniseWith_NTP_Time(); 
          lastNTPSync = millis();     
          ntpSyncAttemptedThisConnection = true; 
      }

      getLocalTime(&timeinfo, 100); 
      if (timeinfo.tm_year + 1900 >= 2000) { 
          Serial.println("Time synchronized. Transitioning to STATE_RUNNING.");
          currentState = STATE_RUNNING;
          stateStartTime = millis();
          syncESPtoRTC();          
          needWeatherUpdate = true; 
          ntpSyncAttemptedThisConnection = false; 
      } else if (ntpSyncAttemptedThisConnection && (millis() - lastNTPSync > 30000)) { 
          Serial.println("NTP sync timeout. Transitioning to STATE_RUNNING (using RTC).");
          currentState = STATE_RUNNING; 
          stateStartTime = millis();
          syncRTCtoESP(); 
          getLocalTime(&timeinfo); 
          ntpSyncAttemptedThisConnection = false; 
      }
      break;
    }

    case STATE_RUNNING: { // State 3
      displayClockFace = true;
      if (WiFi.status() != WL_CONNECTED) {
          Serial.println("WiFi disconnected during STATE_RUNNING. Transitioning to STATE_WIFI_DISCONNECTED.");
          currentState = STATE_WIFI_DISCONNECTED;
          stateStartTime = millis();
          // Consider if OTA handle should be paused
      } else {
          if (networkServicesStarted) ArduinoOTA.handle(); 
          checkNTPSync(); 
      }
      break;
    }

    case STATE_WIFI_DISCONNECTED: { // State 4
      displayClockFace = true;
      static unsigned long lastReconnectAttempt = 0;
      const unsigned long reconnectRetryInterval = 10000; // actively kick every 10s instead of just waiting

      if (WiFi.status() == WL_CONNECTED) {
          Serial.println("WiFi reconnected (from DISCONNECTED). Transitioning to STATE_WIFI_CONNECTING.");
          currentState = STATE_WIFI_CONNECTING;
          stateStartTime = millis();
          ntpSyncAttemptedThisConnection = false;
      } else {
          if (millis() - lastReconnectAttempt >= reconnectRetryInterval) {
              Serial.println("STATE_WIFI_DISCONNECTED: actively calling WiFi.reconnect()...");
              WiFi.reconnect();
              lastReconnectAttempt = millis();
          }
          if (millis() - stateStartTime > 60000) { // FIX: was 3600000 (1hr) despite the comment always saying 1 minute
              Serial.println("Persistently disconnected for 60s. Transitioning to STATE_WIFI_SETUP to re-attempt with saved creds.");
              currentState = STATE_WIFI_SETUP; // Try connecting with saved creds again
              stateStartTime = millis();
              // If STATE_WIFI_SETUP also fails, it will go to NO_CREDENTIALS for portal
          }
      }
      break;
    }
  }
  
  // --- Centralized Display Logic ---
  if (displayClockFace) {
    // // Screen 6 is only for RUNNING state. Screen 7 is only for NO_CREDENTIALS portal active.
    // if (currentScreen == 6 && currentState != STATE_RUNNING) {
    //     currentScreen = 7; // Default to screen 1 if trying to access screen 6 when not running
    // }
    // if (currentScreen == 7 && currentState != STATE_WIFI_NO_CREDENTIALS) {
    //     currentScreen = 1; // Default to screen 1 if trying to access screen 7 when not in no-creds state
    // }
    
    choosescreen(); 

    bool showRedDot = false;
    if (currentState == STATE_WIFI_DISCONNECTED) showRedDot = true;
    else if (currentState == STATE_WIFI_SETUP) showRedDot = true;
    else if (currentState == STATE_WIFI_NO_CREDENTIALS && SETUP_PORTAL_TIMEOUT > 0 && (millis() - stateNOWIFITime >= SETUP_PORTAL_TIMEOUT) ) showRedDot = true;
    else if (currentState == STATE_WIFI_CONNECTING && (timeinfo.tm_year + 1900 < 2000)) showRedDot = true;

    if (showRedDot) {
      if(dma_canvas_is_valid()) dma_canvas.drawPixel(0, 31, cc_bred); 
    }
  } 
  // If !displayClockFace, Screen7() has already prepared dma_canvas.
  
  debounceSaveSettings();
  handleSerialInput();
  
  if(dma_display_is_valid() && dma_canvas_is_valid()) {    
    memcpy(screenshotBuffer, dma_canvas.getBuffer(), sizeof(screenshotBuffer)); // Before pushing the buffer to the display, copy the completed frame to our safe buffer for the web server to use.
    dma_display->drawRGBBitmap(0, 0, dma_canvas.getBuffer(), dma_canvas.width(), dma_canvas.height());
  }
  vTaskDelay(1); 
}




// Helper functions to check if display objects are initialized (optional but good practice)
bool dma_display_is_valid() {
  return dma_display != nullptr;
}

bool dma_canvas_is_valid() {
  // GFXcanvas16 is usually stack allocated or global, so it's always "valid" in terms of memory.
  // This is more for consistency if you were dynamically allocating it.
  return true; 
}




// Helper function for rendering an animated value pair
void renderAnimatedValue_Left_Right(
    GFXcanvas16& dma_canvas,      // Canvas object
    unsigned long currentMillis,  // Current time from millis()
    bool enabled,                 // Is this specific animation slot enabled?
    float valueA, const String& prefixA, const String& suffixA, // First value and its formatting
    float valueB, const String& prefixB, const String& suffixB, // Second value and its formatting
    int baseX, int baseY,         // X, Y coordinates for this animation
    uint16_t color,               // Color for the text
    int& textWidthCache,           // Reference to a static int variable for caching text width for this slot
    int decimalPlaces             // <<< NEW PARAMETER for number of decimal places
  ) {
    if (!enabled) return;

    String strA = prefixA + String(valueA,decimalPlaces) + suffixA;
    String strB = prefixB + String(valueB,decimalPlaces) + suffixB;

    // Calculate and cache max text width for this animation slot if not already done
    if (textWidthCache == 0) {
        int16_t x1_temp, y1_temp; 
        uint16_t w_a_temp, h_a_temp, w_b_temp, h_b_temp;
        // Note: dma_canvas.setFont() should have been called before this helper
        dma_canvas.getTextBounds(strA.c_str(), 0, 0, &x1_temp, &y1_temp, &w_a_temp, &h_a_temp);
        dma_canvas.getTextBounds(strB.c_str(), 0, 0, &x1_temp, &y1_temp, &w_b_temp, &h_b_temp);
        textWidthCache = max(w_a_temp, w_b_temp);
        // Fallback if calculation yields 0 (e.g., empty strings or font issue)
        if (textWidthCache == 0) { textWidthCache = 10; } 
    }

    int currentCycleTime = (currentMillis / 1000) % 20; // 0-19 for a 20-second full cycle
    dma_canvas.setTextColor(color);

    if ((currentMillis % 10000) < 9800) { // Static display part (9.5s of a 10s half-cycle)
        // Determine which string to show: strA for 0-9s of cycle, strB for 10-19s
        bool showA = (currentCycleTime < 10);
        dma_canvas.setCursor(baseX, baseY);
        dma_canvas.print(showA ? strA : strB);
    } else { // Animation part (last 0.5s of a 10s half-cycle)
        float animProgress = ((currentMillis % 10000) - 9800) / 200.0f; // Progress: 0.0 to 1.0

        // Determine which string is sliding out and which is sliding in
        String strOut = (currentCycleTime < 10) ? strA : strB; // String that was displayed
        String strIn  = (currentCycleTime < 10) ? strB : strA; // String to transition to
        
        // Calculate positions based on original animation style
        int currentOutX = baseX - (int)(animProgress * textWidthCache);             // Slides from baseX towards left
        int currentInX  = baseX - textWidthCache + (int)(animProgress * textWidthCache); // Slides in from left

        dma_canvas.setCursor(currentOutX, baseY);
        dma_canvas.print(strOut);
        dma_canvas.setCursor(currentInX, baseY);
        dma_canvas.print(strIn);
    }
}

void renderAnimatedValue_Down_Up( // Renamed function
    GFXcanvas16& dma_canvas,      // Canvas object
    unsigned long currentMillis,  // Current time from millis()
    bool enabled,                 // Is this specific animation slot enabled?
    float valueA, const String& prefixA, const String& suffixA, // First value and its formatting
    float valueB, const String& prefixB, const String& suffixB, // Second value and its formatting
    int baseX, int baseY,         // X, Y coordinates for this animation (baseY is the target top Y)
    uint16_t colorA,              // Color for strA (valueA)
    uint16_t colorB,              // Color for strB (valueB)
    int& textHeightCache,         // text height
    int decimalPlaces
  ) {
    if (!enabled) return;

    String strA = prefixA + String(valueA, decimalPlaces) + suffixA;
    String strB = prefixB + String(valueB, decimalPlaces) + suffixB;

    // Calculate and cache max text height for this animation slot if not already done
    if (textHeightCache == 0) {
        int16_t x1_temp, y1_temp;
        uint16_t w_a_temp, h_a_temp, w_b_temp, h_b_temp;
        // Note: dma_canvas.setFont() should have been called before this helper
        dma_canvas.getTextBounds(strA.c_str(), 0, 0, &x1_temp, &y1_temp, &w_a_temp, &h_a_temp);
        dma_canvas.getTextBounds(strB.c_str(), 0, 0, &x1_temp, &y1_temp, &w_b_temp, &h_b_temp);
        textHeightCache = max(h_a_temp, h_b_temp); // <<< CHANGED: Calculate max height
        // Fallback if calculation yields 0 (e.g., empty strings or font issue)
        if (textHeightCache == 0) { textHeightCache = 8; } // Default height, adjust if needed
    }

    int currentCycleTime = (currentMillis / 1000) % 20; // 0-19 for a 20-second full cycle

    if ((currentMillis % 10000) < 9800) { // Static display part (9.8s of a 10s half-cycle)
        // Determine which string to show: strA for 0-9s of cycle, strB for 10-19s
        bool showA = (currentCycleTime < 10);
        dma_canvas.setTextColor(showA ? colorA : colorB);
        dma_canvas.setCursor(baseX, baseY); // X is constant, Y is the target top Y
        dma_canvas.print(showA ? strA : strB);
    } else { // Animation part (last 0.2s of a 10s half-cycle)
        float animProgress = ((currentMillis % 10000) - 9800) / 200.0f; // Progress: 0.0 to 1.0

        // Ensure animProgress is capped between 0.0 and 1.0 if millis() timing is tricky
        animProgress = max(0.0f, min(1.0f, animProgress));

        String strOut = (currentCycleTime < 10) ? strA : strB; // String that was displayed
        String strIn  = (currentCycleTime < 10) ? strB : strA; // String to transition to
        uint16_t colorOut = (currentCycleTime < 10) ? colorA : colorB; // <<< Color for outgoing text
        uint16_t colorIn  = (currentCycleTime < 10) ? colorB : colorA; // <<< Color for incoming text

        // Calculate Y positions for vertical sliding animation
        int currentOutY = baseY + (int)(animProgress * textHeightCache);          // Slides from baseY downwards
        int currentInY  = baseY + textHeightCache - (int)(animProgress * textHeightCache); // Slides in from (baseY + textHeightCache) upwards to baseY

        // Draw outgoing text sliding down
        dma_canvas.setTextColor(colorOut);
        dma_canvas.setCursor(baseX, currentOutY);
        dma_canvas.print(strOut);

        // Draw incoming text sliding up
        dma_canvas.setTextColor(colorIn);
        dma_canvas.setCursor(baseX, currentInY);
        dma_canvas.print(strIn);
    }
}


void Screen1() { // Info Clock
  int screenIndex = currentScreen - 1;
  const auto& settings = allScreenSettings[screenIndex];

  dma_canvas.fillScreen(0);
  
  getLocalTime(&timeinfo);  
  struct tm localTimeinfo;
  if (!getLocalTime(&localTimeinfo)) {
      DateTime rtcTime = rtc.now();
      localTimeinfo.tm_year = rtcTime.year() - 1900;
      localTimeinfo.tm_mon = rtcTime.month() - 1;
      localTimeinfo.tm_mday = rtcTime.day();
      localTimeinfo.tm_hour = rtcTime.hour();
      localTimeinfo.tm_min = rtcTime.minute();
      localTimeinfo.tm_sec = rtcTime.second();
      localTimeinfo.tm_isdst = -1;
  }
  timeinfo = localTimeinfo;

  uint16_t time_color = dma_display->color565(settings.time_col.r, settings.time_col.g, settings.time_col.b);
  uint16_t ampm_color = dma_display->color565(settings.ampm_col.r, settings.ampm_col.g, settings.ampm_col.b);
  uint16_t seconds_color = dma_display->color565(settings.seconds_col.r, settings.seconds_col.g, settings.seconds_col.b);
  uint16_t day_color = dma_display->color565(settings.day_col.r, settings.day_col.g, settings.day_col.b);
  uint16_t date_color = dma_display->color565(settings.date_col.r, settings.date_col.g, settings.date_col.b);
  uint16_t month_color = dma_display->color565(settings.month_col.r, settings.month_col.g, settings.month_col.b);
  uint16_t date_bg_color = dma_display->color565(settings.dateBG_col.r, settings.dateBG_col.g, settings.dateBG_col.b);
  uint16_t temp_col = dma_display->color565(settings.temp_col.r, settings.temp_col.g, settings.temp_col.b);
  uint16_t humidity_col = dma_display->color565(settings.humidity_col.r, settings.humidity_col.g, settings.humidity_col.b);

  // --- MODIFIED: Centralized Temperature Preparation Logic ---
  String unitSuffix = (tempUnits == "fahrenheit") ? "f" : "c";
  bool isFahrenheit = (tempUnits == "fahrenheit");
  
  float effectiveInternalTemp = internalTemp + indoorTempOffset; // Apply offset
  
  float internalTempToDisplay = effectiveInternalTemp;
  if (isFahrenheit && effectiveInternalTemp > -99.0) {
      internalTempToDisplay = (effectiveInternalTemp * 9.0 / 5.0) + 32.0;
  }
  float webTempInCelsius = (tempType == "feels_like") ? currentApparentTemp.toFloat() : currentTemp.toFloat();
  float webTempToDisplay = webTempInCelsius;
  if (isFahrenheit) {
      webTempToDisplay = (webTempInCelsius * 9.0 / 5.0) + 32.0;
  }
  float minTempToDisplay = todayMinTemp;
  float maxTempToDisplay = todayMaxTemp;
  if (isFahrenheit) {
      minTempToDisplay = (todayMinTemp * 9.0 / 5.0) + 32.0;
      maxTempToDisplay = (todayMaxTemp * 9.0 / 5.0) + 32.0;
  }
  
  updateCurrentHoursMins();

  static int temperatureAnimationTextWidthCache = 0;
  static int humidityAnimationTextWidthCache = 0;

  String timeStr = String(current_hoursmins);
  String hoursMins, seconds, ampm;
  if (settings.twentyFourHourSwitch) {
    if (settings.secondsSwitch) {
      hoursMins = timeStr.substring(0, 5);
      seconds = timeStr.substring(6);
    } else {
      hoursMins = timeStr.substring(0, 5);
    }
  } else {
    if (settings.ampmSwitch) {
      if (settings.secondsSwitch) {
        hoursMins = timeStr.substring(0, 5);
        seconds = timeStr.substring(6, 8);
        ampm = timeStr.substring(9);
      } else {
        hoursMins = timeStr.substring(0, 5);
        ampm = timeStr.substring(6);
      }
    } else {
      if (settings.secondsSwitch) {
        hoursMins = timeStr.substring(0, 5);
        seconds = timeStr.substring(6);
      } else {
        hoursMins = timeStr.substring(0, 5);
      }
    }
  }

  dma_canvas.setFont(&timeFont);
  int16_t x1, y1;
  uint16_t w, h, totalWidth = 0;
  dma_canvas.getTextBounds(hoursMins, 0, 18, &x1, &y1, &w, &h);
  totalWidth += w;
  if (settings.secondsSwitch && seconds.length() > 0) {
    dma_canvas.setFont(&TomThumb);
    dma_canvas.getTextBounds(seconds, 0, 19, &x1, &y1, &w, &h);
    totalWidth += w + 1;
  }
  if (!settings.twentyFourHourSwitch && settings.ampmSwitch && ampm.length() > 0) {
    dma_canvas.setFont(&TomThumb);
    dma_canvas.getTextBounds(ampm, 0, 19, &x1, &y1, &w, &h);
    totalWidth += w + 2;
  }

  int xPos = (PANEL_RES_X / 2) - (totalWidth / 2);
  int yPos = 20;
  if (hoursMins.length() > 0) {
    dma_canvas.setTextColor(time_color, cc_blk);
    dma_canvas.setFont(&timeFont);
    dma_canvas.setCursor(xPos, yPos - timeFontOffset);
    dma_canvas.print(hoursMins);
    dma_canvas.getTextBounds(hoursMins, xPos, yPos, &x1, &y1, &w, &h);
    xPos += w;
  }
  if (settings.secondsSwitch && seconds.length() > 0) {
    dma_canvas.setTextColor(seconds_color, cc_blk);
    dma_canvas.setFont(&TomThumb);
    dma_canvas.setCursor(xPos + 1, yPos + 1);
    dma_canvas.print(seconds);
    dma_canvas.getTextBounds(seconds, xPos, yPos, &x1, &y1, &w, &h);
    xPos += w + 1;
  }
  if (!settings.twentyFourHourSwitch && settings.ampmSwitch && ampm.length() > 0) {
    dma_canvas.setTextColor(ampm_color, cc_blk);
    dma_canvas.setFont(&TomThumb);
    dma_canvas.setCursor(xPos + 2, yPos + 1);
    dma_canvas.print(ampm);
  }

  String dateString = "";
  char dayBuf[16] = "";
  char monthBuf[16] = "";

  if (settings.daySwitch) {
      if (currentLanguage == "de") {
        const char* format = (settings.daySwitch && !settings.dateSwitch && !settings.monthSwitch) ? days_DE_long[timeinfo.tm_wday] : days_DE_short[timeinfo.tm_wday];
        strncpy(dayBuf, format, sizeof(dayBuf));
      } else if (currentLanguage == "sv") { // ADDED SWEDISH
        const char* format = (settings.daySwitch && !settings.dateSwitch && !settings.monthSwitch) ? days_SE_long[timeinfo.tm_wday] : days_SE_short[timeinfo.tm_wday];
        strncpy(dayBuf, format, sizeof(dayBuf));
      } else {
        strftime(dayBuf, sizeof(dayBuf), (settings.daySwitch && !settings.dateSwitch && !settings.monthSwitch) ? "%A" : "%a", &timeinfo);
      }
      dateString += String(dayBuf) + " ";
  }
  if (settings.dateSwitch) {
    dateString += String(timeinfo.tm_mday) + " ";
  }
  if (settings.monthSwitch) {
      if (currentLanguage == "de") {
        const char* format = (settings.monthSwitch && !settings.daySwitch && !settings.dateSwitch) ? months_DE_long[timeinfo.tm_mon] : months_DE_short[timeinfo.tm_mon];
        strncpy(monthBuf, format, sizeof(monthBuf));
      } else if (currentLanguage == "sv") { // ADDED SWEDISH
        const char* format = (settings.monthSwitch && !settings.daySwitch && !settings.dateSwitch) ? months_SE_long[timeinfo.tm_mon] : months_SE_short[timeinfo.tm_mon];
        strncpy(monthBuf, format, sizeof(monthBuf));
      } else {
        strftime(monthBuf, sizeof(monthBuf), (settings.monthSwitch && !settings.daySwitch && !settings.dateSwitch) ? "%B" : "%b", &timeinfo);
      }
      dateString += String(monthBuf);
  }
  dateString.trim();

  if (settings.daySwitch || settings.dateSwitch || settings.monthSwitch) {
    dma_canvas.fillRect(0, 22, 64, 10, date_bg_color);
    dma_canvas.setFont(&Font_5x7_practical8pt7b);
    dma_canvas.getTextBounds(dateString, 0, 30, &x1, &y1, &w, &h);
    int xPosDate = (PANEL_RES_X / 2) - (w / 2);
    int yDatePos = 29;
    
    if (settings.daySwitch) {
      dma_canvas.setTextColor(day_color);
      dma_canvas.setCursor(xPosDate, yDatePos);
      dma_canvas.print(dayBuf);
      dma_canvas.getTextBounds(dayBuf, xPosDate, yDatePos, &x1, &y1, &w, &h);
      xPosDate += w + 2;
    }
    if (settings.dateSwitch) {
      dma_canvas.setTextColor(date_color);
      dma_canvas.setCursor(xPosDate, yDatePos);
      dma_canvas.print(String(timeinfo.tm_mday));
      dma_canvas.getTextBounds(String(timeinfo.tm_mday), xPosDate, yDatePos, &x1, &y1, &w, &h);
      xPosDate += w + 2;
    }
    if (settings.monthSwitch) {
      dma_canvas.setTextColor(month_color);
      dma_canvas.setCursor(xPosDate, yDatePos);
      dma_canvas.print(monthBuf);
    }
  }

  dma_canvas.setFont(&Tiny_Phil);
  if (selectedWeatherService != "none") {
      unsigned long current_millis_for_anim = millis();
      renderAnimatedValue_Left_Right(dma_canvas, current_millis_for_anim,
          settings.temperatureSwitch,
          internalTempToDisplay, "#", unitSuffix,
          webTempToDisplay, "$", unitSuffix,
          0, 4, temp_col,
          temperatureAnimationTextWidthCache, 1
      );
      renderAnimatedValue_Left_Right(dma_canvas, current_millis_for_anim,
          settings.humiditySwitch,
          internalHumid, "#", "%",
          currentHumidity.toFloat(), "$", "%",
          0, 10, humidity_col,
          humidityAnimationTextWidthCache, 0
      );
      if (settings.minMaxTempsSwitch) {
        dma_canvas.setFont(&TomThumb);
        dma_canvas.setTextColor(temp_col);
        drawRightString(String((int)round(maxTempToDisplay)) + unitSuffix, 0, 5);
        dma_canvas.setTextColor(humidity_col);
        drawRightString(String((int)round(minTempToDisplay)) + unitSuffix, 0, 11);
      }
      if (settings.iconsSwitch) {
        displayWeatherIcon(weatherIcon);
      }
  } else {
      if (settings.temperatureSwitch) {
          dma_canvas.setTextColor(temp_col);
          dma_canvas.setCursor(0, 4);
          dma_canvas.print("#" + String(internalTempToDisplay, 1) + unitSuffix);
      }
      if (settings.humiditySwitch) {
          dma_canvas.setTextColor(humidity_col);
          dma_canvas.setCursor(0, 10);
          dma_canvas.print("#" + String((int)internalHumid) + "%");
      }
  }
}


void Screen2() { // Large Weather Clock
  // Get the settings for the current screen (currentScreen is 1-based, vector is 0-based).
  int screenIndex = currentScreen - 1;
  const auto& settings = allScreenSettings[screenIndex];

  dma_canvas.fillScreen(0);
  dma_canvas.setFont(&Font_5x7_practical8pt7b);

  // Get time from system (either NTP or RTC)
  struct tm localTimeinfo;
  if (!getLocalTime(&localTimeinfo)) {
      DateTime rtcTime = rtc.now();
      localTimeinfo.tm_year = rtcTime.year() - 1900;
      localTimeinfo.tm_mon = rtcTime.month() - 1;
      localTimeinfo.tm_mday = rtcTime.day();
      localTimeinfo.tm_hour = rtcTime.hour();
      localTimeinfo.tm_min = rtcTime.minute();
      localTimeinfo.tm_sec = rtcTime.second();
      localTimeinfo.tm_isdst = -1;
  }
  timeinfo = localTimeinfo;

  // Use the settings object to define colors for this screen.
  uint16_t time_color = dma_display->color565(settings.time_col.r, settings.time_col.g, settings.time_col.b);
  uint16_t ampm_color = dma_display->color565(settings.ampm_col.r, settings.ampm_col.g, settings.ampm_col.b);
  uint16_t seconds_color = dma_display->color565(settings.seconds_col.r, settings.seconds_col.g, settings.seconds_col.b);
  uint16_t day_color = dma_display->color565(settings.day_col.r, settings.day_col.g, settings.day_col.b);
  uint16_t date_color = dma_display->color565(settings.date_col.r, settings.date_col.g, settings.date_col.b);
  uint16_t month_color = dma_display->color565(settings.month_col.r, settings.month_col.g, settings.month_col.b);
  uint16_t date_bg_color = dma_display->color565(settings.dateBG_col.r, settings.dateBG_col.g, settings.dateBG_col.b);
  uint16_t temp_col = dma_display->color565(settings.temp_col.r, settings.temp_col.g, settings.temp_col.b);
  uint16_t humidity_col = dma_display->color565(settings.humidity_col.r, settings.humidity_col.g, settings.humidity_col.b);
  
  // --- MODIFIED: Centralized Temperature Preparation Logic ---
  String unitSuffix = (tempUnits == "fahrenheit") ? "f" : "c";
  bool isFahrenheit = (tempUnits == "fahrenheit");

  // Apply the stored offset to the raw internal temperature
  float effectiveInternalTemp = internalTemp + indoorTempOffset; 

  float internalTempToDisplay = effectiveInternalTemp;
  if (isFahrenheit && effectiveInternalTemp > -99.0) {
      internalTempToDisplay = (effectiveInternalTemp * 9.0 / 5.0) + 32.0;
  }
  float webTempInCelsius = (tempType == "feels_like") ? currentApparentTemp.toFloat() : currentTemp.toFloat();
  float webTempToDisplay = webTempInCelsius;
  if (isFahrenheit) {
      webTempToDisplay = (webTempInCelsius * 9.0 / 5.0) + 32.0;
  }
  float minTempToDisplay = todayMinTemp;
  float maxTempToDisplay = todayMaxTemp;
  if (isFahrenheit) {
      minTempToDisplay = (todayMinTemp * 9.0 / 5.0) + 32.0;
      maxTempToDisplay = (todayMaxTemp * 9.0 / 5.0) + 32.0;
  }
  
  static int temperatureAnimationTextHeightCache = 0;
  static int humidityAnimationTextHeightCache = 0;

  int16_t x1, y1;
  uint16_t w_time, h_time, w_datum = 0;
  int xLine = 32;

  // --- ICON & DIVIDER ---
  if (selectedWeatherService != "none" && settings.iconsSwitch) {
    displayLargeWeatherIcon(weatherIcon);
  }
  dma_canvas.drawFastVLine(xLine, 0, 32, date_bg_color);

  // --- TIME & DATE DISPLAY ---
  char timeBuf[32];
  char ampmBuf[2] = "";
  if (settings.twentyFourHourSwitch) {
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &timeinfo);
  } else {
    strftime(timeBuf, sizeof(timeBuf), "%l:%M", &timeinfo);
    if (settings.ampmSwitch && timeinfo.tm_hour >= 12) {
      strcpy(ampmBuf, "'");
    }
  }

  dma_canvas.setTextColor(time_color);
  dma_canvas.setFont(&Tidbyt_Numbers1);
  dma_canvas.getTextBounds(String(timeBuf), 0, 0, &x1, &y1, &w_time, &h_time);
  int totalWidth = w_time;
  int xPos = (PANEL_RES_X / 4) - (totalWidth / 2);
  int yPos = 9;
  dma_canvas.setCursor(xPos, yPos - timeFontOffset);
  dma_canvas.print(timeBuf);

  if (!settings.twentyFourHourSwitch && settings.ampmSwitch && strlen(ampmBuf) > 0) {
    dma_canvas.setTextColor(ampm_color);
    dma_canvas.setCursor(xPos + w_time + 1, yPos + 1);
    dma_canvas.print(ampmBuf);
  }

  if (settings.secondsSwitch) {
    float secondsFraction = (float)timeinfo.tm_sec / 60.0;
    int lineWidth = w_time;
    int animatedLength = secondsFraction * lineWidth;
    dma_canvas.drawLine(xPos, yPos + 2, xPos + animatedLength, yPos + 2, seconds_color);
  }

  if (settings.dateSwitch || settings.monthSwitch) {
    String dateString = "";
    char monthBuf[16] = ""; // Increased buffer for German
    char dateBuf[32] = "";

    if (settings.dateSwitch) {
      dateString += String(timeinfo.tm_mday);
      strftime(dateBuf, sizeof(dateBuf), "%d/%m/%y", &timeinfo);
      dma_canvas.setFont(&TomThumb);
      dma_canvas.getTextBounds(String(dateBuf), 0, 0, &x1, &y1, &w_datum, &h_time);
    }
    if (settings.monthSwitch) {
        if (currentLanguage == "de") {
            strncpy(monthBuf, months_DE_short[timeinfo.tm_mon], sizeof(monthBuf));
        } else if (currentLanguage == "sv") { // ADDED SWEDISH
            strncpy(monthBuf, months_SE_short[timeinfo.tm_mon], sizeof(monthBuf));
        } else {
            strftime(monthBuf, sizeof(monthBuf), "%b", &timeinfo);
        }
        dateString += (settings.dateSwitch ? " " : "") + String(monthBuf);
    }

    dma_canvas.setFont(&Font_5x7_practical8pt7b);
    dma_canvas.getTextBounds(dateString, 0, 0, &x1, &y1, &w_time, &h_time);
    int dateXPos = xLine - (w_time - 2);
    int dateYPos = 20;

    if (settings.dateSwitch && settings.monthSwitch) {
      String dayStr = String(timeinfo.tm_mday);
      dma_canvas.setTextColor(date_color);
      dma_canvas.setFont(&Font_5x7_practical8pt7b);
      dma_canvas.setCursor(dateXPos, dateYPos);
      dma_canvas.print(dayStr);
      dma_canvas.getTextBounds(dayStr, 0, 0, &x1, &y1, &w_time, &h_time);
      dateXPos += w_time + 2;
      dma_canvas.setTextColor(month_color);
      dma_canvas.setCursor(dateXPos, dateYPos);
      dma_canvas.print(monthBuf);
    } else if (settings.monthSwitch) {
      dma_canvas.setFont(&Font_5x7_practical8pt7b);
      dma_canvas.setTextColor(month_color);
      dma_canvas.setCursor(dateXPos-3, dateYPos);
      dma_canvas.print(monthBuf);
    } else if (settings.dateSwitch) {
      dma_canvas.setFont(&TomThumb);
      dma_canvas.setCursor(xLine - (w_datum+1) , dateYPos);
      dma_canvas.setTextColor(date_color);
      dma_canvas.print(dateBuf);
    }
  }
  
  if (settings.daySwitch) {
    char dayBuf[16]; // Increased buffer for German
    if (currentLanguage == "de") {
        strncpy(dayBuf, days_DE_short[timeinfo.tm_wday], sizeof(dayBuf));
    } else if (currentLanguage == "sv") { // ADDED SWEDISH
        strncpy(dayBuf, days_SE_short[timeinfo.tm_wday], sizeof(dayBuf));
    } else {
        strftime(dayBuf, sizeof(dayBuf), "%a", &timeinfo);
    }
    dma_canvas.setTextColor(day_color);
    dma_canvas.setFont(&Font_5x7_practical8pt7b);
    dma_canvas.getTextBounds(dayBuf, 0, 0, &x1, &y1, &w_time, &h_time);
    dma_canvas.setCursor((xLine - (w_time + 1)), 30);
    dma_canvas.print(dayBuf);
  }

  // --- WEATHER/SENSOR DISPLAY ---
  if (selectedWeatherService != "none") {
      dma_canvas.setFont(&Tiny_Phil);
      unsigned long current_millis_for_anim = millis();
      renderAnimatedValue_Down_Up(dma_canvas, current_millis_for_anim,
          settings.temperatureSwitch,
          internalTempToDisplay, "", unitSuffix,
          webTempToDisplay, "", unitSuffix,
          34, 24, temp_col, temp_col,
          temperatureAnimationTextHeightCache, 1);
      renderAnimatedValue_Down_Up(dma_canvas, current_millis_for_anim,
          settings.humiditySwitch,
          internalHumid, "", "%",
          currentHumidity.toFloat(), "", "%",
          34, 30, humidity_col, humidity_col,
          humidityAnimationTextHeightCache, 0);
      
      String indicatorSymbolToShow;
      int currentCycleTimeForIndicator = (current_millis_for_anim / 1000) % 20;
      if (currentCycleTimeForIndicator < 10) { indicatorSymbolToShow = "#"; } else { indicatorSymbolToShow = "$"; }                                             
      dma_canvas.setTextColor(temp_col); dma_canvas.setCursor(34, 17); dma_canvas.print(indicatorSymbolToShow);

      if (settings.minMaxTempsSwitch) {
        dma_canvas.setFont(&TomThumb);
        String minMaxSuffix = "'";
        String maxTempStr = String((int)round(maxTempToDisplay)) + minMaxSuffix;
        dma_canvas.setTextColor(temp_col);
        dma_canvas.getTextBounds(maxTempStr, 0, 0, &x1, &y1, &w_time, &h_time);
        dma_canvas.setCursor(64 - w_time - 1, 25);
        dma_canvas.print(maxTempStr);
        
        String minTempStr = String((int)round(minTempToDisplay)) + minMaxSuffix;
        dma_canvas.setTextColor(humidity_col);
        dma_canvas.getTextBounds(minTempStr, 0, 0, &x1, &y1, &w_time, &h_time);
        dma_canvas.setCursor(64 - w_time - 1, 31);
        dma_canvas.print(minTempStr);
      }
  } else {
      dma_canvas.setFont(&Tiny_Phil);
      dma_canvas.setTextColor(temp_col);
      dma_canvas.setCursor(34, 17);
      dma_canvas.print("#");

      if (settings.temperatureSwitch) {
          dma_canvas.setTextColor(temp_col);
          dma_canvas.setCursor(34, 24);
          dma_canvas.print(String(internalTempToDisplay, 1) + unitSuffix);
      }
      if (settings.humiditySwitch) {
          dma_canvas.setTextColor(humidity_col);
          dma_canvas.setCursor(34, 30);
          dma_canvas.print(String((int)internalHumid) + "%");
      }
  }
}

// Function to draw an anti-aliased line (simplified)
// Make sure these helper functions are defined before Screen3 or are globally accessible
// (Copied from previous response for completeness here)

// Helper function to blend two 565 colors
// Factor is 0.0 (pure bg_color) to 1.0 (pure fg_color)
uint16_t blend_565(uint16_t fg_color, uint16_t bg_color, float factor) {
    factor = fmaxf(0.0f, fminf(1.0f, factor)); // Clamp factor

    uint8_t r1 = (fg_color >> 11) & 0x1F;
    uint8_t g1 = (fg_color >> 5) & 0x3F;
    uint8_t b1 = fg_color & 0x1F;

    uint8_t r2 = (bg_color >> 11) & 0x1F;
    uint8_t g2 = (bg_color >> 5) & 0x3F;
    uint8_t b2 = bg_color & 0x1F;

    uint8_t r_blend = static_cast<uint8_t>(r1 * factor + r2 * (1.0f - factor));
    uint8_t g_blend = static_cast<uint8_t>(g1 * factor + g2 * (1.0f - factor));
    uint8_t b_blend = static_cast<uint8_t>(b1 * factor + b2 * (1.0f - factor));

    return (r_blend << 11) | (g_blend << 5) | b_blend;
}

// Function to draw an anti-aliased line (Xiaolin Wu's concept, simplified for GFX)
void draw_aa_line_simple(GFXcanvas16 &canvas, float x0, float y0, float x1, float y1, uint16_t color, float fuzz_factor) {
    // The fuzz factor is the minimum alpha value, controlling the "thickness" of the anti-aliasing.
    // A value of 0.0 is sharp, 1.0 is very blurry.
    const float AA_INTENSITY_MIN_FACTOR = constrain(fuzz_factor, 0.0f, 1.0f);

    auto calculate_final_alpha = [&](float original_coverage_alpha) {
        return AA_INTENSITY_MIN_FACTOR + original_coverage_alpha * (1.0f - AA_INTENSITY_MIN_FACTOR);
    };

    bool steep = fabsf(y1 - y0) > fabsf(x1 - x0);

    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = (dx == 0.0f) ? 1.0f : dy / dx;

    auto plot_aa_pixel = [&](int16_t x, int16_t y, float coverage) {
        if (coverage < 0.001f) return;
        coverage = fmaxf(0.0f, fminf(1.0f, coverage));

        uint16_t original_bg_color;
        float final_alpha = calculate_final_alpha(coverage);

        if (steep) {
            if (y < 0 || y >= canvas.width() || x < 0 || x >= canvas.height()) return;
            original_bg_color = canvas.getPixel(y, x);
            canvas.drawPixel(y, x, blend_565(color, original_bg_color, final_alpha));
        } else {
            if (x < 0 || x >= canvas.width() || y < 0 || y >= canvas.height()) return;
            original_bg_color = canvas.getPixel(x, y);
            canvas.drawPixel(x, y, blend_565(color, original_bg_color, final_alpha));
        }
    };
    
    float xend1 = roundf(x0);
    float yend1 = y0 + gradient * (xend1 - x0);
    float xgap1 = 1.0f - fmodf(x0 + 0.5f, 1.0f);
    int16_t ix1 = static_cast<int16_t>(xend1);
    int16_t iy1 = static_cast<int16_t>(floorf(yend1));
    
    plot_aa_pixel(ix1, iy1,     (1.0f - fmodf(yend1, 1.0f)) * xgap1);
    plot_aa_pixel(ix1, iy1 + 1,         fmodf(yend1, 1.0f)  * xgap1);
    
    float intery = yend1 + gradient;

    float xend2 = roundf(x1);
    float yend2 = y1 + gradient * (xend2 - x1);
    float xgap2 = fmodf(x1 + 0.5f, 1.0f);
    int16_t ix2 = static_cast<int16_t>(xend2);
    int16_t iy2 = static_cast<int16_t>(floorf(yend2));

    plot_aa_pixel(ix2, iy2,     (1.0f - fmodf(yend2, 1.0f)) * xgap2);
    plot_aa_pixel(ix2, iy2 + 1,         fmodf(yend2, 1.0f)  * xgap2);

    for (int16_t x = ix1 + 1; x < ix2; ++x) {
        plot_aa_pixel(x, static_cast<int16_t>(floorf(intery)),     1.0f - fmodf(intery, 1.0f));
        plot_aa_pixel(x, static_cast<int16_t>(floorf(intery)) + 1,         fmodf(intery, 1.0f));
        intery += gradient;
    }
}


void Screen3() {  // Analogue & calendar Clock
 
  dma_canvas.fillScreen(0);

  // Get the settings for the current screen.
  int screenIndex = currentScreen - 1;
  const auto& settings = allScreenSettings[screenIndex];

  // Get time from system - this logic remains the same.
  struct tm localTimeinfo;
  if (!getLocalTime(&localTimeinfo)) {
      DateTime rtcTime = rtc.now();
      localTimeinfo.tm_year = rtcTime.year() - 1900;
      localTimeinfo.tm_mon = rtcTime.month() - 1;
      localTimeinfo.tm_mday = rtcTime.day();
      localTimeinfo.tm_hour = rtcTime.hour();
      localTimeinfo.tm_min = rtcTime.minute();
      localTimeinfo.tm_sec = rtcTime.second();
      localTimeinfo.tm_isdst = -1;
  }
  timeinfo = localTimeinfo;

  // Use the settings object to define colors for this screen.
  uint16_t time_color = dma_display->color565(settings.time_col.r, settings.time_col.g, settings.time_col.b);
  uint16_t ampm_color = dma_display->color565(settings.ampm_col.r, settings.ampm_col.g, settings.ampm_col.b);
  uint16_t seconds_color = dma_display->color565(settings.seconds_col.r, settings.seconds_col.g, settings.seconds_col.b);
  uint16_t day_color = dma_display->color565(settings.day_col.r, settings.day_col.g, settings.day_col.b);
  uint16_t date_color = dma_display->color565(settings.date_col.r, settings.date_col.g, settings.date_col.b);
  uint16_t month_color = dma_display->color565(settings.month_col.r, settings.month_col.g, settings.month_col.b);
  uint16_t date_bg_color = dma_display->color565(settings.dateBG_col.r, settings.dateBG_col.g, settings.dateBG_col.b);
  uint16_t temp_col = dma_display->color565(settings.temp_col.r, settings.temp_col.g, settings.temp_col.b);

  // --- Draw Clock Face Background ---
  if (settings.clockSwitch) {
    if (settings.clock_use_image) {
      for (int i = 0; i < 2048; i++) {
        int x = i % 64; 
        int y = i / 64;
        dma_canvas.drawPixel(x, y, pgm_read_word(&screen3_bitmaps[settings.clock_bitmap_index][i]));
      }
    } else {
      for (int i = 0; i < 2048; i++) {
        if (pgm_read_word(&MASK_Clock[i]) == 0x0000) {
            int x = i % 64; 
            int y = i / 64;
            dma_canvas.drawPixel(x, y, time_color); // 'time_color' is used for the solid clock face.
        }
      }
    }
  }

  // --- Clock Parameters ---
  float clockCenterX_f = 15.5f; 
  float clockCenterY_f = 15.5f;
  float clockRadius_f = 15.0f; 

  // --- Calculate length multiplier from pageSlider2 ---
  float length_multiplier = map_float(settings.pageSlider2, 0, 255, 0.5f, 1.25f);

  // --- Apply multiplier to hand lengths ---
  float minuteLength_f = clockRadius_f * 0.8f * length_multiplier;
  float hourLength_f = clockRadius_f * 0.55f * length_multiplier;
  float secondLength_f = minuteLength_f * 1.05f; // 5% longer than the minute hand
  
  float hour_angle_deg = (timeinfo.tm_hour % 12 + timeinfo.tm_min / 60.0f) * 30.0f; 
  float minute_angle_deg = (timeinfo.tm_min + timeinfo.tm_sec / 60.0f) * 6.0f;
  float second_angle_deg = (timeinfo.tm_sec / 60.0f) * 360.0f;    
  
  float hour_rad = (hour_angle_deg - 90.0f) * PI / 180.0f;
  float minute_rad = (minute_angle_deg - 90.0f) * PI / 180.0f;
  float second_rad = (second_angle_deg - 90.0f) * PI / 180.0f;
  
  float minuteX1_f = clockCenterX_f + minuteLength_f * cosf(minute_rad);
  float minuteY1_f = clockCenterY_f + minuteLength_f * sinf(minute_rad);
  
  float hourX1_f = clockCenterX_f + hourLength_f * cosf(hour_rad);
  float hourY1_f = clockCenterY_f + hourLength_f * sinf(hour_rad);

  float secondX1_f = clockCenterX_f + secondLength_f * cosf(second_rad);
  float secondY1_f = clockCenterY_f + secondLength_f * sinf(second_rad);


  // --- Draw Hands with Anti-Aliasing ---
  // Calculate the fuzz factor from pageSlider (Hand Fuzziness)
  float fuzziness_factor = map_float((float)settings.pageSlider, 0.0f, 255.0f, 0.0f, 1.0f);
  
  // Draw Minute Hand (using 'ampm_color')
  draw_aa_line_simple(dma_canvas, clockCenterX_f, clockCenterY_f, minuteX1_f, minuteY1_f, ampm_color, fuzziness_factor);
  
  // Draw Hour Hand (using 'time_color' as per the original color picker association)
  draw_aa_line_simple(dma_canvas, clockCenterX_f, clockCenterY_f, hourX1_f, hourY1_f, time_color, fuzziness_factor);

  // Draw Seconds Hand
  if (settings.secondsSwitch) {
    draw_aa_line_simple(dma_canvas, clockCenterX_f, clockCenterY_f, secondX1_f, secondY1_f, seconds_color, fuzziness_factor);
  }


  // --- Calendar Area ---
  if (settings.temperatureSwitch) {
    dma_canvas.fillRect(33, 7, 31, 3, temp_col);
    dma_canvas.fillRoundRect(33, 1, 31, 9, 3, temp_col);
    dma_canvas.fillRect(33, 10, 31, 4, date_bg_color);
    dma_canvas.fillRoundRect(33, 10, 31, 21, 3, date_bg_color);
  }

  // Draw month
  char monthBuf[16]; // Increased buffer size for German
  if (currentLanguage == "de") {
    strncpy(monthBuf, months_DE_short[timeinfo.tm_mon], sizeof(monthBuf));
  } else if (currentLanguage == "sv") { // ADDED SWEDISH
    strncpy(monthBuf, months_SE_short[timeinfo.tm_mon], sizeof(monthBuf));
  } else {
    strftime(monthBuf, sizeof(monthBuf), "%b", &timeinfo);
  }

  if (settings.monthSwitch) {
    dma_canvas.setFont(&Font_5x7_practical8pt7b);
    int16_t x1, y1;
    uint16_t w, h;
    dma_canvas.getTextBounds(String(monthBuf), 0, 0, &x1, &y1, &w, &h);
    int monthX = 33 + (16 - w / 2);
    int monthY = 5 + (h / 2);
    if (monthY > 8 && settings.temperatureSwitch) {
      monthY = (5 + (h / 2)) - 1;
      dma_canvas.fillRect(33, 7, 31, 5, temp_col);
    }
    dma_canvas.setTextColor(month_color);
    dma_canvas.setCursor(monthX, monthY);
    dma_canvas.print(monthBuf);
  }

  // Draw date
  char dateBuf[4]; 
  snprintf(dateBuf, sizeof(dateBuf), "%2d", timeinfo.tm_mday);
  String dateNumStr = String(dateBuf); dateNumStr.trim(); 

  if (settings.dateSwitch) { 
    dma_canvas.setFont(&Tidbyt_Numbers1);
    int16_t x1_text, y1_text; uint16_t w_text, h_text;
    dma_canvas.getTextBounds(dateNumStr, 0, 0, &x1_text, &y1_text, &w_text, &h_text);
    int dateX = 33 + (31 - w_text) / 2;
    int dateY_baseline = 10 + (21 + h_text) / 2 - 2;
    
    dma_canvas.setTextColor(date_color);
    dma_canvas.setCursor(dateX, dateY_baseline); 
    dma_canvas.print(dateNumStr);
  }

  // Draw Clock Ticks/Numbers
  if (settings.daySwitch) { // 'daySwitch' controls ticks/numbers on this screen
    uint16_t tick_color_to_use = day_color; 
    if (settings.clockDisplayMode == "4 numbers") {
      for (int i = 0; i < 2048; i++) { 
        if (pgm_read_word(&MASK_Clock_Numbers[i]) == 0x001F) {
          dma_canvas.drawPixel(i % 64, i / 64, tick_color_to_use);
        }
      }
    } else if (settings.clockDisplayMode == "All numbers") {
      for (int i = 0; i < 2048; i++) {
        uint16_t maskVal = pgm_read_word(&MASK_Clock_Numbers[i]);
        if (maskVal == 0xFFE0 || maskVal == 0x001F) {
          dma_canvas.drawPixel(i % 64, i / 64, tick_color_to_use);
        }
      }
    } else if (settings.clockDisplayMode == "4 ticks") {
      for (int i = 0; i < 2048; i++) {
        if (pgm_read_word(&MASK_Clock_Ticks[i]) == 0xF800) {
          dma_canvas.drawPixel(i % 64, i / 64, tick_color_to_use);
        }
      }
    } else if (settings.clockDisplayMode == "All ticks") {
      for (int i = 0; i < 2048; i++) {
        uint16_t maskVal = pgm_read_word(&MASK_Clock_Ticks[i]);
        if (maskVal == 0xF800 || maskVal == 0x07E0) {
          dma_canvas.drawPixel(i % 64, i / 64, tick_color_to_use);
        }
      }
    }
  }
}






// Helper function for mapping float values (ensure this is present)
float map_float(float x, float in_min, float in_max, float out_min, float out_max) {
  if (in_max == in_min) { // Avoid division by zero
    return out_min; 
  }
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void init_star(Star& s, bool initial_setup) {
    // s.y and s.brightness are independent of direction for now
    s.y = random(0, PANEL_RES_Y);
    s.brightness = random(50, 150);

    // --- s.speed calculation logic (from your provided code) ---
    // This logic should result in s.speed having the same sign as EARTH_ROTATION_SPEED_DEG_PER_FRAME
    if (abs(EARTH_ROTATION_SPEED_DEG_PER_FRAME) < 0.001f) { // Earth rotation is practically zero
        s.speed = 0.0f;
    } else {
        float base_min_speed = 0.10f; 
        float base_max_speed = 0.40f; 
        float speed_scale_factor = 1.0f; 

        if (abs(BASELINE_EARTH_ROTATION_FOR_STAR_SPEED) > 0.001f) {
            // This division gives speed_scale_factor the sign of EARTH_ROTATION_SPEED_DEG_PER_FRAME
            // if BASELINE_EARTH_ROTATION_FOR_STAR_SPEED is positive.
            speed_scale_factor = EARTH_ROTATION_SPEED_DEG_PER_FRAME / BASELINE_EARTH_ROTATION_FOR_STAR_SPEED;
        } else {
            // Handle case where BASELINE_EARTH_ROTATION_FOR_STAR_SPEED is near zero
            // Assign a default scale factor, preserving the sign of earth rotation
             speed_scale_factor = (EARTH_ROTATION_SPEED_DEG_PER_FRAME >= 0 ? 1.0f : -1.0f);
        }
        
        float scaled_min_speed = base_min_speed * speed_scale_factor;
        float scaled_max_speed = base_max_speed * speed_scale_factor;

        // Ensure scaled_min_speed is the "lower" bound and scaled_max_speed is the "upper" bound
        // in their signed range. E.g., if negative, min is more negative.
        if (scaled_min_speed > scaled_max_speed) { 
            float temp = scaled_min_speed; 
            scaled_min_speed = scaled_max_speed; 
            scaled_max_speed = temp; 
        }
        
        float effective_earth_speed_direction = (EARTH_ROTATION_SPEED_DEG_PER_FRAME >= 0 ? 1.0f : -1.0f);
        
        if (abs(scaled_max_speed) < 0.01f) { 
             scaled_min_speed = 0.01f * effective_earth_speed_direction;
             scaled_max_speed = 0.02f * effective_earth_speed_direction; 
        } else if (abs(scaled_min_speed) < 0.01f) {
            scaled_min_speed = 0.01f * effective_earth_speed_direction;
            // Ensure min_speed is not "greater" than max_speed after this adjustment
            if ((effective_earth_speed_direction > 0 && scaled_min_speed > scaled_max_speed) ||
                (effective_earth_speed_direction < 0 && scaled_min_speed < scaled_max_speed)) { 
                scaled_min_speed = scaled_max_speed - (0.005f * effective_earth_speed_direction); 
            }
        }
       
        if ( (scaled_min_speed * effective_earth_speed_direction < 0) && (scaled_max_speed * effective_earth_speed_direction < 0) ) { 
             scaled_min_speed *= -1.0f;
             scaled_max_speed *= -1.0f;
        } else if (scaled_min_speed * effective_earth_speed_direction < 0) { 
            scaled_min_speed = 0.01f * effective_earth_speed_direction; 
            if ((effective_earth_speed_direction > 0 && scaled_min_speed > scaled_max_speed) ||
                (effective_earth_speed_direction < 0 && scaled_min_speed < scaled_max_speed) ) {
                 scaled_min_speed = scaled_max_speed - (0.005f * effective_earth_speed_direction);
            }
        }

        if (scaled_min_speed * effective_earth_speed_direction > scaled_max_speed * effective_earth_speed_direction) { 
            scaled_min_speed = scaled_max_speed - (0.01f * effective_earth_speed_direction) ; 
             if (abs(scaled_min_speed) < 0.01f && abs(EARTH_ROTATION_SPEED_DEG_PER_FRAME) > 0.001f) {
                scaled_min_speed = 0.01f * effective_earth_speed_direction;
             }
        }
        
        // Final check to ensure min/max are correctly ordered for random selection
        // If positive direction, min should be <= max.
        // If negative direction, min should be <= max (e.g., -0.4 <= -0.1).
        if ((EARTH_ROTATION_SPEED_DEG_PER_FRAME >= 0 && scaled_min_speed > scaled_max_speed) ||
            (EARTH_ROTATION_SPEED_DEG_PER_FRAME < 0 && scaled_min_speed > scaled_max_speed)) { // min is "less negative" than max, swap
             float temp = scaled_min_speed; scaled_min_speed = scaled_max_speed; scaled_max_speed = temp;
        }

        float random_val_0_to_1 = static_cast<float>(random(0, 1001)) / 1000.0f;
        s.speed = scaled_min_speed + (random_val_0_to_1 * (scaled_max_speed - scaled_min_speed));
    }
    // --- End of s.speed calculation logic ---

    // Set initial or re-entry X position
    if (initial_setup) { 
        s.x = random(0, PANEL_RES_X); // Randomly place on screen for initial setup
    } else { 
        // Star is re-entering. Its speed determines its movement direction.
        // The drawing loop uses `stars[i].x -= stars[i].speed;`
        // If s.speed is positive (or zero, default to R-L), x decreases (R-L). Re-enter from right.
        // If s.speed is negative, x increases (L-R). Re-enter from left.
        if (s.speed >= 0.0f) { // Handles positive speed and zero speed (though zero speed stars won't move off screen)
            s.x = PANEL_RES_X + random(0, 5); // Re-enter from right
        } else { // s.speed is negative
            s.x = 0.0f - random(0, 5);        // Re-enter from left
        }
    }
}

void init_sun_for_sky(SunObject& sun_param, float current_delta_sun) {
    // Sun's speed is calculated in Screen4 based on EARTH_ROTATION_SPEED_DEG_PER_FRAME
    // This speed will have a sign (+ for R-L, - for L-R movement across the sky)
    sun_param.speed = screen4_sun_calculated_speed;

    int current_max_r = 0;
    if (SUN_CORE_RADIUS_S4 >= 0) {
        current_max_r = SUN_CORE_RADIUS_S4 + SUN_FUZZ_LAYERS_S4;
        if (SUN_FUZZ_LAYERS_S4 > 0 && current_max_r > 0) current_max_r -=1;
    } else {
        current_max_r = (SUN_FUZZ_LAYERS_S4 > 0 ? SUN_FUZZ_LAYERS_S4 -1 : 0) ;
    }
    if (current_max_r < 0) current_max_r = 0;

    // Initialize sun's x position based on its speed direction
    if (sun_param.speed >= 0.0f) { // Sun will move Right-to-Left (or be stationary)
        sun_param.x = static_cast<float>(PANEL_RES_X) + static_cast<float>(current_max_r) + 1.0f; // Start off-screen to the RIGHT
    } else { // Sun will move Left-to-Right
        sun_param.x = 0.0f - static_cast<float>(current_max_r) - 1.0f; // Start off-screen to the LEFT
    }

    sun_param.y = (static_cast<float>(PANEL_RES_Y) / 2.0f) - map_float(current_delta_sun, -23.45f, 23.45f, -SUN_Y_OFFSET_RANGE, SUN_Y_OFFSET_RANGE);
    sun_param.y = constrain(sun_param.y,
                           static_cast<float>(current_max_r),
                           static_cast<float>(PANEL_RES_Y) - 1.0f - static_cast<float>(current_max_r));
}

// init_star and init_sun_for_sky functions remain as previously modified.

void Screen4() { // Day/night terminator map
  // Get the settings for the current screen. This is the single source of truth for all options.
  int screenIndex = currentScreen - 1;
  const auto& settings = allScreenSettings[screenIndex];

  // ====================================================================================
  // 3D Version of the clock
  // ====================================================================================
  if (settings.SpareSwitch) {
    TimeDateComponents td_for_display = getTimeDateString();

    // Sourced from settings object.
    uint16_t time_color_val = dma_display->color565(settings.time_col.r, settings.time_col.g, settings.time_col.b);
    uint16_t background_color_s4_val = dma_display->color565(settings.day_col.r, settings.day_col.g, settings.day_col.b);
    uint16_t land_color_map_val = dma_display->color565(settings.land_col.r, settings.land_col.g, settings.land_col.b);
    uint16_t water_color_map_val = dma_display->color565(settings.water_col.r, settings.water_col.g, settings.water_col.b);
    uint16_t ice_color_map_val = dma_display->color565(settings.ice_col.r, settings.ice_col.g, settings.ice_col.b);
    uint16_t night_lights_color_map_val = dma_display->color565(settings.humidity_col.r, settings.humidity_col.g, settings.humidity_col.b);
    uint16_t month_color_for_text_shadow_val = dma_display->color565(settings.month_col.r, settings.month_col.g, settings.month_col.b);

    struct tm localTimeinfo;
    if (!getLocalTime(&localTimeinfo)) {
        DateTime rtcTime = rtc.now();
        localTimeinfo.tm_year = rtcTime.year() - 1900;
        localTimeinfo.tm_mon = rtcTime.month() - 1;
        localTimeinfo.tm_mday = rtcTime.day();
        localTimeinfo.tm_hour = rtcTime.hour();
        localTimeinfo.tm_min = rtcTime.minute();
        localTimeinfo.tm_sec = rtcTime.second();
        localTimeinfo.tm_isdst = -1;
    }
    timeinfo = localTimeinfo;

    EARTH_ROTATION_SPEED_DEG_PER_FRAME = map_float((float)settings.pageSlider2, 0.0f, 255.0f, -2.0f, 2.0f);
    int new_num_stars = (int)map_float((float)settings.pageSlider3, 0.0f, 255.0f, 0.0f, (float)MAX_STAR_CAPACITY - 1.0f);
    new_num_stars = constrain(new_num_stars, 0, MAX_STAR_CAPACITY - 1);
    
    if (new_num_stars != NUM_STARS || !stars_initialized) {
        NUM_STARS = new_num_stars;
        stars_initialized = false;
    }

    time_t utcTimeForSolarCalc = mktime(&timeinfo) - (timezoneOffset * 3600);
    float lambda_sun_calc, delta_sun_calc; 
    solarPosition(utcTimeForSolarCalc, lambda_sun_calc, delta_sun_calc); 
    lambda_sun_calc += (terminatorOffset * 360.0f) / 64.0f; 
    while (lambda_sun_calc < -180.0f) lambda_sun_calc += 360.0f; 
    while (lambda_sun_calc > 180.0f) lambda_sun_calc -= 360.0f;
    
    if (abs(EARTH_ROTATION_SPEED_DEG_PER_FRAME) < 0.001f) {
        screen4_sun_calculated_speed = 0.0f;
    } else {
        screen4_sun_calculated_speed = (static_cast<float>(PANEL_RES_X) * EARTH_ROTATION_SPEED_DEG_PER_FRAME) / 180.0f;
        if (abs(screen4_sun_calculated_speed) < 0.02f && abs(EARTH_ROTATION_SPEED_DEG_PER_FRAME) > 0.001f) {
            screen4_sun_calculated_speed = 0.02f * (EARTH_ROTATION_SPEED_DEG_PER_FRAME > 0 ? 1.0f : -1.0f);
        }
    }
    
    if (!stars_initialized) {
        if (NUM_STARS > 0) { 
            for (int i = 0; i < NUM_STARS; i++) init_star(stars[i], true);
        }
        stars_initialized = true; 
        if (sun_color_global == 0) sun_color_global = dma_display->color565(255, 200, 0); 
    }
    
    const int EARTH_DIAMETER = 30; const int EARTH_RADIUS = EARTH_DIAMETER / 2;
    const int EARTH_CENTER_X = PANEL_RES_X / 2; const int EARTH_CENTER_Y = PANEL_RES_Y / 2; 
    const int MAP_WIDTH = 64; const int MAP_HEIGHT = 32; 
    static float current_rotationAngle_s4_val = 0.0f; 
    
    if (abs(EARTH_ROTATION_SPEED_DEG_PER_FRAME) > 0.001f) { 
        current_rotationAngle_s4_val += EARTH_ROTATION_SPEED_DEG_PER_FRAME; 
        if (current_rotationAngle_s4_val >= 360.0f) current_rotationAngle_s4_val -= 360.0f; 
        if (current_rotationAngle_s4_val < 0.0f) current_rotationAngle_s4_val += 360.0f; 
    }
    
    if (settings.backgroundSwitch) dma_canvas.fillScreen(background_color_s4_val); 
    else dma_canvas.fillScreen(0); 
    
    if (settings.SpareSwitch2) { // Sun Switch is ON
        bool conditions_met_to_start_sun_flag = false;
        float center_lon_earth_view_s5 = -current_rotationAngle_s4_val;
        while (center_lon_earth_view_s5 < -180.0f) center_lon_earth_view_s5 += 360.0f;
        while (center_lon_earth_view_s5 > 180.0f) center_lon_earth_view_s5 -= 360.0f;
        float angular_dist_s5 = lambda_sun_calc - center_lon_earth_view_s5;
        while (angular_dist_s5 < -180.0f) angular_dist_s5 += 360.0f;
        while (angular_dist_s5 > 180.0f) angular_dist_s5 -= 360.0f;
        if (abs(angular_dist_s5) > 90.0f) conditions_met_to_start_sun_flag = true;

        int max_draw_radius_sun = 0;
        if (SUN_CORE_RADIUS_S4 >= 0) {
            max_draw_radius_sun = SUN_CORE_RADIUS_S4 + SUN_FUZZ_LAYERS_S4;
            if (SUN_FUZZ_LAYERS_S4 > 0 && max_draw_radius_sun > 0) max_draw_radius_sun -=1;
        } else {
            max_draw_radius_sun = (SUN_FUZZ_LAYERS_S4 > 0 ? SUN_FUZZ_LAYERS_S4 -1 : 0) ;
        }
        if (max_draw_radius_sun < 0) max_draw_radius_sun = 0;

        if (conditions_met_to_start_sun_flag && !is_screen4_sun_active) {
            init_sun_for_sky(screen4_sun_obj, delta_sun_calc);
            is_screen4_sun_active = true;
        }

        if (is_screen4_sun_active) {
            if (abs(screen4_sun_obj.speed) > 0.001f) { 
                screen4_sun_obj.x -= screen4_sun_obj.speed; 
            }
            screen4_sun_obj.y = (static_cast<float>(PANEL_RES_Y) / 2.0f) - map_float(delta_sun_calc, -23.45f, 23.45f, -SUN_Y_OFFSET_RANGE, SUN_Y_OFFSET_RANGE);
            screen4_sun_obj.y = constrain(screen4_sun_obj.y, static_cast<float>(max_draw_radius_sun), static_cast<float>(PANEL_RES_Y)-1.0f-static_cast<float>(max_draw_radius_sun));

            bool sun_on_screen_flag = (screen4_sun_obj.x + max_draw_radius_sun >= 0.0f) &&
                                      (screen4_sun_obj.x - max_draw_radius_sun < static_cast<float>(PANEL_RES_X));

            if (sun_on_screen_flag) {
                // --- FUZZY SUN DRAWING ---
                int sun_center_x = static_cast<int>(roundf(screen4_sun_obj.x));
                int sun_center_y = static_cast<int>(roundf(screen4_sun_obj.y));
                for (int r_draw = max_draw_radius_sun; r_draw >= 0; r_draw--) {
                    float blend_alpha_val = 0.0f; 
                    if (SUN_CORE_RADIUS_S4 >= 0 && r_draw <= SUN_CORE_RADIUS_S4) { blend_alpha_val = 1.0f; } 
                    else if (SUN_FUZZ_LAYERS_S4 > 0) {
                        float min_fuzz_alpha = 0.3f; float max_fuzz_alpha = 1.0f; 
                        if (SUN_FUZZ_LAYERS_S4 == 1) { blend_alpha_val = max_fuzz_alpha; } 
                        else {
                           int base_radius_for_fuzz = (SUN_CORE_RADIUS_S4 >= 0 ? SUN_CORE_RADIUS_S4 : -1);
                           int current_fuzz_ring_index = r_draw - (base_radius_for_fuzz + 1);
                           if (current_fuzz_ring_index >=0 && current_fuzz_ring_index < SUN_FUZZ_LAYERS_S4) {
                                blend_alpha_val = max_fuzz_alpha - ((float)current_fuzz_ring_index / (float)(SUN_FUZZ_LAYERS_S4 -1 + 0.001f)) * (max_fuzz_alpha - min_fuzz_alpha);
                           }
                        }
                        blend_alpha_val = constrain(blend_alpha_val, 0.0f, 1.0f);
                    }
                    if (blend_alpha_val < 0.01f && r_draw > 0) continue;
                    for (int y_offset_draw = -r_draw; y_offset_draw <= r_draw; ++y_offset_draw) { 
                        for (int x_offset_draw = -r_draw; x_offset_draw <= r_draw; ++x_offset_draw) {
                            if (x_offset_draw*x_offset_draw + y_offset_draw*y_offset_draw <= r_draw*r_draw) {
                                if (r_draw > 0) { if (x_offset_draw*x_offset_draw + y_offset_draw*y_offset_draw < (r_draw-1)*(r_draw-1)) { continue; } }
                                int px_draw = sun_center_x + x_offset_draw; int py_draw = sun_center_y + y_offset_draw;
                                if (px_draw>=0 && px_draw<PANEL_RES_X && py_draw>=0 && py_draw<PANEL_RES_Y) {
                                    uint16_t bg_pixel_val = dma_canvas.getPixel(px_draw,py_draw); 
                                    dma_canvas.drawPixel(px_draw,py_draw,blend_565(sun_color_global,bg_pixel_val,blend_alpha_val));
                                }
                            }
                        }
                    }
                }
                if (SUN_CORE_RADIUS_S4 == 0 && SUN_FUZZ_LAYERS_S4 == 0 && max_draw_radius_sun == 0) { 
                   if (sun_center_x >=0 && sun_center_x < PANEL_RES_X && sun_center_y >=0 && sun_center_y < PANEL_RES_Y)
                      dma_canvas.drawPixel(sun_center_x, sun_center_y, sun_color_global);
                }
              // --- END FUZZY SUN DRAWING ---
            } 

            bool sun_has_moved_off_screen = false;
            if (screen4_sun_obj.speed >= 0.0f) { if (screen4_sun_obj.x < -(static_cast<float>(max_draw_radius_sun) + 5.0f)) { sun_has_moved_off_screen = true; }
            } else { if (screen4_sun_obj.x > static_cast<float>(PANEL_RES_X) + static_cast<float>(max_draw_radius_sun) + 5.0f) { sun_has_moved_off_screen = true; } }
            if (sun_has_moved_off_screen) { is_screen4_sun_active = false; }
        } 
    } else { // Sun Switch is OFF
      is_screen4_sun_active = false; 
    }

    if (NUM_STARS > 0) { 
      for (int i = 0; i < NUM_STARS; i++) { 
        if (abs(stars[i].speed) > 0.001f ) { 
          stars[i].x -= stars[i].speed; 
          if (stars[i].x < -5.0f || stars[i].x > PANEL_RES_X + 5.0f) init_star(stars[i], false); 
        }
        if (stars[i].x >= 0 && stars[i].x < PANEL_RES_X) { 
          uint8_t b = stars[i].brightness;
          uint8_t r_star = (b > 20) ? (b - 20) : 0;
          uint8_t g_star = (b > 10) ? (b - 10) : 0;
          if (i % 5 == 0) dma_canvas.drawPixel(static_cast<int>(stars[i].x), static_cast<int>(stars[i].y), dma_display->color565(r_star, g_star, b));
          else dma_canvas.drawPixel(static_cast<int>(stars[i].x), static_cast<int>(stars[i].y), dma_display->color565(b, b, b));
        }
      }
    }
      
    float dayScale_val = 1.0f; 
    float nightScale_val = constrain(((float)settings.pageSlider / 255.0f - 1.0f) * -1.0f, 0.0f, 1.0f);
    float timeShadowScale_val = 0.5f;
    float midScale_val = (dayScale_val + nightScale_val) / 2.0f;
    float threshold_lower_val = -0.05f; 
    float threshold_upper_val = 0.05f;
      
    // Sphere Rendering loop
    for (int y_s = 0; y_s < PANEL_RES_Y; ++y_s) { for (int x_s = 0; x_s < PANEL_RES_X; ++x_s) {
      float dx_s = (float)x_s - EARTH_CENTER_X + 0.5f; float dy_s = (float)y_s - EARTH_CENTER_Y + 0.5f;
      if (dx_s*dx_s + dy_s*dy_s <= EARTH_RADIUS*EARTH_RADIUS) {
        float n_x_s=dx_s/EARTH_RADIUS; float n_y_s=dy_s/EARTH_RADIUS; float n_z_s_sq=1.0f-n_x_s*n_x_s-n_y_s*n_y_s; 
        if(n_z_s_sq<0.0001f)continue; float n_z_s=sqrtf(n_z_s_sq);
        float lat_r_s=asinf(constrain(-n_y_s, -1.0f, 1.0f)); float lon_r_v_s=atan2f(n_x_s,n_z_s); 
        float lat_d_s=lat_r_s*180.0f/PI; float lon_d_v_s_s=lon_r_v_s*180.0f/PI; 
        float lon_d_e_s=lon_d_v_s_s-current_rotationAngle_s4_val; 
        while(lon_d_e_s<-180.0f)lon_d_e_s+=360.0f; while(lon_d_e_s>180.0f)lon_d_e_s-=360.0f;
        int map_u_s=(int)((lon_d_e_s+180.0f)/360.0f*MAP_WIDTH); map_u_s=(map_u_s%MAP_WIDTH+MAP_WIDTH)%MAP_WIDTH; 
        int map_v_s=(int)((90.0f-lat_d_s)/180.0f*MAP_HEIGHT); map_v_s=constrain(map_v_s,0,MAP_HEIGHT-1);
        int map_px_idx_s=map_v_s*MAP_WIDTH+map_u_s;
        
        uint16_t base_px_c_s = settings.backgroundSwitch ? background_color_s4_val : 0; 
        uint16_t mask_val_s = pgm_read_word(&World_Map_Mask[map_px_idx_s]);
        if(settings.landSwitch&&(mask_val_s==0x07E0||mask_val_s==0x0000)) base_px_c_s=settings.land_use_image?pgm_read_word(&bitmaps[settings.land_bitmap_index][map_px_idx_s]):land_color_map_val;
        else if(settings.waterSwitch&&(mask_val_s==0x001F||mask_val_s==0xFFE0)) base_px_c_s=settings.water_use_image?pgm_read_word(&bitmaps[settings.water_bitmap_index][map_px_idx_s]):water_color_map_val;
        else if(settings.iceSwitch&&(mask_val_s==0xFFFF||mask_val_s==0xF800)) base_px_c_s=settings.ice_use_image?pgm_read_word(&bitmaps[settings.ice_bitmap_index][map_px_idx_s]):ice_color_map_val;
        
        float phi_r_t_s=lat_d_s*PI/180.0f; float lam_r_t_e_s=lon_d_e_s*PI/180.0f; float tht_r_t_s=lam_r_t_e_s-(lambda_sun_calc*PI/180.0f); 
        float sin_h_s=sinf(phi_r_t_s)*sinf(delta_sun_calc*PI/180.0f)+cosf(phi_r_t_s)*cosf(delta_sun_calc*PI/180.0f)*cosf(tht_r_t_s); 
        float r_o_s=((base_px_c_s>>11)&0x1F)/31.0f*255.0f; float g_o_s=((base_px_c_s>>5)&0x3F)/63.0f*255.0f; float b_o_s=(base_px_c_s&0x1F)/31.0f*255.0f;
        float cur_b_s_s = (sin_h_s < threshold_lower_val) ? nightScale_val : ((sin_h_s >= threshold_upper_val) ? dayScale_val : midScale_val);
        uint16_t final_px_c_s = dma_display->color565((uint8_t)min((int)(r_o_s*cur_b_s_s),255), (uint8_t)min((int)(g_o_s*cur_b_s_s),255), (uint8_t)min((int)(b_o_s*cur_b_s_s),255));
        
        bool is_lfnl_s=(mask_val_s==0x07E0||mask_val_s==0x0000);
        if(is_lfnl_s&&sin_h_s<threshold_lower_val&&settings.humiditySwitch){ 
            if(pgm_read_word(&Night_Lights[map_px_idx_s])==0xFFEB) final_px_c_s=night_lights_color_map_val;
        }
        dma_canvas.drawPixel(x_s,y_s,final_px_c_s);
      }
    }}


    // GPS Marker logic for 3D sphere
    if (gpsLat != 0.0f || gpsLon != 0.0f) {
        // Convert latitude to radians for trigonometric functions.
        float lr_g = gpsLat * PI / 180.0f; 

        // Calculate the "view longitude" by adding the Earth's current rotation angle.
        // This determines where the GPS point is relative to the center of the screen.
        float view_lon_deg_gps = gpsLon + current_rotationAngle_s4_val;
        
        // Normalize the longitude to the -180 to +180 degree range.
        while (view_lon_deg_gps < -180.0f) view_lon_deg_gps += 360.0f;
        while (view_lon_deg_gps > 180.0f) view_lon_deg_gps -= 360.0f;
        
        // Convert the view longitude to radians.
        float lon_r_gv = view_lon_deg_gps * PI / 180.0f; 

        // Project the spherical Lat/Lon coordinates into 3D Cartesian (X, Y, Z) space.
        // x3d_view: Left/Right position on the sphere's face.
        // y3d_view: Up/Down position on the sphere's face.
        // z3d_view: Forward/Backward depth. Positive Z is facing the viewer.
        float x3d_view = cosf(lr_g) * sinf(lon_r_gv); 
        float y3d_view = sinf(lr_g);               
        float z3d_view = cosf(lr_g) * cosf(lon_r_gv); 
        
        // Only draw the dot if it's on the visible side of the Earth (z > 0).
        if (z3d_view > 0.05f) { 
            // Convert the 3D view coordinates to 2D screen coordinates.
            float gXs_float = (float)EARTH_CENTER_X + (x3d_view * EARTH_RADIUS);
            float gYs_float = (float)EARTH_CENTER_Y - (y3d_view * EARTH_RADIUS);
            
            // Small manual offsets to fine-tune the dot's position.
            gXs_float -= 1.0f; 
            gYs_float -= 2.0f;

            // Round to the nearest integer pixel.
            int gXs = static_cast<int>(roundf(gXs_float));
            int gYs = static_cast<int>(roundf(gYs_float));
            
            // Constrain to the screen boundaries just in case.
            gXs = constrain(gXs, 0, PANEL_RES_X - 1);
            gYs = constrain(gYs, 0, PANEL_RES_Y - 1);
            
            // Final check to ensure the pixel is on the sphere's surface before drawing.
            float dx_for_check = ((float)gXs + 0.5f) - (float)EARTH_CENTER_X;
            float dy_for_check = ((float)gYs + 0.5f) - (float)EARTH_CENTER_Y;
            if (dx_for_check * dx_for_check + dy_for_check * dy_for_check <= (EARTH_RADIUS * EARTH_RADIUS) + 2.0f) {
                // Draw the GPS marker as a single red pixel.
                dma_canvas.drawPixel(gXs, gYs, dma_display->color565(255, 0, 0));
            }
        }
    }

    

    // Time Display
    if (yearAnimationActive) {
        drawYearAnimationDate(); 
    } else {
        String timeString = td_for_display.hoursMins; 
        dma_canvas.setFont(&TomThumb); 
        int startX = 1; int yPos = PANEL_RES_Y - 1;    
        if(settings.monthSwitch){ 
            uint16_t outlineColor = dma_display->color565(
                static_cast<uint8_t>(settings.month_col.r * timeShadowScale_val), 
                static_cast<uint8_t>(settings.month_col.g * timeShadowScale_val), 
                static_cast<uint8_t>(settings.month_col.b * timeShadowScale_val)
            );
            int offsets[8][2] = {{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
            for(int i_offset = 0; i_offset < 8; i_offset++){
                dma_canvas.setTextColor(outlineColor); 
                dma_canvas.setCursor(startX + offsets[i_offset][0], yPos + offsets[i_offset][1]); 
                dma_canvas.print(timeString);
            }
        } 
        dma_canvas.setCursor(startX, yPos); 
        dma_canvas.setTextColor(time_color_val); 
        dma_canvas.print(timeString);
    }

 
  } else {  // 2D Version of the clock
    TimeDateComponents td = getTimeDateString();
    uint16_t time_color = dma_display->color565(settings.time_col.r, settings.time_col.g, settings.time_col.b);
    uint16_t background_color = dma_display->color565(settings.day_col.r, settings.day_col.g, settings.day_col.b);
    uint16_t land_color = dma_display->color565(settings.land_col.r, settings.land_col.g, settings.land_col.b);
    uint16_t water_color = dma_display->color565(settings.water_col.r, settings.water_col.g, settings.water_col.b);
    uint16_t ice_color = dma_display->color565(settings.ice_col.r, settings.ice_col.g, settings.ice_col.b);
    uint16_t humidity_color = dma_display->color565(settings.humidity_col.r, settings.humidity_col.g, settings.humidity_col.b); 
    uint16_t month_color_shadow = dma_display->color565(settings.month_col.r, settings.month_col.g, settings.month_col.b); 
    
    // (Solar position math is unchanged)
    time_t utcTime = mktime(&timeinfo) - (timezoneOffset * 3600);
    float lambda_sun, delta;
    solarPosition(utcTime, lambda_sun, delta);
    lambda_sun += (terminatorOffset * 360.0f) / 64.0f;
    while (lambda_sun < -180.0) lambda_sun += 360.0;
    while (lambda_sun > 180.0) lambda_sun -= 360.0;
    
    dma_canvas.fillScreen(0);
    if (settings.backgroundSwitch) {
        dma_canvas.fillRect(0, 0, 64, 32, background_color);
    }
    if (settings.landSwitch) {
        for (int i = 0; i < 2048; i++) {
            uint16_t maskColor = pgm_read_word(&World_Map_Mask[i]);
            if (maskColor != 0x07E0 && maskColor != 0x0000) continue;
            int x = i % 64; int y = i / 64; 
            uint16_t pixelColor = settings.land_use_image ? pgm_read_word(&bitmaps[settings.land_bitmap_index][i]) : land_color;
            dma_canvas.drawPixel(x, y, pixelColor);
        }
    }
    if (settings.waterSwitch) {
        for (int i = 0; i < 2048; i++) {
            uint16_t maskColor = pgm_read_word(&World_Map_Mask[i]);
            if (maskColor != 0x001F && maskColor != 0xFFE0) continue;
            int x = i % 64; int y = i / 64;
            uint16_t pixelColor = settings.water_use_image ? pgm_read_word(&bitmaps[settings.water_bitmap_index][i]) : water_color;
            dma_canvas.drawPixel(x, y, pixelColor);
        }
    }
    if (settings.iceSwitch) {
        for (int i = 0; i < 2048; i++) {
            uint16_t maskColor = pgm_read_word(&World_Map_Mask[i]);
            if (maskColor != 0xFFFF && maskColor != 0xF800) continue;
            int x = i % 64; int y = i / 64;
            uint16_t pixelColor = settings.ice_use_image ? pgm_read_word(&bitmaps[settings.ice_bitmap_index][i]) : ice_color;
            dma_canvas.drawPixel(x, y, pixelColor);
        }
    }
    
    float nightScale = ((float)settings.pageSlider / 255.0f - 1.0f) * -1.0f; 
    nightScale = constrain(nightScale, 0.0f, 1.0f);
    float dayScale = 1.0; float timeShadowScale = 0.5; float midScale = (dayScale + nightScale) / 2.0;
    float threshold_lower = -0.03; float threshold_upper = 0.03;
    
    for (int y_coord = 0; y_coord < 32; y_coord++) { for (int x_coord = 0; x_coord < 64; x_coord++) {
        float lambda_pix = (x_coord / 64.0) * 360.0 - 180.0; float phi_pix = 90.0 - (y_coord / 32.0) * 180.0;    
        float phi_rad = phi_pix * PI / 180.0; float lambda_rad = lambda_pix * PI / 180.0;
        float theta_rad = lambda_rad - (lambda_sun * PI / 180.0);
        float sin_h = sin(phi_rad) * sin(delta * PI / 180.0) + cos(phi_rad) * cos(delta * PI / 180.0) * cos(theta_rad);
        uint16_t pixel_on_canvas = dma_canvas.getPixel(x_coord, y_coord);
        float r_orig = ((pixel_on_canvas >> 11) & 0x1F) / 31.0f * 255.0f;
        float g_orig = ((pixel_on_canvas >> 5) & 0x3F) / 63.0f * 255.0f;
        float b_orig = (pixel_on_canvas & 0x1F) / 31.0f * 255.0f;
        float brightnessScale = (sin_h < threshold_lower) ? nightScale : ((sin_h >= threshold_upper) ? dayScale : midScale);
        uint16_t final_pixel_color = dma_display->color565(
            (uint8_t)min(r_orig * brightnessScale, 255.0f), 
            (uint8_t)min(g_orig * brightnessScale, 255.0f), 
            (uint8_t)min(b_orig * brightnessScale, 255.0f));
        
        int pixel_index = y_coord * 64 + x_coord;
        uint16_t mask_pixel_color = pgm_read_word(&World_Map_Mask[pixel_index]); 
        bool isLand = (mask_pixel_color == 0x07E0 || mask_pixel_color == 0x0000);
        if (isLand && sin_h < threshold_lower) {
            if (pgm_read_word(&Night_Lights[pixel_index]) == 0xFFEB) { 
                if (settings.humiditySwitch) { final_pixel_color = humidity_color; }
            }
        }
        dma_canvas.drawPixel(x_coord, y_coord, final_pixel_color);
    }}
        // GPS Marker logic for 2D flat map
    if (gpsLat != 0.0 || gpsLon != 0.0) {
        int gpsX = (int)(((gpsLon + 180.0) / 360.0) * 64);
        int gpsY = (int)((90.0 - gpsLat) / 180.0 * 32);
        gpsX = constrain(gpsX, 0, 63); gpsY = constrain(gpsY, 0, 31);
        dma_canvas.drawPixel(gpsX, gpsY, dma_display->color565(255, 0, 0));
    }
    
    if (yearAnimationActive && animationDateStr != "") {
        drawYearAnimationDate();
    }
    
    if (!yearAnimationActive){
        String timeString = td.hoursMins;
        dma_canvas.setFont(&TomThumb);
        int startX = 1; int yPos = PANEL_RES_Y -1; 
        if(settings.monthSwitch){ 
            uint16_t outlineColor = dma_display->color565(
            (uint8_t)(settings.month_col.r * timeShadowScale),
            (uint8_t)(settings.month_col.g * timeShadowScale),
            (uint8_t)(settings.month_col.b * timeShadowScale) );
            int offsets[8][2] = {{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
            for (int i = 0; i < 8; i++) {
                dma_canvas.setTextColor(outlineColor);
                dma_canvas.setCursor(startX + offsets[i][0], yPos + offsets[i][1]);
                dma_canvas.print(timeString);
            }
        }
        dma_canvas.setCursor(startX, yPos);
        dma_canvas.setTextColor(time_color);
        dma_canvas.print(timeString);
    }
  } 
}


void Screen5() {    // Moon Phase
  // Get the settings for the current screen.
  int screenIndex = currentScreen - 1;
  const auto& settings = allScreenSettings[screenIndex];

  dma_canvas.fillScreen(0);  

  // Get time from system (either NTP or RTC)
  struct tm localTimeinfo;
  if (!getLocalTime(&localTimeinfo)) {
      DateTime rtcTime = rtc.now();
      localTimeinfo.tm_year = rtcTime.year() - 1900;
      localTimeinfo.tm_mon = rtcTime.month() - 1;
      localTimeinfo.tm_mday = rtcTime.day();
      localTimeinfo.tm_hour = rtcTime.hour();
      localTimeinfo.tm_min = rtcTime.minute();
      localTimeinfo.tm_sec = rtcTime.second();
      localTimeinfo.tm_isdst = -1;
  }
  timeinfo = localTimeinfo;

  // Use the settings object to define colors for this screen.
  uint16_t time_color = dma_display->color565(settings.time_col.r, settings.time_col.g, settings.time_col.b);
  uint16_t ampm_color = dma_display->color565(settings.ampm_col.r, settings.ampm_col.g, settings.ampm_col.b);
  uint16_t seconds_color = dma_display->color565(settings.seconds_col.r, settings.seconds_col.g, settings.seconds_col.b);
  uint16_t day_color = dma_display->color565(settings.day_col.r, settings.day_col.g, settings.day_col.b);
  uint16_t date_color = dma_display->color565(settings.date_col.r, settings.date_col.g, settings.date_col.b);
  uint16_t month_color = dma_display->color565(settings.month_col.r, settings.month_col.g, settings.month_col.b);
  uint16_t date_bg_color = dma_display->color565(settings.dateBG_col.r, settings.dateBG_col.g, settings.dateBG_col.b); // Moon % colour
  uint16_t temp_col = dma_display->color565(settings.temp_col.r, settings.temp_col.g, settings.temp_col.b);
  uint16_t humidity_col = dma_display->color565(settings.humidity_col.r, settings.humidity_col.g, settings.humidity_col.b);
  uint16_t cfg_indicatorSymbolColor = temp_col;

  // --- MODIFIED: Temperature Preparation Logic ---
  String unitSuffix = (tempUnits == "fahrenheit") ? "f" : "c";
  bool isFahrenheit = (tempUnits == "fahrenheit");
  
  // Apply the stored offset to the raw internal temperature
  float effectiveInternalTemp = internalTemp + indoorTempOffset;

  float internalTempToDisplay = effectiveInternalTemp;
  if (isFahrenheit && effectiveInternalTemp > -99.0) {
      internalTempToDisplay = (effectiveInternalTemp * 9.0 / 5.0) + 32.0;
  }
  float webTempInCelsius = (tempType == "feels_like") ? currentApparentTemp.toFloat() : currentTemp.toFloat();
  float webTempToDisplay = webTempInCelsius;
  if (isFahrenheit) {
      webTempToDisplay = (webTempInCelsius * 9.0 / 5.0) + 32.0;
  }
  
  static int temperatureAnimationTextWidthCache = 0; 
  static int humidityAnimationTextWidthCache = 0; 
  int cfg_tempX = 0; int cfg_tempY = 4;
  int cfg_humidX = 0; int cfg_humidY = 31;
  int indicatorSymbolX = 28; int indicatorSymbolY = 4;
  
  int16_t x1, y1;
  uint16_t w_time, h_time, w_datum;

  // --- Moon Phase Logic ---
  float currentMoonPhase;
  float currentMoonPercentage;
  if (selectedWeatherService != "none" && currentState == STATE_RUNNING && !yearAnimationActive && !ntpPaused) {
      currentMoonPhase = moonPhase;
      currentMoonPercentage = moonPercentage;
  } else {
      currentMoonPhase = useCalculatedMoonPhase ? calculatedMoonPhase : calculateMoonPhase(timeinfo);
      if (selectedWeatherService == "none" && !yearAnimationActive && !ntpPaused) {
          currentMoonPhase = moonPhase;
      }
      float illumFraction = 1.0f - fabs(2.0f * currentMoonPhase - 1.0f);
      currentMoonPercentage = roundf(illumFraction * 100.0f * 10.0f) / 10.0f;
  }
  currentMoonPhase = clamp(currentMoonPhase, 0.0f, 1.0f);  
  float lambda_sun_rad = PI * (1.0f - 2.0f * currentMoonPhase);  

  // --- Moon Drawing Logic ---
  if (settings.iconsSwitch) {
    const int MOON_W = 31; const int MOON_H = 31; const int MOON_X = 0; const int MOON_Y = 0;
    if (gpsLat < 0) dma_canvas.drawRGBBitmap(MOON_X, MOON_Y, (const uint16_t *)Moon_Phase, MOON_W, MOON_H);
    else dma_canvas.drawRGBBitmap(MOON_X, MOON_Y, (const uint16_t *)Moon_Phase_180, MOON_W, MOON_H);
    
    // Get night shadow intensity from the settings for this screen.
    float nightScale = ((float)settings.pageSlider / 255.0f - 1.0f) * -1.0f;
    nightScale = clamp(nightScale, 0.0f, 1.0f);
    
    const float moonRadius = 16.0f;
    const float moonCenterX_canvas = (float)MOON_X + (float)MOON_W / 2.0f;
    const float moonCenterY_canvas = (float)MOON_Y + (float)MOON_H / 2.0f;
    const float terminatorBlendWidth = 0.12f;
    const float blendStart = -terminatorBlendWidth / 2.0f;
    const float blendEnd = terminatorBlendWidth / 2.0f;
    for (int cy = MOON_Y; cy < MOON_Y + MOON_H; ++cy) {
      for (int cx = MOON_X; cx < MOON_X + MOON_W; ++cx) {
        float dx = (float)cx - moonCenterX_canvas + 0.5f; float dy = (float)cy - moonCenterY_canvas + 0.5f;
        float distSq = dx * dx + dy * dy;
        uint16_t originalPixel = dma_canvas.getPixel(cx, cy);
        if (distSq > moonRadius * moonRadius || originalPixel == 0) continue;
        float phi_rad = asinf(clamp(dy / moonRadius, -1.0f, 1.0f));
        float z_sq = moonRadius * moonRadius - distSq; if (z_sq < 0) z_sq = 0; float z = sqrtf(z_sq);
        float lambda_rad = atan2f(dx, z);
        float cos_incidence = cosf(phi_rad) * cosf(lambda_rad - lambda_sun_rad);
        float brightnessScale = 1.0f;
        if (cos_incidence <= blendStart) brightnessScale = nightScale;
        else if (cos_incidence > blendStart && cos_incidence < blendEnd) {
          float t = (cos_incidence - blendStart) / terminatorBlendWidth;
          brightnessScale = lerp(nightScale, 1.0f, t);
        }
        float r_orig = ((originalPixel >> 11) & 0x1F) / 31.0f * 255.0f;
        float g_orig = ((originalPixel >> 5) & 0x3F) / 63.0f * 255.0f;
        float b_orig = (originalPixel & 0x1F) / 31.0f * 255.0f;
        uint8_t r = (uint8_t)clamp(r_orig * brightnessScale, 0.0f, 255.0f);
        uint8_t g = (uint8_t)clamp(g_orig * brightnessScale, 0.0f, 255.0f);
        uint8_t b = (uint8_t)clamp(b_orig * brightnessScale, 0.0f, 255.0f);
        dma_canvas.drawPixel(cx, cy, dma_display->color565(r, g, b));
      }
    }
  } 

  // --- Time & Date Logic ---
  char timeBuf[32]; char ampmBuf[2] = "";
  if (settings.twentyFourHourSwitch) strftime(timeBuf, sizeof(timeBuf), "%H:%M", &timeinfo);
  else {
    strftime(timeBuf, sizeof(timeBuf), "%l:%M", &timeinfo);
    if (settings.ampmSwitch && timeinfo.tm_hour >= 12) strcpy(ampmBuf, "'");
  }
  dma_canvas.setFont(&Tidbyt_Numbers1);
  dma_canvas.getTextBounds(String(timeBuf), 0, 0, &x1, &y1, &w_time, &h_time);
  int xPos = (PANEL_RES_X / 4) * 3 - (w_time / 2); int yPos = 9;
  dma_canvas.setTextColor(time_color); dma_canvas.setCursor(xPos, yPos - timeFontOffset); dma_canvas.print(timeBuf);
  if (!settings.twentyFourHourSwitch && settings.ampmSwitch && strlen(ampmBuf) > 0) {
    dma_canvas.setTextColor(ampm_color); dma_canvas.setCursor(xPos + w_time + 1, yPos + 1); dma_canvas.print(ampmBuf);
  }
  if (settings.secondsSwitch) {
    float secondsFraction = (float)timeinfo.tm_sec / 60.0;
    dma_canvas.drawLine(xPos, yPos + 2, xPos + (int)(secondsFraction * w_time), yPos + 2, seconds_color);
  }
  if (settings.dateSwitch || settings.monthSwitch) {
    String dateString = "";
    char monthBuf[16] = ""; // Increased buffer
    char dateBuf[32] = "";
    if (settings.dateSwitch) {
      dateString += String(timeinfo.tm_mday);
      strftime(dateBuf, sizeof(dateBuf), "%d/%m/%y", &timeinfo);
      dma_canvas.setFont(&TomThumb);
      dma_canvas.getTextBounds(String(dateBuf), 0, 0, &x1, &y1, &w_datum, &h_time);
    }
    if (settings.monthSwitch) {
      if (currentLanguage == "de") {
        strncpy(monthBuf, months_DE_short[timeinfo.tm_mon], sizeof(monthBuf));
      } else if (currentLanguage == "sv") { // ADDED SWEDISH
        strncpy(monthBuf, months_SE_short[timeinfo.tm_mon], sizeof(monthBuf));
      } else {
        strftime(monthBuf, sizeof(monthBuf), "%b", &timeinfo);
      }
      dateString += (settings.dateSwitch ? " " : "") + String(monthBuf);
    }
    dma_canvas.setFont(&Font_5x7_practical8pt7b);
    dma_canvas.getTextBounds(dateString, 0, 0, &x1, &y1, &w_time, &h_time);
    int dateXPos = 64 - w_time; int dateYPos = 20;
    if (settings.dateSwitch && settings.monthSwitch) {
      dma_canvas.setTextColor(date_color);
      dma_canvas.setCursor(dateXPos + 2, dateYPos);
      dma_canvas.print(String(timeinfo.tm_mday));
      dma_canvas.getTextBounds(String(timeinfo.tm_mday), 0, 0, &x1, &y1, &w_time, &h_time);
      dma_canvas.setTextColor(month_color);
      dma_canvas.setCursor(dateXPos + w_time + 4, dateYPos);
      dma_canvas.print(monthBuf);
    } else if (settings.monthSwitch) {
      dma_canvas.setTextColor(month_color);
      dma_canvas.setCursor(dateXPos -1, dateYPos);
      dma_canvas.print(monthBuf);
    } else if (settings.dateSwitch) {
      dma_canvas.setFont(&TomThumb);
      dma_canvas.setCursor(64 - w_datum -1 , dateYPos);
      dma_canvas.setTextColor(date_color);
      dma_canvas.print(dateBuf);
    }
  }
  if (settings.daySwitch) {
    char dayBuf[16]; // Increased buffer
    if (currentLanguage == "de") {
      strncpy(dayBuf, days_DE_short[timeinfo.tm_wday], sizeof(dayBuf));
    } else if (currentLanguage == "sv") { // ADDED SWEDISH
      strncpy(dayBuf, days_SE_short[timeinfo.tm_wday], sizeof(dayBuf));
    } else {
      strftime(dayBuf, sizeof(dayBuf), "%a", &timeinfo);
    }
    dma_canvas.setFont(&Font_5x7_practical8pt7b);
    dma_canvas.getTextBounds(dayBuf, 0, 0, &x1, &y1, &w_time, &h_time);
    dma_canvas.setTextColor(day_color);
    dma_canvas.setCursor(64 - w_time - 1, 30); dma_canvas.print(dayBuf);
  }
 
  // --- SENSOR & MOON % DISPLAY ---
  dma_canvas.setFont(&Tiny_Phil);
  if (selectedWeatherService != "none") {
      unsigned long current_millis_for_anim = millis();
      renderAnimatedValue_Left_Right(dma_canvas, current_millis_for_anim,
          settings.temperatureSwitch, internalTempToDisplay, "", unitSuffix, webTempToDisplay, "", unitSuffix,
          cfg_tempX + 1, cfg_tempY + 1, cc_blk, temperatureAnimationTextWidthCache, 1);
      renderAnimatedValue_Left_Right(dma_canvas, current_millis_for_anim,
          settings.temperatureSwitch, internalTempToDisplay, "", unitSuffix, webTempToDisplay, "", unitSuffix,
          cfg_tempX, cfg_tempY, temp_col, temperatureAnimationTextWidthCache, 1);
      renderAnimatedValue_Left_Right(dma_canvas, current_millis_for_anim,
          settings.humiditySwitch, internalHumid, "", "%", currentHumidity.toFloat(), "", "%",
          cfg_humidX + 1, cfg_humidY + 1, cc_blk, humidityAnimationTextWidthCache, 0);
      renderAnimatedValue_Left_Right(dma_canvas, current_millis_for_anim,
          settings.humiditySwitch, internalHumid, "", "%", currentHumidity.toFloat(), "", "%",
          cfg_humidX, cfg_humidY, humidity_col, humidityAnimationTextWidthCache, 0);
      String indicatorSymbolToShow;
      int currentCycleTimeForIndicator = (current_millis_for_anim / 1000) % 20;
      if (currentCycleTimeForIndicator < 10) { indicatorSymbolToShow = "#"; } else { indicatorSymbolToShow = "$"; } 
      dma_canvas.setTextColor(cc_blk); dma_canvas.setCursor(indicatorSymbolX + 1, indicatorSymbolY + 1); dma_canvas.print(indicatorSymbolToShow);
      dma_canvas.setTextColor(cfg_indicatorSymbolColor); dma_canvas.setCursor(indicatorSymbolX, indicatorSymbolY); dma_canvas.print(indicatorSymbolToShow);
  } else {
    if (settings.temperatureSwitch) {
        String tempStr = String(internalTempToDisplay, 1) + unitSuffix;
        dma_canvas.setTextColor(cc_blk); dma_canvas.setCursor(cfg_tempX + 1, cfg_tempY + 1); dma_canvas.print(tempStr);
        dma_canvas.setTextColor(temp_col); dma_canvas.setCursor(cfg_tempX, cfg_tempY); dma_canvas.print(tempStr);
    }
    if (settings.humiditySwitch) {
        String humidStr = String((int)internalHumid) + "%";
        dma_canvas.setTextColor(cc_blk); dma_canvas.setCursor(cfg_humidX + 1, cfg_humidY + 1); dma_canvas.print(humidStr);
        dma_canvas.setTextColor(humidity_col); dma_canvas.setCursor(cfg_humidX, cfg_humidY); dma_canvas.print(humidStr);
    }
    dma_canvas.setTextColor(cc_blk); dma_canvas.setCursor(indicatorSymbolX + 1, indicatorSymbolY + 1); dma_canvas.print("#");
    dma_canvas.setTextColor(cfg_indicatorSymbolColor); dma_canvas.setCursor(indicatorSymbolX, indicatorSymbolY); dma_canvas.print("#");
  }
  
  // Moon Percentage Display
  if (settings.minMaxTempsSwitch && selectedWeatherService != "none") {
    dma_canvas.setFont(&Tiny_Phil);
    char percentBuf[8]; 
    if (currentMoonPhase >= 0.5) {
        snprintf(percentBuf, sizeof(percentBuf), "(%d%%", (int)round(currentMoonPercentage));
    } else {
        snprintf(percentBuf, sizeof(percentBuf), ")%d%%", (int)round(currentMoonPercentage));
    }
    dma_canvas.getTextBounds(percentBuf, 0, 0, &x1, &y1, &w_time, &h_time); 
    dma_canvas.setTextColor(cc_blk); dma_canvas.setCursor(25 + 1, 31 + 1); dma_canvas.print(percentBuf);
    dma_canvas.setTextColor(date_bg_color); dma_canvas.setCursor(25, 31); dma_canvas.print(percentBuf);
  }
}

  // Helper function for screen6 to convert HSV (Hue, Saturation, Value) to RGB565 color
  // Helper function to convert HSV (Hue, Saturation, Value) to a 16-bit RGB565 color
uint16_t hsvToRgb565(float h, float s, float v) {
  h = fmod(h, 360.0);
  if (h < 0) h += 360.0;
  
  s = constrain(s, 0.0, 1.0);
  v = constrain(v, 0.0, 1.0);

  int i = floor(h / 60.0);
  float f = h / 60.0 - i;
  float p = v * (1 - s);
  float q = v * (1 - f * s);
  float t = v * (1 - (1 - f) * s);

  float r = 0, g = 0, b = 0;
  switch (i % 6) {
    case 0: r = v, g = t, b = p; break;
    case 1: r = q, g = v, b = p; break;
    case 2: r = p, g = v, b = t; break;
    case 3: r = p, g = q, b = v; break;
    case 4: r = t, g = p, b = v; break;
    case 5: r = v, g = p, b = q; break;
  }

  uint8_t r8 = r * 255;
  uint8_t g8 = g * 255;
  uint8_t b8 = b * 255;

  return ((r8 & 0xF8) << 8) | ((g8 & 0xFC) << 3) | (b8 >> 3);
}

void Screen6() { // Gradient Clock
  // --- 1. SETUP AND DATA PREPARATION ---
  int screenIndex = currentScreen - 1;
  const auto& settings = allScreenSettings[screenIndex];
  getLocalTime(&timeinfo);

  uint16_t time_color = dma_display->color565(settings.time_col.r, settings.time_col.g, settings.time_col.b);
  uint16_t ampm_color = dma_display->color565(settings.ampm_col.r, settings.ampm_col.g, settings.ampm_col.b);
  uint16_t shadow_color = dma_display->color565(settings.dateBG_col.r, settings.dateBG_col.g, settings.dateBG_col.b);

  // --- 2. CALCULATE SLIDER-BASED VALUES (WITH FIXES) ---
  // Gradient Speed: 0% is now fully stopped.
  float speed_divisor;
  if (settings.pageSlider == 0) {
    speed_divisor = 0; // Special case for "stopped"
  } else {
    speed_divisor = map_float(settings.pageSlider, 1, 255, 50.0f, 10.0f);
  }

  // Box Size: Map 0-255 to half the width/height.
  // At 100% (255), it will be slightly LARGER than the screen to ensure full coverage.
  float box_half_width = map_float(settings.pageSlider2, 0, 255, 0.0f, (PANEL_RES_X / 2.0f) + 1.0f);
  float box_half_height = map_float(settings.pageSlider2, 0, 255, 0.0f, (PANEL_RES_Y / 2.0f) + 1.0f);

  // Box Feathering: Map 0-255 to a pixel transition width.
  float feather_width = map_float(settings.pageSlider3, 0, 255, 0.0f, 10.0f);

  // --- 3. DRAW GRADIENT AND FEATHERED BOX (WITH INVERSE LOGIC) ---
  float center_x = PANEL_RES_X / 2.0f;
  float center_y = PANEL_RES_Y / 2.0f;

  for (int y = 0; y < PANEL_RES_Y; y++) {
    for (int x = 0; x < PANEL_RES_X; x++) {
      // Calculate the gradient color for this pixel
      float hue_offset = (speed_divisor > 0) ? (millis() / speed_divisor) : 0;
      float hue = fmod((x * 2.5 + y * 5 + hue_offset), 360);
      uint16_t gradient_color = hsvToRgb565(hue, 1.0f, 0.5f);
      
      uint16_t final_color = gradient_color; // Default to the gradient color

      if (box_half_width > 0 && box_half_height > 0) {
        // Calculate the absolute distance from the center for this pixel
        float dist_from_center_x = abs(x - center_x + 0.5f);
        float dist_from_center_y = abs(y - center_y + 0.5f);

        // Check if the pixel is inside the outer edge of the feather zone
        if (dist_from_center_x < box_half_width + feather_width &&
            dist_from_center_y < box_half_height + feather_width) {
              
            // Determine how far the pixel is from the SOLID inner edge of the box
            float feather_dist_x = dist_from_center_x - box_half_width;
            float feather_dist_y = dist_from_center_y - box_half_height;
            
            if (feather_dist_x < 0 && feather_dist_y < 0) {
              // Pixel is INSIDE the solid black box area
              final_color = cc_blk;
            } else if (feather_width > 0.1f) {
              // Pixel is in the FEATHER zone, find the strongest feathering effect
              float max_feather_dist = max(feather_dist_x, feather_dist_y);
              float blend_factor = clamp(max_feather_dist / feather_width, 0.0f, 1.0f);
              // Blend from black (0.0) to gradient (1.0)
              final_color = blend_565(gradient_color, cc_blk, blend_factor);
            }
        }
      }
      dma_canvas.drawPixel(x, y, final_color);
    }
  }

  // --- 4. THE TIME DISPLAY (No changes) ---
  char timeBuf[16];
  if (settings.twentyFourHourSwitch) {
    strftime(timeBuf, sizeof(timeBuf), "%H:%M", &timeinfo);
  } else {
    strftime(timeBuf, sizeof(timeBuf), "%l:%M", &timeinfo);
  }
  String timeStr = String(timeBuf);
  timeStr.trim();

  dma_canvas.setFont(&Pixel_Bold10pt7b);
  int16_t x1, y1;
  uint16_t w, h;
  dma_canvas.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);

  int centerX = (PANEL_RES_X - w) / 2;
  int centerY = (PANEL_RES_Y / 2) + (h / 2) - 2;

  dma_canvas.setTextColor(shadow_color);
  dma_canvas.setCursor(centerX + 2, centerY + 2);
  dma_canvas.print(timeStr);
  
  dma_canvas.setTextColor(time_color);
  dma_canvas.setCursor(centerX, centerY);
  dma_canvas.print(timeStr);

  // --- 5. AM/PM INDICATOR (No changes) ---
  if (!settings.twentyFourHourSwitch && settings.ampmSwitch) {
    char ampmBuf[4];
    strftime(ampmBuf, sizeof(ampmBuf), "%p", &timeinfo);
    dma_canvas.setFont(&TomThumb);
    
    int16_t ampm_x1, ampm_y1;
    uint16_t ampm_w, ampm_h;
    dma_canvas.getTextBounds(ampmBuf, 0, 0, &ampm_x1, &ampm_y1, &ampm_w, &ampm_h);

    int ampmX = centerX + w + 4;
    int ampmY = centerY;

    dma_canvas.setTextColor(shadow_color);
    dma_canvas.setCursor(ampmX + 1, ampmY + 1);
    dma_canvas.print(ampmBuf);
    
    dma_canvas.setTextColor(ampm_color);
    dma_canvas.setCursor(ampmX, ampmY);
    dma_canvas.print(ampmBuf);
  }
}

// NEW Helper function for Screen 7 to draw a STRING with a "glow" effect.
void drawGlowString(String text, int x, int y, uint16_t core_color, uint16_t glow_color, float glow_intensity) {
  // The glow is drawn first, behind the main text.
  if (glow_intensity > 0.05) {
    uint16_t final_glow_color = blend_565(cc_blk, glow_color, glow_intensity);
    dma_canvas.setTextColor(final_glow_color);
    // Draw glow text at slightly offset positions.
    dma_canvas.setCursor(x + 1, y); dma_canvas.print(text);
    dma_canvas.setCursor(x - 1, y); dma_canvas.print(text);
    dma_canvas.setCursor(x, y + 1); dma_canvas.print(text);
    dma_canvas.setCursor(x, y - 1); dma_canvas.print(text);
  }

  // Draw the bright, core text on top.
  dma_canvas.setTextColor(core_color);
  dma_canvas.setCursor(x, y);
  dma_canvas.print(text);
}


void Screen7() { // Digital watch
  // --- 1. SETUP ---
  int screenIndex = currentScreen - 1;
  const auto& settings = allScreenSettings[screenIndex];
  getLocalTime(&timeinfo);

  // --- 2. MAP SETTINGS TO VISUALS ---
  uint16_t digit_core_color = dma_display->color565(settings.time_col.r, settings.time_col.g, settings.time_col.b);
  uint16_t digit_glow_color = dma_display->color565(settings.ampm_col.r, settings.ampm_col.g, settings.ampm_col.b);
  uint16_t date_color = dma_display->color565(settings.date_col.r, settings.date_col.g, settings.date_col.b);
  uint16_t month_color = dma_display->color565(settings.month_col.r, settings.month_col.g, settings.month_col.b);
  uint16_t background_color = dma_display->color565(settings.dateBG_col.r, settings.dateBG_col.g, settings.dateBG_col.b);
  float glow_intensity = map_float(settings.pageSlider, 0, 255, 0.7f, 0.0f);

  // --- 3. DRAW BACKGROUND ---
  if (settings.backgroundSwitch) {
      dma_canvas.fillScreen(background_color);
  } else {
      dma_canvas.fillScreen(cc_blk);
  }

  // --- 4. PREPARE AND DRAW TIME DIGITS ---
  char hourBuf[4];
  char minBuf[4];
  
  dma_canvas.setFont(&DS_DIGI12pt7b);
  int time_y = 17;

  if (settings.twentyFourHourSwitch) {
    strftime(hourBuf, sizeof(hourBuf), "%H", &timeinfo);
  } else {
    strftime(hourBuf, sizeof(hourBuf), "%l", &timeinfo);
    String(hourBuf).trim();
  }
  strftime(minBuf, sizeof(minBuf), "%M", &timeinfo);

  const int hour_right_edge = 21;
  const int min_left_edge = 32;

  int16_t x1, y1;
  uint16_t w, h;
  dma_canvas.getTextBounds(hourBuf, 0, 0, &x1, &y1, &w, &h);
  int hour_x = hour_right_edge - w;

  drawGlowString(String(hourBuf), hour_x, time_y, digit_core_color, digit_glow_color, glow_intensity);
  drawGlowString(String(minBuf), min_left_edge, time_y, digit_core_color, digit_glow_color, glow_intensity);

  // --- 5. DRAW BLINKING COLON WITH CONSISTENT GLOW ---
  if (timeinfo.tm_sec % 2 == 0) {
      int colon_x = 25; // Manually tuned X position for the colon
      drawGlowString(":", colon_x, time_y, digit_core_color, digit_glow_color, glow_intensity);
  }

  // --- 6. DRAW DATE / MONTH ---
  String date_num_str = "";
  String month_str = "";
  int total_width = 0;
  int space_width = 0;
  dma_canvas.setFont(&Font_5x7_practical8pt7b);
  if (settings.dateSwitch) {
    char date_buf[4]; strftime(date_buf, sizeof(date_buf), "%d", &timeinfo);
    date_num_str = String(date_buf);
    dma_canvas.getTextBounds(date_num_str, 0, 0, &x1, &y1, &w, &h);
    total_width += w;
  }
  if (settings.monthSwitch) {
    char month_buf[16]; // Increased buffer size
    // *** MODIFICATION IS HERE ***
    if (currentLanguage == "de") {
        strncpy(month_buf, months_DE_short[timeinfo.tm_mon], sizeof(month_buf));
    } else {
        strftime(month_buf, sizeof(month_buf), "%b", &timeinfo);
    }
    month_str = String(month_buf);
    dma_canvas.getTextBounds(month_str, 0, 0, &x1, &y1, &w, &h);
    total_width += w;
  }
  if (settings.dateSwitch && settings.monthSwitch) {
    space_width = 3; total_width += space_width;
  }
  if (total_width > 0) {
    int current_x = (PANEL_RES_X - total_width) / 2;
    int date_y = 28;
    if (settings.dateSwitch) {
      drawGlowString(date_num_str, current_x, date_y, date_color, digit_glow_color, glow_intensity);
      dma_canvas.getTextBounds(date_num_str, 0, 0, &x1, &y1, &w, &h);
      current_x += w + space_width;
    }
    if (settings.monthSwitch) {
      drawGlowString(month_str, current_x, date_y, month_color, digit_glow_color, glow_intensity);
    }
  }

  // --- 7. DRAW PM INDICATOR APOSTROPHE ---
  uint16_t apostrophe_color = dma_display->color565(settings.seconds_col.r, settings.seconds_col.g, settings.seconds_col.b);
  if (!settings.twentyFourHourSwitch && settings.ampmSwitch && timeinfo.tm_hour >= 12) {
    int apostrophe_x = 57;
    int apostrophe_y = 17;
    dma_canvas.setFont(&DS_DIGI12pt7b);
    drawGlowString("'", apostrophe_x, apostrophe_y, apostrophe_color, digit_glow_color, glow_intensity);
  }
}

// Helper function for Screen 8 to draw the hour markers (numbers or stars).
void drawHourMarker(int hour, const String& mode, uint16_t num_color, uint16_t star_color) {
    
    // Using mathematically calculated coordinates for the stars to ensure they are perfectly symmetrical.
    const float center_x = 31.5f;
    const float center_y = 15.5f;
    const float radius_x = 29.0f;
    const float radius_y = 14.5f;
    float angle = (hour / 12.0f) * 360.0f - 90.0f;
    float angle_rad = angle * PI / 180.0f;
    int marker_x = round(center_x + radius_x * cos(angle_rad));
    int marker_y = round(center_y + radius_y * sin(angle_rad));

    // Your manually adjusted coordinates for numbers.
    const int num_x[] = {61, 30, 0, 29}; // Positions for 3, 6, 9, 12
    const int num_y[] = {19, 32, 19, 5}; // Baseline Y-positions for the font

    bool is_cardinal = (hour == 3 || hour == 6 || hour == 9 || hour == 12);
    
    bool should_draw_number = (mode == "All Numbers") || (is_cardinal && (mode == "4 Numbers" || mode == "4 Numbers + Stars"));
    bool should_draw_star = (mode == "All Stars") || (is_cardinal && mode == "4 Stars") || (!is_cardinal && mode == "4 Numbers + Stars");

    if (should_draw_number) {
        dma_canvas.setFont(&TomThumb);
        dma_canvas.setTextColor(num_color);
        int index = 0;
        if(is_cardinal) {
            index = (hour / 3) - 1;
            if (hour == 12) index = 3;
            dma_canvas.setCursor(num_x[index], num_y[index]);
        } else { // For "All Numbers" mode, place non-cardinal numbers at calculated marker positions.
             int num_offset_x = (String(hour).length() > 1) ? 3 : 1;
             dma_canvas.setCursor(marker_x - num_offset_x, marker_y + 2);
        }
        dma_canvas.print(hour);
    }
    
    if (should_draw_star) {
        // Draw the star-like marker at the mathematically calculated position.
        dma_canvas.drawPixel(marker_x, marker_y, star_color);
        dma_canvas.drawPixel(marker_x + 1, marker_y, star_color);
        dma_canvas.drawPixel(marker_x - 1, marker_y, star_color);
        dma_canvas.drawPixel(marker_x, marker_y + 1, star_color);
        // ** THE FIX IS HERE **
        dma_canvas.drawPixel(marker_x, marker_y - 1, star_color);
    }
}

void Screen8() { // Fullscreen Analog Clock
    int screenIndex = currentScreen - 1;
    const auto& settings = allScreenSettings[screenIndex];
    getLocalTime(&timeinfo);

    uint16_t hour_color = dma_display->color565(settings.time_col.r, settings.time_col.g, settings.time_col.b);
    uint16_t minute_color = dma_display->color565(settings.ampm_col.r, settings.ampm_col.g, settings.ampm_col.b);
    uint16_t second_color = dma_display->color565(settings.seconds_col.r, settings.seconds_col.g, settings.seconds_col.b);
    uint16_t custom_number_color = dma_display->color565(settings.number_color.r, settings.number_color.g, settings.number_color.b);
    uint16_t custom_star_color = dma_display->color565(settings.star_color.r, settings.star_color.g, settings.star_color.b);
    uint16_t date_num_color = dma_display->color565(settings.date_col.r, settings.date_col.g, settings.date_col.b);
    uint16_t day_text_color = dma_display->color565(settings.month_col.r, settings.month_col.g, settings.month_col.b);

    float cycle_speed = map_float(settings.pageSlider, 0, 255, 1000.0f, 50.0f);
    float fuzziness_factor = map_float(settings.pageSlider2, 0, 255, 0.0f, 1.0f);
    float length_multiplier = map_float(settings.pageSlider3, 0, 255, 0.5f, 1.2f);

    dma_canvas.fillScreen(cc_blk);
    
    if (settings.daySwitch) { // This switch controls all markers
        float base_hue = (cycle_speed > 0) ? (millis() / cycle_speed) : 0;
        for (int i = 1; i <= 12; i++) {
            float current_hue = fmod(base_hue + (i * 30), 360.0f);
            uint16_t rainbow_color = hsvToRgb565(current_hue, 1.0f, 1.0f);
            uint16_t num_color_to_use = (settings.numberColorMode == "Rainbow") ? rainbow_color : custom_number_color;
            uint16_t star_color_to_use = (settings.starColorMode == "Rainbow") ? rainbow_color : custom_star_color;
            drawHourMarker(i, settings.markerDisplayMode, num_color_to_use, star_color_to_use);
        }
    }

    float center_x = 31.5f; float center_y = 15.5f;
    float second_len = 14.0f;
    float minute_len = 12.0f * length_multiplier;
    float hour_len = 8.0f * length_multiplier;

    float second_angle = (timeinfo.tm_sec / 60.0f) * 360.0f;
    float minute_angle = ((timeinfo.tm_min + timeinfo.tm_sec / 60.0f) / 60.0f) * 360.0f;
    float hour_angle = (((timeinfo.tm_hour % 12) + timeinfo.tm_min / 60.0f) / 12.0f) * 360.0f;

    float second_rad = (second_angle - 90.0f) * PI / 180.0f;
    float minute_rad = (minute_angle - 90.0f) * PI / 180.0f;
    float hour_rad = (hour_angle - 90.0f) * PI / 180.0f;

    float sec_x = center_x + second_len * cosf(second_rad); float sec_y = center_y + second_len * sinf(second_rad);
    float min_x = center_x + minute_len * cosf(minute_rad); float min_y = center_y + minute_len * sinf(minute_rad);
    float hr_x = center_x + hour_len * cosf(hour_rad); float hr_y = center_y + hour_len * sinf(hour_rad);

    if(settings.minuteHandSwitch) draw_aa_line_simple(dma_canvas, center_x, center_y, min_x, min_y, minute_color, fuzziness_factor);
    if(settings.hourHandSwitch) draw_aa_line_simple(dma_canvas, center_x, center_y, hr_x, hr_y, hour_color, fuzziness_factor);
    if(settings.secondHandSwitch) draw_aa_line_simple(dma_canvas, center_x, center_y, sec_x, sec_y, second_color, fuzziness_factor);
    
    dma_canvas.fillCircle(round(center_x), round(center_y), 1, cc_bgry);

    // --- UPDATED DATE & DAY RENDERING LOGIC ---
    String day_str = "";
    String date_num_str = "";
    int total_width = 0;
    int space_width = 0;

    dma_canvas.setFont(&TomThumb);
    int16_t x1, y1;
    uint16_t w, h;

    // Check if the Day switch is on (using the repurposed monthSwitch)
    if (settings.monthSwitch) {
    char day_buf[4];
    if (currentLanguage == "de") {
        strncpy(day_buf, days_DE_short[timeinfo.tm_wday], sizeof(day_buf));
    } else if (currentLanguage == "sv") { // ADDED SWEDISH
        strncpy(day_buf, days_SE_short[timeinfo.tm_wday], sizeof(day_buf));
    } else {
        strftime(day_buf, sizeof(day_buf), "%a", &timeinfo);
    }
    day_str = String(day_buf);
    dma_canvas.getTextBounds(day_str, 0, 0, &x1, &y1, &w, &h);
    total_width += w;
    }

    // Check if the Date switch is on
    if (settings.dateSwitch) {
        char date_num_buf[4];
        strftime(date_num_buf, sizeof(date_num_buf), "%d", &timeinfo);
        date_num_str = String(date_num_buf);
        dma_canvas.getTextBounds(date_num_str, 0, 0, &x1, &y1, &w, &h);
        total_width += w;
    }
    
    // Add spacing if both are enabled
    if(settings.monthSwitch && settings.dateSwitch) {
      space_width = 2; // 2 pixel space
      total_width += space_width;
    }

    // Calculate the starting X position for the combined text to be centered
    int current_x = 47 - (total_width / 2);

    // Draw the Day part if enabled
    if(settings.monthSwitch) {
      dma_canvas.setTextColor(day_text_color);
      dma_canvas.setCursor(current_x, 19);
      dma_canvas.print(day_str);
      dma_canvas.getTextBounds(day_str, 0, 0, &x1, &y1, &w, &h);
      current_x += w + space_width; // Move cursor for the next part
    }
    
    // Draw the Date part if enabled
    if(settings.dateSwitch) {
      dma_canvas.setTextColor(date_num_color);
      dma_canvas.setCursor(current_x, 19);
      dma_canvas.print(date_num_str);
    }
}

void Screen9() { // Nixie Tube Clock
  // --- 1. SETUP ---
  int screenIndex = currentScreen - 1;
  const auto& settings = allScreenSettings[screenIndex];
  getLocalTime(&timeinfo);

  // --- 2. PREPARE ALL POSSIBLE TIME & DATE STRINGS ---
  // We prepare everything upfront, then decide what to draw.
  char hourBuf[4];
  char minBuf[4];
  char dayBuf[4];
  char monthBuf[4];

  strftime(minBuf, sizeof(minBuf), "%M", &timeinfo);
  
  bool is_pm = (timeinfo.tm_hour >= 12);
  int hour_12 = timeinfo.tm_hour % 12;
  if (hour_12 == 0) hour_12 = 12;

  if (settings.twentyFourHourSwitch) {
    strftime(hourBuf, sizeof(hourBuf), "%H", &timeinfo);
  } else {
    sprintf(hourBuf, "%d", hour_12);
  }

  strftime(dayBuf, sizeof(dayBuf), "%d", &timeinfo);
  strftime(monthBuf, sizeof(monthBuf), "%m", &timeinfo);

  // --- 3. DRAW BACKGROUND & DEFINE LAYOUT ---
  dma_canvas.fillScreen(cc_blk);
  dma_canvas.drawRGBBitmap(0, 0, pixie_table, 64, 32);

  const unsigned short* pixie_numbers[] = {
    pixie_0, pixie_1, pixie_2, pixie_3, pixie_4, 
    pixie_5, pixie_6, pixie_7, pixie_8, pixie_9
  };
  const int x_pos[] = {0, 13, 26, 39, 52}; 
  const int y_pos = (PANEL_RES_Y - 20) / 2;

  // --- 4. NEW LOGIC: DECIDE WHETHER TO SHOW DATE OR TIME ---
  bool showDateThisFrame = false;
  if (settings.dateSwitch) {
    // Check if the current second is within our 3-second window (20, 21, 22).
    if (timeinfo.tm_sec >= 20 && timeinfo.tm_sec < 23) {
      showDateThisFrame = true;
    }
  }

  // --- 5. RENDER EITHER DATE OR TIME BASED ON THE FLAG ---
  if (showDateThisFrame) {
    // --- DATE MODE ---
    String d1_str, d2_str, m1_str, m2_str;

    if (tempUnits == "fahrenheit") { // MM/DD
      m1_str = String(monthBuf[0]);
      m2_str = String(monthBuf[1]);
      d1_str = String(dayBuf[0]);
      d2_str = String(dayBuf[1]);
    } else { // DD/MM (Celsius)
      d1_str = String(dayBuf[0]);
      d2_str = String(dayBuf[1]);
      m1_str = String(monthBuf[0]);
      m2_str = String(monthBuf[1]);
    }
    
    dma_canvas.drawRGBBitmap(x_pos[0], y_pos, pixie_numbers[d1_str.toInt()], 12, 20);
    dma_canvas.drawRGBBitmap(x_pos[1], y_pos, pixie_numbers[d2_str.toInt()], 12, 20);
    dma_canvas.drawRGBBitmap(x_pos[2], y_pos, pixie_slash, 12, 20);
    dma_canvas.drawRGBBitmap(x_pos[3], y_pos, pixie_numbers[m1_str.toInt()], 12, 20);
    dma_canvas.drawRGBBitmap(x_pos[4], y_pos, pixie_numbers[m2_str.toInt()], 12, 20);

  } else {
    // --- TIME MODE ---
    String h_str = String(hourBuf);
    String m_str = String(minBuf);

    // Draw Hour Digits
    if (h_str.length() == 2) {
      dma_canvas.drawRGBBitmap(x_pos[0], y_pos, pixie_numbers[h_str.substring(0,1).toInt()], 12, 20);
      dma_canvas.drawRGBBitmap(x_pos[1], y_pos, pixie_numbers[h_str.substring(1).toInt()], 12, 20);
    } else {
      dma_canvas.drawRGBBitmap(x_pos[0], y_pos, pixie_Dot_OFF, 12, 20);
      dma_canvas.drawRGBBitmap(x_pos[1], y_pos, pixie_numbers[h_str.toInt()], 12, 20);
    }

    // Draw Blinking Colon
    if (timeinfo.tm_sec % 2 == 0) {
      dma_canvas.drawRGBBitmap(x_pos[2], y_pos, pixie_Dot_ON, 12, 20);
    } else {
      dma_canvas.drawRGBBitmap(x_pos[2], y_pos, pixie_Dot_OFF, 12, 20);
    }

    // Draw Minute Digits
    dma_canvas.drawRGBBitmap(x_pos[3], y_pos, pixie_numbers[m_str.substring(0,1).toInt()], 12, 20);
    dma_canvas.drawRGBBitmap(x_pos[4], y_pos, pixie_numbers[m_str.substring(1).toInt()], 12, 20);
  }
}


void Screen90() {     // Setup Clock QR code page
  // This screen is for showing the IP/QR code when connected for configuration
  if(currentState == STATE_RUNNING){
  // Define colors for this screen (using specific indices for this setup screen)        
  uint16_t time_color = dma_display->color565(web_time_col[currentScreen - 1].r, web_time_col[currentScreen - 1].g, web_time_col[currentScreen - 1].b);
  uint16_t ampm_color = dma_display->color565(web_ampm_col[currentScreen - 1].r, web_ampm_col[currentScreen - 1].g, web_ampm_col[currentScreen - 1].b);
  uint16_t seconds_color = dma_display->color565(web_seconds_col[currentScreen - 1].r, web_seconds_col[currentScreen - 1].g, web_seconds_col[currentScreen - 1].b);
  uint16_t background_color = dma_display->color565(web_day_col[currentScreen - 1].r, web_day_col[currentScreen - 1].g, web_day_col[currentScreen - 1].b);
  uint16_t date_color = dma_display->color565(web_date_col[currentScreen - 1].r, web_date_col[currentScreen - 1].g, web_date_col[currentScreen - 1].b);
  uint16_t month_color = dma_display->color565(web_month_col[currentScreen - 1].r, web_month_col[currentScreen - 1].g, web_month_col[currentScreen - 1].b);               // Time Shadow
  uint16_t date_bg_color = dma_display->color565(web_dateBG_col[currentScreen - 1].r, web_dateBG_col[currentScreen - 1].g, web_dateBG_col[currentScreen - 1].b);
  uint16_t temp_color = dma_display->color565(web_temp_col[currentScreen - 1].r, web_temp_col[currentScreen - 1].g, web_temp_col[currentScreen - 1].b);
  uint16_t humidity_color = dma_display->color565(web_humidity_col[currentScreen - 1].r, web_humidity_col[currentScreen - 1].g, web_humidity_col[currentScreen - 1].b);
  uint16_t land_color = dma_display->color565(web_land_col[currentScreen - 1].r, web_land_col[currentScreen - 1].g, web_land_col[currentScreen - 1].b);
  uint16_t water_color = dma_display->color565(web_water_col[currentScreen - 1].r, web_water_col[currentScreen - 1].g, web_water_col[currentScreen - 1].b);
  uint16_t ice_color = dma_display->color565(web_ice_col[currentScreen - 1].r, web_ice_col[currentScreen - 1].g, web_ice_col[currentScreen - 1].b);

  //  Showing Setup Wifi   //
  dma_canvas.fillScreen(0);    // Clear canvas (not display)
  dma_canvas.drawRGBBitmap(0, 0, (const uint16_t *)Frame_Neon_Fog, 64, 32);
  dma_canvas.setTextColor(cc_blk, cc_blk);  dma_canvas.setCursor(3, 8); dma_canvas.setFont(&Font_5x7_practical8pt7b);  dma_canvas.print("Setup"); // Shadow
  dma_canvas.setTextColor(cc_bylw, cc_blk);  dma_canvas.setCursor(2, 7); dma_canvas.setFont(&Font_5x7_practical8pt7b);  dma_canvas.print("Setup");
  dma_canvas.setTextColor(cc_blk, cc_blk);  dma_canvas.setCursor(6, 18); dma_canvas.setFont(&Font_5x7_practical8pt7b);  dma_canvas.print("Clock"); // Shadow
  dma_canvas.setTextColor(cc_bylw, cc_blk);  dma_canvas.setCursor(5, 17); dma_canvas.setFont(&Font_5x7_practical8pt7b);  dma_canvas.print("Clock");
  
  // Get the IP address as a string
  String ip = WiFi.localIP().toString();

  // Find the position of the second dot
  int firstDot = ip.indexOf('.');
  int secondDot = ip.indexOf('.', firstDot + 1);

  // Print first part ("192.168.")
  dma_canvas.setTextColor(cc_bgrn, cc_blk);
  dma_canvas.setFont(&TomThumb);
  dma_canvas.setCursor(0, 25);
  dma_canvas.print(ip.substring(0, secondDot + 1));

  // Print second part ("1.157")
  dma_canvas.setCursor(0, 31);
  dma_canvas.print(ip.substring(secondDot + 1));


  // Create QR code object
  QRCode qrcode;  // Declare the QRCode object
  
  // Get the IP address from WiFi
  String ipAddress = "http://" + WiFi.localIP().toString();
  uint8_t qrcodeData[qrcode_getBufferSize(3)];

  // Generate QR code (version 3 gives us 29x29 pixels)
  qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, ipAddress.c_str());

  // Calculate offsets: flush right and centered vertically
  int offsetX = 64 - qrcode.size - 1;  // Flush with right edge (35 pixels offset) with a -1 offset
  int offsetY = (32 - qrcode.size) / 2;  // Center vertically (1 pixel offset)

  // Draw QR code on matrix
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
        if (qrcode_getModule(&qrcode, x, y)) {
            // Draw a single pixel (black module)
            dma_canvas.drawPixel(
                offsetX + x, 
                offsetY + y,
                cc_bwht  // White color
            );
        }
        // Background remains black (0) from fillScreen
    }
  }
  } 

  
}

void Screen91(){ // Setup Wifi (when no credentials saved or connection failed)
  // This screen should only be shown when in STATE_WIFI_NO_CREDENTIALS or STATE_WIFI_DISCONNECTED

  dma_canvas.fillScreen(0);    // Clear canvas (not display)
  dma_canvas.drawRGBBitmap(0, 0, (const uint16_t *)Frame_Neon_Fog, 64, 32);
  dma_canvas.setTextColor(cc_blk, cc_blk);  dma_canvas.setCursor(5, 9); dma_canvas.setFont(&Font_5x7_practical8pt7b);  dma_canvas.print("Setup"); // Shadow
  dma_canvas.setTextColor(cc_bylw, cc_blk);  dma_canvas.setCursor(4, 8); dma_canvas.setFont(&Font_5x7_practical8pt7b);  dma_canvas.print("Setup");
  dma_canvas.setTextColor(cc_blk, cc_blk);  dma_canvas.setCursor(11, 18); dma_canvas.setFont(&Font_5x7_practical8pt7b);  dma_canvas.print("WIFI"); // Shadow
  dma_canvas.setTextColor(cc_bylw, cc_blk);  dma_canvas.setCursor(10, 17); dma_canvas.setFont(&Font_5x7_practical8pt7b);  dma_canvas.print("WIFI");
  dma_canvas.setTextColor(cc_bgrn, cc_blk);  dma_canvas.setCursor(0, 30); dma_canvas.setFont(&TomThumb); dma_canvas.print(AutoChipID);
        
  // Create QR code object
  QRCode qrcode;

  // Generate WiFi connection string
  String wifiString = "WIFI:S:" + String(AutoChipID) + ";T:nopass;P:;;";
  uint8_t qrcodeData[qrcode_getBufferSize(3)]; // Version 3

  // Generate QR code (version 3, 29x29 pixels)
  qrcode_initText(&qrcode, qrcodeData, 3, ECC_MEDIUM, wifiString.c_str());

  // Calculate offsets: flush right and centered vertically
  int offsetX = 64 - qrcode.size ;  // Flush with right edge (35 pixels offset) 
  int offsetY = (32 - qrcode.size) / 3;  // Center vertically (1 pixel offset)

  // Draw QR code on matrix
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        // Draw a single pixel (black module)
        dma_canvas.drawPixel(
          offsetX + x, 
          offsetY + y,
          cc_bwht  // White color
        );
      }
      // Background remains black (0) from fillScreen
    }
  }  
}



void Screen92() { // Matrix Settings Menu
  dma_canvas.fillScreen(0);
  dma_canvas.setFont(&Font_5x7_practical8pt7b);
  dma_canvas.drawRect(0, 0, 64, 10, cc_bblu);
  dma_canvas.setTextColor(cc_bwht);
  drawCentreChar("Menu", 0, 7);

  // --- Draw WiFi Signal Bar (Graphical) ---
  if (WiFi.status() == WL_CONNECTED) {
    long rssi = WiFi.RSSI();
    long barHeight = map(rssi, -90, -30, 1, 22);
    barHeight = constrain(barHeight, 1, 22);
    long percentage = map(rssi, -90, -30, 0, 100);
    percentage = constrain(percentage, 0, 100);

    // Calculate the top Y-coordinate for the bar so it draws from the bottom up.
    int barY = 32 - barHeight;
    dma_canvas.setFont(&TomThumb);
    dma_canvas.setTextColor(cc_bwht, cc_blk); // Default text color
    String wifiString = String(percentage) + "% ";

    // Draw the 2-pixel wide green bar on the far right (columns 62 and 63).
    dma_canvas.fillRect(63, barY, 1, barHeight, cc_bgrn);
    drawRightString(wifiString, -1, 32);
  }
  // --- End of WiFi Signal Bar code ---

  dma_canvas.setFont(&TomThumb);
  dma_canvas.setTextColor(cc_bwht, cc_blk); // Default text color

  // Draw Menu Options
  dma_canvas.setCursor(12, 16); dma_canvas.print("FORMAT SSD");
  dma_canvas.setCursor(12, 23); dma_canvas.print("CLEAR WIFI");
  dma_canvas.setCursor(12, 30); dma_canvas.print("EXIT");

  // Draw Selection Highlight
  switch(Menu_Select) {
    case 1: // Highlight Format SSD
      dma_canvas.fillTriangle(7,10, 7,16, 10,13, cc_bgrn); // Triangle highlight
      break;
    case 2: // Highlight CLEAR WIFI
      dma_canvas.fillTriangle(7,17, 7,23, 10,20, cc_bgrn);
      break;
    case 3: // Highlight EXIT
      dma_canvas.fillTriangle(7,24, 7,30, 10,27, cc_bgrn);
      break;
  }

  dma_canvas.setFont(); // Reset font if needed later
}



// Helper function for linear interpolation
float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Helper function to clamp values
float clamp(float val, float minVal, float maxVal) {
    return std::max(minVal, std::min(val, maxVal));
}

// Helper functions for solar position calculations (adapted from amCharts code)
float equationOfTime(float centuries) {
  float e = 0.016708634 - centuries * (0.000042037 + 0.0000001267 * centuries); // eccentricityEarthOrbit
  float m = (357.52911 + centuries * (35999.05029 - 0.0001537 * centuries)) * PI / 180.0; // solarGeometricMeanAnomaly
  float l = (280.46646 + centuries * (36000.76983 + centuries * 0.0003032)) * PI / 180.0; // solarGeometricMeanLongitude
  l = fmod(l, 2 * PI);
  if (l < 0) l += 2 * PI;

  float y = tan((23 + (26 + (21.448 - centuries * (46.815 + centuries * (0.00059 - centuries * 0.001813))) / 60) / 60) * PI / 180.0 / 2);
  y *= y;

  return y * sin(2 * l) - 2 * e * sin(m) + 4 * e * y * sin(m) * cos(2 * l) - 0.5 * y * y * sin(4 * l) - 1.25 * e * e * sin(2 * m);
}

float solarDeclination(float centuries) {
  float obliquity = (23 + (26 + (21.448 - centuries * (46.815 + centuries * (0.00059 - centuries * 0.001813))) / 60) / 60) * PI / 180.0;
  obliquity += 0.00256 * cos((125.04 - 1934.136 * centuries) * PI / 180.0) * PI / 180.0;

  float l = (280.46646 + centuries * (36000.76983 + centuries * 0.0003032));
  l = fmod(l, 360);
  if (l < 0) l += 360;
  l = (l / 180) * PI;

  float m = (357.52911 + centuries * (35999.05029 - 0.0001537 * centuries)) * PI / 180.0;
  float eqCenter = (sin(m) * (1.914602 - centuries * (0.004817 + 0.000014 * centuries)) +
                    sin(2 * m) * (0.019993 - 0.000101 * centuries) +
                    sin(3 * m) * 0.000289) * PI / 180.0;
  float trueLon = l + eqCenter;
  float apparentLon = trueLon - (0.00569 + 0.00478 * sin((125.04 - 1934.136 * centuries) * PI / 180.0)) * PI / 180.0;

  return asin(sin(obliquity) * sin(apparentLon));
}

void solarPosition(time_t time, float &longitude, float &latitude) {
  // Calculate centuries since J2000 (January 1, 2000, 12:00 UTC)
  time_t j2000 = 946728000; // Unix timestamp for J2000
  float centuries = (float)(time - j2000) / (86400.0 * 36525.0);

  // Calculate longitude (adapted from amCharts)
  time_t dayStart = time - (time % 86400); // Round to start of day
  float offset = timezoneOffset * 3600; // Timezone offset in seconds
  longitude = ((dayStart - time - offset) / 86400.0) * 360.0 - 180.0;
  longitude = longitude - equationOfTime(centuries) * 180.0 / PI;

  // Normalize longitude to -180 to 180
  while (longitude < -180.0) longitude += 360.0;
  while (longitude > 180.0) longitude -= 360.0;

  // Calculate latitude (declination)
  latitude = solarDeclination(centuries) * 180.0 / PI;
}

float calculateMoonPhase(struct tm timeinfo) {
  // Reference new moon: March 29, 2025, 06:58 UTC (JD ~2460764.79)
  double year = timeinfo.tm_year + 1900;
  double month = timeinfo.tm_mon + 1;
  double day = timeinfo.tm_mday + (timeinfo.tm_hour + timeinfo.tm_min / 60.0 + timeinfo.tm_sec / 3600.0) / 24.0;

  int a = (14 - month) / 12;
  int y = year + 4800 - a;
  int m = month + 12 * a - 3;
  double jd = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;

  double days_since_new_moon = jd - 2460764.79;

  const double synodic_period = 29.530588853;

  double phase = days_since_new_moon / synodic_period;
  phase -= floor(phase);

  if (phase < 0.0) phase += 1.0;

  // Calibration offset to match PirateWeather on April 12, 2025
  phase += 0.023;
  phase -= floor(phase);

  // Apply terminator offset if any
  phase += terminatorOffset / 360.0;
  phase -= floor(phase);

  return (float)phase;
}

void dayOfYearToDate(int dayOfYear, int year, int &month, int &day) {
  int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool isLeap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  if (isLeap) daysInMonth[1] = 29;

  int remainingDays = dayOfYear - 1;
  month = 0;
  while (remainingDays >= daysInMonth[month]) {
    remainingDays -= daysInMonth[month];
    month++;
  }
  day = remainingDays + 1;
}

void handleSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() > 0) {
      if (input == "year" && !yearAnimationActive) {
        handleFullYearAnimation(); 
      } else if (input == "colour") {
        // Unchanged colour output code
        Serial.println("Settings Grouped by Variable:");
        Serial.println("=====================================");
        // ... (rest of colour output unchanged)
      } else if (input == "time save") {
        syncESPtoRTC();
      } else if (input == "time load") {
        syncRTCtoESP();
      } else if (input == "print spiffs") {
        printSpiffsFile();
      } else if (input.startsWith("d") && input.length() > 1) {
        String dayStr = input.substring(1);
        int newDay = dayStr.toInt();
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
          Serial.println("Failed to get current time.");
          return;
        }
        int currentYear = timeinfo.tm_year + 1900;
        int maxDays = (currentYear % 4 == 0 && currentYear % 100 != 0) || (currentYear % 400 == 0) ? 366 : 365;
        if (newDay >= 1 && newDay <= maxDays) {
          int month, day;
          dayOfYearToDate(newDay, currentYear, month, day);
          timeinfo.tm_year = currentYear - 1900;
          timeinfo.tm_mon = month;
          timeinfo.tm_mday = day;
          timeinfo.tm_hour = 12;
          timeinfo.tm_min = 0;
          timeinfo.tm_sec = 0;
          timeinfo.tm_isdst = -1;

          time_t newTime = mktime(&timeinfo);
          if (newTime == -1) {
            Serial.println("Error: mktime failed to normalize the time structure.");
            return;
          }

          struct timeval tv = {newTime, 0};
          settimeofday(&tv, NULL);

          // Calculate moon phase for new date
          useCalculatedMoonPhase = true;
          calculatedMoonPhase = calculateMoonPhase(timeinfo);
          float illumFraction = 1.0f - fabs(2.0f * calculatedMoonPhase - 1.0f);
          moonPercentage = roundf(illumFraction * 100.0f * 10.0f) / 10.0f;

          struct tm updatedTimeinfo;
          if (getLocalTime(&updatedTimeinfo)) {
            char dateStr[20];
            strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &updatedTimeinfo);
            Serial.printf("Day set to %d (Year: %d, Date: %s), Moon phase: %.3f, Moon percentage: %.1f%%, NTP paused for 10 seconds.\n", 
                          newDay, currentYear, dateStr, calculatedMoonPhase, moonPercentage);
          } else {
            Serial.printf("Day set to %d (Year: %d), Moon phase: %.3f, Moon percentage: %.1f%%, NTP paused for 10 seconds.\n", 
                          newDay, currentYear, calculatedMoonPhase, moonPercentage);
          }

          ntpPaused = true;
          pauseStartTime = millis();
        } else {
          Serial.printf("Invalid day. Please enter a number between 1 and %d for year %d (e.g., d135).\n", 
                        maxDays, currentYear);
        }
      } else {
        int newOffset = input.toInt();
        if (input == "0" || newOffset != 0) {
          terminatorOffset = newOffset;
          Serial.printf("Terminator offset updated to: %d\n", terminatorOffset);
        } else {
          Serial.println("Invalid input. Please enter an integer (e.g., 5 or -3), day (e.g., d135), 'year', 'colour', or 'sync'.");
        }
      }
    }
  }

  // Handle year animation
  if (yearAnimationActive) {
    unsigned long currentTime = millis();
     if (currentTime - animationStartTime < ANIMATION_DURATION) {       
      unsigned long elapsedTime = currentTime - animationStartTime;
      int targetDay = 1 + (elapsedTime / DISPLAY_TIME_PER_DAY);
      if (targetDay != currentAnimationDay) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          int currentYear = timeinfo.tm_year + 1900;
          int month, day;
          dayOfYearToDate(targetDay, currentYear, month, day);
          timeinfo.tm_year = currentYear - 1900;
          timeinfo.tm_mon = month;
          timeinfo.tm_mday = day;
          timeinfo.tm_hour = 12;
          timeinfo.tm_min = 0;
          timeinfo.tm_sec = 0;
          timeinfo.tm_isdst = -1;

          time_t newTime = mktime(&timeinfo);
          struct timeval tv = {newTime, 0};
          settimeofday(&tv, NULL);

          // Update moon phase
          calculatedMoonPhase = calculateMoonPhase(timeinfo);
          float illumFraction = 1.0f - fabs(2.0f * calculatedMoonPhase - 1.0f);
          moonPercentage = roundf(illumFraction * 100.0f * 10.0f) / 10.0f;

          // Update the date string for display, considering language
          char dateStr[8];
          if (currentLanguage == "de") {
            strncpy(dateStr, months_DE_short[timeinfo.tm_mon], sizeof(dateStr));
          } else if (currentLanguage == "sv") { // ADDED SWEDISH
            strncpy(dateStr, months_SE_short[timeinfo.tm_mon], sizeof(dateStr));
          } else {
            strftime(dateStr, sizeof(dateStr), "%b", &timeinfo);
          }
          animationDateStr = String(dateStr);

          currentAnimationDay = targetDay;
          Serial.printf("Animation day %d, Moon phase: %.3f, Moon percentage: %.1f%%\n", 
                        targetDay, calculatedMoonPhase, moonPercentage);
        }
      }
    } else {
      // Animation complete
      File file = SPIFFS.open("/settings.json", "r");
      if (!file) {
        Serial.println("Failed to open settings file for reading, using defaults");
        return;
      }

      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, file);
      if (error) {
        Serial.println("Failed to parse settings file: " + String(error.c_str()));
        file.close();
        return;
      }
      yearAnimationActive = false;
      animationDateStr = "";
      ntpPaused = false;
      useCalculatedMoonPhase = false; // Resume PirateWeather moonPhase
      moonPhase = doc["moon_Phase"] | 0.15;
      moonPercentage = doc["moon_Percentage"] | 25.0;
      configTime(0, 0, ntpServer);
      setenv("TZ", MY_TZ, 1);
      tzset();
      Serial.println("Year animation completed, NTP sync resumed.");
    }
  }

  if (ntpPaused && !yearAnimationActive && (millis() - pauseStartTime >= 10000)) {
    File file = SPIFFS.open("/settings.json", "r");
    if (!file) {
      Serial.println("Failed to open settings file for reading, using defaults");
      return;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
      Serial.println("Failed to parse settings file: " + String(error.c_str()));
      file.close();
      return;
    }
    ntpPaused = false;
    useCalculatedMoonPhase = false; // Resume PirateWeather moonPhase
    configTime(0, 0, ntpServer);
    moonPhase = doc["moon_Phase"] | 0.15;
    moonPercentage = doc["moon_Percentage"] | 25.0;
    setenv("TZ", MY_TZ, 1);
    tzset();
    Serial.println("NTP sync resumed.");
  }
}

void handleLanguage() {
  if (server.hasArg("lang")) {
    String lang = server.arg("lang");
    if (lang == "en" || lang == "de" || lang == "sv") { // ADDED SWEDISH
      currentLanguage = lang;
      saveSettings();
      server.send(200, "text/plain", "OK");
      Serial.println("Language set to: " + currentLanguage);
    } else {
      server.send(400, "text/plain", "Invalid language");
    }
  } else {
    server.send(400, "text/plain", "Missing lang parameter");
  }
}

void handleIndoorTempOffset() {
  if (server.hasArg("value")) {
    indoorTempOffset = server.arg("value").toFloat();
    saveSettings();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing value");
  }
}

void handleSchedulesEnabled() {
  if (server.hasArg("state")) {
    schedulesEnabled = (server.arg("state") == "true");
    saveSettings();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing state");
  }
}

void handleUpdateSchedules() {
  if (server.hasArg("defaultScreen")) {
    defaultScreen = server.arg("defaultScreen").toInt();
  }

  if (server.hasArg("schedules")) {
    String scheduleData = server.arg("schedules");
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, scheduleData);

    if (error) {
      server.send(400, "text/plain", "Invalid JSON for schedules");
      return;
    }

    schedules.clear(); // Clear the old schedule list
    JsonArray newSchedules = doc.as<JsonArray>();
    for (JsonObject s : newSchedules) {
      Schedule newSched;
      newSched.screen = s["screen"];
      
      // Parse "HH:MM" string back into integers
      String startStr = s["start"];
      String endStr = s["end"];
      newSched.start_hour = startStr.substring(0, 2).toInt();
      newSched.start_min = startStr.substring(3, 5).toInt();
      newSched.end_hour = endStr.substring(0, 2).toInt();
      newSched.end_min = endStr.substring(3, 5).toInt();
      
      schedules.push_back(newSched);
    }
    Serial.printf("Received and saved %d schedules.\n", schedules.size());
  }
  
  saveSettings();
  server.send(200, "text/plain", "Schedules updated");
}

void handlePanelType() {
  if (server.hasArg("value")) {
    String newType = server.arg("value");
    if (newType == "P2.5" || newType == "P5.0") {
      panelType = newType;
      Serial.println("[Web Handler] Global panelType variable set to: " + panelType);
      saveSettings();
      server.send(200, "text/plain", "OK. Reboot required.");
      Serial.println("Panel type set to: " + panelType + ". Reboot required for changes to take effect.");
    } else {
      server.send(400, "text/plain", "Invalid panel type");
    }
  } else {
    server.send(400, "text/plain", "Missing value parameter");
  }
}

void printSpiffsFile() {
  File file = SPIFFS.open("/settings.json", "r");
  if (!file) {
    Serial.println("Failed to open settings.json for printing.");
    return;
  }
  Serial.println("\n--- Contents of /settings.json ---");
  while (file.available()) {
    Serial.write(file.read());
  }
  Serial.println("\n------------------------------------");
  file.close();
}

void drawYearAnimationDate() {
  if (yearAnimationActive && animationDateStr != "") {
    dma_canvas.setFont(&TomThumb);
    dma_canvas.setTextColor(dma_display->color565(255, 0, 0)); // Red
    dma_canvas.setCursor(1, 31);
    dma_canvas.print(animationDateStr);
  }
}



void updateCurrentHoursMins() {
  // Get the settings for the currently active screen.
  int screenIndex = currentScreen - 1;
  
  // Safety check: if currentScreen is invalid, default to the first screen's settings.
  if (screenIndex < 0 || screenIndex >= allScreenSettings.size()) {
    screenIndex = 0;
  }
  const auto& settings = allScreenSettings[screenIndex];

  char format[16];  // Buffer for the strftime format string

  // ** THE CORRECTION **
  // Now we use the 'settings' object for the currently active screen to decide the format.
  if (settings.twentyFourHourSwitch) {
    // 24-hour format (HH:MM or HH:MM:SS)
    if (settings.secondsSwitch) {
      strcpy(format, "%H:%M:%S");  // 24-hour with seconds (e.g., "14:30:45")
    } else {
      strcpy(format, "%H:%M");     // 24-hour without seconds (e.g., "14:30")
    }
  } else {
    // 12-hour format
    if (settings.ampmSwitch) {
      // 12-hour format with AM/PM
      if (settings.secondsSwitch) {
        strcpy(format, "%l:%M:%S %p");  // 12-hour with seconds and AM/PM (e.g., "8:30:45 PM")
      } else {
        strcpy(format, "%l:%M %p");     // 12-hour without seconds and with AM/PM (e.g., "8:30 PM")
      }
    } else {
      // 12-hour format without AM/PM
      if (settings.secondsSwitch) {
        strcpy(format, "%l:%M:%S");     // 12-hour with seconds, no AM/PM (e.g., "8:30:45")
      } else {
        strcpy(format, "%l:%M");        // 12-hour without seconds and no AM/PM (e.g., "8:30")
      }
    }
  }

  // Use strftime to format the time based on the selected format
  strftime(current_hoursmins, 64, format, &timeinfo);
}



void choosescreen() {
  // 1. Start with the currently selected screen
  int screenToShow = currentScreen; 

  // 2. Only apply schedules if we are NOT in a special menu (Screens 90+)
  //    and Schedules are actually enabled.
  if (currentScreen < 90 && schedulesEnabled) {
    screenToShow = defaultScreen; // Start with default
    getLocalTime(&timeinfo);

    int now_in_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;

    // Check all schedules
    for (const auto& sched : schedules) {
      int start_in_minutes = sched.start_hour * 60 + sched.start_min;
      int end_in_minutes = sched.end_hour * 60 + sched.end_min;
      
      bool isActive = false;
      if (start_in_minutes <= end_in_minutes) {
        // Normal day (e.g., 09:00 to 17:00)
        if (now_in_minutes >= start_in_minutes && now_in_minutes < end_in_minutes) {
          isActive = true;
        }
      } else {
        // Overnight (e.g., 22:00 to 02:00)
        if (now_in_minutes >= start_in_minutes || now_in_minutes < end_in_minutes) {
          isActive = true;
        }
      }
      
      if (isActive) {
        screenToShow = sched.screen;
        break; // Found a match, stop looking
      }
    }

    // *** THE CRITICAL FIX ***
    // We must update the global 'currentScreen' so that ScreenX() 
    // loads the correct color settings from the array.
    currentScreen = screenToShow; 
  }

  // 3. Dispatch to the correct screen function
  switch (screenToShow) {
    case SCREEN_ID_SETUP_QR:
      Screen90(); 
      break;
    case SCREEN_ID_WIFI_PORTAL:
      Screen91();
      break;
    case SCREEN_ID_MENU:
      Screen92();
      break;
    default:
      if (screenToShow >= 1 && screenToShow <= NUM_CLOCK_SCREENS) {
        clockScreenFunctions[screenToShow - 1]();
      } else {
        clockScreenFunctions[0](); // Fallback
      }
      break;
  }
}


void synchroniseWith_NTP_Time() {
  // This function attempts NTP sync without blocking indefinitely.
  // State machine in loop() is responsible for retries/timeouts.

  Serial.print("Attempting NTP synchronization...");
  configTime(0, 0, ntpServer); // Set timezone to UTC first for configTime
  setenv("TZ", selectedTimezone.c_str(), 1); // Set user selected timezone
  tzset(); // Apply timezone settings

  time_t now_t = 0;
  struct tm timeinfo_local;
  // Attempt to get local time using the configured NTP/TZ settings
  bool success = getLocalTime(&timeinfo_local, 5000); // Use a timeout

  if (success && timeinfo_local.tm_year + 1900 >= 2000) {
    Serial.println("Time synchronized successfully via NTP.");
    // Update the global timeinfo struct (though getLocalTime updates internal clock)
    timeinfo = timeinfo_local;
    updateCurrentHoursMins(); // Update formatted time string
    // State machine in loop() will detect this and transition to STATE_RUNNING
    syncESPtoRTC();
  } else {
    Serial.println("NTP synchronization failed or timed out.");
    // State machine in loop() will handle the failure (e.g., transition to limited mode)
    // The timeinfo struct will retain the last known (potentially incorrect) time
    // or time from RTC if syncRTCtoESP() was called.
  }
  // This function should NOT block indefinitely.
}

void syncRTCtoESP() {
  // Get time from DS3231
  DateTime rtcTime = rtc.now();
  
  // Check if RTC time is valid (year >= 2000)
  if (rtcTime.year() >= 2000) {
    // Convert RTC time to struct tm
    struct tm timeinfo;
    timeinfo.tm_year = rtcTime.year() - 1900; // Years since 1900
    timeinfo.tm_mon = rtcTime.month() - 1;    // Months 0-11
    timeinfo.tm_mday = rtcTime.day();
    timeinfo.tm_hour = rtcTime.hour();
    timeinfo.tm_min = rtcTime.minute();
    timeinfo.tm_sec = rtcTime.second();
    timeinfo.tm_isdst = -1; // Let system determine DST

    // Convert to Unix time and set ESP32 clock
    time_t newTime = mktime(&timeinfo);
    if (newTime != -1) {
      struct timeval tv = {newTime, 0};
      settimeofday(&tv, NULL);
      
      // Format and print the synchronized time and date
      char timeStr[20];
      strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &timeinfo);
      Serial.println("*************************************************");
      Serial.printf("ESP32 clock synchronized with RTC: %s\n", timeStr);
      Serial.println("*************************************************");
    } else {
      Serial.println("Error: Failed to convert RTC time to Unix time");
    }
  } else {
    Serial.println("Invalid RTC time, ESP32 sync skipped");
  }
}

void syncESPtoRTC() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  
  // Check if NTP time is valid (year >= 2000)
  if (timeinfo.tm_year + 1900 >= 2000) {
    // Sync to DS3231
    rtc.adjust(DateTime(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    ));
    // Format and print the synchronized time and date
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%d-%m-%Y %H:%M:%S", &timeinfo);
      Serial.println("*************************************************");
    Serial.printf("RTC synchronized with NTP: %s\n", timeStr);
      Serial.println("*************************************************");
  } else {
    Serial.println("Invalid NTP time, RTC sync skipped");
  }
}

void checkNTPSync() {
  // Only attempt NTP sync if in the running state and WiFi is connected
  if (currentState != STATE_RUNNING || WiFi.status() != WL_CONNECTED) {
      return;
  }

  unsigned long currentTime = millis();
  if (currentTime - lastNTPSync >= ntpSyncInterval || lastNTPSync == 0) {
    synchroniseWith_NTP_Time(); // This function already handles potential failure
    syncESPtoRTC(); // Sync RTC after attempting NTP
    lastNTPSync = currentTime;
  }
}

void drawCentreTime(const String &buf, int x, int y){
    int16_t x1, y1;
    uint16_t w, h;
    dma_canvas.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string

    if (timeinfo.tm_hour >= 1 && timeinfo.tm_hour <= 9) {dma_canvas.setCursor((PANEL_RES_X/2-2) - (w / 2), y);}                             // 1AM - 9AM:    Single digit hours get shifted by -4 pixels
    if (timeinfo.tm_hour >= 10 && timeinfo.tm_hour <= 12) {dma_canvas.setCursor((PANEL_RES_X/2+1) - (w / 2), y);}                           // 10AM - 12PM:     Double digit hours get shifted by +4 pixels
    if (timeinfo.tm_hour >= 13 && timeinfo.tm_hour <= 21) {dma_canvas.setCursor((PANEL_RES_X/2-2) - (w / 2), y);}                           // 1 - 9PM:      Single digit hours get shifted by -4 pixels
    if (timeinfo.tm_hour >= 22) {dma_canvas.setCursor((PANEL_RES_X/2+1) - (w / 2), y);}                                                     // 10 - 11PM:       Double digit hours get shifted by +4 pixels
    if (timeinfo.tm_hour == 0) {dma_canvas.setCursor((PANEL_RES_X/2+1) - (w / 2), y);}                                                      // 12AM:            Double digit hours get shifted by +4 pixels
    
    dma_canvas.print(buf); 

}

void drawCentreString(const String &buf, int x, int y){
    int16_t x1, y1;
    uint16_t w, h;
    dma_canvas.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    dma_canvas.setCursor((PANEL_RES_X/2) - (w / 2), y);
    dma_canvas.print(buf); 
}

void drawCentreThirdLeftString(const String &buf, int x, int y){
    int16_t x1, y1;
    uint16_t w, h;
    dma_canvas.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    dma_canvas.setCursor((PANEL_RES_X/4) - (w / 2), y);
    dma_canvas.print(buf); 
}


void drawRightString(const String &buf, int x, int y){
    int16_t x1, y1;
    uint16_t w, h;
    dma_canvas.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    dma_canvas.setCursor((PANEL_RES_X - w), y);
    dma_canvas.print(buf); 
}

void drawRightCenterString(const String &buf, int x, int y){
    int16_t x1, y1;
    uint16_t w, h;
    dma_canvas.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    dma_canvas.setCursor(((PANEL_RES_X/2) - (w+1)), y);
    dma_canvas.print(buf); 
}


void drawLeftString(const String &buf, int x, int y){
    int16_t x1, y1;
    uint16_t w, h;
    dma_canvas.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    dma_canvas.setCursor(0, y);
    dma_canvas.print(buf); 
  //  dma_display->print(buf);
}

void drawCentreChar(const char *buf, int x, int y){
    int16_t x1, y1;
    uint16_t w, h;
    dma_canvas.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    dma_canvas.setCursor((PANEL_RES_X/2) - (w / 2), y);
    dma_canvas.print(buf); 
}


void MenuButtonPressed() {
  // This function handles a short press of the menu button.
  // Its behavior depends on whether we are viewing clock faces (Menu_Page 1)
  // or are inside the on-device settings menu (Menu_Page 2).

  switch (Menu_Page) {
    case 1: // We are currently viewing a standard clock face.
      
      // Case 1: We are on a special setup/info screen (ID >= 90).
      if (currentScreen >= 90) {
        currentScreen = 1; // Always cycle from any special screen back to the first screen.
      } 
      // Case 2: We are on the last standard clock screen (screen 9).
      else if (currentScreen == NUM_CLOCK_SCREENS) {
        // Check for a valid WiFi connection by checking the system state.
        if (currentState == STATE_RUNNING) {
          // If fully connected and running, go to the Setup Clock QR code screen.
          currentScreen = SCREEN_ID_SETUP_QR; // Go to screen 90
        } else {
          // If not connected (or in any other state), go to the Setup WiFi QR code screen.
          currentScreen = SCREEN_ID_WIFI_PORTAL; // Go to screen 91
        }
      } 
      // Case 3: We are on any other standard screen (1 through 8).
      else if (currentScreen < NUM_CLOCK_SCREENS) {
        currentScreen++; // Simply go to the next screen.
      } 
      // Safety net: If currentScreen is somehow an invalid number, reset to 1.
      else {
        currentScreen = 1;
      }
      
      markSettingsChanged(); // Defer the flash write (was: saveSettings()) so the button stays responsive; debounceSaveSettings() in loop() flushes it shortly after.
      break;

    case 2: // We are currently inside the on-device settings menu.
      // A short press moves the selection highlight to the next menu item.
      switch (Menu_Select) {
        case 1: // Currently on "FORMAT SSD"
          Menu_Select = 2; // Move to "CLEAR WIFI"
          break;
        case 2: // Currently on "CLEAR WIFI"
          Menu_Select = 3; // Move to "EXIT"
          break;
        case 3: // Currently on "EXIT"
          Menu_Select = 1; // Wrap around to "FORMAT SSD"
          break;
      }
      break; 
  }
}


void MenuButtonHeld() {
  // This function handles a long press (hold) of the menu button.
  // Its behavior toggles between viewing clock faces and using the on-device menu.

  if (Menu_Page == 1) {
    // If we are currently viewing a clock face, a long press will ENTER the menu.

    // 1. Save the current screen so we can return to it later.
    prevScreen = currentScreen;
    
    // 2. Change the menu state to indicate we are now in the on-device menu.
    Menu_Page = 2;
    
    // 3. Set the menu selection to the first item by default.
    Menu_Select = 1;
    
    // 4. Set the current screen to the new ID for the menu screen.
    //    This replaces the old hardcoded 'currentScreen = 10'.
    currentScreen = SCREEN_ID_MENU; 

    return; // Exit function after setting up the menu.
  }

  if (Menu_Page == 2) {
    // If we are already in the menu, a long press will EXECUTE the highlighted option.

    switch (Menu_Select) {
      case 1: // Execute "FORMAT SSD"
        // We call the existing handler function for this action.
        handleFormatSSD(); 
        break;

      case 2: // Execute "CLEAR WIFI"
        // We call the existing handler function for this action.
        ClearWifi();
        break;

      case 3: // Execute "EXIT"
        // Change the menu state back to normal clock face viewing.
        Menu_Page = 1;
        Menu_Select = 0; // Reset menu selection.
        
        // Restore the screen to the one we were viewing before entering the menu.
        currentScreen = prevScreen; 
        
        return; // Exit function after handling the EXIT action.
    }
  }
}

void reboot(){
  ESP.restart();
}


void ClearWifi(){
  myWM.resetSettings();
  delay(500);  
  ESP.restart();
}


