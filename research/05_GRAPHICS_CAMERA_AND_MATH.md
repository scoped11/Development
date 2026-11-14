# Graphics, camera geometry, transforms and timing

Sources V01–V05. All examples are owned test scenes or offline replays, not a competitive overlay implementation.

## 1. Spaces, conventions and projection

Local coordinates describe geometry relative to an object; world coordinates refer to scene origin; view coordinates are camera-relative; clip coordinates are homogeneous projection results; normalized device coordinates follow perspective division; viewport coordinates are pixels. Storage order (row/column major) is distinct from the use of row or column vectors. Units, up axis, handedness, matrix multiplication order, depth range and screen origin must be declared. Do not transfer one engine's conventions to another.

Column-vector convention: `p_clip = P * V * M * p_local`, homogeneous p=(x,y,z,1). M places the model; V maps world to camera; P projects. Row-vector conventions reverse multiplication order. After appropriate clipping, divide x,y,z by w. For a standard D3D-style viewport, x maps [-1,1] to [0,width], y maps to screen-down pixels, and NDC z uses [0,1]. Other APIs/projections may differ. Microsoft's transform and viewport material [V01] and DirectXMath projection API [V02] are useful references. Never divide by zero or assume a computed pixel implies visibility.

### Worked synthetic projection

Input clip `(0.5,-0.25,0.6,1.0)`, viewport `(0,0,1920,1080)`, screen origin upper-left, clip depth [0,1]. NDC `(0.5,-0.25,0.6)`. Expected screen `(1440,675)`; if viewport origin becomes `(100,50)` expect `(1540,725)`. Formula x=originX+(ndcX+1)*width/2, y=originY+(1-ndcY)*height/2. An invalid w or non-finite input yields invalid sample. Out-of-frustum samples must be classified according to the actual clip convention rather than silently clamped to display bounds.

## 2. Camera pose, FOV and hierarchy

Camera pose is translation and rotation; view transform is generally inverse camera pose for a rigid camera. Projection can be perspective, orthographic, asymmetric, jittered or reversed-Z. Vertical vs horizontal FOV are not interchangeable. For symmetric perspective, `tan(hfov/2) = aspect * tan(vfov/2)`; vertical 60 degrees at 16:9 yields approximately 91.5 degrees horizontal. Orthographic projection has spatial extent rather than perspective foreshortening. Explicitly store degrees/radians, viewport dimensions and projection type.

Quaternion component ordering varies. Normalize nonzero finite rotations [V03]; q and -q describe the same rotation. Euler angles can have discontinuities and gimbal lock. A non-rigid matrix with scale/shear is not automatically a rotation. Parent/local/global transforms differ. An animation hierarchy may store local bone poses, cached world transforms and skinning matrices. Skinning uses weights and inverse-bind transforms, so a bone's translation is not a rendered vertex. Render interpolation and physics time may differ. Verify on a simple owned skeleton with a known parent transform rather than borrowing offsets or layouts.

## 3. Pipeline and presentation

Graphics processing can involve vertex transformation, primitive assembly, rasterization, shader evaluation, depth tests, blending and composition. Depth buffers represent rendered depth, not semantic visibility. LOD, occlusion culling, transparencies and temporal effects can change the observed image. A screenshot does not reveal off-screen state.

DXGI's Present submits a rendered image for presentation [V04]. CPU submission, GPU completion and physical display are different events. Synchronization, buffering, compositor and refresh schedule introduce delay. Frame generation/upscaling can produce a display frame without a new simulation tick. Vulkan also defines explicit synchronization and presentation; V05 is a further-reading pointer because fetch failed. Do not infer graphics timing from nominal refresh alone.

| Method | Input | Legitimate use | Limitation |
|---|---|---|---|
| Engine debug draw | owned scene + supported API | developer diagnostics | engine-specific lifecycle |
| Offline renderer | authorized replay + camera | replay visualization | matching timestamps |
| Separate diagnostic window | IPC + viewport | QA dashboard | DPI and origin differences |
| Capture | authorized owned application | graphics investigation | may perturb timing |
| Display composition | consented video | production | no recovery of absent data |

Separate displays are not guaranteed invisible to capture. Capture may operate at different stages of composition; location does not establish permission or detection risk.

## 4. Synchronization and performance

Track producer tick, capture timestamp, parse time, render submission and display time separately. A camera from tick 103 with objects from tick 100 may cause apparent jitter. A complete older snapshot can be more accurate than a mixed new one. Include sample age. Use monotonic timestamps, bounded queues and explicit dropped-record counters. Example offline pipeline: read 2 ms + parse 1 ms + transform 0.5 ms + submit 1.5 ms = 5 ms serial CPU work; presentation latency is additional. Frame intervals 60 Hz=16.67 ms and 144 Hz=6.94 ms. Measure p50, p95, p99 and queue depth, not just average FPS.

| Symptom | Hypotheses to test |
|---|---|
| Center correct, edges wrong | FOV, aspect, viewport offset |
| Vertical reflection | y inversion, handedness |
| Shift after resize | viewport generation, DPI, letterbox |
| Jump at near plane | clip test, homogeneous w |
| Lagging positions | stale data, render interpolation |
| Low GPU utilization yet stutter | CPU stall, synchronization, backlog |
| Rotated scene | matrix order, quaternion layout, parent transform |

Test two resolutions, nonzero viewport origin, camera motion, near-plane crossing, NaN/zero inputs, orthographic and perspective views. Compare against known engine-projected results. No graphics tests executed here.
