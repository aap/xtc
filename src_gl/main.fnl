;; xtc sketch: load a model (anything assimp reads, or .xm/.chk), draw it,
;; play its animations.
;;
;;   ./xtc [model] [-texpath DIR] [-yup|-zup] [-anim NAME] [-time T]
;;         [-save BASE] [-skeleton] [-wire] [-noaxes] [-shot FILE] [-frames N]
;;
;; -yup rotates the model to z-up on import (default for glTF/collada/fbx).
;; -save writes BASE.xm/.xan and the .chk versions after loading.
;; -shot writes a screenshot after -frames frames (default 3) and quits.

(global time 0)
(global dt 0)
(global material nil)
(global cam nil)
(global mdl nil)
(global anims nil)
(global player nil)
(global light nil)

(local DBG_NODES 1)
(local DBG_SKELETON 2)
(local DBG_WIRE 4)
(local DBG_POINTS 8)

;; panel state
(global ui {:anim 1 :names [] :playing true :speed 1
            :skeleton false :nodes false :wire false :points false
            :axes true :spin false})

(fn parse-args []
  (let [opts {:model "../samples/fox/fox.glb" :frames 3}]
    (var i 1)
    (while (<= i (length arg))
      (let [a (. arg i)]
        (if (= a "-anim") (do (set opts.anim (. arg (+ i 1))) (set i (+ i 1)))
            (= a "-time") (do (set opts.time (tonumber (. arg (+ i 1)))) (set i (+ i 1)))
            (= a "-shot") (do (set opts.shot (. arg (+ i 1))) (set i (+ i 1)))
            (= a "-frames") (do (set opts.frames (tonumber (. arg (+ i 1)))) (set i (+ i 1)))
            (= a "-save") (do (set opts.save (. arg (+ i 1))) (set i (+ i 1)))
            (= a "-texpath") (do (set opts.texpath (. arg (+ i 1))) (set i (+ i 1)))
            (= a "-skeleton") (set ui.skeleton true)
            (= a "-wire") (set ui.wire true)
            (= a "-noaxes") (set ui.axes false)
            (= a "-yup") (set opts.yup true)
            (= a "-zup") (set opts.yup false)
            (set opts.model a)))
      (set i (+ i 1)))
    opts))

(fn draw-axes [s]
  (xtcSetPipeline defaultPipeline)
  (xtcSetTexture nil)
  (xtcSetStdMaterial material)
  (xtcSetColorMaterial _G.XTC_EMISSIVE)

  (xtcBegin _G.XTC_LINELIST)
    (xtcColor 255 0 0 255)
    (xtcVertex 0 0 0)
    (xtcVertex s 0 0)

    (xtcColor 0 255 0 255)
    (xtcVertex 0 0 0)
    (xtcVertex 0 s 0)

    (xtcColor 0 0 255 255)
    (xtcVertex 0 0 0)
    (xtcVertex 0 0 s)
  (xtcEnd))

(fn file-exists? [path]
  (let [f (io.open path "r")]
    (when f (f:close) true)))

(fn ends-with? [s suffix]
  (= (string.sub s (- (length suffix))) suffix))

;; y-up formats get rotated into our z-up world on import,
;; so our own files are always z-up
(fn yup-format? [path]
  (or (ends-with? path ".glb") (ends-with? path ".gltf")
      (ends-with? path ".dae") (ends-with? path ".fbx")))

;; returns model and animation list (or nil)
(fn load-model [path yup]
  (if (ends-with? path ".xm")
      (let [base (string.sub path 1 -4)
            xan (.. base ".xan")]
        (values (loadXModel path) (when (file-exists? xan) (loadXAnimList xan))))
      (ends-with? path ".xm.chk")
      (let [base (string.sub path 1 -8)
            xan (.. base ".xan.chk")]
        (values (loadXModelChunk path) (when (file-exists? xan) (loadXAnimListChunk xan))))
      (importScene path (when yup (_G.rotX (/ math.pi 2))))))

(fn find-anim [names name]
  (var found nil)
  (each [i n (ipairs names)]
    (when (= n name) (set found i)))
  found)

(fn select-anim [i]
  (set ui.anim i)
  (player:setAnim (anims:get i)))

