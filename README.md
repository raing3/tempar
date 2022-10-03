## TempAR

##### Overview

TempAR is a cheat plugin for the PlayStation Portable.

It is a heavily modified version of MKUltra which itself is a modified version of NitePR.

For more information please refer to the docs folder.

Special thanks in particular to:

 - RedHate: Developer of MKUltra
 - SaNiK: Developer of NitePR
 

##### Compiling

To compile, ensure you have Docker installed and run the following in PowerShell:

```
docker run -it -v "${PWD}:/src" -w "/src" pspdev/pspsdk make release
```

A tar.gz file containing the binaries and other misc files will be output to a `build` folder.

Build process tested on Windows 10.