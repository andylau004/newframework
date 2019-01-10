if test -d gen-cpp
then
    rm -rf gen-cpp/*
else
    mkdir -v gen-cpp
fi

for file in `ls thrift-file/`
do
    if [ -a thrift-file/$file ]
    then
        if echo thrift-file/$file | grep "thrift$"
        then
            tools/thrift --gen cpp:simple_code,moveable_types thrift-file/$file
        fi
    fi
done

rm -rf gen-cpp/*skeleton.cpp

