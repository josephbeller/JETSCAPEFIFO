#!/usr/bin/env bash

###############################################################################
# Copyright (c) The JETSCAPE Collaboration, 2018
#
# For the list of contributors see AUTHORS.
#
# Report issues at https://github.com/JETSCAPE/JETSCAPE/issues
#
# or via email to bugs.jetscape@gmail.com
#
# Distributed under the GNU General Public License 3.0 (GPLv3 or later).
# See COPYING for details.
##############################################################################

# make sure all auguments are passed
echo Installing Rivet
export curdir=$PWD
export rivetdir=$PWD/../../RIVET
echo $rivetdir
mkdir -p $rivetdir
achitecture=$1
# blank if not specified
if [ -z "$achitecture" ]
then
      echo "Architecture not specified: Defaulting to Windows/Linux"
      cp rivet-bootstrap $rivetdir/.
fi

    # mac 
if [ "$achitecture" == "mac" ]
then
    echo "Architecture specified: $achitecture"
    cp rivet-bootstrap-mac $rivetdir/rivet-bootstrap
fi

cd $rivetdir
chmod +x rivet-bootstrap
./rivet-bootstrap 

