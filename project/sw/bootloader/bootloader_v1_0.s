
_start:
    li a0, 0x40000008   # UART base
    li a1, 0xDEADBEEF   # Magic terminator word
    li a2, 0 # byte counter (image index)
    li a3, 0x24 # header size in bytes
    li a5, 32 # max shift
    li a7, 0x40000000 # LEDS address
    li a6, 1
    sw a6, 0(a7) # Pull up first LED to indicate bootloader is running

main:
    # Read 4 bytes from UART and accumulate into a word
    # The word will be stored in t0 after the loop
    read_word:
        li t0, 0 # word accumulator
        li t1, 0 # shift counter
        wait_valid:
            lw t2, 0(a0) # Read UART data
            andi t3, t2, 0x200 # Extract valid byte
            beq t3, x0, wait_valid # Wait for valid byte
        accumulate:
            andi t2, t2, 0xFF # Mask to get the byte
            sll t2, t2, t1 # Shift byte to correct position
            or t0, t0, t2 # Accumulate byte into word
            addi t1, t1, 8 # Increment shift counter            
            bltu t1, a5, wait_valid # Read 4 bytes for a full word

    process_word:        
        beq t0, a1, done # If we read the magic terminator, we're done        
        bgeu a2, a3, process_image # If we've read the header, start loading image
        j process_header # else process header word

process_header:
    beq a2, zero, store_s0  # : .text VMA (runtime address)  
    li t3, 0x4
    beq a2, t3, store_s1    # : .text LMA (load address)    
    li t3, 0x8
    beq a2, t3, store_s2    # : .text size (in bytes)   
    li t3, 0xC
    beq a2, t3, store_s3    # : .rodata VMA  
    li t3, 0x10
    beq a2, t3, store_s4    # : .rodata LMA   
    li t3, 0x14
    beq a2, t3, store_s5    # : .rodata size   
    li t3, 0x18
    beq a2, t3, store_s6    # : .data VMA   
    li t3, 0x1C
    beq a2, t3, store_s7    # : .data LMA    
    li t3, 0x20
    beq a2, t3, store_s8    # : .data size
    # guard jump for invalid header words (e.g. if we read past the header)
    j increment_byte_counter # If we somehow read past the header, just keep counting bytes until we reach the terminator. Should never happen

    store_s0:
        mv s9, t0 # save original text VMA
        mv s0, t0 # Store first header word in s0
        j increment_byte_counter
    store_s1:
        mv s1, t0 # Store second header word in s1
        j increment_byte_counter
    store_s2:
        mv s2, t0 # Store third header word in s2
        j increment_byte_counter
    store_s3:
        mv s3, t0 # Store fourth header word in s3
        j increment_byte_counter
    store_s4:
        mv s4, t0 # Store fifth header word in s4
        j increment_byte_counter
    store_s5:
        mv s5, t0 # Store sixth header word in s5
        j increment_byte_counter
    store_s6:
        mv s6, t0 # Store seventh header word in s6
        j increment_byte_counter
    store_s7:
        mv s7, t0 # Store eighth header word in s7
        j increment_byte_counter
    store_s8:
        mv s8, t0 # Store ninth header word in s8
        j increment_byte_counter

process_image:   
    # process sections
    bge a2, s7, load_data # If we've read all headers, start loading data
    bge a2, s4, load_rodata
    bge a2, s1,  load_text
    j increment_byte_counter

load_text:
    sw t0, 0(s0) # Store word in .text LMA
    addi s0, s0, 4 # Increment .text LMA pointer
    j increment_byte_counter

load_rodata:  
    sw t0, 0(s3) # Store word in .rodata LMA
    addi s3, s3, 4 # Increment .rodata LMA pointer
    j increment_byte_counter

load_data: 
    sw t0, 0(s6) # Store word in .data LMA
    addi s6, s6, 4 # Increment .data LMA pointer
    j increment_byte_counter


increment_byte_counter:
    addi a2, a2, 4 # Increment byte counter (4 bytes per word)
    j main
    
done:
    sw zero, 0(a7) # Turn off LED to indicate we're done loading
    jalr x0, s9, 0
