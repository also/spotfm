#!/bin/sh
rm -rf build

for b in {chrome,safari}
do
    mkdir -p build build/$b/ext
    cp ext/$b/* build/$b/ext
    cp ext/common/* build/$b/ext
    
    cp -r browse build/$b
    cp -r omnifm build/$b
    cp -r source build/$b
    
    cp common/* build/$b
    
    cp $b/* build/$b
    
    cp ../js/omnifm-core.js build/$b/omnifm
    cp ../js/spotfm-core.js build/$b/source/spotfm
    cp ../js/jquery-1.4.4.min.js build/$b
done

mv build/safari build/spotfm.safariextension
