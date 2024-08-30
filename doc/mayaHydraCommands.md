# Maya command plugins used in MayaHydra

# MayaHydra Command Plugin

The `MayaHydra` command plugin provides a set of utilities for interacting with the Hydra rendering framework within Maya. Below are the available flags and their descriptions.

## Basic Usage

```bash
mayaHydra [flags]
```
Available Flags
```
-listDelegates / -ld:
```
Returns the names of available scene delegates.
```
-listRenderers / -lr:
```
Returns the names of available render delegates.
```
-listActiveRenderers / -lar:
```
Returns the names of render delegates that are in use in at least one viewport.

Renderer-Specific Commands
```
-renderer / -r [RENDERER]:
```
Specifies the renderer to target for the commands below.
```
-getRendererDisplayName / -gn:
```
Returns the display name for the given render delegate.
```
-createRenderGlobals / -crg:
```
Creates the render globals, optionally targeting a specific renderer.
```
-userDefaults / -ud:
```
A flag for -createRenderGlobals to restore user defaults on create.
```
-updateRenderGlobals / -urg [ATTRIBUTE]:
```
Forces the update of the render globals for the viewport, optionally targeting a specific renderer or setting.

Advanced / Debugging Flags
To access advanced and debugging flags, use the -verbose / -v flag.

Debug Flags
```
-listRenderIndex / -lri -r [RENDERER]:
```
Returns a list of all the rprims in the render index for the given render delegate.
```
-visibleOnly / -vo:
```
Affects the behavior of -listRenderIndex. If provided, only visible items in the render index are returned.
```
-sceneDelegateId / -sid [SCENE_DELEGATE] -r [RENDERER]:
```
Returns the path ID corresponding to the given render delegate and scene delegate pair.

# MayaHydra Versioning and Build Information Flags

The following flags are used to retrieve versioning and build information for the `MayaHydra` plugin. Each flag has both a short and a long form.

## Usage Example

To retrieve the full version of the `MayaHydra` plugin, use the following command:

```bash
mayaHydraBuildInfo [-flags]
```
## Version Information

- **`-mjv` / `-majorVersion`**:  
  Returns the major version number of the plugin.

- **`-mnv` / `-minorVersion`**:  
  Returns the minor version number of the plugin.

- **`-pv` / `-patchVersion`**:  
  Returns the patch version number of the plugin.

- **`-v` / `-version`**:  
  Returns the full version string of the plugin, which may include major, minor, and patch version numbers.

## Build Information 

This information is expected to be set by a parent build system that has access and/or generates the following information.

- **`-c` / `-cutIdentifier`**:  
  Returns the cut identifier associated with this build of the plugin.

- **`-bn` / `-buildNumber`**:  
  Returns the build number for the plugin, typically representing the incremental number assigned during the build process.

- **`-gc` / `-gitCommit`**:  
  Returns the Git commit hash that the build is based on, useful for tracing the exact source code used.

- **`-gb` / `-gitBranch`**:  
  Returns the Git branch name that the build was created from.

- **`-bd` / `-buildDate`**:  
  Returns the date on which the plugin was built.


### Summary
- **Version Information Flags**: Cover major, minor, and patch version details, as well as the full version string.
- **Build Information Flags**: Include cut identifier, build number, Git commit, Git branch, and build date.
- **Usage Examples**: Show how to retrieve specific pieces of information using the provided flags.

This Markdown document provides a clear and concise reference for users who need to access versioning and build information for the `MayaHydra` plugin.


