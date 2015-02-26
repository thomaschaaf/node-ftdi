#!/bin/bash

FTDI_LINUX_LINK='http://www.ftdichip.com/Drivers/D2XX/Linux/libftd2xx1.1.12.tar.gz'
FTDI_MAC_LINK='http://www.ftdichip.com/Drivers/D2XX/MacOSX/D2XX1.2.2.dmg'
LIB_VERSION_LINUX='1.1.12'
LIB_VERSION_MAC='1.2.2'

echo Detected Platform:

ostype='unknown'
platform='unknown'
link=''
getcommand=''
unamestr=`uname -s`

if [ "$unamestr" == 'Linux' ]
then
   LIB_VERSION=$LIB_VERSION_LINUX
   ostype='linux'
   link=$FTDI_LINUX_LINK
   getcommand='wget'
elif [ "$unamestr" == 'Darwin' ]
then
   LIB_VERSION=$LIB_VERSION_MAC
   ostype='mac'
   link=$FTDI_MAC_LINK
   filename=`basename $link`
   getcommand="curl -o $filename"
fi

if [ $(uname -m) == 'x86_64' ]
then
   platform='64bit'
else
   platform='32bit'
fi



echo $ostype \($platform\)
echo Download FTDI Library
$getcommand $link
filename=`basename $link`

if [ "$ostype" ==  "linux" ]
then
   mkdir -p tmp/
   tar xfvz $filename -C tmp/
   if [ "$platform" ==  "64bit" ]
   then
   	buildPath="tmp/release/build/x86_64/"
   else
      buildPath="tmp/release/build/i386/"
   fi
   libPath=$buildPath
   libPath+=libftd*
   cp $libPath /usr/local/lib
   mkdir /usr/local/include/libftd2xx
   cp tmp/release/ftd2xx.h /usr/local/include/libftd2xx
   cp tmp/release/WinTypes.h /usr/local/include/libftd2xx
   chmod 0755 /usr/local/lib/libftd2xx.so.$LIB_VERSION
   ln -sf /usr/local/lib/libftd2xx.so.$LIB_VERSION /usr/local/lib/libftd2xx.so
   rm -f $filename
   rm -rf tmp
   ldconfig
elif [ "$ostype" ==  "mac" ]
then
   hdiutil attach $filename
   cp /Volumes/release/D2XX/bin/10.5-10.7/libftd2xx.$LIB_VERSION.dylib /usr/local/lib/libftd2xx.$LIB_VERSION.dylib
   ln -sf /usr/local/lib/libftd2xx.$LIB_VERSION.dylib /usr/local/lib/libftd2xx.dylib
   cp /Volumes/release/D2XX/Samples/ftd2xx.h /usr/local/include/ftd2xx.h
   cp /Volumes/release/D2XX/Samples/WinTypes.h /usr/local/include/WinTypes.h
   rm $filename
   hdiutil detach /Volumes/release
fi
