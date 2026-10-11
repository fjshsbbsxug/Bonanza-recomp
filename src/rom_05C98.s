| ROM 0x05C98

sub_05C98:
	lea (0xFF0000).l,a0
	move.w (0xFF17A4).l,d0
	move.w d0,d1
	lsl.w #0x3,d1
	adda.w d1,a0
	lea (0xFF07A0).l,a1
	lea (0xFF0FA0).l,a2
	move.w (0xFF17A6).l,d1
	move.w (0xFF17AA).l,d2
loc_05CC2:
	tst.w d1
	beq.w loc_05CE2
	cmpi.w #0x40,d0
	beq.w loc_05D6E
	addq.w #0x1,d0
	moveq #0x0,d7
	move.w d0,d7
	andi.w #0x3F,d7
	add.l (a1)+,d7
	move.l d7,(a0)+
	move.l (a1)+,(a0)+
	subq.w #0x1,d1
loc_05CE2:
	tst.w d2
	bne.w loc_05CF0
	tst.w d1
	beq.w loc_05D0C
	bra.s loc_05CC2
loc_05CF0:
	cmpi.w #0x40,d0
	beq.w loc_05D6E
	addq.w #0x1,d0
	moveq #0x0,d7
	move.w d0,d7
	andi.w #0x3F,d7
	add.l (a2)+,d7
	move.l d7,(a0)+
	move.l (a2)+,(a0)+
	subq.w #0x1,d2
	bra.s loc_05CC2
loc_05D0C:
	lea (0xFF0BA0).l,a1
	lea (0xFF13A0).l,a2
	move.w (0xFF17A8).l,d1
	move.w (0xFF17AC).l,d2
loc_05D24:
	tst.w d1
	beq.w loc_05D44
	cmpi.w #0x40,d0
	beq.w loc_05D6E
	addq.w #0x1,d0
	moveq #0x0,d7
	move.w d0,d7
	andi.w #0x3F,d7
	add.l (a1)+,d7
	move.l d7,(a0)+
	move.l (a1)+,(a0)+
	subq.w #0x1,d1
loc_05D44:
	tst.w d2
	bne.w loc_05D52
	tst.w d1
	beq.w loc_05D6E
	bra.s loc_05D24
loc_05D52:
	cmpi.w #0x40,d0
	beq.w loc_05D6E
	addq.w #0x1,d0
	moveq #0x0,d7
	move.w d0,d7
	andi.w #0x3F,d7
	add.l (a2)+,d7
	move.l d7,(a0)+
	move.l (a2)+,(a0)+
	subq.w #0x1,d2
	bra.s loc_05D24
loc_05D6E:
	tst.w d0
	bne.w loc_05D80
	lea (0xFF0000).l,a0
	clr.l (a0)+
	clr.l (a0)
	rts
loc_05D80:
	andi.w #0xFFC0,-0x6(a0)
	rts
sub_05D88:
	lea (0xFF304E).l,a4
	cmpi.w #0xC0,(a4,d0.w)
	bcs.w loc_05E26
	cmpi.w #0x140,(a4,d0.w)
	bcc.w loc_05E26
	lea (0xFF3252).l,a6
	move.w (a6,d0.w),d7
	beq.w loc_05E26
sub_05DB0:
	lsl.w #0x2,d7
	lea (tbl_24B80).l,a6
	movea.l (a6,d7.w),a6
	move.w (a6)+,d4
	lsr.w #0x8,d4
	lea (0xFF0000).l,a5
	move.w (0xFF17A4).l,d7
	lsl.w #0x3,d7
	adda.w d7,a5
	lea (0xFF32FE).l,a4
	tst.w (a4,d0.w)
	bne.w loc_05E28
loc_05DDE:
	addq.w #0x1,(0xFF17A4).l
	moveq #0x0,d7
	move.b (a6)+,d7
	ext.w d7
	lea (0xFF30FA).l,a4
	add.w (a4,d0.w),d7
	move.w d7,(a5)
	moveq #0x0,d7
	move.b (a6)+,d7
	ext.w d7
	lea (0xFF304E).l,a4
	add.w (a4,d0.w),d7
	move.w d7,0x6(a5)
	move.w (a6)+,d7
	move.w (0xFF17A4).l,d6
	andi.w #0x3F,d6
	add.w d6,d7
	move.w d7,0x2(a5)
	move.w (a6)+,0x4(a5)
	addq.w #0x8,a5
	dbra d4,loc_05DDE
loc_05E26:
	rts
loc_05E28:
	addq.w #0x1,(0xFF17A4).l
	moveq #0x0,d7
	move.b (a6)+,d7
	ext.w d7
	lea (0xFF30FA).l,a4
	add.w (a4,d0.w),d7
	move.w d7,(a5)
	moveq #0x0,d7
	move.b (a6)+,d7
	ext.w d7
	move.w (a6),d6
	lsr.w #0x7,d6
	andi.w #0x18,d6
	addq.w #0x8,d6
	add.w d7,d6
	lea (0xFF304E).l,a4
	move.w (a4,d0.w),d7
	sub.w d6,d7
	move.w d7,0x6(a5)
	move.w (a6)+,d7
	move.w (0xFF17A4).l,d6
	andi.w #0x3F,d6
	add.w d6,d7
	move.w d7,0x2(a5)
	move.w (a6)+,d7
	eori.w #0x800,d7
	move.w d7,0x4(a5)
	addq.w #0x8,a5
	dbra d4,loc_05E28
	rts
tbl_05E86:
	.long dat_05FF2
	.long dat_06000
	.long dat_0600E
	.long dat_0601C
	.long dat_0602A
	.long dat_06038
	.long dat_06046
	.long dat_0602A
	.long dat_06054
	.long dat_06062
	.long dat_06070
	.long dat_0607E
	.long dat_0608C
	.long dat_0609A
	.long dat_060A8
	.long dat_060A8
	.long dat_060A8
	.long dat_060A8
	.long dat_060B6
	.long dat_060C4
	.long dat_060D2
	.long dat_060E0
	.long dat_060EE
	.long dat_060FC
	.long dat_0610A
	.long dat_06118
	.long dat_06126
	.long dat_06134
	.long dat_06142
	.long dat_06150
	.long dat_06150
	.long dat_0615E
	.long dat_0616C
	.long dat_0617A
	.long dat_06188
	.long dat_0616C
	.long dat_0616C
	.long dat_0616C
	.long dat_06196
	.long dat_061A4
	.long dat_061B2
	.long dat_061C0
	.long dat_061CE
	.long dat_061DC
	.long dat_061EA
	.long dat_061EA
	.long dat_061EA
	.long dat_061EA
	.long dat_061F8
	.long dat_06206
	.long dat_06214
	.long dat_06222
	.long dat_06230
	.long dat_0623E
	.long dat_0624C
	.long dat_0625A
	.long dat_06268
	.long dat_06276
	.long dat_06284
	.long dat_06292
	.long dat_06292
	.long dat_062A0
	.long dat_062AE
	.long dat_062BC
	.long dat_062CA
	.long dat_062AE
	.long dat_062AE
	.long dat_062AE
	.long dat_062D8
	.long dat_062E6
	.long dat_062F4
	.long dat_06302
	.long dat_06310
	.long dat_0631E
	.long dat_0632C
	.long dat_0633A
	.long dat_06348
	.long dat_06356
	.long dat_06364
	.long dat_06372
	.long dat_06380
	.long dat_0638E
	.long dat_063AA
	.long dat_063B8
	.long dat_063C6
	.long dat_063D4
	.long dat_0639C
	.long dat_063E2
	.long dat_063F0
	.long dat_063FE
	.long tbl_0640C
dat_05FF2:
	.long dat_40000
	.long 0x0000AD00
	.byte 0x00,0x08,0x00,0x02,0x00,0x01
dat_06000:
	.byte 0x00,0x04,0x09,0x86,0x00,0x00,0x00,0x00,0x00,0x32,0x00,0x02,0x00,0x00
dat_0600E:
	.byte 0x00,0x04,0x00,0x20,0x00,0x00,0xA4,0x00,0x00,0x60,0x00,0x10,0x00,0x01
dat_0601C:
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x01
dat_0602A:
	.byte 0x00,0x04,0x0F,0xC6,0x00,0x00,0xA0,0x00,0x00,0x80,0x00,0x80,0x00,0x01
dat_06038:
	.byte 0x00,0x05,0x6B,0xF8,0x00,0x00,0xA0,0x00,0x00,0x80,0x00,0x80,0x00,0x01
dat_06046:
	.byte 0x00,0x05,0x71,0xD4,0x00,0x00,0xA0,0x00,0x00,0x80,0x00,0x80,0x00,0x01
dat_06054:
	.byte 0x00,0x05,0xA0,0x00,0x00,0x00,0x20,0x00,0x00,0x28,0x00,0x08,0x00,0x00
dat_06062:
	.byte 0x00,0x05,0xA5,0x20,0x00,0x00,0x25,0x00,0x00,0x20,0x00,0x10,0x00,0x00
dat_06070:
	.byte 0x00,0x05,0xA9,0x20,0x00,0x00,0x24,0xC0,0x00,0x20,0x00,0x04,0x00,0x00
dat_0607E:
	.byte 0x00,0x05,0xAD,0x20,0x00,0x00,0x28,0xC0,0x00,0x29,0x00,0x04,0x00,0x00
dat_0608C:
	.byte 0x00,0x05,0xB4,0x40,0x00,0x00,0x25,0x00,0x00,0x38,0x00,0x0E,0x00,0x00
dat_0609A:
	.byte 0x00,0x05,0xBB,0x40,0x00,0x00,0x25,0x00,0x00,0x1C,0x00,0x04,0x00,0x00
dat_060A8:
	.byte 0x00,0x05,0xCB,0x40,0x00,0x00,0x25,0x00,0x00,0x40,0x00,0x04,0x00,0x00
dat_060B6:
	.byte 0x00,0x05,0xD3,0x40,0x00,0x00,0x25,0x00,0x00,0x24,0x00,0x08,0x00,0x00
dat_060C4:
	.byte 0x00,0x05,0xD7,0xC0,0x00,0x00,0x25,0x00,0x00,0x2C,0x00,0x0A,0x00,0x00
dat_060D2:
	.byte 0x00,0x05,0xDD,0x40,0x00,0x00,0x25,0x00,0x00,0x1C,0x00,0x05,0x00,0x00
dat_060E0:
	.byte 0x00,0x05,0xE0,0xC0,0x00,0x00,0x25,0x00,0x00,0x30,0x00,0x0C,0x00,0x00
dat_060EE:
	.byte 0x00,0x05,0xE6,0xC0,0x00,0x00,0x25,0x00,0x00,0x10,0x00,0x08,0x00,0x00
dat_060FC:
	.byte 0x00,0x05,0xE8,0xC0,0x00,0x00,0x28,0x80,0x00,0x29,0x00,0x05,0x00,0x00
dat_0610A:
	.byte 0x00,0x05,0xED,0xE0,0x00,0x00,0x25,0x00,0x00,0x3F,0x00,0x04,0x00,0x00
dat_06118:
	.byte 0x00,0x05,0xF5,0xC0,0x00,0x00,0x25,0x00,0x00,0x3C,0x00,0x04,0x00,0x00
dat_06126:
	.byte 0x00,0x05,0xFD,0x40,0x00,0x00,0x25,0x00,0x00,0x24,0x00,0x04,0x00,0x00
dat_06134:
	.byte 0x00,0x06,0x01,0xC0,0x00,0x00,0x25,0x00,0x00,0x32,0x00,0x08,0x00,0x00
dat_06142:
	.byte 0x00,0x06,0x08,0x00,0x00,0x00,0x25,0x00,0x00,0x08,0x00,0x04,0x00,0x00
dat_06150:
	.byte 0x00,0x06,0x09,0xC0,0x00,0x00,0x25,0x00,0x00,0x0A,0x00,0x0A,0x00,0x00
dat_0615E:
	.byte 0x00,0x06,0x0B,0x00,0x00,0x00,0x25,0x00,0x00,0x0A,0x00,0x0A,0x00,0x00
dat_0616C:
	.byte 0x00,0x06,0x0C,0x40,0x00,0x00,0x25,0x00,0x00,0x20,0x00,0x10,0x00,0x00
dat_0617A:
	.byte 0x00,0x06,0x85,0x20,0x00,0x00,0x25,0x00,0x00,0x08,0x00,0x04,0x00,0x00
dat_06188:
	.byte 0x00,0x06,0x87,0x40,0x00,0x00,0x29,0x00,0x00,0x28,0x00,0x10,0x00,0x00
dat_06196:
	.byte 0x00,0x06,0x10,0x40,0x00,0x00,0x30,0x00,0x00,0x2D,0x00,0x08,0x00,0x00
dat_061A4:
	.byte 0x00,0x06,0x15,0xE0,0x00,0x00,0x35,0xA0,0x00,0x1E,0x00,0x10,0x00,0x00
dat_061B2:
	.byte 0x00,0x06,0x19,0xA0,0x00,0x00,0x32,0xE0,0x00,0x2A,0x00,0x05,0x00,0x00
dat_061C0:
	.byte 0x00,0x06,0x1E,0xE0,0x00,0x00,0x38,0x20,0x00,0x3F,0x00,0x05,0x00,0x00
dat_061CE:
	.byte 0x00,0x06,0x26,0xC0,0x00,0x00,0x35,0xA0,0x00,0x3F,0x00,0x08,0x00,0x00
dat_061DC:
	.byte 0x00,0x06,0x2E,0xA0,0x00,0x00,0x35,0xA0,0x00,0x20,0x00,0x04,0x00,0x00
dat_061EA:
	.byte 0x00,0x06,0x40,0x20,0x00,0x00,0x35,0xA0,0x00,0x4B,0x00,0x04,0x00,0x00
dat_061F8:
	.byte 0x00,0x06,0x49,0x80,0x00,0x00,0x35,0xA0,0x00,0x1B,0x00,0x08,0x00,0x00
dat_06206:
	.byte 0x00,0x06,0x4C,0xE0,0x00,0x00,0x35,0xA0,0x00,0x28,0x00,0x0A,0x00,0x00
dat_06214:
	.byte 0x00,0x06,0x51,0xE0,0x00,0x00,0x35,0xA0,0x00,0x1E,0x00,0x05,0x00,0x00
dat_06222:
	.byte 0x00,0x06,0x55,0x40,0x00,0x00,0x35,0xA0,0x00,0x30,0x00,0x0C,0x00,0x00
dat_06230:
	.byte 0x00,0x06,0x5B,0x40,0x00,0x00,0x35,0xA0,0x00,0x12,0x00,0x08,0x00,0x00
dat_0623E:
	.byte 0x00,0x06,0x5D,0x80,0x00,0x00,0x39,0xA0,0x00,0x2E,0x00,0x05,0x00,0x00
dat_0624C:
	.byte 0x00,0x06,0x63,0x40,0x00,0x00,0x35,0xA0,0x00,0x2D,0x00,0x04,0x00,0x00
dat_0625A:
	.byte 0x00,0x06,0x68,0xE0,0x00,0x00,0x35,0xA0,0x00,0x3C,0x00,0x04,0x00,0x00
dat_06268:
	.byte 0x00,0x06,0x70,0x60,0x00,0x00,0x35,0xA0,0x00,0x30,0x00,0x04,0x00,0x00
dat_06276:
	.byte 0x00,0x06,0x76,0x60,0x00,0x00,0x35,0xA0,0x00,0x34,0x00,0x08,0x00,0x00
dat_06284:
	.byte 0x00,0x06,0x7C,0xE0,0x00,0x00,0x35,0xA0,0x00,0x08,0x00,0x04,0x00,0x00
dat_06292:
	.byte 0x00,0x06,0x7E,0xA0,0x00,0x00,0x35,0xA0,0x00,0x08,0x00,0x08,0x00,0x00
dat_062A0:
	.byte 0x00,0x06,0x7F,0xA0,0x00,0x00,0x35,0xA0,0x00,0x08,0x00,0x08,0x00,0x00
dat_062AE:
	.byte 0x00,0x06,0x80,0xA0,0x00,0x00,0x35,0xA0,0x00,0x24,0x00,0x10,0x00,0x00
dat_062BC:
	.byte 0x00,0x06,0x86,0x20,0x00,0x00,0x35,0xA0,0x00,0x09,0x00,0x04,0x00,0x00
dat_062CA:
	.byte 0x00,0x06,0x8C,0x40,0x00,0x00,0x3A,0x20,0x00,0x24,0x00,0x10,0x00,0x00
dat_062D8:
	.byte 0x00,0x05,0x39,0x28,0x00,0x00,0x20,0x00,0x00,0x3A,0x00,0x01,0x00,0x01
dat_062E6:
	.byte 0x00,0x05,0x3D,0x00,0x00,0x00,0x30,0x00,0x00,0x3D,0x00,0x01,0x00,0x01
dat_062F4:
	.byte 0x00,0x05,0x34,0x22,0x00,0x00,0x20,0x00,0x00,0x30,0x00,0x01,0x00,0x01
dat_06302:
	.byte 0x00,0x05,0x36,0x7E,0x00,0x00,0x30,0x00,0x00,0x34,0x00,0x01,0x00,0x01
dat_06310:
	.byte 0x00,0x02,0xB2,0xA6,0x00,0x00,0xF4,0x00,0x00,0x30,0x00,0x01,0x00,0x01
dat_0631E:
	.byte 0x00,0x02,0xB4,0x80,0x00,0x00,0xF4,0x00,0x00,0x30,0x00,0x01,0x00,0x01
dat_0632C:
	.byte 0x00,0x02,0xB6,0x7C,0x00,0x00,0xF4,0x00,0x00,0x30,0x00,0x01,0x00,0x01
dat_0633A:
	.byte 0x00,0x02,0xB8,0x1A,0x00,0x00,0xF4,0x00,0x00,0x30,0x00,0x01,0x00,0x01
dat_06348:
	.byte 0x00,0x02,0xBA,0xB6,0x00,0x00,0xF4,0x00,0x00,0x30,0x00,0x01,0x00,0x01
dat_06356:
	.byte 0x00,0x02,0xBC,0x96,0x00,0x00,0xF4,0x00,0x00,0x30,0x00,0x01,0x00,0x01
dat_06364:
	.byte 0x00,0x02,0xC0,0x18,0x00,0x00,0xF4,0x00,0x00,0x30,0x00,0x01,0x00,0x01
dat_06372:
	.byte 0x00,0x02,0xC1,0xCC,0x00,0x00,0xF4,0x00,0x00,0x30,0x00,0x01,0x00,0x01
dat_06380:
	.byte 0x00,0x05,0x8B,0x64,0x00,0x00,0xF4,0x00,0x00,0x30,0x00,0x01,0x00,0x01
dat_0638E:
	.byte 0x00,0x05,0x8E,0xE7,0x00,0x00,0x2C,0x80,0x00,0x1C,0x00,0x01,0x00,0x01
