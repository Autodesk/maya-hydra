# Hydra for Maya (Technology Preview)

The _Hydra for Maya_ project creates a Maya plugin that replaces the main Maya viewport with a Hydra viewer. _Hydra for Maya_ is developed and maintained by Autodesk. The project and this documentation are a work-in-progress and under active development. The contents of this repository are fully open source and open to contributions under the [Apache license](./doc/LICENSE.md)!

Hydra is the rendering API included with [Pixar's USD](http://openusd.org/).

## Motivation

The goal for _Hydra for Maya_ is to introduce Hydra as an open source viewport framework for Maya to extend the viewport render engine through Hydra render delegates. _Hydra for Maya_ uses the previous Maya to Hydra (MtoH) code, which is part of MayaUSD, as a foundation to build on. You can find more details on what changed from MtoH [here](./doc/mayaHydraDetails.md). 
With _Hydra for Maya_ we are fully leveraging the new [SceneIndex API (aka Hydra v2)](https://openusd.org/release/api/class_hd_scene_index_base.html#details) for more flexibility and customizability over the Hydra scene graph. Therefore the HdSceneDelegate is *not supported* by this plugin.

_Hydra for Maya_ is currently a Technology Preview; we are just laying the foundation and there is still work ahead. As the plugin evolves and the Hydra technology matures, you can expect changes to API and to face limitations.

## What's it good for?

This project allows you to use Hydra (the pluggable USD render ecosystem)
and Storm (the OpenGL renderer for Hydra, bundled with USD), as an
alternative to Viewport 2.0.

Using Hydra has big benefits for offline renderers: any renderer that
implements a Hydra render delegate can now have an interactive render viewport
in Maya, along with support for render proxies.

As an example, when paired with
[arnold-usd](https://github.com/Autodesk/arnold-usd) (Arnold's USD plugin +
render delegate), it provides an Arnold render of the viewport, where both maya
objects and USD objects (through proxies) can be modified interactively.

This could also be particularly useful for newer renderers, like Radeon
ProRender (which already has a
[render delegate](https://github.com/GPUOpen-LibrariesAndSDKs/RadeonProRenderUSD)),
or in-house renderers, as an easier means of implementing an interactive
render viewport.

What about HdStorm, Hydra's OpenGL renderer? Why would you want to use HdStorm
instead of "normal" Viewport 2.0, given that there are other methods for displaying
USD proxies in Viewport 2.0? Some potential reasons include: 

- Using HdStorm gives lighting and shading between Hydra-enabled applications:
  Maya, Katana, usdview, etc
- HdStorm is open source: you can add core features as you need them
- HdStorm is extensible: you can create plugins for custom objects, which then allows
  them to be displayed not just in Maya, but any Hydra-enabled application

## Known Limitations

- Limited Maya material support:
  - Maya Standard Surface materials are translated into the USD Preview Surface which has limitations in available parameters and does not match one to one (e.g. transparency and opacity)
  - Only direct texture inputs are supported for Maya materials. No other Hypershade nodes are supported (e.g. ramp or procedural textures)
- Hydra shading differs from Maya's Viewport 2.0
- Animation Ghosting has the wrong shading
- Backface Culling
- Isolate Select only functions with Maya nodes
- Following Maya node types are not rendered:
  - Bifrost
  - nParticles
  - Fluid
  - Blue Pencil
  - Maya's volume and ambient light
- Current limitations with USD:
  - Maya layers don't show effect
  - Viewport display modes like Xray, wireframe or default shading are not supported yet
  - Gprims currently don't cast shadows in Storm
  - No material bindings on GeomSubsets (Hydra v2 limitation)

## Limitations with Storm
- Screen space effects like depth of field and motion blur or Hardware Fog are not supported
- Limited shadow quality of lights
- Long loading times and low viewport performance with MaterialX materials with Storm


## Detailed Documentation

+ [Contributing](./doc/CONTRIBUTING.md)
+ [Building the mayaHydra.mll plugin](./doc/build.md)
+ [Coding guidelines](./doc/codingGuidelines.md)
+ [License](./doc/LICENSE.md)
+ [Plugin documentation](./README_DOC.md)
+ [Flow viewport toolkit documentation](./doc/flowViewportToolkit.md)