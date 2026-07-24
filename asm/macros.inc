	.include "asm/macros/asm.inc"
	.include "asm/macros/function.inc"
	.include "asm/macros/movement.inc"
	.include "asm/macros/map.inc"
	.include "asm/macros/field_effect_script.inc"
	.include "asm/macros/trainer_hill.inc"
	.include "asm/macros/battle_tent.inc"
	.include "asm/macros/battle_frontier/apprentice.inc"
	.include "asm/macros/battle_frontier/battle_arena.inc"
	.include "asm/macros/battle_frontier/battle_dome.inc"
	.include "asm/macros/battle_frontier/battle_factory.inc"
	.include "asm/macros/battle_frontier/battle_palace.inc"
	.include "asm/macros/battle_frontier/battle_pike.inc"
	.include "asm/macros/battle_frontier/battle_pyramid.inc"
	.include "asm/macros/battle_frontier/battle_tower.inc"
	.include "asm/macros/battle_frontier/frontier_util.inc"

	@ Immutable asset references retain the GBA's four-byte layout. Android
	@ encodes them relative to their own address so shared-library relocation
	@ does not depend on loading below 4 GiB.
	.macro asset_ptr value:req
	.ifdef PLATFORM_RELATIVE_POINTERS
	.ifdef PLATFORM_AARCH64
	.reloc ., R_AARCH64_PREL32, \value
	.else
	.ifdef PLATFORM_X86_64
	.reloc ., R_X86_64_PC32, \value
	.else
	.ifdef PLATFORM_X86
	.reloc ., R_386_PC32, \value
	.else
	.ifdef PLATFORM_ARM
	.reloc ., R_ARM_REL32, \value
	.else
	.error "relative asset pointers are unsupported on this architecture"
	.endif
	.endif
	.endif
	.endif
	.4byte 0
	.else
	.4byte \value
	.endif
	.endm

	.macro asset_ptr_align
	.align 2
	.endm

	@ Alignment for mutable assembly storage whose C representation contains
	@ native pointers. Immutable asset records must use asset_ptr_align instead.
	.macro runtime_ptr_align
	.ifdef PLATFORM_64BIT
	.balign 8
	.else
	.align 2
	.endif
	.endm

	@ Android packages the core as a relocatable shared library. Keep script
	@ operands four bytes wide, but make their value relative to the operand so
	@ they do not require forbidden 32-bit absolute ELF relocations.
	.macro script_ptr value:req
	asset_ptr \value
	.endm
