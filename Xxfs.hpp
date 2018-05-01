#ifndef XXFS_XXFS_HPP_
#define XXFS_XXFS_HPP_

#include "Common.hpp"

#include "BitmapAllocator.hpp"
#include "ClusterCache.hpp"
#include "InodeCache.hpp"
#include "OpenedFile.hpp"
#include "OpenedDir.hpp"
#include "Raii.hpp"

namespace xxfs {

class Xxfs {
private:
    friend class InodeCache;
    friend FilePtrR;
    friend FilePtrW;
    
public:
    Xxfs(RaiiFile &&vRf, ShrPtr<MetaCluster> &&spcMeta);

public:
    // returns inode number, increase cLookup when hit
    uint32_t Lookup(FileStat &vStat, uint32_t linPar, const char *pszName);
    void Forget(uint32_t lin, uint64_t cLookup);
    void GetAttr(FileStat &vStat, uint32_t lin);
    void SetAttr(FileStat &vStat, FileStat *pStat, uint32_t lin, int nFlags, fuse_file_info *pInfo);
    const char *ReadLink(uint32_t lin);
    uint32_t MkDir(FileStat &vStat, uint32_t linPar, const char *pszName);
    void Unlink(uint32_t linPar, const char *pszName);
    void RmDir(uint32_t linPar, const char *pszName);
    uint32_t SymLink(FileStat &vStat, const char *pszLink, uint32_t linPar, const char *pszName);
    void Rename(
        uint32_t linPar, const char *pszName,
        uint32_t linNewPar, const char *pszNewName,
        unsigned uFlags
    );
    void Link(FileStat &vStat, uint32_t lin, uint32_t linNewPar, const char *pszNewName);
    OpenedFile *Open(uint32_t lin, fuse_file_info *pInfo);
    uint64_t Read(OpenedFile *pFile, std::unique_ptr<char []> &upBuf, uint64_t cbSize, uint64_t cbOff);
    uint64_t Write(OpenedFile *pFile, const void *pBuf, uint64_t cbSize, uint64_t cbOff);
    void Release(OpenedFile *pFile) noexcept;
    void FSync(OpenedFile *pFile) noexcept;
    OpenedDir *OpenDir(uint32_t lin);
    size_t ReadDir(OpenedDir *pDir, fuse_req_t pReq, char *pBuf, size_t cbSize, off_t vOff);
    void ReleaseDir(OpenedDir *pDir) noexcept;
    void StatFs(VfsStat &vStat) const noexcept;
    std::pair<uint32_t, OpenedFile *> Create(FileStat &vStat, uint32_t linPar, const char *pszName);

public:
    // get count of free cluster
    uint32_t AvailClu() const noexcept;

private:
    // allocate a free cluster and update meta cluster
    ShrPtr<InodeCluster> Y_MapInoClu(uint32_t vcn) noexcept;
    uint32_t Y_AllocIno();
    // invoked when both lookup count and link count are 0
    void Y_EraseIno(uint32_t lin, Inode *pi) noexcept;
    // noexcept, assume mmap does not fail
    template<class tObj>
    inline ShrPtr<tObj> Y_Map(uint32_t lcn) noexcept {
        return x_vCluCache.At<tObj>(lcn);
    }

private:
    // allocate a free cluster and update inode if lin is 0
    // invokes Y_AllocClu
    void Y_FileAllocClu(Inode *pi, uint32_t &lcn);
    // only used in shrink
    // check if each lcn is 0, do not double free
    void Y_FileFreeClu(Inode *pi, uint32_t &lcn) noexcept;
    void Y_FileFreeIdx1(Inode *pi, uint32_t &lcn, uint32_t vcnFrom = 0) noexcept;
    void Y_FileFreeIdx2(Inode *pi, uint32_t &lcn, uint32_t vcnFrom = 0) noexcept;
    void Y_FileFreeIdx3(Inode *pi, uint32_t &lcn, uint32_t vcnFrom = 0) noexcept;
    // invoked when fsync and close
    void Y_FileShrink(Inode *pi) noexcept;

private:
    constexpr static void X_FillStat(FileStat &vStat, uint32_t lin, Inode *pNod) noexcept;

private:
    RaiiFile x_vRf;
    ShrPtr<MetaCluster> x_spcMeta;
    ClusterCache<65536> x_vCluCache;
    InodeCache x_vInoCache;
    BitmapAllocator x_vCluAlloc;
    BitmapAllocator x_vInoAlloc;
    
};

}

#endif
