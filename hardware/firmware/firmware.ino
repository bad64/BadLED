#include <FastLED.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

//#define DEBUG 1
//#define ENABLE_BUTTON_ECHO 1

/* State flags */
#define RESET_EEPROM            0b10000000
#define USE_LOOPBACK            0b01000000
#define LEDS_STATE              0b00100000
#define USE_BT                  0b00010000
#define STATE_UNUSED_BIT_1      0b00001000
#define STATE_UNUSED_BIT_2      0b00000100
#define STATE_UNUSED_BIT_3      0b00000010
#define STATE_UNUSED_BIT_4      0b00000001

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

static const char* idString = "I'm a BadLED, duh";
static const uint8_t pins[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
static const char* names[] = { "RIGHT", "DOWN", "LEFT", "UP", "1P", "2P", "3P", "4P", "1K", "2K", "3K", "4K" };
static byte totalNumberOfKeys;
static byte totalNumberOfLeds;
static char serialBuffer[256];
static char *command, *target;
static byte msgsize;
static uint8_t lastSwitchState, currentSwitchState;
SoftwareSerial bt(15, 16);
Stream* serialStream;

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
  EEPROM.update(2, newDelay);
  updateDelay = EEPROM.read(2);
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
  void getFlags(Stream* interface)
  {
    char line[32] = { '\0' };

    sprintf(line, "RESET_EEPROM = %d\n", ((stateFlags & RESET_EEPROM) >> 7));
    interface->print(line);
    
    sprintf(line, "USE_LOOPBACK = %d\n", ((stateFlags & USE_LOOPBACK) >> 6));
    interface->print(line);

    sprintf(line, "LEDS_STATE = %d\n\n", ((stateFlags & LEDS_STATE) >> 5));
    interface->print(line);
  
    sprintf(line, "JOYSTICK_HAS_LEDS = %d\n", ((layoutFlags & JOYSTICK_HAS_LEDS) >> 7));
    interface->print(line);   
    
    sprintf(line, "HAS_4P = %d\n", (layoutFlags & HAS_4P) >> 6);
    interface->print(line);
    
    sprintf(line, "FOUR_P_TURNS_ON_ALL_P = %d\n", (layoutFlags & FOUR_P_TURNS_ON_ALL_P) >> 5);
    interface->print(line);

    sprintf(line, "FOUR_P_SHARES_COLOR = %d\n", (layoutFlags & FOUR_P_SHARES_COLOR) >> 4);
    interface->print(line);
    
    sprintf(line, "HAS_4K = %d\n", (layoutFlags & HAS_4K) >> 3);
    interface->print(line);
    
    sprintf(line, "FOUR_K_TURNS_ON_ALL_K = %d\n", (layoutFlags & FOUR_K_TURNS_ON_ALL_K) >> 2);
    interface->print(line);

    sprintf(line, "FOUR_K_SHARES_COLOR = %d\n", (layoutFlags & FOUR_K_SHARES_COLOR) >> 1);
    interface->print(line);
    
    sprintf(line, "HAS_EXTRA_UP_BUTTON = %d\n", (layoutFlags & HAS_EXTRA_UP_BUTTON));
    interface->print(line);
  }
  
  void buttonToString(Stream* interface, Button btn)
  {
    /* Prints out a status report for a given button */
    
    char* msg = (char*)calloc(64, sizeof(char));
    sprintf(msg, "Button %s on pin %d:\r\n\t", btn.comName, btn.pin);
    interface->print(msg);
  
    sprintf(msg, "Address: 0x%02x\r\n\t", btn.address);
    interface->print(msg);
  
    sprintf(msg, "Status: %d\r\n\t", btn.state);
    interface->print(msg);
  
    sprintf(msg, "Update on: %d\r\n\t", btn.updateOn);
    interface->print(msg);
  
    sprintf(msg, "Unpressed RGB values: (%d, %d, %d)\r\n\t", btn.colorWhenNotPressed.r, btn.colorWhenNotPressed.g, btn.colorWhenNotPressed.b);
    interface->print(msg);
    
    sprintf(msg, "Pressed RGB values: (%d, %d, %d)\r\n", btn.colorWhenPressed.r, btn.colorWhenPressed.g, btn.colorWhenPressed.b);
    interface->print(msg);
  
    free(msg);
  }
#endif

void setup()
{ 
  // Turn on serial comms
  Serial.begin(9600);

  // Clear the buffer
  for (int i = 0; i < 256; i++)
    serialBuffer[i] = '\0';

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

  // Init BT comms if needed
  if (stateFlags & USE_BT)
  {
    bt.begin(57600);
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
  bt.flush();

  if ((stateFlags & USE_BT) && (bt.available() > 0))
  {
    serialStream = &bt;
  }
  else if ((stateFlags ^ USE_BT) && (Serial.available() > 0))
  {
    serialStream = &Serial;
  }
  
  if (serialStream->available() != 0)
  {
    msgsize = serialStream->readBytes(serialBuffer, 255);
    
      // Echo back command sent for debug
    if ((stateFlags & USE_LOOPBACK) && ((strcmp(serialBuffer, "get hwinfo\n") != 0)))   // Don't echo back at the client if it's asking for a state dump
    {
      char commandLoopbackBuffer[255];
      commandLoopbackBuffer[0] = '\0';

      strcat(commandLoopbackBuffer, "Received: ");
      if (serialBuffer[msgsize-1] == '\n')
        strncat(commandLoopbackBuffer, serialBuffer, msgsize-1);
      else
        strncat(commandLoopbackBuffer, serialBuffer, msgsize);
      serialStream->println(commandLoopbackBuffer);
    }
    
    if (strcmp(serialBuffer, "new phone who dis\n") == 0)
    {
      serialStream->println(idString);
    }    
    else
    {  
      command = strtok(serialBuffer, " \n");
  
      if (strcmp(command, "get") == 0)
      {
        target = strtok(NULL, " \n");
        if (strcmp(target, "hwinfo") == 0)
        {
          // Order goes state flags, layout flags, update delay, number of buttons, then color info
          size_t size = 16;      
          char* msg = (char*)calloc(size, sizeof(char));
          if (msg == NULL)
            panic("Failed to allocate memory for stage 1 loopback message buffer !!");
  
          char* buffer = (char*)calloc(4, sizeof(char));
          msg[0] = '\0';
  
          itoa(stateFlags, buffer, 10);
          strcat(msg, buffer);
          strcat(msg, " ");
  
          itoa(layoutFlags, buffer, 10);
          strcat(msg, buffer);
          strcat(msg, " ");
          
          itoa(updateDelay, buffer, 10);
          strcat(msg, buffer);
          strcat(msg, " ");
          
          itoa(totalNumberOfKeys, buffer, 10);
          strcat(msg, buffer);
          strcat(msg, " ");
  
          serialStream->print(msg);
  
          size = ((6 * 3) + 6);
          realloc(msg, size*sizeof(char));
          if (msg == NULL)
            panic("Failed to allocate memory for stage 2 loopback message buffer !!");
  
          // TODO: Remove last trailing space
          for (int i = 0; i < totalNumberOfKeys; i++)
          {
            msg[0] = '\0';
            itoa(buttonsList[i].colorWhenNotPressed.r, buffer, 10);
            strcat(msg, buffer);
            strcat(msg, " ");
  
            itoa(buttonsList[i].colorWhenNotPressed.g, buffer, 10);
            strcat(msg, buffer);
            strcat(msg, " ");
  
            itoa(buttonsList[i].colorWhenNotPressed.b, buffer, 10);
            strcat(msg, buffer);
            strcat(msg, " ");
  
            itoa(buttonsList[i].colorWhenPressed.r, buffer, 10);
            strcat(msg, buffer);
            strcat(msg, " ");
  
            itoa(buttonsList[i].colorWhenPressed.g, buffer, 10);
            strcat(msg, buffer);
            strcat(msg, " ");
  
            itoa(buttonsList[i].colorWhenPressed.b, buffer, 10);
            strcat(msg, buffer);
            strcat(msg, " ");
  
            serialStream->print(msg);
          }  
  
          serialStream->println("");
          free(buffer);
          free(msg);
        }
        else
        {
          serialStream->println("ERROR: Unrecognized target");
        }
      }
      else if (strcmp(command, "toggle") == 0)
      {
        target = strtok(NULL, " \n");
        if (strcmp(target, "leds") == 0)
        {
          stateFlags ^= LEDS_STATE;
          setFlags(stateFlags, layoutFlags);
          serialStream->print("LEDs are now ");
          if (stateFlags & LEDS_STATE) serialStream->println("ON");
          else serialStream->println("OFF");
        }
        else
        {
          serialStream->println("ERROR: Unrecognized target"); 
        }
      }
      else if (strcmp(command, "set") == 0)
      {
        target = strtok(NULL, " \n");
        if (strcmp(target, "flags") == 0)
        {
          char* args = strtok(NULL, " \n");
          uint16_t newStateFlags = atoi(args);
          if (newStateFlags >= 256)
            newStateFlags = stateFlags;
          args = strtok(NULL, " \n");
          uint16_t newLayoutFlags = atoi(args);
          if (newLayoutFlags >= 256)
            newLayoutFlags = layoutFlags;
          setFlags(newStateFlags, newLayoutFlags);
  
          char* msg = (char*)calloc(32, sizeof(char));
          msg[0] = '\0';
          strcat(msg, "Flags set to ");
  
          char* c = (char*)calloc(4, sizeof(char));
          itoa(stateFlags, c, 10);
          strcat(msg, c);
          strcat(msg, " ");
          itoa(layoutFlags, c, 10);
          strcat(msg, c);
  
          serialStream->println(msg);
  
          free(c);
          free(msg);
          
          #ifdef DEBUG
            getFlags(serialStream);
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
            }
          }
          serialStream->println("OK");
        }
        else if (strcmp(target, "delay") == 0)
        {
          byte newDelay;
          char* args = strtok(NULL, " \n");
          newDelay = atoi(args);
  
          setDelay(newDelay);
          char* msg = (char*)calloc(16, sizeof(char));
          sprintf(msg, "Delay set to %d", updateDelay); 
          serialStream->println(msg);
          free(msg);
        }
        #ifdef DEBUG
        else if (strcmp(target, "button") == 0)
        {
          byte i;
          byte rn, gn, bn, ra, ga, ba;
          char* buffer;
  
          buffer = strtok(NULL, " \n");
          i = atoi(buffer);
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
  
          if ((buttonsList[i].colorWhenNotPressed.r != rn) || (buttonsList[i].colorWhenNotPressed.g != gn) || (buttonsList[i].colorWhenNotPressed.b != bn) ||
          (buttonsList[i].colorWhenPressed.r != ra) || (buttonsList[i].colorWhenPressed.g != ga) || (buttonsList[i].colorWhenPressed.b != ba))
          {
            updateColor(&buttonsList[i], rn, gn, bn, ra, ga, ba);
            buttonToString(serialStream, buttonsList[i]);
          }
        }
        #endif
        else
        {
          serialStream->println("ERROR: Unrecognized target");
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
            getFlags(serialStream);
          }
          else if (strcmp(target, "button") == 0)
          {
            byte buttonId;
      
            char* args = strtok(NULL, " \n");
            if (args == NULL)
            {
              serialStream->println("Error: Missing button ID");
            }
            else
            {
              buttonId = atoi(args);
              buttonToString(serialStream, buttonsList[buttonId]);
            }
          }
          else
          {
            serialStream->println("ERROR: Unrecognized item");
          }
        #else
          serialStream->println("INFO: ROM compiled without debug options");
        #endif
      }
      else if (strcmp(command, "reset") == 0)
      {
        resetEEPROM();
        serialStream->println("EEPROM cleared");
        initButtons(buttonsList, totalNumberOfKeys, names, pins);
      }
      else
      {
          serialStream->println("Error: Unrecognized command");
      }
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
      if ((buttonsList[i].colorWhenPressed.r != buttonsList[i].colorWhenNotPressed.r) || \
      (buttonsList[i].colorWhenPressed.g != buttonsList[i].colorWhenNotPressed.g) || \
      (buttonsList[i].colorWhenPressed.b != buttonsList[i].colorWhenNotPressed.b))
      {
        if (digitalRead(buttonsList[i].pin) == LOW)
        {
          #ifdef ENABLE_BUTTON_ECHO
            serialStream.print(buttonsList[i].comName);
            serialStream.println(" pressed");
          #endif
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
            // Maybe find a better/less expensive way to do this check ?
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
