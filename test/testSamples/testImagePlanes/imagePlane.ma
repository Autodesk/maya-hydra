//Maya ASCII 2026 scene
//Name: imagePlane.ma
//Last modified: Fri, Apr 17, 2026 02:50:13 PM
//Codeset: 1252
requires maya "2026";
currentUnit -l centimeter -a degree -t film;
fileInfo "application" "maya";
fileInfo "product" "Maya 2026";
fileInfo "version" "Preview Release";
fileInfo "cutIdentifier" "202604010801-000000";
fileInfo "osv" "Windows 11 Enterprise v2009 (Build: 26200)";
fileInfo "UUID" "4F87C0EC-4AE5-E755-524B-419416D2683C";
createNode transform -s -n "persp";
	rename -uid "47580B1B-4996-2CFD-ADCD-A2BDE9084B39";
	setAttr ".t" -type "double3" 12.954803073348437 35.55706400568814 -12.971498433504983 ;
	setAttr ".r" -type "double3" -68.138352729572617 463.3999999999553 0 ;
createNode camera -s -n "perspShape" -p "persp";
	rename -uid "E43056FE-42EB-18B0-7836-CDAD3E61EEA4";
	setAttr -k off ".v";
	setAttr ".fl" 34.999999999999993;
	setAttr ".coi" 44.821869662040221;
	setAttr ".imn" -type "string" "persp";
	setAttr ".den" -type "string" "persp_depth";
	setAttr ".man" -type "string" "persp_mask";
	setAttr ".hc" -type "string" "viewSet -p %camera";
createNode transform -n "imagePlane1" -p "perspShape";
	rename -uid "96B6A9D4-4699-1426-7037-C1BE0A36CE94";
createNode imagePlane -n "imagePlaneShape1" -p "imagePlane1";
	rename -uid "C29F24FB-44C0-04F6-EDFD-2981A4C6E8E6";
	setAttr -k off ".v";
	setAttr ".fc" 10;
	setAttr ".imn" -type "string" "Image1.png";
	setAttr ".ufe" yes;
	setAttr ".cov" -type "short2" 64 64 ;
	setAttr ".w" 0.64;
	setAttr ".h" 0.64;
	setAttr ".cs" -type "string" "sRGB Encoded Rec.709 (sRGB)";
createNode transform -s -n "top";
	rename -uid "F147285C-4D46-543C-3F70-D1ABF540AD9F";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 0 1000.1 0 ;
	setAttr ".r" -type "double3" -90 0 0 ;
createNode camera -s -n "topShape" -p "top";
	rename -uid "61B4ABC3-4FCB-C7F8-469B-68908E07DE7D";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".coi" 1000.1;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "top";
	setAttr ".den" -type "string" "top_depth";
	setAttr ".man" -type "string" "top_mask";
	setAttr ".hc" -type "string" "viewSet -t %camera";
	setAttr ".o" yes;
createNode transform -s -n "front";
	rename -uid "BAF51FBE-4071-B12C-1139-13B6DD6D60DA";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 0 0 1000.1 ;
createNode camera -s -n "frontShape" -p "front";
	rename -uid "DBA6EE67-4FBD-753F-F278-8FA60C793367";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".coi" 1000.1;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "front";
	setAttr ".den" -type "string" "front_depth";
	setAttr ".man" -type "string" "front_mask";
	setAttr ".hc" -type "string" "viewSet -f %camera";
	setAttr ".o" yes;
createNode transform -s -n "side";
	rename -uid "ABB88104-481F-4BDD-550B-0C9951D3EEB7";
	setAttr ".v" no;
	setAttr ".t" -type "double3" 1000.1 0 0 ;
	setAttr ".r" -type "double3" 0 90 0 ;
createNode camera -s -n "sideShape" -p "side";
	rename -uid "583C2DC5-4CBE-2285-3750-5386FF34D4E0";
	setAttr -k off ".v" no;
	setAttr ".rnd" no;
	setAttr ".coi" 1000.1;
	setAttr ".ow" 30;
	setAttr ".imn" -type "string" "side";
	setAttr ".den" -type "string" "side_depth";
	setAttr ".man" -type "string" "side_mask";
	setAttr ".hc" -type "string" "viewSet -s %camera";
	setAttr ".o" yes;
