# highlight

`highlight` is a small C utility that reads text from **stdin**, searches for a POSIX Extended Regular Expression, and highlights all matching text using ANSI terminal colours.

It behaves similarly to:

```sh
grep --color=always
```

except it always outputs the complete input stream with matching text highlighted.

---

## Features

- Reads from standard input
- POSIX Extended Regular Expressions (ERE)
- Highlights every match
- Multiple matches per line
- Lightweight
- No external dependencies
- Portable across Linux and Unix-like systems

---

## Building

Requirements:

- GCC or Clang
- POSIX regex library (included with glibc and most Unix systems)
- make

Compile with:

```sh
make
```

This creates the executable:

```
highlight
```

The default build enables fortified libc checks, stack protection, format-string
warnings, and position-independent executables. On Linux it also enables
stack-clash and x86 control-flow protection, plus full RELRO and a non-executable
stack. Set `HARDENING_CFLAGS` or `HARDENING_LDFLAGS` on the `make` command line
to customize or disable these options for a specific toolchain.

---

## Installation

System-wide:

```sh
sudo make install
```

This installs the binary to:

```
/usr/local/bin/highlight
```

Remove it with:

```sh
sudo make uninstall
```

---

## Usage

```sh
highlight <regex>
```

Example:

```sh
cat logfile.txt | highlight "error|warning"
```

Search for IPv4 addresses:

```sh
journalctl | highlight '[0-9]{1,3}(\.[0-9]{1,3}){3}'
```

Highlight numbers:

```sh
echo "abc123def456" | highlight '[0-9]+'
```

Case-sensitive search:

```sh
echo "Error error ERROR" | highlight 'error'
```

---

## Regular Expressions

The program uses POSIX Extended Regular Expressions.

Examples:

| Regex | Description |
|--------|-------------|
| `error` | Match the word "error" |
| `error|warning` | Match either word |
| `[0-9]+` | One or more digits |
| `[A-Za-z_][A-Za-z0-9_]*` | C identifier |
| `https?://[^ ]+` | Simple URL matcher |

---

## Exit Status

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Invalid arguments or regex compilation failure |

---

## Examples

Highlight HTTP status codes:

```sh
tail -f access.log | highlight ' (200|301|302|404|500) '
```

Highlight IP addresses:

```sh
tcpdump -n | highlight '[0-9]{1,3}(\.[0-9]{1,3}){3}'
```

Highlight dates:

```sh
cat file.txt | highlight '[0-9]{4}-[0-9]{2}-[0-9]{2}'
```

---

## Cleaning

Remove build artifacts:

```sh
make clean
```

---

## Limitations

- Operates line by line.
- Uses ANSI escape sequences for colouring.
- Uses POSIX regex rather than PCRE.
- Designed for terminal output.

---

## License

Released under the MIT License.
