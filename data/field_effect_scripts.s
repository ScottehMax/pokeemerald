	.include "asm/macros.inc"
	.include "constants/constants.inc"

	.section script_data, "aw", %progbits

	asset_ptr_align
gFieldEffectScriptPointers::
	asset_ptr gFieldEffectScript_ExclamationMarkIcon1      @ FLDEFF_EXCLAMATION_MARK_ICON
	asset_ptr gFieldEffectScript_UseCutOnTallGrass         @ FLDEFF_USE_CUT_ON_GRASS
	asset_ptr gFieldEffectScript_UseCutOnTree              @ FLDEFF_USE_CUT_ON_TREE
	asset_ptr gFieldEffectScript_Shadow                    @ FLDEFF_SHADOW
	asset_ptr gFieldEffectScript_TallGrass                 @ FLDEFF_TALL_GRASS
	asset_ptr gFieldEffectScript_Ripple                    @ FLDEFF_RIPPLE
	asset_ptr gFieldEffectScript_FieldMoveShowMon          @ FLDEFF_FIELD_MOVE_SHOW_MON
	asset_ptr gFieldEffectScript_Ash                       @ FLDEFF_ASH
	asset_ptr gFieldEffectScript_SurfBlob                  @ FLDEFF_SURF_BLOB
	asset_ptr gFieldEffectScript_UseSurf                   @ FLDEFF_USE_SURF
	asset_ptr gFieldEffectScript_GroundImpactDust          @ FLDEFF_DUST
	asset_ptr gFieldEffectScript_UseSecretPowerCave        @ FLDEFF_USE_SECRET_POWER_CAVE
	asset_ptr gFieldEffectScript_JumpTallGrass             @ FLDEFF_JUMP_TALL_GRASS
	asset_ptr gFieldEffectScript_SandFootprints            @ FLDEFF_SAND_FOOTPRINTS
	asset_ptr gFieldEffectScript_JumpBigSplash             @ FLDEFF_JUMP_BIG_SPLASH
	asset_ptr gFieldEffectScript_Splash                    @ FLDEFF_SPLASH
	asset_ptr gFieldEffectScript_JumpSmallSplash           @ FLDEFF_JUMP_SMALL_SPLASH
	asset_ptr gFieldEffectScript_LongGrass                 @ FLDEFF_LONG_GRASS
	asset_ptr gFieldEffectScript_JumpLongGrass             @ FLDEFF_JUMP_LONG_GRASS
	asset_ptr gFieldEffectScript_UnusedGrass               @ FLDEFF_UNUSED_GRASS
	asset_ptr gFieldEffectScript_UnusedGrass2              @ FLDEFF_UNUSED_GRASS_2
	asset_ptr gFieldEffectScript_UnusedSand                @ FLDEFF_UNUSED_SAND
	asset_ptr gFieldEffectScript_WaterSurfacing            @ FLDEFF_WATER_SURFACING
	asset_ptr gFieldEffectScript_BerryTreeGrowthSparkle    @ FLDEFF_BERRY_TREE_GROWTH_SPARKLE
	asset_ptr gFieldEffectScript_DeepSandFootprints        @ FLDEFF_DEEP_SAND_FOOTPRINTS
	asset_ptr gFieldEffectScript_PokeCenterHeal            @ FLDEFF_POKECENTER_HEAL
	asset_ptr gFieldEffectScript_UseSecretPowerTree        @ FLDEFF_USE_SECRET_POWER_TREE
	asset_ptr gFieldEffectScript_UseSecretPowerShrub       @ FLDEFF_USE_SECRET_POWER_SHRUB
	asset_ptr gFieldEffectScript_TreeDisguise              @ FLDEFF_TREE_DISGUISE
	asset_ptr gFieldEffectScript_MountainDisguise          @ FLDEFF_MOUNTAIN_DISGUISE
	asset_ptr gFieldEffectScript_NPCUseFly                 @ FLDEFF_NPCFLY_OUT
	asset_ptr gFieldEffectScript_UseFly                    @ FLDEFF_USE_FLY
	asset_ptr gFieldEffectScript_FlyIn                     @ FLDEFF_FLY_IN
	asset_ptr gFieldEffectScript_QuestionMarkIcon          @ FLDEFF_QUESTION_MARK_ICON
	asset_ptr gFieldEffectScript_FeetInFlowingWater        @ FLDEFF_FEET_IN_FLOWING_WATER
	asset_ptr gFieldEffectScript_BikeTireTracks            @ FLDEFF_BIKE_TIRE_TRACKS
	asset_ptr gFieldEffectScript_SandDisguisePlaceholder   @ FLDEFF_SAND_DISGUISE
	asset_ptr gFieldEffectScript_UseRockSmash              @ FLDEFF_USE_ROCK_SMASH
	asset_ptr gFieldEffectScript_UseDig                    @ FLDEFF_USE_DIG
	asset_ptr gFieldEffectScript_SandPile                  @ FLDEFF_SAND_PILE
	asset_ptr gFieldEffectScript_UseStrength               @ FLDEFF_USE_STRENGTH
	asset_ptr gFieldEffectScript_ShortGrass                @ FLDEFF_SHORT_GRASS
	asset_ptr gFieldEffectScript_HotSpringsWater           @ FLDEFF_HOT_SPRINGS_WATER
	asset_ptr gFieldEffectScript_UseWaterfall              @ FLDEFF_USE_WATERFALL
	asset_ptr gFieldEffectScript_UseDive                   @ FLDEFF_USE_DIVE
	asset_ptr gFieldEffectScript_PokeballTrail             @ FLDEFF_POKEBALL_TRAIL
	asset_ptr gFieldEffectScript_HeartIcon                 @ FLDEFF_HEART_ICON
	asset_ptr gFieldEffectScript_Nop47                     @ FLDEFF_NOP_47
	asset_ptr gFieldEffectScript_Nop48                     @ FLDEFF_NOP_48
	asset_ptr gFieldEffectScript_AshPuff                   @ FLDEFF_ASH_PUFF
	asset_ptr gFieldEffectScript_AshLaunch                 @ FLDEFF_ASH_LAUNCH
	asset_ptr gFieldEffectScript_SweetScent                @ FLDEFF_SWEET_SCENT
	asset_ptr gFieldEffectScript_SandPillar                @ FLDEFF_SAND_PILLAR
	asset_ptr gFieldEffectScript_Bubbles                   @ FLDEFF_BUBBLES
	asset_ptr gFieldEffectScript_Sparkle                   @ FLDEFF_SPARKLE
	asset_ptr gFieldEffectScript_ShowSecretPowerCave       @ FLDEFF_SECRET_POWER_CAVE
	asset_ptr gFieldEffectScript_ShowSecretPowerTree       @ FLDEFF_SECRET_POWER_TREE
	asset_ptr gFieldEffectScript_ShowSecretPowerShrub      @ FLDEFF_SECRET_POWER_SHRUB
	asset_ptr gFieldEffectScript_ShowCutGrass              @ FLDEFF_CUT_GRASS
	asset_ptr gFieldEffectScript_FieldMoveShowMonInit      @ FLDEFF_FIELD_MOVE_SHOW_MON_INIT
	asset_ptr gFieldEffectScript_UsePuzzleEffect           @ FLDEFF_USE_TOMB_PUZZLE_EFFECT
	asset_ptr gFieldEffectScript_SecretBaseBootPC          @ FLDEFF_PCTURN_ON
	asset_ptr gFieldEffectScript_HallOfFameRecord          @ FLDEFF_HALL_OF_FAME_RECORD
	asset_ptr gFieldEffectScript_UseTeleport               @ FLDEFF_USE_TELEPORT
	asset_ptr gFieldEffectScript_RayquazaSpotlight         @ FLDEFF_RAYQUAZA_SPOTLIGHT
	asset_ptr gFieldEffectScript_DestroyDeoxysRock         @ FLDEFF_DESTROY_DEOXYS_ROCK
	asset_ptr gFieldEffectScript_MoveDeoxysRock            @ FLDEFF_MOVE_DEOXYS_ROCK

