XXFS  
====
A simple filesystem based on FUSE 3  

# Build  
* Dependencies: `pkg-config`, `fuse3`  
* Language: `C++17`  
* Command: `make clean all`  

# Usage  
## XXFS  
Load a file or device and mount it using XXFS filesystem.  
```
xxfs [-f] [-v] <filepath> <mountpoint>

-f          run in foreground (default: run in background)
-v          enable verbose mode (which produces more output)
filepath    the file or device
mountpoint  literally, a mount point
```

## MKXXFS
Format a file or device to XXFS.  
```
mkxxfs <path>

path: the file or device
```

## CLUXX  
Examine and extract information of a XXFS file or device
```
cluxx <filepath> [options]

filepath    the file or device
options:
-m          print the meta cluster
-b          check the bitmap, which should agree with the meta cluster
-x i j      print the j-th index at i-th cluster
-i i        print the i-th inode
-d i j      print the j-th dirent at i-th cluster
-o i path   dump the i-th cluster to a file
```
