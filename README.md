# CSE321_Operating_System

# Custom UNIX Shell (`gsh`)

Welcome to a UNIX-like terminal built in C. This shell supports basic command execution, piping, redirection, command history, signal handling, and more. It replicates core functionalities of traditional shells like `bash` or `sh` in a simplified, educational implementation.

---

## Features
- Executes Linux system commands (`ls`, `pwd`, `mkdir`, etc.)
- Input and output redirection (`<`, `>`, `>>`)
- Command piping (`|`) for chaining commands
- Sequential (`;`) and conditional (`&&`) command execution
- Command history saved to `history.txt`
- Signal handling (e.g., `Ctrl+C` interrupts only running commands, not the shell)
- Clean and colored UI with exit message

---

## How to Compile & Run

```bash
gcc shell.c -o shell
./shell
