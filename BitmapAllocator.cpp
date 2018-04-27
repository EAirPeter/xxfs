#include "Common.hpp"

#include "BitmapAllocator.hpp"

BitmapAllocator::BitmapAllocator(
    int fd, uint32_t &cUsed,
    uint32_t bnoBmpBmp, uint32_t cBmpBmpBlk,
    uint32_t bnoBmp
) :
    x_fd {fd}, x_cUsed {cUsed},
    x_bnoBmp {bnoBmp}, x_cBmpBmpQwd {cBmpBmpBlk * kcQwdPerBlk},
    x_rpBmpBmp {MapAt<uint64_t>(fd, bnoBmpBmp, (size_t) kcbBlkSize * cBmpBmpBlk)}
{}

uint32_t BitmapAllocator::Alloc() {
    if (!x_rpBmpBmp)
        RAISE("The bitmap bitmap is null");
    if (x_rpCurBmp && x_cCurBit == kcbBlkSize * 8)
        x_rpCurBmp.Release();
    if (!x_rpCurBmp) {
        auto idxCurQwd = x_idxCurBmpBlk / 64;
        auto idxQwd = idxCurQwd;
        while (idxQwd < x_cBmpBmpQwd && ~x_rpBmpBmp[idxQwd])
            ++idxQwd;
        if (idxQwd == x_cBmpBmpQwd) {
            idxQwd = 0;
            while (idxQwd < idxCurQwd && ~x_rpBmpBmp[idxQwd])
                ++idxQwd;
            if (idxQwd == idxCurQwd)
                throw Exception {ENOSPC};
        }
        for (uint32_t j = 0; j < 64; ++j)
            if ((uint64_t {1} << j) & ~x_rpBmpBmp[idxQwd]) {
                x_idxCurBmpBlk = idxQwd * 64 + j;
                x_rpCurBmp = MapAt<uint64_t>(x_fd, x_bnoBmp + x_idxCurBmpBlk);
                break;
            }
    }
    if (x_rpCurBmp) {
        for (uint32_t i = 0; i < kcQwdPerBlk; ++i) {
            for (uint32_t j = 0; j < 64; ++j)
                if ((uint64_t {1} << j) & ~x_rpCurBmp[i]) {
                    auto uRes = i * 64 + j;
                    x_rpCurBmp[i] |= uint64_t {1} << j;
                    ++x_cCurBit;
                    ++x_cUsed;
                    return uRes;
                }
        }
    }
    RAISE("Should not reach here");
}

void BitmapAllocator::Free(uint32_t idxBit) {
    auto idxBitInQwd = idxBit % 64;
    auto idxQwd = idxBit / 64;
    auto idxQwdInBlk = idxQwd % kcQwdPerBlk;
    auto idxBlk = idxQwd / kcQwdPerBlk;
    if (x_rpCurBmp && x_idxCurBmpBlk == idxBlk) {
        x_rpCurBmp[idxQwdInBlk] &= ~(uint64_t {1} << idxBitInQwd);
        --x_cCurBit;
    }
    else {
        auto rpBmp = MapAt<uint64_t>(x_fd, x_bnoBmp + idxBlk);
        rpBmp[idxQwdInBlk] &= ~(uint64_t {1} << idxBitInQwd);
    }
    --x_cUsed;
    idxBitInQwd = idxBlk % 64;
    idxQwd = idxBlk / 64;
    idxQwdInBlk = idxQwd % kcQwdPerBlk;
    idxBlk = idxQwd / kcQwdPerBlk;
    x_rpBmpBmp[idxBlk] &= ~(uint64_t {1} << idxBitInQwd);
}
