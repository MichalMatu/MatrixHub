/*
 * USBHIDConsumerControl.h
 * 
 * Wrapper for HID Consumer Control (Media Keys)
 */

#pragma once

#include "soc/soc_caps.h"
#if SOC_USB_OTG_SUPPORTED

#include "USBHID.h"

// Consumer Page (0x0C) Usage IDs
#define CONSUMER_POWER          0x0030
#define CONSUMER_RESET          0x0031
#define CONSUMER_SLEEP          0x0032

#define CONSUMER_MENU           0x0040
#define CONSUMER_SELECTION      0x0080
#define CONSUMER_ASSIGN_SEL     0x0081
#define CONSUMER_MODE_STEP      0x0082
#define CONSUMER_RECALL_LAST    0x0083
#define CONSUMER_ENTER_CHANNEL  0x0084
#define CONSUMER_CHANNEL_UP     0x009C
#define CONSUMER_CHANNEL_DOWN   0x009D

#define CONSUMER_PLAY           0x00B0
#define CONSUMER_PAUSE          0x00B1
#define CONSUMER_RECORD         0x00B2
#define CONSUMER_FAST_FORWARD   0x00B3
#define CONSUMER_REWIND         0x00B4
#define CONSUMER_SCAN_NEXT      0x00B5
#define CONSUMER_SCAN_PREV      0x00B6
#define CONSUMER_STOP           0x00B7
#define CONSUMER_EJECT          0x00B8
#define CONSUMER_RANDOM_PLAY    0x00B9
#define CONSUMER_REPEAT         0x00BC
#define CONSUMER_TRACK_NORMAL   0x00BE
#define CONSUMER_PLAY_PAUSE     0x00CD
#define CONSUMER_PLAY_SKIP      0x00CE

#define CONSUMER_VOLUME         0x00E0
#define CONSUMER_BALANCE        0x00E1
#define CONSUMER_MUTE           0x00E2
#define CONSUMER_BASS           0x00E3
#define CONSUMER_TREBLE         0x00E4
#define CONSUMER_BASS_BOOST     0x00E5
#define CONSUMER_VOLUME_UP      0x00E9
#define CONSUMER_VOLUME_DOWN    0x00EA

#define CONSUMER_EMAIL          0x018A
#define CONSUMER_CALCULATOR     0x0192
#define CONSUMER_LOCAL_BROWSER  0x0194

#define CONSUMER_WWW_SEARCH     0x0221
#define CONSUMER_WWW_HOME       0x0223
#define CONSUMER_WWW_BACK       0x0224
#define CONSUMER_WWW_FORWARD    0x0225
#define CONSUMER_WWW_STOP       0x0226
#define CONSUMER_WWW_REFRESH    0x0227
#define CONSUMER_WWW_BOOKMARKS  0x022A

class USBHIDConsumerControl : public USBHIDDevice {
private:
  USBHID hid;

public:
  USBHIDConsumerControl(void);
  void begin(void);
  void end(void);
  
  bool press(uint16_t usage);
  bool release(void);
  
  // Internal use
  uint16_t _onGetDescriptor(uint8_t *buffer);
};

#endif /* SOC_USB_OTG_SUPPORTED */
