#ifndef XXFS_XXFS_HPP_
#define XXFS_XXFS_HPP_

#include "Common.hpp"

#include "BitmapAllocator.hpp"
#include "Raii.hpp"

class Xxfs {
private:
    struct X_OpenedInodeBlock {
        MapPtr<InodeBlock> rpBlk;
        uint64_t cNodLookup[kcNodPerBlk] {};
        uint64_t cBlkLookup = 0;

        X_OpenedInodeBlock(MapPtr<InodeBlock> &&rpBlk_);
    };
    
    struct X_FilePointer {
        uint32_t idxBlkInFile;
        MapPtr<FileBlock> rpData;
    };

    struct X_OpenedFile {
        X_FilePointer vR;
        X_FilePointer vW;
        Xxfs *pXxfs;
        Inode *pNod;
        uint32_t bWrite : 1;
        uint32_t bAppend : 1;
        uint32_t bDirect : 1;
        uint32_t : 0;

        X_OpenedFile(Xxfs *pXxfs_, Inode *pNod_, bool bWrite_, bool bAppend_, bool bDirect_);
        
        template<bool kAlloc = false>
        void Seek(X_FilePointer &v, uint32_t idxNewBlkInFile);
        
        // DoXxx does not check any thing about the file itself
        // DoXxx only transfers data
        void DoRead(void *pBuf, uint64_t cbSize, uint64_t cbOff);
        uint64_t DoWrite(const void *pBuf, uint64_t cbSize, uint64_t cbOff);
        void DoSync() noexcept;
    };

    // there's no hole in directory file
    struct X_OpenedDir {
        DefArr<MapPtr<DirBlock>> rpBlks;
        Xxfs *pXxfs;
        Inode *pNod;
        uint32_t aStk[kcEntPerBlk];
        uint32_t cStkSize = 0;
        char szName[kcEntPerBlk];
        
        X_OpenedDir(Xxfs *pXxfs_, Inode *pNod);
        
        DirEnt *GetEnt(uint32_t eno);
        // returns inode number
        uint32_t DoLookup(const char *pszName);

        // for readdir
        void IterSeek(off_t vOff);
        const char *IterNext(struct stat &vStat, off_t &vOff);

        void TrieSetMin();
        // not empty, 
        void TrieNext();

        uint32_t &StkTop();
        void StkPush(uint32_t eno);
        uint32_t StkPop();
    };

public:
    Xxfs(RaiiFile &&roFile, MapPtr<SuperBlock> &&rpSupBlk);

public:
    // returns inode number, increase cLookup when hit
    uint32_t Lookup(struct stat &vStat, uint32_t inoPar, const char *pszName);
    void Forget(uint32_t ino, uint64_t cLookup);
    void GetAttr(struct stat &vStat, uint32_t ino);
    MapPtr<char> ReadLink(uint32_t ino);
    uint32_t MkDir(struct stat &vStat, uint32_t inoPar, const char *pszName);
    void Unlink(uint32_t inoPar, const char *pszName);
    void RmDir(uint32_t inoPar, const char *pszName);
    uint32_t SymLink(struct stat &vStat, const char *pszLink, uint32_t inoPar, const char *pszName);
    void Rename(
        uint32_t inoPar, const char *pszName,
        uint32_t inoNewPar, const char *pszNewName,
        unsigned uFlags
    );
    void Link(struct stat &vStat, uint32_t ino, uint32_t inoNewPar, const char *pszNewName);
    void *Open(uint32_t ino, fuse_file_info *pInfo);
    uint64_t Read(void *pParam, PodPtr<uint8_t> &rpBuf, uint64_t cbSize, uint64_t cbOff);
    uint64_t Write(void *pParam, const uint8_t *pBuf, uint64_t cbSize, uint64_t cbOff);
    void Release(void *pParam) noexcept;
    void FSync(void *pParam) noexcept;
    void *OpenDir(uint32_t ino);
    size_t ReadDir(void *pParam, fuse_req_t pReq, char *pBuf, size_t cbSize, off_t vOff);
    
private:
    
    /*
    // returns inode number
    uint32_t X_Lookup(Inode *pParNod, const char *pszName);
    uint32_t X_Create(Inode *pParNod, const char *pszName);
    
    
    template<bool kReserve = false>
    PodPtr<DirEnt> X_DirOpen(Inode *pNod);
    
    // pEnts can hold (cBlock + 1) blocks
    uint32_t X_DirAlloc(DirEnt *pEnts, uint32_t &cBlock);
    
    template<bool kCreate = false>
    DirEnt *X_DirLookup(DirEnt *pEnts, const char *pszName);
*/
private:
    template<bool kAddRef = false>
    Inode *X_GetInode(uint32_t ino);
    template<class tBlock>
    uint32_t X_AllocFor(Inode *pNod, MapPtr<tBlock> &rpBlk);
    template<class tBlock, bool kAlloc = false>
    uint32_t X_Seek(Inode *pNod, MapPtr<tBlock> &rpBlk, uint32_t idxBlkInFile);
    template<class tBlock>
    MapPtr<tBlock> X_Map(uint32_t bno);

private:
    static void X_FillStat(struct stat &vStat, uint32_t ino, Inode *pNod);

private:
    RaiiFile x_roFile;
    MapPtr<SuperBlock> x_rpSupBlk;
    BitmapAllocator x_bmpBno;
    BitmapAllocator x_bmpIno;
    std::unordered_map<uint32_t, X_OpenedInodeBlock> x_mapNodBlks;
    uint32_t x_aCreatedEnts[256];
    uint32_t x_cCreateEnt;
    
};

#endif
