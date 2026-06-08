// usbds_transport.cpp - see usbds_transport.h. libnx usb:ds bulk interface setup.
#include "usbds_transport.h"
#include <malloc.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

#define USBCHK(call, label) do { Result _rc=(call); if (R_FAILED(_rc)) { \
    printf("[usb] %s rc=0x%08X (mod=%u desc=%u)\n", label, _rc, R_MODULE(_rc), R_DESCRIPTION(_rc)); \
    usbDsExit(); return false; } } while(0)

namespace dbi::usb {

static constexpr size_t BOUNCE = 0x100000; // 1 MiB, page-aligned

// Standard USB 3.0 Binary Object Store (USB 2.0 ext + SuperSpeed device cap).
static const uint8_t BOS[] = {
    0x05,0x0F,0x16,0x00,0x02,                       // BOS header, 2 device caps
    0x07,0x10,0x02,0x02,0x00,0x00,0x00,0x00,        // USB 2.0 extension
    0x0A,0x10,0x03,0x00,0x0E,0x00,0x01,0x00,0x00,0x00 // SuperSpeed device cap
};

static void devDesc(struct usb_device_descriptor* d, uint16_t bcdUSB, uint8_t maxPkt0) {
    std::memset(d, 0, sizeof(*d));
    d->bLength=USB_DT_DEVICE_SIZE; d->bDescriptorType=USB_DT_DEVICE; d->bcdUSB=bcdUSB;
    d->bDeviceClass=0; d->bDeviceSubClass=0; d->bDeviceProtocol=0; d->bMaxPacketSize0=maxPkt0;
    d->idVendor=0x057E; d->idProduct=0x3000; d->bcdDevice=0x0100;
    d->iManufacturer=1; d->iProduct=2; d->iSerialNumber=3; d->bNumConfigurations=1;
}

static void epDesc(struct usb_endpoint_descriptor* e, uint8_t addr, uint16_t mps) {
    std::memset(e,0,sizeof(*e));
    e->bLength=USB_DT_ENDPOINT_SIZE; e->bDescriptorType=USB_DT_ENDPOINT;
    e->bEndpointAddress=addr; e->bmAttributes=USB_TRANSFER_TYPE_BULK; e->wMaxPacketSize=mps;
}

static Result appendSpeed(UsbDsInterface* iface, UsbDeviceSpeed speed, uint16_t mps, bool ss,
                          uint8_t ifnum, uint8_t epInAddr, uint8_t epOutAddr) {
    struct usb_interface_descriptor id; std::memset(&id,0,sizeof(id));
    id.bLength=USB_DT_INTERFACE_SIZE; id.bDescriptorType=USB_DT_INTERFACE;
    id.bInterfaceNumber=ifnum; id.bNumEndpoints=2;
    id.bInterfaceClass=0xFF; id.bInterfaceSubClass=0xFF; id.bInterfaceProtocol=0xFF;
    struct usb_endpoint_descriptor ein, eout;
    epDesc(&ein, epInAddr,  mps);
    epDesc(&eout,epOutAddr, mps);
    Result rc;
    if (R_FAILED(rc=usbDsInterface_AppendConfigurationData(iface, speed, &id, USB_DT_INTERFACE_SIZE))) return rc;
    if (R_FAILED(rc=usbDsInterface_AppendConfigurationData(iface, speed, &ein, USB_DT_ENDPOINT_SIZE))) return rc;
    if (ss) { struct usb_ss_endpoint_companion_descriptor c={USB_DT_SS_ENDPOINT_COMPANION_SIZE,USB_DT_SS_ENDPOINT_COMPANION,0,0,0};
              if (R_FAILED(rc=usbDsInterface_AppendConfigurationData(iface, speed, &c, sizeof(c)))) return rc; }
    if (R_FAILED(rc=usbDsInterface_AppendConfigurationData(iface, speed, &eout, USB_DT_ENDPOINT_SIZE))) return rc;
    if (ss) { struct usb_ss_endpoint_companion_descriptor c={USB_DT_SS_ENDPOINT_COMPANION_SIZE,USB_DT_SS_ENDPOINT_COMPANION,0,0,0};
              if (R_FAILED(rc=usbDsInterface_AppendConfigurationData(iface, speed, &c, sizeof(c)))) return rc; }
    return 0;
}

bool UsbDsTransport::init() {
    USBCHK(usbDsInitialize(), "usbDsInitialize");
    u8 idx;
    usbDsAddUsbStringDescriptor(&idx, "open-dbi");       // 1 manufacturer
    usbDsAddUsbStringDescriptor(&idx, "Open-DBI USB");   // 2 product
    usbDsAddUsbStringDescriptor(&idx, "0001");           // 3 serial

    struct usb_device_descriptor full, high, super;
    devDesc(&full, 0x0110, 0x40); devDesc(&high, 0x0200, 0x40); devDesc(&super, 0x0300, 0x09);
    USBCHK(usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_Full,  &full),  "setDevDesc(full)");
    USBCHK(usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_High,  &high),  "setDevDesc(high)");
    USBCHK(usbDsSetUsbDeviceDescriptor(UsbDeviceSpeed_Super, &super), "setDevDesc(super)");
    USBCHK(usbDsSetBinaryObjectStore(BOS, sizeof(BOS)), "setBOS");
    USBCHK(usbDsClearDeviceData(), "clearDeviceData");

    USBCHK(usbDsRegisterInterface(&iface_), "registerInterface");
    // usb:ds assigns the interface index; endpoint addresses are index+1 (libnx convention),
    // used identically in both the config descriptors and RegisterEndpoint.
    uint8_t ifnum    = iface_->interface_index;
    uint8_t epInAddr = (uint8_t)(USB_ENDPOINT_IN  + ifnum + 1);
    uint8_t epOutAddr= (uint8_t)(USB_ENDPOINT_OUT + ifnum + 1);
    printf("[usb] interface_index=%u epIn=0x%02X epOut=0x%02X\n", ifnum, epInAddr, epOutAddr);
    USBCHK(appendSpeed(iface_, UsbDeviceSpeed_Full,  0x40,  false, ifnum, epInAddr, epOutAddr), "appendFull");
    USBCHK(appendSpeed(iface_, UsbDeviceSpeed_High,  0x200, false, ifnum, epInAddr, epOutAddr), "appendHigh");
    USBCHK(appendSpeed(iface_, UsbDeviceSpeed_Super, 0x400, true,  ifnum, epInAddr, epOutAddr), "appendSuper");
    USBCHK(usbDsInterface_RegisterEndpoint(iface_, &epIn_,  epInAddr),  "regEpIn");
    USBCHK(usbDsInterface_RegisterEndpoint(iface_, &epOut_, epOutAddr), "regEpOut");
    USBCHK(usbDsInterface_EnableInterface(iface_), "enableInterface");
    USBCHK(usbDsEnable(), "usbDsEnable");

    bounce_ = memalign(0x1000, BOUNCE);
    if (!bounce_) { printf("[usb] bounce alloc failed\n"); return false; }
    return true;
}

