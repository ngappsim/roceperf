#!/bin/bash

make -C client/ clean
make -C server/ clean
make -C pingpong/ clean
rm -rf tags .bin
