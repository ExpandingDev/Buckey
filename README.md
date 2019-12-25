# NOTICE
Buckey is currently being rebuilt with a different architecture. This first attempt at Buckey was successful but revealed flaws in its design. Buckey's main features are being broken down into smalled, independently running processes that intercommunicate through DBus. This will increase robustness, configurability, and hackability.

# Buckey
Buckey is an opensource, privacy oriented, configurable and hackable personal assistant.  
Because of it's privacy driven nature, it lacks some features that other commercial personal assistants boast (For example, good speech recognition)  


# Installation
Buckey relies on `SDL2`, `SDL_mixer`, `yaml-cpp`, `cppfs`,  `mimic1`, `pocketsphinx`, and `JSGF_Kit++`.

`cppfs` can be found at: https://github.com/cginternals/cppfs

`JSGF_Kit++` can be found at: https://github.com/ExpandingDev/JSGFKit_Plus_Plus.git

Buckey uses the GNU autotools build system, so to compile and install on you system:  
    
    ./configure
    make -j 4
    make install