bool UsbDsTransport::waitConfigured(uint64_t timeout_ns) {
    Event* e = usbDsGetStateChangeEvent();
    UsbState st;
    for (;;) {
        if (R_SUCCEEDED(usbDsGetState(&st)) && st == UsbState_Configured) return true;
        if (R_FAILED(eventWait(e, timeout_ns))) return false;
        eventClear(e);
    }
}

size_t UsbDsTransport::postChunk(UsbDsEndpoint* ep, void* page, size_t len, uint64_t timeout) {
    u32 urb=0;
    if (R_FAILED(usbDsEndpoint_PostBufferAsync(ep, page, len, &urb))) return 0;
    if (R_FAILED(eventWait(&ep->CompletionEvent, timeout))) {
        usbDsEndpoint_Cancel(ep); return 0;
    }
    eventClear(&ep->CompletionEvent);
    UsbDsReportData rd; if (R_FAILED(usbDsEndpoint_GetReportData(ep, &rd))) return 0;
    u32 req=0, done=0; usbDsParseReportData(&rd, urb, &req, &done);
    return done;
}

size_t UsbDsTransport::write(const void* src, size_t len) {
    const uint8_t* p = (const uint8_t*)src; size_t total=0;
    while (total < len) {
        size_t n = std::min(len-total, BOUNCE);
        std::memcpy(bounce_, p+total, n);
        size_t done = postChunk(epIn_, bounce_, n, UINT64_MAX); // host always drains IN
        if (done==0) break;
        total += done;
    }
    return total;
}

size_t UsbDsTransport::read(void* dst, size_t len) {
    uint8_t* p = (uint8_t*)dst; size_t total=0;
    while (total < len) {
        size_t n = std::min(len-total, BOUNCE);
        size_t done = postChunk(epOut_, bounce_, n, readTimeout_);
        if (done==0) break;                 // timeout or short -> stop
        std::memcpy(p+total, bounce_, done);
        total += done;
        if (done < n) break;                // short packet = end of host data
    }
    return total;
}

void UsbDsTransport::exit() {
    if (bounce_) { free(bounce_); bounce_=nullptr; }
    usbDsExit();
}

} // namespace dbi::usb
