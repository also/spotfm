#!/bin/sh

rm -rf build

mkdir -p build build/chrome build/spotfm.safariextension

cp chrome/* build/chrome
cp spotfm.safariextension/* build/spotfm.safariextension

cp common/* build/chrome
cp common/* build/spotfm.safariextension

cp ../js/* build/chrome
cp ../js/* build/spotfm.safariextension
