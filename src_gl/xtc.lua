--
-- XTC
--

---- setTexPath(path)

-- xtceState
XTC_DEPTH_TEST = 0
XTC_ALPHA_TEST = 1
XTC_BLEND = 2
XTC_FOG = 3
XTC_TEXTURE = 4
XTC_CLIPPING = 5

---- xtcEnable(state)
---- xtcDisable(state)

-- xtcBlendFactor
XTC_BLEND_ZERO = 0
XTC_BLEND_ONE = 1
XTC_BLEND_SRCALPHA = 2
XTC_BLEND_INVSRCALPHA = 3
XTC_BLEND_DSTALPHA = 4
XTC_BLEND_INVDSTALPHA = 5

---- xtcBlendFuncSrcDst(src, dst)

-- xtcPrimType
XTC_POINTS = 0
XTC_LINELIST = 1
XTC_LINESTRIP = 2
XTC_TRILIST = 3
XTC_TRISTRIP = 4

-- xtcColorBit, for xtcSetColorMaterial
XTC_EMISSIVE = 1
XTC_AMBIENT = 2
XTC_DIFFUSE = 4
XTC_SPECULAR = 8

---- xtcSetViewMatrix
---- xtcSetWorldMatrix
---- xtcGetWorldMatrix
---- xtcSetBoneMatrices({mat4, ...})
---- xtcBegin(primtype)
---- xtcEnd()
---- xtcVertex(x, y, z)
---- xtcColor(r, g, b, a)	-- 0-255
---- xtcNormal(x, y, z)
---- xtcTexCoord(u, v [, q])
---- xtcIndices(i1, i2, i3, i4)
---- xtcWeights(w1, w2, w3, w4)


---- xtcSetPipeline(pipe)	-- defaultPipeline, skinPipeline, stdPipeline
---- xtcSetTexture
---- xtcStdMaterial()
---- xtcSetStdMaterial(m)
---- xtcSetColorMaterial(bits)

-- xtcLightType
XTC_LIGHT_DIRECT = 0
XTC_LIGHT_POINT = 1
---- xtcLight()
---- xtcSetAmbient(r, g, b)	-- 0-1
---- xtcSetLight(n, light)

-- TODO:
--	xtcSetProjectionMatrix

--
-- xModel
--

---- loadXModel(path)
---- loadXModelChunk(path)
---- xModel.draw()
