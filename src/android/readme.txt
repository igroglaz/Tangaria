Android
=======

Prepare the project
-------------------

SDL2 https://www.libsdl.org/download-2.0.php
Source Code: https://www.libsdl.org/release/SDL2-2.0.16.zip

SDL_ttf 2.0 https://www.libsdl.org/projects/SDL_ttf/
Source: https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-2.0.15.zip

SDL_image 2.0 https://www.libsdl.org/projects/SDL_image/
Source: https://www.libsdl.org/projects/SDL_image/release/SDL2_image-2.0.5.zip

SDL_mixer 2.0 https://www.libsdl.org/projects/SDL_mixer/
Source: https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-2.0.4.zip

Extract them, copy or symlink directory into:
  android/pwmangclient/jni/SDL2
  android/pwmangclient/jni/SDL2_image
  android/pwmangclient/jni/SDL2_ttf
  android/pwmangclient/jni/SDL2_mixer

Add 'lib' directory into archive src/android/pwmangclient/src/main/assets/lib.zip

Prepare gradle
--------------

Download and install Android Studio.
https://developer.android.com/studio

Run it, select 'Open existing project' and navigate to pwmangband src/android directory.

Android Studio will prompt you to download lots of stuff, android SDK, NDK, platform-tools, etc, and will ask to accept several EULAs. Do it.

Note: Android Studio will also ask if you want to add some of the newly generated files to git. Don't!

After all is done, both build and run buttons should work! That's it.

You can also build from command line, by running ./gradlew build (or gradlew.bat, on Windows), but you will need to prepare some environmental variables, like JAVA_HOME and ANDROID_HOME, to do so.