dat_0639C:
	.byte 0x00,0x05,0x90,0x38,0x00,0x00,0x3C,0x80,0x00,0x1C,0x00,0x01,0x00,0x01
dat_063AA:
	.byte 0x00,0x02,0x8B,0x00,0x00,0x00,0x26,0x80,0x00,0x30,0x00,0x01,0x00,0x01
dat_063B8:
	.byte 0x00,0x02,0x8B,0x00,0x00,0x00,0x36,0x80,0x00,0x30,0x00,0x01,0x00,0x01
dat_063C6:
	.byte 0x00,0x05,0xA4,0x80,0x00,0x00,0x24,0x80,0x00,0x04,0x00,0x01,0x00,0x00
dat_063D4:
	.byte 0x00,0x06,0x13,0x20,0x00,0x00,0x32,0xE0,0x00,0x16,0x00,0x04,0x00,0x00
dat_063E2:
	.byte 0x00,0x04,0x00,0x20,0x00,0x00,0x26,0xC0,0x00,0x40,0x00,0x01,0x00,0x01
dat_063F0:
	.byte 0x00,0x04,0x00,0x20,0x00,0x00,0x36,0xC0,0x00,0x40,0x00,0x01,0x00,0x01
dat_063FE:
	.byte 0x00,0x05,0xB2,0x20,0x00,0x00,0x2D,0xC0,0x00,0x10,0x00,0x02,0x00,0x00
tbl_0640C:
	.long sub_06440
	.long sub_066B2
	.long sub_06958
	.long sub_06440
	.long sub_06C4C
	.long sub_06F5A
	.long sub_07234
	.long sub_06440
	.long sub_07542
	.long sub_07850
	.long sub_07AF6
	.long sub_06440
	.long sub_07E04
sub_06440:
	lea (dat_42438).l,a6
	move.l #0x40000000,(VDP_CTRL).l
	move.w #0xFF,d7
	jsr (sub_01146).l
	lea (dat_436FC).l,a6
	move.l #0x70200002,(VDP_CTRL).l
	move.w #0x6E,d7
	jsr (sub_01146).l
	lea (dat_40FC6).l,a6
	move.l #0x60000002,(VDP_CTRL).l
	move.w #0x7F,d7
	jsr (sub_01146).l
	lea (dat_5A000).l,a6
	move.l #0x60000000,(VDP_CTRL).l
	move.w #0x27,d7
	jsr (sub_0119C).l
	lea (dat_61040).l,a6
	move.l #0x70000000,(VDP_CTRL).l
	move.w #0x2C,d7
	jsr (sub_0119C).l
	lea (dat_415A1).l,a6
	move.l #0x68000002,(VDP_CTRL).l
	move.w #0xE,d7
	jsr (sub_01146).l
	lea (dat_6C586).l,a6
	move.l #0x45800002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6C5F2).l,a6
	move.l #0x4FA00002,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_599A6).l,a6
	move.l #0x7A000003,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_690C0).l,a6
	move.l #0x4FC00002,(VDP_CTRL).l
	move.w #0x1,d7
	jsr (sub_01146).l
	lea (dat_6D436).l,a6
	move.l #0x73800003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_56BCA).l,a6
	move.l #0x6A000003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_599B2).l,a6
	move.l #0x6A800003,(VDP_CTRL).l
	move.w #0x4,d7
	jsr (sub_01146).l
	lea (dat_577B0).l,a6
	move.l #0x50000002,(VDP_CTRL).l
	move.w #0xE,d7
	jsr (sub_01146).l
	lea (dat_579A2).l,a6
	move.l #0x5B600002,(VDP_CTRL).l
	move.w #0x23,d7
	jsr (sub_01146).l
	lea (dat_6C600).l,a6
	move.l #0x40000001,(VDP_CTRL).l
	move.w #0x75,d7
	jsr (sub_01146).l
	lea (dat_6CDEC).l,a6
	move.l #0x7AC00001,(VDP_CTRL).l
	move.w #0x9,d7
	jsr (sub_01146).l
	lea (dat_6CEB0).l,a6
	move.l #0x46800002,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6CF58).l,a6
	move.l #0x70C00003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6D08C).l,a6
	move.l #0x48200002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6D2EA).l,a6
	move.l #0x44000002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6A000).l,a6
	move.l #0x4EC00001,(VDP_CTRL).l
	move.w #0x8A,d7
	jsr (sub_01146).l
	lea (dat_6A7D0).l,a6
	move.l #0x49A00002,(VDP_CTRL).l
	move.w #0x18,d7
	jsr (sub_01146).l
	lea (dat_6A94A).l,a6
	move.l #0x6B200003,(VDP_CTRL).l
	move.w #0xD,d7
	jsr (sub_01146).l
	rts
sub_066B2:
	lea (dat_43D6C).l,a6
	move.l #0x40000000,(VDP_CTRL).l
	move.w #0xFF,d7
	jsr (sub_01146).l
	lea (dat_45070).l,a6
	move.l #0x70200002,(VDP_CTRL).l
	move.w #0x6E,d7
	jsr (sub_01146).l
	lea (dat_40FC6).l,a6
	move.l #0x60000002,(VDP_CTRL).l
	move.w #0x7F,d7
	jsr (sub_01146).l
	lea (dat_5A000).l,a6
	move.l #0x60000000,(VDP_CTRL).l
	move.w #0x27,d7
	jsr (sub_0119C).l
	lea (dat_61040).l,a6
	move.l #0x70000000,(VDP_CTRL).l
	move.w #0x2C,d7
	jsr (sub_0119C).l
	lea (dat_416CB).l,a6
	move.l #0x68000002,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6C586).l,a6
	move.l #0x45800002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6C5F2).l,a6
	move.l #0x4FA00002,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_599A6).l,a6
	move.l #0x7A000003,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_690C0).l,a6
	move.l #0x4FC00002,(VDP_CTRL).l
	move.w #0x1,d7
	jsr (sub_01146).l
	lea (dat_6D436).l,a6
	move.l #0x73800003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_56BCA).l,a6
	move.l #0x6A000003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_599B2).l,a6
	move.l #0x6A800003,(VDP_CTRL).l
	move.w #0x4,d7
	jsr (sub_01146).l
	lea (dat_577B0).l,a6
	move.l #0x50000002,(VDP_CTRL).l
	move.w #0xE,d7
	jsr (sub_01146).l
	lea (dat_28DBA).l,a6
	move.l #0x51E00002,(VDP_CTRL).l
	move.w #0x50,d7
	jsr (sub_01146).l
	lea (dat_57BBC).l,a6
	move.l #0x5D600002,(VDP_CTRL).l
	move.w #0x13,d7
	jsr (sub_01146).l
	lea (dat_6C600).l,a6
	move.l #0x40000001,(VDP_CTRL).l
	move.w #0x75,d7
	jsr (sub_01146).l
	lea (dat_6CEB0).l,a6
	move.l #0x46800002,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6D08C).l,a6
	move.l #0x48200002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6D14C).l,a6
	move.l #0x4CC00002,(VDP_CTRL).l
	move.w #0x16,d7
	jsr (sub_01146).l
	lea (dat_6A000).l,a6
	move.l #0x4EC00001,(VDP_CTRL).l
	move.w #0x8A,d7
	jsr (sub_01146).l
	lea (dat_6A7D0).l,a6
	move.l #0x49A00002,(VDP_CTRL).l
	move.w #0x18,d7
	jsr (sub_01146).l
	lea (dat_6AAB0).l,a6
	move.l #0x60200001,(VDP_CTRL).l
	move.w #0x8B,d7
	jsr (sub_01146).l
	lea (dat_6B334).l,a6
	move.l #0x6E800003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6B478).l,a6
	move.l #0x79000001,(VDP_CTRL).l
	move.w #0x63,d7
	jsr (sub_01146).l
	lea (dat_6BC9E).l,a6
	move.l #0x71A00001,(VDP_CTRL).l
	move.w #0x3A,d7
	jsr (sub_01146).l
	rts
sub_06958:
	lea (dat_4544C).l,a6
	move.l #0x40000000,(VDP_CTRL).l
	move.w #0xFF,d7
	jsr (sub_01146).l
	lea (dat_469C0).l,a6
	move.l #0x70200002,(VDP_CTRL).l
	move.w #0x6E,d7
	jsr (sub_01146).l
	lea (dat_56BF8).l,a6
	move.l #0x60000002,(VDP_CTRL).l
	move.w #0x7F,d7
	jsr (sub_01146).l
	lea (dat_5A000).l,a6
	move.l #0x60000000,(VDP_CTRL).l
	move.w #0x27,d7
	jsr (sub_0119C).l
	lea (dat_61040).l,a6
	move.l #0x70000000,(VDP_CTRL).l
	move.w #0x2C,d7
	jsr (sub_0119C).l
	lea (dat_4181C).l,a6
	move.l #0x68000002,(VDP_CTRL).l
	move.w #0xF,d7
	jsr (sub_01146).l
	lea (dat_6C586).l,a6
	move.l #0x45800002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6C5F2).l,a6
	move.l #0x4FA00002,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_599A6).l,a6
	move.l #0x7A000003,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_690C0).l,a6
	move.l #0x4FC00002,(VDP_CTRL).l
	move.w #0x1,d7
	jsr (sub_01146).l
	lea (dat_6D436).l,a6
	move.l #0x73800003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_56BCA).l,a6
	move.l #0x6A000003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_599B2).l,a6
	move.l #0x6A800003,(VDP_CTRL).l
	move.w #0x4,d7
	jsr (sub_01146).l
	lea (dat_57856).l,a6
	move.l #0x50000002,(VDP_CTRL).l
	move.w #0xE,d7
	jsr (sub_01146).l
	lea (dat_29210).l,a6
	move.l #0x51E00002,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_57CFE).l,a6
	move.l #0x5BE00002,(VDP_CTRL).l
	move.w #0x1F,d7
	jsr (sub_01146).l
	lea (dat_6C600).l,a6
	move.l #0x40000001,(VDP_CTRL).l
	move.w #0x75,d7
	jsr (sub_01146).l
	lea (dat_6CDEC).l,a6
	move.l #0x7AC00001,(VDP_CTRL).l
	move.w #0x9,d7
	jsr (sub_01146).l
	lea (dat_6CEB0).l,a6
	move.l #0x46800002,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6CF58).l,a6
	move.l #0x70C00003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6D08C).l,a6
	move.l #0x48200002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6D14C).l,a6
	move.l #0x4CC00002,(VDP_CTRL).l
	move.w #0x16,d7
	jsr (sub_01146).l
	lea (dat_6A000).l,a6
	move.l #0x4EC00001,(VDP_CTRL).l
	move.w #0x8A,d7
	jsr (sub_01146).l
	lea (dat_6A7D0).l,a6
	move.l #0x49A00002,(VDP_CTRL).l
	move.w #0x18,d7
	jsr (sub_01146).l
	lea (dat_6A94A).l,a6
	move.l #0x6B200003,(VDP_CTRL).l
	move.w #0xD,d7
	jsr (sub_01146).l
	lea (dat_6AAB0).l,a6
	move.l #0x60200001,(VDP_CTRL).l
	move.w #0x8B,d7
	jsr (sub_01146).l
	lea (dat_6B26A).l,a6
	move.l #0x6CE00003,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6B334).l,a6
	move.l #0x6E800003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6B44E).l,a6
	move.l #0x7DA00001,(VDP_CTRL).l
	move.w #0x2,d7
	jsr (sub_01146).l
	rts
sub_06C4C:
	lea (dat_46FC8).l,a6
	move.l #0x40000000,(VDP_CTRL).l
	move.w #0xFF,d7
	jsr (sub_01146).l
	lea (dat_48298).l,a6
	move.l #0x70200002,(VDP_CTRL).l
	move.w #0x6E,d7
	jsr (sub_01146).l
	lea (dat_40FC6).l,a6
	move.l #0x60000002,(VDP_CTRL).l
	move.w #0x7F,d7
	jsr (sub_01146).l
	lea (dat_5A000).l,a6
	move.l #0x60000000,(VDP_CTRL).l
	move.w #0x27,d7
	jsr (sub_0119C).l
	lea (dat_61040).l,a6
	move.l #0x70000000,(VDP_CTRL).l
	move.w #0x2C,d7
	jsr (sub_0119C).l
	lea (dat_4193D).l,a6
	move.l #0x68000002,(VDP_CTRL).l
	move.w #0x16,d7
	jsr (sub_01146).l
	lea (dat_6C586).l,a6
	move.l #0x45800002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6C5F2).l,a6
	move.l #0x4FA00002,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_599A6).l,a6
	move.l #0x7A000003,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_690C0).l,a6
	move.l #0x4FC00002,(VDP_CTRL).l
	move.w #0x1,d7
	jsr (sub_01146).l
	lea (dat_6D436).l,a6
	move.l #0x73800003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_56BCA).l,a6
	move.l #0x6A000003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_599B2).l,a6
	move.l #0x6A800003,(VDP_CTRL).l
	move.w #0x4,d7
	jsr (sub_01146).l
	lea (dat_577B0).l,a6
	move.l #0x50000002,(VDP_CTRL).l
	move.w #0xE,d7
	jsr (sub_01146).l
	lea (dat_28DBA).l,a6
	move.l #0x51E00002,(VDP_CTRL).l
	move.w #0x50,d7
	jsr (sub_01146).l
	lea (dat_57EBE).l,a6
	move.l #0x5CE00002,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C600).l,a6
	move.l #0x40000001,(VDP_CTRL).l
	move.w #0x75,d7
	jsr (sub_01146).l
	lea (dat_6CEB0).l,a6
	move.l #0x46800002,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6D08C).l,a6
	move.l #0x48200002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6D3BE).l,a6
	move.l #0x44000002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6A000).l,a6
	move.l #0x4EC00001,(VDP_CTRL).l
	move.w #0x8A,d7
	jsr (sub_01146).l
	lea (dat_6A7D0).l,a6
	move.l #0x49A00002,(VDP_CTRL).l
	move.w #0x18,d7
	jsr (sub_01146).l
	lea (dat_6A94A).l,a6
	move.l #0x6B200003,(VDP_CTRL).l
	move.w #0xD,d7
	jsr (sub_01146).l
	lea (dat_6AA30).l,a6
	move.l #0x7C000002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6AAB0).l,a6
	move.l #0x60200001,(VDP_CTRL).l
	move.w #0x8B,d7
	jsr (sub_01146).l
	lea (dat_6B334).l,a6
	move.l #0x6E800003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6BC9E).l,a6
	move.l #0x71A00001,(VDP_CTRL).l
	move.w #0x3A,d7
	jsr (sub_01146).l
	lea (dat_6C068).l,a6
	move.l #0x7E000001,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C1F0).l,a6
	move.l #0x41000002,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C358).l,a6
	move.l #0x70C00003,(VDP_CTRL).l
	move.w #0x15,d7
	jsr (sub_01146).l
	rts
sub_06F5A:
	lea (dat_48918).l,a6
	move.l #0x40000000,(VDP_CTRL).l
	move.w #0xFF,d7
	jsr (sub_01146).l
	lea (dat_49B26).l,a6
	move.l #0x70200002,(VDP_CTRL).l
	move.w #0x6E,d7
	jsr (sub_01146).l
	lea (dat_40FC6).l,a6
	move.l #0x60000002,(VDP_CTRL).l
	move.w #0x7F,d7
	jsr (sub_01146).l
	lea (dat_5A000).l,a6
	move.l #0x60000000,(VDP_CTRL).l
	move.w #0x27,d7
	jsr (sub_0119C).l
	lea (dat_61040).l,a6
	move.l #0x70000000,(VDP_CTRL).l
	move.w #0x2C,d7
	jsr (sub_0119C).l
	lea (dat_41B0A).l,a6
	move.l #0x68000002,(VDP_CTRL).l
	move.w #0x14,d7
	jsr (sub_01146).l
	lea (dat_6C586).l,a6
	move.l #0x45800002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6C5F2).l,a6
	move.l #0x4FA00002,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_599A6).l,a6
	move.l #0x7A000003,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_690C0).l,a6
	move.l #0x4FC00002,(VDP_CTRL).l
	move.w #0x1,d7
	jsr (sub_01146).l
	lea (dat_6D436).l,a6
	move.l #0x73800003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_56BCA).l,a6
	move.l #0x6A000003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_599B2).l,a6
	move.l #0x6A800003,(VDP_CTRL).l
	move.w #0x4,d7
	jsr (sub_01146).l
	lea (dat_2958C).l,a6
	move.l #0x51E00002,(VDP_CTRL).l
	move.w #0x1A,d7
	jsr (sub_01146).l
	lea (dat_60900).l,a6
	move.l #0x55400002,(VDP_CTRL).l
	move.w #0x5,d7
	jsr (sub_0119C).l
	lea (dat_67DE0).l,a6
	move.l #0x56000002,(VDP_CTRL).l
	move.w #0x5,d7
	jsr (sub_0119C).l
	lea (dat_58098).l,a6
	move.l #0x5CE00002,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C600).l,a6
	move.l #0x40000001,(VDP_CTRL).l
	move.w #0x75,d7
	jsr (sub_01146).l
	lea (dat_6D08C).l,a6
	move.l #0x48200002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6D14C).l,a6
	move.l #0x4CC00002,(VDP_CTRL).l
	move.w #0x16,d7
	jsr (sub_01146).l
	lea (dat_6D2EA).l,a6
	move.l #0x44000002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6A000).l,a6
	move.l #0x4EC00001,(VDP_CTRL).l
	move.w #0x8A,d7
	jsr (sub_01146).l
	lea (dat_6A94A).l,a6
	move.l #0x6B200003,(VDP_CTRL).l
	move.w #0xD,d7
	jsr (sub_01146).l
	lea (dat_6BC9E).l,a6
	move.l #0x71A00001,(VDP_CTRL).l
	move.w #0x3A,d7
	jsr (sub_01146).l
	lea (dat_6C068).l,a6
	move.l #0x7E000001,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C1F0).l,a6
	move.l #0x41000002,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C358).l,a6
	move.l #0x70C00003,(VDP_CTRL).l
	move.w #0x15,d7
	jsr (sub_01146).l
	lea (dat_6C49C).l,a6
	move.l #0x7C000001,(VDP_CTRL).l
	move.w #0xF,d7
	jsr (sub_01146).l
	rts
