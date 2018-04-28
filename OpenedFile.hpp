#ifndef XXFS_OPENED_FILE_HPP_
#define XXFS_OPENED_FILE_HPP_

#include "Common.hpp"

#include "FilePointer.hpp"

namespace xxfs {

struct OpenedFile : NoCopyMove {
    Xxfs *const px;
    Inode *const pi;
    FilePointer fpR, fpW;
    uint32_t bWrite : 1;
    uint32_t bAppend : 1;
    uint32_t bDirect : 1;
    uint32_t : 0;
    
    constexpr OpenedFile(Xxfs *px_, Inode *pi_, bool bWrite_, bool bAppend_, bool bDirect_) noexcept :
        px {px_}, pi {pi_}, bWrite {bWrite_}, bAppend {bAppend_}, bDirect {bDirect_}
    {}

    // read and write dose not check access right and bounds
    // they are handled in Xxfs
    uint64_t Read(void *pBuf, uint64_t cbSize, uint64_t cbOff);
    uint64_t Write(const void *pBuf, uint64_t cbSize, uint64_t cbOff);
    void Sync() noexcept;
};

}

#endif
