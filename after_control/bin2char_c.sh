#!/bin/bash
for f in *.bin;
do echo "Processing $f file..";
xxd -i $f > $f.c
done
