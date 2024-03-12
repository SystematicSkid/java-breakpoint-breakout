# Java Breakpoint Breakout
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## Overview
This project is used to demonstrate how to apply breakpoints to java interpreter-executed methods without the use of JNI (almost).

You can find the full writeup on my blog [here](https://systemfailu.re/2024/01/22/java-breakpoint-breakout/).

## Features
- Dynamic detection of required offsets
- Very easy-to-use method breakpoint interface
- Simple, clean, and easy-to-understand code

## Requirements
- A Java application target
- A non-JITd method
  - If your method IS JITd, check out my other project [here](https://github.com/SystematicSkid/java-jit-hooks)!
- A basic understanding of the method's bytecode
  - Or not if you just want to know when the opcode is executed!
- CMake
- A C++ compiler

## Dependencies
This project uses hde64 for disassembly, which is developed by Vyacheslav Patkov.

## Building
To build the project, please use the provided CMakeLists.txt file.
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -T host=x64 -A x64
cmake --build . --config Release
```

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Disclaimer
This project is for educational purposes only. 