sub_07234:
	lea (dat_4A0E6).l,a6
	move.l #0x40000000,(VDP_CTRL).l
	move.w #0xFF,d7
	jsr (sub_01146).l
	lea (dat_4B25E).l,a6
	move.l #0x70200002,(VDP_CTRL).l
	move.w #0x6E,d7
	jsr (sub_01146).l
	lea (dat_40FC6).l,a6
	move.l #0x60000002,(VDP_CTRL).l
	move.w #0x7F,d7
	jsr (sub_01146).l
	lea (dat_5A000).l,a6
	move.l #0x60000000,(VDP_CTRL).l
	move.w #0x27,d7
	jsr (sub_0119C).l
	lea (dat_61040).l,a6
	move.l #0x70000000,(VDP_CTRL).l
	move.w #0x2C,d7
	jsr (sub_0119C).l
	lea (dat_41C9C).l,a6
	move.l #0x68000002,(VDP_CTRL).l
	move.w #0xA,d7
	jsr (sub_01146).l
	lea (dat_6C586).l,a6
	move.l #0x45800002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6C5F2).l,a6
	move.l #0x4FA00002,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_599A6).l,a6
	move.l #0x7A000003,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_690C0).l,a6
	move.l #0x4FC00002,(VDP_CTRL).l
	move.w #0x1,d7
	jsr (sub_01146).l
	lea (dat_6D436).l,a6
	move.l #0x73800003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_56BCA).l,a6
	move.l #0x6A000003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_599B2).l,a6
	move.l #0x6A800003,(VDP_CTRL).l
	move.w #0x4,d7
	jsr (sub_01146).l
	lea (dat_577B0).l,a6
	move.l #0x50000002,(VDP_CTRL).l
	move.w #0xE,d7
	jsr (sub_01146).l
	lea (dat_28DBA).l,a6
	move.l #0x51E00002,(VDP_CTRL).l
	move.w #0x50,d7
	jsr (sub_01146).l
	lea (dat_5823C).l,a6
	move.l #0x5CE00002,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C600).l,a6
	move.l #0x40000001,(VDP_CTRL).l
	move.w #0x75,d7
	jsr (sub_01146).l
	lea (dat_6CDEC).l,a6
	move.l #0x7AC00001,(VDP_CTRL).l
	move.w #0x9,d7
	jsr (sub_01146).l
	lea (dat_6CEB0).l,a6
	move.l #0x46800002,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6CF58).l,a6
	move.l #0x70C00003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6D08C).l,a6
	move.l #0x48200002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6D14C).l,a6
	move.l #0x4CC00002,(VDP_CTRL).l
	move.w #0x16,d7
	jsr (sub_01146).l
	lea (dat_6D2EA).l,a6
	move.l #0x44000002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6A000).l,a6
	move.l #0x4EC00001,(VDP_CTRL).l
	move.w #0x8A,d7
	jsr (sub_01146).l
	lea (dat_6A7D0).l,a6
	move.l #0x49A00002,(VDP_CTRL).l
	move.w #0x18,d7
	jsr (sub_01146).l
	lea (dat_6A94A).l,a6
	move.l #0x6B200003,(VDP_CTRL).l
	move.w #0xD,d7
	jsr (sub_01146).l
	lea (dat_6AAB0).l,a6
	move.l #0x60200001,(VDP_CTRL).l
	move.w #0x8B,d7
	jsr (sub_01146).l
	lea (dat_6B26A).l,a6
	move.l #0x6CE00003,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6B334).l,a6
	move.l #0x6E800003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6B44E).l,a6
	move.l #0x7DA00001,(VDP_CTRL).l
	move.w #0x2,d7
	jsr (sub_01146).l
	rts
sub_07542:
	lea (dat_4B994).l,a6
	move.l #0x40000000,(VDP_CTRL).l
	move.w #0xFF,d7
	jsr (sub_01146).l
	lea (dat_4CD92).l,a6
	move.l #0x70200002,(VDP_CTRL).l
	move.w #0x6C,d7
	jsr (sub_01146).l
	lea (dat_40FC6).l,a6
	move.l #0x60000002,(VDP_CTRL).l
	move.w #0x7F,d7
	jsr (sub_01146).l
	lea (dat_5A000).l,a6
	move.l #0x60000000,(VDP_CTRL).l
	move.w #0x27,d7
	jsr (sub_0119C).l
	lea (dat_61040).l,a6
	move.l #0x70000000,(VDP_CTRL).l
	move.w #0x2C,d7
	jsr (sub_0119C).l
	lea (dat_41D66).l,a6
	move.l #0x68000002,(VDP_CTRL).l
	move.w #0x10,d7
	jsr (sub_01146).l
	lea (dat_6C586).l,a6
	move.l #0x45800002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6C5F2).l,a6
	move.l #0x4FA00002,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_599A6).l,a6
	move.l #0x7A000003,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_690C0).l,a6
	move.l #0x4FC00002,(VDP_CTRL).l
	move.w #0x1,d7
	jsr (sub_01146).l
	lea (dat_6D436).l,a6
	move.l #0x73800003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_56BCA).l,a6
	move.l #0x6A000003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_599B2).l,a6
	move.l #0x6A800003,(VDP_CTRL).l
	move.w #0x4,d7
	jsr (sub_01146).l
	lea (dat_577B0).l,a6
	move.l #0x50000002,(VDP_CTRL).l
	move.w #0xE,d7
	jsr (sub_01146).l
	lea (dat_29414).l,a6
	move.l #0x51E00002,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_583BC).l,a6
	move.l #0x5C600002,(VDP_CTRL).l
	move.w #0x1B,d7
	jsr (sub_01146).l
	lea (dat_6C600).l,a6
	move.l #0x40000001,(VDP_CTRL).l
	move.w #0x75,d7
	jsr (sub_01146).l
	lea (dat_6CDEC).l,a6
	move.l #0x7AC00001,(VDP_CTRL).l
	move.w #0x9,d7
	jsr (sub_01146).l
	lea (dat_6CEB0).l,a6
	move.l #0x46800002,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6CF58).l,a6
	move.l #0x70C00003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6D08C).l,a6
	move.l #0x48200002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6D14C).l,a6
	move.l #0x4CC00002,(VDP_CTRL).l
	move.w #0x16,d7
	jsr (sub_01146).l
	lea (dat_6D2EA).l,a6
	move.l #0x44000002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6A000).l,a6
	move.l #0x4EC00001,(VDP_CTRL).l
	move.w #0x8A,d7
	jsr (sub_01146).l
	lea (dat_6A7D0).l,a6
	move.l #0x49A00002,(VDP_CTRL).l
	move.w #0x18,d7
	jsr (sub_01146).l
	lea (dat_6A94A).l,a6
	move.l #0x6B200003,(VDP_CTRL).l
	move.w #0xD,d7
	jsr (sub_01146).l
	lea (dat_6AAB0).l,a6
	move.l #0x60200001,(VDP_CTRL).l
	move.w #0x8B,d7
	jsr (sub_01146).l
	lea (dat_6B26A).l,a6
	move.l #0x6CE00003,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6B334).l,a6
	move.l #0x6E800003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6B44E).l,a6
	move.l #0x7DA00001,(VDP_CTRL).l
	move.w #0x2,d7
	jsr (sub_01146).l
	rts
sub_07850:
	lea (dat_4D494).l,a6
	move.l #0x40000000,(VDP_CTRL).l
	move.w #0xFF,d7
	jsr (sub_01146).l
	lea (dat_4E610).l,a6
	move.l #0x70200002,(VDP_CTRL).l
	move.w #0x6E,d7
	jsr (sub_01146).l
	lea (dat_40FC6).l,a6
	move.l #0x60000002,(VDP_CTRL).l
	move.w #0x7F,d7
	jsr (sub_01146).l
	lea (dat_5A000).l,a6
	move.l #0x60000000,(VDP_CTRL).l
	move.w #0x27,d7
	jsr (sub_0119C).l
	lea (dat_61040).l,a6
	move.l #0x70000000,(VDP_CTRL).l
	move.w #0x2C,d7
	jsr (sub_0119C).l
	lea (dat_41EC7).l,a6
	move.l #0x68000002,(VDP_CTRL).l
	move.w #0x14,d7
	jsr (sub_01146).l
	lea (dat_6C586).l,a6
	move.l #0x45800002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6C5F2).l,a6
	move.l #0x4FA00002,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_599A6).l,a6
	move.l #0x7A000003,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_690C0).l,a6
	move.l #0x4FC00002,(VDP_CTRL).l
	move.w #0x1,d7
	jsr (sub_01146).l
	lea (dat_6D436).l,a6
	move.l #0x73800003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_56BCA).l,a6
	move.l #0x6A000003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_599B2).l,a6
	move.l #0x6A800003,(VDP_CTRL).l
	move.w #0x4,d7
	jsr (sub_01146).l
	lea (dat_577B0).l,a6
	move.l #0x50000002,(VDP_CTRL).l
	move.w #0xE,d7
	jsr (sub_01146).l
	lea (dat_28DBA).l,a6
	move.l #0x51E00002,(VDP_CTRL).l
	move.w #0x50,d7
	jsr (sub_01146).l
	lea (dat_58594).l,a6
	move.l #0x5CE00002,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C600).l,a6
	move.l #0x40000001,(VDP_CTRL).l
	move.w #0x75,d7
	jsr (sub_01146).l
	lea (dat_6CEB0).l,a6
	move.l #0x46800002,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6D08C).l,a6
	move.l #0x48200002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6D14C).l,a6
	move.l #0x4CC00002,(VDP_CTRL).l
	move.w #0x16,d7
	jsr (sub_01146).l
	lea (dat_6A000).l,a6
	move.l #0x4EC00001,(VDP_CTRL).l
	move.w #0x8A,d7
	jsr (sub_01146).l
	lea (dat_6A7D0).l,a6
	move.l #0x49A00002,(VDP_CTRL).l
	move.w #0x18,d7
	jsr (sub_01146).l
	lea (dat_6A94A).l,a6
	move.l #0x6B200003,(VDP_CTRL).l
	move.w #0xD,d7
	jsr (sub_01146).l
	lea (dat_6AA30).l,a6
	move.l #0x7C000002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6B478).l,a6
	move.l #0x79000001,(VDP_CTRL).l
	move.w #0x63,d7
	jsr (sub_01146).l
	lea (dat_6BB3A).l,a6
	move.l #0x76C00001,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	rts
sub_07AF6:
	lea (dat_4E95A).l,a6
	move.l #0x40000000,(VDP_CTRL).l
	move.w #0xFF,d7
	jsr (sub_01146).l
	lea (dat_4FE66).l,a6
	move.l #0x70200002,(VDP_CTRL).l
	move.w #0x6E,d7
	jsr (sub_01146).l
	lea (dat_571D4).l,a6
	move.l #0x60000002,(VDP_CTRL).l
	move.w #0x7F,d7
	jsr (sub_01146).l
	lea (dat_5A000).l,a6
	move.l #0x60000000,(VDP_CTRL).l
	move.w #0x27,d7
	jsr (sub_0119C).l
	lea (dat_61040).l,a6
	move.l #0x70000000,(VDP_CTRL).l
	move.w #0x2C,d7
	jsr (sub_0119C).l
	lea (dat_42073).l,a6
	move.l #0x68000002,(VDP_CTRL).l
	move.w #0x12,d7
	jsr (sub_01146).l
	lea (dat_6C586).l,a6
	move.l #0x45800002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6C5F2).l,a6
	move.l #0x4FA00002,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_599A6).l,a6
	move.l #0x7A000003,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_690C0).l,a6
	move.l #0x4FC00002,(VDP_CTRL).l
	move.w #0x1,d7
	jsr (sub_01146).l
	lea (dat_6D436).l,a6
	move.l #0x73800003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_56BCA).l,a6
	move.l #0x6A000003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_599B2).l,a6
	move.l #0x6A800003,(VDP_CTRL).l
	move.w #0x4,d7
	jsr (sub_01146).l
	lea (dat_578FC).l,a6
	move.l #0x50000002,(VDP_CTRL).l
	move.w #0xE,d7
	jsr (sub_01146).l
	lea (dat_28DBA).l,a6
	move.l #0x51E00002,(VDP_CTRL).l
	move.w #0x50,d7
	jsr (sub_01146).l
	lea (dat_5871C).l,a6
	move.l #0x5CE00002,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C600).l,a6
	move.l #0x40000001,(VDP_CTRL).l
	move.w #0x75,d7
	jsr (sub_01146).l
	lea (dat_6CDEC).l,a6
	move.l #0x7AC00001,(VDP_CTRL).l
	move.w #0x9,d7
	jsr (sub_01146).l
	lea (dat_6CEB0).l,a6
	move.l #0x46800002,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6CF58).l,a6
	move.l #0x70C00003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6D08C).l,a6
	move.l #0x48200002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6D14C).l,a6
	move.l #0x4CC00002,(VDP_CTRL).l
	move.w #0x16,d7
	jsr (sub_01146).l
	lea (dat_6D2EA).l,a6
	move.l #0x44000002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6A000).l,a6
	move.l #0x4EC00001,(VDP_CTRL).l
	move.w #0x8A,d7
	jsr (sub_01146).l
	lea (dat_6A7D0).l,a6
	move.l #0x49A00002,(VDP_CTRL).l
	move.w #0x18,d7
	jsr (sub_01146).l
	lea (dat_6A94A).l,a6
	move.l #0x6B200003,(VDP_CTRL).l
	move.w #0xD,d7
	jsr (sub_01146).l
	lea (dat_6AAB0).l,a6
	move.l #0x60200001,(VDP_CTRL).l
	move.w #0x8B,d7
	jsr (sub_01146).l
	lea (dat_6B26A).l,a6
	move.l #0x6CE00003,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6B334).l,a6
	move.l #0x6E800003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6B44E).l,a6
	move.l #0x7DA00001,(VDP_CTRL).l
	move.w #0x2,d7
	jsr (sub_01146).l
	rts
sub_07E04:
	lea (dat_5055A).l,a6
	move.l #0x40000000,(VDP_CTRL).l
	move.w #0xFF,d7
	jsr (sub_01146).l
	lea (dat_51BCC).l,a6
	move.l #0x70200002,(VDP_CTRL).l
	move.w #0x6E,d7
	jsr (sub_01146).l
	lea (dat_40FC6).l,a6
	move.l #0x60000002,(VDP_CTRL).l
	move.w #0x7F,d7
	jsr (sub_01146).l
	lea (dat_5A000).l,a6
	move.l #0x60000000,(VDP_CTRL).l
	move.w #0x27,d7
	jsr (sub_0119C).l
	lea (dat_61040).l,a6
	move.l #0x70000000,(VDP_CTRL).l
	move.w #0x2C,d7
	jsr (sub_0119C).l
	lea (dat_421EE).l,a6
	move.l #0x68000002,(VDP_CTRL).l
	move.w #0x1A,d7
	jsr (sub_01146).l
	lea (dat_6C586).l,a6
	move.l #0x45800002,(VDP_CTRL).l
	move.w #0x7,d7
	jsr (sub_01146).l
	lea (dat_6C5F2).l,a6
	move.l #0x4FA00002,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_599A6).l,a6
	move.l #0x7A000003,(VDP_CTRL).l
	move.w #0x0,d7
	jsr (sub_01146).l
	lea (dat_690C0).l,a6
	move.l #0x4FC00002,(VDP_CTRL).l
	move.w #0x1,d7
	jsr (sub_01146).l
	lea (dat_6D436).l,a6
	move.l #0x73800003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_56BCA).l,a6
	move.l #0x6A000003,(VDP_CTRL).l
	move.w #0x3,d7
	jsr (sub_01146).l
	lea (dat_599B2).l,a6
	move.l #0x6A800003,(VDP_CTRL).l
	move.w #0x4,d7
	jsr (sub_01146).l
	lea (dat_2945A).l,a6
	move.l #0x51E00002,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_588FA).l,a6
	move.l #0x5B600002,(VDP_CTRL).l
	move.w #0x23,d7
	jsr (sub_01146).l
	lea (dat_6C600).l,a6
	move.l #0x40000001,(VDP_CTRL).l
	move.w #0x75,d7
	jsr (sub_01146).l
	lea (dat_6CDEC).l,a6
	move.l #0x7AC00001,(VDP_CTRL).l
	move.w #0x9,d7
	jsr (sub_01146).l
	lea (dat_6CF58).l,a6
	move.l #0x70C00003,(VDP_CTRL).l
	move.w #0x11,d7
	jsr (sub_01146).l
	lea (dat_6D08C).l,a6
	move.l #0x48200002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6D14C).l,a6
	move.l #0x4CC00002,(VDP_CTRL).l
	move.w #0x16,d7
	jsr (sub_01146).l
	lea (dat_6D2EA).l,a6
	move.l #0x44000002,(VDP_CTRL).l
	move.w #0xB,d7
	jsr (sub_01146).l
	lea (dat_6A000).l,a6
	move.l #0x4EC00001,(VDP_CTRL).l
	move.w #0x8A,d7
	jsr (sub_01146).l
	lea (dat_6A94A).l,a6
	move.l #0x6B200003,(VDP_CTRL).l
	move.w #0xD,d7
	jsr (sub_01146).l
	lea (dat_6AAB0).l,a6
	move.l #0x60200001,(VDP_CTRL).l
	move.w #0x8B,d7
	jsr (sub_01146).l
	lea (dat_6B26A).l,a6
	move.l #0x6CE00003,(VDP_CTRL).l
	move.w #0xC,d7
	jsr (sub_01146).l
	lea (dat_6BC9E).l,a6
	move.l #0x71A00001,(VDP_CTRL).l
	move.w #0x3A,d7
	jsr (sub_01146).l
	lea (dat_6C068).l,a6
	move.l #0x7E000001,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C1F0).l,a6
	move.l #0x41000002,(VDP_CTRL).l
	move.w #0x17,d7
	jsr (sub_01146).l
	lea (dat_6C49C).l,a6
	move.l #0x7C000001,(VDP_CTRL).l
	move.w #0xF,d7
	jsr (sub_01146).l
	rts
sub_080F8:
	rts
sub_080FA:
	move.w (0xFF17A2).l,d0
	andi.w #0x1,d0
	bne.w loc_08338
	lea (0xFF1858).l,a0
	lea (0xFF1868).l,a1
	move.w #0x7,d0
loc_08118:
	move.w (a1),d1
	add.w d1,d1
	tst.w (a0,d1.w)
	beq.w loc_0812E
	add.w d1,d1
	movea.l tbl_0813A(pc,d1.w),a0
	jsr (a0)
	bra.s loc_08138
loc_0812E:
	addq.w #0x1,(a1)
	andi.w #0x7,(a1)
	dbra d0,loc_08118
loc_08138:
	rts
tbl_0813A:
	.long sub_08156
	.long sub_0817A
	.long sub_081B8
	.long sub_081F6
	.long sub_0824E
	.long sub_082BC
	.long sub_082D2
sub_08156:
	tst.w (0xFF186C).l
	bne.w loc_08168
	jsr (sub_28624).l
	bra.s loc_0816E
loc_08168:
	jsr (sub_286E2).l
loc_0816E:
	lea (0xFF1858).l,a0
	clr.w (0.w,a0)
	rts
sub_0817A:
	lea (0xFFC114).l,a0
	tst.w (0xFF186A).l
	bne.w loc_08194
	move.w #0xC500,(a0)+
	move.w #0xCA00,(a0)
	bra.s loc_0819C
loc_08194:
	move.w #0xC500,(a0)+
	move.w #0xC000,(a0)
loc_0819C:
	move.l #sub_08B08,(0xFFC10C).l
	lea (0xFF1858).l,a0
	clr.w 0x2(a0)
	move.w #0x1,0x4(a0)
	rts
