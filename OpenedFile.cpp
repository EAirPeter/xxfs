#include "Common.hpp"

#include "OpenedFile.hpp"
#include "Xxfs.hpp"

namespace xxfs {

void OpenedFile::DoRead(void *pBuf, uint64_t cbSize, uint64_t cbOff) {
    auto pBytes = (uint8_t *) pBuf;
    uint64_t cbRead = 0;
    while (cbRead < cbSize) {
        auto vby = cbOff % kcbCluSize;
        auto vcn = cbOff / kcbCluSize;
        auto cbToRead = std::min(kcbCluSize - vby, cbSize);
        auto spc = x_fpR.Seek<ByteCluster>(px, pi, vcn);
        if (spc)
            memcpy(pBytes, spc->aData + vby, cbToRead);
        else
            memset(pBytes, 0, cbToRead);
        pBytes += cbToRead;
        cbOff += cbToRead;
        cbRead += cbToRead;
    }
}

uint64_t OpenedFile::DoWrite(const void *pBuf, uint64_t cbSize, uint64_t cbOff) {
    auto pBytes = (const uint8_t *) pBuf;
    uint64_t cbWritten = 0;
    try {
        while (cbWritten < cbSize) {
            auto vby = cbOff % kcbCluSize;
            auto vcn = cbOff / kcbCluSize;
            auto cbToWrite = std::min(kcbCluSize - vby, cbSize);
            auto spc = x_fpW.Seek<ByteCluster>(px, pi, vcn);
            memcpy(spc->aData + vby, pBytes, cbToWrite);
            if (bDirect)
                DoSync();
            pBytes += cbToWrite;
            cbOff += cbToWrite;
            cbWritten += cbToWrite;
        }
    }
    catch (Exception &e) {
        if (e.nErrno != ENOSPC || !cbWritten)
            throw;
    }
    return cbWritten;
}

void OpenedFile::DoSync() noexcept {
    x_fpR.Sync();
    x_fpW.Sync();
}

}
