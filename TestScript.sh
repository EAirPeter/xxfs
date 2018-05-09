function test0 ()
{
    echo "$DIVIDING_LINE"
    echo "test 0: mount/umount test"
    printf "    mount filesystem ... "
    eval " $FUSE_MAIN $VOLUME_FILE $MOUNT_POINT "
    if [ $? -ne 0 ]; then
        echo "failed"
        return 1
    fi
    echo "    cd into the mount point ... "
    old_pwd="$PWD"
    cd "$MOUNT_POINT"
    if [ $? -ne 0 ]; then
        echo "failed"
        cd "$old_pwd"
        return 1
    fi
    echo "done"
    cd "$old_pwd"

    sleep .5

    printf "    try to umount ... "
    fusermount -u "$MOUNT_POINT"
    if [ $? -ne 0 ]; then
        echo "failed"
        return 1
    fi

    sleep .5
    eval " $FUSE_MAIN $VOLUME_FILE $MOUNT_POINT "

    echo "done"
}

function test1 ()
{
    echo "$DIVIDING_LINE"
    echo "test 1: mkdir/rmdir in root directory"

    # make directory
    echo "test 1.1: mkdir testdir"
    old_pwd="$PWD"
    cd "$MOUNT_POINT"
    mkdir "testdir"
    if [[ ($? -eq 0) && (-e "testdir") ]]; then
            printf "    testdir exists, "
            # if it is a directory
            if [ -d "testdir" ]; then
                echo "and is indeed a directory"
                echo "test 1.1 passed"
            else
                echo "but is not a directory"
                echo "test 1.1 failed"
                return 1
            fi
    else
        echo "    mkdir failed or testdir not exist"
        echo "test 1.1 failed"
        return 1
    fi
    
    # remove directory
    echo "test 1.2: rmdir testdir"
    rmdir "testdir"
    if [[ ($? -eq 0) && !(-e "testdir") ]]; then
            echo "    the directory was removed"
            echo "test 1.2 passed"
    else 
        echo "    rm failed or directory still exist"
        echo "test 1.2 failed"
        return 1
    fi

    # make multiple directory and delete them
    echo "test 1.3: mkdir multiple directory"
    for i in {a..b}
    do
        mkdir "$i"
        if [ $? -ne 0 ]; then
            echo "test 1.3 failed"
            return 1
        fi
    done
    echo   "    mkdir done"
    printf "    check if they did exist: "
    for i in {a..b}
    do
        if ! [ -d "$i" ]; then
            echo "directory $i not exists"
            return 1
        fi
    done
    echo "all exist"
    printf "    try to remove them: "
    for i in {a..b}
    do
        rmdir "$i"
        if [[ ($? -ne 0) || (-e "$i") ]]; then
            echo "    rm failed or directory still exist"
            echo "test 1.3 failed"
            return 1
        fi
    done
    echo "done"
    echo "test 1.3 passed"
    cd "$old_pwd"
    return 0
}

function test2 ()
{
    echo "$DIVIDING_LINE"
    echo "test 2: nested directory"
    local old_pwd="$PWD"
    cd "$MOUNT_POINT"

    # make 16-layer nested directory
    printf "    start ... "
    for i in {1..16}
    do
        mkdir "$i"
        cd "$i"
        if [ $? -ne 0 ]; then
            echo "    failed to cd into $i, it might not be created successfully or can not be entered"
            return 1
        fi
    done
    for i in {16..1..-1}
    do
        cd ".."
        rmdir "$i"
        if [ $? -ne 0 ]; then
            echo "    failed to rmdir $i, it might not be created successfully or cd .. was not handled correctly"
            return 1
        fi
        if [ -e $i ]; then
            echo "    directory $i still exist"
            return 1
        fi
    done
    echo "done"
    cd "$old_pwd"
    return 0
}