gFieldEffectScript_ExclamationMarkIcon1::
	field_eff_callnative FldEff_ExclamationMarkIcon
	field_eff_end

gFieldEffectScript_UseCutOnTallGrass::
	field_eff_callnative FldEff_UseCutOnGrass
	field_eff_end

gFieldEffectScript_UseCutOnTree::
	field_eff_callnative FldEff_UseCutOnTree
	field_eff_end

gFieldEffectScript_Shadow::
	field_eff_callnative FldEff_Shadow
	field_eff_end

gFieldEffectScript_TallGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_TallGrass
	field_eff_end

gFieldEffectScript_Ripple::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_Ripple
	field_eff_end

gFieldEffectScript_FieldMoveShowMon::
	field_eff_callnative FldEff_FieldMoveShowMon
	field_eff_end

gFieldEffectScript_Ash::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_Ash
	field_eff_end

gFieldEffectScript_SurfBlob::
	field_eff_callnative FldEff_SurfBlob
	field_eff_end

gFieldEffectScript_UseSurf::
	field_eff_callnative FldEff_UseSurf
	field_eff_end

gFieldEffectScript_GroundImpactDust::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_Dust
	field_eff_end

gFieldEffectScript_UseSecretPowerCave::
	field_eff_callnative FldEff_UseSecretPowerCave
	field_eff_end

