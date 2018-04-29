#include "Common.hpp"

#include "FilePointer.hpp"
#include "Xxfs.hpp"

namespace xxfs {

template<bool kAlloc>
ShrPtr<void> FilePointer<kAlloc>::X_Seek(Xxfs *px, Inode *pi, uint32_t vcn) noexcept(!kAlloc) {
    if (x_bVld && vcn == x_vcn)
        return x_sp;
    if (vcn < kvcnIdx1) {
        X_Seek0(px, pi, 0, vcn, pi->lcnIdx0);
        return x_sp;
    }
    if (x_bVld1 && x_vcn1 <= vcn && vcn < x_vcn1 + kccIdx1) {
        X_Seek0(px, pi, x_vcn1, vcn - x_vcn1, kAlloc || x_sp1 ? x_sp1->aLcns : nullptr);
        return x_sp;
    }
    if (vcn < kvcnIdx2) {
        X_Seek1(px, pi, kvcnIdx1, vcn - kvcnIdx1, &pi->lcnIdx1);
        return x_sp;
    }
    if (x_bVld2 && x_vcn2 <= vcn && vcn < x_vcn2 + kccIdx2) {
        X_Seek1(px, pi, x_vcn2, vcn - x_vcn2, kAlloc || x_sp2 ? x_sp2->aLcns : nullptr);
        return x_sp;
    }
    if (vcn < kvcnIdx3) {
        X_Seek2(px, pi, kvcnIdx2, vcn - kvcnIdx2, &pi->lcnIdx2);
        return x_sp;
    }
    if (!x_bVld3) {
        x_bVld3 = true;
        if (kAlloc && !pi->lcnIdx3)
            px->Y_FileAllocClu(pi, pi->lcnIdx3);
        if (pi->lcnIdx3)
            x_sp3 = px->Y_Map<IndexCluster>(pi->lcnIdx3);
    }
    X_Seek2(px, pi, kvcnIdx3, vcn - kvcnIdx3, kAlloc || x_sp3 ? x_sp3->aLcns : nullptr);
    return x_sp;
}

template<bool kAlloc>
void FilePointer<kAlloc>::X_Seek0(
    Xxfs *px, Inode *pi, uint32_t vcnOff, uint32_t vcn, uint32_t *pLcns
) noexcept(!kAlloc) {
    x_bVld = true;
    x_vcn = vcnOff + vcn;
    x_sp.reset();
    if (pLcns) {
        if (kAlloc && !pLcns[vcn])
            px->Y_FileAllocClu(pi, pLcns[vcn]);
        if (pLcns[vcn])
            x_sp = px->Y_Map<void>(pLcns[vcn]);
    }
}

template<bool kAlloc>
void FilePointer<kAlloc>::X_Seek1(
    Xxfs *px, Inode *pi, uint32_t vcnOff, uint32_t vcn, uint32_t *pLcns
) noexcept(!kAlloc) {
    auto vcn1 = vcn % kccIdx1;
    auto idx1 = vcn / kccIdx1;
    x_bVld1 = true;
    x_vcn1 = vcnOff + kccIdx1 * idx1;
    x_sp1.reset();
    if (pLcns) {
        if (kAlloc && !pLcns[idx1])
            px->Y_FileAllocClu(pi, pLcns[idx1]);
        if (pLcns[idx1])
            x_sp1 = px->Y_Map<IndexCluster>(pLcns[idx1]);
    }
    X_Seek0(px, pi, x_vcn1, vcn1, kAlloc || x_sp1 ? x_sp1->aLcns : nullptr);
}

template<bool kAlloc>
void FilePointer<kAlloc>::X_Seek2(
    Xxfs *px, Inode *pi, uint32_t vcnOff, uint32_t vcn, uint32_t *pLcns
) noexcept(!kAlloc) {
    auto vcn2 = vcn % kccIdx2;
    auto idx2 = vcn / kccIdx2;
    x_bVld2 = true;
    x_vcn2 = vcnOff + kccIdx2 * idx2;
    x_sp2.reset();
    if (pLcns) {
        if (kAlloc && !pLcns[idx2])
            px->Y_FileAllocClu(pi, pLcns[idx2]);
        if (pLcns[idx2])
            x_sp2 = px->Y_Map<IndexCluster>(pLcns[idx2]);
    }
    X_Seek1(px, pi, x_vcn2, vcn2, kAlloc || x_sp2 ? x_sp2->aLcns : nullptr);
}

/*
template<bool kAlloc>
ShrPtr<void> FilePointer::X_Seek(Xxfs *px, Inode *pi, uint32_t vcn) noexcept(!kAlloc) {
    if (!x_bVld || vcn != x_vcn) {
        x_bVld = true;
        x_sp.reset();
        uint32_t *pLcn = nullptr;
        if (vcn < kvcnIdx1) {
            x_vcn = vcn;
            pLcn = &pi->lcnIdx0[vcn];
        }
        else {
            if (!x_bVld1 || vcn < x_vcn1 || x_vcn1 + kccIdx1 <= vcn) {
                x_bVld1 = true;
                x_sp1.reset();
                uint32_t *pLcn1 = nullptr;
                if (vcn < kvcnIdx2) {
                    vcn -= kvcnIdx1;
                    x_vcn1 = kvcnIdx1;
                    pLcn1 = &pi->lcnIdx1;
                }
                else {
                    if (!x_bVld2 || vcn < x_vcn2 || x_vcn2 + kccIdx2 <= vcn) {
                        x_bVld2 = true;
                        x_sp2.reset();
                        uint32_t *pLcn2 = nullptr;
                        if (vcn < kvcnIdx3) {
                            vcn -= kvcnIdx2;
                            x_vcn2 = kvcnIdx2;
                            pLcn2 = &pi->lcnIdx2;
                        }
                        else {
                            if (!x_bVld3) {
                                x_bVld3 = true;
                                auto &lcn3 = pi->lcnIdx3;
                                if constexpr (kAlloc)
                                    px->Y_FileAllocClu(pi, lcn3);
                                if (lcn3)
                                    x_sp3 = px->Y_Map<IndexCluster>(lcn3);
                            }
                            vcn -= kvcnIdx3;
                            x_vcn2 = kvcnIdx3 + vcn / kccIdx2 * kccIdx2;
                            if (x_sp3)
                                pLcn2 = &x_sp3->aLcns[vcn / kccIdx2];
                            vcn %= kvcnIdx2;
                        }
                        if (pLcn2) {
                            auto &lcn2 = *pLcn2;
                            if constexpr (kAlloc)
                                px->Y_FileAllocClu(pi, lcn2);
                            if (lcn2)
                                x_sp2 = px->Y_Map<IndexCluster>(lcn2);
                        }
                    }
                    x_vcn1 = x_vcn2 + vcn / kccIdx1 * kccIdx1;
                    if (x_sp2)
                        pLcn1 = &x_sp2->aLcns[vcn / kccIdx1];
                    vcn %= kccIdx1;
                }
                if (pLcn1) {
                    auto &lcn1 = *pLcn1;
                    if constexpr (kAlloc)
                        px->Y_FileAllocClu(pi, lcn1);
                    if (lcn1)
                        x_sp1 = px->Y_Map<IndexCluster>(lcn1);
                }
            }
            x_vcn = x_vcn1 + vcn;
            if (x_sp1)
                pLcn = &x_sp1->aLcns[vcn];
        }
        if (pLcn) {
            auto &lcn = *pLcn;
            if constexpr (kAlloc)
                px->Y_FileAllocClu(pi, lcn);
            if (lcn)
                x_sp = px->Y_Map<tClu>(lcn);
        }
    }
    return x_sp;
}*/

template class FilePointer<false>;
template class FilePointer<true>;

}