(fn init []
  (let [m (xtcStdMaterial)]
    (set m.ambient (vec4 0 0 0 1))
    (set m.diffuse (vec4 0 0 0 1))
    (set material m))

  (let [opts (parse-args)]
    (set _G.opts opts)
    (when (= opts.yup nil)
      (set opts.yup (yup-format? opts.model)))
    ;; textures are looked up by name, next to the model unless told otherwise
    (setTexPath (or opts.texpath
                    (if (ends-with? opts.model ".dff") "/u/aap/bits/3dmodels/gta3_textures"
                        (or (string.match opts.model "^(.*)/[^/]*$") "."))))
    (let [(m a) (load-model opts.model opts.yup)]
      (set mdl m)
      (set anims a))
    (when (ends-with? opts.model ".dff")
      (mdl:hideCarParts))
    ;; write out text and chunk versions
    (when opts.save
      (saveXModel mdl (.. opts.save ".xm"))
      (saveXModelChunk mdl (.. opts.save ".xm.chk"))
      (when anims
        (saveXAnimList anims (.. opts.save ".xan"))
        (saveXAnimListChunk anims (.. opts.save ".xan.chk"))))
    (let [info (mdl:info)]
      (print (: "%s: %d meshes, %d materials, %d bones, %d vertices, %d triangles"
                :format opts.model info.numMeshes info.numMaterials info.numBones
                info.numVertices info.numTriangles)))
    (when anims
      (set ui.names (anims:names))
      (print (.. (anims:count) " animations"))
      (set player (xAnimPlayer mdl))
      (select-anim (or (and opts.anim (find-anim ui.names opts.anim)) 1))
      (when opts.time
        (set ui.playing false)
        (player:setTime opts.time)))

    ;; frame the model
    (let [(center radius) (mdl:bounds)]
      (set cam (Camera))
      (set cam.position (+ center (* (: (vec3 4 -6 4) :normalized) (* radius 3))))
      (set cam.target center)
      (set cam.up (vec3 0 0 1))))

  (xtcSetAmbient (/ 100 255) (/ 100 255) (/ 100 255))
  (let [l (xtcLight)]
    (set l.enabled 1)
    (set l.type XTC_LIGHT_DIRECT)
    (set l.color (vec4 0.8 0.8 0.8 1))
    (set l.specColor (vec4 1 1 1 1))
    (set l.direction (: (vec3 -1 1 -1) :normalized))
    (xtcSetLight 0 l)
    (set light l))

  (set time 0))

(fn draw-flags []
  (+ (if ui.nodes DBG_NODES 0)
     (if ui.skeleton DBG_SKELETON 0)
     (if ui.wire DBG_WIRE 0)
     (if ui.points DBG_POINTS 0)))

(fn draw []
  (let [io (imguiIO)
        aspect (/ io.DisplaySize.x io.DisplaySize.y)]
    (set dt io.DeltaTime)
    (set time (+ time dt))

    ;; the light circles around, except in batch mode where we want
    ;; reproducible pictures
;    (let [phi (if _G.opts.shot 0 (* time 1.5))
    (let [phi 0
          ld (: (vec3 (math.cos phi) (math.sin phi) -1) :normalized)]
      (set light.direction ld)
      (xtcSetLight 0 light)

      (set cam.aspect aspect)
      (set cam.fov 60)
      (cam:process)
      (xtcSetProjectionMatrix (cam:getProjMat))
      (xtcSetViewMatrix (cam:getViewMat))
      (xtcSetWorldMatrix (mat4 1))

      (xtcEnable _G.XTC_DEPTH_TEST)
      (when ui.axes
        (draw-axes 1)

        (xtcSetPipeline defaultPipeline)
        (xtcSetTexture nil)
        (xtcSetStdMaterial material)
  (xtcSetColorMaterial _G.XTC_EMISSIVE)
        (xtcBegin _G.XTC_LINELIST)
          (xtcColor 255 255 255 255)
          (xtcVertex 0 0 0)
          (xtcVertex (- (* ld.x 3)) (- (* ld.y 3)) (- (* ld.z 3)))
        (xtcEnd)))

    (when player
      (when ui.playing
        (player:addTime (* dt ui.speed)))
      (player:apply))

    (xtcSetWorldMatrix (_G.rotZ (if ui.spin (* 0.3 time) 0)))

    (xtcEnable _G.XTC_BLEND)
    (xtcBlendFuncSrcDst _G.XTC_BLEND_SRCALPHA _G.XTC_BLEND_INVSRCALPHA)
    (mdl:draw (draw-flags)))

  ;; batch mode: screenshot and leave
  (when (and _G.opts.shot (> time 0))
    (set _G.opts.frames (- _G.opts.frames 1))
    (when (<= _G.opts.frames 0)
      (screenshot _G.opts.shot)
      (quit))))

(fn gui []
  (when (imguiBegin "Model")
    (let [info (mdl:info)]
      (imguiText (: "%d verts, %d tris, %d bones" :format
                    info.numVertices info.numTriangles info.numBones)))
    (set ui.axes (imguiCheckbox "Axes" ui.axes))
    (imguiSameLine)
    (set ui.spin (imguiCheckbox "Spin" ui.spin))
    (set ui.skeleton (imguiCheckbox "Skeleton" ui.skeleton))
    (imguiSameLine)
    (set ui.nodes (imguiCheckbox "Nodes" ui.nodes))
    (set ui.wire (imguiCheckbox "Wireframe" ui.wire))
    (imguiSameLine)
    (set ui.points (imguiCheckbox "Points" ui.points))
    (when (imguiButton "Screenshot")
      (screenshot "shot.png"))
    (when anims
      (imguiSeparator)
      (let [i (imguiCombo "Animation" ui.anim ui.names)]
        (when (not= i ui.anim)
          (select-anim i)))
      (set ui.playing (imguiCheckbox "Play" ui.playing))
      (imguiSameLine)
      (set ui.speed (imguiSliderFloat "Speed" ui.speed 0 3))
      (let [a (anims:get ui.anim)
            t (imguiSliderFloat "Time" (player:getTime) 0 (a:duration))]
        (when (not ui.playing)
          (player:setTime t)))))
  (imguiEnd))

(set _G.init init)
(set _G.draw draw)
(set _G.gui gui)
