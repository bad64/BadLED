#include <FastLED.h>
#include <EEPROM.h>

#define DEBUG 1

/* State flags */
#define RESET_EEPROM            0b10000000
#define USE_LOOPBACK            0b01000000
#define LEDS_STATE              0b00100000
#define STATE_UNUSED_BIT_1      0b00010000
#define STATE_UNUSED_BIT_2      0b00001000
#define STATE_UNUSED_BIT_3      0b00000100
#define STATE_UNUSED_BIT_4      0b00000010
#define STATE_UNUSED_BIT_5      0b00000001

/* Layout flags */
#define JOYSTICK_HAS_LEDS       0b10000000
#define HAS_4P                  0b01000000
#define FOUR_P_TURNS_ON_ALL_P   0b00100000
#define FOUR_P_SHARES_COLOR     0b00010000
#define HAS_4K                  0b00001000
#define FOUR_K_TURNS_ON_ALL_K   0b00000100
#define FOUR_K_SHARES_COLOR     0b00000010
#define HAS_EXTRA_UP_BUTTON     0b00000001

/* Physical constants */
#define NUMBER_OF_JOYSTICK_LEDS 4
#define NUMBER_OF_BUTTON_LEDS 9

/* Memory offset */
#define EEPROM_OFFSET 3

/* Pinout */
#define LED_DATA A0

