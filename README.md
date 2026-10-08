# nyush

## Overview

`nyush` is a simple interactive shell written in C. It implements command parsing, process creation, pipelines, file redirection, and suspended job management. On startup, it displays a `[nyush current-directory-name]$` prompt and reads and executes commands.

Its main features include:

- Running external programs.
- Connecting multiple programs with `|`.
- Reading input from a file with `<`, overwriting an output file with `>`, and appending output with `>>`.
- Built-in commands: `cd`, `jobs`, `fg`, and `exit`.
- Terminating a foreground program with `Ctrl+C` or suspending it with `Ctrl+Z`.

## Building

Requires a Linux/POSIX environment, GCC, and GNU Make. Windows users can run these commands in WSL or a Linux container. Starting from the parent `labs` directory:

```bash
cd nyush
make
```

This creates the `nyush` executable in the current directory. The makefile uses C99 and enables debugging information and strict compiler warnings.

To remove build artifacts:

```bash
make clean
```

## Usage

Start the shell:

```bash
./nyush
```

Then enter commands at the `nyush` prompt, for example:

```text
pwd
ls -l
cd ..
ls -l | wc -l
cat < input.txt
ls > output.txt
ls >> output.txt
cat < input.txt | wc -l > count.txt
```

For command names without `/`, the shell looks for executables in `/usr/bin`. You can also provide an absolute path or a relative path containing `/`, such as `/bin/ls` or `./program`. The shell does not search the `PATH` environment variable.

Built-in commands:

| Command | Description |
| --- | --- |
| `cd directory` | Change the working directory. Exactly one directory argument is required. |
| `jobs` | List suspended jobs and their job numbers. |
| `fg number` | Resume the suspended job with the given number in the foreground. |
| `exit` | Exit the shell. Exiting is refused while suspended jobs remain. |

For example, run `sleep 30` and press `Ctrl+Z`. Enter `jobs` to view its number, then use `fg 1` to resume the first suspended job. You can terminate the resumed foreground program with `Ctrl+C`, then enter `exit`.

This is a simplified shell for a course assignment. Enter one command or pipeline per line. Quoting, wildcard expansion, variable expansion, and background execution with `&` are not supported. Avoid spaces in filenames and arguments. Place redirections after arguments; for pipelines, put input redirection on the first command and output redirection on the last command.
