// main.cpp - Open-DBI: console harness exercising the clean-room modules.
//
// Not the full DBI UI. It validates the reconstructed slices on-console: DBI0
// protocol framing, config parser, save-backup format, and the install state
// machine (driven through an in-memory NSP + a logging backend).
#include <switch.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

#include "proto/dbi0.h"
#include "config/config.h"
#include "saves/save_backup.h"
#include "io/content_source.h"
#include "fs/pfs0.h"
#include "install/installer.h"
#include "crypto/aes.h"
#include "fs/cnmt.h"
#include "usb/usbds_transport.h"
#include "usb/dbi0_client.h"
#include "io/file_source.h"
#include "crypto/keyset.h"
#include "crypto/nca.h"
#include "install/ncm_backend.h"

using namespace dbi;

// --- in-memory ContentSource ---
class MemorySource : public io::ContentSource {
public:
    explicit MemorySource(std::vector<uint8_t> d) : d_(std::move(d)) {}
    uint64_t size() const override { return d_.size(); }
    size_t read(void* dst, uint64_t off, size_t len) override {
        if (off >= d_.size()) return 0;
        size_t n = std::min<uint64_t>(len, d_.size() - off);
        std::memcpy(dst, d_.data() + off, n);
        return n;
    }
private:
    std::vector<uint8_t> d_;
};

// --- backend that just logs the install sequence ---
class LoggingBackend : public install::InstallBackend {
public:
    bool open(install::StorageId s) override { printf("  open(storage=%d)\n", (int)s); return true; }
    bool generatePlaceHolderId(install::ContentId& ph) override { ph.fill(0xAB); printf("  generatePlaceHolderId\n"); return true; }
    bool createPlaceHolder(const install::ContentId&, const install::ContentId&, uint64_t sz) override { printf("  createPlaceHolder(size=%llu)\n", (unsigned long long)sz); return true; }
    bool writePlaceHolder(const install::ContentId&, uint64_t off, const void*, size_t len) override { printf("  writePlaceHolder(off=%llu,len=%zu)\n", (unsigned long long)off, len); return true; }
    bool registerContent(const install::ContentId&, const install::ContentId&) override { printf("  registerContent\n"); return true; }
    bool importTicket(const void*, size_t tl, const void*, size_t) override { printf("  importTicket(len=%zu)\n", tl); return true; }
    bool setContentMeta(uint64_t id, uint32_t ver, uint8_t type, const void*, size_t bl) override {
        printf("  setContentMeta(id=%016llx ver=%u type=0x%02x blob=%zu)\n",
               (unsigned long long)id, ver, type, bl); return true; }
    bool commit() override { printf("  commit\n"); return true; }
    void closeStorage() override { printf("  closeStorage\n"); }
};

