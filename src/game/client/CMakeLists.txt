# =============================================================================
# Client (CSGO)
# Generated from: client_csgo.vcxproj
# =============================================================================

set(CLIENT_CSGO_SOURCES
    mp3player.cpp
    ${SRC_DIR}/game/shared/base_gcmessages.pb.cc  # NO_PCH
    ${SRC_DIR}/game/shared/cstrike15/cstrike15/cstrike15_gcmessages.pb.cc  # NO_PCH
    ${SRC_DIR}/common/engine_gcmessages.pb.cc  # NO_PCH
    ${SRC_DIR}/game/shared/cstrike15/cstrike15/cstrike15_usermessages.pb.cc  # NO_PCH
    ${SRC_DIR}/common/netmessages.pb.cc  # NO_PCH
    ${SRC_DIR}/common/network_connection.pb.cc  # NO_PCH
    ${SRC_DIR}/common/protobuf_helpers.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/achievement_saverestore.cpp
    ${SRC_DIR}/game/shared/achievementmgr.cpp
    ${SRC_DIR}/game/shared/achievements_hlx.cpp
    ${SRC_DIR}/game/shared/activitylist.cpp
    ${SRC_DIR}/game/shared/ammodef.cpp
    animatedentitytextureproxy.cpp
    animatedoffsettextureproxy.cpp
    animatedtextureproxy.cpp
    AnimateSpecificTextureProxy.cpp
    ${SRC_DIR}/game/shared/animation.cpp
    ${SRC_DIR}/game/shared/bannedwords.cpp
    ${SRC_DIR}/game/shared/base_playeranimstate.cpp
    ${SRC_DIR}/game/shared/baseachievement.cpp
    baseanimatedtextureproxy.cpp
    baseclientrendertargets.cpp
    ${SRC_DIR}/game/shared/basecombatcharacter_shared.cpp
    ${SRC_DIR}/game/shared/basecombatweapon_shared.cpp
    ${SRC_DIR}/game/shared/baseentity_shared.cpp
    ${SRC_DIR}/game/shared/basegrenade_shared.cpp
    ${SRC_DIR}/game/shared/baseparticleentity.cpp
    ${SRC_DIR}/game/shared/baseplayer_shared.cpp
    ${SRC_DIR}/game/shared/baseviewmodel_shared.cpp
    ${SRC_DIR}/game/shared/beam_shared.cpp
    beamdraw.cpp
    ${SRC_DIR}/common/blackbox_helper.cpp
    ${SRC_DIR}/public/bone_accessor.cpp
    ${SRC_DIR}/game/shared/bone_merge_cache.cpp
    ${SRC_DIR}/utils/common/bsplib.cpp  # NO_PCH
    c_ai_basehumanoid.cpp
    c_ai_basenpc.cpp
    c_baseanimating.cpp
    c_baseanimatingoverlay.cpp
    c_basecombatcharacter.cpp
    c_basecombatweapon.cpp
    c_basedoor.cpp
    c_baseentity.cpp
    c_baseflex.cpp
    c_baselesson.cpp
    c_baseplayer.cpp
    c_basetoggle.cpp
    c_baseviewmodel.cpp
    c_beamspotlight.cpp
    c_breakableprop.cpp
    c_colorcorrection.cpp
    c_colorcorrectionvolume.cpp
    c_dynamiclight.cpp
    c_entitydissolve.cpp
    c_entityflame.cpp
    c_entityfreezing.cpp
    c_entityparticletrail.cpp
    c_env_ambient_light.cpp
    c_env_cascade_light.cpp
    c_env_dof_controller.cpp
    c_env_fog_controller.cpp
    c_env_particlescript.cpp
    c_env_projectedtexture.cpp
    c_env_screenoverlay.cpp
    c_env_tonemap_controller.cpp
    c_fire_smoke.cpp
    c_fish.cpp
    c_func_areaportalwindow.cpp
    c_func_breakablesurf.cpp
    c_func_brush.cpp
    c_func_conveyor.cpp
    c_func_dust.cpp
    c_func_lod.cpp
    ${SRC_DIR}/game/client/hl2/C_Func_Monitor.cpp
    c_func_movelinear.cpp
    c_func_occluder.cpp
    c_func_reflective_glass.cpp
    c_func_rotating.cpp
    c_func_smokevolume.cpp
    c_func_tracktrain.cpp
    c_gameinstructor.cpp
    c_gib.cpp
    c_hairball.cpp
    c_info_overlay_accessor.cpp
    c_keyvalue_saver.cpp
    c_lightglow.cpp
    C_MaterialModifyControl.cpp
    c_movie_display.cpp
    c_particle_system.cpp
    c_physbox.cpp
    c_physics_prop_statue.cpp
    c_physicsprop.cpp
    c_physmagnet.cpp
    c_pixel_visibility.cpp
    c_plasma.cpp
    c_playerresource.cpp
    c_point_camera.cpp
    c_point_commentary_node.cpp
    c_postprocesscontroller.cpp
    c_prop_hallucination.cpp
    c_props.cpp
    c_ragdoll_manager.cpp
    c_rope.cpp
    c_rumble.cpp
    c_sceneentity.cpp
    c_shadowcontrol.cpp
    c_slideshow_display.cpp
    c_soundscape.cpp
    c_spatialentity.cpp
    c_spotlight_end.cpp
    c_sprite.cpp
    c_sprite_perfmonitor.cpp
    c_sun.cpp
    c_sunlightshadowcontrol.cpp
    c_surfacerender.cpp
    c_team.cpp
    c_team_objectiveresource.cpp
    c_tesla.cpp
    c_test_proxytoggle.cpp
    c_triggers.cpp
    c_user_message_register.cpp
    c_vehicle_choreo_generic.cpp
    c_vehicle_jeep.cpp
    c_vguiscreen.cpp
    c_vote_controller.cpp
    ${SRC_DIR}/game/client/hl2/c_waterbullet.cpp
    C_WaterLODControl.cpp
    c_world.cpp
    ${SRC_DIR}/game/shared/cam_thirdperson.cpp
    camomaterialproxy.cpp
    cdll_bounded_cvars.cpp
    cdll_client_int.cpp
    cdll_util.cpp
    cl_mat_stub.cpp
    classmap.cpp
    ${SRC_DIR}/public/client_class.cpp  # NO_PCH
    client_factorylist.cpp
    client_thinklist.cpp
    clientalphaproperty.cpp
    cliententitylist.cpp
    clientleafsystem.cpp
    clientmode_shared.cpp
    clientshadowmgr.cpp
    clientsideeffects.cpp
    clientsideeffects_test.cpp
    clientsteamcontext.cpp
    ${SRC_DIR}/public/closedcaptions.cpp  # NO_PCH
    ${SRC_DIR}/utils/common/cmdlib.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/collisionproperty.cpp
    ${SRC_DIR}/public/collisionutils.cpp  # NO_PCH
    colorcorrectionmgr.cpp
    commentary_modelviewer.cpp
    ${SRC_DIR}/common/compiledcaptionswap.cpp  # NO_PCH
    convarproxy.cpp
    ${SRC_DIR}/public/crtmemdebug.cpp  # NO_PCH
    csm_parallel_split.cpp
    cycleproxy.cpp
    ${SRC_DIR}/game/shared/death_pose.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/debugoverlay_shared.cpp
    ${SRC_DIR}/game/shared/decals.cpp
    detailobjectsystem.cpp
    ${SRC_DIR}/public/dt_recv.cpp  # NO_PCH
    ${SRC_DIR}/public/dt_utlvector_common.cpp  # NO_PCH
    ${SRC_DIR}/public/dt_utlvector_recv.cpp  # NO_PCH
    dummyproxy.cpp
    ${SRC_DIR}/game/shared/effect_dispatch_data.cpp
    EffectsClient.cpp
    ${SRC_DIR}/game/shared/ehandle.cpp
    ${SRC_DIR}/game/shared/entitylist_base.cpp
    entityoriginmaterialproxy.cpp
    ${SRC_DIR}/game/shared/EntityParticleTrail_Shared.cpp
    ${SRC_DIR}/game/shared/env_detail_controller.cpp
    ${SRC_DIR}/game/shared/env_wind_shared.cpp
    ${SRC_DIR}/game/shared/eventlist.cpp
    ${SRC_DIR}/tier1/fileio.cpp  # NO_PCH
    ${SRC_DIR}/public/filesystem_helpers.cpp  # NO_PCH
    ${SRC_DIR}/public/filesystem_init.cpp  # NO_PCH
    ${SRC_DIR}/utils/common/filesystem_tools.cpp  # NO_PCH
    flashlighteffect.cpp
    foundryhelpers_client.cpp
    ${SRC_DIR}/game/shared/func_ladder.cpp
    functionproxy.cpp
    fx_blood.cpp
    fx_cube.cpp
    fx_explosion.cpp
    fx_fleck.cpp
    fx_impact.cpp
    fx_interpvalue.cpp
    fx_quad.cpp
    fx_shelleject.cpp
    fx_staticline.cpp
    fx_tracer.cpp
    fx_trail.cpp
    fx_water.cpp
    ${SRC_DIR}/game/shared/game_timescale_shared.cpp
    ${SRC_DIR}/game/shared/gamemovement.cpp
    ${SRC_DIR}/game/shared/gamerules.cpp
    ${SRC_DIR}/game/shared/gamerules_register.cpp
    ${SRC_DIR}/game/shared/GameStats.cpp
    ${SRC_DIR}/game/shared/gamestringpool.cpp
    gametrace_client.cpp
    ${SRC_DIR}/game/shared/gamevars_shared.cpp
    geiger.cpp
    glow_outline_effect.cpp
    glow_overlay.cpp
    ${SRC_DIR}/game/shared/hintmessage.cpp
    ${SRC_DIR}/game/shared/hintsystem.cpp
    hltvcamera.cpp
    hud.cpp
    hud_animationinfo.cpp
    hud_base_account.cpp
    hud_baseachievement_tracker.cpp
    hud_basechat.cpp
    hud_basetimer.cpp
    hud_bitmapnumericdisplay.cpp
    hud_closecaption.cpp
    hud_crosshair.cpp
    hud_element_helper.cpp
    hud_hintdisplay.cpp
    hud_lcd.cpp  # NO_PCH
    hud_locator_target.cpp
    hud_msg.cpp
    hud_numericdisplay.cpp
    hud_pdump.cpp
    hud_redraw.cpp
    hud_subtitles.cpp
    hud_vehicle.cpp
    hud_voicestatus.cpp
    hud_weapon.cpp
    ${SRC_DIR}/game/shared/igamesystem.cpp
    ${SRC_DIR}/common/imageutils.cpp  # NO_PCH
    in_camera.cpp
    in_joystick.cpp
    in_main.cpp
    in_mouse.cpp  # NO_PCH
    in_steamcontroller.cpp  # NO_PCH
    initializer.cpp
    ${SRC_DIR}/public/tier1/interpolatedvar.cpp
    ${SRC_DIR}/public/interpolatortypes.cpp  # NO_PCH
    IsNPCProxy.cpp
    ${SRC_DIR}/public/jigglebones.cpp  # NO_PCH
    lampbeamproxy.cpp
    lamphaloproxy.cpp
    ${SRC_DIR}/public/lumpfiles.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/mapentities_shared.cpp
    materialproxydict.cpp
    mathproxy.cpp
    matrixproxy.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp  # NO_PCH
    menu.cpp
    message.cpp
    modelrendersystem.cpp
    movehelper_client.cpp
    ${SRC_DIR}/game/shared/movevars_shared.cpp
    ${SRC_DIR}/game/shared/mp_shareddefs.cpp
    ${SRC_DIR}/game/shared/multiplay_gamerules.cpp
    mumble.cpp  # NO_PCH
    ${SRC_DIR}/public/networkvar.cpp  # NO_PCH
    object_motion_blur_effect.cpp
    ${SRC_DIR}/game/shared/obstacle_pushaway.cpp
    panelmetaclassmgr.cpp
    particle_collision.cpp
    particle_litsmokeemitter.cpp
    ${SRC_DIR}/game/shared/particle_parse.cpp
    ${SRC_DIR}/game/shared/particle_property.cpp
    particle_proxies.cpp
    particle_simple3d.cpp
    particlemgr.cpp
    particles_attractor.cpp
    particles_ez.cpp
    particles_localspace.cpp
    particles_new.cpp
    particles_simple.cpp
    ${SRC_DIR}/game/shared/particlesystemquery.cpp
    perfvisualbenchmark.cpp
    ${SRC_DIR}/public/phonemeconverter.cpp  # NO_PCH
    physics.cpp
    physics_main_client.cpp
    ${SRC_DIR}/game/shared/physics_main_shared.cpp
    ${SRC_DIR}/game/shared/physics_saverestore.cpp
    ${SRC_DIR}/game/shared/physics_shared.cpp
    physics_softbody.cpp
    physpropclientside.cpp
    ${SRC_DIR}/game/shared/cstrike15/player_pickup.cpp
    playerandobjectenumerator.cpp
    ${SRC_DIR}/game/shared/point_bonusmaps_accessor.cpp
    ${SRC_DIR}/game/shared/point_posecontroller.cpp
    ${SRC_DIR}/public/posedebugger.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/precache_register.cpp
    ${SRC_DIR}/game/shared/predictableid.cpp
    ${SRC_DIR}/game/shared/predicted_viewmodel.cpp
    prediction.cpp
    ${SRC_DIR}/game/shared/predictioncopy.cpp
    ${SRC_DIR}/game/shared/predictioncopy_helpers.cpp
    ${SRC_DIR}/game/shared/predictioncopy_test.cpp
    ${SRC_DIR}/game/shared/props_shared.cpp
    proxyentity.cpp
    ProxyHealth.cpp
    proxyplayer.cpp
    proxypupil.cpp
    ragdoll.cpp
    ${SRC_DIR}/game/shared/ragdoll_shared.cpp
    ${SRC_DIR}/common/randoverride.cpp  # NO_PCH
    recvproxy.cpp
    render_to_rt_helper.cpp  # NO_PCH
    rendertexture.cpp  # NO_PCH
    replaycamera.cpp
    ${SRC_DIR}/game/shared/rope_helpers.cpp
    ${SRC_DIR}/public/rope_physics.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/saverestore.cpp
    ${SRC_DIR}/game/shared/sceneentity_shared.cpp
    ${SRC_DIR}/public/scratchpad3d.cpp  # NO_PCH
    ${SRC_DIR}/public/ScratchPadUtils.cpp  # NO_PCH
    ScreenSpaceEffects.cpp
    ${SRC_DIR}/utils/common/scriplib.cpp  # NO_PCH
    ${SRC_DIR}/public/sentence.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/sequence_Transitioner.cpp
    ${SRC_DIR}/game/shared/sheetsimulator.cpp  # NO_PCH
    simple_keys.cpp
    ${SRC_DIR}/public/simple_physics.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/simtimer.cpp
    ${SRC_DIR}/game/shared/singleplay_gamerules.cpp
    ${SRC_DIR}/game/shared/SoundEmitterSystem.cpp
    ${SRC_DIR}/game/shared/soundenvelope.cpp
    ${SRC_DIR}/public/SoundParametersInternal.cpp
    spatialentitymgr.cpp
    splinepatch.cpp
    ${SRC_DIR}/game/shared/Sprite.cpp
    spritemodel.cpp
    ${SRC_DIR}/game/shared/SpriteTrail.cpp
    ${SRC_DIR}/game/shared/steamworks_gamestats.cpp
    ${SRC_DIR}/public/stringregistry.cpp  # NO_PCH
    ${SRC_DIR}/public/studio.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/studio_shared.cpp
    subtitlepanel.cpp
    ${SRC_DIR}/game/shared/takedamageinfo.cpp
    ${SRC_DIR}/game/shared/teamplay_gamerules.cpp
    ${SRC_DIR}/game/shared/teamplayroundbased_gamerules.cpp
    ${SRC_DIR}/game/shared/test_ehandle.cpp
    text_message.cpp
    texturescrollmaterialproxy.cpp
    timematerialproxy.cpp
    toggletextureproxy.cpp
    train.cpp
    ${SRC_DIR}/game/shared/ugc_file_info_manager.cpp
    ${SRC_DIR}/game/shared/ugc_request_manager.cpp
    ${SRC_DIR}/game/shared/ugc_utils.cpp
    ${SRC_DIR}/game/shared/usercmd.cpp
    ${SRC_DIR}/game/shared/usermessages.cpp
    ${SRC_DIR}/game/shared/util_shared.cpp
    ${SRC_DIR}/public/vallocator.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/vehicle_viewblend_shared.cpp
    vgui_avatarimage.cpp
    vgui_basepanel.cpp
    vgui_bitmapbutton.cpp
    vgui_bitmapimage.cpp
    vgui_bitmappanel.cpp
    vgui_centerstringpanel.cpp
    vgui_consolepanel.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp  # NO_PCH
    vgui_debugoverlaypanel.cpp
    vgui_fpspanel.cpp
    vgui_game_viewport.cpp
    vgui_grid.cpp
    vgui_hudvideo.cpp
    vgui_int.cpp
    vgui_loadingdiscpanel.cpp
    vgui_messagechars.cpp
    vgui_movie_display.cpp
    vgui_netgraphpanel.cpp
    vgui_slideshow_display_screen.cpp
    vgui_video.cpp
    view.cpp
    view_beams.cpp
    view_effects.cpp
    view_scene.cpp
    viewangleanim.cpp
    ViewConeImage.cpp
    viewdebug.cpp
    viewpostprocess.cpp
    viewrender.cpp
    ${SRC_DIR}/game/shared/voice_banmgr.cpp
    ${SRC_DIR}/game/shared/voice_status.cpp
    vscript_client.cpp
    ${SRC_DIR}/game/shared/vscript_shared.cpp
    warp_overlay.cpp
    WaterLODMaterialProxy.cpp
    ${SRC_DIR}/game/shared/weapon_parse.cpp
    weapon_selection.cpp
    weapons_resource.cpp
    WorldDimsProxy.cpp
    ${SRC_DIR}/public/XUnzip.cpp  # NO_PCH
    ${SRC_DIR}/public/XZip.cpp  # NO_PCH
    ${SRC_DIR}/public/zip_utils.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/cstrike15/achievements_cs.cpp
    ${SRC_DIR}/game/shared/cstrike15/basecsgrenade_projectile.cpp
    ${SRC_DIR}/game/client/cstrike15/c_cs_hostage.cpp
    ${SRC_DIR}/game/client/cstrike15/c_cs_lesson.cpp
    ${SRC_DIR}/game/client/cstrike15/c_cs_player.cpp
    ${SRC_DIR}/game/client/cstrike15/c_cs_playerresource.cpp
    ${SRC_DIR}/game/client/cstrike15/c_cs_team.cpp
    ${SRC_DIR}/game/client/cstrike15/c_csrootpanel.cpp
    ${SRC_DIR}/game/client/cstrike15/c_plantedc4.cpp
    ${SRC_DIR}/game/client/cstrike15/c_te_radioicon.cpp
    ${SRC_DIR}/game/client/cstrike15/c_te_shotgun_shot.cpp
    ${SRC_DIR}/game/client/cstrike15/clientmode_csnormal.cpp
    ${SRC_DIR}/game/shared/cstrike15/cs_ammodef.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_client_gamestats.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_custom_clothing_visualsdata_processor.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_custom_embroider_visualsdata_processor.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_custom_epidermis_visualsdata_processor.cpp
    ${SRC_DIR}/game/shared/cs_custom_material_swap.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_custom_texture_saver.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_custom_weapon_visualsdata_processor.cpp
    ${SRC_DIR}/game/shared/cstrike15/cs_gamemovement.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_gameplay_hints.cpp
    ${SRC_DIR}/game/shared/cstrike15/cs_gamerules.cpp
    ${SRC_DIR}/game/shared/cstrike15/cs_gamestats_shared.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_in_main.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_player_rank_mgr.cpp
    ${SRC_DIR}/game/shared/cstrike15/cs_player_shared.cpp
    ${SRC_DIR}/game/shared/cstrike15/cs_playeranimstate.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_prediction.cpp
    ${SRC_DIR}/game/shared/cstrike15/cs_shareddefs.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_view_scene.cpp
    ${SRC_DIR}/game/shared/cstrike15/cs_weapon_parse.cpp
    ${SRC_DIR}/game/shared/cstrike15/cs_workshop_manager.cpp
    ${SRC_DIR}/game/shared/cstrike15/csgo_playeranimstate.cpp
    ${SRC_DIR}/game/client/cstrike15/fx_cs_blood.cpp
    ${SRC_DIR}/game/client/cstrike15/fx_cs_christmaslights.cpp
    ${SRC_DIR}/game/client/cstrike15/fx_cs_impacts.cpp
    ${SRC_DIR}/game/client/cstrike15/fx_cs_knifeslash.cpp
    ${SRC_DIR}/game/client/cstrike15/fx_cs_muzzleflash.cpp
    ${SRC_DIR}/game/shared/cstrike15/fx_cs_shared.cpp
    ${SRC_DIR}/game/client/cstrike15/fx_cs_weaponfx.cpp
    ${SRC_DIR}/game/shared/cstrike15/gametypes.cpp  # NO_PCH
    model_combiner.cpp
    ${SRC_DIR}/common/platforminputdevice.cpp  # NO_PCH
    ${SRC_DIR}/game/shared/cstrike15/bot/shared_util.cpp
    steamworks_gamestats_client.cpp
    ${SRC_DIR}/game/shared/ugc_workshop_manager.cpp
    ${SRC_DIR}/game/client/cstrike15/vgui_rootpanel_cs.cpp
    ${SRC_DIR}/game/client/cstrike15/Effects/c_chicken.cpp
    ${SRC_DIR}/game/client/cstrike15/Effects/clientinferno.cpp
    ${SRC_DIR}/game/shared/cstrike15/infernoshared.cpp
    hltvreplaysystem.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_hud_achievement_announce.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_hud_achievement_tracker.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_hud_ammo.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_hud_chat.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_hud_damageindicator.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_hud_freezepanel.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_hud_health.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_hud_playerhealth.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_hud_scope.cpp
    ${SRC_DIR}/game/client/cstrike15/cs_hud_weaponselection.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_account.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_armor.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_deathnotice.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_defuser.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_flashbang.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_hostagerescue.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_progressbar.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_radar.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_roundtimer.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_scenarioicon.cpp
    ${SRC_DIR}/game/client/cstrike15/hud_shopping_cart.cpp
    ${SRC_DIR}/game/client/cstrike15/radio_status.cpp
    ${SRC_DIR}/game/client/cstrike15/VGUI/backgroundpanel.cpp
    ${SRC_DIR}/game/client/cstrike15/VGUI/bordered_panel.cpp
    ${SRC_DIR}/game/client/cstrike15/VGUI/counterstrikeviewport.cpp
    ${SRC_DIR}/game/client/cstrike15/VGUI/cstrikebuyequipmenu.cpp
    ${SRC_DIR}/game/client/cstrike15/VGUI/cstrikebuymenu.cpp
    ${SRC_DIR}/game/client/cstrike15/VGUI/cstrikeclientscoreboard.cpp
    ${SRC_DIR}/game/client/cstrike15/VGUI/cstrikespectatorgui.cpp
    ${SRC_DIR}/game/client/cstrike15/VGUI/cstriketeammenu.cpp
    ${SRC_DIR}/game/client/cstrike15/VGUI/cstriketextwindow.cpp
    ${SRC_DIR}/game/client/cstrike15/vgui_c4panel.cpp
    ${SRC_DIR}/game/client/cstrike15/vgui_viewc4panel.cpp
    ${SRC_DIR}/game/client/cstrike15/VGUI/win_panel_round.cpp
    ${SRC_DIR}/game/shared/cstrike15/decoy_projectile.cpp
    ${SRC_DIR}/game/shared/cstrike15/item_healthshot.cpp
    ${SRC_DIR}/game/shared/cstrike15/molotov_projectile.cpp
    ${SRC_DIR}/game/shared/cstrike15/sensorgrenade_projectile.cpp
    ${SRC_DIR}/game/shared/cstrike15/smokegrenade_projectile.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_basecsgrenade.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_baseitem.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_c4.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_csbase.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_csbasegun.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_deagle.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_decoy.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_elite.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_flashbang.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_hegrenade.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_ironsightcontroller.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_knife.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_m3.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_molotov.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_nova.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_sawedoff.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_sensorgrenade.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_sg552.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_smokegrenade.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_taser.cpp
    ${SRC_DIR}/game/shared/cstrike15/weapon_xm1014.cpp
    ${SRC_DIR}/game/shared/econ/attribute_manager.cpp
    ${SRC_DIR}/game/shared/cstrike15/cs_econ_item_string_table.cpp
    ${SRC_DIR}/game/shared/cstrike15/cstrike15_item_inventory.cpp
    ${SRC_DIR}/game/shared/cstrike15/cstrike15_item_schema.cpp
    ${SRC_DIR}/game/shared/cstrike15/cstrike15_item_system.cpp
    ${SRC_DIR}/game/shared/econ/econ_entity.cpp
    ${SRC_DIR}/game/shared/econ/econ_entity_creation.cpp
    ${SRC_DIR}/game/shared/econ/econ_game_account_client.cpp
    ${SRC_DIR}/game/shared/econ/econ_item.cpp
    ${SRC_DIR}/game/shared/econ/econ_item_constants.cpp
    ${SRC_DIR}/game/shared/econ/econ_item_description.cpp
    ${SRC_DIR}/game/shared/econ/econ_item_interface.cpp
    ${SRC_DIR}/game/shared/econ/econ_item_inventory.cpp
    ${SRC_DIR}/game/shared/econ/econ_item_schema.cpp
    ${SRC_DIR}/game/shared/econ/econ_item_system.cpp
    ${SRC_DIR}/game/shared/econ/econ_item_view.cpp
    ${SRC_DIR}/game/shared/econ/item_selection_criteria.cpp
    ${SRC_DIR}/game/shared/econ/localization_provider.cpp
    ${SRC_DIR}/game/client/game_controls/basemodel_panel.cpp
    ${SRC_DIR}/game/client/game_controls/basemodelpanel.cpp
    ${SRC_DIR}/game/client/game_controls/baseviewport.cpp
    ${SRC_DIR}/game/client/game_controls/buymenu.cpp
    ${SRC_DIR}/game/client/game_controls/buysubmenu.cpp
    ${SRC_DIR}/game/client/game_controls/classmenu.cpp
    ${SRC_DIR}/game/client/game_controls/ClientScoreBoardDialog.cpp
    ${SRC_DIR}/game/client/game_controls/commandmenu.cpp
    ${SRC_DIR}/game/client/game_controls/IconPanel.cpp
    ${SRC_DIR}/game/client/game_controls/intromenu.cpp
    ${SRC_DIR}/game/client/game_controls/MapOverview.cpp
    ${SRC_DIR}/game/client/game_controls/NavProgress.cpp
    ${SRC_DIR}/game/client/game_controls/SpectatorGUI.cpp
    ${SRC_DIR}/game/client/game_controls/teammenu.cpp
    ${SRC_DIR}/game/client/game_controls/vguitextwindow.cpp
    ${SRC_DIR}/game/client/NextBot/C_NextBot.cpp
    ${SRC_DIR}/game/shared/SharedFunctorUtils.cpp
    stdafx.cpp  # NO_PCH
    ${SRC_DIR}/game/client/sixense/in_sixense.cpp
    ${SRC_DIR}/game/client/sixense/in_sixense_gesture_bindings.cpp
    ${SRC_DIR}/game/shared/sixense/sixense_convars.cpp
    c_basetempentity.cpp
    c_effects.cpp
    c_impact_effects.cpp
    c_movie_explosion.cpp
    c_particle_fire.cpp
    c_particle_smokegrenade.cpp
    c_prop_vehicle.cpp
    c_recipientfilter.cpp
    c_smoke_trail.cpp
    c_smokestack.cpp
    c_steamjet.cpp
    c_stickybolt.cpp
    c_te.cpp
    c_te_armorricochet.cpp
    c_te_basebeam.cpp
    c_te_beamentpoint.cpp
    c_te_beaments.cpp
    c_te_beamfollow.cpp
    c_te_beamlaser.cpp
    c_te_beampoints.cpp
    c_te_beamring.cpp
    c_te_beamringpoint.cpp
    c_te_beamspline.cpp
    c_te_bloodsprite.cpp
    c_te_bloodstream.cpp
    c_te_breakmodel.cpp
    c_te_bspdecal.cpp
    c_te_bubbles.cpp
    c_te_bubbletrail.cpp
    c_te_clientprojectile.cpp
    c_te_decal.cpp
    c_te_dynamiclight.cpp
    c_te_effect_dispatch.cpp
    c_te_energysplash.cpp
    c_te_explosion.cpp
    c_te_fizz.cpp
    c_te_footprint.cpp
    c_te_glassshatter.cpp
    c_te_glowsprite.cpp
    c_te_impact.cpp
    c_te_killplayerattachments.cpp
    c_te_largefunnel.cpp
    c_te_legacytempents.cpp
    c_te_muzzleflash.cpp
    c_te_particlesystem.cpp
    c_te_physicsprop.cpp
    c_te_playerdecal.cpp
    c_te_projecteddecal.cpp
    c_te_showline.cpp
    c_te_smoke.cpp
    c_te_sparks.cpp
    c_te_sprite.cpp
    c_te_spritespray.cpp
    c_te_worlddecal.cpp
    c_testtraceline.cpp
    c_tracer.cpp
    fx.cpp
    fx_discreetline.cpp
    fx_envelope.cpp
    fx_line.cpp
    fx_sparks.cpp
    particlesphererenderer.cpp
    smoke_fog_overlay.cpp
    ${SRC_DIR}/public/tools/bonelist.cpp
    entity_client_tools.cpp
    toolframework_client.cpp
)

