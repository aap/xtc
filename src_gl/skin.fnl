;; the skinning test subject, the same tube as the PS2's tube scene: a
;; tube along z with 6 bones spaced along it, every vertex weighted
;; between the two bones around it and coloured like its weights, the
;; joints waving.  left: vertex colours only, right: lit.
;;
;;   ./xtc -script skin.fnl [-nospin] [-shot FILE] [-frames N]

(global time 0)
(global cam nil)
(global tube nil)
(global colorsonly nil)
(global lit nil)
(global linemat nil)
(global ui {:spin true})

(local BONES 6)
(local RINGS 5)          ; per bone segment
(local SIDES 16)
(local LEN 3)
(local RADIUS 0.4)
(local SPACING (/ LEN (- BONES 1)))
(local colors [[255 60 60] [255 200 40] [60 220 60] [60 200 255] [90 90 255] [240 80 240]])

(fn parse-args []
  (let [opts {:frames 3}]
    (var i 1)
    (while (<= i (length arg))
      (let [a (. arg i)]
        (if (= a "-shot") (do (set opts.shot (. arg (+ i 1))) (set i (+ i 1)))
            (= a "-frames") (do (set opts.frames (tonumber (. arg (+ i 1)))) (set i (+ i 1)))
            (= a "-script") (set i (+ i 1))
            (= a "-nospin") (set ui.spin false)))
      (set i (+ i 1)))
    opts))

(fn translate [x y z]
  (mkmat4 [[1 0 0 x] [0 1 0 y] [0 0 1 z] [0 0 0 1]]))

(fn rotY [phi]
  (let [c (math.cos phi) s (math.sin phi)]
    (mkmat4 [[c 0 s 0] [0 1 0 0] [(- s) 0 c 0] [0 0 0 1]])))

(fn tube-vertex [ring side]
  (var t (/ ring RINGS))
  (var b (math.floor t))
  (when (> b (- BONES 2)) (set b (- BONES 2)))
  (set t (- t b))
  (let [z (+ (/ LEN -2) (* (+ b t) SPACING))
        phi (* 2 math.pi (/ side SIDES))
        c (math.cos phi)
        s (math.sin phi)
        c0 (. colors (+ b 1))
        c1 (. colors (+ b 2))]
    (xtcColor (math.floor (+ (* (. c0 1) (- 1 t)) (* (. c1 1) t)))
              (math.floor (+ (* (. c0 2) (- 1 t)) (* (. c1 2) t)))
              (math.floor (+ (* (. c0 3) (- 1 t)) (* (. c1 3) t))) 255)
    (xtcNormal c s 0)
    (xtcTexCoord (/ side SIDES) t)
    (xtcIndices b (+ b 1) 0 0)
    (xtcWeights (- 1 t) t 0 0)
    (xtcVertex (* RADIUS c) (* RADIUS s) z)))

(fn build-tube []
  (let [pl (xtcCreatePrimList)
        nrings (* (- BONES 1) RINGS)]
    (xtcStartList pl)
    (xtcSetPipeline skinPipeline)
    (xtcBegin _G.XTC_TRILIST)
    (for [r 0 (- nrings 1)]
      (for [s 0 (- SIDES 1)]
        (let [s1 (% (+ s 1) SIDES)]
          (tube-vertex r s) (tube-vertex (+ r 1) s) (tube-vertex (+ r 1) s1)
          (tube-vertex r s) (tube-vertex (+ r 1) s1) (tube-vertex r s1))))
    (xtcEnd)
    (xtcEndList)
    pl))

;; the skinning matrices, exactly as tubeBones() on the PS2
(fn bones [t]
  (var m (translate 0 0 (/ LEN -2)))
  (let [out []]
    (for [b 0 (- BONES 1)]
      (let [pivot (+ (/ LEN -2) (* b SPACING))
            a (* 0.3 (math.sin (+ (* 2 t) (* 0.9 b))))]
        (when (> b 0) (set m (* m (translate 0 0 SPACING))))
        (set m (* m (if (= (% b 2) 1) (rotY a) (_G.rotX a))))
        (tset out (+ b 1) (* m (translate 0 0 (- pivot))))))
    out))

