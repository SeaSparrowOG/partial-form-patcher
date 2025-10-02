## Building
### Requirements:
* CMake
* VCPKG
  * Add the root to an environment variable called `VCPKG_ROOT`.
* Visual Studio (with desktop C++ development)
---
### Instructions:
```
git clone https://github.com/SeaSparrowOG/SKSE-Plugin-Template
cd SKSE-Plugin-Template
git submodule update --init --recursive
cmake --preset vs2022-windows-vcpkg-release
cmake --build --preset Release
```
---
### Automatic deployment to MO2:
You can automatically deploy to MO2's mods folder by defining an Environment Variable named SKYRIM_MODS_FOLDER and pointing it to your MO2 mods folder. It will create a new mod with the appropriate name. After that, simply refresh MO2 and enable the mod.