add_library(client_csgo SHARED ${CLIENT_CSGO_SOURCES})

valve_apply_base_settings(client_csgo)

target_include_directories(client_csgo PRIVATE
    ${SRC_DIR}/game/client
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/thirdparty/protobuf-2.5.0/src
    ${SRC_DIR}/game/shared
    ${SRC_DIR}/game/shared/cstrike15/cstrike15
    ${SRC_DIR}/game/client/game_controls
    ${CMAKE_CURRENT_BINARY_DIR}/
    ${SRC_DIR}/thirdparty/SixenseSDK/include
    ${SRC_DIR}/gcsdk
    ${SRC_DIR}/gcsdk/steamextra
    ${SRC_DIR}/game/shared/econ
    ${SRC_DIR}/game/client/econ
    ${SRC_DIR}/external/crypto++-5.61
    ${SRC_DIR}/game/shared/cstrike15/control
    ${SRC_DIR}/game/client/cstrike15
    ${SRC_DIR}/game/client/cstrike15/control
    ${SRC_DIR}/game/client/cstrike15/VGUI
    ${SRC_DIR}/game/shared/cstrike15
)

target_compile_definitions(client_csgo PRIVATE
    _USRDLL
    DLLNAME=client
    MEMOVERRIDE_MODULE=client
    PROTOBUF
    NO_STRING_T
    CLIENT_DLL
    VECTOR
    VERSION_SAFE_STEAM_API_INTERFACES
    PROTECTED_THINGS_ENABLE
    sprintf=use_Q_snprintf_instead_of_sprintf
    strncpy=use_Q_strncpy_instead
    _snprintf=use_Q_snprintf_instead
    fopen=dont_use_fopen
    ENABLE_CHROMEHTMLWINDOW
    ENABLE_STUDIO_SOFTBODY
    CSTRIKE_CLIENT_DLL
    CSTRIKE_DLL
    USE_ECONOMY_FEATURES
)