gFieldEffectScript_JumpTallGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_JumpTallGrass
	field_eff_end

gFieldEffectScript_SandFootprints::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_SandFootprints
	field_eff_end

gFieldEffectScript_JumpBigSplash::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_JumpBigSplash
	field_eff_end

gFieldEffectScript_Splash::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_Splash
	field_eff_end

gFieldEffectScript_JumpSmallSplash::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_JumpSmallSplash
	field_eff_end

gFieldEffectScript_LongGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_LongGrass
	field_eff_end

gFieldEffectScript_JumpLongGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_JumpLongGrass
	field_eff_end

gFieldEffectScript_UnusedGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_UnusedGrass
	field_eff_end

gFieldEffectScript_UnusedGrass2::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_UnusedGrass2
	field_eff_end

gFieldEffectScript_UnusedSand::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_UnusedSand
	field_eff_end

gFieldEffectScript_WaterSurfacing::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_WaterSurfacing
	field_eff_end

gFieldEffectScript_BerryTreeGrowthSparkle::
	field_eff_callnative FldEff_BerryTreeGrowthSparkle
	field_eff_end

gFieldEffectScript_DeepSandFootprints::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_DeepSandFootprints
	field_eff_end

gFieldEffectScript_PokeCenterHeal::
	field_eff_loadfadedpal gSpritePalette_PokeballGlow
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_PokecenterHeal
	field_eff_end

gFieldEffectScript_UseSecretPowerTree::
	field_eff_callnative FldEff_UseSecretPowerTree
	field_eff_end

gFieldEffectScript_UseSecretPowerShrub::
	field_eff_callnative FldEff_UseSecretPowerShrub
	field_eff_end

gFieldEffectScript_TreeDisguise::
	field_eff_callnative ShowTreeDisguiseFieldEffect
	field_eff_end

gFieldEffectScript_MountainDisguise::
	field_eff_callnative ShowMountainDisguiseFieldEffect
	field_eff_end

gFieldEffectScript_NPCUseFly::
	field_eff_callnative FldEff_NPCFlyOut
	field_eff_end

gFieldEffectScript_UseFly::
	field_eff_callnative FldEff_UseFly
	field_eff_end

gFieldEffectScript_FlyIn::
	field_eff_callnative FldEff_FlyIn
	field_eff_end

gFieldEffectScript_QuestionMarkIcon::
	field_eff_callnative FldEff_QuestionMarkIcon
	field_eff_end

gFieldEffectScript_FeetInFlowingWater::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_FeetInFlowingWater
	field_eff_end

gFieldEffectScript_BikeTireTracks::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_BikeTireTracks
	field_eff_end

gFieldEffectScript_SandDisguisePlaceholder::
	field_eff_callnative ShowSandDisguiseFieldEffect
	field_eff_end

gFieldEffectScript_UseRockSmash::
	field_eff_callnative FldEff_UseRockSmash
	field_eff_end

