set(MEL_SCRIPT_FILES
    mayaHydraMenus.mel
    mayaHydra_registerUI_load.mel
    mayaHydra_registerUI_unload.mel
    mayaHydra_registerUI_batch_load.mel
    mayaHydra_registerUI_batch_unload.mel
)

install(
    FILES ${MEL_SCRIPT_FILES}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/scripts
)
