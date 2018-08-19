/**
 * Human interface device driver
 *
 * Kevin Cuzner
 */

#include "usb_hid.h"

/**
 * <include>usb_hid.h</include>
 * <descriptor id="hid_interface" type="0x04" childof="configuration">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <index name="bInterfaceNumber" size="1" />
 *  <byte name="bAlternateSetting">0</byte>
 *  <count name="bNumEndpoints" type="0x05" associated="associated" size="1" />
 *  <byte name="bInterfaceClass">0x03</byte>
 *  <byte name="bInterfaceSubClass">0x00</byte>
 *  <byte name="bInterfaceProtocol">0x00</byte>
 *  <byte name="iInterface">0</byte>
 *  <children type="0x21" />
 *  <children type="0x05" />
 * </descriptor>
 * <descriptor id="hid" type="0x21" childof="hid_interface">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <word name="bcdHID">0x0111</word>
 *  <byte name="bCountryCode">0x00</byte>
 *  <count name="bNumDescriptors" type="0x22" size="1" associated="associated" />
 *  <foreach type="0x22" associated="associated">
 *    <echo name="bDescriptorType" />
 *    <echo name="wLength" />
 *  </foreach>
 * </descriptor>
 * <descriptor id="hid_in_endpoint" type="0x05" childof="hid_interface">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <inendpoint name="bEndpointAddress" define="HID_IN_ENDPOINT" />
 *  <byte name="bmAttributes">0x03</byte>
 *  <word name="wMaxPacketSize">USB_HID_ENDPOINT_SIZE</word>
 *  <byte name="bInterval">10</byte>
 * </descriptor>
 * <descriptor id="hid_out_endpoint" type="0x05" childof="hid_interface">
 *  <length name="bLength" size="1" />
 *  <type name="bDescriptorType" size="1" />
 *  <outendpoint name="bEndpointAddress" define="HID_OUT_ENDPOINT" />
 *  <byte name="bmAttributes">0x03</byte>
 *  <word name="wMaxPacketSize">USB_HID_ENDPOINT_SIZE</word>
 *  <byte name="bInterval">10</byte>
 * </descriptor>
 * <descriptor id="hid_report" childof="hid" top="top" type="0x22">
 *  <hidden name="bDescriptorType" size="1">0x22</hidden>
 *  <hidden name="wLength" size="2">sizeof(hid_report)</hidden>
 *  <raw>
 *  HID_SHORT(0x04, 0x00, 0xFF), //USAGE_PAGE (Vendor Defined)
 *  HID_SHORT(0x08, 0x01), //USAGE (Vendor 1)
 *  HID_SHORT(0xa0, 0x01), //COLLECTION (Application)
 *  HID_SHORT(0x08, 0x01), //  USAGE (Vendor 1)
 *  HID_SHORT(0x14, 0x00), //  LOGICAL_MINIMUM (0)
 *  HID_SHORT(0x24, 0xFF, 0x00), //LOGICAL_MAXIMUM (0x00FF)
 *  HID_SHORT(0x74, 0x08), //  REPORT_SIZE (8)
 *  HID_SHORT(0x94, 64), //  REPORT_COUNT(64)
 *  HID_SHORT(0x80, 0x02), //  INPUT (Data, Var, Abs)
 *  HID_SHORT(0x08, 0x01), //  USAGE (Vendor 1)
 *  HID_SHORT(0x90, 0x02), //  OUTPUT (Data, Var, Abs)
 *  HID_SHORT(0xc0),       //END_COLLECTION
 *  </raw>
 * </descriptor>
 */

#define HID_IN_ENDPOINT 1
#define HID_OUT_ENDPOINT 2

void __attribute__((weak)) hook_usb_hid_configured(void) { }
void __attribute__((weak)) hook_usb_hid_out_report(const USBTransferData *report) { }
void __attribute__((weak)) hook_usb_hid_in_report_sent(const USBTransferData *report) { }
void __attribute__((weak)) hook_usb_hid_out_report_received(const USBTransferData *report) { }

void usb_hid_send(const USBTransferData *report)
{
    usb_endpoint_send(HID_IN_ENDPOINT, report->addr, report->len);
}

void usb_hid_receive(const USBTransferData *report)
{
    usb_endpoint_receive(HID_OUT_ENDPOINT, report->addr, report->len);
}

/**
 * Implementation of hook_usb_handle_setup_request which implements HID class
 * requests
 */
static USBAppControlResult hid_usb_handle_setup_request(USBSetupPacket const *setup, USBTransferData *nextTransfer)
{
    switch (setup->wRequestAndType)
    {
        /*case USB_REQ(0x01, USB_REQ_DIR_IN | USB_REQ_TYPE_CLS | USB_REQ_RCP_IFACE):
            //Get report request
            nextTransfer->addr = report_in;
            nextTransfer->len = sizeof(report_in);
            return USB_CTL_OK;*/
        case USB_REQ(0x0A, USB_REQ_DIR_OUT | USB_REQ_TYPE_CLS | USB_REQ_RCP_IFACE):
            return USB_CTL_OK;
    }
    return USB_APP_CTL_UNHANDLED;
}

static void hid_usb_set_configuration(uint16_t configuration)
{
    usb_endpoint_setup(HID_IN_ENDPOINT, 0x81, USB_HID_ENDPOINT_SIZE, USB_ENDPOINT_INTERRUPT, USB_FLAGS_NOZLP);
    usb_endpoint_setup(HID_OUT_ENDPOINT, 0x02, USB_HID_ENDPOINT_SIZE, USB_ENDPOINT_INTERRUPT, USB_FLAGS_NOZLP);

    hook_usb_hid_configured();
}

static void hid_usb_endpoint_sent(uint8_t endpoint, void *buf, uint16_t len)
{
    USBTransferData report = { buf, len };
    if (endpoint == HID_IN_ENDPOINT)
    {
        hook_usb_hid_in_report_sent(&report);
    }
}

static void hid_usb_endpoint_received(uint8_t endpoint, void *buf, uint16_t len)
{
    USBTransferData report = { buf, len };
    if (endpoint == HID_OUT_ENDPOINT)
    {
        hook_usb_hid_out_report_received(&report);
    }
}

const USBInterface hid_interface = {
    .hook_usb_handle_setup_request = &hid_usb_handle_setup_request,
    .hook_usb_set_configuration = &hid_usb_set_configuration,
    .hook_usb_endpoint_sent = &hid_usb_endpoint_sent,
    .hook_usb_endpoint_received = &hid_usb_endpoint_received,
};

