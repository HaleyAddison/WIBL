##\file dcdb_upload.py
# \brief Send a GeoJSON file into DCDB for archive
#
# This is a simple command-line utility to send a given GeoJSON file directly
# to DCDB for archival (assuming that all of the appropriate security tokens
# are available).  This duplicates the functionality of the cloud-based processing
# but allows CLI interface (for testing, etc.)
#
# Copyright 2023 Center for Coastal and Ocean Mapping & NOAA-UNH Joint
# Hydrographic Center, University of New Hampshire.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights to use,
# copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
# OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
# OR OTHER DEALINGS IN THE SOFTWARE.

import argparse as arg
import sys
import json
import os

from wibl.cmd import get_subcommand_prog
import wibl.core.config as conf
from wibl.core import getenv
from wibl.submission.cloud.aws import get_config_file
from wibl.submission.cloud.aws.lambda_function import transmit_geojson

def dcdb_upload():
    parser = arg.ArgumentParser(description="Upload GeoJSON files to DCDB for archival.",
                prog=get_subcommand_prog())
    parser.add_argument('-s', '--sourceid', type=str, help = 'Set SourceID as in the GeoJSON metadata.')
    parser.add_argument('-p', '--provider', type=str, help='Provider ID as generated by DCDB')
    parser.add_argument('-a', '--auth', type=str, help='File containing the Provider authorisation token generated by DCDB')
    parser.add_argument('-c', '--config', type=str, help='Specify configuration file for installation')
    parser.add_argument('input', help='GeoJSON file to upload to DCDB')

    optargs = parser.parse_args(sys.argv[2:])

    filename = optargs.input
    
    sourceID = None
    if hasattr(optargs, 'sourceid'):
        sourceID = optargs.sourceid
    else:
        with open(filename) as f:
            data = json.load(f)
        # We need to make sure that we get the source ID from the right location in the
        # metadata --- it varies between V2 and V3 metadata (and possibly from others)
        if 'trustedNode' in data['properties']:
            # This was added in V3 of the metadata specification
            if data['properties']['trustedNode']['convention'] == 'GeoJSON CSB 3.0':
                sourceID = data['properties']['trustedNode']['uniqueVesselID']
            else:
                # If we don't get a convention that we recognise, we can at least check to see
                # whether the vessel ID is still in the same place
                if 'uniqueVesselID' in data['properties']['trustedNode']:
                    sourceID = data['properties']['trustedNode']['uniqueVesselID']
                else:
                    sys.exit('Error: failed to find a unique vessel ID in metadata.')
        else:
            if 'convention' in data['properties']:
                # This was the case in V2 metadata, but we should check that it's a version
                # that we understand.  This is more a nicety than anything else: there was
                # only one version, so this should always pass!
                if data['properties']['convention'] == 'CSB 2.0':    
                    sourceID = data['properties']['platform']['uniqueID']
                else:
                    sys.exit('Error: unrecognised CSB metadata convention.')
    try:
        if hasattr(optargs, 'config'):
            config_filename = optargs.config
        else:
            config_filename = get_config_file()
        config = conf.read_config(config_filename)
    except conf.BadConfiguration:
        sys.exit('Error: bad configuration file.')
    
    provider_id = None
    if 'provider_id' in config:
        provider_id = config['provider_id']
    else:
        if hasattr(optargs, 'provider'):
            provider_id = optargs.provider
    provider_auth = None
    if hasattr(optargs, 'auth'):
        auth_filename = optargs.auth
        with open(auth_filename) as f:
            provider_auth = f.readline()

    if provider_id is None or provider_auth is None:
        sys.exit('Error: you must specify a provider ID and authorisation token.')

    if 'upload_point' not in config:
        sys.exit('Error: your configuration must include a DCDB upload point URL.')
    else:
        # We can't modify the cloud-based code that uses enivornment variables to
        # determine where to send the data, so we need to add this information into
        # the environment directly.
        os.environ['UPLOAD_POINT'] = config['upload_point']

    rc = transmit_geojson(sourceID, provider_id, provider_auth, filename, config)
    if rc is None:
        sys.exit(f'Error: failed to transfer file {filename}')
    else:
        print(f'Success: transmitted file {filename} with return {rc}.')
