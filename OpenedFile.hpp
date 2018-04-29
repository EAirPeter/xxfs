#ifndef XXFS_OPENED_FILE_HPP_
#define XXFS_OPENED_FILE_HPP_

#include "Common.hpp"

#include "FilePointer.hpp"

namespace xxfs {

class Xxfs;

class OpenedFile : NoCopyMove {
public:    
    constexpr OpenedFile(Xxfs *px_, Inode *pi_, bool bWrite_, bool bAppend_, bool bDirect_) noexcept :
        px {px_}, pi {pi_}, bWrite {bWrite_}, bAppend {bAppend_}, bDirect {bDirect_}
    {}

    // DoRead and DoWrite dose not check access right and bounds
    // they are handled in Xxfs
    void DoRead(void *pBuf, uint64_t cbSize, uint64_t cbOff);
    uint64_t DoWrite(const void *pBuf, uint64_t cbSize, uint64_t cbOff);
    void DoSync() noexcept;

public:
    Xxfs *const px;
    Inode *const pi;
    const uint32_t bWrite : 1;
    const uint32_t bAppend : 1;
    const uint32_t bDirect : 1;
    const uint32_t : 0;
    
protected:
    FilePtrR x_fpR;
    FilePtrW x_fpW;
};

}

#endif