sub_081B8:
	lea (0xFFC114).l,a0
	tst.w (0xFF186A).l
	bne.w loc_081D2
	move.w #0xC000,(a0)+
	move.w #0xC500,(a0)
	bra.s loc_081DA
loc_081D2:
	move.w #0xCA00,(a0)+
	move.w #0xC500,(a0)
loc_081DA:
	move.l #sub_08B28,(0xFFC10C).l
	lea (0xFF1858).l,a0
	clr.w 0x4(a0)
	move.w #0x1,0x6(a0)
	rts
sub_081F6:
	lea (0xFF2CD4).l,a0
	move.w (0xFF1880).l,d7
	bpl.w loc_0820C
	add.w (0xFF2CCC).l,d7
loc_0820C:
	move.w d7,(0.w,a0)
	move.w (0xFF187A).l,d7
	tst.w (0xFF186A).l
	beq.w loc_08224
	addi.w #0xA,d7
loc_08224:
	move.w d7,0x2(a0)
	move.w (0xFF1882).l,d7
	andi.w #0x7F,d7
	move.w d7,0x4(a0)
	jsr (sub_284E2).l
	lea (0xFF1858).l,a0
	clr.w 0x6(a0)
	move.w #0x1,0x8(a0)
	rts
sub_0824E:
	lea (0xFF2CD4).l,a0
	move.w (0xFF1880).l,d7
	addi.w #0xA,d7
	cmp.w (0xFF2CCC).l,d7
	bcs.w loc_0826A
	moveq #0x0,d7
loc_0826A:
	move.w d7,(0.w,a0)
	move.w (0xFF187A).l,d7
	tst.w (0xFF186A).l
	beq.w loc_08282
	addi.w #0xA,d7
loc_08282:
	move.w d7,0x2(a0)
	move.w (0xFF1882).l,d7
	andi.w #0x7F,d7
	addi.w #0x28,d7
	cmpi.w #0x80,d7
	bcs.w loc_082A0
	subi.w #0x80,d7
loc_082A0:
	move.w d7,0x4(a0)
	jsr (sub_284E2).l
	lea (0xFF1858).l,a0
	clr.w 0x8(a0)
	move.w #0x1,0xA(a0)
	rts
sub_082BC:
	move.l #sub_08B50,(0xFFC10C).l
	lea (0xFF1858).l,a0
	clr.w 0xA(a0)
	rts
sub_082D2:
	movea.l (0xFF5712).l,a6
	move.l (0xFF5716).l,(VDP_CTRL).l
	move.w (0xFF571A).l,d7
	lea (VDP_DATA).l,a0
loc_082EE:
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	dbra d7,loc_082EE
	lea (0xFF1858).l,a0
	clr.w 0xC(a0)
	tst.w (0xFF5724).l
	beq.w loc_08336
	lea (0xFF571C).l,a6
	move.w #0x3,d6
loc_08320:
	move.w (a6)+,d7
	bne.w loc_0832A
	dbra d6,loc_08320
loc_0832A:
	subq.w #0x1,(0xFF5724).l
	lea (0xFF1864).l,a5
loc_08336:
	rts
loc_08338:
	move.w (0xFF17A2).l,d0
	andi.w #0x1,d0
	beq.w loc_08376
	lea (0xFF229C).l,a0
	lea (0xFF22AC).l,a1
	move.w #0x7,d0
loc_08356:
	move.w (a1),d1
	add.w d1,d1
	tst.w (a0,d1.w)
	beq.w loc_0836C
	add.w d1,d1
	movea.l tbl_08378(pc,d1.w),a0
	jsr (a0)
	bra.s loc_08376
loc_0836C:
	addq.w #0x1,(a1)
	andi.w #0x7,(a1)
	dbra d0,loc_08356
loc_08376:
	rts
tbl_08378:
	.long sub_08394
	.long sub_083B8
	.long sub_083F6
	.long sub_08434
	.long sub_0848C
	.long sub_084FA
	.long sub_08510
sub_08394:
	tst.w (0xFF22B0).l
	bne.w loc_083A6
	jsr (sub_28914).l
	bra.s loc_083AC
loc_083A6:
	jsr (sub_289D2).l
loc_083AC:
	lea (0xFF229C).l,a0
	clr.w (0.w,a0)
	rts
sub_083B8:
	lea (0xFFC118).l,a0
	tst.w (0xFF22AE).l
	bne.w loc_083D2
	move.w #0xD600,(a0)+
	move.w #0xDB00,(a0)
	bra.s loc_083DA
loc_083D2:
	move.w #0xD600,(a0)+
	move.w #0xD100,(a0)
loc_083DA:
	move.l #sub_08D62,(0xFFC110).l
	lea (0xFF229C).l,a0
	clr.w 0x2(a0)
	move.w #0x1,0x4(a0)
	rts
sub_083F6:
	lea (0xFFC118).l,a0
	tst.w (0xFF22AE).l
	bne.w loc_08410
	move.w #0xD100,(a0)+
	move.w #0xD600,(a0)
	bra.s loc_08418
loc_08410:
	move.w #0xDB00,(a0)+
	move.w #0xD600,(a0)
loc_08418:
	move.l #sub_08D82,(0xFFC110).l
	lea (0xFF229C).l,a0
	clr.w 0x4(a0)
	move.w #0x1,0x6(a0)
	rts
sub_08434:
	lea (0xFF2CD4).l,a0
	move.w (0xFF22C4).l,d7
	bpl.w loc_0844A
	add.w (0xFF2CCC).l,d7
loc_0844A:
	move.w d7,(0.w,a0)
	move.w (0xFF22BE).l,d7
	tst.w (0xFF22AE).l
	beq.w loc_08462
	addi.w #0xA,d7
loc_08462:
	move.w d7,0x2(a0)
	move.w (0xFF22C6).l,d7
	andi.w #0x7F,d7
	move.w d7,0x4(a0)
	jsr (sub_287D2).l
	lea (0xFF229C).l,a0
	clr.w 0x6(a0)
	move.w #0x1,0x8(a0)
	rts
sub_0848C:
	lea (0xFF2CD4).l,a0
	move.w (0xFF22C4).l,d7
	addi.w #0xA,d7
	cmp.w (0xFF2CCC).l,d7
	bcs.w loc_084A8
	moveq #0x0,d7
loc_084A8:
	move.w d7,(0.w,a0)
	move.w (0xFF22BE).l,d7
	tst.w (0xFF22AE).l
	beq.w loc_084C0
	addi.w #0xA,d7
loc_084C0:
	move.w d7,0x2(a0)
	move.w (0xFF22C6).l,d7
	andi.w #0x7F,d7
	addi.w #0x28,d7
	cmpi.w #0x80,d7
	bcs.w loc_084DE
	subi.w #0x80,d7
loc_084DE:
	move.w d7,0x4(a0)
	jsr (sub_287D2).l
	lea (0xFF229C).l,a0
	clr.w 0x8(a0)
	move.w #0x1,0xA(a0)
	rts
sub_084FA:
	move.l #sub_08DAA,(0xFFC110).l
	lea (0xFF229C).l,a0
	clr.w 0xA(a0)
	rts
sub_08510:
	movea.l (0xFF57BC).l,a6
	move.l (0xFF57C0).l,(VDP_CTRL).l
	move.w (0xFF57C4).l,d7
	lea (VDP_DATA).l,a0
loc_0852C:
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	move.l (a6)+,(a0)
	dbra d7,loc_0852C
	lea (0xFF229C).l,a0
	clr.w 0xC(a0)
	tst.w (0xFF57CE).l
	beq.w loc_08574
	lea (0xFF57C6).l,a6
	move.w #0x3,d6
loc_0855E:
	move.w (a6)+,d7
	bne.w loc_08568
	dbra d6,loc_0855E
loc_08568:
	subq.w #0x1,(0xFF57CE).l
	lea (0xFF22A8).l,a5
loc_08574:
	rts
sub_08576:
	addq.w #0x1,(0xFF17A2).l
	jsr (sub_080FA).l
	tst.w (0xFF1878).l
	bne.w loc_08796
	move.w (0xFF17A2).l,d1
	andi.w #0x1,d1
	add.w (0xFF1870).l,d1
	lsr.w #0x1,d1
	move.w d1,(0xFF1874).l
	move.w (0xFF1876).l,d0
	btst #0x0,d0
	beq.w loc_085EE
	cmpi.w #0xFFFB,(0xFF187A).l
	bne.w loc_085CA
	cmpi.w #0xFFC9,(0xFF1844).l
	beq.w loc_08636
loc_085CA:
	sub.w d1,(0xFF1844).l
	sub.w d1,(0xFF1850).l
	sub.w d1,(0xFF1854).l
	movem.l d1,-(a7)
	jsr (sub_285A6).l
	movem.l (a7)+,d1
	bra.w loc_08636
loc_085EE:
	btst #0x1,d0
	beq.w loc_08636
	move.w (0xFF2CCE).l,d7
	subi.w #0xA,d7
	cmp.w (0xFF187A).l,d7
	bne.w loc_08616
	cmpi.w #0xFFC9,(0xFF1844).l
	beq.w loc_08636
loc_08616:
	add.w d1,(0xFF1844).l
	add.w d1,(0xFF1850).l
	add.w d1,(0xFF1854).l
	movem.l d1,-(a7)
	jsr (sub_285A6).l
	movem.l (a7)+,d1
loc_08636:
	move.w (0xFF1846).l,d1
	andi.w #0x1,d1
	beq.w loc_08648
	moveq #0x1,d1
	bra.s loc_08660
loc_08648:
	move.w (0xFF17A2).l,d1
	andi.w #0x1,d1
	add.w (0xFF186E).l,d1
	lsr.w #0x1,d1
	move.w d1,(0xFF1872).l
loc_08660:
	move.w (0xFF1876).l,d0
	beq.w loc_08796
	btst #0x2,d0
	beq.w loc_08706
	move.w (0xFF187C).l,d7
	lea (0xFF1858).l,a0
	tst.w (0.w,a0)
	beq.w loc_0869A
	move.w #0x1,d6
	tst.w (0xFF186C).l
	beq.w loc_08698
	move.w #0xFFFF,d6
loc_08698:
	add.w d6,d7
loc_0869A:
	move.w (0xFF2CD0).l,d6
	cmp.w d6,d7
	bne.w loc_086B4
	move.w (0xFF1846).l,d7
	andi.w #0xF,d7
	beq.w loc_08796
loc_086B4:
	sub.w d1,(0xFF1846).l
	add.w d1,(0xFF1852).l
	add.w d1,(0xFF1856).l
	move.w (0xFF1846).l,d7
	bpl.w loc_086D2
	neg.w d7
loc_086D2:
	lsr.w #0x4,d7
	addi.w #0x11,d7
	cmp.w (0xFF187C).l,d7
	beq.w loc_08796
	move.w (0xFF1846).l,d7
	andi.w #0xF,d7
	bne.w loc_08796
	lea (0xFF1858).l,a0
	move.w #0x1,(0.w,a0)
	clr.w (0xFF186C).l
	bra.w loc_08796
loc_08706:
	btst #0x3,d0
	beq.w loc_08796
	move.w (0xFF1880).l,d6
	lea (0xFF1858).l,a0
	tst.w (0.w,a0)
	beq.w loc_08736
	move.w #0x1,d7
	tst.w (0xFF186C).l
	beq.w loc_08734
	move.w #0xFFFF,d7
loc_08734:
	add.w d7,d6
loc_08736:
	cmpi.w #0xFFFE,d6
	bne.w loc_0874C
	move.w (0xFF1846).l,d7
	andi.w #0xF,d7
	beq.w loc_08796
loc_0874C:
	add.w d1,(0xFF1846).l
	sub.w d1,(0xFF1852).l
	sub.w d1,(0xFF1856).l
	move.w (0xFF1846).l,d7
	bpl.w loc_0876A
	neg.w d7
loc_0876A:
	lsr.w #0x4,d7
	subq.w #0x2,d7
	cmp.w d6,d7
	beq.w loc_08796
	move.w (0xFF1846).l,d7
	andi.w #0xF,d7
	bne.w loc_08796
	lea (0xFF1858).l,a0
	move.w #0x1,(0.w,a0)
	move.w #0x1,(0xFF186C).l
loc_08796:
	clr.w (0xFF1876).l
loc_0879C:
	tst.w (0xFF17B6).l
	beq.s loc_0879C
	tst.w (0xFF22BC).l
	bne.w loc_089B8
	move.w (0xFF17A2).l,d1
	andi.w #0x1,d1
	add.w (0xFF22B4).l,d1
	lsr.w #0x1,d1
	move.w d1,(0xFF22B8).l
	move.w (0xFF22BA).l,d0
	btst #0x0,d0
	beq.w loc_08810
	cmpi.w #0xFFFB,(0xFF22BE).l
	bne.w loc_087EC
	cmpi.w #0x159,(0xFF2288).l
	beq.w loc_08858
loc_087EC:
	sub.w d1,(0xFF2288).l
	sub.w d1,(0xFF2294).l
	sub.w d1,(0xFF2298).l
	movem.l d1,-(a7)
	jsr (sub_28896).l
	movem.l (a7)+,d1
	bra.w loc_08858
loc_08810:
	btst #0x1,d0
	beq.w loc_08858
	move.w (0xFF2CCE).l,d7
	subi.w #0xA,d7
	cmp.w (0xFF22BE).l,d7
	bne.w loc_08838
	cmpi.w #0x159,(0xFF2288).l
	beq.w loc_08858
loc_08838:
	add.w d1,(0xFF2288).l
	add.w d1,(0xFF2294).l
	add.w d1,(0xFF2298).l
	movem.l d1,-(a7)
	jsr (sub_28896).l
	movem.l (a7)+,d1
loc_08858:
	move.w (0xFF228A).l,d1
	andi.w #0x1,d1
	beq.w loc_0886A
	moveq #0x1,d1
	bra.s loc_08882
loc_0886A:
	move.w (0xFF17A2).l,d1
	andi.w #0x1,d1
	add.w (0xFF22B2).l,d1
	lsr.w #0x1,d1
	move.w d1,(0xFF22B6).l
loc_08882:
	move.w (0xFF22BA).l,d0
	beq.w loc_089B8
	btst #0x2,d0
	beq.w loc_08928
	move.w (0xFF22C0).l,d7
	lea (0xFF229C).l,a0
	tst.w (0.w,a0)
	beq.w loc_088BC
	move.w #0x1,d6
	tst.w (0xFF22B0).l
	beq.w loc_088BA
	move.w #0xFFFF,d6
loc_088BA:
	add.w d6,d7
loc_088BC:
	move.w (0xFF2CD0).l,d6
	cmp.w d6,d7
	bne.w loc_088D6
	move.w (0xFF228A).l,d7
	andi.w #0xF,d7
	beq.w loc_089B8
loc_088D6:
	sub.w d1,(0xFF228A).l
	add.w d1,(0xFF2296).l
	add.w d1,(0xFF229A).l
	move.w (0xFF228A).l,d7
	bpl.w loc_088F4
	neg.w d7
loc_088F4:
	lsr.w #0x4,d7
	addi.w #0x11,d7
	cmp.w (0xFF22C0).l,d7
	beq.w loc_089B8
	move.w (0xFF228A).l,d7
	andi.w #0xF,d7
	bne.w loc_089B8
	lea (0xFF229C).l,a0
	move.w #0x1,(0.w,a0)
	clr.w (0xFF22B0).l
	bra.w loc_089B8
loc_08928:
	btst #0x3,d0
	beq.w loc_089B8
	move.w (0xFF22C4).l,d6
	lea (0xFF229C).l,a0
	tst.w (0.w,a0)
	beq.w loc_08958
	move.w #0x1,d7
	tst.w (0xFF22B0).l
	beq.w loc_08956
	move.w #0xFFFF,d7
loc_08956:
	add.w d7,d6
loc_08958:
	cmpi.w #0xFFFE,d6
	bne.w loc_0896E
	move.w (0xFF228A).l,d7
	andi.w #0xF,d7
	beq.w loc_089B8
loc_0896E:
	add.w d1,(0xFF228A).l
	sub.w d1,(0xFF2296).l
	sub.w d1,(0xFF229A).l
	move.w (0xFF228A).l,d7
	bpl.w loc_0898C
	neg.w d7
loc_0898C:
	lsr.w #0x4,d7
	subq.w #0x2,d7
	cmp.w d6,d7
	beq.w loc_089B8
	move.w (0xFF228A).l,d7
	andi.w #0xF,d7
	bne.w loc_089B8
	lea (0xFF229C).l,a0
	move.w #0x1,(0.w,a0)
	move.w #0x1,(0xFF22B0).l
loc_089B8:
	clr.w (0xFF22BA).l
	rts
sub_089C0:
	move.w sr,-(a7)
	move.w #0x100,(Z80_BUSREQ).l
	lea (VDP_CTRL).l,a6
	move.w #0x8F80,(a6)
	move.w (0xFFC07C).l,d7
	ori.b #0x10,d7
	move.w d7,(a6)
	move.w #0x9400,(a6)
	move.w #0x931E,(a6)
	move.l #0xFF1884,d7
	move.l d7,d6
	lsr.l #0x1,d7
	andi.l #0xFF,d7
	addi.w #0x9500,d7
	lsl.l #0x8,d7
	lsl.l #0x8,d7
	lsr.l #0x8,d6
	lsr.l #0x1,d6
	andi.l #0xFF,d6
	addi.w #0x9600,d6
	add.l d6,d7
	move.l d7,(a6)
	move.l #0xFF1884,d7
	lsr.l #0x8,d7
	lsr.l #0x8,d7
	lsr.l #0x1,d7
	andi.w #0x7F,d7
	addi.w #0x9700,d7
	move.w d7,(a6)
	moveq #0x0,d7
	move.w (0xFFC114).l,d7
	move.l d7,d6
	andi.l #0x3FFF,d6
	ori.w #0x4000,d6
	move.w d6,(a6)
	lsr.l #0x7,d7
	lsr.l #0x7,d7
	ori.b #0x80,d7
	move.w d7,(0xFF17CA).l
	move.w (0xFF17CA).l,(a6)
	move.w (0xFFC07C).l,(a6)
	move.w #0x0,(Z80_BUSREQ).l
	move.w #0x100,(Z80_BUSREQ).l
	move.w (0xFFC07C).l,d7
	ori.b #0x10,d7
	move.w d7,(a6)
	move.w #0x9400,(a6)
	move.w #0x931E,(a6)
	move.l #0xFF1D84,d7
	move.l d7,d6
	lsr.l #0x1,d7
	andi.l #0xFF,d7
	addi.w #0x9500,d7
	lsl.l #0x8,d7
	lsl.l #0x8,d7
	lsr.l #0x8,d6
	lsr.l #0x1,d6
	andi.l #0xFF,d6
	addi.w #0x9600,d6
	add.l d6,d7
	move.l d7,(a6)
	move.l #0xFF1D84,d7
	lsr.l #0x8,d7
	lsr.l #0x8,d7
	lsr.l #0x1,d7
	andi.w #0x7F,d7
	addi.w #0x9700,d7
	move.w d7,(a6)
	moveq #0x0,d7
	move.w (0xFFC114).l,d7
	addq.w #0x2,d7
	move.l d7,d6
	andi.l #0x3FFF,d6
	ori.w #0x4000,d6
	move.w d6,(a6)
	lsr.l #0x7,d7
	lsr.l #0x7,d7
	ori.b #0x80,d7
	move.w d7,(0xFF17CA).l
	move.w (0xFF17CA).l,(a6)
	move.w (0xFFC07C).l,(a6)
	move.w #0x8F02,(a6)
	move.w #0x0,(Z80_BUSREQ).l
	move.l #0x0,(0xFFC10C).l
	move.w (a7)+,sr
	rts
