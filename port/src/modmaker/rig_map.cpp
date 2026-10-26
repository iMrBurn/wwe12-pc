// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/rig_map.cpp at b2521fe (unchanged).
// Source rigs -> the SvR 2011 skeleton (rig_map.h). A name is reduced to a
// side (left / right / none), a body part and, for fingers, a finger and a
// joint; the part picks the 2011 bone. 2011's names: root (the hips, which the
// legs hang from), koshi (waist), mune (chest), kubi (neck), atama (head);
// per side sakotsu (clavicle), ninoude (upper arm, twist ninoude_x), kote
// (forearm, twists kote_x / kote_x2), te (hand), yubiFJ (finger F 0 thumb ..
// 4 pinky, joint J 0..2), momo (thigh, twist momo_x), sune (shin), ashi
// (foot), tsumasaki (toes); l_eye / r_eye, d_kuchi (jaw).
#include "rig_map.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>

namespace mm {

namespace {

std::string Lower(std::string s) {
  for (char& c : s) c = char(std::tolower(uint8_t(c)));
  return s;
}

struct Parsed {
  char side = 0;   // 'l', 'r' or 0
  std::string part;  // hips, spine, chest, neck, head, clavicle, upperarm, forearm, hand, thigh, shin, foot, toe, eye, jaw, finger
  int finger = -1, joint = -1;
  bool twist = false;
  int spine = -1;  // spine number when counted (spine_01, Spine1, spine.003, Spine2 ...)
};

bool Has(const std::string& s, const char* w) { return s.find(w) != std::string::npos; }

// The digits at the end of s (after the part word), -1 if none.
int TrailingNumber(const std::string& s) {
  size_t e = s.size();
  while (e > 0 && !std::isdigit(uint8_t(s[e - 1]))) --e;
  size_t b = e;
  while (b > 0 && std::isdigit(uint8_t(s[b - 1]))) --b;
  return b == e ? -1 : std::atoi(s.substr(b, e - b).c_str());
}

Parsed Parse(const std::string& raw) {
  Parsed p;
  std::string s = Lower(raw);
  // namespaces / prefixes: "mixamorig:", "armature|", "def-", "org-", "bip01 ", "bip001 ", "bip "
  if (const size_t c = s.find_last_of(":|"); c != std::string::npos) s = s.substr(c + 1);
  for (const char* pre : {"def-", "def_", "org-", "mch-"})
    if (s.rfind(pre, 0) == 0) s = s.substr(std::strlen(pre));
  for (const char* pre : {"bip001 ", "bip01 ", "bip001_", "bip01_", "bip "})
    if (s.rfind(pre, 0) == 0) s = s.substr(std::strlen(pre));
  // the side: "left"/"right" anywhere; "l "/"l_"/"l." at the start; ".l"/"_l"/" l"/"-l" at the end (Blender / UE)
  auto strip = [&](size_t at, size_t len) { s.erase(at, len); };
  if (size_t at = s.find("left"); at != std::string::npos) p.side = 'l', strip(at, 4);
  else if (size_t at2 = s.find("right"); at2 != std::string::npos) p.side = 'r', strip(at2, 5);
  else if (s.size() > 2 && (s[0] == 'l' || s[0] == 'r') && (s[1] == ' ' || s[1] == '_' || s[1] == '.')) p.side = s[0], s = s.substr(2);
  else if (s.size() > 2 && (s[s.size() - 1] == 'l' || s[s.size() - 1] == 'r') &&
           (s[s.size() - 2] == '.' || s[s.size() - 2] == '_' || s[s.size() - 2] == ' ' || s[s.size() - 2] == '-'))
    p.side = s[s.size() - 1], s.resize(s.size() - 2);
  else if (size_t m0 = s.find(".l."); m0 != std::string::npos) p.side = 'l', strip(m0, 2);  // (Rigify: upper_arm.L.001)
  else if (size_t m1 = s.find(".r."); m1 != std::string::npos) p.side = 'r', strip(m1, 2);
  else if (size_t m = s.find("_l_"); m != std::string::npos) p.side = 'l', strip(m, 2);
  else if (size_t m2 = s.find("_r_"); m2 != std::string::npos) p.side = 'r', strip(m2, 2);
  // separators out
  std::string t;
  for (char c : s)
    if (std::isalnum(uint8_t(c))) t += c;
  s = t;
  p.twist = Has(s, "twist") || Has(s, "roll") || (s.size() > 1 && s.back() == 'x' && Has(s, "thigh")) ;
  // fingers: the finger word (or Biped's FingerN: 0 thumb .. 4 pinky) and the joint
  static const std::pair<const char*, int> kFingers[] = {{"thumb", 0}, {"index", 1}, {"middle", 2}, {"ring", 3}, {"little", 4}, {"pinky", 4}, {"pinkie", 4}};
  for (const auto& [w, f] : kFingers)
    if (Has(s, w) && !Has(s, "spring")) {
      p.part = "finger", p.finger = f;
      int j = TrailingNumber(s);
      if (Has(s, "proximal") || Has(s, "metacarpal")) j = Has(s, "metacarpal") ? -2 : 1;
      else if (Has(s, "intermediate")) j = 2;
      else if (Has(s, "distal")) j = 3;
      if (j == -2) p.part.clear();  // (a metacarpal: folds into the hand)
      p.joint = j <= 0 ? 0 : std::min(2, j - 1);  // 1..3 (and 01 / 02 / 03) -> joint 0..2
      if (j > 3 && j < 100) p.joint = std::min(2, j % 10 - 1);
      return p;
    }
  if (s.rfind("finger", 0) == 0 && s.size() > 6 && std::isdigit(uint8_t(s[6]))) {  // Biped: Finger0, Finger01, Finger02 ... Finger4
    p.part = "finger", p.finger = s[6] - '0';
    p.joint = s.size() > 7 && std::isdigit(uint8_t(s[7])) ? std::min(2, s[7] - '0') : 0;
    return p;
  }
  // the body
  if (Has(s, "pelvis") || Has(s, "hips") || s == "hip") p.part = "hips";
  else if (Has(s, "upperchest") || Has(s, "chest") || Has(s, "breast") && p.side == 0) p.part = "chest";
  else if (Has(s, "spine")) p.part = "spine", p.spine = TrailingNumber(s);
  else if (Has(s, "neck")) p.part = "neck";
  else if (Has(s, "head") && !Has(s, "headtop") && !Has(s, "headend")) p.part = "head";
  else if (Has(s, "clavicle") || Has(s, "shoulder") || Has(s, "collar")) p.part = "clavicle";
  else if (Has(s, "upperarm") || Has(s, "uparm") || (Has(s, "arm") && !Has(s, "fore") && !Has(s, "lower") && !Has(s, "low"))) p.part = "upperarm";
  else if (Has(s, "forearm") || Has(s, "lowerarm") || Has(s, "lowarm") || Has(s, "elbow")) p.part = "forearm";
  else if (Has(s, "hand") || Has(s, "wrist")) p.part = "hand";
  else if (Has(s, "thigh") || Has(s, "upperleg") || Has(s, "upleg")) p.part = "thigh";
  else if (Has(s, "calf") || Has(s, "shin") || Has(s, "lowerleg") || Has(s, "knee") || s == "leg") p.part = "shin";
  else if (Has(s, "toe") || Has(s, "ball")) p.part = "toe";
  else if (Has(s, "foot") || Has(s, "ankle")) p.part = "foot";
  else if (Has(s, "eye") && !Has(s, "brow") && !Has(s, "lid")) p.part = "eye";
  else if (Has(s, "jaw")) p.part = "jaw";
  // twist bones: "upperarm_twist_01", "forearm.001" (Rigify's second segment), "L_Thigh_x"
  if (!p.part.empty() && (p.part == "upperarm" || p.part == "forearm" || p.part == "thigh")) {
    const std::string lr = Lower(raw);
    if (lr.size() > 4 && (lr.find(".001") != std::string::npos)) p.twist = true;
    if (lr.size() > 2 && (lr.substr(lr.size() - 2) == "_x")) p.twist = true;
  }
  return p;
}

std::string Side(char side, const char* bone) { return std::string(side == 'r' ? "r_" : "l_") + bone; }

}  // namespace

std::string MapBone(const std::string& source) {
  for (const auto& t : TargetBones())  // (a 2011 skeleton already - the Mod Maker's own FBX export)
    if (t == source) return t;
  {  // Rigify's spine chain: DEF-spine (the pelvis), .001 .. .003 the back, .004 / .005 the neck, .006 the head
    const std::string l = Lower(source);
    if (l.rfind("def-spine", 0) == 0) {
      const int n = TrailingNumber(l);
      static const char* kChain[] = {"root", "koshi", "mune", "mune", "kubi", "kubi", "atama"};
      return n < 0 ? "root" : n < 7 ? kChain[n] : "atama";
    }
  }
  const Parsed p = Parse(source);
  if (p.part.empty()) return "";
  if (p.part == "hips") return "root";
  if (p.part == "spine") return p.spine >= 2 ? "mune" : "koshi";  // (Spine / spine_01 -> waist; Spine2+ -> chest)
  if (p.part == "chest") return "mune";
  if (p.part == "neck") return "kubi";
  if (p.part == "head") return "atama";
  if (p.part == "jaw") return "d_kuchi";
  if (!p.side) return "";
  if (p.part == "clavicle") return Side(p.side, "sakotsu");
  if (p.part == "upperarm") return Side(p.side, p.twist ? "ninoude_x" : "ninoude");
  if (p.part == "forearm") return Side(p.side, p.twist ? "kote_x" : "kote");
  if (p.part == "hand") return Side(p.side, "te");
  if (p.part == "thigh") return Side(p.side, p.twist ? "momo_x" : "momo");
  if (p.part == "shin") return Side(p.side, "sune");
  if (p.part == "foot") return Side(p.side, "ashi");
  if (p.part == "toe") return Side(p.side, "tsumasaki");
  if (p.part == "eye") return Side(p.side, "eye");
  if (p.part == "finger" && p.finger >= 0 && p.finger <= 4)
    return Side(p.side, ("yubi" + std::to_string(p.finger) + std::to_string(std::clamp(p.joint, 0, 2))).c_str());
  return "";
}

std::vector<std::string> MapRig(const std::vector<RigBone>& bones) {
  std::vector<std::string> out(bones.size());
  for (size_t i = 0; i < bones.size(); ++i) out[i] = MapBone(bones[i].name);
  auto under = [&](size_t i, const char* suffix) {  // an ancestor mapped to <side>_<suffix>
    for (int p = bones[i].parent; p >= 0 && p != int(i); p = bones[size_t(p)].parent)
      if (out[size_t(p)].size() > 2 && out[size_t(p)].substr(2) == suffix) return true;
    return false;
  };
  // "leg" with a knee on the same side: the leg is the thigh (CATS-style names), the knee the shin
  for (const char side : {'l', 'r'}) {
    bool knee = false;
    for (size_t i = 0; i < bones.size(); ++i) {
      const Parsed p = Parse(bones[i].name);
      knee |= p.side == side && Lower(bones[i].name).find("knee") != std::string::npos;
    }
    if (!knee) continue;
    for (size_t i = 0; i < bones.size(); ++i) {
      const std::string l = Lower(bones[i].name);
      const Parsed p = Parse(bones[i].name);
      if (p.side == side && p.part == "shin" && l.find("knee") == std::string::npos && l.find("calf") == std::string::npos &&
          l.find("shin") == std::string::npos && l.find("lower") == std::string::npos)
        out[i] = Side(side, "momo");
    }
  }
  // fingers: only under a hand; joints by their order along each finger's chain (0, 1, 2; more fold into the last)
  std::map<std::string, std::vector<std::pair<int, size_t>>> chains;  // "l_yubi1" -> (depth, bone)
  for (size_t i = 0; i < bones.size(); ++i) {
    if (out[i].find("yubi") == std::string::npos) continue;
    if (!under(i, "te")) {
      out[i].clear();  // (a "Middle" or "Index" bone that isn't a finger: a jacket's, a hair strand's)
      continue;
    }
    int depth = 0;
    for (int p = bones[i].parent; p >= 0 && p != int(i); p = bones[size_t(p)].parent) ++depth;
    chains[out[i].substr(0, out[i].size() - 1)].push_back({depth, i});
  }
  for (auto& [key, list] : chains) {
    std::sort(list.begin(), list.end());
    for (size_t k = 0; k < list.size(); ++k) out[list[k].second] = key + std::to_string(std::min<size_t>(k, 2));
  }
  // Mixamo / Unity: "Spine" + "Spine1" + "Spine2" (or Spine, Chest, UpperChest): the first spine bone is the waist,
  // anything above it before the neck the chest - fix up a chain where two bones took koshi
  int first_spine = -1;
  for (size_t i = 0; i < bones.size(); ++i)
    if (out[i] == "koshi") {
      if (first_spine < 0) first_spine = int(i);
      else out[i] = "mune";
    }
  // root: the first hips bone only (an armature "Root" above the hips stays unmapped: it folds nowhere)
  bool root = false;
  int root_bone = -1;
  for (size_t i = 0; i < out.size(); ++i)
    if (out[i] == "root") {
      if (root) out[i].clear();
      else root_bone = int(i);
      root = true;
    }
  // spine by structure (the same rule as the porting tools' rig_maps): the path from the hips to the clavicles'
  // parent - its first bone the waist, its last the chest, the ones between fold into the waist; then the first bone
  // from the chest toward the head is the neck, the ones after it up to the head fold into the neck
  auto parent_of = [&](int i) { return i >= 0 ? bones[size_t(i)].parent : -1; };
  int chest = -1;
  for (size_t i = 0; i < out.size() && chest < 0; ++i)
    if (out[i] == "l_sakotsu" || out[i] == "r_sakotsu") chest = parent_of(int(i));
  if (chest >= 0 && out[size_t(chest)] == "kubi") chest = parent_of(chest);  // (Biped: clavicles hang from the neck)
  if (root_bone >= 0 && chest >= 0 && chest != root_bone) {
    std::vector<int> path;  // chest .. first bone above the hips
    for (int p = chest; p >= 0 && p != root_bone && path.size() < bones.size(); p = parent_of(p)) path.push_back(p);
    if (!path.empty() && parent_of(path.back()) == root_bone) {
      for (size_t k = 0; k < path.size(); ++k)
        out[size_t(path[k])] = k == 0 ? "mune" : k + 1 == path.size() ? "koshi" : "";
      if (path.size() == 1) out[size_t(path[0])] = "koshi";  // one spine bone: the waist (the chest folds into it)
      int head = -1;
      for (size_t i = 0; i < out.size() && head < 0; ++i)
        if (out[i] == "atama") head = int(i);
      std::vector<int> neck;  // head's ancestors up to the chest
      for (int p = parent_of(head); p >= 0 && p != chest && neck.size() < bones.size(); p = parent_of(p)) neck.push_back(p);
      if (head >= 0 && parent_of(neck.empty() ? head : neck.back()) == chest)
        for (size_t k = 0; k < neck.size(); ++k) out[size_t(neck[k])] = k + 1 == neck.size() ? "kubi" : "";
    }
  }
  return out;
}

std::string DetectRig(const std::vector<RigBone>& bones) {
  int mixamo = 0, rigify = 0, biped = 0, ue = 0, unity = 0, blender_lr = 0;
  for (const auto& b : bones) {
    const std::string s = Lower(b.name);
    mixamo += s.rfind("mixamorig", 0) == 0;
    rigify += s.rfind("def-", 0) == 0;
    biped += s.rfind("bip01", 0) == 0 || s.rfind("bip001", 0) == 0 || s.rfind("bip ", 0) == 0;
    ue += s == "pelvis" || s == "spine_01" || s == "upperarm_l" || s == "thigh_l" || s == "ball_l";
    unity += s == "hips" || s == "upperchest" || s.find("upperarm") != std::string::npos || s.find("upperleg") != std::string::npos ||
             s.find("proximal") != std::string::npos;
    blender_lr += s.size() > 2 && (s.substr(s.size() - 2) == ".l" || s.substr(s.size() - 2) == ".r");
  }
  {  // the 2011 skeleton itself (the Mod Maker's own export, or a model made on it)
    int own = 0;
    for (const auto& b : bones)
      for (const char* n : {"root", "koshi", "mune", "kubi", "atama", "l_ninoude", "r_ninoude", "l_momo", "r_momo"}) own += b.name == n;
    if (own >= 7) return "SvR 2011 skeleton";
  }
  if (mixamo > 5) return "Mixamo";
  if (rigify > 5) return "Blender Rigify";
  if (biped > 5) return "3ds Max Biped";
  if (ue >= 3) return "UE Mannequin";
  int cats = 0;
  for (const auto& b : bones) {
    const std::string s = Lower(b.name);
    cats += s == "left wrist" || s == "right wrist" || s == "left ankle" || s == "right ankle" || s == "left knee" || s == "left elbow";
  }
  if (cats >= 3) return "VRChat (CATS-style names)";
  if (unity >= 3) return blender_lr > 5 ? "Unity Humanoid (Blender .L / .R names)" : "Unity Humanoid / VRChat";
  return "unknown (mapped by its names)";
}

const std::vector<std::string>& TargetBones() {
  static const std::vector<std::string> v = [] {
    std::vector<std::string> t = {"root", "koshi", "mune", "kubi", "atama", "d_kuchi"};
    for (const char* side : {"l_", "r_"}) {
      for (const char* b : {"sakotsu", "ninoude", "ninoude_x", "kote", "kote_x", "te", "momo", "momo_x", "sune", "ashi", "tsumasaki", "eye"})
        t.push_back(std::string(side) + b);
      for (int f = 0; f < 5; ++f)
        for (int j = 0; j < 3; ++j) t.push_back(std::string(side) + "yubi" + std::to_string(f) + std::to_string(j));
    }
    return t;
  }();
  return v;
}

std::vector<std::string> RigSelfTest() {
  // each rig's standard names (generic, as their tools name them) -> the 2011 bone
  static const std::pair<const char*, const char*> kCases[] = {
      // Unity Humanoid / VRChat
      {"Hips", "root"}, {"Spine", "koshi"}, {"Chest", "mune"}, {"UpperChest", "mune"}, {"Neck", "kubi"}, {"Head", "atama"},
      {"LeftShoulder", "l_sakotsu"}, {"LeftUpperArm", "l_ninoude"}, {"LeftLowerArm", "l_kote"}, {"LeftHand", "l_te"},
      {"Left Index Proximal", "l_yubi10"}, {"Left Index Intermediate", "l_yubi11"}, {"Left Index Distal", "l_yubi12"},
      {"Left Thumb Proximal", "l_yubi00"}, {"Right Little Distal", "r_yubi42"}, {"RightUpperLeg", "r_momo"},
      {"RightLowerLeg", "r_sune"}, {"RightFoot", "r_ashi"}, {"RightToes", "r_tsumasaki"}, {"LeftEye", "l_eye"}, {"Jaw", "d_kuchi"},
      // the same in Blender's .L / .R style
      {"UpperArm.L", "l_ninoude"}, {"LowerArm.R", "r_kote"}, {"Hand.L", "l_te"}, {"UpperLeg.R", "r_momo"},
      {"Thumb Proximal.L", "l_yubi00"}, {"IndexFinger1_L", "l_yubi10"}, {"Shoulder.R", "r_sakotsu"},
      // Mixamo
      {"mixamorig:Hips", "root"}, {"mixamorig:Spine", "koshi"}, {"mixamorig:Spine2", "mune"}, {"mixamorig:Neck", "kubi"},
      {"mixamorig:Head", "atama"}, {"mixamorig:LeftShoulder", "l_sakotsu"}, {"mixamorig:LeftArm", "l_ninoude"},
      {"mixamorig:LeftForeArm", "l_kote"}, {"mixamorig:LeftHand", "l_te"}, {"mixamorig:LeftHandThumb1", "l_yubi00"},
      {"mixamorig:LeftHandIndex3", "l_yubi12"}, {"mixamorig:RightHandPinky2", "r_yubi41"}, {"mixamorig:RightUpLeg", "r_momo"},
      {"mixamorig:RightLeg", "r_sune"}, {"mixamorig:RightFoot", "r_ashi"}, {"mixamorig:RightToeBase", "r_tsumasaki"},
      // Blender Rigify
      {"DEF-spine", "root"}, {"DEF-spine.001", "koshi"}, {"DEF-spine.003", "mune"}, {"DEF-spine.004", "kubi"}, {"DEF-spine.006", "atama"}, {"DEF-shoulder.L", "l_sakotsu"}, {"DEF-upper_arm.L", "l_ninoude"}, {"DEF-upper_arm.L.001", "l_ninoude_x"},
      {"DEF-forearm.R", "r_kote"}, {"DEF-forearm.R.001", "r_kote_x"}, {"DEF-hand.R", "r_te"}, {"DEF-thumb.01.L", "l_yubi00"},
      {"DEF-f_index.02.R", "r_yubi11"}, {"DEF-f_pinky.03.L", "l_yubi42"}, {"DEF-thigh.L", "l_momo"}, {"DEF-shin.R", "r_sune"},
      {"DEF-foot.L", "l_ashi"}, {"DEF-toe.R", "r_tsumasaki"},
      // 3ds Max Biped (and WWE '13 / 2K "Bip")
      {"Bip01 Pelvis", "root"}, {"Bip01 Spine", "koshi"}, {"Bip01 Spine1", "koshi"}, {"Bip01 Neck", "kubi"}, {"Bip01 Head", "atama"},
      {"Bip01 L Clavicle", "l_sakotsu"}, {"Bip01 L UpperArm", "l_ninoude"}, {"Bip01 L Forearm", "l_kote"}, {"Bip01 L Hand", "l_te"},
      {"Bip01 L Finger0", "l_yubi00"}, {"Bip01 L Finger01", "l_yubi01"}, {"Bip01 R Finger12", "r_yubi12"},
      {"Bip01 R Thigh", "r_momo"}, {"Bip01 R Calf", "r_sune"}, {"Bip01 R Foot", "r_ashi"}, {"Bip01 R Toe0", "r_tsumasaki"},
      {"Bip L Thigh", "l_momo"}, {"L_Thigh_x", "l_momo_x"},
      // UE Mannequin
      {"pelvis", "root"}, {"spine_01", "koshi"}, {"spine_03", "mune"}, {"neck_01", "kubi"}, {"head", "atama"},
      {"clavicle_l", "l_sakotsu"}, {"upperarm_l", "l_ninoude"}, {"upperarm_twist_01_l", "l_ninoude_x"}, {"lowerarm_r", "r_kote"},
      {"lowerarm_twist_01_r", "r_kote_x"}, {"hand_r", "r_te"}, {"thumb_01_l", "l_yubi00"}, {"index_03_r", "r_yubi12"},
      {"pinky_02_l", "l_yubi41"}, {"thigh_l", "l_momo"}, {"thigh_twist_01_l", "l_momo_x"}, {"calf_r", "r_sune"},
      {"foot_r", "r_ashi"}, {"ball_l", "l_tsumasaki"},
  };
  std::vector<std::string> fails;
  // chains (MapRig): CATS-style legs and 0-based thumbs; a jacket bone named "Middle" that isn't a finger
  {
    const std::vector<RigBone> rig = {{"Hips", -1},          {"Left leg", 0},          {"Left knee", 1},          {"Left ankle", 2},
                                      {"Spine", 0},          {"Chest", 4},             {"Left shoulder", 5},      {"Left arm", 6},
                                      {"Left elbow", 7},     {"Left wrist", 8},        {"Thumb0_L", 9},           {"Thumb1_L", 10},
                                      {"Thumb2_L", 11},      {"IndexFinger1_L", 9},    {"IndexFinger2_L", 13},    {"IndexFinger3_L", 14},
                                      {"Jacket_Middle_1_Left", 5}, {"mixamorig:LeftUpLeg", 0}, {"mixamorig:LeftLeg", 17}};
    const char* want[] = {"root", "l_momo", "l_sune", "l_ashi", "koshi", "mune", "l_sakotsu", "l_ninoude", "l_kote", "l_te",
                          "l_yubi00", "l_yubi01", "l_yubi02", "l_yubi10", "l_yubi11", "l_yubi12", "", "l_momo", "l_momo"};
    auto got = MapRig(rig);
    const std::vector<RigBone> uni = {{"Hips", -1}, {"Spine", 0}, {"Chest", 1}, {"UpperChest", 2}, {"Neck", 3}, {"Head", 4},
                                      {"LeftShoulder", 3}};
    const char* uwant[] = {"root", "koshi", "", "mune", "kubi", "atama", "l_sakotsu"};
    const auto ugot = MapRig(uni);
    for (size_t i = 0; i < uni.size(); ++i)
      if (ugot[i] != uwant[i]) fails.push_back("spine: " + uni[i].name + " -> " + (ugot[i].empty() ? "(none)" : ugot[i]) + ", want " + (uwant[i][0] ? uwant[i] : "(none)"));
    const std::vector<RigBone> bip = {{"Bip01 Pelvis", -1}, {"Bip01 Spine", 0}, {"Bip01 Spine1", 1}, {"Bip01 Spine2", 2},
                                      {"Bip01 Neck", 3}, {"Bip01 Head", 4}, {"Bip01 L Clavicle", 4}};
    const char* bwant[] = {"root", "koshi", "", "mune", "kubi", "atama", "l_sakotsu"};
    const auto bgot = MapRig(bip);
    for (size_t i = 0; i < bip.size(); ++i)
      if (bgot[i] != bwant[i]) fails.push_back("biped spine: " + bip[i].name + " -> " + (bgot[i].empty() ? "(none)" : bgot[i]) + ", want " + (bwant[i][0] ? bwant[i] : "(none)"));
    for (size_t i = 0; i < rig.size(); ++i)
      if (got[i] != want[i]) fails.push_back("chain: " + rig[i].name + " -> " + (got[i].empty() ? "(none)" : got[i]) + ", want " + (want[i][0] ? want[i] : "(none)"));
  }
  for (const auto& [src, want] : kCases)
    if (const std::string got = MapBone(src); got != want) fails.push_back(std::string(src) + " -> " + (got.empty() ? "(none)" : got) + ", want " + want);
  return fails;
}

}  // namespace mm