// Build a tiny valid PFS0/NSP in memory with one ".nca" entry.
static std::vector<uint8_t> buildFakeNsp() {
    const std::string name = "00112233445566778899aabbccddeeff.nca";
    const std::vector<uint8_t> data = {'N','C','A','!'}; // 4-byte fake NCA payload
    std::vector<uint8_t> strtab(name.begin(), name.end()); strtab.push_back(0);
    while (strtab.size() % 4) strtab.push_back(0);
    auto put32 = [](std::vector<uint8_t>& v, uint32_t x){ v.push_back(x); v.push_back(x>>8); v.push_back(x>>16); v.push_back(x>>24); };
    auto put64 = [](std::vector<uint8_t>& v, uint64_t x){ for(int i=0;i<8;i++) v.push_back((uint8_t)(x>>(8*i))); };
    std::vector<uint8_t> out;
    out.insert(out.end(), {'P','F','S','0'});
    put32(out, 1);                 // file count
    put32(out, (uint32_t)strtab.size());
    put32(out, 0);                 // reserved
    put64(out, 0);                 // entry: data offset (rel to data start)
    put64(out, data.size());       // entry: size
    put32(out, 0);                 // entry: name offset
    put32(out, 0);                 // entry: reserved
    out.insert(out.end(), strtab.begin(), strtab.end());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

static void selftest() {
    // DBI0 framing
    using namespace dbi::proto;
    uint8_t hdr[HEADER_SIZE];
    writeHeader(hdr, CmdType::Response, CmdId::FileRange, 0x100000);
    CmdType t; CmdId id; uint32_t sz;
    printf("[proto] parse=%d type=%u id=%u size=0x%X magic=%.4s\n",
           parseHeader(hdr, t, id, sz), (unsigned)t, (unsigned)id, sz, hdr);

    // config
    dbi::config::Config cfg;
    cfg.parse("[General]\nExitToHomeScreen=true\n;LogEvents=true\n[Install]\nCheckHash=true\n");
    printf("[config] ExitToHomeScreen=%d LogEvents(def)=%d CheckHash=%d\n",
           cfg.getBool("General","ExitToHomeScreen"), cfg.getBool("General","LogEvents"),
           cfg.getBool("Install","CheckHash"));

    // save-backup format
    dbi::saves::SaveInfo si; si.titleId=0x0100000000010000ULL; si.titleName="Demo";
    si.backupDate="2026-06-07"; si.account="user"; si.space=5;
    printf("[saves] %s\n", dbi::saves::backupFileName(si.titleId,si.space,"20260607_120000",0).c_str());

    // AES-128 correctness vs NIST FIPS-197 vector
    {
        const uint8_t key[16]={0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
        const uint8_t pt[16] ={0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
        const uint8_t ct[16] ={0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
        dbi::crypto::Aes128 a; a.setKey(key);
        uint8_t enc[16], dec[16]; a.encryptBlock(pt,enc); a.decryptBlock(enc,dec);
        printf("[crypto] AES-128 FIPS-197 enc=%s dec=%s\n",
               std::memcmp(enc,ct,16)==0?"PASS":"FAIL", std::memcmp(dec,pt,16)==0?"PASS":"FAIL");
    }

    // CNMT parser on a hand-built blob (no crypto needed)
    {
        std::vector<uint8_t> c(0x20 + 0x38, 0);
        auto w64=[&](size_t o,uint64_t v){ for(int i=0;i<8;i++) c[o+i]=(uint8_t)(v>>(8*i)); };
        w64(0x00, 0x0100000000010000ULL);     // title id
        c[0x08]=2; c[0x09]=0; c[0x0A]=0; c[0x0B]=0; // version=2
        c[0x0C]=(uint8_t)dbi::fs::ContentMetaType::Application;
        c[0x0E]=0; c[0x0F]=0;                 // ext header size 0
        c[0x10]=1;                            // content count 1
        // one content record at 0x20: 0x20 hash + info{id,size(6),type}
        c[0x20+0x20+22]=1;                    // content type = Program
        dbi::fs::Cnmt cn;
        bool ok=cn.parse(c.data(), c.size());
        printf("[cnmt] parse=%d titleId=%016llx ver=%u type=0x%02x contents=%zu\n",
               ok, (unsigned long long)cn.titleId, cn.version, cn.metaType, cn.contents.size());
    }

    // install state machine over an in-memory NSP
    printf("[install] installNsp() sequence:\n");
    MemorySource nsp(buildFakeNsp());
    LoggingBackend be;
    install::Installer inst(be);
    auto r = inst.installNsp(nsp, install::StorageId::SdCard);
    printf("[install] ok=%d ncas=%d tickets=%d err=%s\n",
           r.ok, r.ncasWritten, r.ticketsImported, r.error.c_str());
}

// Safe USB transfer test: connect to the PC `dbibackend`, LIST, then receive the
// first 0x200 bytes of the first file and save to SD. No ncm, no install.
static void usbReceiveTest() {
    printf("\n[usb] init usb:ds (VID 057E PID 3000)...\n"); consoleUpdate(NULL);
    dbi::usb::UsbDsTransport t;
    if (!t.init()) { printf("[usb] init FAILED\n"); return; }
    printf("[usb] waiting for host - run dbibackend on PC (10s)...\n"); consoleUpdate(NULL);
    if (!t.waitConfigured(10000000000ULL)) { printf("[usb] not configured (timeout)\n"); t.exit(); return; }
    printf("[usb] configured! sending LIST...\n"); consoleUpdate(NULL);
    dbi::usb::Dbi0Client c(t);
    std::vector<std::string> files;
    if (!c.listFiles(files)) { printf("[usb] LIST failed\n"); t.exit(); return; }
    printf("[usb] host offers %zu file(s):\n", files.size());
    for (size_t i=0;i<files.size() && i<8;i++) printf("   %s\n", files[i].c_str());
    if (!files.empty()) {
        uint8_t buf[0x200];
        size_t got = c.fileRange(files[0], 0, buf, sizeof(buf));
        printf("[usb] FILE_RANGE got %zu bytes; first16: ", got);
        for (size_t i=0;i<16 && i<got;i++) printf("%02x", buf[i]);
        printf("\n");
        FILE* f=fopen("sdmc:/open-dbi-recv.bin","wb");
        if (f){ fwrite(buf,1,got,f); fclose(f); printf("[usb] saved sdmc:/open-dbi-recv.bin\n"); }
    }
    c.sendExit();
    t.exit();
    printf("[usb] done.\n");
}

// Read-only NCA crypto validation: read an NSP + prod.keys from SD, decrypt the
// Meta NCA, and print the recovered CNMT. No ncm, no writes.
//   place a test NSP at sdmc:/install.nsp and keys at sdmc:/switch/prod.keys
static void ncaDecryptTest() {
    const char* NSP = "sdmc:/install.nsp";
    const char* KEYS = "sdmc:/switch/prod.keys";
    printf("\n[nca] NSP=%s  keys=%s\n", NSP, KEYS); consoleUpdate(NULL);

    dbi::crypto::Keyset ks;
    int nk = ks.load(KEYS);
    printf("[nca] keys: parsed=%d header_key=%d\n", nk, ks.has_header_key);
    if (nk <= 0 || !ks.has_header_key) { printf("[nca] missing/short prod.keys\n"); return; }

    dbi::io::FileSource nsp(NSP);
    if (!nsp.valid()) { printf("[nca] cannot open NSP (place one at %s)\n", NSP); return; }
    printf("[nca] NSP size=%llu\n", (unsigned long long)nsp.size());

    dbi::fs::Pfs0 pfs;
    if (!pfs.open(nsp)) { printf("[nca] not a PFS0/NSP\n"); return; }
    printf("[nca] PFS0 entries=%zu\n", pfs.entries().size());

    // find the Meta NCA (*.cnmt.nca)
    const dbi::fs::Pfs0Entry* meta = nullptr;
    for (const auto& e : pfs.entries()) {
        size_t n = e.name.size();
        if (n >= 9 && e.name.compare(n-9, 9, ".cnmt.nca") == 0) { meta = &e; break; }
    }
    if (!meta) { printf("[nca] no .cnmt.nca in NSP\n"); return; }
    printf("[nca] meta NCA: %s (size=%llu)\n", meta->name.c_str(), (unsigned long long)meta->size);

    std::vector<uint8_t> ncaBuf((size_t)meta->size);
    if (nsp.read(ncaBuf.data(), meta->offset, ncaBuf.size()) != ncaBuf.size()) { printf("[nca] read meta nca failed\n"); return; }

    // step-by-step diagnostics
    dbi::crypto::NcaInfo info;
    bool ok = dbi::crypto::ncaParseHeader(ncaBuf.data(), ncaBuf.size(), ks, info);
    printf("[nca] parseHeader ok=%d (NCA3 magic + key-area) \n", ok);
    if (ok) {
        printf("[nca]  contentType=%u keygen=%u kaekIndex=%u titleId=%016llx rightsId=%d\n",
               (unsigned)info.contentType, info.keygen, info.kaekIndex,
               (unsigned long long)info.titleId, info.hasRightsId);
        printf("[nca]  section0 blocks %u..%u\n", info.sections[0].start_block, info.sections[0].end_block);
    } else { printf("[nca] header decrypt FAILED (wrong header_key, XTS, or not NCA3)\n"); }

    std::vector<uint8_t> cnmt = dbi::crypto::ncaExtractCnmt(ncaBuf.data(), ncaBuf.size(), ks);
    printf("[nca] extracted CNMT bytes=%zu\n", cnmt.size());
    if (!cnmt.empty()) {
        dbi::fs::Cnmt cn;
        if (cn.parse(cnmt.data(), cnmt.size())) {
            printf("[nca] CNMT titleId=%016llx ver=%u type=0x%02x contents=%zu\n",
                   (unsigned long long)cn.titleId, cn.version, cn.metaType, cn.contents.size());
            for (size_t i=0;i<cn.contents.size() && i<6;i++)
                printf("[nca]   content[%zu] type=%u size=%llu id=%02x%02x%02x..\n",
                       i, cn.contents[i].type, (unsigned long long)cn.contents[i].size,
                       cn.contents[i].id[0], cn.contents[i].id[1], cn.contents[i].id[2]);
            printf("[nca] >>> NCA CRYPTO VALIDATED if titleId matches the NSP name <<<\n");
        }
    } else {
        printf("[nca] CNMT extract failed (section CTR / PFS0-offset likely need a fix)\n");
    }
    printf("[nca] done.\n");
}

// REAL install from sdmc:/install.nsp to SD via real ncm/es. WRITES to ncm storage.
static void realInstall() {
    printf("\n[INSTALL] *** WRITES to ncm *** sdmc:/install.nsp -> SD\n"); consoleUpdate(NULL);
    dbi::crypto::Keyset ks;
    if (ks.load("sdmc:/switch/prod.keys") <= 0 || !ks.has_header_key) { printf("[INSTALL] need sdmc:/switch/prod.keys\n"); return; }
    dbi::io::FileSource nsp("sdmc:/install.nsp");
    if (!nsp.valid()) { printf("[INSTALL] no sdmc:/install.nsp\n"); return; }
    dbi::install::NcmBackend be;
    dbi::install::Installer inst(be);
    inst.setKeyset(&ks);
    printf("[INSTALL] running (writes NCAs, registers, sets meta, commits)...\n"); consoleUpdate(NULL);
    auto r = inst.installNsp(nsp, dbi::install::StorageId::SdCard);
    printf("[INSTALL] ok=%d ncas=%d tickets=%d err=%s\n", r.ok, r.ncasWritten, r.ticketsImported, r.error.c_str());
    if (r.ok) printf("[INSTALL] DONE - check home menu / DBI; delete there if unwanted.\n");
}

// REAL install streamed over USB from the PC dbibackend. WRITES to ncm storage.
static void usbInstall() {
    printf("\n[usb-install] *** WRITES to ncm *** stream-install NSP over USB\n"); consoleUpdate(NULL);
    dbi::crypto::Keyset ks;
    if (ks.load("sdmc:/switch/prod.keys") <= 0 || !ks.has_header_key) { printf("[usb-install] need sdmc:/switch/prod.keys\n"); return; }
    dbi::usb::UsbDsTransport t;
    if (!t.init()) { printf("[usb-install] usb init failed\n"); return; }
    printf("[usb-install] waiting for host - run dbibackend on PC (10s)...\n"); consoleUpdate(NULL);
    if (!t.waitConfigured(10000000000ULL)) { printf("[usb-install] not configured\n"); t.exit(); return; }
    t.setReadTimeout(30000000000ULL);                 // 30s per chunk during install
    dbi::usb::Dbi0Client c(t);
    std::vector<std::string> files;
    if (!c.listFiles(files) || files.empty()) { printf("[usb-install] LIST empty\n"); t.exit(); return; }
    std::string nsp;
    for (auto& f : files) if (f.size()>=4 && (f.compare(f.size()-4,4,".nsp")==0 || f.compare(f.size()-4,4,".nsz")==0)) { nsp=f; break; }
    if (nsp.empty()) nsp = files[0];
    printf("[usb-install] installing over USB: %s\n", nsp.c_str()); consoleUpdate(NULL);

    dbi::usb::Dbi0FileSource src(c, nsp, 0);          // ContentSource over FILE_RANGE
    dbi::install::NcmBackend be;
    dbi::install::Installer inst(be);
    inst.setKeyset(&ks);
    auto r = inst.installNsp(src, dbi::install::StorageId::SdCard);
    printf("[usb-install] ok=%d ncas=%d tickets=%d err=%s\n", r.ok, r.ncasWritten, r.ticketsImported, r.error.c_str());
    if (r.ok) printf("[usb-install] DONE - check home menu / DBI.\n");
    c.sendExit();
    t.exit();
}

int main(int, char**) {
    consoleInit(NULL);
    printf("Open-DBI self-test (clean-room modules)  [BUILD r8: +NSZ]\n\n");
    selftest();
    printf("\nPress A = USB transfer test (read-only, needs dbibackend)\n"
           "Press B = NCA decrypt test (read-only)\n"
           "Hold L+R+X = REAL install from sdmc:/install.nsp (writes ncm!)\n"
           "Hold L+R+Y = REAL install over USB from dbibackend (writes ncm!)\n"
           "Press + to exit.\n");
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad; padInitializeDefault(&pad);
    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        u64 held = padGetButtons(&pad);
        if (down & HidNpadButton_Plus) break;
        if (down & HidNpadButton_A) { usbReceiveTest();  printf("\nPress + to exit.\n"); }
        if (down & HidNpadButton_B) { ncaDecryptTest();  printf("\nPress + to exit.\n"); }
        if ((down & HidNpadButton_X) && (held & HidNpadButton_L) && (held & HidNpadButton_R)) {
            realInstall(); printf("\nPress + to exit.\n");
        }
        if ((down & HidNpadButton_Y) && (held & HidNpadButton_L) && (held & HidNpadButton_R)) {
            usbInstall(); printf("\nPress + to exit.\n");
        }
        consoleUpdate(NULL);
    }
    consoleExit(NULL);
    return 0;
}
