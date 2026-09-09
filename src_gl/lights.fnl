;; lighting test for the lit4/lit8 pipelines: one sphere prim list under
;; a global ambient, up to 8 coloured directional lights and one specular.
;;
;;   ./xtc -script lights.fnl [-model FILE] [-lights N] [-shininess S]
;;         [-intensity I] [-nospin] [-noaxes] [-shot FILE] [-frames N]
;;
;; -model draws that (y-up, e.g. ../monkey.obj) instead of the sphere.
;; -lights picks the pipeline: up to 4 lights use lit4, more use lit8.
;; light 0 is white and the specular light, the others go round the hue
;; wheel, alternately from above and below.  the lights stay put in world
;; space while the sphere turns, so the object-space light matrices get
;; exercised.  in batch mode (-shot) the sphere stands still.

(global time 0)
(global cam nil)
(global sphere nil)
(global mdl nil)
(global material nil)
(global linemat nil)
(global lights [])

;; panel state
(global ui {:nlights 4 :intensity nil :shininess 40 :specular true
            :spin true :dirs true :axes true})

(local colors [(vec4 1 1 1 1) (vec4 1 0.2 0.2 1) (vec4 0.2 1 0.2 1) (vec4 0.3 0.4 1 1)
               (vec4 1 1 0.2 1) (vec4 1 0.2 1 1) (vec4 0.2 1 1 1) (vec4 1 0.6 0.2 1)])

(fn parse-args []
  (let [opts {:frames 3}]
    (var i 1)
    (while (<= i (length arg))
      (let [a (. arg i)]
        (if (= a "-model") (do (set opts.model (. arg (+ i 1))) (set i (+ i 1)))
            (= a "-lights") (do (set ui.nlights (tonumber (. arg (+ i 1)))) (set i (+ i 1)))
            (= a "-shininess") (do (set ui.shininess (tonumber (. arg (+ i 1)))) (set i (+ i 1)))
            (= a "-intensity") (do (set ui.intensity (tonumber (. arg (+ i 1)))) (set i (+ i 1)))
            (= a "-shot") (do (set opts.shot (. arg (+ i 1))) (set i (+ i 1)))
            (= a "-frames") (do (set opts.frames (tonumber (. arg (+ i 1)))) (set i (+ i 1)))
            (= a "-script") (set i (+ i 1))
            (= a "-nospin") (set ui.spin false)
            (= a "-noaxes") (set ui.axes false)))
      (set i (+ i 1)))
    ;; 8 lights at once saturate quickly, so dim them unless told otherwise
    (when (not ui.intensity)
      (set ui.intensity (if (<= ui.nlights 4) 0.6 0.35)))
    opts))

(fn scale4 [c s]
  (vec4 (* c.x s) (* c.y s) (* c.z s) c.w))

;; where light i (1-based) shines from
(fn light-from [i]
  (let [a (* (/ (- i 1) 8) 2 math.pi)
        z (if (= (% i 2) 1) 0.8 -0.5)]
    (: (vec3 (math.cos a) (math.sin a) z) :normalized)))

(fn sphere-vertex [theta phi]
  (let [x (* (math.sin theta) (math.cos phi))
        y (* (math.sin theta) (math.sin phi))
        z (math.cos theta)]
    (xtcNormal x y z)
    (xtcTexCoord (/ phi (* 2 math.pi)) (/ theta math.pi))
    (xtcVertex x y z)))

;; the single prim list: a unit sphere as a triangle list
(fn build-sphere []
  (let [pl (xtcCreatePrimList)
        nh 48
        nv 24]
    (xtcStartList pl)
    (xtcBegin _G.XTC_TRILIST)
    (xtcColor 255 255 255 255)
    (for [i 0 (- nv 1)]
      (let [t1 (* math.pi (/ i nv))
            t2 (* math.pi (/ (+ i 1) nv))]
        (for [j 0 (- nh 1)]
          (let [p1 (* 2 math.pi (/ j nh))
                p2 (* 2 math.pi (/ (+ j 1) nh))]
            (sphere-vertex t1 p1) (sphere-vertex t2 p1) (sphere-vertex t2 p2)
            (sphere-vertex t1 p1) (sphere-vertex t2 p2) (sphere-vertex t1 p2)))))
    (xtcEnd)
    (xtcEndList)
    pl))

(fn set-lights []
  (xtcSetAmbient 30 30 30)
  (for [i 1 8]
    (let [l (. lights i)
          from (light-from i)]
      (set l.enabled (if (<= i ui.nlights) 1 0))
      (set l.type _G.XTC_LIGHT_DIRECT)
      (set l.color (scale4 (. colors i) ui.intensity))
      (set l.specColor (if (and ui.specular (= i 1)) (vec4 1 1 1 1) (vec4 0 0 0 1)))
      (set l.direction (- from))
      (xtcSetLight (- i 1) l))))

