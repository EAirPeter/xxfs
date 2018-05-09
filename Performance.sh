function testWrite ()
{
    local temp_spd
    local runs
    local num
    local res
    local unit

    echo "performace test: write"
    runs=0
    res=0
    while [ $runs -lt 5 ]; do
        temp_spd=$(dd if=/dev/zero of=$MOUNT_POINT/testfile bs=64M count=2 oflag=sync 2>&1)
        temp_spd=$(echo ${temp_spd##*,})
        num=$(echo $temp_spd | awk '{print $1}')
        unit=$(echo $temp_spd | awk '{print $2}')
        if [ "$unit" = "GB/s" ]; then num=$(echo $num|awk '{print $1*1024}'); fi
        if [ "$unit" = "KB/s" ]; then num=$(echo $num|awk '{print $1/1024}'); fi
        if [ "$unit" = "kB/s" ]; then num=$(echo $num|awk '{print $1/1024}'); fi
        res=$(echo $res $num|awk '{print $1+$2}')
        let runs=runs+1
        echo "Pass" $runs ":" $num "MB/s"
    done
    res=$(echo $res|awk '{print $1/5}')
    echo "average write speed: $res MB/s"
    echo "performace test: read"
    runs=0
    res=0
    while [ $runs -lt 5 ]; do
        temp_spd=$(dd if=$MOUNT_POINT/testfile of=/dev/null bs=64M count=2 iflag=nocache oflag=sync 2>&1)
        temp_spd=$(echo ${temp_spd##*,})
        num=$(echo $temp_spd | awk '{print $1}')
        unit=$(echo $temp_spd | awk '{print $2}')
        if [ "$unit" = "GB/s" ]; then num=$(echo $num|awk '{print $1*1024}'); fi
        if [ "$unit" = "KB/s" ]; then num=$(echo $num|awk '{print $1/1024}'); fi
        res=$(echo $res $num|awk '{print $1+$2}')
        let runs=runs+1
        echo "Pass" $runs ":" $num "MB/s"
    done
    res=$(echo $res|awk '{print $1/5}')
    echo "average read speed: $res MB/s"
    rm $MOUNT_POINT/testfile
    return 0
}

if  [ $# -lt 1 ]; then
    echo "Usage: $0 <test_dir>"
else
    MOUNT_POINT="$1"
    echo "test_dir : $MOUNT_POINT"
    testWrite
fi