sub_08B08:
	lea (0xFFC114).l,a0
	move.w (a0)+,d7
	move.w (a0),d6
	move.w #0x500,d5
	jsr (sub_28200).l
	move.l #0x0,(0xFFC10C).l
	rts
sub_08B28:
	lea (0xFFC114).l,a0
	move.w (a0)+,d7
	move.w (a0),d6
	move.w #0x500,d5
	jsr (sub_28200).l
	move.w #0xFFC9,(0xFF1844).l
	move.l #0x0,(0xFFC10C).l
	rts
sub_08B50:
	move.w sr,-(a7)
	move.w #0x100,(Z80_BUSREQ).l
	lea (VDP_CTRL).l,a6
	move.w #0x8F02,(a6)
	move.w (0xFFC07C).l,d7
	ori.b #0x10,d7
	move.w d7,(a6)
	move.w #0x9402,(a6)
	move.w #0x9380,(a6)
	move.l #0xFF1884,d7
	move.l d7,d6
	lsr.l #0x1,d7
	andi.l #0xFF,d7
	addi.w #0x9500,d7
	lsl.l #0x8,d7
	lsl.l #0x8,d7
	lsr.l #0x8,d6
	lsr.l #0x1,d6
	andi.l #0xFF,d6
	addi.w #0x9600,d6
	add.l d6,d7
	move.l d7,(a6)
	move.l #0xFF1884,d7
	lsr.l #0x8,d7
	lsr.l #0x8,d7
	lsr.l #0x1,d7
	andi.w #0x7F,d7
	addi.w #0x9700,d7
	move.w d7,(a6)
	moveq #0x0,d7
	move.w #0xC000,d7
	tst.w (0xFF186A).l
	beq.w loc_08BCC
	move.w #0xCA00,d7
loc_08BCC:
	move.l d7,d6
	andi.l #0x3FFF,d6
	ori.w #0x4000,d6
	move.w d6,(a6)
	lsr.l #0x7,d7
	lsr.l #0x7,d7
	ori.b #0x80,d7
	move.w d7,(0xFF17CA).l
	move.w (0xFF17CA).l,(a6)
	move.w (0xFFC07C).l,(a6)
	nop
	nop
	nop
	nop
	nop
	move.w #0x0,(Z80_BUSREQ).l
	clr.w (0xFF1878).l
	move.l #0x0,(0xFFC10C).l
	move.w (a7)+,sr
	rts
sub_08C1A:
	move.w sr,-(a7)
	move.w #0x100,(Z80_BUSREQ).l
	lea (VDP_CTRL).l,a6
	move.w #0x8F80,(a6)
	move.w (0xFFC07C).l,d7
	ori.b #0x10,d7
	move.w d7,(a6)
	move.w #0x9400,(a6)
	move.w #0x931E,(a6)
	move.l #0xFF22C8,d7
	move.l d7,d6
	lsr.l #0x1,d7
	andi.l #0xFF,d7
	addi.w #0x9500,d7
	lsl.l #0x8,d7
	lsl.l #0x8,d7
	lsr.l #0x8,d6
	lsr.l #0x1,d6
	andi.l #0xFF,d6
	addi.w #0x9600,d6
	add.l d6,d7
	move.l d7,(a6)
	move.l #0xFF22C8,d7
	lsr.l #0x8,d7
	lsr.l #0x8,d7
	lsr.l #0x1,d7
	andi.w #0x7F,d7
	addi.w #0x9700,d7
	move.w d7,(a6)
	moveq #0x0,d7
	move.w (0xFFC118).l,d7
	move.l d7,d6
	andi.l #0x3FFF,d6
	ori.w #0x4000,d6
	move.w d6,(a6)
	lsr.l #0x7,d7
	lsr.l #0x7,d7
	ori.b #0x80,d7
	move.w d7,(0xFF17CA).l
	move.w (0xFF17CA).l,(a6)
	move.w (0xFFC07C).l,(a6)
	move.w #0x0,(Z80_BUSREQ).l
	move.w #0x100,(Z80_BUSREQ).l
	move.w (0xFFC07C).l,d7
	ori.b #0x10,d7
	move.w d7,(a6)
	move.w #0x9400,(a6)
	move.w #0x931E,(a6)
	move.l #0xFF27C8,d7
	move.l d7,d6
	lsr.l #0x1,d7
	andi.l #0xFF,d7
	addi.w #0x9500,d7
	lsl.l #0x8,d7
	lsl.l #0x8,d7
	lsr.l #0x8,d6
	lsr.l #0x1,d6
	andi.l #0xFF,d6
	addi.w #0x9600,d6
	add.l d6,d7
	move.l d7,(a6)
	move.l #0xFF27C8,d7
	lsr.l #0x8,d7
	lsr.l #0x8,d7
	lsr.l #0x1,d7
	andi.w #0x7F,d7
	addi.w #0x9700,d7
	move.w d7,(a6)
	moveq #0x0,d7
	move.w (0xFFC118).l,d7
	addq.w #0x2,d7
	move.l d7,d6
	andi.l #0x3FFF,d6
	ori.w #0x4000,d6
	move.w d6,(a6)
	lsr.l #0x7,d7
	lsr.l #0x7,d7
	ori.b #0x80,d7
	move.w d7,(0xFF17CA).l
	move.w (0xFF17CA).l,(a6)
	move.w (0xFFC07C).l,(a6)
	move.w #0x8F02,(a6)
	move.w #0x0,(Z80_BUSREQ).l
	move.l #0x0,(0xFFC110).l
	move.w (a7)+,sr
	rts
sub_08D62:
	lea (0xFFC118).l,a0
	move.w (a0)+,d7
	move.w (a0),d6
	move.w #0x500,d5
	jsr (sub_28200).l
	move.l #0x0,(0xFFC110).l
	rts
sub_08D82:
	lea (0xFFC118).l,a0
	move.w (a0)+,d7
	move.w (a0),d6
	move.w #0x500,d5
	jsr (sub_28200).l
	move.w #0x159,(0xFF2288).l
	move.l #0x0,(0xFFC110).l
	rts
sub_08DAA:
	move.w sr,-(a7)
	move.w #0x100,(Z80_BUSREQ).l
	lea (VDP_CTRL).l,a6
	move.w #0x8F02,(a6)
	move.w (0xFFC07C).l,d7
	ori.b #0x10,d7
	move.w d7,(a6)
	move.w #0x9402,(a6)
	move.w #0x9380,(a6)
	move.l #0xFF22C8,d7
	move.l d7,d6
	lsr.l #0x1,d7
	andi.l #0xFF,d7
	addi.w #0x9500,d7
	lsl.l #0x8,d7
	lsl.l #0x8,d7
	lsr.l #0x8,d6
	lsr.l #0x1,d6
	andi.l #0xFF,d6
	addi.w #0x9600,d6
	add.l d6,d7
	move.l d7,(a6)
	move.l #0xFF22C8,d7
	lsr.l #0x8,d7
	lsr.l #0x8,d7
	lsr.l #0x1,d7
	andi.w #0x7F,d7
	addi.w #0x9700,d7
	move.w d7,(a6)
	moveq #0x0,d7
	move.w #0xD100,d7
	tst.w (0xFF22AE).l
	beq.w loc_08E26
	move.w #0xDB00,d7
loc_08E26:
	move.l d7,d6
	andi.l #0x3FFF,d6
	ori.w #0x4000,d6
	move.w d6,(a6)
	lsr.l #0x7,d7
	lsr.l #0x7,d7
	ori.b #0x80,d7
	move.w d7,(0xFF17CA).l
	move.w (0xFF17CA).l,(a6)
	move.w (0xFFC07C).l,(a6)
	nop
	nop
	nop
	nop
	nop
	move.w #0x0,(Z80_BUSREQ).l
	clr.w (0xFF22BC).l
	move.l #0x0,(0xFFC110).l
	move.w (a7)+,sr
	rts
sub_08E74:
	tst.w d0
	bne.w loc_08E96
	cmpi.w #0x6,(0xFFC05A).l
	bne.w loc_08E96
	clr.w (0xFF56BE).l
	move.w #0x1A,d7
	jsr (sub_01550).l
loc_08E96:
	move.w #0x8,d7
	jsr (sub_01550).l
	jsr (sub_0AE8A).l
	lea tbl_0B492(pc),a0
	movea.l (a0),a0
	move.l a0,(0xFF568A).l
	clr.w (0xFF5688).l
	clr.w (0xFF6C6A).l
	move.w (0xFFC05A).l,d7
	subq.w #0x1,d7
	add.w d7,d7
	lea dat_08FD8(pc),a6
	move.w (a6,d7.w),d7
	jsr (sub_09846).l
	move.w (0xFFC05A).l,d7
	subq.w #0x1,d7
	lsl.w #0x2,d7
	lea dat_08FF2(pc),a6
	adda.w d7,a6
	move.w (a6)+,(0xFF1840).l
	move.w (a6),(0xFF1842).l
	move.w #0x1,d7
	jsr (sub_097F6).l
	move.w #0x6,(0xFF3706).l
	move.w #0xE,(0xFF37B2).l
	move.w #0x10,(0xFF6C0E).l
	jsr (sub_0B23A).l
	move.w #0xB4,d7
	jsr (sub_09EDA).l
	rts
sub_08F26:
	tst.w d0
	bne.w loc_08F48
	cmpi.w #0x6,(0xFFC05A).l
	bne.w loc_08F48
	clr.w (0xFF5768).l
	move.w #0x38,d7
	jsr (sub_01550).l
loc_08F48:
	move.w #0x26,d7
	jsr (sub_01550).l
	jsr (sub_0AEA6).l
	lea tbl_0D996(pc),a0
	movea.l (a0),a0
	move.l a0,(0xFF5734).l
	clr.w (0xFF5732).l
	clr.w (0xFF6C6C).l
	move.w (0xFFC05A).l,d7
	subq.w #0x1,d7
	add.w d7,d7
	lea dat_08FD8(pc),a6
	move.w (a6,d7.w),d7
	jsr (sub_09894).l
	move.w (0xFFC05A).l,d7
	subq.w #0x1,d7
	lsl.w #0x2,d7
	lea dat_08FF2(pc),a6
	adda.w d7,a6
	move.w (a6)+,(0xFF1840).l
	move.w (a6),(0xFF1842).l
	move.w #0x1,d7
	jsr (sub_0981E).l
	move.w #0x6,(0xFF3708).l
	move.w #0xA,(0xFF37B4).l
	move.w #0xC,(0xFF6C10).l
	jsr (sub_0B2BC).l
	move.w #0xB4,d7
	jsr (sub_09EF4).l
	rts
dat_08FD8:
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x08,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
dat_08FF2:
	.byte 0x04,0x21,0x00,0x80,0x03,0x68,0x00,0x80,0x02,0xD0,0x00,0x80,0x00,0x00,0x00,0x00
	.byte 0x03,0x01,0x00,0x80,0x03,0xC0,0x00,0x80,0x00,0x90,0x00,0x80,0x00,0x00,0x00,0x00
	.byte 0x01,0x91,0x00,0x80,0x00,0xA1,0x00,0xD0,0x02,0xC0,0x00,0x80,0x00,0x00,0x00,0x00
	.byte 0x01,0xB0,0x00,0x80
sub_09026:
	tst.b (0xFF6C6F).l
	beq.w loc_09036
	jmp (sub_12F02).l
loc_09036:
	move.w (0xFF2EF6).l,d7
	bne.w loc_09046
	jmp (sub_137A6).l
loc_09046:
	tst.w d7
	bpl.w loc_09052
	clr.w (0xFF17C0).l
loc_09052:
	clr.w (0xFF56E2).l
	move.b (0xFF17C1).l,d7
	move.w d7,d6
	andi.b #0x20,d7
	tst.w (0xFFC058).l
	bne.w loc_09072
	lsl.b #0x1,d7
	bra.s loc_09074
loc_09072:
	lsr.b #0x1,d7
loc_09074:
	or.w d7,d6
	move.b d6,(0xFF17C1).l
	tst.w (0xFF56BE).l
	beq.w loc_090A0
	subq.w #0x1,(0xFF56C0).l
	bne.w loc_090A0
	clr.w (0xFF56BE).l
	move.w (0xFF56C2).l,(0xFF3252).l
loc_090A0:
	tst.w (0xFF5682).l
	beq.w loc_090B2
	movea.l (0xFF5684).l,a0
	jmp (a0)
loc_090B2:
	move.w (0xFF5688).l,d0
	andi.w #0x8000,d0
	bne.w loc_09188
	move.b (0xFF17C1).l,d7
	moveq #0x0,d0
	move.b (0xFF3A63).l,d0
	andi.b #0x3,d0
	bne.w loc_09140
	btst #0x6,d7
	beq.w loc_090E6
	move.w #0x8002,d0
	bra.w loc_09180
loc_090E6:
	btst #0x4,d7
	beq.w loc_09140
	move.w (0xFF3D12).l,d7
	beq.w loc_0911E
	lea (0xFF9E9E).l,a6
	tst.w (a6,d7.w)
	bne.w loc_0911E
	move.w (0xFF3E6A).l,d7
	cmp.b (0xFF3A62).l,d7
	bne.w loc_0911E
	move.w #0x800F,d0
	bra.w loc_09180
loc_0911E:
	cmpi.w #0x2,(0xFF9DF8).l
	beq.w loc_09140
	move.w #0x0,d0
	move.w #0x8,d7
	jsr (sub_18736).l
	move.w #0x8004,d0
	bra.w loc_09180
loc_09140:
	moveq #0x0,d0
	move.b (0xFF17C0).l,d7
	andi.b #0xF,d7
	beq.w loc_09180
	move.w #0x8019,d0
	andi.b #0x3,d7
	beq.w loc_0917E
	movem.l d0,-(a7)
	jsr (sub_0AFE8).l
	movem.l (a7)+,d0
	beq.w loc_09180
	moveq #0x0,d0
	move.b (0xFF17C0).l,d7
	andi.b #0xC,d7
	beq.w loc_09180
loc_0917E:
	moveq #0x1,d0
loc_09180:
	move.w d0,d7
	jsr (sub_09846).l
loc_09188:
	movea.l (0xFF568A).l,a0
	jsr (a0)
	jsr (sub_09CBA).l
	jsr (sub_09ADE).l
	jsr (sub_094E6).l
	jmp (sub_0932A).l
sub_091A8:
	tst.b (0xFF6C71).l
	beq.w loc_091B8
	jmp (sub_13354).l
loc_091B8:
	move.w (0xFF2EF8).l,d7
	bne.w loc_091C8
	jmp (sub_13ADC).l
loc_091C8:
	tst.w d7
	bpl.w loc_091D4
	clr.w (0xFF17C2).l
loc_091D4:
	clr.w (0xFF578C).l
	move.b (0xFF17C3).l,d7
	move.w d7,d6
	andi.b #0x20,d7
	tst.w (0xFFC058).l
	bne.w loc_091F4
	lsl.b #0x1,d7
	bra.s loc_091F6
loc_091F4:
	lsr.b #0x1,d7
loc_091F6:
	or.w d7,d6
	move.b d6,(0xFF17C3).l
	tst.w (0xFF5768).l
	beq.w loc_09222
	subq.w #0x1,(0xFF576A).l
	bne.w loc_09222
	clr.w (0xFF5768).l
	move.w (0xFF576C).l,(0xFF3254).l
loc_09222:
	tst.w (0xFF572C).l
	beq.w loc_09234
	movea.l (0xFF572E).l,a0
	jmp (a0)
loc_09234:
	move.w (0xFF5732).l,d0
	andi.w #0x8000,d0
	bne.w loc_0930A
	move.b (0xFF17C3).l,d7
	moveq #0x0,d0
	move.b (0xFF3A65).l,d0
	andi.b #0x3,d0
	bne.w loc_092C2
	btst #0x6,d7
	beq.w loc_09268
	move.w #0x8002,d0
	bra.w loc_09302
loc_09268:
	btst #0x4,d7
	beq.w loc_092C2
	move.w (0xFF3D14).l,d7
	beq.w loc_092A0
	lea (0xFF9E9E).l,a6
	tst.w (a6,d7.w)
	bne.w loc_092A0
	move.w (0xFF3E6C).l,d7
	cmp.b (0xFF3A64).l,d7
	bne.w loc_092A0
	move.w #0x800F,d0
	bra.w loc_09302
loc_092A0:
	cmpi.w #0x2,(0xFF9DFA).l
	beq.w loc_092C2
	move.w #0x2,d0
	move.w #0xC,d7
	jsr (sub_18736).l
	move.w #0x8004,d0
	bra.w loc_09302
loc_092C2:
	moveq #0x0,d0
	move.b (0xFF17C2).l,d7
	andi.b #0xF,d7
	beq.w loc_09302
	move.w #0x8019,d0
	andi.b #0x3,d7
	beq.w loc_09300
	movem.l d0,-(a7)
	jsr (sub_0B050).l
	movem.l (a7)+,d0
	beq.w loc_09302
	moveq #0x0,d0
	move.b (0xFF17C2).l,d7
	andi.b #0xC,d7
	beq.w loc_09302
loc_09300:
	moveq #0x1,d0
loc_09302:
	move.w d0,d7
	jsr (sub_09894).l
loc_0930A:
	movea.l (0xFF5734).l,a0
	jsr (a0)
	jsr (sub_09DC6).l
	jsr (sub_09B00).l
	jsr (sub_0966E).l
	jmp (sub_09408).l
sub_0932A:
	cmpi.w #0x1,(0xFF56BE).l
	bne.w loc_09352
	moveq #0x0,d6
	move.w (0xFF17A0).l,d7
	andi.w #0x1,d7
	beq.w loc_0934C
	move.w (0xFF56C2).l,d6
loc_0934C:
	move.w d6,(0xFF3252).l
loc_09352:
	tst.w (0xFF1806).l
	bne.w loc_093EA
	clr.w (0xFF56D0).l
	subq.w #0x1,(0xFF365A).l
	bne.w loc_093EA
	move.w (0xFF35AE).l,d7
	lsl.w #0x3,d7
	movea.l (0xFF3456).l,a6
	move.w 0x2(a6,d7.w),d6
	tst.w (0xFF3252).l
	beq.w loc_0938E
	move.w d6,(0xFF3252).l
