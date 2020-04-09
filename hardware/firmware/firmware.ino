#include <FastLED.h>
#include <EEPROM.h>

/* Flags */
// TODO: Better flags and/or use cases
#define USE_LOOPBACK            0b10000000
#define JOYSTICK_HAS_LEDS       0b01000000  // Unused
#define JOYSTICK_LEDS_ARE_FIRST 0b00100000  // Unused
#define HAS_4P                  0b00010000
#define FOUR_P_TURNS_ON_ALL_P   0b00001000  // Not presently used but planned
#define HAS_4K                  0b00000100
#define FOUR_K_TURNS_ON_ALL_K   0b00000010  // Not presently used but planned
#define HAS_EXTRA_UP_BUTTON     0b00000001

/* Physical constants */
#define NUMBER_OF_JOYSTICK_LEDS 4
#define NUMBER_OF_BUTTON_LEDS 9

/* Memory offset */
#define EEPROM_OFFSET 3

/* Pinout */
#define LED_DATA A0

static const uint8_t pins[] = { A4, A5, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static const char* names[] = { "RIGHT", "DOWN", "LEFT", "UP", "1P", "2P", "3P", "4P", "1K", "2K", "3K", "4K" };   // LED mappings should follow that order
static byte totalNumberOfKeys;
static byte totalNumberOfLeds;
static char serialBuffer[128];

static CRGB* ledsList;

static uint16_t counter; // Cycle counter, used for timing and stuff
static byte flags;
static byte updateDelay;

enum State { INACTIVE, ACTIVE, FADING };

typedef struct
{
  CRGB colorWhenNotPressed;
  CRGB colorWhenPressed;
  CRGB currentColor;
  byte id;
  char comName[6];
  byte pin;
  byte address;
  uint16_t updateOn;
  State state;
} Button;
Button* buttonsList;

typedef struct
{
  byte key;
  byte value[4];
  byte values;
} KeyValueMapping;
KeyValueMapping* dict;

void resetEEPROM()
{
  /* Sets all the bytes in the EEPROM to 0. Also sets up all button LEDs to be off by default, which is also fairly nice */

  EEPROM.write(0, 0x00);   // Set EEPROM Reset flag to 0. Any value other than 0xFF is fine
  EEPROM.write(1, 0x01);   // Default delay is 1 cycle
  EEPROM.write(2, flags);  // Conserve flags
  
  for (int i = EEPROM_OFFSET ; i < EEPROM.length() ; i++)
  {
    EEPROM.update(i, 0x00);
  }
}

void initButtons(Button* arr, byte len, const char** names, const uint8_t* pins)
{
  /* Creates all the data structures needed to hold color data */

  for (int i = 0; i < len; i++)
  {
    arr[i].address = EEPROM_OFFSET + (6 * i);
    
    arr[i].colorWhenNotPressed = CRGB(EEPROM.read(arr[i].address + 0), EEPROM.read(arr[i].address + 1), EEPROM.read(arr[i].address + 2));
    arr[i].colorWhenPressed = CRGB(EEPROM.read(arr[i].address + 3), EEPROM.read(arr[i].address + 4), EEPROM.read(arr[i].address + 5));
    arr[i].currentColor = arr[i].colorWhenNotPressed;

    arr[i].id = i;
    strcpy(arr[i].comName, names[i]);
    arr[i].pin = pins[i];
    arr[i].updateOn = 0;
    arr[i].state = INACTIVE;
  }
}

bool updateColor(Button* btn, byte rn, byte gn, byte bn, byte ra, byte ga, byte ba)
{
  /* Updates a button's color values */
  
  // Updates color values for the button proper
  btn->colorWhenNotPressed = CRGB(rn, gn, bn);
  btn->colorWhenPressed = CRGB(ra, ga, ba);
  btn->currentColor = btn->colorWhenNotPressed;

  // Store new color values in the EEPROM
  EEPROM.update(btn->address, rn);
  EEPROM.update(btn->address + 1, gn);
  EEPROM.update(btn->address + 2, bn);
  EEPROM.update(btn->address + 3, ra);
  EEPROM.update(btn->address + 4, ga);
  EEPROM.update(btn->address + 5, ba);

  return true;
}

void getFlags()
{
  char line[32] = { '\0' };
  
  sprintf(line, "USE_LOOPBACK = %d\n", ((flags & USE_LOOPBACK) >> 7));
  Serial.print(line);

  sprintf(line, "JOYSTICK_HAS_LEDS = %d\n", ((flags & JOYSTICK_HAS_LEDS) >> 6));
  Serial.print(line);
  
  sprintf(line, "JOYSTICK_LEDS_ARE_FIRST = %d\n", (flags & JOYSTICK_LEDS_ARE_FIRST) >> 5);
  Serial.print(line);       
  
  sprintf(line, "HAS_4P = %d\n", (flags & HAS_4P) >> 4);
  Serial.print(line);
  
  sprintf(line, "FOUR_P_TURNS_ON_ALL_P = %d\n", (flags & FOUR_P_TURNS_ON_ALL_P) >> 3);
  Serial.print(line);
  
  sprintf(line, "HAS_4K = %d\n", (flags & HAS_4K) >> 2);
  Serial.print(line);
  
  sprintf(line, "FOUR_K_TURNS_ON_ALL_K = %d\n", (flags & FOUR_K_TURNS_ON_ALL_K) >> 1);
  Serial.print(line);
  
  sprintf(line, "HAS_EXTRA_UP_BUTTON = %d\n", (flags & HAS_EXTRA_UP_BUTTON));
  Serial.print(line);
}

void setFlags(byte newFlags)
{
  /* Updates the flags currently in use */
  
  EEPROM.write(2, newFlags);
  flags = EEPROM.read(2); // To ensure reading happens after updating the flags in the EEPROM
}

void buttonToString(Button btn)
{
  /* Prints out a status report for a given button */
  
  char* msg = (char*)calloc(128, sizeof(char));
  sprintf(msg, "Button %s on pin %d:\r\n\t", btn.comName, btn.pin);
  Serial.print(msg);

  sprintf(msg, "Address: 0x%02x\r\n\t", btn.address);
  Serial.print(msg);

  sprintf(msg, "Status: %d\r\n\t", btn.state);
  Serial.print(msg);

  sprintf(msg, "Update on: %d\r\n\t", btn.updateOn);
  Serial.print(msg);

  sprintf(msg, "Unpressed RGB values: (%d, %d, %d)\r\n\t", btn.colorWhenNotPressed.r, btn.colorWhenNotPressed.g, btn.colorWhenNotPressed.b);
  Serial.print(msg);
  
  sprintf(msg, "Pressed RGB values: (%d, %d, %d)\r\n", btn.colorWhenPressed.r, btn.colorWhenPressed.g, btn.colorWhenPressed.b);
  Serial.print(msg);

  free(msg);
}

void setDelay(byte newDelay)
{
  EEPROM.write(1, newDelay);
  updateDelay = EEPROM.read(1);
}

void setup()
{ 
  // Turn on serial comms
  Serial.begin(9600);
  Serial.println("Hi");
  
  // Check if EEPROM has been initialized
  if (EEPROM.read(0) == 0xFF)
  {
    Serial.print("Erasing EEPROM...");
    resetEEPROM();
    Serial.println(" Done");
    delay(50);
  }

  // Read flags
  updateDelay = EEPROM.read(1);
  flags = EEPROM.read(2);

  // Register number of keys and LEDs
  totalNumberOfKeys = (4 + 6 + ((flags & HAS_4P) >> 4) + ((flags & HAS_4K) >> 2) + (flags & HAS_EXTRA_UP_BUTTON));
  totalNumberOfLeds = NUMBER_OF_JOYSTICK_LEDS + NUMBER_OF_BUTTON_LEDS;


  // Initialize button list
  buttonsList = (Button*)calloc((totalNumberOfKeys), sizeof(Button));
  if (!buttonsList)
  {
    Serial.println("FATAL ERROR: Failure to allocate memory for button array !!");
    while(true)
    {
      __asm__("nop\n\t");
    }
  }
  initButtons(buttonsList, totalNumberOfKeys, names, pins);

  // Initialize LEDs
  ledsList = (CRGB*)calloc((totalNumberOfLeds), sizeof(CRGB));
  if (!ledsList)
  {
    Serial.println("FATAL ERROR: Failure to allocate memory for LED array !!");
    while(true)
    {
      __asm__("nop\n\t");
    }
  }
  FastLED.addLeds<WS2812B, LED_DATA, GRB>(ledsList, totalNumberOfLeds);

  // Initialize mappings
  /*dict = (KeyValueMapping*)malloc((totalNumberOfKeys+1)*sizeof(KeyValueMapping));
  if (!dict)
  {
    Serial.println("FATAL ERROR: Failure to allocate memory for key/value associative array !!");
    while(true)
    {
      __asm__("nop\n\t");
    }
  }*/
    
  // Set up pins
  for (int i = 0; i < totalNumberOfKeys; i++)
    pinMode(pins[i], INPUT_PULLUP); 
}

void loop()
{
  // Check for incoming message
  Serial.flush();
  if (Serial.available() != 0)
  {
    Serial.readBytes(serialBuffer, 128);
    
    // Echo back command sent for debug
    if ((flags & USE_LOOPBACK) && (strstr(serialBuffer, "get flags") == 0))
    {
      char commandLoopbackBuffer[128];
      commandLoopbackBuffer[0] = '\0';

      strcat(commandLoopbackBuffer, "Received: ");
      strcat(commandLoopbackBuffer, serialBuffer);
      
      Serial.println(commandLoopbackBuffer);
    }

    char *command, *target;
    
    command = strtok(serialBuffer, " \n");

    if (strcmp(command, "get") == 0)
    {
      target = strtok(NULL, " \n");
      if (strcmp(target, "colorinfo") == 0)
      {
        char* msg = (char*)calloc(totalNumberOfKeys * 6, sizeof(char));
  
        for (int i = 0; i < totalNumberOfKeys - 1; i++)
        {
          int j = i * 6;
          msg[j+0] = buttonsList[i].colorWhenNotPressed.r;
          msg[j+1] = buttonsList[i].colorWhenNotPressed.g;
          msg[j+2] = buttonsList[i].colorWhenNotPressed.b;
          msg[j+3] = buttonsList[i].colorWhenPressed.r;
          msg[j+4] = buttonsList[i].colorWhenPressed.g;
          msg[j+5] = buttonsList[i].colorWhenPressed.b;
        }
  
        Serial.write(msg, totalNumberOfKeys * 6);
        Serial.println();
        free(msg);
      }
      else if (strcmp(target, "hwinfo") == 0)
      {
        char msg[3];
        itoa(totalNumberOfKeys, msg, 10);
        Serial.println(msg);
      }
      else if (strcmp(target, "flags") == 0)
      {
        Serial.println(flags);
      }
      else
      {
        Serial.println("Error: Unrecognized target");
      }
    }
    else if (strcmp(command, "set") == 0)
    {
      target = strtok(NULL, " \n");
      if (strcmp(target, "flags") == 0)
      {
        char* args = strtok(NULL, " \n");
        byte newFlags = atoi(args);
        setFlags(newFlags);
        Serial.println("Flags updated");
        getFlags();
      }
      else if (strcmp(target, "button") == 0)
      {
        byte buttonId, rn, gn, bn, ra, ga, ba;
  
        char* args = strtok(NULL, " \n");
        buttonId = atoi(args);
  
        if ((buttonId < 0) || (buttonId > totalNumberOfKeys))
        {
          Serial.print("Error: invalid button ID");
        }
        else
        {
          char* buffer;
          
          buffer = strtok(NULL, " \n");
          rn = atoi(buffer);
          buffer = strtok(NULL, " \n");
          gn = atoi(buffer);
          buffer = strtok(NULL, " \n");
          bn = atoi(buffer);
          buffer = strtok(NULL, " \n");
          ra = atoi(buffer);
          buffer = strtok(NULL, " \n");
          ga = atoi(buffer);
          buffer = strtok(NULL, " \n");
          ba = atoi(buffer);
          
          updateColor(&buttonsList[buttonId], rn, gn, bn, ra, ga, ba);
          buttonToString(buttonsList[buttonId]);
        }
      }
      else if (strcmp(target, "delay") == 0)
      {
        byte newDelay;
        char* args = strtok(NULL, " \n");
        newDelay = atoi(args);

        setDelay(newDelay);
        Serial.println("OK");
      }
      else if (strcmp(target, "allbuttons") == 0)
      {
        for (int i = 0; i < totalNumberOfKeys - 1; i++)
        {
          byte rn, gn, bn, ra, ga, ba;
          char* buffer;
          
          buffer = strtok(NULL, " \n");
          rn = atoi(buffer);
          buffer = strtok(NULL, " \n");
          gn = atoi(buffer);
          buffer = strtok(NULL, " \n");
          bn = atoi(buffer);
          buffer = strtok(NULL, " \n");
          ra = atoi(buffer);
          buffer = strtok(NULL, " \n");
          ga = atoi(buffer);
          buffer = strtok(NULL, " \n");
          ba = atoi(buffer);
          
          updateColor(&buttonsList[i], rn, gn, bn, ra, ga, ba);
          //buttonToString(buttonsList[i]);
        }
        Serial.println("OK");
      }
      else
      {
        Serial.println("Error: Unrecognized target");
      }
    }
    else if (strcmp(command, "show") == 0)
    {
      target = strtok(NULL, " \n");
      
      if (strcmp(target, "flags") == 0)
      {
        getFlags();
      }
      else if (strcmp(target, "button") == 0)
      {
        byte buttonId;
  
        char* args = strtok(NULL, " \n");
        if (args == NULL)
        {
          Serial.println("Error: Missing button ID");
        }
        else
        {
          buttonId = atoi(args);
          buttonToString(buttonsList[buttonId]);
        }
      }
      else if (strcmp(target, "delay") == 0)
      {
        char msg[4];
        itoa(updateDelay, msg, 10);
        Serial.println(msg);
      }
      else
      {
        Serial.println("Error: Unrecognized item");
      }
    }
    else if (strcmp(command, "reset") == 0)
    {
      resetEEPROM();
      Serial.println("EEPROM cleared");
      initButtons(buttonsList, totalNumberOfKeys, names, pins);
    }
    else
    {
      Serial.println("Error: Unrecognized command");
    }
  }

  // Read button presses
  for (int i = 0; i < totalNumberOfKeys; i++)
  {
    if ((buttonsList[i].colorWhenPressed.r != buttonsList[i].colorWhenNotPressed.r) || (buttonsList[i].colorWhenPressed.g != buttonsList[i].colorWhenNotPressed.g) || (buttonsList[i].colorWhenPressed.b != buttonsList[i].colorWhenNotPressed.b))
    {
      if (digitalRead(buttonsList[i].pin) == LOW)
      {
        buttonsList[i].currentColor = buttonsList[i].colorWhenPressed;
        buttonsList[i].state = ACTIVE;

        if (counter + updateDelay < 65535) 
          buttonsList[i].updateOn = counter + updateDelay;
        else
          buttonsList[i].updateOn = (counter + updateDelay) - 65535;
      }
      else
      {
        if (buttonsList[i].state == ACTIVE)
        {
          buttonsList[i].state = FADING;
        }
        else if (buttonsList[i].state == FADING)
        {
          if (counter >= buttonsList[i].updateOn)
          {
            if (buttonsList[i].currentColor.r != buttonsList[i].colorWhenNotPressed.r)
            {
              if (buttonsList[i].currentColor.r < buttonsList[i].colorWhenNotPressed.r)
                buttonsList[i].currentColor.r++;
              else if (buttonsList[i].currentColor.r > buttonsList[i].colorWhenNotPressed.r)
                buttonsList[i].currentColor.r--;
            }
    
            if (buttonsList[i].currentColor.g != buttonsList[i].colorWhenNotPressed.g)
            {
              if (buttonsList[i].currentColor.g < buttonsList[i].colorWhenNotPressed.g)
                buttonsList[i].currentColor.g++;
              else if (buttonsList[i].currentColor.g > buttonsList[i].colorWhenNotPressed.g)
                buttonsList[i].currentColor.g--;
            }
    
            if (buttonsList[i].currentColor.b != buttonsList[i].colorWhenNotPressed.b)
            {
              if (buttonsList[i].currentColor.b < buttonsList[i].colorWhenNotPressed.b)
                buttonsList[i].currentColor.b++;
              else if (buttonsList[i].currentColor.b > buttonsList[i].colorWhenNotPressed.b)
                buttonsList[i].currentColor.b--;
            }
    
            if ((buttonsList[i].currentColor.r == buttonsList[i].colorWhenNotPressed.r) && (buttonsList[i].currentColor.g == buttonsList[i].colorWhenNotPressed.g) && (buttonsList[i].currentColor.b == buttonsList[i].colorWhenNotPressed.b))
            {
              buttonsList[i].state = INACTIVE;
              buttonsList[i].updateOn = counter;
            }
            else
            {
              if (counter + updateDelay > 65535) buttonsList[i].updateOn = 65535 - (counter + updateDelay);
              else buttonsList[i].updateOn = counter + updateDelay;
            }
          }
        }
      }
    }
    else
    {
      // Frankly, why do anything if the colors are the same ?
      __asm__("nop\n\t");
    }
  }

  // Show them lights
  for (int i = 0; i < totalNumberOfKeys; i++)
  {
    ledsList[i] = buttonsList[i].currentColor;
  }
  FastLED.show();

  // Update cycle counter
  if (counter + 1 >= 65535) counter = 0;
  else counter++;
}
