// save_backup.cpp - see save_backup.h. Clean-room: renders the observed backup format.
#include "save_backup.h"
#include <cstdio>

namespace dbi::saves {

static std::string titleIdHex(uint64_t tid) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llX", (unsigned long long)tid);
    return std::string(buf, 16);
}

std::string buildInfoIni(const SaveInfo& info) {
    std::string s = "[General]\n";
    s += "TitleId="   + titleIdHex(info.titleId)        + "\n";
    s += "TitleName=" + info.titleName                  + "\n";
    s += "BackupDate="+ info.backupDate                 + "\n";
    s += "Account="   + info.account                    + "\n";
    s += "Space="     + std::to_string((int)info.space) + "\n";
    return s;
}

std::string backupFileName(uint64_t titleId, uint8_t space,
                          const std::string& timestamp, int index) {
    return titleIdHex(titleId) + "_" + std::to_string((int)space) + "_" +
           timestamp + "_" + std::to_string(index) + ".zip";
}

} // namespace dbi::saves
