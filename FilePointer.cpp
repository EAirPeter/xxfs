#include "Common.hpp"

#include "FilePointer.hpp"
#include "Xxfs.hpp"

namespace xxfs {

template<bool kAlloc>
ShrPtr<void> FilePointer<kAlloc>::X_Seek(Xxfs *px, Inode *pi, uint32_t vcn) noexcept(!kAlloc) {
    if (x_sp && vcn == x_vcn) {
        px->Y_Touch(x_lcn);
        return x_sp;
    }
    if (vcn < kvcnIdx1) {
        X_Seek0(px, pi, 0, vcn, pi->lcnIdx0);
        return x_sp;
    }
    if (x_sp1 && x_vcn1 <= vcn && vcn < x_vcn1 + kccIdx1) {
        px->Y_Touch(x_lcn1);
        X_Seek0(px, pi, x_vcn1, vcn - x_vcn1, kAlloc || x_sp1 ? x_sp1->aLcns : nullptr);
        return x_sp;
    }
    if (vcn < kvcnIdx2) {
        X_Seek1(px, pi, kvcnIdx1, vcn - kvcnIdx1, &pi->lcnIdx1);
        return x_sp;
    }
    if (x_sp2 && x_vcn2 <= vcn && vcn < x_vcn2 + kccIdx2) {
        px->Y_Touch(x_lcn2);
        X_Seek1(px, pi, x_vcn2, vcn - x_vcn2, kAlloc || x_sp2 ? x_sp2->aLcns : nullptr);
        return x_sp;
    }
    if (vcn < kvcnIdx3) {
        X_Seek2(px, pi, kvcnIdx2, vcn - kvcnIdx2, &pi->lcnIdx2);
        return x_sp;
    }
    if (x_sp3)
        px->Y_Touch(pi->lcnIdx3);
    else {
        if (kAlloc && !pi->lcnIdx3)
            x_sp3 = px->Y_FileAllocClu<IndexCluster>(pi, pi->lcnIdx3);
        else if (pi->lcnIdx3)
            x_sp3 = px->Y_Map<IndexCluster>(pi->lcnIdx3);
    }
    X_Seek2(px, pi, kvcnIdx3, vcn - kvcnIdx3, kAlloc || x_sp3 ? x_sp3->aLcns : nullptr);
    return x_sp;
}

template<bool kAlloc>
void FilePointer<kAlloc>::X_Seek0(
    Xxfs *px, Inode *pi, uint32_t vcnOff, uint32_t vcn, uint32_t *pLcns
) noexcept(!kAlloc) {
    x_vcn = vcnOff + vcn;
    x_sp.reset();
    if (pLcns) {
        if (kAlloc && !pLcns[vcn])
            x_sp = px->Y_FileAllocClu<void>(pi, pLcns[vcn]);
        else if (pLcns[vcn])
            x_sp = px->Y_Map<void>(pLcns[vcn]);
        x_lcn = pLcns[vcn];
    }
}

template<bool kAlloc>
void FilePointer<kAlloc>::X_Seek1(
    Xxfs *px, Inode *pi, uint32_t vcnOff, uint32_t vcn, uint32_t *pLcns
) noexcept(!kAlloc) {
    auto vcn1 = vcn % kccIdx1;
    auto idx1 = vcn / kccIdx1;
    x_vcn1 = vcnOff + kccIdx1 * idx1;
    x_sp1.reset();
    if (pLcns) {
        if (kAlloc && !pLcns[idx1])
            x_sp1 = px->Y_FileAllocClu<IndexCluster>(pi, pLcns[idx1]);
        else if (pLcns[idx1])
            x_sp1 = px->Y_Map<IndexCluster>(pLcns[idx1]);
        x_lcn1 = pLcns[idx1];
    }
    X_Seek0(px, pi, x_vcn1, vcn1, kAlloc || x_sp1 ? x_sp1->aLcns : nullptr);
}

template<bool kAlloc>
void FilePointer<kAlloc>::X_Seek2(
    Xxfs *px, Inode *pi, uint32_t vcnOff, uint32_t vcn, uint32_t *pLcns
) noexcept(!kAlloc) {
    auto vcn2 = vcn % kccIdx2;
    auto idx2 = vcn / kccIdx2;
    x_vcn2 = vcnOff + kccIdx2 * idx2;
    x_sp2.reset();
    if (pLcns) {
        if (kAlloc && !pLcns[idx2])
            x_sp2 = px->Y_FileAllocClu<IndexCluster>(pi, pLcns[idx2]);
        else if (pLcns[idx2])
            x_sp2 = px->Y_Map<IndexCluster>(pLcns[idx2]);
        x_lcn2 = pLcns[idx2];
    }
    X_Seek1(px, pi, x_vcn2, vcn2, kAlloc || x_sp2 ? x_sp2->aLcns : nullptr);
}

template class FilePointer<false>;
template class FilePointer<true>;

}
