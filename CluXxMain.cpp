#include "Common.hpp"

#include "Raii.hpp"

#include <string>
#include <vector>

namespace xxfs { namespace {

void ShowHelp(const char *pszExec) {
    printf(
        "\n"
        "Usage: %s filepath [options]\n"
        "\n"
        "Options:\n"
        "    -m         print the meta cluster\n"
        "    -b         check the bitmap which should agree with the meta cluster\n"
        "    -x i j     print the j-th index at i-th cluster (has to be index cluster)\n"
        "    -i i       print the i-th inode\n"
        "    -d i j     print the j-th dirent at i-th cluster (has to be dir cluster)\n"
        "    -o i path  dump the i-th cluster to a file (usually a byte cluster)\n"
        "    -h         print this help\n",
        pszExec
    );
}

enum class ReqType {
    kMeta,      // print the meta cluster
    kBitmap,    // check the bitmap which should agree with the meta cluster
    kIndex,     // print the j-th index at i-th cluster (has to be index cluster)
    kInode,     // print the j-th inode at i-th cluster (has to be inode cluster)
    kDir,       // print the j-th dirent at i-th cluster (has to be dir cluster)
    kDump,      // dump the i-th cluster to a file (usually a byte cluster)
};

constexpr const char *f_aTypeName[] {
    "meta",
    "bitmap",
    "index",
    "inode",
    "dir",
    "dump",
};

struct Request {
    ReqType vType = ReqType::kMeta;
    uint32_t lcn = 0;
    uint32_t num = 0;
    std::string sFile {};

    inline Request(ReqType vType_, uint32_t lcn_ = 0, uint32_t num_ = 0) :
        vType {vType_}, lcn {lcn_}, num {num_}
    {}

    inline Request(ReqType vType_, uint32_t lcn_, const char *pszFile) :
        vType {vType_}, lcn {lcn_}, sFile(pszFile)
    {}
};

struct CvtExn {};

inline uint32_t FromNtmbs(const char *psz) {
    uint32_t uRes = 0;
    while (*psz) {
        if (!isdigit(*psz))
            throw CvtExn {};
        auto uNew = uRes * 10 + (uint32_t) (*psz & 0x0f);
        if (uNew < uRes)
            throw CvtExn {};
        uRes = uNew;
        ++psz;
    }
    return uRes;
}

template<class tObj>
inline ShrPtr<tObj> RdMap(int fd, uint32_t lcn) {
    auto pVoid = mmap(
        nullptr, kcbCluSize, PROT_READ,
        MAP_PRIVATE, fd, (off_t) kcbCluSize * lcn
    );
    if (pVoid == MAP_FAILED)
        RAISE("Failed to invoke mmap()", errno);
    return ShrPtr<tObj>(reinterpret_cast<tObj *>(pVoid), UnmapDeleter {});
}

}}

