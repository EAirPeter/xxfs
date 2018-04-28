#include "Common.hpp"

#include "BitmapAllocator.hpp"

namespace xxfs {

BitmapAllocator::X_BmpPtr BitmapAllocator::X_Map(int fd, uint32_t lcn, uint32_t cc) {
    auto cbSize = (size_t) kcbCluSize * cc;
    auto cbOff = (off_t) kcbCluSize * lcn;
    auto pVoid = mmap(nullptr, cbSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, cbOff);
    if (pVoid == MAP_FAILED)
        RAISE("Failed to map bitmap", errno);
    return {reinterpret_cast<uint64_t *>(pVoid), X_Deleter {cbSize}};
}


BitmapAllocator::BitmapAllocator(int fd, uint32_t lcnBmp, uint32_t ccBmp) :
    x_cqBmp {ccBmp * kcqPerClu}, x_upBmp {X_Map(fd, lcnBmp, ccBmp)}
{}

uint32_t BitmapAllocator::Alloc() {
    for (uint32_t i = x_vqwCur; i < x_cqBmp; ++i) {
        auto &uCur = x_upBmp[i];
        for (uint32_t j = 0; j < 64; ++j) {
            auto uMask = uint64_t {1} << j;
            if (uMask & ~uCur) {
                uCur |= uMask;
                return i * 64 + j;
            }
        }
    }
    for (uint32_t i = 0; i < x_vqwCur; ++i) {
        auto &uCur = x_upBmp[i];
        for (uint32_t j = 0; j < 64; ++j) {
            auto uMask = uint64_t {1} << j;
            if (uMask & ~uCur) {
                uCur |= uMask;
                return i * 64 + j;
            }
        }
    }
    throw Exception {ENOSPC};
}

void BitmapAllocator::Free(uint32_t lbi) noexcept {
    auto vbi = lbi % 64;
    auto vqw = lbi / 64;
    x_upBmp[vqw] &= ~(uint64_t {1} << vbi);
}

}
