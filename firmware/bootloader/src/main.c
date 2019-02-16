/**
 * STM32L052X8 Wristwatch Firmware
 *
 * Kevin Cuzner
 */

#include "stm32f0xx.h"

#include "usb.h"
#include "osc.h"
#include "usb_hid.h"
#include "bootloader.h"

#include <stddef.h>

/**
 * <descriptor id="device" type="0x01">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <word name="bcdUSB">0x0110</word>
 *  <byte name="bDeviceClass">0</byte>
 *  <byte name="bDeviceSubClass">0</byte>
 *  <byte name="bDeviceProtocol">0</byte>
 *  <byte name="bMaxPacketSize0">USB_CONTROL_ENDPOINT_SIZE</byte>
 *  <word name="idVendor">0x16c0</word>
 *  <word name="idProduct">0x05dc</word>
 *  <word name="bcdDevice">0x0010</word>
 *  <ref name="iManufacturer" type="0x03" refid="manufacturer" size="1" />
 *  <ref name="iProduct" type="0x03" refid="product" size="1" />
 *  <byte name="iSerialNumber">0</byte>
 *  <count name="bNumConfigurations" type="0x02" size="1" />
 * </descriptor>
 * <descriptor id="lang" type="0x03" first="first">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <foreach type="0x03" unique="unique">
 *    <echo name="wLang" />
 *  </foreach>
 * </descriptor>
 * <descriptor id="manufacturer" type="0x03" wIndex="0x0409">
 *  <hidden name="wLang" size="2">0x0409</hidden>
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <string name="wString">kevincuzner.com</string>
 * </descriptor>
 * <descriptor id="product" type="0x03" wIndex="0x0409">
 *  <hidden name="wLang" size="2">0x0409</hidden>
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <string name="wString">Midi-Fader Bootloader</string>
 * </descriptor>
 * <descriptor id="configuration" type="0x02">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <length name="wTotalLength" size="2" all="all" />
 *  <count name="bNumInterfaces" type="0x04" associated="associated" size="1" />
 *  <byte name="bConfigurationValue">1</byte>
 *  <byte name="iConfiguration">0</byte>
 *  <byte name="bmAttributes">0x80</byte>
 *  <byte name="bMaxPower">250</byte>
 *  <children type="0x04" />
 * </descriptor>
 */

typedef struct __attribute__((packed))
{
    uint8_t data[8];
} WristwatchReport;

static volatile uint8_t segment = 0;

static const USBInterfaceListNode hid_interface_node = {
    .interface = &hid_interface,
    .next = NULL,
};

const USBApplicationSetup setup = {
    .interface_list = &hid_interface_node,
};

const USBApplicationSetup *usb_app_setup = &setup;

int main(void)
{
    bootloader_init();

    SystemCoreClockUpdate();

    usb_init();

    osc_request_hsi8();
    usb_enable();

    bootloader_run();

    while (1) { }

    return 0;
}

void TIM2_IRQHandler()
{
    TIM2->SR = 0;
}

