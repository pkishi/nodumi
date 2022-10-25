#pragma once

#if defined(LOCRAY)
  #include "../dpd/raylib/src/raylib.h"
#else
  #include <raylib.h>
#endif

#include <string>
#include <vector>
#include "color.h"
#include "note.h"
#include "box.h"

using std::vector;
using std::string;
using std::to_string;
using std::is_same;

double getDistance(int x1, int y1, int x2, int y2);

template<class T> 
T clampValue(T val, T lo, T hi) {
  return val > hi ? hi : (val < lo ? lo : val);
}

template<class T>
bool pointInBox(T pt, rect box) {
  if constexpr (is_same<T, Vector2>::value || is_same<T, point>::value) {
    return static_cast<int>(pt.x) >= box.x && static_cast<int>(pt.x) < box.x + box.width &&
           static_cast<int>(pt.y) >= box.y && static_cast<int>(pt.y) < box.y + box.height;
  }
  
  logW(LL_WARN, "call to", __func__, "using invalid type", typeid(T).name());
  return false;
}
rect pointToRect(point a, point b);

string colorToHex(colorRGB col);

string getNoteInfo(int noteTrack, int notePos);

string getSongPercent (double pos, double total);
string getSongTime(double pos,  double total);
string toMinutes(double seconds);

vector<string>& formatPortName(vector<string>& ports);

bool isMKI(string path);
bool isValidPath(string path, int pathType);
string getExtension(string path);

string toHex(int dec);
