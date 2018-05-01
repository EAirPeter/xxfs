#ifndef XXFS_OPENED_DIR_HPP_
#define XXFS_OPENED_DIR_HPP_

#include "Common.hpp"

#include "OpenedFile.hpp"

namespace xxfs {

enum class DirPolicy {
    kAny,
    kDir,
    kNotDir,
    kNone,
};

class OpenedDir : public OpenedFile {
public:
    constexpr static auto kItBegin = off_t {0};
    constexpr static auto kItEnd = ~off_t {0};

public:
    constexpr OpenedDir(Xxfs *px, Inode *pi, uint32_t lin) noexcept : OpenedFile(px, pi, lin, false, false, false) {}

    // care the case empty dir
    // requires pszName not empty
    // returns inode number
    std::pair<uint32_t, uint16_t> Lookup(const char *pszName, DirPolicy vPolicy);

    // for readdir
    off_t IterSeek(off_t vOff);
    const char *IterGet(FileStat &vStat) noexcept;
    off_t IterNext() noexcept;

    // allocate a new entry in the directory
    // requires pszName not empty
    // returns the previous inode number, 0 for not exist before
    // does not overrite directory
    std::pair<uint32_t, uint16_t> Insert(const char *pszName, uint32_t lin, uint16_t uMode, DirPolicy vPolicy);

    // free an entry in the directory
    // does not check if the directory requeseted to delete is empty (bDir = true)
    // requires pszName not empty
    // returns the inode number
    std::pair<uint32_t, uint16_t> Remove(const char *pszName, DirPolicy vPolicy);

    // re-organize the structure if the count of used entry * 2 is less than the total count
    // required to call Xxfs::Y_FileShrink after destruction closely
    void Shrink(bool bForce = false) noexcept;
    // does not check constraints, just do it

private:
    // get the next entry in the dir, bExist does not be necessarily true
    // requires stack not empty
    void X_Next() noexcept;

    // allocate the root entry and the first cluster if ccSize is zero
    void X_PrepareRoot();

    // allocate a new dirent and update any metadata
    // append a block if no space available and update the free entry list
    // invokes FilePtrW::Seek
    uint32_t X_Alloc();
    void X_Free(uint32_t len) noexcept;

    DirEnt *X_GetEnt(uint32_t len) noexcept;

    template<bool kAlloc>
    DirEnt *X_MapEnt(FilePointer<kAlloc> &fp, uint32_t len) noexcept(!kAlloc);

    uint32_t X_Top() const noexcept;
    void X_Push(uint32_t len) noexcept;
    void X_Pop() noexcept;

private:
    // 0 should never be in the stack
    uint32_t x_alenStk[kcePerClu] {};
    char x_szName[kcePerClu] {};
    uint32_t x_cStkSize = 0;

};

}

#endif
