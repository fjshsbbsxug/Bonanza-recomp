| Mega Drive hardware definitions

	.set VDP_DATA,      0xC00000
	.set VDP_CTRL,      0xC00004
	.set VDP_HVCOUNTER, 0xC00008
	.set PSG_PORT,      0xC00011

	.set IO_VERSION,    0xA10001
	.set IO_DATA1,      0xA10003
	.set IO_DATA2,      0xA10005
	.set IO_DATA3,      0xA10007
	.set IO_CTRL1,      0xA10009
	.set IO_CTRL2,      0xA1000B
	.set IO_CTRL3,      0xA1000D

	.set Z80_RAM,       0xA00000
	.set Z80_BUSREQ,    0xA11100
	.set Z80_RESET,     0xA11200
	.set TMSS_REG,      0xA14000
