// usbds_transport.h - a BulkTransport backed by libnx usb:ds.
//
// Presents a vendor-specific bulk interface as VID 0x057E / PID 0x3000 with one IN
// + one OUT endpoint, matching what the `dbibackend` host scans for. Implemented
// from the USB spec + the libnx usb:ds API (not from decompiled code).
#pragma once
#include <switch.h>
#include "transport.h"

namespace dbi::usb {

class UsbDsTransport : public BulkTransport {
public:
    bool init();                              // set up + enable the interface
    bool waitConfigured(uint64_t timeout_ns); // wait until the host configures us
    void exit();
    void setReadTimeout(uint64_t ns) { readTimeout_ = ns; }

    size_t write(const void* src, size_t len) override;   // bulk IN (device->host)
    size_t read(void* dst, size_t len) override;          // bulk OUT (host->device)

private:
    size_t postChunk(UsbDsEndpoint* ep, void* page, size_t len, uint64_t timeout);
    UsbDsInterface* iface_ = nullptr;
    UsbDsEndpoint*  epIn_  = nullptr;   // 0x81
    UsbDsEndpoint*  epOut_ = nullptr;   // 0x00
    void*    bounce_ = nullptr;         // page-aligned transfer buffer
    uint64_t readTimeout_ = 2000000000ULL; // 2s
};

} // namespace dbi::usb
