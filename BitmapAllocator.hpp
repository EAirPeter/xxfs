#ifndef XXFS_BITMAP_ALLOCATOR_HPP_
#define XXFS_BITMAP_ALLOCATOR_HPP_

#include "Common.hpp"

#include "Raii.hpp"

class BitmapAllocator {
public:
    BitmapAllocator(
        int fd, uint32_t &cUsed,
        uint32_t bnoBmpBmp, uint32_t cBmpBmpBlk,
        uint32_t bnoBmp
    );

public:
    uint32_t Alloc();
    void Free(uint32_t idxBit);

private:
    int x_fd;
    uint32_t &x_cUsed;
    uint32_t x_bnoBmp;
    uint32_t x_cBmpBmpQwd;
    uint32_t x_idxCurBmpBlk = 0;
    uint32_t x_cCurBit = 0;
    MapPtr<uint64_t> x_rpBmpBmp;
    MapPtr<uint64_t> x_rpCurBmp;

};

#endif
