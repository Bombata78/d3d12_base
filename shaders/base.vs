///////////////////////////////////////////////////////////////////////////////////////////////////
//  base.vs
//    Base vertex shader
///////////////////////////////////////////////////////////////////////////////////////////////////

struct VsInput {
   float3 position : POSITION;
   float3 normal   : NORMAL; 
   float2 texCoord : TEXCOORD;
};

struct PsInput {
   float4 position      : SV_POSITION;
   float3 positionWorld : POSITION_WORLD; 
   float3 normal        : NORMAL;
   float2 texCoord      : TEXCOORD;
};

cbuffer cbTransforms : register( b0 )
{
    matrix MVPMatrix;
    matrix modelMatrix;
};

PsInput main(VsInput input)
{
   PsInput vsOut;
   vsOut.position      = mul(MVPMatrix, float4(input.position.xyz, 1.0));
   vsOut.positionWorld = mul(modelMatrix, float4(input.position.xyz, 1.0)).xyz;
   vsOut.normal        = mul(modelMatrix, float4(input.normal, 0.0)).xyz;
   vsOut.texCoord      = input.texCoord;
   return vsOut;
}
