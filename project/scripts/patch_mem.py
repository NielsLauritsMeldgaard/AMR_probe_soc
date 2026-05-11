
import sys
import os

def patch_mem_file(input_file, output_file, target_address_hex):
    try:
        # 1. Parse the new base address
        # We need the address to be 4KB aligned (lower 12 bits must be 0)
        # because the code uses 'LUI' (Load Upper Immediate) to set the base.
        target_addr = int(target_address_hex, 16)
        
        if (target_addr & 0xFFF) != 0:
            print(f"Error: Target address {target_address_hex} must be 4KB aligned (end in 000).")
            print("The assembly uses 'LUI' to set the base pointer, which only sets the upper 20 bits.")
            return

        # 2. Re-encode the first instruction: LUI s0, upper_20_bits
        # Instruction format: U-Type
        # [31:12] imm[31:12] | [11:7] rd | [6:0] opcode
        # s0 is register x8. Opcode for LUI is 0110111 (0x37).
        
        upper_20_bits = (target_addr >> 12) & 0xFFFFF
        rd = 8  # s0/x8
        opcode = 0x37
        
        # Assemble the 32-bit instruction
        new_instruction_val = (upper_20_bits << 12) | (rd << 7) | opcode
        new_instruction_hex = f"{new_instruction_val:08x}"

        # 3. Read the original .mem file
        with open(input_file, 'r') as f:
            lines = f.readlines()

        if not lines:
            print("Error: Input file is empty.")
            return

        # 4. Patch the file
        old_instruction = lines[0].strip()
        print(f"Patching Base Address...")
        print(f"Original (Line 1): {old_instruction}")
        print(f"New      (Line 1): {new_instruction_hex} (Base: {target_address_hex})")
        
        lines[0] = new_instruction_hex + "\n"

        # 5. Write the output file
        with open(output_file, 'w') as f:
            f.writelines(lines)
            
        print(f"Success! Patched file saved to: {output_file}")

    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python patch_mem.py <input.mem> <output.mem> <new_base_address_hex>")
        print("Example: python patch_mem.py gcd_std.mem gcd_custom.mem 0x00010000")
    else:
        patch_mem_file(sys.argv[1], sys.argv[2], sys.argv[3])
