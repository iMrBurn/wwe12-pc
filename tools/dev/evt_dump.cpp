// Lists an event pac of WWE '13 / 2K (xcompressed EPK8 / EPAC, e.g. pac\evt\Universe00.pac) and writes its entries
// unpacked: evt_dump <pac> [out dir]. Read only (the Mod Maker's svrfmt readers). docs/univers.md section 44.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "../../port/src/modmaker/svrfmt/archive.h"
#include "../../port/src/modmaker/svrfmt/pac.h"
#include "../../port/src/modmaker/svrfmt/xcompress.h"

using svrfmt::Bytes;

int main(int argc, char **argv) {
  if (argc < 2)
    return std::fprintf(stderr, "evt_dump <pac> [out dir]\n"), 2;
  std::ifstream f(argv[1], std::ios::binary);
  Bytes b(std::istreambuf_iterator<char>(f), {});
  if (svrfmt::IsXcompress(b)) {
    Bytes o;
    std::string why;
    if (!svrfmt::XcompressDecode(b, o, &why))
      return std::fprintf(stderr, "xcompress: %s\n", why.c_str()), 1;
    b.swap(o);
  }
  svrfmt::Archive a;
  std::string why;
  if (!svrfmt::ArchiveOpen(b, a, &why))
    return std::fprintf(stderr, "open: %s\n", why.c_str()), 1;
  std::printf("%s, %zu entries\n", a.format.c_str(), a.root.kids.size());
  for (auto &k : a.root.kids) {
    const Bytes d = svrfmt::Unpack(svrfmt::ArchiveData(k));
    std::printf("%s/%s %zu %.4s\n", k.group.c_str(), k.name.c_str(), d.size(), d.size() >= 4 ? (const char *)d.data() : "");
    if (argc > 2) {
      std::string g = k.group, n = k.name;
      for (auto *s : {&g, &n})
        for (auto &c : *s)
          if (c == ' ' || c == '/' || c == '\\')
            c = '_';
      std::filesystem::create_directories(argv[2]);
      std::ofstream(std::filesystem::path(argv[2]) / (g + "_" + n + ".bin"), std::ios::binary)
          .write((const char *)d.data(), std::streamsize(d.size()));
    }
  }
  return 0;
}
