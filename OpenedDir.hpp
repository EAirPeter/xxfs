#ifndef XXFS_OPENED_DIR_HPP_
#define XXFS_OPENED_DIR_HPP_

#include "Common.hpp"

#include "OpenedFile.hpp"

namespace xxfs {

class OpenedDir : public OpenedFile {
public:
    constexpr OpenedDir(Xxfs *px, Inode *pi) noexcept : OpenedFile(px, pi, false, false, false) {}

    // returns inode number
    uint32_t Lookup(const char *pszName);

    // for readdir
    void IterSeek(off_t vOff);
    const char *IterNext(FileStat &vStat, off_t &vOff);

    // re-organize the structure if needed
    // less than half
    void Shrink() noexcept;

private:
    DirEnt *X_GetEnt(uint32_t len);
    // requires: stack not empty
    void X_Next();

    // the stack bottom is always the root if not empty
    uint32_t X_Top() const noexcept;
    void X_Push(uint32_t len) noexcept;
    void X_Pop() noexcept;

private:
    uint32_t aStk[kcePerClu] {};
    char szName[kcePerClu] {};
    uint32_t cStkSize = 0;

};

}

#endif
