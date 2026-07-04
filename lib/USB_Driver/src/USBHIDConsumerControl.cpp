/*
 * USBHIDConsumerControl.cpp
 * 
 * Wrapper for HID Consumer Control (Media Keys)
 */

#include "USBHID.h"

#if SOC_USB_OTG_SUPPORTED
#if CONFIG_TINYUSB_HID_ENABLED

#include "USBHIDConsumerControl.h"

static const uint8_t report_descriptor[] = {
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(HID_REPORT_ID_CONSUMER_CONTROL))
};

USBHIDConsumerControl::USBHIDConsumerControl() : hid(HID_ITF_PROTOCOL_NONE) {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    hid.addDevice(this, sizeof(report_descriptor));
  }
}

void USBHIDConsumerControl::begin() {
  hid.begin();
}

void USBHIDConsumerControl::end() {
}

uint16_t USBHIDConsumerControl::_onGetDescriptor(uint8_t *dst) {
  memcpy(dst, report_descriptor, sizeof(report_descriptor));
  return sizeof(report_descriptor);
}

bool USBHIDConsumerControl::press(uint16_t usage) {
  return hid.SendReport(HID_REPORT_ID_CONSUMER_CONTROL, &usage, sizeof(usage));
}

bool USBHIDConsumerControl::release() {
  uint16_t zero = 0;
  return hid.SendReport(HID_REPORT_ID_CONSUMER_CONTROL, &zero, sizeof(zero));
}

#endif /* CONFIG_TINYUSB_HID_ENABLED */
#endif /* SOC_USB_OTG_SUPPORTED */
