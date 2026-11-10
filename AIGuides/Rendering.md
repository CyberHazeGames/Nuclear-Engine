# Materials and rendering

Source inspected on 2026-11-09. These are implementation notes; sample intent is
not evidence that every rendering option runs correctly. See
[BuildAndValidation.md](BuildAndValidation.md) for validation and local content.

## Ownership and responsibilities

| Owner | Responsibilities |
| --- | --- |
| [GraphicsModule](../Nuclear.Engine/Source/Graphics/GraphicsModule.cpp) | Diligent device/context/swap chain, GPU uploads, shader compilation and resource classification |
| [RenderingModule](../Nuclear.Engine/Source/Rendering/RenderingModule.cpp) | Shared camera and 100-matrix animation buffers, final color/depth targets, fullscreen scene presentation |
| [Shader](../Nuclear.Engine/include/Assets/Shader.h) | Shader build description, material reflection and ShaderPipeline |
| [Material](../Nuclear.Engine/Source/Assets/Material.cpp) | Borrowed shader/textures, source texture sets and cached binding slots |
| [MeshComponent](../Nuclear.Engine/Source/Components/MeshComponent.cpp) | Borrowed mesh/material/animator, visibility, feature switches and selected pipeline variant |
| [RenderSystem](../Nuclear.Engine/Source/Systems/RenderSystem.cpp) | Light lists/buffers, shader baking, mesh ordering, active-camera iteration and ordered render passes |
| [GeometryPass](../Nuclear.Engine/Source/Rendering/RenderPasses/GeometryPass.cpp) | Owns forward/deferred path objects and selects one for each mesh's active variant |

Library-map assets are borrowed, not caller-owned. Sample-local materials and
animators must outlive components that reference them. RenderSystem borrows pass
pointers added by the client; its pass list does not own them.

## Material texture sets

A Material contains one shader reference and several MaterialTextureSet objects.
Assimp collects a model's source material slots into this one library material;
each submesh's `data.TexSetIndex` selects its set. This is not one shader/material
pointer per submesh. Preserve slot indices when changing import or assignment.
Untextured source slots are retained so they cannot inherit a textured slot.

Each MaterialTexture stores a texture pointer and TextureUsageType. `Diffuse` and
`Albedo` alias value 0; `Specular` and `Metallic` alias value 1. Usage matching does
not distinguish those physical meanings. Roughness, Normal and AO have their own
roles. Material has no general scalar/vector material-parameter buffer.

`Material::Create(shader)` matches each source set to the shader's reflected
material textures and caches SRB variable indices. Missing roles use reflected
fallbacks, currently black by default. That generic default is not appropriate
for every role; the editor supplies white albedo/AO, black metallic, a flat normal
and grey roughness before creation. A material with no sets does not automatically
get a complete default set from Create.

`RenderingPath::DrawMesh` binds each submesh's texture set, vertex/index buffers
and submits one DrawIndexed call. Binding mutates the selected variant's shared
SRB immediately before committing resources. It does not allocate a private GPU
material binding per asset.

`Material::BindTexSet` uses set zero when a submesh requests a slot beyond the
assigned material's cached sets. This lets the editor assign an independent material
to geometry with more source slots; valid matching indices retain their original sets.

Repeated Create calls append cached sets without clearing them. `SetShader` only
changes the shader pointer/ID; it does not rebuild cached bindings. Editing
`GetTextures()` after creation also does not rebuild them. Avoid treating these
APIs as automatic material rebinding or hot reload.

## Shader descriptions, reflection and variants

Editor material viewers and thumbnails use a private `MaterialPreviewRenderer` shader
copy. It clears feature switches and bakes one static forward variant against private
camera and two-light buffers, using the preview camera's target formats. Private material
bindings copy the selected source texture set and fill missing standard maps. Preview
rendering does not rebake the library shader or register preview entities in the scene.
Deferred-only shaders, shadows, IBL and animated variants are excluded from these previews.

[ShaderParser](../Nuclear.Engine/Source/Parsers/ShaderParser.cpp) reads TOML-style
`.NuclearShader` files. Descriptions specify PSO formats/state, inline or external
HLSL, forward/G-buffer/deferred PSOs and boolean variant switches. `.NEShader` is
the exported binary form; `.NEMaterial` serializes shader/texture UUID references
and cached material bindings. Changing reflection or bindings can invalidate
assumptions in existing exported assets; inspect both load and export paths.

Resource names are part of the C++/HLSL contract:

| Prefix | Meaning |
| --- | --- |
| `NEMat_` | Dynamic material textures, usage inferred from the suffix |
| `NEStatic_` | Static PSO bindings such as camera, animation and light buffers |
| `NEMutable_`, `NEDynamic_` | Explicit mutable/dynamic resource classes |
| `NE_RT_` | Dynamic render-target inputs |
| `NEIBL_` | Environment irradiance, prefilter and BRDF lookup inputs |
| `NE_FX_` | Rendering effects, including SSAO |
| `NE_ShadowMap_` | Shadow textures |