function test3 ()
{
    echo "$DIVIDING_LINE"
    echo "test 3: nested directory with small charater file creating and removing"
    local old_pwd="$PWD"
    cd "$MOUNT_POINT"

    echo "test 3.1: create single file in root directory"
    echo "    echo \"$TEST_STRING\" >> testfile"
    echo "$TEST_STRING" >> "testfile"
    printf "    check if it exist:"
    if [[ ($? -eq 0) && (-e "testfile") ]]; then
        printf " testfile exists, "
        if [ -f "testfile" ]; then
            echo "and is indeed a file"
            echo "    check if the content is correct"
            content=$(cat "testfile")
            if [ "$content" = "$TEST_STRING" ]; then
                echo "test 3.1 passed"
            else
                echo "    content mismatch, actual content: $content"
                echo "test 3.1 failed"
                return 1
            fi
        else
            echo "but is not a file"
            echo "test 3.1 failed"
            return 1
        fi
    else
        echo "test 3.1 failed"
        return 1
    fi

    echo "test 3.2: remove the file that just created"
    echo "    rm testfile"
    rm "testfile"
    if [[ ($? -eq 0) && !(-e "testfile") ]]; then
            echo "    the file was removed"
            echo "test 3.2 passed"
    else 
        echo "    rm failed or file still exist"
        echo "test 3.2 failed"
        return 1
    fi

    echo "test 3.3: create multiple files in root directory"
    printf "    start ... "
    for i in {a..p}
    do
        echo "$i:$TEST_STRING" >> "$i"
        content=$(cat "$i")
        if ! [[ "$content" = "$i:$TEST_STRING" ]]; then
            echo "    content mismatch, actual content: $content"
            echo "test 3.3 failed"
            return 1
        fi
    done
    echo "done"
    echo "test 3.3 passed"
    
    echo "test 3.4: remove the files just created"
    printf "    start ... "
    for i in {a..p}
    do
        rm "$i"
        if [[ ($? -ne 0) || (-e "$i") ]]; then
            echo "    rm failed or file still exist"
            echo "test 3.4 failed"
            return 1
        fi
    done
    echo "done"
    echo "test 3.4 passed"

    echo "test 3.5: multiple files in nested directory"
    printf "    start ... "
    for i in {1..16}
    do
        mkdir "$i"
        cd "$i"
        for j in {a..p}
        do
            echo "$i$j:$TEST_STRING" >> "$j"
            content=$(cat "$j")
            if ! [ "$content" = "$i$j:$TEST_STRING" ]; then
                echo "    content mismatch, actual content: $content"
                echo "                    expected content: $i$j:$TEST_STRING"
                echo "test 3.5 failed"
                return 1
            fi
        done
    done
    echo "done"
    echo "test 3.5 passed"

    echo "test 3.6: recursively remove directories and files"
    cd "$old_pwd"
    cd "$MOUNT_POINT"
    echo "    rm -r 1"
    rm -r 1
    if [[ ($? -ne 0) || (-e "1") ]]; then
        echo "    rm failed or file still exist"
        echo "test 3.6 failed"
        return 1
    fi

    cd "$old_pwd"
    return 0
}

function test4 ()
{
    echo "$DIVIDING_LINE"
    echo "test 4: small files' integrity after umount"
    local old_pwd="$PWD"
    cd "$MOUNT_POINT"

    printf "    create multiple files in nested directory ... "
    for i in {1..16}
    do
        mkdir "$i"
        cd "$i"
        for j in {a..p}
        do
            echo "$i$j:$TEST_STRING" >> "$j"
        done
    done
    echo "done"
    
    printf "    umount filesystem ... "
    cd "$old_pwd"
    fusermount -u "$MOUNT_POINT"
    if [ $? -ne 0 ]; then
        echo "failed"
        return 1
    fi
    echo "done"

    printf "    mount filesystem ... "
    eval " $FUSE_MAIN $VOLUME_FILE $MOUNT_POINT "
    if [ $? -ne 0 ]; then
        echo "failed"
        return 1
    fi
    echo "done"

    cd "$MOUNT_POINT"
    printf "    check files integrity ... "
    for i in {1..16}
    do
        cd "$i"
        for j in {a..p}
        do
            content=$(cat "$j")
            if ! [ "$content" = "$i$j:$TEST_STRING" ]; then
                echo "    content mismatch, actual content: $content"
                echo "                    expected content: $i$j:$TEST_STRING"
                return 1
            fi
        done
    done
    echo "done"
    cd "$old_pwd"
    cd "$MOUNT_POINT"
    rm -r "1"
    cd "$old_pwd"
    return 0
}