createNode transform -n "pSphere1";
	rename -uid "644A5571-4FE2-2DE2-A626-4FAA3C8CD4DD";
createNode mesh -n "pSphereShape1" -p "pSphere1";
	rename -uid "FAFE678E-4C30-80FA-A12A-FC85C62004AD";
	setAttr -k off ".v";
	setAttr ".vir" yes;
	setAttr ".vif" yes;
	setAttr ".uvst[0].uvsn" -type "string" "map1";
	setAttr ".cuvs" -type "string" "map1";
	setAttr ".dcc" -type "string" "Ambient+Diffuse";
	setAttr ".covm[0]"  0 1 1;
	setAttr ".cdvm[0]"  0 1 1;
createNode lightLinker -s -n "lightLinker1";
	rename -uid "56E75F25-47FE-1277-4218-528201EFA882";
	setAttr -s 2 ".lnk";
	setAttr -s 2 ".slnk";
createNode displayLayerManager -n "layerManager";
	rename -uid "163E81AB-49CD-7F02-12FC-B5933B709B24";
createNode displayLayer -n "defaultLayer";
	rename -uid "C8E8E324-41DE-DCEF-2539-1FBA0E3947A5";
	setAttr ".ufem" -type "stringArray" 0  ;
createNode renderLayerManager -n "renderLayerManager";
	rename -uid "1941D45A-4362-0465-EE14-29AF6050D152";
createNode renderLayer -n "defaultRenderLayer";
	rename -uid "1E39B45D-4D4B-5078-708C-ACA18DD7C5B8";
	setAttr ".g" yes;
createNode shapeEditorManager -n "shapeEditorManager";
	rename -uid "D2EAF893-4AEF-368A-85AE-99B9EDCEED71";
createNode poseInterpolatorManager -n "poseInterpolatorManager";
	rename -uid "AB3E6959-4F24-0FCA-BE6C-649A67A6E776";
createNode polySphere -n "polySphere1";
	rename -uid "C261D809-4E09-746D-7E47-F090A48EE813";
createNode timeToUnitConversion -n "timeToUnitConversion1";
	rename -uid "3652FA15-45E9-1F6F-4D8A-49BEFBC63CF4";
	setAttr ".cf" 0.004;
select -ne :time1;
	setAttr ".o" 1;
	setAttr ".unw" 1;
select -ne :hardwareRenderingGlobals;
	setAttr ".otfna" -type "stringArray" 22 "NURBS Curves" "NURBS Surfaces" "Polygons" "Subdiv Surface" "Particles" "Particle Instance" "Fluids" "Strokes" "Image Planes" "UI" "Lights" "Cameras" "Locators" "Joints" "IK Handles" "Deformers" "Motion Trails" "Components" "Hair Systems" "Follicles" "Misc. UI" "Ornaments"  ;
	setAttr ".otfva" -type "Int32Array" 22 0 1 1 1 1 1
		 1 1 1 0 0 0 0 0 0 0 0 0
		 0 0 0 0 ;
	setAttr ".fprt" yes;
	setAttr ".rtfm" 1;
select -ne :renderPartition;
	setAttr -s 2 ".st";
select -ne :renderGlobalsList1;
select -ne :defaultShaderList1;
	setAttr -s 6 ".s";
select -ne :postProcessList1;
	setAttr -s 2 ".p";
select -ne :defaultRenderingList1;
select -ne :standardSurface1;
	setAttr ".bc" -type "float3" 0.40000001 0.40000001 0.40000001 ;
	setAttr ".sr" 0.5;
select -ne :openPBR_shader1;
	setAttr ".bc" -type "float3" 0.40000001 0.40000001 0.40000001 ;
	setAttr ".sr" 0.5;
select -ne :initialShadingGroup;
	setAttr ".ro" yes;
select -ne :initialParticleSE;
	setAttr ".ro" yes;
