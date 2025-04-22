set(MEL_SCRIPT_FILES
    mayaHydra_registerUIStrings.mel
    mayaHydra_registerUI_load.mel
    mayaHydra_registerUI_unload.mel
    mayaHydra_registerUI_batch_load.mel
    mayaHydra_registerUI_batch_unload.mel
    mayaHydra_GeomSubsetsPickMode.mel
    mayaHydra_create_globals_tab_Storm.mel
    mayaHydra_register_renderer_Storm.mel
    mayaHydra_render_Storm.mel
)

install(
    FILES ${MEL_SCRIPT_FILES}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/scripts
)
