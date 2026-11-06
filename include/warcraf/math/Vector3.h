// Vector3.h -- copy of the TinkerWorX/warcraftIII header of the same name.
// Source: ~/workspace/warcraftIII/Vector3.h (MIT License, Copyright (c) 2017
// Nikolaj Mariager). The original repository was NOT modified.
// Changes vs the original (see README.md): include guard added (originals
// have none -- required for the Warcraf.h umbrella); intra-repo includes
// retargeted to "warcraf/sim/..." / "warcraf/math/..."; Types.h included
// where _DWORD/_WORD/_BYTE is used (the originals relied on an IDA prelude).
// Field offsets: NOT spot-checked against 1.27a evidence -- kept exactly as in the TinkerWorX original, UNVERIFIED.

#ifndef WARCRAF_MATH_VECTOR3_H
#define WARCRAF_MATH_VECTOR3_H
struct Vector3 { /* size = 12 */
    float x;
    float y;
    float z;
};

#endif // WARCRAF_MATH_VECTOR3_H
