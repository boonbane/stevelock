Extremely lightweight process sandboxing for macOS and Linux.

# Overview
Most sandboxing approaches involve virtualization or containerization. Sometimes this is for reproducability (e.g. shipping a Docker image with exact versions of tools preinstalled). Sometimes this is for security.

Usually, this is the more rigorous and correct approach. However, I had a use case in which it was important to legitimately run on the user's normal system (both for the user's ergonomics and security). Functionally, all I wanted was to restrict a process to a whitelist of files, network access, etc.

Thus, `stevelock`. macOS and Linux both provide native APIs for doing exactly this. On macOS, it's the long deprecated Seatbelt. On Linux, it's Landlock. `stevelock` simply wraps them and provides good ergonomics. They are simple APIs to start with, which makes `stevelock` itself extremely simple.

# Usage
```
bun install stevelock
```