loc_0938E:
	move.w d6,(0xFF56C2).l
	move.w 0x4(a6,d7.w),d6
	move.w d6,(0xFF365A).l
	moveq #0x0,d6
	move.b 0x6(a6,d7.w),d6
	ext.w d6
	move.w d6,(0xFF56E6).l
	moveq #0x0,d6
	move.b 0x7(a6,d7.w),d6
	ext.w d6
	move.w d6,(0xFF56E8).l
	move.b 0x8(a6,d7.w),(0xFF56EB).l
	move.b 0x9(a6,d7.w),(0xFF56ED).l
	addq.w #0x1,(0xFF35AE).l
	move.w (a6),d6
	cmp.w (0xFF35AE).l,d6
	bne.w loc_093EA
	clr.w (0xFF35AE).l
	move.w #0x3,(0xFF56D0).l
loc_093EA:
	move.w (0xFF3D12).l,(0xFF3DBE).l
	clr.w (0xFF3D12).l
	clr.w (0xFFA3AE).l
	clr.w (0xFFA3A8).l
	rts
sub_09408:
	cmpi.w #0x1,(0xFF5768).l
	bne.w loc_09430
	moveq #0x0,d6
	move.w (0xFF17A0).l,d7
	andi.w #0x1,d7
	beq.w loc_0942A
	move.w (0xFF576C).l,d6
loc_0942A:
	move.w d6,(0xFF3254).l
loc_09430:
	tst.w (0xFF1806).l
	bne.w loc_094C8
	clr.w (0xFF577A).l
	subq.w #0x1,(0xFF365C).l
	bne.w loc_094C8
	move.w (0xFF35B0).l,d7
	lsl.w #0x3,d7
	movea.l (0xFF345A).l,a6
	move.w 0x2(a6,d7.w),d6
	tst.w (0xFF3254).l
	beq.w loc_0946C
	move.w d6,(0xFF3254).l
loc_0946C:
	move.w d6,(0xFF576C).l
	move.w 0x4(a6,d7.w),d6
	move.w d6,(0xFF365C).l
	moveq #0x0,d6
	move.b 0x6(a6,d7.w),d6
	ext.w d6
	move.w d6,(0xFF5790).l
	moveq #0x0,d6
	move.b 0x7(a6,d7.w),d6
	ext.w d6
	move.w d6,(0xFF5792).l
	move.b 0x8(a6,d7.w),(0xFF5795).l
	move.b 0x9(a6,d7.w),(0xFF5797).l
	addq.w #0x1,(0xFF35B0).l
	move.w (a6),d6
	cmp.w (0xFF35B0).l,d6
	bne.w loc_094C8
	clr.w (0xFF35B0).l
	move.w #0x3,(0xFF577A).l
loc_094C8:
	move.w (0xFF3D14).l,(0xFF3DC0).l
	clr.w (0xFF3D14).l
	clr.w (0xFFA3B0).l
	clr.w (0xFFA3AA).l
	rts
sub_094E6:
	moveq #0x0,d0
	moveq #0x0,d7
	move.b (0xFF3A63).l,d7
	andi.b #0x3,d7
	bne.w loc_09560
	addq.w #0x2,(0xFF186E).l
	move.w (0xFF304E).l,d7
	sub.w (0xFF1852).l,d7
	lea dat_0A41A(pc),a6
	moveq #0x0,d6
	move.b (a6,d7.w),d6
	bpl.w loc_0952A
loc_09518:
	moveq #0x0,d6
	move.b (0xFF3A62).l,d6
	move.w d6,(0xFF2284).l
	bra.w loc_09560
loc_0952A:
	btst #0x4,d6
	beq.w loc_09558
	move.w d6,d5
	andi.b #0xF,d5
	cmp.b (0xFF3A62).l,d5
	bne.s loc_09518
	cmpi.b #0x1,(0xFF3A62).l
	bne.w loc_09552
	bset #0x3,d0
	bra.s loc_09560
loc_09552:
	bset #0x2,d0
	bra.s loc_09560
loc_09558:
	cmp.b (0xFF3A62).l,d6
	bne.s loc_09518
loc_09560:
	lea (0xFF5688).l,a6
	cmpi.w #0x8014,(a6)
	beq.w loc_095BE
	move.w (0xFF2FA2).l,d7
	andi.w #0x1,d7
	beq.w loc_095BE
	move.w (0xFF30FA).l,d7
	sub.w (0xFF1854).l,d7
	bpl.w loc_0958E
	neg.w d7
loc_0958E:
	cmpi.w #0x28,d7
	bcc.w loc_095A0
	tst.w (0xFF56BC).l
	beq.w loc_095EC
loc_095A0:
	cmpi.w #0x8002,(a6)
	beq.w loc_095EC
	cmpi.w #0x800B,(a6)
	bne.w loc_095BE
	move.w (0xFF56BA).l,d7
	andi.w #0x80,d7
	beq.w loc_095EC
loc_095BE:
	addq.w #0x4,(0xFF1870).l
	move.w (0xFF30FA).l,d7
	sub.w (0xFF1854).l,d7
	bpl.w loc_095DC
loc_095D4:
	bset #0x0,d0
	bra.w loc_095F0
loc_095DC:
	cmpi.w #0x20,d7
	beq.w loc_095F0
	bcs.s loc_095D4
	bset #0x1,d0
	bra.s loc_095F0
loc_095EC:
	andi.b #0xFC,d0
loc_095F0:
	tst.w (0xFF2284).l
	beq.w loc_09666
	move.w (0xFF304E).l,d7
	sub.w (0xFF1852).l,d7
	lea dat_0A41A(pc),a6
	moveq #0x0,d6
	move.b (a6,d7.w),d6
	btst #0x4,d6
	beq.w loc_0963C
	andi.w #0xF,d6
	move.w (0xFF2284).l,d5
	cmp.w d5,d6
	beq.w loc_09632
	cmp.b (0xFF3A62).l,d6
	bne.w loc_0963C
loc_09632:
	clr.w (0xFF2284).l
	bra.w loc_09666
loc_0963C:
	move.b (0xFF17C0).l,d7
	andi.b #0xC,d7
	beq.w loc_09650
	addq.w #0x2,(0xFF186E).l
loc_09650:
	moveq #0x2,d7
	cmpi.w #0x2,(0xFF2284).l
	beq.w loc_09660
	moveq #0x3,d7
loc_09660:
	bset d7,d0
	bra.w loc_09666
loc_09666:
	move.w d0,(0xFF1876).l
	rts
sub_0966E:
	moveq #0x0,d0
	moveq #0x0,d7
	move.b (0xFF3A65).l,d7
	andi.b #0x3,d7
	bne.w loc_096E8
	addq.w #0x2,(0xFF22B2).l
	move.w (0xFF3050).l,d7
	sub.w (0xFF2296).l,d7
	lea dat_0A41A(pc),a6
	moveq #0x0,d6
	move.b (a6,d7.w),d6
	bpl.w loc_096B2
loc_096A0:
	moveq #0x0,d6
	move.b (0xFF3A64).l,d6
	move.w d6,(0xFF2CC8).l
	bra.w loc_096E8
loc_096B2:
	btst #0x4,d6
	beq.w loc_096E0
	move.w d6,d5
	andi.b #0xF,d5
	cmp.b (0xFF3A64).l,d5
	bne.s loc_096A0
	cmpi.b #0x1,(0xFF3A64).l
	bne.w loc_096DA
	bset #0x3,d0
	bra.s loc_096E8
loc_096DA:
	bset #0x2,d0
	bra.s loc_096E8
loc_096E0:
	cmp.b (0xFF3A64).l,d6
	bne.s loc_096A0
loc_096E8:
	lea (0xFF5732).l,a6
	cmpi.w #0x8014,(a6)
	beq.w loc_09746
	move.w (0xFF2FA4).l,d7
	andi.w #0x2,d7
	beq.w loc_09746
	move.w (0xFF30FC).l,d7
	sub.w (0xFF2298).l,d7
	bpl.w loc_09716
	neg.w d7
loc_09716:
	cmpi.w #0x28,d7
	bcc.w loc_09728
	tst.w (0xFF5766).l
	beq.w loc_09774
loc_09728:
	cmpi.w #0x8002,(a6)
	beq.w loc_09774
	cmpi.w #0x800B,(a6)
	bne.w loc_09746
	move.w (0xFF5764).l,d7
	andi.w #0x80,d7
	beq.w loc_09774
loc_09746:
	addq.w #0x4,(0xFF22B4).l
	move.w (0xFF30FC).l,d7
	sub.w (0xFF2298).l,d7
	bpl.w loc_09764
loc_0975C:
	bset #0x0,d0
	bra.w loc_09778
loc_09764:
	cmpi.w #0x24,d7
	beq.w loc_09778
	bcs.s loc_0975C
	bset #0x1,d0
	bra.s loc_09778
loc_09774:
	andi.b #0xFC,d0
loc_09778:
	tst.w (0xFF2CC8).l
	beq.w loc_097EE
	move.w (0xFF3050).l,d7
	sub.w (0xFF2296).l,d7
	lea dat_0A41A(pc),a6
	moveq #0x0,d6
	move.b (a6,d7.w),d6
	btst #0x4,d6
	beq.w loc_097C4
	andi.w #0xF,d6
	move.w (0xFF2CC8).l,d5
	cmp.w d5,d6
	beq.w loc_097BA
	cmp.b (0xFF3A64).l,d6
	bne.w loc_097C4
loc_097BA:
	clr.w (0xFF2CC8).l
	bra.w loc_097EE
loc_097C4:
	move.b (0xFF17C2).l,d7
	andi.b #0xC,d7
	beq.w loc_097D8
	addq.w #0x2,(0xFF22B2).l
loc_097D8:
	moveq #0x2,d7
	cmpi.w #0x2,(0xFF2CC8).l
	beq.w loc_097E8
	moveq #0x3,d7
loc_097E8:
	bset d7,d0
	bra.w loc_097EE
loc_097EE:
	move.w d0,(0xFF22BA).l
	rts
sub_097F6:
	move.w d7,(0xFF33AA).l
	clr.w (0xFF35AE).l
	move.w #0x1,(0xFF365A).l
	lsl.w #0x2,d7
	lea (tbl_36E00).l,a6
	movea.l (a6,d7.w),a6
	move.l a6,(0xFF3456).l
	rts
sub_0981E:
	move.w d7,(0xFF33AC).l
	clr.w (0xFF35B0).l
	move.w #0x1,(0xFF365C).l
	lsl.w #0x2,d7
	lea (tbl_37720).l,a6
	movea.l (a6,d7.w),a6
	move.l a6,(0xFF345A).l
	rts
sub_09846:
	move.w (0xFF5688).l,d6
	lsl.w #0x5,d6
	lea dat_0A07A(pc),a6
	add.w d7,d6
	andi.w #0x7FFF,d6
	move.b (a6,d6.w),d6
	beq.w loc_09892
	cmp.w (0xFF5688).l,d7
	beq.w loc_09892
sub_0986A:
	move.w (0xFF5688).l,(0xFF568E).l
	move.l (0xFF568A).l,(0xFF5690).l
	move.w d7,(0xFF5688).l
	lsl.w #0x2,d7
	lea tbl_0B492(pc),a6
	move.l (a6,d7.w),(0xFF568A).l
loc_09892:
	rts
sub_09894:
	move.w (0xFF5732).l,d6
	lsl.w #0x5,d6
	lea dat_0A07A(pc),a6
	add.w d7,d6
	andi.w #0x7FFF,d6
	move.b (a6,d6.w),d6
	beq.w loc_098E0
	cmp.w (0xFF5732).l,d7
	beq.w loc_098E0
sub_098B8:
	move.w (0xFF5732).l,(0xFF5738).l
	move.l (0xFF5734).l,(0xFF573A).l
	move.w d7,(0xFF5732).l
	lsl.w #0x2,d7
	lea tbl_0D996(pc),a6
	move.l (a6,d7.w),(0xFF5734).l
loc_098E0:
	rts
sub_098E2:
	move.w (0xFF33AA).l,(0xFF56D2).l
	move.l (0xFF3456).l,(0xFF56D4).l
	move.w (0xFF35AE).l,(0xFF56D8).l
	move.w (0xFF365A).l,(0xFF56DA).l
	move.w (0xFF3252).l,d7
	tst.w (0xFF56BE).l
	beq.w loc_09920
	move.w (0xFF56C2).l,d7
loc_09920:
	move.w d7,(0xFF56DC).l
	rts
sub_09928:
	move.w (0xFF33AC).l,(0xFF577C).l
	move.l (0xFF345A).l,(0xFF577E).l
	move.w (0xFF35B0).l,(0xFF5782).l
	move.w (0xFF365C).l,(0xFF5784).l
	move.w (0xFF3254).l,d7
	tst.w (0xFF5768).l
	beq.w loc_09966
	move.w (0xFF576C).l,d7
loc_09966:
	move.w d7,(0xFF5786).l
	rts
sub_0996E:
	move.w (0xFF56D2).l,(0xFF33AA).l
	move.l (0xFF56D4).l,(0xFF3456).l
	move.w (0xFF56D8).l,(0xFF35AE).l
	move.w (0xFF56DA).l,(0xFF365A).l
	move.w (0xFF56DC).l,(0xFF3252).l
	rts
sub_099A2:
	move.w (0xFF577C).l,(0xFF33AC).l
	move.l (0xFF577E).l,(0xFF345A).l
	move.w (0xFF5782).l,(0xFF35B0).l
	move.w (0xFF5784).l,(0xFF365C).l
	move.w (0xFF5786).l,(0xFF3254).l
	rts
sub_099D6:
	moveq #0x0,d7
	jsr (sub_09846).l
sub_099DE:
	moveq #0x1,d7
	cmpi.b #0x1,(0xFF3A62).l
	beq.w loc_099EE
	moveq #0x2,d7
loc_099EE:
	jsr (sub_097F6).l
	movea.l (0xFF3456).l,a6
	move.w 0x2(a6),(0xFF3252).l
	move.w #0x10,(0xFF6C0E).l
	move.w #0x6,(0xFF3706).l
	move.w #0xE,(0xFF37B2).l
	rts
sub_09A1C:
	moveq #0x0,d7
	jsr (sub_09894).l
sub_09A24:
	moveq #0x1,d7
	cmpi.b #0x1,(0xFF3A64).l
	beq.w loc_09A34
	moveq #0x2,d7
loc_09A34:
	jsr (sub_0981E).l
	movea.l (0xFF345A).l,a6
	move.w 0x2(a6),(0xFF3254).l
	move.w #0xC,(0xFF6C10).l
	move.w #0x6,(0xFF3708).l
	move.w #0xA,(0xFF37B4).l
	rts
sub_09A62:
	tst.w (0xFF56B2).l
	bne.w loc_09A8A
	move.w #0x1,(0xFF56B2).l
	move.l #sub_09AB6,(0xFF56B4).l
	clr.w (0xFF56B8).l
	move.l a6,(0xFF6C06).l
loc_09A8A:
	rts
sub_09A8C:
	tst.w (0xFF575C).l
	bne.w loc_09AB4
	move.w #0x1,(0xFF575C).l
	move.l #sub_09AB6,(0xFF575E).l
	clr.w (0xFF5762).l
	move.l a6,(0xFF6C0A).l
loc_09AB4:
	rts
sub_09AB6:
	ori.b #0x1,d0
	ori.b #0x1,d0
	ori.b #0x2,d0
	ori.b #0x2,d0
	ori.b #0x2,d0
	ori.b #0x3,d0
	ori.b #0x5,d0
	ori.b #0x6,d0
	ori.b #0x1,d1
	.short 0x0000,0x9AD2	| ori.b #0xD2,d0
sub_09ADE:
	tst.w (0xFF56B2).l
	beq.w loc_09AFE
loc_09AE8:
	movea.l (0xFF56B4).l,a6
	move.w (a6)+,d7
	lsl.w #0x2,d7
	lea (tbl_09B22).l,a5
	movea.l (a5,d7.w),a5
	jmp (a5)
loc_09AFE:
	rts
sub_09B00:
	tst.w (0xFF575C).l
	beq.w loc_09B20
loc_09B0A:
	movea.l (0xFF575E).l,a6
	move.w (a6)+,d7
	lsl.w #0x2,d7
	lea (tbl_09B2A).l,a5
	movea.l (a5,d7.w),a5
	jmp (a5)
loc_09B20:
	rts
tbl_09B22:
	.long sub_09B32
	.long sub_09C9A
tbl_09B2A:
	.long sub_09BE6
	.long sub_09CAA
sub_09B32:
	move.w (a6)+,d7
	move.l a6,(0xFF56B4).l
	move.w (0xFF304E).l,d0
	subi.w #0x20,d0
	movem.l d7,-(a7)
	jsr (sub_0B8BC).l
	movem.l (a7)+,d7
	bne.w loc_09BBC
	movea.l (0xFF6C06).l,a6
	move.w (0xFF304E).l,d0
	add.w 0x4(a6),d0
	addq.w #0x3,d0
	movem.l d7,-(a7)
	jsr (sub_0B8BC).l
	movem.l (a7)+,d7
	bne.w loc_09BBC
	movea.l (0xFF6C06).l,a6
	move.w (0xFF304E).l,d0
	add.w 0xC(a6),d0
	subq.w #0x2,d0
	movem.l d7,-(a7)
	jsr (sub_0B8BC).l
	movem.l (a7)+,d7
	bne.w loc_09BBC
loc_09B9E:
	move.w (0xFF30FA).l,d6
	add.w d7,d6
	move.w d6,(0xFF30FA).l
	move.w (0xFF56B8).l,d6
	add.w d7,d6
	move.w d6,(0xFF56B8).l
	rts
loc_09BBC:
	clr.w (0xFF56B2).l
	jsr (sub_0BF10).l
	cmpi.w #0x4A,(0xFF56B8).l
	bcs.s loc_09B9E
	movem.l d7,-(a7)
	move.w #0x8010,d7
	jsr (sub_09846).l
	movem.l (a7)+,d7
	bra.s loc_09B9E
sub_09BE6:
	move.w (a6)+,d7
	move.l a6,(0xFF575E).l
	move.w (0xFF3050).l,d0
	subi.w #0x20,d0
	movem.l d7,-(a7)
	jsr (sub_0DDB0).l
	movem.l (a7)+,d7
	bne.w loc_09C70
	movea.l (0xFF6C0A).l,a6
	move.w (0xFF3050).l,d0
	add.w 0x4(a6),d0
	addq.w #0x3,d0
	movem.l d7,-(a7)
	jsr (sub_0DDB0).l
	movem.l (a7)+,d7
	bne.w loc_09C70
	movea.l (0xFF6C0A).l,a6
	move.w (0xFF3050).l,d0
	add.w 0xC(a6),d0
	subq.w #0x2,d0
	movem.l d7,-(a7)
	jsr (sub_0DDB0).l
	movem.l (a7)+,d7
	bne.w loc_09C70
