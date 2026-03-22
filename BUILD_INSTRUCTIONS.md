# ** Dayturn macOS Build Instructions **

This [repository](https://github.com/geirnoklebye/dayturn.git) is dedicated to build a macOS version of the Dayturn viewer for OpenSim grids and standalone installations.
 
There is one active branch; dayturn-norlv.  


**Development system:**

Linden Lab's development system for their viewer is referenced in: http://wiki.secondlife.com/wiki/Compiling_the_viewer_(Mac_OS_X_XCode_6.1)

*NOTE* Their development system over time is diverging from our because they are geared to do their builds on a build farm that individual builders usually don't have access to. Because of this the repository will not build with their autobuild version 1.1 or higher.

At times their instructions are horrendously outdated or incorrect, so try to follow the instructions below instead.


**You will need these items before you begin:**

The current development environment is
- XCode 13.2.1 running on OS X 11.6.2 where the viewer is built 64-bit with OS X SDK 12.1 and Deployment Target of OS X 10.13.

You can both build from the command line in terminal or build the XCode project that is generated duing configuration.

NOTE: On Upgrading XCode to a new version you probably should delete the Derived Data folder that is found in Development/XCode as it may contain links to old system library locations preventing your build from linking.
	
--- 

Linden’s version of autobuild requires a different version of Python than the system installed so the best way to get it installed is to first install MacPorts from https://www.macports.org with the latest current release (2.6.2).

With MacPorts installed, in terminal install the following ports:

• sudo port install python27

• sudo port install py27-pip

• sudo port install cmake

When prompted by the installer run python_select to use the version you just installed. It will be installed in /opt/local/bin

If you have a newer versions of Xcode installed then you also need to run xcode-select to make sure you use 10.3 for your build

---

Pip Install auto build python dependencies by typing:

sudo pip install ‘hg+http://dayturn.com:5005/autobuild-1.0#egg=autobuild'

If everything goes well it should be installed in /opt/local/Library/Frameworks/Python.framework/Versions/2.7/bin/autobuild

To make life easier edit your .bash_profile and add the lines

alias autobuild="/opt/local/Library/Frameworks/Python.framework/Versions/2.7/bin/autobuild"

export AUTOBUILD=/opt/local/Library/Frameworks/Python.framework/Versions/2.7/bin/autobuild

Then source your .bash_profile

---

To build Dayturn first download the Dayturn source code with the following command in terminal:

git clone https://github.com/geirnoklebye/dayturn.git

Change directory to the location where you cloned the repository, i.e Dayturn-viewer 

Check your git branches with git branch -a

Change to the main branch with git checkout dayturn-norlv

You can configure the build with:

autobuild configure -c RelWithDebInfoOS -- -DCMAKE_VERBOSE_MAKEFILE:BOOL=TRUE -DUSE_KDU:BOOL=FALSE  -DFMODSTUDIO:BOOL=TRUE -DLL_TESTS:BOOL=FALSE -DOPENAL:BOOL=FALSE -DPACKAGE=ON -DVIEWER_CHANNEL="Release"

or 

autobuild configure -c ReleaseOS -- -DCMAKE_VERBOSE_MAKEFILE:BOOL=TRUE -DUSE_KDU:BOOL=FALSE  -DFMODSTUDIO:BOOL=TRUE -DLL_TESTS:BOOL=FALSE -DOPENAL:BOOL=FALSE -DPACKAGE=ON -DVIEWER_CHANNEL="Release"

Build with the command

autobuild build -c ReleaseOS or autobuild build -c RelWithDebInfoOS respectively


**NOTE:** It is recommended you use RelWithDebInfoOS because that will generate proper crash reports in the event the application should crash. It provides significantly better information if the user submits the crash report. 

When you have made sure your configuration is working (and compiles in Xcode) you can also compile on the command line by substituting configure with build in the two commands above. 

BUILD NOTE: When building in Xcode at some point the build will fail because it cannot find packages-info.txt. At this point just restart the build and it will continue from there.

The root cause of this is that it tries to run autobuild by spawning a shell from inside autobuild, but Xcode will not allow any other version than the system python to be called so autobuild will fail - it does not even find it.  For anything but a (final) release build this is not significant. *This build has to be done from the command line.*

For building with Xcode you also need to set the optimization level manually in the Xcode project generated as make currently is not able to set this correct in the project. You should optimize at O3 level for both RelWithDebInfoOS and ReleaseOS builds. 

You may also have to configure the build type in the Xcode project, which can be done from the Project -> Scheme -> Edit Scheme... menu. It is set in the Scheme's Run section. Usually you only have to do this once, when you generate a new project.

You will find your Dayturn.xcodeproj in the subdirectory build-darwin-64.

The resulting DMG installer image will be found in build-darwin-64/newview. 


***Git client Sourcetree (optional)***

- Download and install [https://www.sourcetreeapp.com] (64-bit)


**How to turn off signing for development builds in Xcode**
https://stackoverflow.com/a/54296008


**Disclaimer**

This software is not provided nor supported by Linden Lab, the makers of Second Life.







