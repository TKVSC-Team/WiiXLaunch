------------------------Shader Information------------------------
RequiredScratchMemorySizePerWarp: 0
RecommendedScratchMemorySizePerWarp: 0


------------------------Shader Statistics------------------------
Latency: 61
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
AttributeMemUsage: 32
ProgramSize: 192
RequiresGlobalLoadUniformEmulation: false
Fp16VectPercent: 0
ThroughputLimiter
    Issue: 0.235294
    Fp: 0
    Half: 0
    Trancedental: 0
    Ipa: 0
    Shared: 0.0625
    ControlFlow: 1
    TexLoadStore: 0
    Reg: 1.04918
    Warp: 1.04918
    SharedMemResource: 0
LoopData
    PartiallyUnrolled: 0
    NonUnrolled: 0


------------------------Dump------------------------

#version 450
#version 450 core
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) in vec4 a_Position;
layout(location = 1) in vec4 a_Color;

layout(location = 0) out vec4 v_Color;

void main() {
    gl_Position = a_Position;
    v_Color = a_Color;
}

------------------------Assembly------------------------
	!!SPA5.3
	.THREAD_TYPE vertex
	     ALD             R0, a[0x80];                                  # [000008] ATTR0 
	     ALD             R1, a[0x84];                                  # [000010] GENERIC_ATTRIBUTE_00_Y 
	     ALD             R2, a[0x88];                                  # [000018] GENERIC_ATTRIBUTE_00_Z 
	     ALD             R3, a[0x8c];                                  # [000028] GENERIC_ATTRIBUTE_00_W 
	     ALD             R4, a[0x90];                                  # [000030] ATTR1 
	     ALD             R5, a[0x94];                                  # [000038] GENERIC_ATTRIBUTE_01_Y 
	     ALD             R6, a[0x98];                                  # [000048] GENERIC_ATTRIBUTE_01_Z 
	     ALD             R7, a[0x9c];                                  # [000050] GENERIC_ATTRIBUTE_01_W 
	     AST             a[0x70], R0;                                  # [000058] POSITION_X 
	     AST             a[0x74], R1;                                  # [000068] POSITION_Y 
	     AST             a[0x78], R2;                                  # [000070] POSITION_Z 
	     AST             a[0x7c], R3;                                  # [000078] POSITION_W 
	     AST             a[0x80], R4;                                  # [000088] ATTR0 
	     AST             a[0x84], R5;                                  # [000090] GENERIC_ATTRIBUTE_00_Y 
	     AST             a[0x88], R6;                                  # [000098] GENERIC_ATTRIBUTE_00_Z 
	     AST             a[0x8c], R7;                                  # [0000a8] GENERIC_ATTRIBUTE_00_W 
	     EXIT;                                                         # [0000b0] 
L00b8:
	     BRA             L00b8;                                        # [0000b8] 
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



