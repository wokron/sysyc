# Contributing

## Contribute Code
Contribute code by checking out a new branch, formatted as `<username>--<description>`. For example, `wokron--something-to-do`.

## Coding Style

- File names should be in `snake_case`, with the suffix `.h` or `.cpp`.
- Use `#pragma once` in header files instead of `#ifndef`.
- Variables should be in `snake_case`.
- Class names should be in `PascalCase`.
- Functions and class methods should be in `snake_case`.
- Private methods and attributes should be prefixed with an underscore, such as `_private_method` and `_private_attr`.
- Other unspecified standards are determined by the `.clang-format` and `.clang-tidy` in the project. You can use `scripts/format.sh` and `scripts/tidy.sh` to format your code.
