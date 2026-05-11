 `timescale 1ns / 1ps

 module tb_datapath();

     logic clk, rst;

     // Simple clock/reset
     initial begin
         clk = 0;
         forever #5 clk = ~clk;
     end

     // DUT
//     datapath #(.DATA_MEM_WORDS(65535), .INSTRUCTION_MEM_WORDS(65535), .BOOTLOADER_MEM_WORDS(65535)) dut (
//         .clk(clk),
//         .rst(rst)
//     );
     
     datapath #(.DATA_MEM_WORDS(1024), .INSTRUCTION_MEM_WORDS(1024), .BOOTLOADER_MEM_WORDS(1024)) dut (
         .clk(clk),
         .rst(rst)
     );

    //NB: tests expected to fail:
    // -- task3/string (uses .string for pre initialized memory),
    // -- task3/width (unaligned addressing),
    // -- t11 (unaligned addressing), 
    // -- t12 (C code: need to set data mem base address),
    // -- t13 (C code: need to set data mem base address),
    // -- t14 (uses .string for pre initialized memory)    
     

     // ------------ Test control ------------
     // XSim runs from RISC_V/RISC_V.sim/sim_1/behav/xsim
     // So we need to go up 4 levels to reach RISC_V/, then into tests/
     string test_root = "C:/Users/niels/OneDrive/Skrivebord/digital_design_code/AMR_probe_soc/project/hdl/test/tb_risc_v/";       // default root (relative to sim run dir)
     string test = "task3/recursive";                // test name like "task1/addpos"
     
     int cycles = 1000000;                         // default max cycles per test
     int passed, failed;
     
     // Allow +arg to overwrite
     initial begin
        void'($value$plusargs("TESTROOT=%s", test_root));
        void'($value$plusargs("CYCLES=%d", cycles));
        void'($value$plusargs("TEST=%s", test));
    end

     
     // HELPERS
     
     task automatic load_program(string memfile);
         $display("[TB] Loading program %s", memfile);
         $readmemh(memfile, dut.bootloader.mem);
     endtask
     
      task automatic load_expected(string resfile, output logic [31:0] exp[32]);
         int fd; int bytes_read; byte buffer[4];
         for (int i = 0; i < 32; i++) exp[i] = 32'h0;
         
         fd = $fopen(resfile, "rb");  // Open in binary mode
         if (!fd) $fatal(1, "[TB] Cannot open %s", resfile);
         
         $display("[TB] Loading expected results from %s", resfile);
         
         // Read 32 registers x 4 bytes each (little-endian)
         for (int i = 0; i < 32; i++) begin
             bytes_read = $fread(buffer, fd);
             if (bytes_read == 4) begin
                 // Little-endian: buffer[0] is LSB, buffer[3] is MSB
                 exp[i] = {buffer[3], buffer[2], buffer[1], buffer[0]};
                 if (exp[i] != 0) begin
                     $display("[TB]   x%0d = 0x%08h", i, exp[i]);
                 end
             end else if (bytes_read > 0) begin
                 $warning("[TB] Incomplete read for register x%0d", i);
             end
         end
         $fclose(fd);
     endtask
     
     function automatic bit compare_regs(logic [31:0] exp[32]);
         bit ok = 1;
         for (int i = 0; i < 32; i++) begin
             logic [31:0] act = dut.id_stage.regFile.regs[i];
             if (act !== exp[i]) begin
                 ok = 0;
                 $display("[TB] Mismatch x%0d exp=%08h got=%08h", i, exp[i], act);
             end
         end
         return ok;
     endfunction
     
     task automatic reset_dut();
         rst = 1'b0;
         repeat (1) @(posedge clk);
         rst = 1'b1;
         repeat (4) @(posedge clk);
         rst = 1'b0;
     endtask
     
     // Wait for ECALL instruction (opcode 0x73, full instr = 0x00000073)
     task automatic wait_for_ecall();
         int timeout = cycles;  // Use cycles as timeout
         int cycle_count = 0;
         logic [31:0] current_instr;
         
         forever begin
             @(posedge clk);
             cycle_count++;
             
             // Check instruction in ID stage
             current_instr = dut.id_stage.IR;
             
             // ECALL: opcode = 0x73 (bits [6:0] = 7'b1110011)
             if (current_instr[6:0] == 7'b1110011 && current_instr[31:7] == 25'b0) begin
                 $display("[TB] ECALL detected at cycle %0d (IR=0x%08h)", cycle_count, current_instr);
                 repeat (2) @(posedge clk);
                 break;
             end
             
             // Timeout check
             if (cycle_count >= timeout) begin
                 $warning("[TB] Timeout: ECALL not found after %0d cycles", timeout);
                 break;
             end
         end
     endtask
     
     task automatic run_one(string tname);
         string memfile = {test_root, tname, ".mem"};
         string resfile = {test_root, tname, ".res"};
         logic [31:0] exp[32];

         load_program(memfile);
         load_expected(resfile, exp);
         reset_dut();
         wait_for_ecall();  // Run until ECALL instead of fixed cycles

         if (compare_regs(exp)) begin
             passed++;
             $display("[TB] PASS %s", tname);
         end else begin
             failed++;
             $display("[TB] FAIL %s", tname);
         end
     endtask

     // ------------ Main ------------
     initial begin
         // Wait one time unit so plusargs are parsed
         #1;
         if (test == "") $fatal(1, "[TB] No tests specified");

         run_one(test);

        $display("[TB] Summary: %0d total, %0d passed, %0d failed",
                 passed + failed, passed, failed);
         $finish;
     end
     
     
 endmodule