(fn draw-lines []
  (xtcSetPipeline defaultPipeline)
  (xtcSetTexture nil)
  (xtcSetMaterial linemat)
  (xtcBegin _G.XTC_LINELIST)
  (when ui.axes
    (xtcColor 255 0 0 255) (xtcVertex 0 0 0) (xtcVertex 1.5 0 0)
    (xtcColor 0 255 0 255) (xtcVertex 0 0 0) (xtcVertex 0 1.5 0)
    (xtcColor 0 0 255 255) (xtcVertex 0 0 0) (xtcVertex 0 0 1.5))
  ;; a stub pointing at each light, in its colour, just outside the sphere
  (when ui.dirs
    (for [i 1 ui.nlights]
      (let [c (. colors i)
            f (light-from i)]
        (xtcColor (math.floor (* c.x 255)) (math.floor (* c.y 255)) (math.floor (* c.z 255)) 255)
        (xtcVertex (* f.x 1.1) (* f.y 1.1) (* f.z 1.1))
        (xtcVertex (* f.x 1.6) (* f.y 1.6) (* f.z 1.6)))))
  (xtcEnd))

(fn init []
  (set _G.opts (parse-args))

  ;; a blinn-style material for the sphere...
  (let [m (xtcMaterial)]
    (set m.colorSelector (vec4 0 0 0 0))
    (set m.ambient (vec4 1 1 1 1))
    (set m.diffuse (vec4 0.9 0.9 0.9 1))
    (set m.specular (vec4 1 1 1 1))
    (set m.emissive (vec4 0 0 0 1))
    (set material m))
  ;; ...and an unlit one for the lines: emissive from the vertex colour
  (let [m (xtcMaterial)]
    (set m.colorSelector (vec4 0 0 0 1))
    (set m.ambient (vec4 0 0 0 1))
    (set m.diffuse (vec4 0 0 0 1))
    (set linemat m))

  (for [i 1 8]
    (tset lights i (xtcLight)))

  ;; the sphere, or a y-up model scaled to about the sphere's size
  (if _G.opts.model
      (let [scale (mkmat4 [[0.75 0 0 0] [0 0.75 0 0] [0 0 0.75 0] [0 0 0 1]])]
        ;; y-up to z-up, then turned round so the face looks at the camera
        (set mdl (importScene _G.opts.model (* (_G.rotZ math.pi) (* (_G.rotX (/ math.pi 2)) scale)))))
      (set sphere (build-sphere)))

  (set cam (Camera))
  (set cam.position (vec3 3 -4 2.2))
  (set cam.target (vec3 0 0 0))
  (set cam.up (vec3 0 0 1))
  (set time 0))

(fn draw []
  (let [io (imguiIO)
        aspect (/ io.DisplaySize.x io.DisplaySize.y)
        spin (and ui.spin (not _G.opts.shot))]
    (when spin
      (set time (+ time io.DeltaTime)))

    (set cam.aspect aspect)
    (set cam.fov 50)
    (cam:process)
    (xtcSetProjectionMatrix (cam:getProjMat))
    (xtcSetViewMatrix (cam:getViewMat))
    (xtcEnable _G.XTC_DEPTH_TEST)
    (xtcDisable _G.XTC_BLEND)

    (set-lights)

    (xtcSetWorldMatrix (mat4 1))
    (draw-lines)

    ;; the sphere turns, the lights don't
    (xtcSetWorldMatrix (* (_G.rotZ (* 0.4 time)) (_G.rotX 0.4)))
    (set material.shininess ui.shininess)
    (xtcSetMaterial material)
    (xtcSetTexture nil)
    (let [pipe (if (<= ui.nlights 4) lit4Pipeline lit8Pipeline)]
      (xtcSetPipeline pipe)
      (if mdl
          (do (mdl:setMaterial material)
              (mdl:draw 0 pipe))
          (xtcPrimListDraw sphere))))

  ;; batch mode: screenshot and leave
  (when _G.opts.shot
    (set _G.opts.frames (- _G.opts.frames 1))
    (when (<= _G.opts.frames 0)
      (screenshot _G.opts.shot)
      (quit))))

(fn gui []
  (when (imguiBegin "Lights")
    (imguiText (.. "pipeline: " (if (<= ui.nlights 4) "lit4" "lit8")))
    (set ui.nlights (imguiSliderInt "Lights" ui.nlights 0 8))
    (set ui.intensity (imguiSliderFloat "Intensity" ui.intensity 0 1))
    (set ui.shininess (imguiSliderFloat "Shininess" ui.shininess 0 128))
    (set ui.specular (imguiCheckbox "Specular" ui.specular))
    (imguiSameLine)
    (set ui.spin (imguiCheckbox "Spin" ui.spin))
    (set ui.dirs (imguiCheckbox "Light stubs" ui.dirs))
    (imguiSameLine)
    (set ui.axes (imguiCheckbox "Axes" ui.axes))
    (when (imguiButton "Screenshot")
      (screenshot "shot.png")))
  (imguiEnd))

(set _G.init init)
(set _G.draw draw)
(set _G.gui gui)
