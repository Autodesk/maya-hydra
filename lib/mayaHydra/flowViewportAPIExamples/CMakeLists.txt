add_subdirectory(flowViewportAPILocator)
add_subdirectory(footPrintNode)
add_subdirectory(customShadersNode)
add_subdirectory(simpleHydraGenerativeProceduralPlugin)

# The custom-node translation sample is retained as documentation, but is not
# built or installed. Its Arnold implementation now lives in arnold-usd's
# MTOA_BUILD-only HdArnoldMtoaSceneIndexPlugin; loading both would register two
# Arnold scene-index translators for the same Maya custom nodes.
# add_subdirectory(customNodeTranslationSceneIndex)
