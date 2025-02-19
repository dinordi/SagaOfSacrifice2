
# Sage Of Sacrifice Wrapper

The Saga Of Sacrifice wrapper is used mainly in development to test and run the game locally on a laptop using the SDL framework!

## Setup Windows

```shell
git clone --recurse-submodules https://github.com/dinordi/SagaOfSacrifice2.git
```

If you already cloned it you can get the submodules via:
```shell
git submodule update --init --recursive
```
NOTE: This can take a little bit

To build the wrapper application:

```shell
mkdir wrapper/build
cd wrapper/build
```
Make sure you have CMake installed VERSION < 3.5
https://cmake.org/download/
Or use choco in a administrator powershell:
```shell
choco install cmake
```

```
```shell
cmake -S .. -B ..\build\win
```

Then start the sln with Visual Studio 2022 and run the app!



