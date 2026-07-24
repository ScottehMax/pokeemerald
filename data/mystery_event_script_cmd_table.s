	.include "asm/macros.inc"

	.section script_data, "aw", %progbits

	asset_ptr_align
gMysteryEventScriptCmdTable::
	asset_ptr MEScrCmd_nop                 @ 0x00
	asset_ptr MEScrCmd_checkcompat         @ 0x01
	asset_ptr MEScrCmd_end                 @ 0x02
	asset_ptr MEScrCmd_setmsg              @ 0x03
	asset_ptr MEScrCmd_setstatus           @ 0x04
	asset_ptr MEScrCmd_runscript           @ 0x05
	asset_ptr MEScrCmd_initramscript       @ 0x06
	asset_ptr MEScrCmd_setenigmaberry      @ 0x07
	asset_ptr MEScrCmd_giveribbon          @ 0x08
	asset_ptr MEScrCmd_givenationaldex     @ 0x09
	asset_ptr MEScrCmd_addrareword         @ 0x0a
	asset_ptr MEScrCmd_setrecordmixinggift @ 0x0b
	asset_ptr MEScrCmd_givepokemon         @ 0x0c
	asset_ptr MEScrCmd_addtrainer          @ 0x0d
	asset_ptr MEScrCmd_enableresetrtc      @ 0x0e
	asset_ptr MEScrCmd_checksum            @ 0x0f
	asset_ptr MEScrCmd_crc                 @ 0x10
gMysteryEventScriptCmdTableEnd::
