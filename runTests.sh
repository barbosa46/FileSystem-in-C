#!/bin/bash

if [ $# -ne 4 ]
then
    echo "Usage: $0 inputdir outputdir maxthreads numbuckets"
    exit 1
fi

inputdir="${1}"
outputdir="${2}"
maxthreads="${3}"
numbuckets="${4}"

if ! [ -d "${inputdir}" ]
then
    echo "Error:" "${inputdir}" "does not exist or is not a valid directory!"
    exit 1
fi
if ! [ -d "${outputdir}" ]
then
    mkdir ${outputdir}
fi

for input in $(ls ${inputdir})
do
    echo "InputFile=""${input}" "NumThreads=​1"
    ./tecnicofs-nosync ${inputdir}/${input} ${outputdir}/${input%.*}-1.txt 1 1 | \
    grep "TecnicoFS completed in"
    echo ""

    for threads in $(seq 2 ${maxthreads})
    do
        echo "InputFile=""${input}" "NumThreads=""​${threads}"
        ./tecnicofs-mutex ${inputdir}/${input} ${outputdir}/${input%.*}-${threads}.txt \
        ${threads} ${numbuckets} | grep "TecnicoFS completed in"
        echo ""
    done
done
