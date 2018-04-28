#include "Common.hpp"

#include "ClusterCache.hpp"
#include "Raii.hpp"

namespace xxfs { namespace {
using Cache = ClusterCache<4096>;

void WriteCluster(Cache &vCache, uint32_t lcn, uint32_t vcn, int nVal) {
    for (uint32_t i = 0; i < vcn; ++i) {
        auto spc = vCache.At<BitmapCluster>(lcn + i);
        memset(spc.get(), nVal, kcbCluSize);
    }
}

void WriteBitmap(Cache &vCache, uint32_t lcnBmp, uint32_t cbi, int nVal) {
    auto vbi = cbi % 64;
    auto lqw = cbi / 64;
    auto vqw = lqw % kcqPerClu;
    auto vcn = lqw / kcqPerClu;
    for (uint32_t i = 0; i < vcn; ++i) {
        auto spc = vCache.At<BitmapCluster>(lcnBmp + i);
        memset(spc.get(), nVal, kcbCluSize);
    }
    auto spc = vCache.At<BitmapCluster>(lcnBmp + vcn);
    memset(spc.get(), nVal, sizeof(uint64_t) * vqw);
    auto uMask = (uint64_t {1} << vbi) - 1;
    if (nVal)
        spc->aBmp[vqw] |= uMask;
    else
        spc->aBmp[vqw] &= ~uMask;
}

void BitmapAlloc(Cache &vCache, uint32_t lcnBmp, uint32_t lbi) {
    auto vbi = lbi % 64;
    auto lqw = lbi / 64;
    auto vqw = lqw % kcqPerClu;
    auto vcn = lqw / kcqPerClu;
    auto spc = vCache.At<BitmapCluster>(lcnBmp + vcn);
    spc->aBmp[vqw] |= uint64_t {1} << vbi;
}

}}

int main(int ncArg, char *ppszArgs[]) {
    using namespace xxfs;
    if (ncArg != 2) {
        fprintf(stderr, "Incorrect argument.\n");
        printf("\nUsage: %s path\n", ppszArgs[0]);
        return -1;
    }
    CheckPageSize();
    auto fd = open(ppszArgs[1], O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "Failed to open the file.\n");
        return -1;
    }
    RaiiFile vRf(fd);
    FileStat vStat;
    if (fstat(fd, &vStat)) {
        fprintf(stderr, "Failed to get the file\'s size.\n");
        return -1;
    }
    try {
        Cache vCache(fd);
        auto spcMeta = vCache.At<MetaCluster>(0);
        auto vRes = FillMeta(spcMeta.get(), (size_t) vStat.st_size);
        switch (vRes) {
        case MetaResult::kTooLarge:
            fprintf(stderr, "The file is too large (%zu B over %zu B).\n", (size_t) vStat.st_size, kcbMaxSize);
            return -1;
        case MetaResult::kTooSmall:
            fprintf(stderr, "The file is too small (%zu B less than %zu B).\n", (size_t) vStat.st_size, kcbMinSize);
            return -1;
        case MetaResult::kPartial:
            fprintf(stderr, "The file\'s size is not dividable by 4KiB block size.\n");
            return -1;
        default:
            break;
        }
        WriteCluster(vCache, spcMeta->lcnCluBmp, spcMeta->ccCluBmp, 0xff);
        WriteBitmap(vCache, spcMeta->lcnCluBmp, spcMeta->ccTotal, 0x00);
        WriteCluster(vCache, spcMeta->lcnInoBmp, spcMeta->ccInoBmp, 0xff);
        WriteBitmap(vCache, spcMeta->lcnInoBmp, spcMeta->ciTotal, 0x00);
        for (uint32_t i = 0; i < spcMeta->ccUsed; ++i)
            BitmapAlloc(vCache, spcMeta->lcnCluBmp, i);
        // spcMeta->ciUsed is expected to be 1, that's the root directory
        for (uint32_t i = 0; i < spcMeta->ciUsed; ++i) {
            BitmapAlloc(vCache, spcMeta->lcnInoBmp, i);
            auto vin = i % kciPerClu;
            auto vcn = i / kciPerClu;
            auto spc = ShrMap<InodeCluster>(fd, spcMeta->lcnIno + vcn);
            memset(&spc->aInos[vin], 0x00, sizeof(Inode));
            spc->aInos[vin].uMode = 0777 | S_IFDIR;
        }
        printf("%-8s %14s %14s %14s\n", "", "Total", "Allocated", "Free");
        printf(
            "%-8s %14" PRIu64 " %14" PRIu64 " %14" PRIu64 "\n",
            "Byte", spcMeta->cbSize, (uint64_t) kcbCluSize * spcMeta->ccUsed,
            (uint64_t) kcbCluSize * (spcMeta->ccTotal - spcMeta->ccUsed)
        );
        printf(
            "%-8s %14" PRIu32 " %14" PRIu32 " %14" PRIu32 "\n",
            "Cluster", spcMeta->ccTotal, spcMeta->ccUsed,
            spcMeta->ccTotal - spcMeta->ccUsed
        );
        printf(
            "%-8s %14" PRIu32 " %14" PRIu32 " %14" PRIu32 "\n",
            "Inode", spcMeta->ciTotal, spcMeta->ciUsed,
            spcMeta->ciTotal - spcMeta->ciUsed
        );
    }
    catch (FatalException &e) {
        e.ShowWhat(stderr);
        return -1;
    }
    return 0;
}