gFieldEffectScript_UseStrength::
	field_eff_callnative FldEff_UseStrength
	field_eff_end

gFieldEffectScript_UseDig::
	field_eff_callnative FldEff_UseDig
	field_eff_end

gFieldEffectScript_SandPile::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_SandPile
	field_eff_end

gFieldEffectScript_ShortGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_ShortGrass
	field_eff_end

gFieldEffectScript_HotSpringsWater::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect1, FldEff_HotSpringsWater
	field_eff_end

gFieldEffectScript_UseWaterfall::
	field_eff_callnative FldEff_UseWaterfall
	field_eff_end

gFieldEffectScript_UseDive::
	field_eff_callnative FldEff_UseDive
	field_eff_end

gFieldEffectScript_PokeballTrail::
	field_eff_loadpal gSpritePalette_Pokeball
	field_eff_callnative FldEff_PokeballTrail
	field_eff_end

gFieldEffectScript_HeartIcon::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_HeartIcon
	field_eff_end

gFieldEffectScript_Nop47::
	field_eff_callnative FldEff_Nop47
	field_eff_end

gFieldEffectScript_Nop48::
	field_eff_callnative FldEff_Nop48
	field_eff_end

gFieldEffectScript_AshPuff::
	field_eff_loadfadedpal_callnative gSpritePalette_Ash, FldEff_AshPuff
	field_eff_end

gFieldEffectScript_AshLaunch::
	field_eff_loadfadedpal_callnative gSpritePalette_Ash, FldEff_AshLaunch
	field_eff_end

gFieldEffectScript_SweetScent::
	field_eff_callnative FldEff_SweetScent
	field_eff_end

gFieldEffectScript_SandPillar::
	field_eff_loadfadedpal_callnative gSpritePalette_SandPillar, FldEff_SandPillar
	field_eff_end

gFieldEffectScript_Bubbles::
	field_eff_loadfadedpal_callnative gSpritePalette_GeneralFieldEffect0, FldEff_Bubbles
	field_eff_end

gFieldEffectScript_Sparkle::
	field_eff_loadfadedpal_callnative gSpritePalette_SmallSparkle, FldEff_Sparkle
	field_eff_end

gFieldEffectScript_ShowSecretPowerCave::
	field_eff_loadfadedpal_callnative gSpritePalette_SecretPower_Cave, FldEff_SecretPowerCave
	field_eff_end

gFieldEffectScript_ShowSecretPowerTree::
	field_eff_loadfadedpal_callnative gSpritePalette_SecretPower_Plant, FldEff_SecretPowerTree
	field_eff_end

gFieldEffectScript_ShowSecretPowerShrub::
	field_eff_loadfadedpal_callnative gSpritePalette_SecretPower_Plant, FldEff_SecretPowerShrub
	field_eff_end

gFieldEffectScript_ShowCutGrass::
	field_eff_loadfadedpal_callnative gSpritePalette_CutGrass, FldEff_CutGrass
	field_eff_end

gFieldEffectScript_FieldMoveShowMonInit::
	field_eff_callnative FldEff_FieldMoveShowMonInit
	field_eff_end

gFieldEffectScript_UsePuzzleEffect::
	field_eff_callnative FldEff_UsePuzzleEffect
	field_eff_end

gFieldEffectScript_SecretBaseBootPC::
	field_eff_callnative FldEff_SecretBasePCTurnOn
	field_eff_end

gFieldEffectScript_HallOfFameRecord::
	field_eff_loadfadedpal gSpritePalette_PokeballGlow
	field_eff_loadfadedpal_callnative gSpritePalette_HofMonitor, FldEff_HallOfFameRecord
	field_eff_end

gFieldEffectScript_UseTeleport::
	field_eff_callnative FldEff_UseTeleport
	field_eff_end

gFieldEffectScript_RayquazaSpotlight::
	field_eff_callnative FldEff_RayquazaSpotlight
	field_eff_end

gFieldEffectScript_DestroyDeoxysRock::
	field_eff_callnative FldEff_DestroyDeoxysRock
	field_eff_end

gFieldEffectScript_MoveDeoxysRock::
	field_eff_callnative FldEff_MoveDeoxysRock
	field_eff_end
