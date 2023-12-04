#! /bin/bash
local=$1
docker run -it -v "${local}"/data/stronglogic/dcv-channel/docker:/dcv dcvbuilder bash
