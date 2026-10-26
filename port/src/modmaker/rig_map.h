// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/rig_map.h at b2521fe (unchanged).
// Source rigs -> the SvR 2011 skeleton, by bone names (FBX import, F2): the
// well-known rigs - Unity Humanoid / VRChat (Blender .L / .R too), Mixamo,
// Blender Rigify (DEF-), 3ds Max Biped (Bip01, and WWE '13 / 2K's "Bip"), the
// UE Mannequin - and anything named like them. Every source bone gets a 2011
// bone or "" (folded into its nearest mapped parent when the weights move).
#pragma once

#include <string>
#include <vector>

namespace mm {

struct RigBone {
  std::string name;
  int parent = -1;  // index in the list
};

// The rig's family by its names ("Mixamo", "Unity Humanoid", ..., "unknown").
std::string DetectRig(const std::vector<RigBone>& bones);
// The 2011 bone a source bone maps to ("" = none: its weights go to the nearest mapped parent).
// Twist / helper bones of the source map to the 2011 twist bones when 2011 has one (ninoude_x, kote_x, momo_x).
std::string MapBone(const std::string& source);
// Every bone mapped (index -> 2011 name), the same 2011 bone kept by the first source bone only where
// it must be unique (root); the rest fold.
std::vector<std::string> MapRig(const std::vector<RigBone>& bones);
// The 2011 bones a mapping can name (for the hand-edit list): the body, fingers and twist bones.
const std::vector<std::string>& TargetBones();
// Test: each known rig's standard names against what they must map to; a line per failure, empty if all pass.
std::vector<std::string> RigSelfTest();

}  // namespace mm