target_precompile_headers(client_csgo PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:cbase.h>"
)

# 这些文件不使用预编译头
set_source_files_properties(
    ${SRC_DIR}/game/shared/base_gcmessages.pb.cc
    ${SRC_DIR}/game/shared/cstrike15/cstrike15/cstrike15_gcmessages.pb.cc
    ${SRC_DIR}/common/engine_gcmessages.pb.cc
    ${SRC_DIR}/game/shared/cstrike15/cstrike15/cstrike15_usermessages.pb.cc
    ${SRC_DIR}/common/netmessages.pb.cc
    ${SRC_DIR}/common/network_connection.pb.cc
    ${SRC_DIR}/common/protobuf_helpers.cpp
    ${SRC_DIR}/utils/common/bsplib.cpp
    ${SRC_DIR}/public/client_class.cpp
    ${SRC_DIR}/public/closedcaptions.cpp
    ${SRC_DIR}/utils/common/cmdlib.cpp
    ${SRC_DIR}/public/collisionutils.cpp
    ${SRC_DIR}/common/compiledcaptionswap.cpp
    ${SRC_DIR}/public/crtmemdebug.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/dt_recv.cpp
    ${SRC_DIR}/public/dt_utlvector_common.cpp
    ${SRC_DIR}/public/dt_utlvector_recv.cpp
    ${SRC_DIR}/tier1/fileio.cpp
    ${SRC_DIR}/public/filesystem_helpers.cpp
    ${SRC_DIR}/public/filesystem_init.cpp
    ${SRC_DIR}/utils/common/filesystem_tools.cpp
    hud_lcd.cpp
    ${SRC_DIR}/common/imageutils.cpp
    in_mouse.cpp
    in_steamcontroller.cpp
    ${SRC_DIR}/public/interpolatortypes.cpp
    ${SRC_DIR}/public/jigglebones.cpp
    ${SRC_DIR}/public/lumpfiles.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    mumble.cpp
    ${SRC_DIR}/public/networkvar.cpp
    ${SRC_DIR}/public/phonemeconverter.cpp
    ${SRC_DIR}/public/posedebugger.cpp
    ${SRC_DIR}/common/randoverride.cpp
    render_to_rt_helper.cpp
    rendertexture.cpp
    ${SRC_DIR}/public/rope_physics.cpp
    ${SRC_DIR}/public/scratchpad3d.cpp
    ${SRC_DIR}/public/ScratchPadUtils.cpp
    ${SRC_DIR}/utils/common/scriplib.cpp
    ${SRC_DIR}/public/sentence.cpp
    ${SRC_DIR}/game/shared/sheetsimulator.cpp
    ${SRC_DIR}/public/simple_physics.cpp
    ${SRC_DIR}/public/stringregistry.cpp
    ${SRC_DIR}/public/studio.cpp
    ${SRC_DIR}/public/vallocator.cpp
    ${SRC_DIR}/public/vgui_controls/vgui_controls.cpp
    ${SRC_DIR}/public/XUnzip.cpp
    ${SRC_DIR}/public/XZip.cpp
    ${SRC_DIR}/public/zip_utils.cpp
    ${SRC_DIR}/game/shared/cstrike15/gametypes.cpp
    ${SRC_DIR}/common/platforminputdevice.cpp
    stdafx.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

target_link_libraries(client_csgo PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    tier2
    tier3
    bitmap
    bonesetup
    choreoobjects
    Dmxloader
    mathlib
    mathlib_extended
    matsys_controls
    particles
    Raytrace
    vgui_controls
    videocfg
    vtf
    vpklib
    resourcefile
    meshutils
    prebuilt::libjpeg
    prebuilt::libprotobuf
    prebuilt::libz
    prebuilt::steam_api
    # 系统库
    Imm32.lib
    Shlwapi.lib
    Ws2_32.lib
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    odbc32.lib
    odbccp32.lib
    ole32.lib
    shell32.lib
    user32.lib
    winmm.lib
    # 其他（路径引用）
    debug "${SRC_DIR}/lib/win32/2015/debug/cryptlib.lib"
    optimized "${SRC_DIR}/lib/win32/2015/release/cryptlib.lib"
)

# protobuf 生成文件：标记为 GENERATED（由 protoc 生成，不检查是否存在）
set_source_files_properties(
    ${SRC_DIR}/game/shared/base_gcmessages.pb.cc
    ${SRC_DIR}/game/shared/cstrike15/cstrike15/cstrike15_gcmessages.pb.cc
    ${SRC_DIR}/common/engine_gcmessages.pb.cc
    ${SRC_DIR}/game/shared/cstrike15/cstrike15/cstrike15_usermessages.pb.cc
    ${SRC_DIR}/common/netmessages.pb.cc
    ${SRC_DIR}/common/network_connection.pb.cc
    PROPERTIES GENERATED TRUE)

# 如需重新生成 .proto 文件，可运行：
# cmake --build . --target generate_proto  （需先在 CMakeLists.txt 中添加自定义目标）

valve_set_dll_importlib_output(client_csgo)

valve_publish_dll(client_csgo "${GAME_CSGO_BIN}")
