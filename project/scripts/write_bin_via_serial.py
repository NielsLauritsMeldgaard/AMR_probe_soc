import argparse
import serial
from pathlib import Path
import time  # Added for introducing delay

# Defaults; can be overridden via CLI
DEFAULT_PORT = "COM3"
DEFAULT_BAUD = 115200
MAGIC = 0xDEADBEEF


def load_and_send(bin_path: Path, port: str, baud: int) -> None:
    data = bin_path.read_bytes()
    # Pad to 4-byte boundary
    if len(data) % 4:
        data += b"\x00" * (4 - (len(data) % 4))
    # Append magic word (little-endian)
    data += MAGIC.to_bytes(4, "little")

    total = len(data)
    chunk = 1024
    sent = 0

    def render_progress(sent_bytes: int) -> None:
        pct = (sent_bytes * 100) // total if total else 100
        bar_len = 30
        filled = (sent_bytes * bar_len) // total if total else bar_len
        bar = "#" * filled + "-" * (bar_len - filled)
        print(f"\r[{bar}] {pct:3d}% ({sent_bytes}/{total} bytes)", end="", flush=True)

    with serial.Serial(port, baud, timeout=1) as ser:
        render_progress(0)
        for i in range(0, total, chunk):
            part = data[i:i+chunk]
            for byte in part:
                ser.write(byte.to_bytes(1, 'little'))  # Write one byte at a time
                ser.flush()
                # time.sleep(0.001)  # Add a small delay of 10ms between each byte
            sent += len(part)
            render_progress(sent)
        print()  # newline after progress bar
        print(f"Sent {sent} bytes (including magic) from {bin_path}")


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Send a .bin file over UART with magic terminator")
    p.add_argument("bin_path", type=Path, help="Path to program .bin file")
    p.add_argument("--port", default=DEFAULT_PORT, help=f"Serial port (default: {DEFAULT_PORT})")
    p.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Baud rate (default: {DEFAULT_BAUD})")
    return p.parse_args()


def main() -> None:
    args = parse_args()
    load_and_send(args.bin_path, args.port, args.baud)

# python uart_loader.py path/to/program.bin --port COM3 --baud 115200
# Ex: python uart_loader.py programs/uart_hello_world.bin --port COM10 --baud 115200 
if __name__ == "__main__":
    main()
    
# OLD VERSION

# import argparse
# import serial
# from pathlib import Path
# import time  # Added for introducing delay

# # Defaults; can be overridden via CLI
# DEFAULT_PORT = "COM3"
# DEFAULT_BAUD = 115200
# MAGIC = 0xDEADBEEF


# def load_and_send(bin_path: Path, port: str, baud: int) -> None:
#     data = bin_path.read_bytes()
#     # Pad to 4-byte boundary
#     if len(data) % 4:
#         data += b"\x00" * (4 - (len(data) % 4))
#     # Append magic word (little-endian)
#     data += MAGIC.to_bytes(4, "little")

#     total = len(data)
#     chunk = 1024
#     sent = 0

#     def render_progress(sent_bytes: int) -> None:
#         pct = (sent_bytes * 100) // total if total else 100
#         bar_len = 30
#         filled = (sent_bytes * bar_len) // total if total else bar_len
#         bar = "#" * filled + "-" * (bar_len - filled)
#         print(f"\r[{bar}] {pct:3d}% ({sent_bytes}/{total} bytes)", end="", flush=True)

#     with serial.Serial(port, baud, timeout=1) as ser:
#         render_progress(0)
#         for i in range(0, total, chunk):
#             part = data[i:i+chunk]
#             ser.write(part)
#             ser.flush()
#             # time.sleep(1)  # Add a small delay of 10ms between writes
#             sent += len(part)
#             render_progress(sent)
#         print()  # newline after progress bar
#         print(f"Sent {sent} bytes (including magic) from {bin_path}")


# def parse_args() -> argparse.Namespace:
#     p = argparse.ArgumentParser(description="Send a .bin file over UART with magic terminator")
#     p.add_argument("bin_path", type=Path, help="Path to program .bin file")
#     p.add_argument("--port", default=DEFAULT_PORT, help=f"Serial port (default: {DEFAULT_PORT})")
#     p.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Baud rate (default: {DEFAULT_BAUD})")
#     return p.parse_args()


# def main() -> None:
#     args = parse_args()
#     load_and_send(args.bin_path, args.port, args.baud)

# # python uart_loader.py path/to/program.bin --port COM3 --baud 115200
# # Ex: python uart_loader.py programs/uart_hello_world.bin --port COM10 --baud 115200 
# if __name__ == "__main__":
#     main()