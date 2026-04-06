# =============================================================================
# stdshader_dbg
# Generated from: stdshader_dbg.vcxproj
# =============================================================================

set(STDSHADER_DBG_SOURCES
    BaseVSShader.cpp
    ${SRC_DIR}/common/debug_dll_check.cpp
    debugdepth.cpp
    DebugDrawEnvmapMask.cpp
    debugluxel.cpp
    debugnormalmap.cpp
    debugtangentspace.cpp
    fillrate.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    ${SRC_DIR}/materialsystem/shader_dll_verify.cpp
)

add_library(stdshader_dbg SHARED ${STDSHADER_DBG_SOURCES})

valve_apply_base_settings(stdshader_dbg)

target_include_directories(stdshader_dbg PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/materialsystem/stdshaders/fxctmp9
    ${SRC_DIR}/materialsystem/stdshaders/vshtmp9
    ${SRC_DIR}/dx9sdk/include
    ${SRC_DIR}/materialsystem
)

target_compile_definitions(stdshader_dbg PRIVATE
    _USRDLL
    DLLNAME=stdshader_dbg
    MEMOVERRIDE_MODULE=stdshader_dbg
    STDSHADER_DBG_DLL_EXPORT
    FAST_MATERIALVAR_ACCESS
)

target_link_libraries(stdshader_dbg PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    vstdlib
    mathlib
    shaderlib
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    version.lib
    winmm.lib
)

valve_set_dll_importlib_output(stdshader_dbg)

valve_publish_dll(stdshader_dbg "${GAME_BIN_DIR}")

# =============================================================================
# stdshader_dx9
# Generated from: stdshader_dx9.vcxproj
# =============================================================================

