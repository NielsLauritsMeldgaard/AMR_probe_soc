"""Convert RISC-V .bin test programs into .mem files.

This script reads raw binary test programs (little-endian bytes) and emits
`.mem` files with one 32-bit word per line, written as big-endian hex text so
the least significant bits appear on the right of each line (standard
left-to-right hex ordering).
"""

from __future__ import annotations

import argparse
from pathlib import Path


def bin_to_mem(src: Path, dst: Path, word_bytes: int = 4) -> None:
	data = src.read_bytes()

	words = []
	for i in range(0, len(data), word_bytes):
		chunk = data[i : i + word_bytes]
		if len(chunk) < word_bytes:
			# Pad partial final word
			chunk += b"\x00" * (word_bytes - len(chunk))
		
		# Little-endian bytes to integer word
		word = int.from_bytes(chunk, byteorder="little", signed=False)
		words.append(f"{word:0{word_bytes*2}x}")

	dst.write_text("\n".join(words) + "\n")


def convert_all(root: Path, word_bytes: int) -> None:
	for bin_path in root.rglob("*.bin"):
		dst = bin_path.with_suffix(".mem")
		bin_to_mem(bin_path, dst, word_bytes)
		print(f"Wrote {dst.relative_to(root.parent)}")


def main() -> None:
	parser = argparse.ArgumentParser(description="Convert .bin to .mem (hex words)")
	parser.add_argument(
		"input",
		nargs="?",
		type=Path,
		help="Single .bin file or directory to convert. If omitted, all .bin files under the script directory are processed.",
	)
	parser.add_argument(
		"--word-bytes",
		type=int,
		default=4,
		help="Word size in bytes (default: 4 for 32-bit instructions)",
	)
	parser.add_argument(
		"--output",
		type=Path,
		help="Output .mem file (only when converting a single input file)",
	)

	args = parser.parse_args()

	base = Path(__file__).resolve().parent

	if args.input:
		src = args.input if args.input.is_absolute() else base / args.input
		
		# Check if input is a directory
		if src.is_dir():
			print(f"Converting all .bin files in {src.relative_to(base)}/")
			for bin_path in sorted(src.glob("*.bin")):
				dst = bin_path.with_suffix(".mem")
				bin_to_mem(bin_path, dst, args.word_bytes)
				print(f"  Wrote {dst.name}")
		else:
			# Single file
			dst = args.output if args.output else src.with_suffix(".mem")
			bin_to_mem(src, dst, args.word_bytes)
			print(f"Wrote {dst}")
	else:
		convert_all(base, args.word_bytes)


if __name__ == "__main__":
    # # Convert all (everything in tests/)
    # python bin_to_mem.py

    # # Convert specific task folder only
    # python bin_to_mem.py task1
    # python bin_to_mem.py task2
    # python bin_to_mem.py task3
    # python bin_to_mem.py task4

    # # Convert single file
    # python bin_to_mem.py task1/addpos.bin

    # # With custom output
    # python bin_to_mem.py task1/addpos.bin --output task1/addpos.mem
      
	main()
