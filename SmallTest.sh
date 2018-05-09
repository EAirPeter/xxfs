for ((i = 0; i < 1024; ++i)); do
    dd if=/dev/urandom of=$i bs=3K count=1
done
for ((i = 0; i < 1024; ++i)); do
    rm -f $i
done
