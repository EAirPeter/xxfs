#ifndef XXFS_BITMAP_ALLOCATOR_HPP_
#define XXFS_BITMAP_ALLOCATOR_HPP_

#include "Common.hpp"

#include "Raii.hpp"

namespace xxfs {

class BitmapAllocator : NoCopyMove {
private:
    struct X_Deleter {
        inline void operator ()(void *pObj) noexcept {
            munmap(pObj, cbSize);
        }

        size_t cbSize;
    };
    
    using X_BmpPtr = std::unique_ptr<uint64_t[], X_Deleter>;
    static X_BmpPtr X_Map(int fd, uint32_t lcn, uint32_t cc);

public:
    BitmapAllocator(int fd, uint32_t lcnBmp, uint32_t ccBmp);

    uint32_t Alloc();
    void Free(uint32_t lbi) noexcept;

private:
    uint32_t x_vqwCur = 0;
    uint32_t x_cqBmp;
    X_BmpPtr x_upBmp;

};

}

#endif
