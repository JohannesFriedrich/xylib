#!/bin/bash
# convert all of the sample files in ./sample to ASCII plain format in ./output
# using ./xyconv

for i in samples/*/[^R]*
do
	echo "processing file $i ..."
	outpath=$(echo $i | sed -e 's@^samples/@output/@')"_tr.txt"
	mkdir -p $(dirname $outpath)
	./xyconv $i $outpath || echo Failed: ./xyconv $i $outpath
done