(fn draw-lines []
  (xtcSetPipeline defaultPipeline)
  (xtcSetTexture nil)
  (xtcSetStdMaterial linemat)
  (xtcSetColorMaterial _G.XTC_EMISSIVE)
  (xtcBegin _G.XTC_LINELIST)
    (xtcColor 255 0 0 255) (xtcVertex 0 0 0) (xtcVertex 1.5 0 0)
    (xtcColor 0 255 0 255) (xtcVertex 0 0 0) (xtcVertex 0 1.5 0)
    (xtcColor 0 0 255 255) (xtcVertex 0 0 0) (xtcVertex 0 0 1.5)
  (xtcEnd))

(fn init []
  (set _G.opts (parse-args))
  (let [m (xtcStdMaterial)]
    (set m.ambient (vec4 0 0 0 1))
    (set m.diffuse (vec4 0 0 0 1))
    (set colorsonly m))
  (let [m (xtcStdMaterial)]
    (set m.ambient (vec4 1 1 1 1))
    (set m.diffuse (vec4 1 1 1 1))
    (set lit m))
  (let [m (xtcStdMaterial)]
    (set m.ambient (vec4 0 0 0 1))
    (set m.diffuse (vec4 0 0 0 1))
    (set linemat m))
  (set tube (build-tube))

  (xtcSetAmbient 0.2 0.2 0.2)
  (let [l (xtcLight)]
    (set l.enabled 1)
    (set l.type _G.XTC_LIGHT_DIRECT)
    (set l.color (vec4 0.8 0.8 0.8 1))
    (set l.specColor (vec4 0 0 0 1))
    (set l.direction (: (vec3 -1 1 -1) :normalized))
    (xtcSetLight 0 l))

  (set cam (Camera))
  (set cam.position (vec3 5 -6.5 3))
  (set cam.target (vec3 0 0 0))
  (set cam.up (vec3 0 0 1))
  ;; a fixed pose in batch mode
  (set time (if _G.opts.shot 1 0)))

(fn draw []
  (let [io (imguiIO)
        aspect (/ io.DisplaySize.x io.DisplaySize.y)]
    (when (and ui.spin (not _G.opts.shot))
      (set time (+ time io.DeltaTime)))
    (set cam.aspect aspect)
    (set cam.fov 50)
    (cam:process)
    (xtcSetProjectionMatrix (cam:getProjMat))
    (xtcSetViewMatrix (cam:getViewMat))
    (xtcEnable _G.XTC_DEPTH_TEST)
    (xtcDisable _G.XTC_BLEND)

    (xtcSetWorldMatrix (mat4 1))
    (draw-lines)

    (xtcSetBoneMatrices (bones time))
    (xtcSetTexture nil)
    (xtcSetPipeline skinPipeline)

    (xtcSetWorldMatrix (translate -1.3 0 0))
    (xtcSetStdMaterial colorsonly)
    (xtcSetColorMaterial _G.XTC_EMISSIVE)
    (xtcPrimListDraw tube)

    (xtcSetWorldMatrix (translate 1.3 0 0))
    (xtcSetStdMaterial lit)
    (xtcSetColorMaterial 0)
    (xtcPrimListDraw tube))

  (when _G.opts.shot
    (set _G.opts.frames (- _G.opts.frames 1))
    (when (<= _G.opts.frames 0)
      (screenshot _G.opts.shot)
      (quit))))

(fn gui []
  (when (imguiBegin "Skin")
    (imguiText "6 bones along z, 2 weights per vertex")
    (set ui.spin (imguiCheckbox "Wave" ui.spin))
    (when (imguiButton "Screenshot")
      (screenshot "shot.png")))
  (imguiEnd))

(set _G.init init)
(set _G.draw draw)
(set _G.gui gui)