set(STDSHADER_DX9_SOURCES
    ${SRC_DIR}/common/debug_dll_check.cpp
    ${SRC_DIR}/public/tier0/memoverride.cpp
    AccumBuff4Sample.cpp
    accumbuff5sample.cpp
    aftershock.cpp
    aftershock_helper.cpp
    BaseVSShader.cpp
    bik_dx90.cpp
    black.cpp
    Blob_dx9.cpp
    Blob_helper.cpp
    Bloom.cpp
    BlurFilterX.cpp
    BlurFilterY.cpp
    BufferClearObeyStencil_dx9.cpp
    cable_dx9.cpp
    character.cpp
    character_ssao.cpp
    cloak.cpp
    cloak_blended_pass_helper.cpp
    cloak_dx9_helper.cpp
    colorcorrection.cpp
    colorout.cpp
    cs_grass.cpp
    customcharacter.cpp
    customclothing.cpp
    customweapon_dx9.cpp
    customweapon_dx9_helper.cpp
    debugmorphaccumulator_dx9.cpp
    debugmrttexture.cpp
    DebugTextureView.cpp
    DecalModulate_dx9.cpp
    DepthOfField_dx9.cpp
    depthwrite.cpp
    Downsample.cpp
    downsample_nohdr.cpp
    embroider.cpp
    emissive_scroll_blended_pass_helper.cpp
    Engine_Post_dx9.cpp
    eye_refract.cpp
    eye_refract_helper.cpp
    eyeglint_dx9.cpp
    eyes_dx8_dx9_helper.cpp
    eyes_dx9.cpp
    filmdust.cpp
    filmgrain.cpp
    flesh_interior_blended_pass_helper.cpp
    floatcombine.cpp
    floatcombine_autoexpose.cpp
    floattoscreen.cpp
    floattoscreen_vanilla.cpp
    gamecontrols.cpp
    HDRCombineTo16Bit.cpp
    HDRSelectRange.cpp
    hsl_filmgrain_pass1.cpp
    hsl_filmgrain_pass2.cpp
    hsv.cpp
    IceSurface_dx9.cpp
    IceSurface_helper.cpp
    introscreenspaceeffect.cpp
    lightmapped_4wayblend.cpp
    lightmapped_4wayblend_dx9_helper.cpp
    lightmappedgeneric_dx9.cpp
    lightmappedgeneric_dx9_helper.cpp
    lightmappedpaint_dx9_helper.cpp
    lightmappedreflective.cpp
    lightshafts.cpp
    lightshafts_helper.cpp
    modulate_dx9.cpp
    MonitorScreen_dx9.cpp
    morphaccumulate_dx9.cpp
    morphweight_dx9.cpp
    motion_blur_dx9.cpp
    multiblend.cpp
    multiblend_dx9_helper.cpp
    object_motion_blur_dx9.cpp
    occlusion_dx9.cpp
    paintblob_dx9.cpp
    paintblob_helper.cpp
    parallaxtest.cpp
    particlelitgeneric_dx9.cpp
    particlelitgeneric_dx9_helper.cpp
    particlesphere_dx9.cpp
    phong_dx9_helper.cpp
    portal.cpp
    portal_refract.cpp
    portal_refract_helper.cpp
    portalstaticoverlay.cpp
    projected.cpp
    projected_dx9_helper.cpp
    proto_ice.cpp
    proto_ice_helper.cpp
    prototype.cpp
    prototype_helper.cpp
    refract.cpp
    refract_dx9_helper.cpp
    sample4x4.cpp
    sample4x4_blend.cpp
    screenspace_general.cpp
    sfm_ambientocclusion.cpp
    sfm_ao_blur.cpp
    sfm_blurfilterx.cpp
    sfm_blurfiltery.cpp
    sfm_downsample.cpp
    sfm_integercombine.cpp
    sfm_shape.cpp
    shadow.cpp
    shadowbuild_dx9.cpp
    shadowmodel_dx9.cpp
    shatteredglass.cpp
    showz.cpp
    sky_hdr_dx9.cpp
    solidenergy_dx9.cpp
    solidenergy_dx9_helper.cpp
    splinerope.cpp
    sprite_dx9.cpp
    spritecard.cpp
    teeth.cpp
    TreeLeaf.cpp
    unlitgeneric_dx9.cpp
    unlittwotexture_dx9.cpp
    unlitworld_screensample.cpp
    vertexlitgeneric_dx9.cpp
    vertexlitgeneric_dx9_helper.cpp
    volume_clouds.cpp
    volume_clouds_helper.cpp
    vortwarp_dx9.cpp
    water.cpp
    weapondecal_dx9.cpp
    weapondecal_dx9_helper.cpp
    windowimposter_dx90.cpp
    wireframe_dx9.cpp
    worldimposter.cpp
    worldtwotextureblend.cpp
    worldvertextransition.cpp
    worldvertextransition_dx8_helper.cpp
    writestencil_dx9.cpp
    writez_dx9.cpp
    ${SRC_DIR}/tier1/checksum_crc.cpp
    ${SRC_DIR}/tier1/checksum_md5.cpp
    ${SRC_DIR}/materialsystem/shader_dll_verify.cpp
)

add_library(stdshader_dx9 SHARED ${STDSHADER_DX9_SOURCES})

valve_apply_base_settings(stdshader_dx9)

target_include_directories(stdshader_dx9 PRIVATE
    ${SRC_DIR}/common
    ${SRC_DIR}/public
    ${SRC_DIR}/public/tier0
    ${SRC_DIR}/public/tier1
    ${SRC_DIR}/materialsystem/stdshaders/fxctmp9
    ${SRC_DIR}/materialsystem/stdshaders/vshtmp9
    ${SRC_DIR}/dx9sdk/include
    ${SRC_DIR}/materialsystem
)

target_compile_definitions(stdshader_dx9 PRIVATE
    _USRDLL
    DLLNAME=stdshader_dx9
    MEMOVERRIDE_MODULE=stdshader_dx9
    STDSHADER_DX9_DLL_EXPORT
    FAST_MATERIALVAR_ACCESS
    USE_ACTUAL_DX
)

target_link_libraries(stdshader_dx9 PRIVATE
    # Valve libs
    tier0
    tier1
    interfaces
    vstdlib
    mathlib
    shaderlib
    # 系统库
    advapi32.lib
    comdlg32.lib
    gdi32.lib
    ole32.lib
    shell32.lib
    user32.lib
    version.lib
    winmm.lib
)

valve_set_dll_importlib_output(stdshader_dx9)

valve_publish_dll(stdshader_dx9 "${GAME_BIN_DIR}")
