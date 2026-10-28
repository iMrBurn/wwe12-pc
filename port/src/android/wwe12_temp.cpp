// Version Android : température du téléphone pour le compteur FPS (contribution de Yexclsv).
// TempMonitor.java lit la température de la batterie (la seule qu'Android donne aux applis, en
// dixièmes de degré) et l'envoie ici ; wwe12_display.cpp l'affiche après les FPS.
#include <jni.h>

#include <atomic>

namespace {
std::atomic<int> g_tenths{-1000};  // -1000 = pas de valeur (réglage coupé)
}  // namespace

int Wwe12PhoneTempTenths() { return g_tenths.load(std::memory_order_relaxed); }

extern "C" JNIEXPORT void JNICALL Java_io_github_imrburn_wwe12_TempMonitor_nativeSetTemp(JNIEnv *, jclass,
                                                                                       jint tenths) {
  g_tenths.store(tenths, std::memory_order_relaxed);
}
