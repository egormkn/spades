#!/bin/bash
set -e
pushd ../../../
make clean
./cpcfg
./spades.py src/test/teamcity/spades_config_mc_is220_1k.info
popd