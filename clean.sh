#!/bin/bash

make -C client/ clean
make -C server/ clean
rm -rf tags .bin
