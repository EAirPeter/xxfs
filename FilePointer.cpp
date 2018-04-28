#include "Common.hpp"

#include "FilePointer.hpp"
#include "Xxfs.hpp"

namespace xxfs {

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
}

template ShrPtr<void> FilePointer::X_Seek<false>(Xxfs *px, Inode *pi, uint32_t vcn) noexcept;
template ShrPtr<void> FilePointer::X_Seek<true>(Xxfs *px, Inode *pi, uint32_t vcn);

}