select -ne :defaultRenderGlobals;
	addAttr -ci true -sn "mtohMotionSampleStart" -ln "mtohMotionSampleStart" -at "float";
	addAttr -ci true -sn "mtohMotionSampleEnd" -ln "mtohMotionSampleEnd" -at "float";
	addAttr -ci true -sn "mayaHydraRenderPurpose" -ln "mayaHydraRenderPurpose" -dv 1 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "mayaHydraProxyPurpose" -ln "mayaHydraProxyPurpose" -dv 1 -min 
		0 -max 1 -at "bool";
	addAttr -ci true -sn "mayaHydraGuidePurpose" -ln "mayaHydraGuidePurpose" -dv 1 -min 
		0 -max 1 -at "bool";
	addAttr -ci true -sn "mtohTextureMemoryPerTexture" -ln "mtohTextureMemoryPerTexture" 
		-dv 4096 -min 1 -max 262144 -smn 16384 -at "long";
	addAttr -ci true -sn "mtohMaximumShadowMapResolution" -ln "mtohMaximumShadowMapResolution" 
		-dv 2048 -min 32 -max 8192 -at "long";
	addAttr -ci true -sn "mayaHydraRefinementLevel" -ln "mayaHydraRefinementLevel" -min 
		0 -max 8 -at "long";
	addAttr -ci true -sn "HdPrmanXpuLoaderRendererPlugin__convergedSamplesPerPixel" 
		-ln "HdPrmanXpuLoaderRendererPlugin__convergedSamplesPerPixel" -at "long";
	addAttr -ci true -sn "HdPrmanXpuLoaderRendererPlugin__convergedVariance" -ln "HdPrmanXpuLoaderRendererPlugin__convergedVariance" 
		-dv 0.014999999664723873 -at "float";
	addAttr -ci true -sn "HdPrmanXpuLoaderRendererPlugin__disableMotionBlur" -ln "HdPrmanXpuLoaderRendererPlugin__disableMotionBlur" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdStormRendererPlugin__enableTinyPrimCulling" -ln "HdStormRendererPlugin__enableTinyPrimCulling" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdStormRendererPlugin__volumeRaymarchingStepSize" -ln "HdStormRendererPlugin__volumeRaymarchingStepSize" 
		-dv 1 -at "float";
	addAttr -ci true -sn "HdStormRendererPlugin__volumeRaymarchingStepSizeLighting" 
		-ln "HdStormRendererPlugin__volumeRaymarchingStepSizeLighting" -dv 10 -at "float";
	addAttr -ci true -sn "HdStormRendererPlugin__volumeMaxTextureMemoryPerField" -ln "HdStormRendererPlugin__volumeMaxTextureMemoryPerField" 
		-dv 128 -at "float";
	addAttr -ci true -sn "HdStormRendererPlugin__maxLights" -ln "HdStormRendererPlugin__maxLights" 
		-dv 60 -at "long";
	addAttr -ci true -sn "HdStormRendererPlugin__domeLightCameraVisibility" -ln "HdStormRendererPlugin__domeLightCameraVisibility" 
		-dv 1 -min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdStormRendererPlugin__enableExposureCompensation" -ln "HdStormRendererPlugin__enableExposureCompensation" 
		-dv 1 -min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__enable_progressive_render" -ln "HdArnoldRendererPlugin__enable_progressive_render" 
		-dv 1 -min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__progressive_min_AA_samples" -ln "HdArnoldRendererPlugin__progressive_min_AA_samples" 
		-dv -4 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__enable_adaptive_sampling" -ln "HdArnoldRendererPlugin__enable_adaptive_sampling" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__enable_gpu_rendering" -ln "HdArnoldRendererPlugin__enable_gpu_rendering" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__interactive_target_fps" -ln "HdArnoldRendererPlugin__interactive_target_fps" 
		-dv 32 -at "float";
	addAttr -ci true -sn "HdArnoldRendererPlugin__interactive_target_fps_min" -ln "HdArnoldRendererPlugin__interactive_target_fps_min" 
		-dv 8 -at "float";
	addAttr -ci true -sn "HdArnoldRendererPlugin__interactive_fps_min" -ln "HdArnoldRendererPlugin__interactive_fps_min" 
		-dv 2 -at "float";
	addAttr -ci true -sn "HdArnoldRendererPlugin__threads" -ln "HdArnoldRendererPlugin__threads" 
		-dv -1 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__AA_samples" -ln "HdArnoldRendererPlugin__AA_samples" 
		-dv 3 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__AA_samples_max" -ln "HdArnoldRendererPlugin__AA_samples_max" 
		-dv 20 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__GI_diffuse_samples" -ln "HdArnoldRendererPlugin__GI_diffuse_samples" 
		-dv 2 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__GI_specular_samples" -ln "HdArnoldRendererPlugin__GI_specular_samples" 
		-dv 2 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__GI_transmission_samples" -ln "HdArnoldRendererPlugin__GI_transmission_samples" 
		-dv 2 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__GI_sss_samples" -ln "HdArnoldRendererPlugin__GI_sss_samples" 
		-dv 2 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__GI_volume_samples" -ln "HdArnoldRendererPlugin__GI_volume_samples" 
		-dv 2 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__auto_transparency_depth" -ln "HdArnoldRendererPlugin__auto_transparency_depth" 
		-dv 10 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__GI_diffuse_depth" -ln "HdArnoldRendererPlugin__GI_diffuse_depth" 
		-dv 1 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__GI_specular_depth" -ln "HdArnoldRendererPlugin__GI_specular_depth" 
		-dv 1 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__GI_transmission_depth" -ln "HdArnoldRendererPlugin__GI_transmission_depth" 
		-dv 8 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__GI_volume_depth" -ln "HdArnoldRendererPlugin__GI_volume_depth" 
		-at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__GI_total_depth" -ln "HdArnoldRendererPlugin__GI_total_depth" 
		-dv 10 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__abort_on_error" -ln "HdArnoldRendererPlugin__abort_on_error" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_textures" -ln "HdArnoldRendererPlugin__ignore_textures" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_shaders" -ln "HdArnoldRendererPlugin__ignore_shaders" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_atmosphere" -ln "HdArnoldRendererPlugin__ignore_atmosphere" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_lights" -ln "HdArnoldRendererPlugin__ignore_lights" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_shadows" -ln "HdArnoldRendererPlugin__ignore_shadows" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_subdivision" -ln "HdArnoldRendererPlugin__ignore_subdivision" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_displacement" -ln "HdArnoldRendererPlugin__ignore_displacement" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_bump" -ln "HdArnoldRendererPlugin__ignore_bump" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_motion" -ln "HdArnoldRendererPlugin__ignore_motion" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_motion_blur" -ln "HdArnoldRendererPlugin__ignore_motion_blur" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_dof" -ln "HdArnoldRendererPlugin__ignore_dof" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_smoothing" -ln "HdArnoldRendererPlugin__ignore_smoothing" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_sss" -ln "HdArnoldRendererPlugin__ignore_sss" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__ignore_operators" -ln "HdArnoldRendererPlugin__ignore_operators" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__report_mtohns_file" -ln "HdArnoldRendererPlugin__report_mtohns_file" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__log_mtohns_verbosity" -ln "HdArnoldRendererPlugin__log_mtohns_verbosity" 
		-dv 2 -at "long";
	addAttr -ci true -sn "HdArnoldRendererPlugin__log_mtohns_file" -ln "HdArnoldRendererPlugin__log_mtohns_file" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__profile_mtohns_file" -ln "HdArnoldRendererPlugin__profile_mtohns_file" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__stats_mtohns_file" -ln "HdArnoldRendererPlugin__stats_mtohns_file" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__plugin_searchpath" -ln "HdArnoldRendererPlugin__plugin_searchpath" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__asset_searchpath" -ln "HdArnoldRendererPlugin__asset_searchpath" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__osl_includepath" -ln "HdArnoldRendererPlugin__osl_includepath" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__subdiv_dicing_camera" -ln "HdArnoldRendererPlugin__subdiv_dicing_camera" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__subdiv_frustum_culling" -ln "HdArnoldRendererPlugin__subdiv_frustum_culling" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdArnoldRendererPlugin__subdiv_frustum_padding" -ln "HdArnoldRendererPlugin__subdiv_frustum_padding" 
		-at "float";
	addAttr -ci true -sn "HdArnoldRendererPlugin__shader_override" -ln "HdArnoldRendererPlugin__shader_override" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__background" -ln "HdArnoldRendererPlugin__background" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__atmosphere" -ln "HdArnoldRendererPlugin__atmosphere" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__aov_shaders" -ln "HdArnoldRendererPlugin__aov_shaders" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__imager" -ln "HdArnoldRendererPlugin__imager" 
		-dt "string";
	addAttr -ci true -sn "HdArnoldRendererPlugin__texture_auto_generate_tx" -ln "HdArnoldRendererPlugin__texture_auto_generate_tx" 
		-dv 1 -min 0 -max 1 -at "bool";
	addAttr -ci true -sn "HdPrmanLoaderRendererPlugin__convergedSamplesPerPixel" -ln "HdPrmanLoaderRendererPlugin__convergedSamplesPerPixel" 
		-at "long";
	addAttr -ci true -sn "HdPrmanLoaderRendererPlugin__convergedVariance" -ln "HdPrmanLoaderRendererPlugin__convergedVariance" 
		-dv 0.014999999664723873 -at "float";
	addAttr -ci true -sn "HdPrmanLoaderRendererPlugin__disableMotionBlur" -ln "HdPrmanLoaderRendererPlugin__disableMotionBlur" 
		-min 0 -max 1 -at "bool";
	addAttr -ci true -h true -sn "dss" -ln "defaultSurfaceShader" -dt "string";
	setAttr ".ren" -type "string" "arnold";
	setAttr ".dss" -type "string" "openPBR_shader1";