Material reflection compiles a representative pixel shader; variant reflection
separately discovers IBL, shadow and effect inputs. Material binding assumes its
cached indices agree with the actual variant SRB layout, including the deferred
G-buffer SRB. Resource ordering changes need checking against both.

[ShaderPipeline](../Nuclear.Engine/Source/Graphics/ShaderPipeline.cpp) enumerates
the power set of switches and uses a sum of enabled switch-name hashes as its
variant key. Bake creates GPU objects for the combinations through the variant
factory. A deferred variant has a geometry PSO/SRB and a fullscreen lighting
PSO/SRB; its parent ShaderPipeline owns the shared GBuffer. Variant addresses are
kept stable across rebakes, but GPU objects and reflection are recreated.

The [PBR description](../Assets/NuclearEngine/Shaders/PBR/PBR.NuclearShader)
declares NE_DEFFERED, NE_SHADOWS, NE_ANIMATION, NE_SSAO and IBL_ENABLED: 32
combinations before import exclusions. These are compile-time variants selected
at runtime, not arbitrary shader branches driven by a material uniform.

RenderSystem::Bake snapshots light counts into HLSL defines, binds shared camera,
animation and light buffers, and supplies environment textures through its IBL
context. Structural light changes require an appropriate rebake. Do not assume
the NeedsBaking status automatically handles scene changes.

## Frame and pass flow

Clients explicitly drive Scene::Update or RenderSystem::Update. The engine's
client callback does not automatically update every scene system. SampleBase
creates systems/camera, and its derived samples call Scene::Update from Render.

RenderSystem updates light positions, updates mesh variants and sorts MeshComponent
storage by the variant's numeric render queue. Queue 0 skips geometry; positive
queue values group pipeline variants. This loop does not perform frustum culling,
instanced batching or transparency distance sorting.

For each active CameraComponent it updates camera-position-dependent light data,
clears camera color/depth, builds FrameRenderData and executes its pass vector.
AddRenderPass preserves insertion order; no render-graph scheduler infers resource
dependencies. Scene system order is separate and uses unordered-map iteration.

- ForwardRenderingPath shades meshes directly into camera color using material,
  light and optional IBL/shadow inputs.
- DefferedRenderingPath writes GBuffer targets and camera depth, recording the
  variants used. PBR stores position, normal, albedo/metallic and roughness/AO.
- AmbientOcclusionPass extracts and blurs SSAO from deferred buffers. PBR consumes
  this result only in its deferred SSAO variant; the pass is not a forward AO path.
- DefferedPass draws fullscreen lighting for recorded variants, binding GBuffer,
  IBL and optional AO inputs.
- PostProcessingPass draws an optional skybox, performs optional bloom extraction
  and blur, then draws the selected post-effect combination and copies to camera
  color. Its pipeline is baked separately from the scene shaders.

With mRenderToScreen enabled, the main camera color is copied to RenderingModule's
final target and drawn to the swap chain. Other active cameras render their own
targets. The editor disables this screen copy and samples its camera directly in
retained UI. Engine UI, optional ImGui and Present follow client rendering.

## Examples and limitations

| Example | Read for |
| --- | --- |
| [Sample1](../Samples/Sample1.h) | Manual texture sets, Create(shader), imported materials, BlinnPhong/DiffuseOnly and animator attachment |
| [Sample2](../Samples/Sample2.h) | Geometry -> SSAO -> Deffered -> PostFX registration; PBR material switching and per-mesh feature toggles; HDR/IBL capture |
| [Sample4](../Samples/Sample4.h) | Multiple active cameras, independent color/depth targets and viewing the second texture |
| [Sample5](../Samples/Sample5.h) | Prepared asynchronous assets and readiness gates |

Sample2's NE_DEFFERED/NE_SSAO/IBL_ENABLED checkboxes call MeshComponent's
SetVariantSwitch. SetMaterial changes the material reference and marks pipeline
selection dirty. Standalone ForwardRP members in samples do not drive rendering;
GeometryPass uses its own path instances.

Source-observed constraints, not runtime-verified bug reproductions:

- MeshComponent currently resets its deferred request when a deferred pass exists
  and shadow receiving when a shadow pass exists. Custom switches are applied
  afterward, allowing Sample2's deferred checkbox to override that assignment.
- Deferred path startup clears its parent's shared GBuffer and camera depth on a
  variant change. Several deferred variants of one shader can erase earlier
  geometry before lighting. Mixed forward/deferred composition also needs care.
- Mesh readiness checks mesh/material Created states, not every dependent texture.
  MaterialLoadTask restores serialized bindings and marks Created independently
  of texture upload completion. Non-null texture views may be placeholders.
- Shader PSO formats must match camera targets. PBR descriptions specify RGBA8
  sRGB, while RenderingModule's final target is RGBA16 float; do not assume every
  sample's target setup matches. The editor explicitly matches its camera format.
- PBR currently applies a manual albedo power transform. Check imported texture
  sRGB formats before changing color handling or judging rendering numerically.

See [Editor.md](Editor.md#assets-and-diagnostics) for automatic imported-material
assignment and the separate neutral mesh-document/thumbnail preview limitations.
