// Per-frame values shared by every program (TECH_SPEC 5). Members are highp so both stages agree on precision.
layout(std140) uniform Frame {
    highp vec4 uScreen;   // the target: w, h, 1/w, 1/h (pixels)
    highp vec4 uTime;     // x seconds since start
};
