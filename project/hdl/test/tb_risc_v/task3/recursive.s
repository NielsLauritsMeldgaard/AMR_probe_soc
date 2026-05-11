	li sp, 0x2000FFFC
	jal main
	li a7, 10
	ecall

recursive:
	addi	sp,sp,-32
	sw	ra,28(sp)
	sw	s0,24(sp)
	addi	s0,sp,32
	sw	a0,-20(s0)
	lw	a5,-20(s0)
	bgt	a5, zero, L2
	li	a5,1
	j	L3
L2:
	lw	a5,-20(s0)
	addi	a5,a5,-1
	mv	a0,a5
	call	recursive
	mv	a5,a0
	addi	a5,a5,1
L3:
	mv	a0,a5
	lw	ra,28(sp)
	lw	s0,24(sp)
	addi	sp,sp,32
	jr	ra

main:
	addi	sp,sp,-16
	sw	ra,12(sp)
	sw	s0,8(sp)
	addi	s0,sp,16
	li	a0,100
	call	recursive
	mv	a5,a0
	mv	a0,a5
	lw	ra,12(sp)
	lw	s0,8(sp)
	addi	sp,sp,16
	jr	ra