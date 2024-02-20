import("stdfaust.lib");
pitchshifter = vgroup("Pitch Shifter", ef.transpose(
    vslider("window (samples)", 1000, 50, 10000, 1),
    vslider("xfade (samples)", 10, 1, 10000, 1),
    vslider("shift (semitones)", 0, -24, +24, 0.1)
 )
);
process = _ : pitchshifter;