loc_09C52:
	move.w (0xFF30FC).l,d6
	add.w d7,d6
	move.w d6,(0xFF30FC).l
	move.w (0xFF5762).l,d6
	add.w d7,d6
	move.w d6,(0xFF5762).l
	rts
loc_09C70:
	clr.w (0xFF575C).l
	jsr (sub_0E392).l
	cmpi.w #0x4A,(0xFF5762).l
	bcs.s loc_09C52
	movem.l d7,-(a7)
	move.w #0x8010,d7
	jsr (sub_09894).l
	movem.l (a7)+,d7
	bra.s loc_09C52
sub_09C9A:
	move.w (a6)+,d7
	movea.l (a6)+,a5
	move.l a5,(0xFF56B4).l
	jmp (loc_09AE8).l
sub_09CAA:
	move.w (a6)+,d7
	movea.l (a6)+,a5
	move.l a5,(0xFF575E).l
	jmp (loc_09B0A).l
sub_09CBA:
	clr.w (0xFF56BA).l
	tst.w (0xFF31A6).l
	beq.w loc_09D46
	move.w (0xFF304E).l,d0
	subi.w #0x20,d0
	moveq #0x0,d7
	jsr (sub_0B8BC).l
	bne.w loc_09D2A
	lea dat_0B858(pc),a6
	move.w (0xFF304E).l,d0
	add.w 0x4(a6),d0
	addq.w #0x2,d0
	moveq #0x0,d7
	jsr (sub_0B8BC).l
	bne.w loc_09D2A
	lea dat_0B858(pc),a6
	move.w (0xFF304E).l,d0
	add.w 0xC(a6),d0
	subq.w #0x2,d0
	moveq #0x0,d7
	jsr (sub_0B8BC).l
	bne.w loc_09D2A
	move.w d2,d7
	andi.b #0x80,d7
	beq.w loc_09D46
	move.w d2,(0xFF56BA).l
	bra.s loc_09D46
loc_09D2A:
	move.w d2,d7
	andi.w #0xF0,d7
	cmpi.w #0x80,d7
	bne.w loc_09D46
	move.w d2,(0xFF56BA).l
	move.w #0x1,(0xFF56BC).l
loc_09D46:
	move.w (0xFF30FA).l,d7
	lsr.w #0x3,d7
	lea dat_09F8A(pc),a6
	move.b (a6,d7.w),d7
	beq.w loc_09D62
	move.w #0x1,(0xFF56BC).l
loc_09D62:
	cmpi.w #0x10,(0xFF31A6).l
	bne.w loc_09DC4
	move.w (0xFF56BA).l,d7
	andi.w #0xF000,d7
	cmpi.w #0xF000,d7
	bne.w loc_09DC4
	tst.w (0xFF1818).l
	beq.w loc_09DAE
	tst.w (0xFF56E4).l
	bne.w loc_09DC4
	cmpi.w #0x800B,(0xFF5688).l
	beq.w loc_09DC4
	move.w #0x8016,d7
	jsr (sub_09846).l
	bra.w loc_09DC4
loc_09DAE:
	move.w (0xFF181E).l,d7
	bne.w loc_09DC4
	moveq #0x0,d0
	move.w #0x28,d7
	jsr (sub_18736).l
loc_09DC4:
	rts
sub_09DC6:
	clr.w (0xFF5764).l
	tst.w (0xFF31A8).l
	beq.w loc_09E5A
	move.w (0xFF3050).l,d0
	subi.w #0x20,d0
	moveq #0x0,d7
	jsr (sub_0DDB0).l
	bne.w loc_09E3E
	lea dat_0DD4C(pc),a6
	move.w (0xFF3050).l,d0
	add.w 0x4(a6),d0
	addq.w #0x2,d0
	moveq #0x0,d7
	jsr (sub_0DDB0).l
	bne.w loc_09E3E
	lea dat_0DD4C(pc),a6
	move.w (0xFF3050).l,d0
	add.w 0xC(a6),d0
	subq.w #0x2,d0
	moveq #0x0,d7
	jsr (sub_0DDB0).l
	bne.w loc_09E3E
	move.w d2,d7
	andi.b #0x80,d7
	beq.w loc_09E5A
	move.b #0x0,(0xFF5764).l
	move.w d2,(0xFF5764).l
	bra.s loc_09E5A
loc_09E3E:
	move.w d2,d7
	andi.w #0xF0,d7
	cmpi.w #0x80,d7
	bne.w loc_09E5A
	move.w d2,(0xFF5764).l
	move.w #0x1,(0xFF5766).l
loc_09E5A:
	move.w (0xFF30FC).l,d7
	lsr.w #0x2,d7
	lea dat_09FDA(pc),a6
	move.b (a6,d7.w),d7
	beq.w loc_09E76
	move.w #0x1,(0xFF5766).l
loc_09E76:
	cmpi.w #0x10,(0xFF31A8).l
	bne.w loc_09ED8
	move.w (0xFF5764).l,d7
	andi.w #0xF000,d7
	cmpi.w #0xF000,d7
	bne.w loc_09ED8
	tst.w (0xFF1818).l
	beq.w loc_09EC2
	tst.w (0xFF578E).l
	bne.w loc_09ED8
	cmpi.w #0x800B,(0xFF5732).l
	beq.w loc_09ED8
	move.w #0x8016,d7
	jsr (sub_09894).l
	bra.w loc_09ED8
loc_09EC2:
	move.w (0xFF1820).l,d7
	bne.w loc_09ED8
	moveq #0x2,d0
	move.w #0x28,d7
	jsr (sub_18736).l
loc_09ED8:
	rts
sub_09EDA:
	move.w d7,(0xFF56C0).l
	move.w #0x1,(0xFF56BE).l
	move.w (0xFF3252).l,(0xFF56C2).l
	rts
sub_09EF4:
	move.w d7,(0xFF576A).l
	move.w #0x1,(0xFF5768).l
	move.w (0xFF3254).l,(0xFF576C).l
	rts
sub_09F0E:
	move.w d7,(0xFF56C0).l
	move.w #0x2,(0xFF56BE).l
	move.w (0xFF3252).l,(0xFF56C2).l
	rts
sub_09F28:
	move.w d7,(0xFF576A).l
	move.w #0x2,(0xFF5768).l
	move.w (0xFF3254).l,(0xFF576C).l
	rts
sub_09F42:
	lea (0xFF931C).l,a6
	move.w #0xC,d7
	move.w #0x1F,d6
loc_09F50:
	cmpi.w #0x2,(a6,d7.w)
	bne.w loc_09F5E
	clr.w (a6,d7.w)
loc_09F5E:
	addq.w #0x2,d7
	dbra d6,loc_09F50
	rts
sub_09F66:
	lea (0xFF931C).l,a6
	move.w #0xC,d7
	move.w #0x1F,d6
loc_09F74:
	cmpi.w #0x4,(a6,d7.w)
	bne.w loc_09F82
	clr.w (a6,d7.w)
loc_09F82:
	addq.w #0x2,d7
	dbra d6,loc_09F74
	rts
dat_09F8A:
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01
dat_09FDA:
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00
	.byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01
dat_0A07A:
	.byte 0x37,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x37,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00
	.byte 0x01,0x00,0x37,0x01,0x01,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x01,0x01
	.byte 0x01,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00
	.byte 0x01,0x01,0x01,0x37,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x00,0x00,0x00,0x37,0x01,0x00,0x00,0x01,0x01,0x01,0x01,0x00,0x00,0x01,0x01
	.byte 0x00,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x37,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x37,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x37,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x37,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x01,0x01,0x01,0x37,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x00,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x01,0x01,0x01,0x01,0x37,0x00,0x01,0x01
	.byte 0x00,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x37,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x37
	.byte 0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x37,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x01,0x01,0x01,0x01,0x00,0x00,0x01,0x01
	.byte 0x00,0x37,0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x37,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x01,0x01,0x01,0x01,0x00,0x00,0x01,0x01
	.byte 0x00,0x00,0x00,0x37,0x01,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x37,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x01,0x37,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x01,0x37,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x01,0x01,0x01,0x01,0x00,0x00,0x01,0x01
	.byte 0x00,0x00,0x00,0x00,0x01,0x00,0x01,0x37,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x37,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x01,0x01
	.byte 0x01,0x01,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x37,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x01,0x01,0x01,0x01,0x00,0x00,0x01,0x01
	.byte 0x00,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x00,0x37,0x00,0x00,0x00,0x00
	.byte 0x01,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x01,0x01,0x01,0x01,0x00,0x00,0x01,0x01
	.byte 0x00,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x37,0x00,0x00,0x00
dat_0A41A:
	.byte 0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02
	.byte 0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02
	.byte 0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02
	.byte 0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02
	.byte 0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02
	.byte 0x02,0x02,0x02,0x02,0x02,0x02,0x12,0x12,0x12,0x12,0x12,0x12,0x12,0x12,0x12,0x12
	.byte 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
	.byte 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
	.byte 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
	.byte 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
	.byte 0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
	.byte 0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
sub_0A51A:
	move.l a6,(0xFF569E).l
	move.w (a6)+,(0xFF385E).l
	move.w (a6)+,(0xFF390A).l
	move.w (a6),(0xFF56B0).l
	move.w #0x1,(0xFF56A2).l
	move.l a5,(0xFF6C12).l
	rts
sub_0A542:
	move.l a6,(0xFF5748).l
	move.w (a6)+,(0xFF3860).l
	move.w (a6)+,(0xFF390C).l
	move.w (a6),(0xFF575A).l
	move.w #0x1,(0xFF574C).l
	move.l a5,(0xFF6C16).l
	rts
sub_0A56A:
	move.w (0xFF385E).l,d7
	tst.w (0xFFA3AE).l
	beq.w loc_0A58A
	move.w (0xFFA3B2).l,d0
	cmp.b (0xFF3A62).l,d0
	bne.w loc_0A642
loc_0A58A:
	move.w d7,d6
	bpl.w loc_0A5E0
	lea (0xFF56F2).l,a6
	moveq #0x0,d6
	jsr (sub_0B442).l
	movem.l d7,-(a7)
	jsr (sub_1016A).l
	jsr (sub_1019E).l
	movem.l (a7)+,d7
	bne.w loc_0A642
	lea (0xFF5706).l,a6
	moveq #0x0,d6
	jsr (sub_0B442).l
	movem.l d7,-(a7)
	jsr (sub_1016A).l
	jsr (sub_1019E).l
	movem.l (a7)+,d7
	bne.w loc_0A642
	bra.w loc_0A62C
loc_0A5E0:
	lea (0xFF56FA).l,a6
	moveq #0x0,d6
	jsr (sub_0B442).l
	movem.l d7,-(a7)
	jsr (sub_1016A).l
	jsr (sub_1019E).l
	movem.l (a7)+,d7
	bne.w loc_0A642
	lea (0xFF570E).l,a6
	moveq #0x0,d6
	jsr (sub_0B442).l
	movem.l d7,-(a7)
	jsr (sub_1016A).l
	jsr (sub_1019E).l
	movem.l (a7)+,d7
	bne.w loc_0A642
loc_0A62C:
	tst.w (0xFF56F0).l
	bne.w loc_0A642
	add.w (0xFF304E).l,d7
	move.w d7,(0xFF304E).l
loc_0A642:
	move.w (0xFF390A).l,d6
	move.w d6,d7
	bpl.w loc_0A69E
	lea (0xFF56F2).l,a6
	moveq #0x0,d7
	jsr (sub_0B442).l
	movem.l d6,-(a7)
	jsr (sub_1016A).l
	jsr (sub_1019E).l
	movem.l (a7)+,d6
	bne.w loc_0A6F6
	lea (0xFF56FA).l,a6
	moveq #0x0,d7
	jsr (sub_0B442).l
	movem.l d6,-(a7)
	jsr (sub_1016A).l
	jsr (sub_1019E).l
	movem.l (a7)+,d6
	bne.w loc_0A6F6
	bra.w loc_0A6EA
loc_0A69E:
	lea (0xFF5706).l,a6
	moveq #0x0,d7
	jsr (sub_0B442).l
	movem.l d6,-(a7)
	jsr (sub_1016A).l
	jsr (sub_1019E).l
	movem.l (a7)+,d6
	bne.w loc_0A6F6
	lea (0xFF570E).l,a6
	moveq #0x0,d7
	jsr (sub_0B442).l
	movem.l d6,-(a7)
	jsr (sub_1016A).l
	jsr (sub_1019E).l
	movem.l (a7)+,d6
	bne.w loc_0A6F6
loc_0A6EA:
	add.w (0xFF30FA).l,d6
	move.w d6,(0xFF30FA).l
loc_0A6F6:
	subq.w #0x1,(0xFF56B0).l
	bne.w loc_0A728
	move.w (0xFF56A2).l,d7
	lsl.w #0x3,d7
	movea.l (0xFF569E).l,a6
	adda.w d7,a6
	move.w (a6)+,(0xFF385E).l
	move.w (a6)+,(0xFF390A).l
	move.w (a6),(0xFF56B0).l
	addq.w #0x1,(0xFF56A2).l
loc_0A728:
	rts
sub_0A72A:
	move.w (0xFF3860).l,d7
	tst.w (0xFFA3B0).l
	beq.w loc_0A74A
	move.w (0xFFA3B4).l,d0
	cmp.b (0xFF3A64).l,d0
	bne.w loc_0A802
loc_0A74A:
	move.w d7,d6
	bpl.w loc_0A7A0
	lea (0xFF579C).l,a6
	moveq #0x0,d6
	jsr (sub_0B45A).l
	movem.l d7,-(a7)
	jsr (sub_1016A).l
	jsr (sub_101C8).l
	movem.l (a7)+,d7
	bne.w loc_0A802
	lea (0xFF57B0).l,a6
	moveq #0x0,d6
	jsr (sub_0B45A).l
	movem.l d7,-(a7)
	jsr (sub_1016A).l
	jsr (sub_101C8).l
	movem.l (a7)+,d7
	bne.w loc_0A802
	bra.w loc_0A7EC
loc_0A7A0:
	lea (0xFF57A4).l,a6
	moveq #0x0,d6
	jsr (sub_0B45A).l
	movem.l d7,-(a7)
	jsr (sub_1016A).l
	jsr (sub_101C8).l
	movem.l (a7)+,d7
	bne.w loc_0A802
	lea (0xFF57B8).l,a6
	moveq #0x0,d6
	jsr (sub_0B45A).l
	movem.l d7,-(a7)
	jsr (sub_1016A).l
	jsr (sub_101C8).l
	movem.l (a7)+,d7
	bne.w loc_0A802
loc_0A7EC:
	tst.w (0xFF579A).l
	bne.w loc_0A802
	add.w (0xFF3050).l,d7
	move.w d7,(0xFF3050).l
loc_0A802:
	move.w (0xFF390C).l,d6
	move.w d6,d7
	bpl.w loc_0A85E
	lea (0xFF579C).l,a6
	moveq #0x0,d7
	jsr (sub_0B45A).l
	movem.l d6,-(a7)
	jsr (sub_1016A).l
	jsr (sub_101C8).l
	movem.l (a7)+,d6
	bne.w loc_0A8B6
	lea (0xFF57A4).l,a6
	moveq #0x0,d7
	jsr (sub_0B45A).l
	movem.l d6,-(a7)
	jsr (sub_1016A).l
	jsr (sub_101C8).l
	movem.l (a7)+,d6
	bne.w loc_0A8B6
	bra.w loc_0A8AA
loc_0A85E:
	lea (0xFF57B0).l,a6
	moveq #0x0,d7
	jsr (sub_0B45A).l
	movem.l d6,-(a7)
	jsr (sub_1016A).l
	jsr (sub_101C8).l
	movem.l (a7)+,d6
	bne.w loc_0A8B6
	lea (0xFF57B8).l,a6
	moveq #0x0,d7
	jsr (sub_0B45A).l
	movem.l d6,-(a7)
	jsr (sub_1016A).l
	jsr (sub_101C8).l
	movem.l (a7)+,d6
	bne.w loc_0A8B6
loc_0A8AA:
	add.w (0xFF30FC).l,d6
	move.w d6,(0xFF30FC).l
loc_0A8B6:
	subq.w #0x1,(0xFF575A).l
	bne.w loc_0A8E8
	move.w (0xFF574C).l,d7
	lsl.w #0x3,d7
	movea.l (0xFF5748).l,a6
	adda.w d7,a6
	move.w (a6)+,(0xFF3860).l
	move.w (a6)+,(0xFF390C).l
	move.w (a6),(0xFF575A).l
	addq.w #0x1,(0xFF574C).l
loc_0A8E8:
	rts
sub_0A8EA:
	move.w (0xFF385E).l,d7
	add.w (0xFF304E).l,d7
	move.w d7,(0xFF304E).l
	move.w (0xFF390A).l,d7
	add.w (0xFF30FA).l,d7
	move.w d7,(0xFF30FA).l
	subq.w #0x1,(0xFF56B0).l
	bne.w loc_0A940
	move.w (0xFF56A2).l,d7
	lsl.w #0x3,d7
	movea.l (0xFF569E).l,a6
	adda.w d7,a6
	move.w (a6)+,(0xFF385E).l
	move.w (a6)+,(0xFF390A).l
	move.w (a6),(0xFF56B0).l
	addq.w #0x1,(0xFF56A2).l
loc_0A940:
	rts
sub_0A942:
	move.w (0xFF3860).l,d7
	add.w (0xFF3050).l,d7
	move.w d7,(0xFF3050).l
	move.w (0xFF390C).l,d7
	add.w (0xFF30FC).l,d7
	move.w d7,(0xFF30FC).l
	subq.w #0x1,(0xFF575A).l
	bne.w loc_0A998
	move.w (0xFF574C).l,d7
	lsl.w #0x3,d7
	movea.l (0xFF5748).l,a6
	adda.w d7,a6
	move.w (a6)+,(0xFF3860).l
	move.w (a6)+,(0xFF390C).l
	move.w (a6),(0xFF575A).l
	addq.w #0x1,(0xFF574C).l
loc_0A998:
	rts
sub_0A99A:
	move.l a6,(0xFF5694).l
	move.w #0x4,(0xFF5698).l
	move.w (a6)+,(0xFF569A).l
	move.w (a6),(0xFF569C).l
	move.w #0x1,(0xFF5682).l
	move.l #sub_0AA02,(0xFF5684).l
	jsr (sub_099D6).l
	rts
sub_0A9CE:
	move.l a6,(0xFF573E).l
	move.w #0x4,(0xFF5742).l
	move.w (a6)+,(0xFF5744).l
	move.w (a6),(0xFF5746).l
	move.w #0x1,(0xFF572C).l
	move.l #sub_0AA72,(0xFF572E).l
	jsr (sub_09A1C).l
	rts