select -ne :defaultResolution;
	setAttr ".pa" 1;
select -ne :defaultColorMgtGlobals;
	setAttr ".cfe" yes;
	setAttr ".cfp" -type "string" "<MAYA_RESOURCES>/OCIO-configs/Maya2022-default/config.ocio";
	setAttr ".vtn" -type "string" "ACES 1.0 SDR-video (sRGB)";
	setAttr ".vn" -type "string" "ACES 1.0 SDR-video";
	setAttr ".dn" -type "string" "sRGB";
	setAttr ".wsn" -type "string" "ACEScg";
	setAttr ".otn" -type "string" "ACES 1.0 SDR-video (sRGB)";
	setAttr ".potn" -type "string" "ACES 1.0 SDR-video (sRGB)";
select -ne :hardwareRenderGlobals;
	setAttr ".ctrs" 256;
	setAttr ".btrs" 512;
connectAttr "imagePlaneShape1.msg" ":perspShape.ip" -na;
connectAttr ":defaultColorMgtGlobals.cme" "imagePlaneShape1.cme";
connectAttr ":defaultColorMgtGlobals.cfe" "imagePlaneShape1.cmcf";
connectAttr ":defaultColorMgtGlobals.cfp" "imagePlaneShape1.cmcp";
connectAttr ":defaultColorMgtGlobals.wsn" "imagePlaneShape1.ws";
connectAttr "timeToUnitConversion1.o" "imagePlaneShape1.fe";
connectAttr "polySphere1.out" "pSphereShape1.i";
relationship "link" ":lightLinker1" ":initialShadingGroup.message" ":defaultLightSet.message";
relationship "link" ":lightLinker1" ":initialParticleSE.message" ":defaultLightSet.message";
relationship "shadowLink" ":lightLinker1" ":initialShadingGroup.message" ":defaultLightSet.message";
relationship "shadowLink" ":lightLinker1" ":initialParticleSE.message" ":defaultLightSet.message";
connectAttr "layerManager.dli[0]" "defaultLayer.id";
connectAttr "renderLayerManager.rlmi[0]" "defaultRenderLayer.rlid";
connectAttr ":time1.o" "timeToUnitConversion1.i";
connectAttr "defaultRenderLayer.msg" ":defaultRenderingList1.r" -na;
connectAttr "pSphereShape1.iog" ":initialShadingGroup.dsm" -na;
// End of imagePlane.ma