static const uint8_t pins[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
static const char* names[] = { "RIGHT", "DOWN", "LEFT", "UP", "1P", "2P", "3P", "4P", "1K", "2K", "3K", "4K" };
static byte totalNumberOfKeys;
static byte totalNumberOfLeds;
static char serialBuffer[128];
static uint8_t lastSwitchState, currentSwitchState;

static CRGB* ledsList;

static uint16_t counter; // Cycle counter, used for timing and stuff
static byte stateFlags;
static byte layoutFlags;
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

void resetEEPROM()
{
  /* Sets all the bytes in the EEPROM to 0. Also sets up all button LEDs to be off by default, which is fairly nice */

  EEPROM.update(0, stateFlags);
  EEPROM.update(1, layoutFlags);
  EEPROM.update(2, 0x01);          // Default delay is 1 cycle
  
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

void updateColor(Button* btn, byte rn, byte gn, byte bn, byte ra, byte ga, byte ba)
{
  /* Updates a button's color values */
  
  // Updates color values for the button proper
  btn->colorWhenNotPressed = CRGB(rn, gn, bn);
  btn->colorWhenPressed = CRGB(ra, ga, ba);
  btn->currentColor = btn->colorWhenNotPressed;

  // Store new color values in the EEPROM
  EEPROM.update(btn->address + 0, rn);
  EEPROM.update(btn->address + 1, gn);
  EEPROM.update(btn->address + 2, bn);
  EEPROM.update(btn->address + 3, ra);
  EEPROM.update(btn->address + 4, ga);
  EEPROM.update(btn->address + 5, ba);
}

void setFlags(byte newStateFlags, byte newLayoutFlags)
{
  /* Updates the flags currently in use */
  EEPROM.update(0, newStateFlags);
  stateFlags = EEPROM.read(0); // To ensure reading happens after updating the flags in the EEPROM
  EEPROM.update(1, newLayoutFlags);
  layoutFlags = EEPROM.read(1);

  totalNumberOfKeys = (4 + 6 + ((layoutFlags & HAS_4P) >= 1) + ((layoutFlags & HAS_4K) >= 1) + (layoutFlags & HAS_EXTRA_UP_BUTTON));
}

void setDelay(byte newDelay)
{
  EEPROM.update(1, newDelay);
  updateDelay = EEPROM.read(1);
}

void panic(const char* errormsg)
{
  /* Panic "handler" if things go real south */
  
  Serial.print("FATAL ERROR: ");
  Serial.println(errormsg);

  while(true)
  {
    __asm__("nop\n\t");
  }
}

#ifdef DEBUG
/*
 * Some human readable functions
 */
  void getFlags()
  {
    char line[32] = { '\0' };

    sprintf(line, "RESET_EEPROM = %d\n", ((stateFlags & RESET_EEPROM) >> 7));
    Serial.print(line);
    
    sprintf(line, "USE_LOOPBACK = %d\n", ((stateFlags & USE_LOOPBACK) >> 6));
    Serial.print(line);

    sprintf(line, "LEDS_STATE = %d\n\n", ((stateFlags & LEDS_STATE) >> 5));
    Serial.print(line);
  
    sprintf(line, "JOYSTICK_HAS_LEDS = %d\n", ((layoutFlags & JOYSTICK_HAS_LEDS) >> 7));
    Serial.print(line);   
    
    sprintf(line, "HAS_4P = %d\n", (layoutFlags & HAS_4P) >> 6);
    Serial.print(line);
    
    sprintf(line, "FOUR_P_TURNS_ON_ALL_P = %d\n", (layoutFlags & FOUR_P_TURNS_ON_ALL_P) >> 5);
    Serial.print(line);

    sprintf(line, "FOUR_P_SHARES_COLOR = %d\n", (layoutFlags & FOUR_P_SHARES_COLOR) >> 4);
    Serial.print(line);
    
    sprintf(line, "HAS_4K = %d\n", (layoutFlags & HAS_4K) >> 3);
    Serial.print(line);
    
    sprintf(line, "FOUR_K_TURNS_ON_ALL_K = %d\n", (layoutFlags & FOUR_K_TURNS_ON_ALL_K) >> 2);
    Serial.print(line);

    sprintf(line, "FOUR_K_SHARES_COLOR = %d\n", (layoutFlags & FOUR_K_SHARES_COLOR) >> 1);
    Serial.print(line);
    
    sprintf(line, "HAS_EXTRA_UP_BUTTON = %d\n", (layoutFlags & HAS_EXTRA_UP_BUTTON));
    Serial.print(line);
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
#endif

void setup()
{ 
  // Turn on serial comms
  Serial.begin(9600);
  Serial.println("I'm a BadLED, duh");

  // Read flags
  stateFlags = EEPROM.read(0);
  layoutFlags = EEPROM.read(1);
  updateDelay = EEPROM.read(2);
  
  // Check if EEPROM has been initialized
  if (stateFlags & RESET_EEPROM)
  {
    stateFlags ^= RESET_EEPROM;
    Serial.print("Erasing EEPROM...");
    resetEEPROM();
    Serial.println(" Done");
    delay(50);
  }

  // Register number of keys and LEDs
  totalNumberOfKeys = (4 + 6 + ((layoutFlags & HAS_4P) >= 1) + ((layoutFlags & HAS_4K) >= 1) + (layoutFlags & HAS_EXTRA_UP_BUTTON));
  totalNumberOfLeds = NUMBER_OF_JOYSTICK_LEDS + NUMBER_OF_BUTTON_LEDS;

  // Initialize button list
  buttonsList = (Button*)calloc((totalNumberOfKeys), sizeof(Button));
  if (!buttonsList)
  {
    panic("Failure to allocate memory for button array !!");
  }
  initButtons(buttonsList, totalNumberOfKeys, names, pins);
  lastSwitchState = LOW;

  // Initialize LEDs
  ledsList = (CRGB*)calloc((totalNumberOfLeds), sizeof(CRGB));
  if (!ledsList)
  {
    panic("Failure to allocate memory for LED array !!");
  }
  FastLED.addLeds<WS2812B, LED_DATA, GRB>(ledsList, totalNumberOfLeds);
    
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
    if ((stateFlags & USE_LOOPBACK) && (strstr(serialBuffer, "get hwinfo") == 0))
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
      if (strcmp(target, "hwinfo") == 0)
      {
        // Order goes number of keys, delay, state flags, layout flags, then color info
        size_t size = 4 + (6 * totalNumberOfKeys);
        char* msg = (char*)calloc(size, sizeof(char));
        msg[0] = totalNumberOfKeys;
        msg[1] = updateDelay;
        msg[2] = stateFlags;
        msg[3] = layoutFlags;

        for (int i = 0; i < totalNumberOfKeys - 1; i++)
        {
          int j = 4 + (i * 6);
          msg[j+0] = buttonsList[i].colorWhenNotPressed.r;
          msg[j+1] = buttonsList[i].colorWhenNotPressed.g;
          msg[j+2] = buttonsList[i].colorWhenNotPressed.b;
          msg[j+3] = buttonsList[i].colorWhenPressed.r;
          msg[j+4] = buttonsList[i].colorWhenPressed.g;
          msg[j+5] = buttonsList[i].colorWhenPressed.b;
        }        

        Serial.write(msg, size);
        Serial.println("");
        free(msg);
      }
      else
      {
        Serial.println("ERROR: Unrecognized target");
      }
    }
    else if (strcmp(command, "set") == 0)
    {
      target = strtok(NULL, " \n");
      if (strcmp(target, "flags") == 0)
      {
        char* args = strtok(NULL, " \n");
        byte newStateFlags = atoi(args);
        args = strtok(NULL, " \n");
        byte newLayoutFlags = atoi(args);
        setFlags(newStateFlags, newLayoutFlags);
        Serial.println("Flags updated");
        
        #ifdef DEBUG
          getFlags();
        #endif
      }
      else if (strcmp(target, "buttons") == 0)
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

          // Don't update the button if there is no color change to save on EEPROM writes
          if ((buttonsList[i].colorWhenNotPressed.r != rn) || (buttonsList[i].colorWhenNotPressed.g != gn) || (buttonsList[i].colorWhenNotPressed.b != bn) ||
          (buttonsList[i].colorWhenPressed.r != ra) || (buttonsList[i].colorWhenPressed.g != ga) || (buttonsList[i].colorWhenPressed.b != ba))
          {
            updateColor(&buttonsList[i], rn, gn, bn, ra, ga, ba);
            //buttonToString(buttonsList[i]);
          }
        }
        Serial.println("OK");
      }
      else if (strcmp(target, "delay") == 0)
      {
        byte newDelay;
        char* args = strtok(NULL, " \n");
        newDelay = atoi(args);

        setDelay(newDelay);
        char* msg = (char*)calloc(16, sizeof(char));
        sprintf(msg, "Delay set to %d", newDelay); 
        Serial.println(msg);
        free(msg);
      }
      else
      {
        Serial.println("ERROR: Unrecognized target");
      }
    }
    else if (strcmp(command, "show") == 0)
    {
      #ifdef DEBUG
        /*
         * Most stuff here is gonna be human-readable debug info
         */
        target = strtok(NULL, " \n");
        
        if (strcmp(target, "flags") == 0)
        {
          getFlags();
        }
        else if (strcmp(target, "hwinfo") == 0)
        {
          char msg[4];
          itoa(totalNumberOfKeys, msg, 10);
          Serial.print(msg);
          Serial.print(" ");

          itoa(updateDelay, msg, 10);
          Serial.print(msg);
          Serial.print(" ");

          itoa(stateFlags, msg, 10);
          Serial.print(msg);
          Serial.print(" ");

          itoa(layoutFlags, msg, 10);
          Serial.println(msg);
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
        else
        {
          Serial.println("ERROR: Unrecognized item");
        }
      #else
        Serial.println("INFO: ROM compiled without debug options");
      #endif
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

  // Do we need to toggle the LEDs ?
  if (digitalRead(A1) == LOW)
  {
    currentSwitchState = LOW;

    if (currentSwitchState != lastSwitchState)
    {
      stateFlags ^= LEDS_STATE;
      setFlags(stateFlags, layoutFlags);
    }
  }
  else
  {
    currentSwitchState = HIGH;
  }
  lastSwitchState = currentSwitchState;

  if (stateFlags & LEDS_STATE)  // If LEDs are on
  {
    // Read button presses
    for (int i = 0; i < totalNumberOfKeys; i++)
    {
      if ((buttonsList[i].colorWhenPressed.r != buttonsList[i].colorWhenNotPressed.r) || (buttonsList[i].colorWhenPressed.g != buttonsList[i].colorWhenNotPressed.g) || (buttonsList[i].colorWhenPressed.b != buttonsList[i].colorWhenNotPressed.b))
      {
        if (digitalRead(buttonsList[i].pin) == LOW)
        {
          Serial.print(buttonsList[i].comName);
          Serial.println(" pressed");
          int j;
  
          if ( ((strcmp(buttonsList[i].comName, "4P") == 0) && ((layoutFlags & FOUR_P_TURNS_ON_ALL_P) != 0)) ||
          ((strcmp(buttonsList[i].comName, "4K") == 0) && ((layoutFlags & FOUR_K_TURNS_ON_ALL_K) != 0)) )
          {
            j = i - 3;
          }
          else
          {
            j = i;
          }
  
          for (j; j <= i; j++)
          {
            if ( ((strcmp(buttonsList[i].comName, "4P") == 0) && ((layoutFlags & FOUR_P_SHARES_COLOR) != 0)) ||
            ((strcmp(buttonsList[i].comName, "4K") == 0) && ((layoutFlags & FOUR_K_SHARES_COLOR) != 0)) )
            {
              buttonsList[j].currentColor = buttonsList[i].colorWhenPressed;
            }
            else
            {
              buttonsList[j].currentColor = buttonsList[j].colorWhenPressed;
            }
            buttonsList[j].state = ACTIVE;
    
            if (counter + updateDelay < 65535) 
              buttonsList[j].updateOn = counter + updateDelay;
            else
              buttonsList[j].updateOn = (counter + updateDelay) - 65535;
          }
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
          else
          {
            // Handle transition back from off state
            if (buttonsList[i].currentColor == CRGB(0, 0, 0))
              buttonsList[i].currentColor = buttonsList[i].colorWhenNotPressed;
          }
        }
      }
      else
      {
        // Frankly, why do anything if the colors are the same ?
        __asm__("nop\n\t");
      }
    }
  }
  else
  {
    for (int i = 0; i < totalNumberOfKeys; i++)
    {
      // Update once
      if (buttonsList[i].currentColor != CRGB(0, 0, 0))
        buttonsList[i].currentColor = CRGB(0, 0, 0);
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