function test5 ()
{
    echo "$DIVIDING_LINE"
    echo "test 5: large file (up to 250MB) hashval and integrity check"
    local old_pwd="$PWD"
    cd "$MOUNT_POINT"

    printf "    generate large file ... "
    dd if=/dev/zero of=/tmp/dat count=204800 bs=1024
    echo "done"
    orig_hash=($(md5sum /tmp/dat))
    echo "    its hash value should be $file_hash"
    
    printf "    copy it into your file system ... "
    mv /tmp/dat .
    if [ $? -ne 0 ]; then
        echo "failed"
        return 1
    fi
    echo "done"

    printf "    check hash value ... "
    file_hash=($(md5sum dat))
    if ! [ "$file_hash" == "$orig_hash" ]; then
        echo "mismatch"
        echo "     current hash value is $file_hash"
        echo "                  expected $orig_hash"
        return 1
    fi
    echo "done"

    printf "    umount filesystem ... "
    cd "$old_pwd"
    fusermount -u "$MOUNT_POINT"
    if [ $? -ne 0 ]; then
        echo "failed"
        return 1
    fi
    echo "done"

    printf "    mount filesystem ... "
    eval " $FUSE_MAIN $VOLUME_FILE $MOUNT_POINT "
    if [ $? -ne 0 ]; then
        echo "failed"
        return 1
    fi
    echo "done"

    cd "$MOUNT_POINT"
    printf "    check hash value ... "
    file_hash=($(md5sum dat))
    if ! [ "$file_hash" == "$orig_hash" ]; then
        echo "mismatch"
        echo "     current hash value is $file_hash"
        echo "                  expected $orig_hash"
        return 1
    fi
    echo "done"

    echo "    check filesystem memory usage ... "
    declare -i tmp1=$(ps -A -F | grep "$FUSE_MAIN" | awk -F" " '{print $6}' | head -1)
    declare -i tmp2=$(ps -A -F | grep "$FUSE_MAIN" | awk -F" " '{print $6}' | tail -1)
    declare -i tmp
    if (( $tmp1 > $tmp2 )); then
        tmp=tmp1
    else tmp=tmp2
    fi
    printf "$tmp KB. "
    declare -i ratio=$(( $tmp*10/204800 ))
    if [ $ratio -ge 10 ]; then
        echo "in-memory file system"
        IS_MEMORY=0
    else
        echo "on-disk file system"
        IS_MEMORY=1
    fi

    printf "    delete it ... "
    rm "dat"
    if [ $? -ne 0  ]; then
        echo "failed"
        return 1
    fi
    echo "done"

    cd "$old_pwd"
    return 0
}

function test6 ()
{
    echo "$DIVIDING_LINE"
    echo "test 6: symbolic link"
    local old_pwd="$PWD"
    cd "$MOUNT_POINT"

    echo "test 6.1: symbolic link between files in your file system"
    echo "    create file"
    echo "$TEST_STRING" > "file"

    printf "    create link ... "
    ln -s "file" "link"
    if [ $? -ne 0  ]; then
        echo "failed"
        return 1
    else 
        echo "done"
        if ! [ -L "link" ]; then
            echo "    but the symlink does not exist or is not a symlink."
            return 1
        fi
    fi

    printf "    delete symbolic link ... "
    rm "link"
    if [[ ($? -ne 0) || (-L "link") ]]; then
        echo "failed"
        return 1
    fi
    rm "file"
    echo "done"
    echo "test 6.1 passed"


    echo "test 6.2: symbolic link between files in /tmp and your file system"
    echo "    create file under /tmp"
    echo "$TEST_STRING" > "/tmp/fs_testfile"

    printf "    create link ... "
    ln -s "/tmp/fs_testfile" "link"
    if [ $? -ne 0  ]; then
        echo "failed"
        return 1
    else 
        echo "done"
        if ! [ -L "link" ]; then
            echo "    but the symlink does not exist or is not a symlink."
            return 1
        fi
    fi

    printf "    delete symbolic link ... "
    rm "link"
    if [[ ($? -ne 0) || (-L "link") ]]; then
        echo "failed"
        return 1
    fi
    rm "/tmp/fs_testfile"
    echo "done"
    echo "test 6.2 passed"

    cd "$old_pwd"
    return 0
}

