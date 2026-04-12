// Created by Crowbar 0.56

$ModelName "player/custom_player/legacy/ctm_fbi.mdl"

$BodyGroup "studio"
{
	studio "ctm_fbi.smd"
}
$BodyGroup "gloves"
{
	studio "ctm_fbi_w_ct_base_glove.smd"
	blank
}

$Include "ctm_fbi_lod.qci"

$SurfaceProp "flesh"

$Contents "solid"

$IllumPosition 0 0 35

$EyePosition 0 0 73

$Opaque

$CDMaterials "models\player\ctm_fbi\"
$CDMaterials "models\player\custom_player\econ\body\ctm_fbi\"
$CDMaterials "models\player\custom_player\econ\head\ctm_fbi\"
$CDMaterials "models\weapons\w_models\arms\"
$CDMaterials ""

$Attachment "legacy_weapon_bone" "ValveBiped.weapon_bone" 0 0 0 rotate 0 0 0
$Attachment "weapon_hand_L" "weapon_hand_L" 0 0 0 rotate 0 0 0
$Attachment "weapon_hand_R" "weapon_hand_R" 0 0 0 rotate 0 0 0
$Attachment "primary" "primary_jiggle_jnt" 0 2 -2.6 rotate 0 0 -22
$Attachment "pistol" "leg_upper_R" 5.9 -7.3 1.7 rotate 5 6 -8
$Attachment "knife" "leg_upper_L" 5.7 -0.7 4.7 rotate -2 149 92
$Attachment "eholster" "leg_upper_L" 0 0 5 rotate 0 0 90
$Attachment "grenade0" "spine_0" 1.3 -3 -10 rotate 89 0 -1
$Attachment "grenade1" "spine_0" 1.3 -0.7 -10.3 rotate 90 0 0
$Attachment "grenade2" "spine_0" 1.3 1.6 -10.1 rotate 90 0 0
$Attachment "grenade3" "spine_0" 1.3 5.1 -10 rotate 90 0 0
$Attachment "grenade4" "spine_0" 1.3 7.8 -7.9 rotate 90 0 0
$Attachment "defusekit" "spine_0" -1.6 -6 0 rotate 0 90 90
$Attachment "c4" "spine_3" -1 -8.5 -0.9 rotate -2 10 95
$Attachment "facemask" "head_0" 2 5.3 0 rotate 0 73 90
$Attachment "clip_limit" "head_0" 7 6 0 rotate 0 0 0
$Attachment "primary_smg" "primary_smg_jiggle_jnt" 0 0 0 rotate 0 0 0

// $Include "ctm_fbi_box.qci"

$Include "ctm_fbi_bone.qci"

$Include "ctm_fbi_animation.qci"

$Include "ctm_fbi_collision.qci"

$KeyValues
{
	snapshot_weights_all 
	{
	}
	snapshot_weights_upperbody 
	{
		"leg_upper_L" "0.5"
		"leg_lower_L" "0"
		"ankle_L" "0"
		"ball_L" "0"
		"leg_upper_R" "0.5"
		"leg_lower_R" "0"
		"ankle_R" "0"
		"ball_R" "0"
		"leg_upper_L_TWIST" "0.5"
		"leg_upper_L_TWIST1" "0.2"
		"leg_upper_R_TWIST" "0.5"
		"leg_upper_R_TWIST1" "0.2"
	}
	qc_path 
	{
		"value" "models\player\custom_player\legacy\qc\ctm_fbi.qc"
	}
}
