# @file build_win32_appConfig.py
# @brief Create the windows app.config file to redirect crt linkage.
#
# $LicenseInfo:firstyear=2009&license=viewerlgpl$
# Second Life Viewer Source Code
# Copyright (C) 2010, Linden Research, Inc.
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation;
# version 2.1 of the License only.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
# 
# Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
# $/LicenseInfo$

import sys, os, re
from xml.dom.minidom import parse

def munge_binding_redirect_version(src_manifest_name, src_config_name, dst_config_name):
    manifest_dom = parse(src_manifest_name)
    node = manifest_dom.getElementsByTagName('assemblyIdentity')[0]
    manifest_assm_ver = node.getAttribute('version')
    
    config_dom = parse(src_config_name)
    node = config_dom.getElementsByTagName('bindingRedirect')[0]
    node.setAttribute('newVersion', manifest_assm_ver)
    src_old_ver = re.match('([^-]*-).*', node.getAttribute('oldVersion')).group(1)
    node.setAttribute('oldVersion', src_old_ver + manifest_assm_ver)
    comment = config_dom.createComment("This file is automatically generated by the build. see indra/newview/build_win32_appConfig.py")
    config_dom.insertBefore(comment, config_dom.childNodes[0])

    print("Writing: " + dst_config_name)
    f = open(dst_config_name, 'w')
    config_dom.writexml(f)
    f.close()
    
    

def main():
    config = sys.argv[1]
    src_dir = sys.argv[2]
    dst_dir = sys.argv[3]
    dst_name = sys.argv[4]
    
    if config.lower() == 'debug':
        src_manifest_name = dst_dir + '/Microsoft.VC80.DebugCRT.manifest'
        src_config_name = src_dir + '/KokuaDebug.exe.config'
    else:
        src_manifest_name = dst_dir + '/Microsoft.VC80.CRT.manifest'
        src_config_name = src_dir + '/Kokua.exe.config'

    dst_config_name = dst_dir + '/' + dst_name
        
    munge_binding_redirect_version(src_manifest_name, src_config_name, dst_config_name)
    
    return 0

if __name__ == "__main__":
    main()