function testWrite ()
{
    local temp_spd
    local runs
    local num
    # local res
    local unit

    echo "performace test: write"
    runs=0
    wres=0
    while [ $runs -lt 5 ]; do
        temp_spd=$(dd if=/dev/zero of=$MOUNT_POINT/testfile bs=64M count=2 oflag=sync 2>&1)
        temp_spd=$(echo ${temp_spd##*,})
        num=$(echo $temp_spd | awk '{print $1}')
        unit=$(echo $temp_spd | awk '{print $2}')
        if [ "$unit" = "GB/s" ]; then num=$(echo $num|awk '{print $1*1024}'); fi
        if [ "$unit" = "KB/s" ]; then num=$(echo $num|awk '{print $1/1024}'); fi
        if [ "$unit" = "kB/s" ]; then num=$(echo $num|awk '{print $1/1024}'); fi
        wres=$(echo $wres $num|awk '{print $1+$2}')
        let runs=runs+1
        echo "Pass" $runs ":" $num "MB/s"
        rm $MOUNT_POINT/testfile
    done
    wres=$(echo $wres|awk '{print $1/5}')
    echo "average write speed: $wres MB/s"
    echo "performace test: read"
    runs=0
    rres=0
    (dd if=/dev/zero of=$MOUNT_POINT/testfile bs=64M count=2 oflag=sync 2>&1) > /dev/null
    while [ $runs -lt 5 ]; do
        temp_spd=$(dd if=$MOUNT_POINT/testfile of=/dev/null bs=64M count=2 iflag=nocache oflag=sync 2>&1)
        temp_spd=$(echo ${temp_spd##*,})
        num=$(echo $temp_spd | awk '{print $1}')
        unit=$(echo $temp_spd | awk '{print $2}')
        if [ "$unit" = "GB/s" ]; then num=$(echo $num|awk '{print $1*1024}'); fi
        if [ "$unit" = "KB/s" ]; then num=$(echo $num|awk '{print $1/1024}'); fi
        rres=$(echo $rres $num|awk '{print $1+$2}')
        let runs=runs+1
        echo "Pass" $runs ":" $num "MB/s"
    done
    rres=$(echo $rres|awk '{print $1/5}')
    echo "average read speed: $rres MB/s"
    rm $MOUNT_POINT/testfile
    return 0
}

function doTest()
{
    test1
    if [ $? -eq 1 ]; then
        echo "test 1 failed"
        echo "see the log above for detailed information"
        return 1
    else
        echo "test 1 passed"
    fi

    test2
    if [ $? -eq 1 ]; then
        echo "test 2 failed"
        echo "see the log above for detailed information"
        return 1
    else
        echo "test 2 passed"
    fi

    test3
    if [ $? -eq 1 ]; then
        echo "test 3 failed"
        echo "see the log above for detailed information"
        return 1
    else
        echo "test 3 passed"
    fi

    test4
    if [ $? -eq 1 ]; then
        echo "test 4 failed"
        echo "see the log above for detailed information"
        return 1
    else
        echo "test 4 passed"
    fi
    
    test5
    T5RES=$?
    if [ "$T5RES" = "1" ]; then
        echo "test 5 failed"
        echo "see the log above for detailed information"
    else
        echo "test 5 passed"
    fi

    test6
    T6RES=$?
    if [ "$T6RES" = "1" ]; then
        echo "test 6 failed"
        echo "see the log above for detailed information"
    else
        echo "test 6 passed"
    fi

    testWrite
    return 0
}

function PrintSummary ()
{
    echo "basic function test passed"
    printf "large file test: "
    if [ "$T5RES" = "1" ]; then
        echo "failed"
    else
        echo "passed"
        printf "file system type: "
        if [ "$IS_MEMORY" = "1" ]; then
            echo "on-disk file system"
        else 
            echo "in-memory file system"
        fi
    fi
    printf "symbolic link test: "
    if [ "$T6RES" = "1" ]; then
        echo "failed"
    else 
        echo "passed"
    fi
    echo "read write performance test result:"
    echo "    average write speed: $wres MB/s"
    echo "    average  read speed: $rres MB/s"
}

if  [ $# -lt 3 ]; then
    echo "Usage: $0 <fuse_main> <volume_file> <mount_point>"
    echo "fuse_main: the path to your fuse main excutable file"
    echo "volume_file: the file where your file system will use as volume"
    echo "mount_point: where to mount your file system"
    echo "This test script will mount your file system by command: \"<fuse_main> <volume_file> <mount_point>\""
    echo "    and unmount it by command: \"fusermount -u <mount_point>\""
else
    DIVIDING_LINE='******************************************************************************'
    FUSE_MAIN="$1"
    VOLUME_FILE="$2"
    MOUNT_POINT="$3"
    echo "fuse_main   : $FUSE_MAIN"
    echo "volume_file : $VOLUME_FILE"
    echo "mount_point : $MOUNT_POINT"

    TEST_STRING="fuse filesystem test script v0.0.0.0.1"
    test0
    if [ $? -eq 1 ]; then
        echo "test 0 failed"
        echo "check if you passed correct parameters, or forgot to umount your filesystem"
        fusermount -u "$MOUNT_POINT"
        exit 1
    else
        echo "test 0 passed"
    fi

    doTest
    TestRet=$?

    echo ""
    echo ""
    echo "Test Summary:"
    if [ "$TestRet" = "1" ]; then
        echo "basic function test failed."
        echo "see the log above to pinpoint error position"
    else
        PrintSummary
    fi

    fusermount -u "$MOUNT_POINT"
fi