int main(int ncArg, char *ppszArgs[]) {
    using namespace xxfs;
    std::vector<Request> vecReqs;
    const char *pszPath = nullptr;
    bool bHelp = false;
    bool bIncorrect = false;
    int chOpt;
    if (optind < ncArg)
        pszPath = ppszArgs[optind++];
    while ((chOpt = getopt(ncArg, ppszArgs, ":mbxidoh")) != -1) {
        switch (chOpt) {
        case 'm':
            vecReqs.emplace_back(ReqType::kMeta);
            break;
        case 'b':
            vecReqs.emplace_back(ReqType::kBitmap);
            break;
        case 'x': {
            if (optind + 2 > ncArg) {
                bIncorrect = true;
                break;
            }
            auto lcn = FromNtmbs(ppszArgs[optind++]);
            auto num = FromNtmbs(ppszArgs[optind++]);
            vecReqs.emplace_back(ReqType::kIndex, lcn, num);
            break;
        }
        case 'i': {
            if (optind + 1 > ncArg) {
                bIncorrect = true;
                break;
            }
            auto lin = FromNtmbs(ppszArgs[optind++]);
            vecReqs.emplace_back(ReqType::kInode, 0, lin);
            break;
        }
        case 'd': {
            if (optind + 2 > ncArg) {
                bIncorrect = true;
                break;
            }
            auto lcn = FromNtmbs(ppszArgs[optind++]);
            auto ven = FromNtmbs(ppszArgs[optind++]);
            vecReqs.emplace_back(ReqType::kDir, lcn, ven);
            break;
        }
        case 'o': {
            if (optind + 2 > ncArg) {
                bIncorrect = true;
                break;
            }
            auto lcn = FromNtmbs(ppszArgs[optind++]);
            auto pszFile = ppszArgs[optind++];
            vecReqs.emplace_back(ReqType::kDump, lcn, pszFile);
            break;
        }
        case 'h':
            bHelp = true;
            break;
        default:
            bIncorrect = true;
            break;
        }
    }
    if (bIncorrect) {
        fprintf(stderr, "Incorrect argument.\n");
        ShowHelp(ppszArgs[0]);
        return -1;
    }
    if (bHelp) {
        ShowHelp(ppszArgs[0]);
        return 0;
    }
    if (!pszPath) {
        fprintf(stderr, "No file specified.\n");
        ShowHelp(ppszArgs[0]);
        return -1;
    }
    auto fd = open(pszPath, O_RDONLY | O_DIRECT);
    if (fd == -1) {
        fprintf(stderr, "Failed to open file: %s.\n", pszPath);
        return -1;
    }
    RaiiFile vRf(fd);
    for (auto &[vType, lcn, num, sFile] : vecReqs) {
        printf(
            "[type = %s, lcn = %" PRIu32 ", num = %" PRIu32 ", file = %s]:\n",
            f_aTypeName[(int) vType], lcn, num, sFile.c_str()
        );
        try {
            switch (vType) {
            case ReqType::kMeta: {
                auto spcMeta = RdMap<MetaCluster>(fd, 0);
                printf("cbSize = %" PRIu64 "\n", spcMeta->cbSize);
                printf("ccTotal = %" PRIu32 "\n", spcMeta->ccTotal);
                printf("ccUsed = %" PRIu32 "\n", spcMeta->ccUsed);
                printf("ciTotal = %" PRIu32 "\n", spcMeta->ciTotal);
                printf("ciTotal = %" PRIu32 "\n", spcMeta->ciUsed);
                printf("lcnCluBmp = %" PRIu32 "\n", spcMeta->lcnCluBmp);
                printf("ccCluBmp = %" PRIu32 "\n", spcMeta->ccCluBmp);
                printf("lcnInoBmp = %" PRIu32 "\n", spcMeta->lcnInoBmp);
                printf("ccInoBmp = %" PRIu32 "\n", spcMeta->ccInoBmp);
                printf("lcnIno = %" PRIu32 "\n", spcMeta->lcnIno);
                printf("ccIno = %" PRIu32 "\n", spcMeta->ccIno);
                break;
            }
            case ReqType::kBitmap: {
                auto spcMeta = RdMap<MetaCluster>(fd, 0);
                uint32_t ccUsed = spcMeta->ccTotal;
                for (uint32_t i = 0; i < spcMeta->ccCluBmp; ++i) {
                    auto spc = RdMap<BitmapCluster>(fd, spcMeta->lcnCluBmp + i);
                    for (uint32_t j = 0; j < kcqPerClu; ++j)
                        for (uint32_t k = 0; k < 64; ++k)
                            if (!(spc->aBmp[j] & uint64_t {1} << k))
                                --ccUsed;
                }
                uint32_t ciUsed = spcMeta->ciTotal;
                for (uint32_t i = 0; i < spcMeta->ccInoBmp; ++i) {
                    auto spc = RdMap<BitmapCluster>(fd, spcMeta->lcnInoBmp + i);
                    for (uint32_t j = 0; j < kcqPerClu; ++j)
                        for (uint32_t k = 0; k < 64; ++k)
                            if (!(spc->aBmp[j] & uint64_t {1} << k))
                                --ciUsed;
                }
                printf("(meta) ccTotal = %" PRIu32 "\n", spcMeta->ccTotal);
                printf("(meta) ccUsed = %" PRIu32 "\n", spcMeta->ccUsed);
                printf("( bmp) ccUsed = %" PRIu32 "\n", ccUsed);
                printf("(meta) ciTotal = %" PRIu32 "\n", spcMeta->ciTotal);
                printf("(meta) ciUsed = %" PRIu32 "\n", spcMeta->ciUsed);
                printf("( bmp) ciUsed = %" PRIu32 "\n", ciUsed);
                auto bCorrect = ccUsed == spcMeta->ccUsed && ciUsed == spcMeta->ciUsed;
                printf("Bitmaps are %s.\n", bCorrect ? "correct" : "incorrect");
                break;
            }
            case ReqType::kIndex: {
                auto spc = RdMap<IndexCluster>(fd, lcn);
                printf("lcn = %" PRIu32 "\n", spc->aLcns[num]);
                break;
            }
            case ReqType::kInode: {
                auto vin = num % kciPerClu;
                auto vcn = num / kciPerClu;
                auto spcMeta = RdMap<MetaCluster>(fd, 0);
                if (num >= spcMeta->ciTotal) {
                    fprintf(
                        stderr,
                        "Requested lin is beyond the total inode count (%" PRIu32 " > %" PRIu32 ").",
                        num, spcMeta->ciTotal
                    );
                    break;
                }
                if (vcn >= spcMeta->ccIno) {
                    fprintf(
                        stderr,
                        "Requested vcn is beyond the total inode cluster count (%" PRIu32 " > %" PRIu32 ").",
                        vcn, spcMeta->ccIno
                    );
                }
                auto lcn = spcMeta->lcnIno + vcn;
                auto spc = RdMap<InodeCluster>(fd, lcn);
                auto pi = &spc->aInos[vin];
                printf("cbSize = %" PRIu64 "\n", pi->cbSize);
                printf("ccSize = %" PRIu32 "\n", pi->ccSize);
                printf("uMode = %06" PRIo16 "\n", pi->uMode);
                printf("cLink = %" PRIu32 "\n", pi->cLink);
                for (uint32_t i = 0; i < kccIdx0; ++i)
                    printf("lcnIdx0[%" PRIu32 "] = %" PRIu32 "\n", i, pi->lcnIdx0[i]);
                printf("lcnIdx1 = %" PRIu32 "\n", pi->lcnIdx1);
                printf("lcnIdx2 = %" PRIu32 "\n", pi->lcnIdx2);
                printf("lcnIdx3 = %" PRIu32 "\n", pi->lcnIdx3);
                break;
            }
            case ReqType::kDir: {
                auto spc = RdMap<DirCluster>(fd, lcn);
                auto pe = &spc->aEnts[num];
                printf("linFile = %" PRIu32 "\n", pe->linFile);
                printf("lenNext = %" PRIu32 "\n", pe->lenNext);
                printf("lenChild = %" PRIu32 "\n", pe->lenChild);
                printf("uMode = %06" PRIo16 "\n", pe->uMode);
                if (isprint((int) pe->byKey))
                    printf("byKey = %" PRIu8 " \'%c\'\n", pe->byKey, (char) pe->byKey);
                else
                    printf("byKey = %" PRIu8 "\n", pe->byKey);
                printf("bExist = %s", pe->bExist ? "true" : "false");
                break;
            }
            case ReqType::kDump: {
                auto fdSave = open(sFile.c_str(), O_CREAT | O_TRUNC, 0777);
                RaiiFile rf(fdSave);
                if (fdSave == -1) {
                    auto nErrno = errno;
                    fprintf(stderr, "Failed to open file: %s.\n", sFile.c_str());
                    throw Exception {nErrno};
                }
                auto spc = RdMap<ByteCluster>(fd, lcn);
                if (write(fdSave, spc.get(), kcbCluSize) != (ssize_t) kcbCluSize) {
                    auto nErrno = errno;
                    fprintf(stderr, "Failed to write data.\n");
                    throw Exception {nErrno};
                }
            }
            }
        }
        catch (Exception &e) {
            fprintf(stderr, "Failed: [%d] %s", e.nErrno, strerror(e.nErrno));
        }
        printf("\n");
    }
}
