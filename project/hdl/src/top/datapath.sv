`timescale 1ns / 1ps

module datapath #(
    // Memory Sizes (in 32-bit words)
    // Total size: 1800 Kbits = 1800 * 1024 bits = 1800 * 1024 / 32 words = 57600 words or 230.4 kB = 225 KB
    // Weight: 70% / 30% split between instruction and data memory
    // Leave some extra headroom 
    parameter INSTRUCTION_MEM_WORDS = 32768, // 131072 bytes or 128 KB  
    parameter DATA_MEM_WORDS = 16384, // 65536 bytes or 64 KB
    parameter BOOTLOADER_MEM_WORDS = 84,
    parameter GPO_WIDTH = 11, // Number of general purpose output pins
    parameter GPI_WIDTH = 9 // Number of general purpose input pins
)(
    input logic clk, 
    input logic rst,
    
    // --- Physical FPGA I/O Pins ---
    output logic [1:0]  leds,
    input  logic        buttons,
    output logic        uart_tx,
    input  logic        uart_rx,
    output logic        driver_set, driver_rst,
    input  logic [3:0]  JA_in,  // JA Header (4-pin input)
    output logic [3:0]  JA_out, // JA Header (4-pin output)

    // --- accelerometer ---
    output logic accel_cs,  // Accelerometer Chip Select (active low) (SPI slave select)
    output logic accel_mosi,
    input logic accel_miso,
    output logic accel_sclk,
    input logic accel_int1,  // Accelerometer Interrupt Signal 1
    input logic accel_int2,  // Accelerometer Interrupt Signal 2

    // --- LoRa ---
    output logic lora_cs,  // LoRa Chip Select (active low) (SPI slave select)
    output logic lora_rst, // LoRa Reset (active low)
    output logic lora_mosi,
    input logic lora_miso,
    output logic lora_sclk,
    input logic lora_dio, // LoRa DIO0 Interrupt Signal
    output logic lora_rf_sw, // 1 receive mode, 0 transmit mode (controls RF switch for LoRa antenna)
    input logic lora_busy, // LoRa Busy Signal

    // --- ADC2 ---
    output logic adc2_cs,  // ADC Chip Select (active low) (SPI slave select)
    input logic  adc2_busy, // ADC busy signal (active high, high when conversion is in progress)
    output logic adc2_shdn, // ADC Shutdown Control (tied low for always on)
    output logic adc2_cnv, // ADC Convert Start Signal
    output logic adc2_mosi,
    input logic adc2_miso,
    output logic adc2_sclk,


    // --- DAC --- 
    // Asynchronous Reset Input. The RESET input is falling edge sensitive.
    output logic        dac_rst, // active high reset for DAC (tied to 1 so that DAC is always active)
    // Active Low Control Input. This is the frame synchronization signal for the input data. When SYNC goes low, data is transferred in on the falling edges of the next 24 clocks.
    output logic        dac_sync, // tied to 0 so that DAC is always ready to receive data
    output logic        dac_sclk, // DAC serial clock input (max 50MHz)
    output logic        dac_sdin, // DAC serial data input
    // LDAC can be operated in two modes, asynchronously and synchronously. Pulsing this pin low allows any or all DAC registers to be updated if the input registers have new data. This allows all DAC outputs to simultaneously update. This pin can also be tied permanently low.
    output logic        dac_ldac, // tied to 0 for synchronous mode (DAC outputs update immediately when new data is received)

    // --- ADC1 ---
    output logic adc1_cs,  // ADC Chip Select (active low) (SPI slave select)
    input logic  adc1_busy, // ADC busy signal (active high, high when conversion is in progress)
    output logic adc1_sdi, // ADC Serial Data Input (MOSI)
    input logic [2:0]  adc1_sdo, // ADC Serial Data Output (MISO) - 3 bits for 3 channels
    output logic adc1_sclk, // ADC Serial Clock Input
    output logic adc1_cnv, // ADC Convert Start Signal 
    output logic adc1_shdn, // ADC Shutdown Control
    
    // Turn off RGB led (no IO driver yet, so we just tie it off)
    output logic led0_b, led0_g, led0_r,

    // XADC pins
    input logic vauxp4,
    input logic vauxn4
);    
    // --- Global Control Signals ---
    logic stall;
    logic br_dec; 
    logic [1:0] aluFwdSrc; 
    logic [4:0] rs1, rs2;
    logic fwd_mem_data;

    // --- CPU Master Instruction Wishbone Bus (I-Bus) ---
    logic [31:0] iwb_adr, iwb_dat;
    logic        iwb_stb, iwb_ack;

    // --- Interconnect Slave Wires for Instruction Memory ---
    // Slave 0: bootloader ROM
    logic [31:0] s0bb_adr, s0bb_dat;
    logic        s0bb_stb, s0bb_ack;
    
    // --- Slave 1: Instruction RAM ---    
    logic [31:0] s1im_adr, s1im_dat;
    logic        s1im_stb, s1im_ack;

    // --- CPU Master Data Wishbone (M-Bus) ---
    logic [31:0] dwb_adr, dwb_dat_o, dwb_dat_i;
    logic [3:0]  dwb_sel;
    logic        dwb_we, dwb_stb, dwb_ack;

    // --- Interconnect Slave Wires ---
    // Slave 0: Data RAM
    logic [31:0] s0_adr, s0_dat_w, s0_dat_r;
    logic [3:0]  s0_sel;
    logic        s0_we, s0_stb, s0_ack;

    // --- Slave 1: IO Subsystem ---
    logic [31:0] s1_adr, s1_dat_w, s1_dat_r;
    logic [3:0]  s1_sel;
    logic        s1_we, s1_stb, s1_ack;
    
    // --- Slave 2: instruction ram ---
    logic [31:0] s2_adr, s2_dat_w, s2_dat_r;
    logic [3:0]  s2_sel;
    logic        s2_we, s2_stb, s2_ack;

    // --- Pipeline Intermediate Wires ---
    logic [31:0] pc_w, instr_w, rs1_d, rs2_id, imm, pc_id, pc_ex, imm_ex, ex_res;
    logic [4:0]  aluOP, rd_id, rd_wb;
    logic [2:0]  funct3_id;
    logic [1:0]  addr_offset_id;
    logic        mToR, rW, rW_wb, aluSrc_id, branch_id;

    // --- JALR Connection Wires ---
    logic        is_jal_or_jalr_id, jal_or_jalr_ex;
    logic [31:0] jal_or_jalr_target_ex;
    
    // --- Sync rst signal ---
    logic rst_meta, rst_sync_internal;
    logic rst_sync;

    // --- SPI and GPIO connections ---
    logic [GPO_WIDTH-1:0] gpio_out; // General Purpose Output from IO manager to peripherals
    logic [GPI_WIDTH-1:0] gpio_in;   // General Purpose Input from peripherals to IO manager
    logic MISO, MOSI, SCLK, SPI_SS; // SPI signals (shared between multiple peripherals, so we route them as separate signals rather than dedicated peripheral outputs)
    
    logic dac_send;
    logic dac_busy;
    always_comb begin
        // --- GLOBAL STALL LOGIC ---
        //assign stall = (iwb_stb && !iwb_ack) || (dwb_stb && !dwb_ack);
        // iwb_ack is now 1, so we only stall when Data Memory (dwb) is busy
        //assign stall = (1'b1 && !iwb_ack) || ((mToR | dwb_we) && !dwb_ack);
        stall   =   0;

        // Tie off RGB LED (no IO driver yet, so we just tie it off)
        led0_b  =   1;
        led0_g  =   1;
        led0_r  =   1;

        // --- HMC DSP block--


        // --- SPI and GPIO connections ---
        // Connect GPO register from IO manager to physical pins
        accel_cs        =       gpio_out[0]; 
        lora_cs         =       gpio_out[1]; 
        lora_rf_sw      =       gpio_out[2];
        adc2_cs         =       gpio_out[3];
        adc2_shdn       =       gpio_out[4];
        adc2_cnv        =       gpio_out[5];
        JA_out          =       gpio_out[9:6];
        lora_rst        =       gpio_out[10];
        // Connect GPI physical pins to IO manager
        gpio_in[0]      =       accel_int1;
        gpio_in[1]      =       accel_int2;
        gpio_in[2]      =       lora_dio;
        gpio_in[3]      =       lora_busy;
        gpio_in[4]      =       adc2_busy;
        gpio_in[8:5]    =       JA_in;
        // Distribute SPI signals from IO manager to peripherals (shared SPI bus)
        adc2_sclk       =       SCLK;
        lora_sclk       =       SCLK;
        accel_sclk      =       SCLK;
        adc2_mosi       =       MOSI;
        lora_mosi       =       MOSI;
        accel_mosi      =       MOSI; 
        //MISO            =       adc2_miso || lora_miso || accel_miso;
        //MISO            =       lora_miso || accel_miso;
        //MISO            =       accel_miso;
        case ({gpio_out[0], gpio_out[1], gpio_out[3]}) // {accel_cs, lora_cs, adc2_cs}
            3'b011: MISO = accel_miso; // Connect to accelerometer MISO when accel_cs is active
            3'b101: MISO = lora_miso; // Connect to LoRa MISO when lora_cs is active
            3'b110: MISO = adc2_miso; // Connect to ADC2 MISO when adc2_cs is active
            default: MISO = 1'b1; // High impedance for invalid states (both CS active, which shouldn't happen)
        endcase
        // lora hardcoded value        
        dac_send = 1;  // allows dac to convert all incoming serial data to Vout
    end

    
    always_ff @(posedge clk) begin
            rst_meta <= rst;
            rst_sync_internal <= rst_meta;
    end
    
    `ifdef SYNTHESIS
    BUFG rst_bufg_inst_r (
        .I(rst_sync_internal),
        .O(rst_sync) 
    );
    `else
    // For simulation, just wire through without BUFG
    assign rst_sync = rst_sync_internal;
    `endif

    // --- 2. STAGE 1: INSTRUCTION FETCH (IF) ---
    IF_stage if_stage (
        .clk(clk), .rst(rst_sync), .stall(stall),
        .pc_sel(br_dec), .pc_from_ex(pc_ex), .imm_from_ex(imm_ex),
        .iwb_adr_o(iwb_adr), .iwb_stb_o(iwb_stb),
        .iwb_dat_i(iwb_dat), .iwb_ack_i(iwb_ack),
        .pc_if_o(pc_w), .instr_if_o(instr_w),                
        .jal_or_jalr_ex_sel(jal_or_jalr_ex),
        .jal_or_jalr_target_ex(jal_or_jalr_target_ex)
    );

       // --- 3. STAGE 2: INSTRUCTION DECODE (ID) ---
    ID id_stage (
        .clk(clk), .rst(rst_sync), .stall(stall),
        .instr_id_i(instr_w), .pc_id_i(pc_w),
        .rd_data_wb(ex_res), .rd_addr_wb(rd_wb), .regWrite_wb(rW_wb),
        .ex_res(ex_res), .fwd_mem_wdata(fwd_mem_data), .branch_taken(br_dec),
        .dwb_adr_o(dwb_adr), .dwb_dat_o(dwb_dat_o), .dwb_sel_o(dwb_sel),
        .dwb_we_o(dwb_we), .dwb_stb_o(dwb_stb), .dwb_ack_i(dwb_ack),
        .rs1_data_o(rs1_d), .rs2_immData(rs2_id), .imm_id_o(imm),
        .pc_id_o(pc_id), .aluCtrl_id_o(aluOP), .memToReg_id_o(mToR),
        .regWrite_id_o(rW), .rd_addr_id_o(rd_id),
        .funct3_id_o(funct3_id), .addr_offset_id_o(addr_offset_id),
        .rs1_id_o(rs1), .rs2_id_o(rs2),
        .aluSrc2_id_o(aluSrc_id), .branch_id_o(branch_id),
        .is_jal_or_jalr_id_o(is_jal_or_jalr_id)   //jal or jalr signal 
    );
    
    // --- 4. STAGE 3: EXECUTE (EX) ---
    EX ex_stage (
        .clk(clk), .rst(rst_sync), .stall(stall),
        .rs1_val_reg_next(rs1_d), .rs2_imm_reg_next(rs2_id),
        .pc_ex_i(pc_id), .imm_ex_i(imm),
        .aluOP_ex_i(aluOP), .memToReg_ex_i(mToR), .regWrite_ex_i(rW),
        .rd_addr_ex_i(rd_id), .funct3_ex_i(funct3_id), .addr_offset_ex_i(addr_offset_id),
        .aluFwdSrc(aluFwdSrc), .dwb_dat_i(dwb_dat_i), 
        .res_ex_o(ex_res), .rd_addr_ex_o(rd_wb), .regWrite_ex_o(rW_wb),
        .pc_ex_o(pc_ex), .imm_ex_o(imm_ex), .br_dec_ex_o(br_dec),
        .is_jal_or_jalr_ex_i(is_jal_or_jalr_id),      // // Pass jump signals, From ID
        .jal_or_jalr_target_ex_o(jal_or_jalr_target_ex), // To IF
        .jal_or_jalr_ex_o(jal_or_jalr_ex)    // To IF
    );
    // --- 5. BUS INTERCONNECT ---
    iwb_interconnect iwb_bus_matrix (
        // Master: CPU Instruction Wishbone Bus
        .m_iwb_adr_i(iwb_adr), .m_iwb_stb_i(iwb_stb),
        .m_iwb_dat_o(iwb_dat), .m_iwb_ack_o(iwb_ack),

        // Slave 0: Bootloader ROM
        .s0bb_adr_o(s0bb_adr), .s0bb_stb_o(s0bb_stb),
        .s0bb_dat_i(s0bb_dat), .s0bb_ack_i(s0bb_ack),

        // Slave 1: Instruction RAM
        .s1im_adr_o(s1im_adr), .s1im_stb_o(s1im_stb),
        .s1im_dat_i(s1im_dat), .s1im_ack_i(s1im_ack)
    );
    

    dwb_interconnect dwb_bus_matrix (
        .m_adr_i(dwb_adr), .m_dat_i(dwb_dat_o), .m_sel_i(dwb_sel),
        .m_we_i(dwb_we), .m_stb_i(dwb_stb),
        .m_dat_o(dwb_dat_i), .m_ack_o(dwb_ack),

        // Slave 0: RAM
        .s0_adr_o(s0_adr), .s0_dat_o(s0_dat_w), .s0_sel_o(s0_sel),
        .s0_we_o(s0_we), .s0_stb_o(s0_stb),
        .s0_dat_i(s0_dat_r), .s0_ack_i(s0_ack),

        // Slave 1: IO Manager
        .s1_adr_o(s1_adr), .s1_dat_o(s1_dat_w), .s1_sel_o(s1_sel),
        .s1_we_o(s1_we), .s1_stb_o(s1_stb),
        .s1_dat_i(s1_dat_r), .s1_ack_i(s1_ack),
        
        // Slave 2: IRAM
        .s2_adr_o(s2_adr), .s2_dat_o(s2_dat_w), .s2_sel_o(s2_sel),
        .s2_we_o(s2_we), .s2_stb_o(s2_stb),
        .s2_dat_i(s2_dat_r), .s2_ack_i(s2_ack)
    );

    // --- 6. SLAVE 1: IO PERIPHERAL MANAGER ---
    io_manager #(.GPO_WIDTH(GPO_WIDTH), .GPI_WIDTH(GPI_WIDTH)) 
    peripherals
    (
        .clk(clk), .rst(rst_sync),
        // wishbone slave interface (CPU side)
        .wb_adr_i(s1_adr), .wb_dat_i(s1_dat_w), .wb_stb_i(s1_stb),
        .wb_we_i(s1_we), .wb_dat_o(s1_dat_r), .wb_ack_o(s1_ack),
        // Physical Pins
        .leds(leds), .buttons(buttons),
        .uart_tx(uart_tx), .uart_rx(uart_rx),
        .MISO(MISO), .MOSI(MOSI), .SCLK(SCLK),
        .gpio_out(gpio_out), .gpio_in(gpio_in),
        // XADC pins
        .vauxp4(vauxp4), .vauxn4(vauxn4),
        //sr logic driver pins
        .driver_set(driver_set),
        .driver_rst(driver_rst),
        
        .adc1_cnv(adc1_cnv),
        .adc1_sclk(adc1_sclk),
        .adc1_busy(adc1_busy),
        .adc1_sdi(adc1_sdi),
        .adc1_sdo(adc1_sdo),
        .adc1_shdn(adc1_shdn),
        .adc1_cs(adc1_cs),
        .dac_send(dac_send),           // start conversion dac
        .dac_busy(dac_busy), 
        .dac_rst(dac_rst),
        .dac_sclk(dac_sclk),
        .dac_sdin(dac_sdin),
        .dac_sync(dac_sync),
        .dac_ldac(dac_ldac)
    );
    // --- 7. SLAVE 0: DATA RAM ---
    data_memory #(.MEM_WORDS(DATA_MEM_WORDS)) data_mem (
        .clk(clk), .rst(rst_sync),
        .addr(s0_adr), .Wdata(s0_dat_w), .sel(s0_sel),
        .En(s0_stb && !s0_we), .We(s0_stb && s0_we),
        .Rdata(s0_dat_r), .Ack(s0_ack)
    );

    // --- 8. INSTRUCTION MEMORY (Dual-Port) ---
    instruction_memory #(.MEM_WORDS(INSTRUCTION_MEM_WORDS)) instr_mem (
        .clk(clk), .rst(rst_sync),
        // PORT A: Write from Data WB (Bootloader writes via slave 2)
        .a_dwb_adr_i(s2_adr), .a_dwb_dat_i(s2_dat_w), .a_dwb_sel_i(s2_sel),
        .a_dwb_we_i(s2_we), .a_dwb_stb_i(s2_stb),
        .a_dwb_dat_o(s2_dat_r), .a_dwb_ack_o(s2_ack),
        // PORT B: Read from Instr WB (CPU instruction fetch via slave 1)
        .b_iwb_adr_i(s1im_adr), .b_iwb_stb_i(s1im_stb),
        .b_iwb_dat_o(s1im_dat), .b_iwb_ack_o(s1im_ack)
    );
    
    // --- 9. I-WB SLAVE 0: BOOTLOADER ROM ---
    brom #(.MEM_WORDS(BOOTLOADER_MEM_WORDS))bootloader (
        .clk(clk), .rst(rst_sync),
        .wb_adr_i(s0bb_adr), .wb_stb_i(s0bb_stb),
        .wb_dat_o(s0bb_dat), .wb_ack_o(s0bb_ack)
    );

    // --- 9. FORWARDING UNIT ---
    forwarding_unit fwd_unit (
        .rW_wb(rW_wb), .dwb_we(dwb_we), .rd_wb(rd_wb),
        .rs1(rs1), .rs2(rs2), .aluSrc_id(aluSrc_id),
        .branch_id(branch_id), .aluFwdSrc(aluFwdSrc),
        .fwd_mem_data(fwd_mem_data)
    );
    
endmodule
