(global time 0)
(global dt 0)
(global material nil)
(global cam nil)
(global mdl nil)
(global light nil)

(fn draw-axes [s]
  (xtcSetShader (xtcGetDefaultShader))
  (xtcSetTexture 0 nil)
  (xtcSetMaterial material)
  
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

(fn init []
  (let [m (xtcMaterial)]
    (set m.colorSelector (vec4 0 0 0 1))
    (set m.ambient (vec4 0 0 0 1))
    (set m.diffuse (vec4 0 0 0 1))
    (set material m))

  (set cam (Camera))
  (set cam.position (* (vec3 4 -6 4) 0.7))
  (set cam.target (vec3 0 0 0))
  (set cam.up (vec3 0 0 1))

  (setTexPath "/u/aap/3dmodels/gta3_textures")
  (set mdl (loadXModelChunk "kuruma.xm.chk"))

  (xtcSetAmbient 100 100 100)
  (let [l (xtcLight)]
    (set l.enabled 1)
    (set l.type XTC_LIGHT_DIRECT)
    (set l.color (vec4 0.8 0.8 0.8 1))
    (set l.specColor (vec4 1 1 1 1))
    (set l.direction (: (vec3 -1 1 -1) :normalized))
    (xtcSetLight 0 l)
    (set light l))

  (set time 0))

(fn draw []
  (let [io (imguiIO)
        aspect (/ io.DisplaySize.x io.DisplaySize.y)]
    (set dt io.DeltaTime)
    (set time (+ time dt))

    (let [phi (* time 1.5)
          ld (: (vec3 (math.cos phi) (math.sin phi) -1) :normalized)]
      (set light.direction ld)
      (xtcSetLight 0 light)

      (set cam.aspect aspect)
      (set cam.fov 81.3)
      (cam:process)
      (xtcSetProjectionMatrix (cam:getProjMat))
      (xtcSetViewMatrix (cam:getViewMat))
      (xtcSetWorldMatrix (_G.rotZ (* 0.2 phi)))

      (xtcEnable _G.XTC_DEPTH_TEST)
      (draw-axes 1)

      (xtcSetShader (xtcGetDefaultShader))
      (xtcSetTexture 0 nil)
      (xtcSetMaterial material)

      (xtcBegin _G.XTC_LINELIST)
        (xtcColor 255 255 255 255)
        (xtcVertex 0 0 0)
        (xtcVertex (- (* ld.x 3)) (- (* ld.y 3)) (- (* ld.z 3)))
      (xtcEnd))

    (xtcEnable _G.XTC_BLEND)
    (xtcBlendFuncSrcDst _G.XTC_BLEND_SRCALPHA _G.XTC_BLEND_INVSRCALPHA)
    (mdl:draw)))

(set _G.init init)
(set _G.draw draw)

;(each [k v (pairs _G)]
;  (print k (type v)))
