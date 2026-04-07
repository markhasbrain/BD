# BD - Binary Decoder

The language that kills HTML, CSS, and JS.

One `.bd` file = one complete website. Structure, style, and logic in a single file using 8-bit binary opcodes. Built from scratch in C++ with zero dependencies.

**The first website ever built entirely in binary.**

---

## How it works

```
# set page title
11000001 "Hello World"

# start document
00000001

# create a heading, style it
00000110 1 "Hello from BD"
01000010 "#fff"
01000011 "48px"
00000100

# add a paragraph
00000101 "Built with binary."

# end document
00000010
```

8 bits per instruction. The first two bits determine the category:

| Prefix | Category | Replaces |
|--------|----------|----------|
| `00` | Structure | HTML |
| `01` | Style | CSS |
| `10` | Logic | JavaScript |
| `11` | Meta | Config |

## Build

Requires g++ with C++20 support.

```sh
g++ -std=c++20 -O2 src/main.cpp -lws2_32 -o bd.exe   # Windows
g++ -std=c++20 -O2 src/main.cpp -o bd                  # Linux/macOS
```

## Usage

```sh
# Compile a .bd file to HTML
bd compile landing.bd

# Start dev server with live reload (default port 5000)
bd serve landing.bd

# Show all opcodes
bd info
```

## Why

HTML was never designed for AI to write. So AI generates bloated, broken, sloppy markup. BD is a binary language built from the ground up for machines to code in -- and it produces clean sites every time.

- **One file** -- structure + style + logic, no more juggling three languages
- **Binary precision** -- 8-bit opcodes, no wasted syntax, no ambiguity
- **AI-native** -- consistent patterns so machines get it right first try
- **Full control** -- no framework opinions, you own every pixel
- **Fast** -- C++ compiler, zero dependencies, compiles in 0ms

## License

MIT