sub_0AA02:
	move.w (0xFF569A).l,(0xFF17C0).l
	subq.w #0x1,(0xFF569C).l
	bne.w loc_0AA4A
	move.w (0xFF5698).l,d7
	movea.l (0xFF5694).l,a6
	adda.w d7,a6
	move.w (a6)+,d7
	cmpi.w #0xFFFF,d7
	bne.w loc_0AA38
	clr.w (0xFF5682).l
	bra.w loc_0AA4A
loc_0AA38:
	move.w d7,(0xFF569A).l
	move.w (a6)+,(0xFF569C).l
	addq.w #0x4,(0xFF5698).l
loc_0AA4A:
	tst.w (0xFF56E4).l
	beq.w loc_090B2
	move.w (0xFF569C).l,d7
	bpl.w loc_090B2
	move.w #0x8017,d7
	jsr (sub_09846).l
	clr.w (0xFF56E4).l
	bra.w loc_090B2
sub_0AA72:
	move.w (0xFF5744).l,(0xFF17C2).l
	subq.w #0x1,(0xFF5746).l
	bne.w loc_0AABA
	move.w (0xFF5742).l,d7
	movea.l (0xFF573E).l,a6
	adda.w d7,a6
	move.w (a6)+,d7
	cmpi.w #0xFFFF,d7
	bne.w loc_0AAA8
	clr.w (0xFF572C).l
	bra.w loc_0AABA
loc_0AAA8:
	move.w d7,(0xFF5744).l
	move.w (a6)+,(0xFF5746).l
	addq.w #0x4,(0xFF5742).l
loc_0AABA:
	tst.w (0xFF578E).l
	beq.w loc_09234
	move.w (0xFF5746).l,d7
	bpl.w loc_09234
	move.w #0x8017,d7
	jsr (sub_09894).l
	clr.w (0xFF578E).l
	bra.w loc_09234
sub_0AAE2:
	moveq #0x0,d7
	lea (0xFF31A6).l,a6
	move.w (a6,d0.w),d6
	cmp.w (a6),d6
	bne.w loc_0AB22
	lea (0xFF9E9E).l,a6
	tst.w (a6,d0.w)
	beq.w loc_0AB20
	move.w (0xFFA3B2).l,d6
	add.w d6,d6
	lea dat_0ABEE(pc),a6
	move.w (a6,d6.w),d6
	lea (0xFF33AA).l,a6
	cmp.w (a6,d0.w),d6
	bne.w loc_0AB22
loc_0AB20:
	moveq #0x1,d7
loc_0AB22:
	rts
sub_0AB24:
	moveq #0x0,d7
	lea (0xFF31A6).l,a6
	move.w (a6,d0.w),d6
	cmp.w 0x2(a6),d6
	bne.w loc_0AB66
	lea (0xFF9E9E).l,a6
	tst.w (a6,d0.w)
	beq.w loc_0AB64
	move.w (0xFFA3B4).l,d6
	add.w d6,d6
	lea dat_0ABEE(pc),a6
	move.w (a6,d6.w),d6
	lea (0xFF33AA).l,a6
	cmp.w (a6,d0.w),d6
	bne.w loc_0AB66
loc_0AB64:
	moveq #0x1,d7
loc_0AB66:
	rts
sub_0AB68:
	moveq #0x0,d7
	lea (0xFF31A6).l,a6
	move.w (a6,d0.w),d6
	cmp.w (a6),d6
	beq.w loc_0ABA8
	lea (0xFF9E9E).l,a6
	tst.w (a6,d0.w)
	beq.w loc_0ABA8
	move.w (0xFFA3B2).l,d6
	add.w d6,d6
	lea dat_0ABEE(pc),a6
	move.w (a6,d6.w),d6
	lea (0xFF33AA).l,a6
	cmp.w (a6,d0.w),d6
	bne.w loc_0ABA8
	moveq #0x1,d7
loc_0ABA8:
	rts
sub_0ABAA:
	moveq #0x0,d7
	lea (0xFF31A6).l,a6
	move.w (a6,d0.w),d6
	cmp.w 0x2(a6),d6
	beq.w loc_0ABEC
	lea (0xFF9E9E).l,a6
	tst.w (a6,d0.w)
	beq.w loc_0ABEC
	move.w (0xFFA3B4).l,d6
	add.w d6,d6
	lea dat_0ABEE(pc),a6
	move.w (a6,d6.w),d6
	lea (0xFF33AA).l,a6
	cmp.w (a6,d0.w),d6
	bne.w loc_0ABEC
	moveq #0x1,d7
loc_0ABEC:
	rts
dat_0ABEE:
	.byte 0x00,0x00,0x00,0x04,0x00,0x02,0x00,0x00
sub_0ABF6:
	jsr (sub_19A18).l
	move.w #0x4C,d0
	move.w #0x1F,d1
	moveq #0x1,d2
loc_0AC06:
	lea (0xFF2EF6).l,a6
	tst.w (a6,d0.w)
	beq.w loc_0ACDC
	lea (0xFF9E12).l,a6
	tst.w (a6,d0.w)
	bne.w loc_0ACDC
	lea (0xFF9E9E).l,a6
	cmpi.w #0x2,(a6,d0.w)
	bne.w loc_0ACDC
	lea (0xFF9FB6).l,a5
	move.w (a5,d0.w),d7
	bne.w loc_0ACDC
	move.w #0x3,(a6,d0.w)
	lea (0xFF304E).l,a6
	move.w (a6,d7.w),(a6,d0.w)
	lea (0xFF30FA).l,a6
	move.w (a6,d7.w),(a6,d0.w)
	moveq #0x0,d6
	lea (0xFF31A6).l,a6
	tst.w (a6,d7.w)
	beq.w loc_0AC72
	move.w #0x10,d6
loc_0AC72:
	move.w d6,(a6,d0.w)
	moveq #-0x2,d6
	cmpi.b #0x1,(0xFF3A62).l
	beq.w loc_0AC86
	moveq #0x2,d6
loc_0AC86:
	lea (0xFF385E).l,a6
	move.w d6,(a6,d0.w)
	lea (0xFF390A).l,a6
	move.w #0xFFFA,(a6,d0.w)
	lea (0xFFA15A).l,a6
	move.w d2,(a6,d0.w)
	tst.w (0xFFA3A8).l
	bne.w loc_0ACB2
	addq.w #0x4,d2
loc_0ACB2:
	lea (0xFFA1E6).l,a6
	move.w #0x1,(a6,d0.w)
	lea (0xFF492A).l,a6
	move.w #0x4,(a6,d0.w)
	lea (0xFF406E).l,a6
	move.w #0x1,(a6,d0.w)
	jsr (sub_0ADD0).l
loc_0ACDC:
	addq.w #0x2,d0
	dbra d1,loc_0AC06
	rts
sub_0ACE4:
	jsr (sub_19A18).l
	move.w #0x4C,d0
	move.w #0x1F,d1
	moveq #0x1,d2
loc_0ACF4:
	lea (0xFF2EF6).l,a6
	tst.w (a6,d0.w)
loc_0ACFE:
	beq.w loc_0ADC8
	lea (0xFF9E12).l,a6
	tst.w (a6,d0.w)
	bne.w loc_0ADC8
	lea (0xFF9E9E).l,a6
	cmpi.w #0x2,(a6,d0.w)
	bne.w loc_0ADC8
	lea (0xFF9FB6).l,a5
	move.w (a5,d0.w),d7
	beq.w loc_0ADC8
	move.w #0x3,(a6,d0.w)
	lea (0xFF304E).l,a6
	move.w (a6,d7.w),(a6,d0.w)
	lea (0xFF30FA).l,a6
	move.w (a6,d7.w),(a6,d0.w)
	moveq #0x0,d6
	lea (0xFF31A6).l,a6
	tst.w (a6,d7.w)
	beq.w loc_0AD60
	move.w #0x10,d6
loc_0AD60:
	move.w d6,(a6,d0.w)
	moveq #-0x2,d6
	cmpi.b #0x1,(0xFF3A64).l
	beq.w loc_0AD74
	moveq #0x2,d6
loc_0AD74:
	lea (0xFF385E).l,a6
	move.w d6,(a6,d0.w)
	lea (0xFF390A).l,a6
	move.w #0xFFFA,(a6,d0.w)
	lea (0xFFA15A).l,a6
	move.w d2,(a6,d0.w)
	tst.w (0xFFA3AA).l
	bne.w loc_0ADA0
	addq.w #0x4,d2
loc_0ADA0:
	lea (0xFFA1E6).l,a6
	move.w #0x1,(a6,d0.w)
	lea (0xFF492A).l,a6
	move.w #0x4,(a6,d0.w)
	lea (0xFF406E).l,a6
	move.w #0x1,(a6,d0.w)
	bsr.w sub_0ADD0
loc_0ADC8:
	addq.w #0x2,d0
	dbra d1,loc_0ACF4
	rts
sub_0ADD0:
	lea (0xFF3252).l,a5
	lea (0xFF5292).l,a6
	move.w (a6,d0.w),(a5,d0.w)
	lea (0xFF49D6).l,a5
	lea (0xFF4A82).l,a6
	move.w (a6,d0.w),(a5,d0.w)
	subq.w #0x1,(0xFF9E0E).l
	clr.w (0xFF1818).l
	rts
sub_0AE02:
	movem.l d0-d1,-(a7)
	move.w #0x4C,d0
	move.w #0x1F,d1
loc_0AE0E:
	lea (0xFF9E12).l,a6
	move.w (a6,d0.w),d7
	bne.w loc_0AE72
	lea (0xFF508E).l,a5
	lea (0xFF304E).l,a6
	move.w (a5,d0.w),(a6,d0.w)
	lea (0xFF513A).l,a5
	lea (0xFF30FA).l,a6
	move.w (a5,d0.w),(a6,d0.w)
	lea (0xFF51E6).l,a5
	lea (0xFF31A6).l,a6
	move.w (a5,d0.w),(a6,d0.w)
	lea (0xFFA5CC).l,a6
	clr.w (a6,d0.w)
	jsr (sub_0ADD0).l
	clr.w (0xFF9E0E).l
	lea (0xFF9E9E).l,a6
	clr.w (a6,d0.w)
loc_0AE72:
	addq.w #0x2,d0
	dbra d1,loc_0AE0E
	jsr (sub_0AE8A).l
	jsr (sub_0AEA6).l
	movem.l (a7)+,d0-d1
	rts
sub_0AE8A:
	move.w (0xFF9DFE).l,d7
	beq.w loc_0AEA4
	clr.w (0xFF9DFE).l
	lea (0xFF2EF6).l,a6
	clr.w (a6,d7.w)
loc_0AEA4:
	rts
sub_0AEA6:
	move.w (0xFF9E00).l,d7
	beq.w loc_0AEC0
	clr.w (0xFF9E00).l
	lea (0xFF2EF6).l,a6
	clr.w (a6,d7.w)
loc_0AEC0:
	rts
sub_0AEC2:
	tst.w (0xFF6C4E).l
	bne.w loc_0AED4
	move.w #0x1,(0xFF180E).l
loc_0AED4:
	jsr (sub_099D6).l
	move.w #0xB4,d7
	jsr (sub_09EDA).l
	move.w #0x1,(0xFF56E2).l
	clr.b (0xFF3A63).l
	jsr (sub_0B23A).l
	rts
sub_0AEFA:
	tst.w (0xFF6C4E).l
	bne.w loc_0AF0C
	move.w #0x1,(0xFF1810).l
loc_0AF0C:
	jsr (sub_09A1C).l
	move.w #0xB4,d7
	jsr (sub_09EF4).l
	move.w #0x1,(0xFF578C).l
	clr.b (0xFF3A65).l
	jsr (sub_0B2BC).l
	rts
sub_0AF32:
	moveq #0x0,d6
	move.b (0xFF17C0).l,d6
	andi.b #0xF,d6
	beq.w loc_0AF56
	moveq #0x1,d7
	andi.b #0xC,d6
	bne.w loc_0AF50
	move.w #0x8019,d7
loc_0AF50:
	jsr (sub_09846).l
loc_0AF56:
	rts
sub_0AF58:
	moveq #0x0,d6
	move.b (0xFF17C2).l,d6
	andi.b #0xF,d6
	beq.w loc_0AF7C
	moveq #0x1,d7
	andi.b #0xC,d6
	bne.w loc_0AF76
	move.w #0x8019,d7
loc_0AF76:
	jsr (sub_09894).l
loc_0AF7C:
	rts
sub_0AF7E:
	lea (0xFF2EF6).l,a0
	lea (0xFF2FA2).l,a1
	lea (0xFF931C).l,a2
	move.w #0xC,d0
	move.w #0x1F,d1
	move.w d7,d2
	lsr.w #0x1,d2
loc_0AF9C:
	tst.w (a0,d0.w)
	beq.w loc_0AFE0
	move.w (a1,d0.w),d3
	and.w d2,d3
	beq.w loc_0AFE0
	tst.w (a2,d0.w)
	bne.w loc_0AFE0
	lea (0xFF31A6).l,a6
	tst.w (a6,d0.w)
	beq.w loc_0AFCE
	cmpi.w #0x10,(a6,d0.w)
	bne.w loc_0AFE0
loc_0AFCE:
	movem.l d2/d7,-(a7)
	move.w d7,d3
	move.w d0,d2
	jsr (sub_17A44).l
	movem.l (a7)+,d2/d7
loc_0AFE0:
	addq.w #0x2,d0
	dbra d1,loc_0AF9C
	rts
sub_0AFE8:
	tst.w (0xFF56B2).l
	beq.w loc_0AFF8
loc_0AFF2:
	moveq #0x1,d0
	bra.w loc_0B04C
loc_0AFF8:
	tst.w (0xFFA3AE).l
	beq.w loc_0B020
	tst.w (0xFF31A6).l
	bne.w loc_0B020
	move.w (0xFF3D12).l,d0
	beq.w loc_0B020
	jsr (sub_0AB68).l
	tst.w d7
	bne.s loc_0AFF2
loc_0B020:
	jsr (sub_0D7E4).l
	bne.s loc_0AFF2
	move.b (0xFF17C0).l,d7
	tst.w (0xFF31A6).l
	bne.w loc_0B044
	btst #0x1,d7
	bne.s loc_0AFF2
	moveq #0x0,d0
	bra.w loc_0B04C
loc_0B044:
	btst #0x0,d7
	bne.s loc_0AFF2
	moveq #0x0,d0
loc_0B04C:
	tst.w d0
	rts
sub_0B050:
	tst.w (0xFF575C).l
	beq.w loc_0B060
loc_0B05A:
	moveq #0x1,d0
	bra.w loc_0B0B4
loc_0B060:
	tst.w (0xFFA3B0).l
	beq.w loc_0B088
	tst.w (0xFF31A8).l
	bne.w loc_0B088
	move.w (0xFF3D14).l,d0
	beq.w loc_0B088
	jsr (sub_0ABAA).l
	tst.w d7
	bne.s loc_0B05A
loc_0B088:
	jsr (sub_0FBFE).l
	bne.s loc_0B05A
	move.b (0xFF17C2).l,d7
	tst.w (0xFF31A8).l
	bne.w loc_0B0AC
	btst #0x1,d7
	bne.s loc_0B05A
	moveq #0x0,d0
	bra.w loc_0B0B4
loc_0B0AC:
	btst #0x0,d7
	bne.s loc_0B05A
	moveq #0x0,d0
loc_0B0B4:
	tst.w d0
	rts
sub_0B0B8:
	tst.w (0xFFA3AE).l
	beq.w loc_0B10E
	move.w (0xFFA3B2).l,d0
	cmp.b (0xFF3A62).l,d0
	bne.w loc_0B10E
	move.w (0xFF3D12).l,d0
	bne.w loc_0B0E2
loc_0B0DC:
	moveq #0x1,d0
	bra.w loc_0B172
loc_0B0E2:
	move.w (0xFF31A6).l,d6
	cmpi.w #0x10,d6
	beq.w loc_0B0F6
	clr.w (0xFF31A6).l
loc_0B0F6:
	movem.l d6,-(a7)
	jsr (sub_0AAE2).l
	movem.l (a7)+,d6
	move.w d6,(0xFF31A6).l
	tst.w d7
	bne.s loc_0B0DC
loc_0B10E:
	moveq #0x0,d7
	move.b (0xFF17C0).l,d7
	andi.b #0xC,d7
	lsr.b #0x2,d7
	subq.b #0x1,d7
	lsl.w #0x3,d7
	lea dat_0B870(pc),a6
	adda.w d7,a6
	move.w (0xFF304E).l,d0
	add.w (a6)+,d0
	move.w (0xFF30FA).l,d1
	move.w (a6)+,d3
	add.w d3,d1
	movem.l a6,-(a7)
	jsr (sub_1016A).l
	jsr (sub_102A6).l
	movem.l (a7)+,a6
	bne.s loc_0B0DC
	move.w (0xFF304E).l,d0
	add.w (a6)+,d0
	move.w (0xFF30FA).l,d1
	move.w (a6),d3
	add.w d3,d1
	jsr (sub_1016A).l
	jsr (sub_102A6).l
	bne.w loc_0B0DC
	moveq #0x0,d0
loc_0B172:
	tst.w d0
	rts
	.byte 0x00,0x00,0xFF,0xFE,0x00,0x02
sub_0B17C:
	tst.w (0xFFA3B0).l
	beq.w loc_0B1D2
	move.w (0xFFA3B4).l,d0
	cmp.b (0xFF3A64).l,d0
	bne.w loc_0B1D2
	move.w (0xFF3D14).l,d0
	bne.w loc_0B1A6
loc_0B1A0:
	moveq #0x1,d0
	bra.w loc_0B236
loc_0B1A6:
	move.w (0xFF31A8).l,d6
	cmpi.w #0x10,d6
	beq.w loc_0B1BA
	clr.w (0xFF31A8).l
loc_0B1BA:
	movem.l d6,-(a7)
	jsr (sub_0AB24).l
	movem.l (a7)+,d6
	move.w d6,(0xFF31A8).l
	tst.w d7
	bne.s loc_0B1A0
loc_0B1D2:
	moveq #0x0,d7
	move.b (0xFF17C2).l,d7
	andi.b #0xC,d7
	lsr.b #0x2,d7
	subq.b #0x1,d7
	lsl.w #0x3,d7
	lea dat_0DD64(pc),a6
	adda.w d7,a6
	move.w (0xFF3050).l,d0
	add.w (a6)+,d0
	move.w (0xFF30FC).l,d1
	move.w (a6)+,d3
	add.w d3,d1
	movem.l a6,-(a7)
	jsr (sub_1016A).l
	jsr (sub_102E4).l
	movem.l (a7)+,a6
	bne.s loc_0B1A0
	move.w (0xFF3050).l,d0
	add.w (a6)+,d0
	move.w (0xFF30FC).l,d1
	move.w (a6),d3
	add.w d3,d1
	jsr (sub_1016A).l
	jsr (sub_102E4).l
	bne.w loc_0B1A0
	moveq #0x0,d0
loc_0B236:
	tst.w d0
	rts
