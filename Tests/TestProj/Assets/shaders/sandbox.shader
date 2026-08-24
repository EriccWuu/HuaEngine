name: Sandbox
language: HLSL
source: sandbox.hlsl
stages:
  vertex:
    entry: VSMain
    profile: vs_6_0
  fragment:
    entry: PSMain
    profile: ps_6_0
parameters:
  u_Color:
    display_name: Color
    scope: Material
    editor: Color
    default: [1.0, 1.0, 1.0, 1.0]
  u_Texture:
    display_name: Texture
    scope: Material
    editor: Texture2D
    default: ""
