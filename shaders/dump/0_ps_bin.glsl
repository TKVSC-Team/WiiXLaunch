------------------------Shader Information------------------------
RequiredScratchMemorySizePerWarp: 0
RecommendedScratchMemorySizePerWarp: 0


------------------------Shader Statistics------------------------
Latency: 47
SpillMem
    NumLmemSpillBytes: 0
    NumLmemRefillBytes: 0
    NumSmemSpillBytes: 0
    NumSmemRefillBytes: 0
    Size: 0
NonSpillLMem
    LoadBytes: 0
    StoreBytes: 0
    Size: 0
Occupancy: 1
NumDivergentBranches: 0
AttributeMemUsage: 20
ProgramSize: 128
RequiresGlobalLoadUniformEmulation: false
Fp16VectPercent: 0
ThroughputLimiter
    Issue: 0.571429
    Fp: 1
    Half: 0
    Trancedental: 1
    Ipa: 0.2
    Shared: 0
    ControlFlow: 1
    TexLoadStore: 0
    Reg: 2.7234
    Warp: 2.7234
    SharedMemResource: 0
LoopData
    PartiallyUnrolled: 0
    NonUnrolled: 0


------------------------Dump------------------------

#version 450
#version 450 core

layout(location = 0) in vec4 v_Color;
layout(location = 0) out vec4 o_Color;

void main() {
    o_Color = v_Color;
}

------------------------Assembly------------------------
	!!SPA5.3
	.THREAD_TYPE pixel
	     IPA.PASS        R4, a[0x7c], RZ;                              # [000008] POSITION_W 
	     MUFU.RCP        R4, R4;                                       # [000010] 
	     IPA             R0, a[0x80], R4;                              # [000018] ATTR0 
	     IPA             R1, a[0x84], R4;                              # [000028] GENERIC_ATTRIBUTE_00_Y 
	     IPA             R2, a[0x88], R4;                              # [000030] GENERIC_ATTRIBUTE_00_Z 
	     IPA             R3, a[0x8c], R4;                              # [000038] GENERIC_ATTRIBUTE_00_W 
	     EXIT;                                                         # [000048] 
L0050:
	     BRA             L0050;                                        # [000050] 
	     NOP;                                                          # [000058] 
	     NOP;                                                          # [000068] 
	     NOP;                                                          # [000070] 
	     NOP;                                                          # [000078] 
	END

------------------------GLSLC Option Flags------------------------
glslSeparable                            : 0
outputAssembly                           : 1
outputGpuBinaries                        : 1
outputPerfStats                          : 1
ouptutShaderReflection                   : 1
language                                 : GLSLC_LANGUAGE_GLSL
outputDebugInfo                          : GLSLC_DEBUG_LEVEL_NONE
spillControl                             : DEFAULT_SPILL
outputThinGpuBinaries                    : 1
tessellationAndPassthroughGS             : 0
prioritizeConsecutiveTextureInstructions : 0
enableFastMathMask                       : 2
optLevel                                 : GLSLC_OPTLEVEL_DEFAULT
unrollControl                            : GLSLC_LOOP_UNROLL_DEFAULT
errorOnScratchMemUsage                   : 0
enableCBFOptimization                    : 0
enableWarpCulling                        : 0
enableMultithreadCompilation             : 0
warnUninitControl                        : GLSLC_WARN_UNINIT_DEFAULT
ignoreBindings                           : 0
singleArrayInfoShaderReflection          : 0
packUnpackHalf2x16WithDenorm             : 0



