// Playwrite dock software (for Teenzy 4.1)
// part of the Playwrite project 
#include <Arduino.h>
#include "USBHost_t36.h"
#include "CircularBuffer.h"
// #define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#endif

const char *PLAYDATE_KEYBOARD_ENABLE_MESSAGE = "KeyboardInputEnable";
const char *PLAYDATE_KEYBOARD_DISABLE_MESSAGE = "KeyboardInputDisable";
const char *PLAYDATE_READY_FOR_KEYSTROKE_MESSAGE = "ReadyForNextInput";
const int PLAYDATE_KEYBOARD_INPUT_ANGLE_OFFSET = 100;

const char *PLAYDATE_PRODUCT_NAME = "Playdate";
const unsigned int MAX_INPUT = 50;
uint32_t baud = 115200;
USBHost myusb;
USBHub hub1(myusb);
KeyboardController keyboard1(myusb);
JoystickController joystick1(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBSerial_BigBuffer userial(myusb, 1);

USBDriver *drivers[] = {&hub1, &hid1, &hid2, &userial};
#define CNT_DEVICES (sizeof(drivers) / sizeof(drivers[0]))
const char *driver_names[CNT_DEVICES] = {"Hub1", "HID1", "HID2", "USERIAL1"};
bool driver_active[CNT_DEVICES] = {false, false, false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&keyboard1, &joystick1};
#define CNT_HIDDEVICES (sizeof(hiddrivers) / sizeof(hiddrivers[0]))
const char *hid_driver_names[CNT_HIDDEVICES] = {"Keyboard", "Joystick"};
bool hid_driver_active[CNT_HIDDEVICES] = {false, false};

CircularBuffer<String, 100> playdateMessageQueue;
bool playdateReadyForKeyboard = false;
bool playdateBusy = false;

void ShowUpdatedDeviceListInfo()
{
  for (uint8_t i = 0; i < CNT_DEVICES; i++)
  {
    if (*drivers[i] != driver_active[i])
    {
      if (driver_active[i])
      {
#ifdef DEBUG
        Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
#endif
        driver_active[i] = false;
      }
      else
      {
        driver_active[i] = true;
        const uint8_t *product = drivers[i]->product();
#ifdef DEBUG
        Serial.printf("*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz)
          Serial.printf("  manufacturer: %s\n", psz);
        if (product && *product)
          Serial.printf("  product: %s\n", product);
        psz = drivers[i]->serialNumber();
        if (psz && *psz)
          Serial.printf("  Serial: %s\n", psz);
#endif

        // Note: with some keyboards there is an issue that they don't output in boot protocol mode
        // and may not work.  The above code can try to force the keyboard into boot mode, but there
        // are issues with doing this blindly with combo devices like wireless keyboard/mouse, which
        // may cause the mouse to not work.  Note: the above id is in the builtin list of
        // vendor IDs that are already forced
        //         if (drivers[i] == &keyboard1)
        //         {
        //           if (keyboard1.idVendor() == 0x04D9) // 0x46D
        //           {
        // #ifdef DEBUG
        //             Serial.println("Gigabyte vendor: force boot protocol");
        // #endif
        //             // Gigabyte keyboard
        //             keyboard1.forceBootProtocol();
        //           }
        //         }
        // If this is a new Serial device.
        if (drivers[i] == &userial && strcmp((const char *)product, PLAYDATE_PRODUCT_NAME) == 0)
        {
#ifdef DEBUG
          Serial.println("\n  Connected to Playdate Serial, yay!");
#endif
          // Lets try first outputting something to our USerial to see if it will go out...
          userial.begin(baud);
        }
      }
    }
  }
  // Then Hid Devices
  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++)
  {
    if (*hiddrivers[i] != hid_driver_active[i])
    {
      if (hid_driver_active[i])
      {
        Serial.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      }
      else
      {
        const uint8_t *product = hiddrivers[i]->product();
        Serial.printf("*** HID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;
        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz)
          Serial.printf("  manufacturer: %s\n", psz);
        if (product && *product)
        {
          Serial.printf("  product: %s\n", product);
        }
        psz = drivers[i]->serialNumber();
        if (psz && *psz)
          Serial.printf("  Serial: %s\n", psz);
        // keyboard1.forceBootProtocol();
      }
    }
  }
}

void OnPress(int key)
{
  // Exit early if keyboard mode isn't enabled.
  if (!playdateReadyForKeyboard)
  {
    return;
  }

  // Arrow keys handled by raw callbacks
  if (key >= 215 && key <= 218)
  {
    return;
  }

  // Also ignore Esc, Return, Backspace, Delete, Caps lock
  if (key == 27 || key == 10 || key == 127 || key == 212 || key == 0)
  {
    return;
  }

  Serial.print("\n\npressed ");
  Serial.print((char)key);
  Serial.print(" - ");
  Serial.println(key);

  if (playdateReadyForKeyboard && (key + PLAYDATE_KEYBOARD_INPUT_ANGLE_OFFSET) < 359)
  {
    Serial.print("changecrank ");
    Serial.println(key + PLAYDATE_KEYBOARD_INPUT_ANGLE_OFFSET);
    playdateMessageQueue.push("changecrank " + (String)(key + PLAYDATE_KEYBOARD_INPUT_ANGLE_OFFSET));
    //userial.print("changecrank ");
    //userial.println(key + 32); 
  }
  // Serial.print("'  ");
  // Serial.print(key);
  // Serial.print(" MOD: ");
  // Serial.print(keyboard1.getModifiers(), HEX);
  // Serial.print(" OEM: ");
  // Serial.print(keyboard1.getOemKey(), HEX);
  // Serial.print(" LEDS: ");
  // Serial.println(keyboard1.LEDS(), HEX);
}

#define RAW_RIGHT_ARROW 0x4F
#define RAW_LEFT_ARROW 0x50
#define RAW_DOWN_ARROW 0x51
#define RAW_UP_ARROW 0x52
#define RAW_ENTER 0x28
#define RAW_BACKSPACE 0x2a
#define RAW_ESCAPE 0x29

String rawKeyToBtn(uint8_t keycode)
{
  if (!playdateReadyForKeyboard)
  {
    switch (keycode)
    {
    case 0x16: // S key
      return "a";
      break;
    case 0x04: // A key in QWERTY
    case 0x14: // A key in AZERTY
      return "b";
      break;
    }
  }
  switch (keycode)
  {
  case RAW_RIGHT_ARROW:
    return "right";
    break;
  case RAW_LEFT_ARROW:
    return "left";
    break;
  case RAW_DOWN_ARROW:
    return "down";
    break;
  case RAW_UP_ARROW:
    return "up";
    break;
  case RAW_ENTER:
    return "a";
    break;
  case RAW_BACKSPACE:
    return "b";
    break;
  case RAW_ESCAPE:
    return "menu";
    break;
  default:
    return "n/a";
    break;
  }
}

void OnRawPress(uint8_t keycode)
{
#ifdef DEBUG
  Serial.println();
  Serial.print("raw press ");
  Serial.println(keycode);
#endif
  String pd = rawKeyToBtn(keycode);
  if (pd != "n/a")
  {
    userial.println("btn +" + (String)pd);
    DEBUG_PRINT(" >> btn +" + (String)pd);
  }
}

void OnRawRelease(uint8_t keycode)
{
#ifdef DEBUG
  Serial.println();
  Serial.print("raw release ");
  Serial.println(keycode);
#endif
  String pd = rawKeyToBtn(keycode);
  if (pd != "n/a")
  {
    userial.println("btn -" + (String)pd);
    DEBUG_PRINT(" >> btn -" + (String)pd);
  }
}

void process_data(const char *data)
{
  Serial.print("[Playdate says] ");
  Serial.println(data);
  if (strcmp(data, PLAYDATE_READY_FOR_KEYSTROKE_MESSAGE) == 0)
  {
    playdateBusy = false;
  }
  if (strcmp(data, PLAYDATE_KEYBOARD_ENABLE_MESSAGE) == 0)
  {
    playdateReadyForKeyboard = true;
#ifdef DEBUG
    digitalWrite(13, HIGH);
    Serial.println("Enabling keyboard input forwarding");
#endif
  }
  else if (strcmp(data, PLAYDATE_KEYBOARD_DISABLE_MESSAGE) == 0)
  {
    playdateReadyForKeyboard = false;
#ifdef DEBUG
    digitalWrite(13, LOW);
    Serial.println("Disabling keyboard input forwarding");
#endif
  }
}

// from http://www.gammon.com.au/serial
void processIncomingByte(const byte inByte)
{
  static char input_line[MAX_INPUT];
  static unsigned int input_pos = 0;

  switch (inByte)
  {
  case '\n':                   // end of text
    input_line[input_pos] = 0; // terminating null byte
    // terminator reached! process input_line here ...
    process_data(input_line);
    // reset buffer for next time
    input_pos = 0;
    break;
  case '\r': // discard carriage return
    break;
  default:
    // keep adding if not full ... allow for terminating null byte
    if (input_pos < (MAX_INPUT - 1))
      input_line[input_pos++] = inByte;
    break;
  } // end of switch

} // end of processIncomingByte

void setup()
{
  pinMode(13, OUTPUT);
  myusb.begin();
#ifdef DEBUG
  while (!Serial)
    ; // wait for Arduino Serial Monitor
  Serial.println("\n\nPLaydate Serial Adapter - debug mode");
  // Serial.println(sizeof(USBHub), DEC);
#endif
  keyboard1.attachPress(OnPress);
  keyboard1.attachRawPress(OnRawPress);
  keyboard1.attachRawRelease(OnRawRelease);

  // Only notify basic axis changes
  joystick1.axisChangeNotifyMask(0x3ff);
}

uint32_t prevButtons = 0;

enum IAXIS
{
  AXIS_UP = 0,
  AXIS_DOWN,
  AXIS_LEFT,
  AXIS_RIGHT
};
bool prevAxis9[4] = {false, false, false, false};

void processButton(uint32_t buttons, uint32_t prev, u_int32_t mask, String key)
{
  if ((buttons & mask))
  {
    if (!(prevButtons & mask))
    {
      userial.println("btn +" + key);
      Serial.println("btn +" + key);
    }
  }
  else if (prevButtons & mask)
  {
    userial.println("btn -" + key);
    Serial.println("btn -" + key);
  }
}

void processAxis(int axisValue, String lowValue, String highValue)
{
  if (axisValue == 0)
  {
    userial.println("btn +" + lowValue);
    Serial.println("btn +" + lowValue);
  }
  else if (axisValue == 255)
  {
    userial.println("btn +" + highValue);
    Serial.println("btn +" + highValue);
  }
  else if (axisValue == 128)
  {
    userial.println("btn -" + lowValue);
    Serial.println("btn -" + lowValue);
    userial.println("btn -" + highValue);
    Serial.println("btn -" + highValue);
  }
}

void process8bitdoAxis(int axisValue)
{
  bool axis9[4] = {false, false, false, false};
  axis9[AXIS_UP] = axisValue == 7 || axisValue == 0 || axisValue == 1;
  axis9[AXIS_RIGHT] = axisValue == 1 || axisValue == 2 || axisValue == 3;
  axis9[AXIS_DOWN] = axisValue == 3 || axisValue == 4 || axisValue == 5;
  axis9[AXIS_LEFT] = axisValue == 5 || axisValue == 6 || axisValue == 7;
  for (int i = 0; i < 4; i++)
  {
    if (axis9[i] != prevAxis9[i])
    {
      String key;
      switch (i)
      {
      case AXIS_UP:
        key = "up";
        break;
      case AXIS_DOWN:
        key = "down";
        break;
      case AXIS_LEFT:
        key = "left";
        break;
      case AXIS_RIGHT:
        key = "right";
        break;
      }
      if (axis9[i] == true)
      {
        userial.println("btn +" + key);
        DEBUG_PRINT("btn +" + key);
      }
      else
      {
        userial.println("btn -" + key);
        DEBUG_PRINT("btn -" + key);
      }
      prevAxis9[i] = axis9[i];
    }
  }
}

void loop()
{
  myusb.Task();
  ShowUpdatedDeviceListInfo();

#ifdef DEBUG
  if (Serial.available())
  {
    int ch = Serial.read(); // get the first char.
    while (Serial.read() != -1)
      ;
    if ((ch == 'k') || (ch == 'K'))
    {
      Serial.print("Toggle keyboard mode: ");
      playdateReadyForKeyboard = !playdateReadyForKeyboard;
      Serial.println(playdateReadyForKeyboard);
    }
  }
#endif

  while (userial.available() > 0)
    processIncomingByte(userial.read());

  if (joystick1.available() && joystick1.joystickType() != JoystickController::PS4)
  {
    // switch (joystick1.joystickType()) {
    // case JoystickController::PS4:
    // case JoystickController::PS3:
    // case JoystickController::XBOXONE:
    // case JoystickController::XBOX360:
    // }
    uint64_t axis_changed_mask = joystick1.axisChangedMask();
    uint32_t buttons = joystick1.getButtons();
    processButton(buttons, prevButtons, 0x0001, "a");    // A
    processButton(buttons, prevButtons, 0x0008, "b");    // X
    processButton(buttons, prevButtons, 0x0002, "b");    // B
    processButton(buttons, prevButtons, 0x0010, "a");    // Y
    processButton(buttons, prevButtons, 0x0800, "menu"); // Start
    processButton(buttons, prevButtons, 0x0400, "lock"); // Select

    for (uint8_t i = 0; axis_changed_mask != 0; i++, axis_changed_mask >>= 1)
    {
      if (axis_changed_mask & 1)
      {
        int axisValue = joystick1.getAxis(i);
        Serial.printf(" %d:%d", i, axisValue);
        switch (i)
        {
        case 0:
          processAxis(axisValue, "left", "right");
          break;
        case 1:
          processAxis(axisValue, "up", "down");
          break;
        case 9:
          process8bitdoAxis(axisValue);
          break;
        }
      }
    }
    prevButtons = buttons;
    joystick1.joystickDataClear();
  }

  if (!playdateBusy && !playdateMessageQueue.isEmpty())
  {
    playdateBusy = true;
    userial.println(playdateMessageQueue.shift());
  }
}
