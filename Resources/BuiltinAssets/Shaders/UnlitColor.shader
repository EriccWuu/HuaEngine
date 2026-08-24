name: UnlitColor
language: HLSL
source: UnlitColor.hlsl
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
