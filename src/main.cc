#include <algorithm>
#include <bit>
#include <ctime>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "aghfile.h"
#include "box.h"
#include "build_target.h"
#include "color.h"
#include "controller.h"
#include "data.h"
#include "define.h"
#include "draw.h"
#include "enum.h"
#include "fft.h"
#include "gl_compat.h"
#include "image.h"
#include "lerp.h"
#include "log.h"
#include "menu.h"
#include "menuctr.h"
#include "misc.h"
#include "wrap.h"

using std::countl_zero;
using std::max;
using std::min;
using std::string;
using std::to_string;
using std::vector;

controller ctr;

int main(int argc, char* argv[]) {
#if defined(TARGET_REL)
  SetTraceLogLevel(LOG_ERROR);
#else
  // debug
  SetTraceLogLevel(LOG_ERROR);
  // SetTraceLogLevel(LOG_DEBUG);
#endif

  // basic window setup
  const string windowTitle = string(W_NAME) + " " + string(W_VER);

// allow resizing of window on windows
#if defined(TARGET_WIN)
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
#else
  SetConfigFlags(FLAG_MSAA_4X_HINT);
#endif

  InitWindow(W_WIDTH, W_HEIGHT, windowTitle.c_str());
  SetExitKey(KEY_F7);
  SetWindowMinSize(W_WIDTH, W_HEIGHT);

  // no packed executable data can be loaded before this point
  ctr.init(assetSet);

  // program-wide variables

  // scaling settings
  double zoomLevel = 0.125;
  double timeOffset = 0;
  const double shiftC = 2.5;
  double pauseOffset = 0;

  // view settings
  bool nowLine = true;
  bool nowMove = false;
  bool showFPS = true;
  bool showImage = true;
  bool colorMove = false;
  bool colorSquare = false;
  bool colorCircle = false;
  bool sheetMusicDisplay = false;

  bool measureLine = true;
  bool measureNumber = true;

  bool showKey = false;
  bool showTempo = false;
  int songTimeType = SONGTIME_NONE;
  int tonicOffset = 0;
  int displayMode = DISPLAY_VORONOI;

  double nowLineX = ctr.getWidth() / 2.0f;

  string FPSText = "";

  enumChecker<hoverType> hoverType;
  ACTION action = ACTION::NONE;

  // color variables
  int selectType = SELECT_NONE;
  int colorMode = COLOR_PART;
  vector<colorRGB>& colorSetOn = ctr.setTrackOn;
  vector<colorRGB>& colorSetOff = ctr.setTrackOff;

  bool clearFile = false;

  // right click variables
  int clickNote = -1;
  int clickTmp = -1;
  bool clickOn = false;
  bool clickOnTmp = false;

  // screen space conversion functions
  const auto convertSSX = [&](int value) { return nowLineX + (value - timeOffset) * zoomLevel; };

  const auto convertSSY = [&](int value) {
    return (ctr.getHeight() -
            (ctr.getHeight() - (ctr.topHeight)) * static_cast<double>(value - MIN_NOTE_IDX + 3) / (NOTE_RANGE + 4));
  };

  // inverse screen space conversion functions
  const auto unconvertSSX = [&](int value) { return timeOffset + (value - nowLineX) / zoomLevel; };

  const auto inverseSSX = [&]() {
    // double spaceR = ctr.getWidth() - nowLineX;
    double minVal = max(0.0, timeOffset - nowLineX / zoomLevel);

    // extra "1" prevents roundoff error
    double maxVal = min(ctr.getLastTime(), static_cast<int>(timeOffset + (ctr.getWidth() + 1 - nowLineX) / zoomLevel));

    return make_pair(minVal, maxVal);
  };

  // menu variables
  constexpr int tl_spacing = 4;
  Vector2 songTimePosition = {6.0f, 26.0f};

  // menu objects
  menu fileMenu(ctr.getSize(), CONTENT_FILE, TYPE_MAIN, ctr.menu.getOffset(), 0);

  menu editMenu(ctr.getSize(), CONTENT_EDIT, TYPE_MAIN, ctr.menu.getOffset(), 0);

  menu viewMenu(ctr.getSize(), CONTENT_VIEW, TYPE_MAIN, ctr.menu.getOffset(), 0);

  menu displayMenu(ctr.getSize(), CONTENT_DISPLAY, TYPE_SUB, viewMenu.getX() + viewMenu.getWidth(),
                   viewMenu.getItemY(VIEW_MENU_DISPLAY_MODE), &viewMenu, 1);

  menu songMenu(ctr.getSize(), CONTENT_SONG, TYPE_SUB, viewMenu.getX() + viewMenu.getWidth(),
                viewMenu.getItemY(VIEW_MENU_SONG_TIME), &viewMenu, 2);

  menu midiMenu(ctr.getSize(), CONTENT_MIDI, TYPE_MAIN, ctr.menu.getOffset(), 0);

  menu inputMenu(ctr.getSize(), CONTENT_INPUT, TYPE_SUB, midiMenu.getX() + midiMenu.getWidth(),
                 midiMenu.getItemY(MIDI_MENU_INPUT), &midiMenu, 1);

  menu outputMenu(ctr.getSize(), CONTENT_OUTPUT, TYPE_SUB, midiMenu.getX() + midiMenu.getWidth(),
                  midiMenu.getItemY(MIDI_MENU_OUTPUT), &midiMenu, 2);

  menu colorMenu(ctr.getSize(), CONTENT_COLOR, TYPE_MAIN, ctr.menu.getOffset(), 0);

  menu schemeMenu(ctr.getSize(), CONTENT_SCHEME, TYPE_SUB, colorMenu.getX() + colorMenu.getWidth(),
                  colorMenu.getItemY(COLOR_MENU_COLOR_BY), &colorMenu, 1);

  menu infoMenu(ctr.getSize(), CONTENT_INFO, TYPE_MAIN, ctr.menu.getOffset(), 0);

  menu paletteMenu(ctr.getSize(), CONTENT_PALETTE, TYPE_SUB, colorMenu.getX() + colorMenu.getWidth(),
                   colorMenu.getItemY(COLOR_MENU_COLOR_SCHEME), &colorMenu, 2);

  vector<string> rightMenuContents = ctr.generateMenuLabels(CONTENT_RIGHT);
  menu rightMenu(ctr.getSize(), CONTENT_RIGHT, TYPE_RIGHT, -100, -100);

  menu colorSelect(ctr.getSize(), CONTENT_COLORSELECT, TYPE_COLOR, -100, -100, &rightMenu, 1);

  if (argc >= 2) {
    // input parameter order is arbitrary
    ctr.updateFiles(&argv[1], argc - 1);
  }

  // DEBUG ONLY
  // ctr.toggleLivePlay();
  // ctr.input.openPort(1);
  // ctr.output.openPort(3);

  // main program logic
  while (ctr.getProgramState()) {
    if (ctr.open_file.pending() || clearFile) {
      ctr.run = false;
      timeOffset = 0;
      pauseOffset = 0;

      if (ctr.open_file.pending()) {
        ctr.load(ctr.open_file.getPath(), nowLine, showFPS, showImage, sheetMusicDisplay, measureLine, measureNumber,
                 colorMode, displayMode, songTimeType, tonicOffset, zoomLevel);

        if (ctr.getFileType() == FILE_MKI) {
          ctr.save_file.setPending(ctr.open_file.getPath());
        }

        ctr.open_file.reset();
      }
      if (clearFile) {
        clearFile = false;
        ctr.clear();
      }
    }

    if (ctr.open_image.pending()) {
      ctr.image.load(ctr.open_image.getPath());
      ctr.open_image.reset();
    }

    if (ctr.getLiveState()) {
      timeOffset = ctr.livePlayOffset;
      ctr.run = false;
    }
    // empty input queue even if not rendering live input
    ctr.input.update();

    // fix FPS count bug
    GetFPS();

    // preprocess variables
    clickTmp = -1;
    hoverType.clear();
    const vector<note>& notes = ctr.getNotes();
    midi& stream = ctr.getStream();
    unsigned int tl_offset = 4;

    // update menu variables
    if (sheetMusicDisplay) {
      songTimePosition.y = 26.0f + ctr.barHeight;
      ctr.topHeight = ctr.barHeight + ctr.menuHeight;
    }
    else {
      songTimePosition.y = 26.0f;
      ctr.topHeight = ctr.menuHeight;
    }

    // update hover status
    if (ctr.dialog.hover()) {
      hoverType.add(HOVER_DIALOG);
    }

    // main render loop
    BeginDrawing();
    clearBackground(ctr.bgColor);

    if (showImage) {
      ctr.image.render();
      if (!hoverType.contains(HOVER_DIALOG)) {
        if (getMousePosition().x > ctr.image.getX() && getMousePosition().x < ctr.image.getX() + ctr.image.getWidth()) {
          if (getMousePosition().y > ctr.image.getY() &&
              getMousePosition().y < ctr.image.getY() + ctr.image.getHeight()) {
            hoverType.add(HOVER_IMAGE);
          }
        }
      }
    }

    switch (displayMode) {
      case DISPLAY_VORONOI:
        if (ctr.voronoi.vertex.size() != 0) {
          int voro_y = ctr.menuHeight + (sheetMusicDisplay ? ctr.sheetHeight : 0);
          ctr.voronoi.resample(voro_y);
          ctr.voronoi.render();
        }
        break;
    }

    switch (displayMode) {
      case DISPLAY_VORONOI:
        ctr.beginBlendMode(CGL_ONE_MINUS_DST_COLOR, CGL_ZERO, CGL_ADD);
        ctr.beginShaderMode("SH_INVERT");
        break;
    }

    int lastMeasureNum = 0;
    double measureSpacing = measureTextEx(to_string(stream.measureMap.size() - 1)).x;

    if (measureLine || measureNumber) {
      constexpr int maxMeasureSpacing = 5;
      for (unsigned int i = 0; i < stream.measureMap.size(); i++) {
        float measureLineWidth = 0.5;
        int measureLineY = ctr.menuHeight + (sheetMusicDisplay ? ctr.menuHeight + ctr.sheetHeight : 0);
        double measureLineX = convertSSX(stream.measureMap[i].getLocation());
        double line_ratio = 1.0;
        if (i && i + 1 < stream.measureMap.size()) {
          double m_space = convertSSX(stream.measureMap[i + 1].getLocation()) - measureLineX;
          if (m_space < maxMeasureSpacing) {
            line_ratio = clampValue(m_space / maxMeasureSpacing, 0.0, 1.0);
          }
        }

        if (measureLine) {
          if (pointInBox(getMousePosition(),
                         {int(measureLineX - 3), measureLineY, 6, ctr.getHeight() - measureLineY}) &&
              !hoverType.containsLastFrame(HOVER_MENU) && !hoverType.contains(HOVER_DIALOG, HOVER_MEASURE)) {
            measureLineWidth = 1;
            hoverType.add(HOVER_MEASURE);
          }
          if (!nowLine || fabs(nowLineX - measureLineX) > 3) {
            drawLineEx(static_cast<int>(measureLineX), measureLineY, static_cast<int>(measureLineX), ctr.getHeight(),
                       measureLineWidth, ctr.bgMeasure, 255 * line_ratio);
          }
        }

        if (measureNumber) {
          double lastMeasureLocation = convertSSX(stream.measureMap[lastMeasureNum].getLocation());
          if (!i || lastMeasureLocation + measureSpacing + 10 < measureLineX) {
            // measure number / song time + key sig collision detection
            Vector2 songInfoSize = {0, 0};
            switch (songTimeType) {
              case SONGTIME_ABSOLUTE:
                songInfoSize = measureTextEx(getSongTime(timeOffset));
                break;
              case SONGTIME_RELATIVE:
                songInfoSize = measureTextEx(getSongPercent(timeOffset));
                break;
            }
            if (showKey) {
              // approximate actual rendered label width, it is usually good
              // enough
              Vector2 keySigSize = measureTextEx(ctr.getKeySigLabel(timeOffset));
              songInfoSize.x += keySigSize.x;
              songInfoSize.y += keySigSize.y;

              if (songTimeType != SONGTIME_NONE) {
                songInfoSize.x += tl_spacing;
              }
            }
            if (showTempo && !ctr.getLiveState()) {
              songInfoSize.x += measureTextEx(ctr.getTempoLabel(timeOffset)).x;
              songInfoSize.x += tl_spacing;
              if (songTimeType != SONGTIME_NONE || showKey) {
                songInfoSize.x += tl_spacing;
              }
            }

            double fadeWidth = 2.0 * measureSpacing;
            int measureLineTextAlpha = 255 * (min(fadeWidth, ctr.getWidth() - measureLineX)) / fadeWidth;

            if (((showTempo && !ctr.getLiveState()) || showKey || songTimeType != SONGTIME_NONE) &&
                measureLineX + 4 < songInfoSize.x + fadeWidth / 2.0) {
              measureLineTextAlpha =
                  max(0.0, min(255.0, 255.0 * (1 - (songInfoSize.x + fadeWidth / 2.0 - measureLineX - 4) / 10)));
            }
            else if (measureLineX < fadeWidth) {
              measureLineTextAlpha = 255 * max(0.0, (min(fadeWidth, measureLineX + measureSpacing)) / fadeWidth);
            }

            int measureTextY = ctr.menuHeight + 4 + (sheetMusicDisplay ? ctr.sheetHeight + ctr.menuHeight : 0);
            drawTextEx(to_string(i + 1), measureLineX + 4, measureTextY, ctr.bgColor2, measureLineTextAlpha);
            lastMeasureNum = i;
          }
        }
      }
    }

    switch (displayMode) {
      case DISPLAY_VORONOI:
        ctr.endShaderMode();
        ctr.endBlendMode();
        break;
    }

    if (nowLine) {
      float nowLineWidth = 0.5;
      int nowLineY = ctr.menuHeight + (sheetMusicDisplay ? ctr.menuHeight + ctr.sheetHeight : 0);
      if (pointInBox(getMousePosition(), {int(nowLineX - 3), nowLineY, 6, ctr.getHeight() - ctr.barHeight}) &&
          !ctr.menu.mouseOnMenu() && !hoverType.contains(HOVER_DIALOG)) {
        nowLineWidth = 1;
        hoverType.add(HOVER_NOW);
      }
      drawLineEx(nowLineX, nowLineY, nowLineX, ctr.getHeight(), nowLineWidth, ctr.bgNow);
    }

    switch (displayMode) {
      case DISPLAY_VORONOI:
        break;
      default:
        if (ctr.option.get(OPTION::SHADOW)) {
          ctr.beginTextureMode(ctr.shadow.buffer);
          clearBackground();
        }
    }

    auto getColorSet = [&](int idx) {
      switch (colorMode) {
        case COLOR_PART:
          return notes[idx].track;
        case COLOR_VELOCITY:
          if (ctr.option.get(OPTION::SCALE_VELOCITY) && !ctr.getLiveState() &&
              stream.velocityBounds.first != stream.velocityBounds.second) {
            int range = stream.velocityBounds.second - stream.velocityBounds.first;
            return static_cast<int>((notes[idx].velocity - stream.velocityBounds.first) * (127.0 / range));
          }
          else {
            return notes[idx].velocity;
          }
        case COLOR_TONIC:
          return (notes[idx].y - MIN_NOTE_IDX + tonicOffset) % 12;
      }
      return 0;
    };

    pair<double, double> currentBoundaries = inverseSSX();
    vector<int> current_note;

    // note rendering
    for (int i = 0; i < ctr.getNoteCount(); i++) {
      auto break_line = [&]() {
        switch (displayMode) {
          case DISPLAY_LINE:
          case DISPLAY_PULSE:
          case DISPLAY_LOOP:
            return i != 0;
          default:
            return false;
        }
      };
      if (break_line()) {
        break;
      }

      if (!ctr.getLiveState()) {
        if (notes[i].x < currentBoundaries.first * 0.9 && notes[i].x > currentBoundaries.second * 1.1) {
          continue;
        }
      }

      bool noteOn = false;

      const auto updateClickIndex = [&](int clickIndex = -1) {
        if (!hoverType.contains(HOVER_DIALOG)) {
          clickOnTmp = noteOn;
          noteOn = !noteOn;
          clickTmp = clickIndex == -1 ? i : clickIndex;
          hoverType.add(HOVER_NOTE);
        }
      };

      float cX = convertSSX(notes[i].x);
      float cY = convertSSY(notes[i].y);
      float cW = notes[i].duration * zoomLevel < 1 ? 1 : notes[i].duration * zoomLevel;
      float cH = (ctr.getHeight() - ctr.menuHeight) / 88.0f;

      switch (displayMode) {
        case DISPLAY_BAR: {
          int colorID = getColorSet(i);
          if (notes[i].isOn || (timeOffset >= notes[i].x && timeOffset < notes[i].x + notes[i].duration)) {
            noteOn = true;
          }

          if (pointInBox(getMousePosition(), (rect){int(cX), int(cY), int(cW), int(cH)}) && !ctr.menu.mouseOnMenu()) {
            updateClickIndex();
          }

          auto cSet = noteOn ? colorSetOn : colorSetOff;
          auto cSetInv = !noteOn ? colorSetOn : colorSetOff;
          const auto& col = cSet[colorID];
          const auto& col_inv = cSetInv[colorID];

          if (timeOffset >= notes[i].x && timeOffset < notes[i].x + notes[i].duration) {
            ctr.particle.add_emitter(i, {nowLineX, cY, 0, cH, col, col_inv});
          }

          if (noteOn) {
          }

          drawRectangle(cX, cY, cW, cH, col);
        } break;
        case DISPLAY_VORONOI:
          if (cX > -0.2 * ctr.getWidth() && cX + cW < 1.2 * ctr.getWidth()) {
            int colorID = getColorSet(i);
            if (notes[i].isOn || (timeOffset >= notes[i].x && timeOffset < notes[i].x + notes[i].duration)) {
              noteOn = true;
            }

            // float radius = -1 + 2 * (32 - countl_zero(int(cW)));
            float radius = 1 + log2(cW + 1);
            if (getDistance(ctr.getMouseX(), ctr.getMouseY(), cX, cY + cH / 2) < radius + 2) {
              updateClickIndex();
            }

            auto cSet = noteOn ? colorSetOn : colorSetOff;
            auto cSetInv = !noteOn ? colorSetOn : colorSetOff;
            const auto& col = cSet[colorID];
            const auto& col_inv = cSetInv[colorID];

            ctr.voronoi.vertex.push_back({cX / ctr.getWidth(), 1 - (cY + cH / 2) / ctr.getHeight()});
            ctr.voronoi.color.push_back(col);

            if (timeOffset >= notes[i].x && timeOffset < notes[i].x + notes[i].duration) {
              ctr.particle.add_emitter(i, {cX, (cY + cH / 2) - radius / 2.0, 0, radius, col, col_inv});
            }

            drawRing({cX, (cY + cH / 2)}, radius - 1, radius + 2, ctr.bgDark);
            drawRing({cX, (cY + cH / 2)}, 0, radius, col);
          }
          break;
        case DISPLAY_BALL: {
          int colorID = getColorSet(i);
          auto cSet = noteOn ? colorSetOn : colorSetOff;
          auto cSetInv = !noteOn ? colorSetOn : colorSetOff;
          const auto& col = cSet[colorID];
          const auto& col_inv = cSetInv[colorID];
          float radius = -1 + 2 * (32 - countl_zero(static_cast<unsigned int>(cW)));
          float maxRad = radius;
          float ballY = cY + 2;
          if (cX + cW + radius > 0 && cX - radius < ctr.getWidth()) {
            if (cX < nowLineX - cW) {
              radius *= 0.3;
            }
            if (notes[i].isOn || (timeOffset >= notes[i].x && timeOffset < notes[i].x + notes[i].duration)) {
              noteOn = true;
              radius *= (0.3f + 0.7f * (1.0f - float(timeOffset - notes[i].x) / notes[i].duration));
            }
            if (!ctr.menu.mouseOnMenu()) {
              int realX = 0;
              if (cX > nowLineX) {
                realX = cX;
              }
              else if (cX + cW < nowLineX) {
                realX = cX + cW;
              }
              else {
                realX = nowLineX;
              }
              if (getDistance(ctr.getMouseX(), ctr.getMouseY(), realX, ballY) < radius) {
                updateClickIndex();
              }
              else if (realX == nowLineX && (getDistance(ctr.getMouseX(), ctr.getMouseY(), cX, ballY) < radius ||
                                             pointInBox(getMousePosition(), (rect){int(cX), int(ballY) - 2,
                                                                                   max(int(nowLineX - cX), 0), 4}))) {
                updateClickIndex();
              }
            }

            if (timeOffset >= notes[i].x && timeOffset < notes[i].x + notes[i].duration) {
              ctr.particle.add_emitter(i, {nowLineX, ballY, 0, 0, col, col_inv});
            }

            if (noteOn) {
              if (cX >= nowLineX) {
                drawRing({cX, ballY}, radius - 2, radius, col);
              }
              else if (cX + cW < nowLineX) {
                drawRing({cX + cW, ballY}, radius - 2, radius, col);
              }
              else if (cX < nowLineX) {
                drawRing({cX, ballY}, radius - 2, radius, col, 255 * radius / maxRad);
                drawRing({static_cast<float>(nowLineX), ballY}, radius - 2, radius, col);
                if (nowLineX - cX > 2 * radius) {
                  drawGradientLineH({cX + radius, ballY + 1}, {static_cast<float>(nowLineX) - radius + 1, ballY + 1}, 2,
                                    col, 255, 255 * radius / maxRad);
                }
              }
            }
            else {
              if (cX < nowLineX && cX + cW > nowLineX) {
                drawRing({cX, ballY}, radius - 2, radius, col_inv, 255 * radius / maxRad);
                drawRing({static_cast<float>(nowLineX), ballY}, radius - 2, radius, col_inv);
                if (nowLineX - cX > 2 * radius) {
                  drawLineEx(cX + radius, ballY + 1, nowLineX - radius, ballY + 1, 2, col_inv);
                }
              }
              else if (cX < nowLineX) {
                drawRing({cX + cW, ballY}, radius - 2, radius, col_inv);
              }
              else {
                drawRing({cX, ballY}, radius - 2, radius, col_inv);
              }
            }
            // drawSymbol(SYM_TREBLE, 75, cX,cY, col_inv);
          }
        } break;
        case DISPLAY_LINE: {
          const vector<lineData>& lp = stream.getLines();
          for (unsigned int j = 0; j < lp.size(); ++j) {
            if (!ctr.getLiveState()) {
              if (convertSSX(lp[j].x_r) < 0) {
                continue;
              }
              if (convertSSX(lp[j].x_l) > ctr.getWidth()) {
                break;
              }
            }
            int colorID = getColorSet(lp[j].idx);

            float convSS[4] = {static_cast<float>(convertSSX(lp[j].x_l)), static_cast<float>(convertSSY(lp[j].y_l)),
                               static_cast<float>(convertSSX(lp[j].x_r)), static_cast<float>(convertSSY(lp[j].y_r))};
            if (lp[j].in_progress) {
              convSS[2] = nowLineX;
            }

            noteOn = convSS[0] <= nowLineX && convSS[2] > nowLineX;
            if (pointInBox(getMousePosition(),
                           pointToRect({static_cast<int>(convSS[0]), static_cast<int>(convSS[1])},
                                       {static_cast<int>(convSS[2]), static_cast<int>(convSS[3])}))) {
              updateClickIndex(lp[j].idx);
            }

            auto cSet = noteOn ? colorSetOn : colorSetOff;
            auto cSetInv = !noteOn ? colorSetOn : colorSetOff;
            const auto& col = cSet[colorID];
            const auto& col_inv = cSetInv[colorID];

            if (timeOffset >= notes[lp[j].idx].x && timeOffset < notes[lp[j].idx].x + notes[lp[j].idx].duration) {
              ctr.particle.add_emitter(lp[j].idx, {convSS[0], convSS[1], 0, 0, col, col_inv});
            }

            if (convSS[2] - convSS[0] > 3) {
              drawLineBezier(convSS[0], convSS[1], convSS[2], convSS[3], 2, col);
            }
            else {
              drawLineEx(convSS[0], convSS[1], convSS[2], convSS[3], 2, col);
            }
          }
        } break;
        case DISPLAY_PULSE: {
          const vector<lineData>& lp = stream.getLines();
          for (unsigned int j = 0; j < lp.size(); ++j) {
            if (!ctr.getLiveState()) {
              if (convertSSX(lp[j].x_r) < 0) {
                continue;
              }
              if (convertSSX(lp[j].x_l) > ctr.getWidth()) {
                break;
              }
            }
            int colorID = getColorSet(lp[j].idx);

            float convSS[4] = {static_cast<float>(convertSSX(lp[j].x_l)), static_cast<float>(convertSSY(lp[j].y_l)),
                               static_cast<float>(convertSSX(lp[j].x_r)), static_cast<float>(convertSSY(lp[j].y_r))};
            if (lp[j].in_progress) {
              convSS[2] = nowLineX;
            }

            noteOn = convSS[0] <= nowLineX && convSS[2] > nowLineX;
            if (pointInBox(getMousePosition(),
                           pointToRect({static_cast<int>(convSS[0]), static_cast<int>(convSS[1])},
                                       {static_cast<int>(convSS[2]), static_cast<int>(convSS[3])}))) {
              updateClickIndex(lp[j].idx);
            }

            auto cSet = noteOn ? colorSetOn : colorSetOff;
            auto cSetInv = !noteOn ? colorSetOn : colorSetOff;
            const auto& col = cSet[colorID];
            const auto& col_inv = cSetInv[colorID];

            double nowRatio = (nowLineX - convSS[0]) / (convSS[2] - convSS[0]);
            if (noteOn || clickTmp == static_cast<int>(lp[j].idx)) {
              double newY = (convSS[3] - convSS[1]) * nowRatio + convSS[1];
              bool nowNote = clickTmp == static_cast<int>(lp[j].idx) ? false : noteOn;
              drawLineEx(
                  nowNote ? nowLineX - floatLERP(0, (nowLineX - convSS[0]) / 2.0, nowRatio, INT_SINE) : convSS[0],
                  nowNote ? newY - floatLERP(0, (newY - convSS[1]) / 2.0, nowRatio, INT_SINE) : convSS[1],
                  nowNote ? nowLineX - floatLERP(0, (nowLineX - convSS[2]) / 2.0, nowRatio, INT_ISINE) : convSS[2],
                  nowNote ? newY - floatLERP(0, (newY - convSS[3]) / 2.0, nowRatio, INT_ISINE) : convSS[3], 3, col);
              if (timeOffset >= notes[lp[j].idx].x && timeOffset < notes[lp[j].idx].x + notes[lp[j].idx].duration) {
                ctr.particle.add_emitter(
                    lp[j].idx,
                    {nowNote ? nowLineX - floatLERP(0, (nowLineX - convSS[2]) / 2.0, nowRatio, INT_ISINE) : convSS[2],
                     nowNote ? newY - floatLERP(0, (newY - convSS[3]) / 2.0, nowRatio, INT_ISINE) : convSS[3], 0, 0,
                     col, col_inv});
              }

              drawRing({float(nowNote ? nowLineX - floatLERP(0, (nowLineX - convSS[0]) / 2.0, nowRatio, INT_SINE)
                                      : convSS[0]),
                        float(nowNote ? newY - floatLERP(0, (newY - convSS[1]) / 2.0, nowRatio, INT_SINE) : convSS[1])},
                       0, 1.5, col);
              drawRing(
                  {float(nowNote ? nowLineX - floatLERP(0, (nowLineX - convSS[2]) / 2.0, nowRatio, INT_ISINE)
                                 : convSS[0]),
                   float(nowNote ? newY - floatLERP(0, (newY - convSS[3]) / 2.0, nowRatio, INT_ISINE) : convSS[1])},
                  0, 1.5, col);
            }

            if (convSS[2] >= nowLineX) {
              double ringFadeAlpha = noteOn ? 255 * (1 - nowRatio) : 255;
              drawRing({convSS[0], convSS[1]}, 0, 3, col, ringFadeAlpha);
            }
            if (convSS[2] <= nowLineX) {
              drawRing({convSS[2], convSS[3]}, 0, 3, col);
            }

            int ringLimit = 400;
            int ringDist = timeOffset - lp[j].x_l;

            double ringRatio = ringDist / static_cast<double>(ringLimit);
            if (ctr.run && lp[j].x_l < pauseOffset && timeOffset >= pauseOffset) {
              ringRatio = 0;
            }
            else if (ctr.getPauseTime() < 1 && timeOffset == pauseOffset) {  // || linePositions[j+1] >=
                                                                             // pauseOffset) {
              // this effect has a run-down time of 1 second
              ringRatio += min(1 - ringRatio, ctr.getPauseTime());
            }
            else if (lp[j].x_l < pauseOffset && timeOffset == pauseOffset) {
              ringRatio = 0;
              // ringRatio *= max(ctr.getRunTime(), 1.0);
            }
            // logQ(timeOffset, (linePositions[j+1], linePositions[j+2]));
            if (ringDist <= ringLimit && ringDist > 4) {
              unsigned int noteLen =
                  notes[lp[j].idx].duration * zoomLevel < 1 ? 1 : notes[lp[j].idx].duration * zoomLevel;
              noteLen = noteLen ? 32 - countl_zero(noteLen) : 0;
              double ringRad = floatLERP(6, 5 * noteLen, ringRatio, INT_ILINEAR);

              if (ringRatio > 0) {
                drawRing({convSS[0], convSS[1]}, ringRad - 3, ringRad,
                         colorLERP(col, col_inv, ringRatio, INT_ICIRCULAR),
                         floatLERP(0, 255, ringRatio, INT_ICIRCULAR));
              }
            }
          }
        } break;
        case DISPLAY_LOOP: {
          const vector<lineData>& lp = stream.getLines();
          for (unsigned int j = 0; j < lp.size(); ++j) {
            if (!ctr.getLiveState()) {
              if (convertSSX(lp[j].x_r) < 0) {
                continue;
              }
              if (convertSSX(lp[j].x_l) > ctr.getWidth()) {
                break;
              }
            }
            int colorID = getColorSet(lp[j].idx);

            float convSS[4] = {static_cast<float>(convertSSX(lp[j].x_l)), static_cast<float>(convertSSY(lp[j].y_l)),
                               static_cast<float>(convertSSX(lp[j].x_r)), static_cast<float>(convertSSY(lp[j].y_r))};
            if (lp[j].in_progress) {
              convSS[2] = nowLineX;
            }

            noteOn = convSS[0] <= nowLineX && convSS[2] > nowLineX;
            if (pointInBox(getMousePosition(),
                           pointToRect({static_cast<int>(convSS[0]), static_cast<int>(convSS[1])},
                                       {static_cast<int>(convSS[2]), static_cast<int>(convSS[3])}))) {
              updateClickIndex(lp[j].idx);
            }

            auto cSet = noteOn ? colorSetOn : colorSetOff;
            auto cSetInv = !noteOn ? colorSetOn : colorSetOff;
            const auto& col = cSet[colorID];
            const auto& col_inv = cSetInv[colorID];

            double nowRatio = (nowLineX - convSS[0]) / (convSS[2] - convSS[0]);
            double newY = (convSS[3] - convSS[1]) * nowRatio + convSS[1];
            if (clickTmp == static_cast<int>(lp[j].idx) || convSS[2] < nowLineX) {
              drawLineEx(convSS[0], convSS[1], convSS[2], convSS[3], 2, col);
            }
            else if (convSS[0] < nowLineX) {
              drawLineEx(
                  convSS[0], convSS[1],
                  noteOn ? nowLineX - floatLERP(0, (nowLineX - convSS[2]) / 2.0, nowRatio, INT_ISINE) : convSS[2],
                  noteOn ? newY - floatLERP(0, (newY - convSS[3]) / 2.0, nowRatio, INT_ISINE) : convSS[3], 3, col);
            }

            if (timeOffset >= notes[lp[j].idx].x && timeOffset < notes[lp[j].idx].x + notes[lp[j].idx].duration) {
              ctr.particle.add_emitter(
                  lp[j].idx,
                  {noteOn ? nowLineX - floatLERP(0, (nowLineX - convSS[2]) / 2.0, nowRatio, INT_ISINE) : convSS[2],
                   noteOn ? newY - floatLERP(0, (newY - convSS[3]) / 2.0, nowRatio, INT_ISINE) : convSS[3], 0, 0, col,
                   col_inv});
            }

            double scale = (1.2 * (32 - countl_zero(static_cast<unsigned int>(cW)))) / 8.0;
            drawRing({convSS[0], convSS[1]}, 0, 3 * scale, col);
            drawRing({convSS[2], convSS[3]}, 0, 3 * scale, col);
            if (convSS[2] < nowLineX) {
              drawRing({convSS[2], convSS[3]}, 4 * scale, 8 * scale, col);
            }

            if (nowRatio > 0 && nowRatio < 1) {
              drawRing({convSS[2], convSS[3]}, 4 * scale, 8 * scale, col, 255, 180.0f, (-nowRatio + 0.5f) * 360.0f);
            }
          }
        } break;
        case DISPLAY_FFT:
          if (cX + cW > 0 && cX < ctr.getWidth()) {
            bool drawFFT = false;
            double fftStretchRatio = 1.78;  // TODO: make fft-selection semi-duration-invariant
            int colorID = getColorSet(i);
            if (notes[i].isOn || (timeOffset >= notes[i].x && timeOffset < notes[i].x + notes[i].duration)) {
              noteOn = true;
              drawFFT = true;
            }
            else if ((timeOffset >= notes[i].x + notes[i].duration &&
                      timeOffset < notes[i].x + fftStretchRatio * notes[i].duration) ||
                     (timeOffset < notes[i].x && timeOffset >= notes[i].x - ctr.getMinTickLen())) {
              drawFFT = true;
            }
            if (pointInBox(getMousePosition(), (rect){int(cX), int(cY), int(cW), int(cH)}) && !ctr.menu.mouseOnMenu()) {
              updateClickIndex();
            }

            auto cSet = noteOn ? colorSetOn : colorSetOff;
            auto cSetInv = !noteOn ? colorSetOn : colorSetOff;
            const auto& col = cSet[colorID];
            const auto& col_inv = cSetInv[colorID];

            if (timeOffset >= notes[i].x && timeOffset < notes[i].x + notes[i].duration) {
              ctr.particle.add_emitter(i, {nowLineX, cY, 0, cH, col, col_inv});
            }

            if (drawFFT) {
              current_note.push_back(i);
            }

            drawRectangle(cX, cY, cW, cH, col);
          }
          break;
      }
    }

    // render FFT lines after notes
    if (displayMode == DISPLAY_FFT) {
      // int pf_calls = 0;
      //  must obtain last bins before dispatching next set
      const auto bins = ctr.fft.getFFTBins();
      ctr.fft.generateFFTBins(current_note, timeOffset);

      bool foundNote = false;
      for (unsigned int bin = 0; bin < bins.size(); ++bin) {
        for (const auto& note_pair : bins[bin]) {
          const int idx = note_pair.first;
          const int bin_len = note_pair.second;
          int colorID = getColorSet(idx);

          if (bin_len >= 1) {
            int startX = FFT_BIN_WIDTH * (bin + 1);
            auto cSet = colorSetOn;

            // collision
            if (clickTmp == idx ||
                (!foundNote && !hoverType.contains(HOVER_DIALOG) &&
                 pointInBox(getMousePosition(),
                            {startX - 3, static_cast<int>(ctr.getHeight() - ctr.fft.bins[bin].second - bin_len), 7,
                             static_cast<int>(bin_len)}))) {
              foundNote = true;
              cSet = colorSetOff;
              hoverType.add(HOVER_NOTE);
              clickOnTmp = true;
              clickTmp = idx;
            }

            // pf_calls++;
            drawLineEx(startX, ctr.getHeight() - ctr.fft.bins[bin].second, startX,
                       ctr.getHeight() - ctr.fft.bins[bin].second - bin_len, 1, cSet[colorID]);
            ctr.fft.bins[bin].second += bin_len;
          }
        }
      }
      // logQ("pf_calls:", pf_calls);
      // logQ("current_note size:", current_note.size());
    }

    // particle handling
    if (ctr.option.get(OPTION::PARTICLE)) {
      if (!ctr.run) {
        ctr.particle.end_emission();
      }
      else {
        ctr.particle.process();
      }
      ctr.particle.render();
    }

    switch (displayMode) {
      case DISPLAY_VORONOI:
        break;
      default:
        if (ctr.option.get(OPTION::SHADOW)) {
          ctr.endTextureMode();

          ctr.beginShaderMode("SH_SHADOW");
          constexpr double shadow_angle = M_PI / 4.0;
          float shadow_off_x = -ctr.option.get(OPTION::SHADOW_DISTANCE) * cos(shadow_angle);
          float shadow_off_y = ctr.option.get(OPTION::SHADOW_DISTANCE) * sin(shadow_angle);
          DrawTextureRec(ctr.shadow.buffer.texture,
                         {0, 0, float(ctr.shadow.buffer.texture.width), float(-ctr.shadow.buffer.texture.height)},
                         {shadow_off_x, shadow_off_y}, WHITE);
          ctr.endShaderMode();

          DrawTextureRec(ctr.shadow.buffer.texture,
                         {0, 0, float(ctr.shadow.buffer.texture.width), float(-ctr.shadow.buffer.texture.height)},
                         {0, 0}, WHITE);
        }
    }

    // menu bar rendering
    drawRectangle(0, 0, ctr.getWidth(), ctr.menuHeight, ctr.bgMenu);

    // sheet music layout
    if (sheetMusicDisplay) {
      // bg
      drawRectangle(0, ctr.menuHeight, ctr.getWidth(), ctr.barHeight, ctr.bgSheet);
      if (pointInBox(getMousePosition(), {0, ctr.menuHeight, ctr.getWidth(), ctr.barHeight}) &&
          !hoverType.contains(HOVER_DIALOG)) {
        hoverType.add(HOVER_SHEET);
      }

      // stave lines
      for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 5; j++) {
          drawLineEx(ctr.sheetSideMargin, ctr.menuHeight + ctr.barMargin + i * ctr.barSpacing + j * ctr.barWidth,
                     ctr.getWidth() - ctr.sheetSideMargin,
                     ctr.menuHeight + ctr.barMargin + i * ctr.barSpacing + j * ctr.barWidth, 1, ctr.bgSheetNote);
        }
      }

      //// end lines
      drawLineEx(ctr.sheetSideMargin, ctr.menuHeight + ctr.barMargin, ctr.sheetSideMargin,
                 ctr.menuHeight + ctr.barMargin + 4 * ctr.barWidth + ctr.barSpacing, 2, ctr.bgSheetNote);
      drawLineEx(ctr.getWidth() - ctr.sheetSideMargin, ctr.menuHeight + ctr.barMargin,
                 ctr.getWidth() - ctr.sheetSideMargin,
                 ctr.menuHeight + ctr.barMargin + 4 * ctr.barWidth + ctr.barSpacing, 2, ctr.bgSheetNote);

      drawSymbol(SYM_STAFF_BRACE, 480, 17.0f, float(ctr.menuHeight + ctr.barMargin) - 120, ctr.bgSheetNote);
      drawSymbol(SYM_CLEF_TREBLE, 155, 40.0f, ctr.menuHeight + ctr.barMargin - 47, ctr.bgSheetNote);
      drawSymbol(SYM_CLEF_BASS, 155, 40.0f, float(ctr.menuHeight + ctr.barSpacing + ctr.barMargin - 67),
                 ctr.bgSheetNote);

      if (stream.measureMap.size() != 0) {
        stream.sheetData.drawSheetPage();
      }

      if (isKeyPressed(KEY_F)) {
      }
      // stream.sheetData.findSheetPages();
      // logQ("cloc", ctr.getCurrentMeasure(timeOffset));
      // logQ("cloc",
      // formatPair(stream.sheetData.findSheetPageLimit(ctr.getCurrentMeasure(timeOffset))));
    }

    // option actions

    string songTimeContent = "";

    switch (songTimeType) {
      case SONGTIME_RELATIVE:
        songTimeContent = getSongPercent(timeOffset);
        break;
      case SONGTIME_ABSOLUTE:
        songTimeContent = getSongTime(timeOffset);
        break;
      case SONGTIME_NONE:
        break;
      default:
        logW(LL_WARN, "invalid song time option:", songTimeType);
        break;
    }

    drawTextEx(songTimeContent, songTimePosition, ctr.bgColor2);
    tl_offset = measureTextEx(songTimeContent).x;

    // draw key signature label
    if (showKey) {
      if (songTimeType != SONGTIME_NONE) {
        tl_offset += tl_spacing;
      }
      // logQ("got label:", ctr.getKeySigLabel(timeOffset));
      string ksl = ctr.getKeySigLabel(timeOffset);
      int cKSOffset = songTimePosition.x + tl_offset;

      drawNoteLabel(ksl, cKSOffset, songTimePosition.y, 14, 74, ctr.bgColor2);

      tl_offset += measureTextEx(ctr.getKeySigLabel(timeOffset)).x;
    }

    if (showTempo && !ctr.getLiveState()) {
      tl_offset += tl_spacing;
      if (songTimeType != SONGTIME_NONE || showKey) {
        tl_offset += tl_spacing;
      }
      drawTextEx(ctr.getTempoLabel(timeOffset), tl_offset, songTimePosition.y, ctr.bgColor2);
    }

    if (!ctr.buffer.empty()) {
      string keyBuffer = ctr.buffer.read();
      Vector2 bufText = measureTextEx(keyBuffer);
      float bufX = min(100.0f, bufText.x);
      BeginScissorMode(ctr.getWidth() - bufX - 4, ctr.getHeight() - bufText.y - 4, bufX + 4, bufText.y + 4);
      drawRectangle(ctr.getWidth() - bufX - 4, ctr.getHeight() - bufText.y - 4, bufX + 4, bufText.y + 4, ctr.bgOpt);
      drawTextEx(keyBuffer, ctr.getWidth() - bufText.x - 2, ctr.getHeight() - bufText.y - 2, ctr.bgSheet);
      EndScissorMode();
    }

    double rtOffset = ctr.getWidth() - 4;

    if (showFPS) {
      if (GetTime() - (int)GetTime() < GetFrameTime()) {
        FPSText = to_string(GetFPS());
      }
      rtOffset -= measureTextEx(FPSText).x;
      drawTextEx(FPSText, rtOffset, 4, ctr.bgDark);
    }

    string zoomText = to_string(zoomLevel) + (showFPS ? " |" : "");
    rtOffset -= 1 + measureTextEx(zoomText).x;
    drawTextEx(zoomText, rtOffset, 4, ctr.bgDark);

    if (!ctr.getLiveState() && ctr.getFilePath() != "") {
      string fileText = ctr.getFilePath() + " |";
      rtOffset -= 1 + measureTextEx(fileText).x;
      drawTextEx(fileText, rtOffset, 4, ctr.bgDark);
    }

    ctr.menu.render();
    ctr.dialog.render();
    ctr.warning.render();  // warning about windows stability

    EndDrawing();

    // key actions
    action = ctr.process(action);

    if (ctr.run && !any_of(action, ACTION::NAV_PREV_MEASURE, ACTION::NAV_NEXT_MEASURE)) {
      if (timeOffset + GetFrameTime() * UNK_CST < ctr.getLastTime()) {
        timeOffset += GetFrameTime() * UNK_CST;
      }
      else {
        timeOffset = ctr.getLastTime();
        ctr.run = false;
        pauseOffset = timeOffset;
      }
    }

    if (colorMove) {
      if (colorSquare) {
        colorSelect.setSquare();
      }
      if (colorCircle) {
        colorSelect.setAngle();
      }
      switch (selectType) {
        case SELECT_NOTE:
          if (clickOn) {
            ctr.setTrackOn[notes[clickNote].track] = colorSelect.getColor();
          }
          else {
            ctr.setTrackOff[notes[clickNote].track] = colorSelect.getColor();
          }
          break;
        case SELECT_BG:
          ctr.bgColor = colorSelect.getColor();
          ctr.optimizeBGColor();
          ctr.setShaderValue("SH_VORONOI", "bg_color", ctr.bgColor);
          break;
        case SELECT_LINE:
          ctr.bgNow = colorSelect.getColor();
          break;
        case SELECT_MEASURE:
          ctr.bgMeasure = colorSelect.getColor();
          break;
        case SELECT_SHEET:
          ctr.bgSheet = colorSelect.getColor();
          break;
        case SELECT_NONE:
          break;
        default:
          selectType = SELECT_NONE;
          break;
      }
    }

    switch (action) {
      case ACTION::OPEN:
        // logQ("offset before:", timeOffset);
        ctr.open_file.dialog();
        ctr.menu.hide();
        // logQ("offset after:", timeOffset);
        break;
      case ACTION::OPEN_IMAGE:
        ctr.open_image.dialog();
        showImage = true;
        viewMenu.setContentLabel("VIEW_MENU_HIDE_BACKGROUND", VIEW_MENU_BACKGROUND);
        ctr.menu.hide();
        break;
      case ACTION::CLOSE:
        clearFile = true;
        break;
      case ACTION::CLOSE_IMAGE:
        ctr.image.unloadData();
        break;
      case ACTION::SAVE:
        if (ctr.getPlayState()) {
          break;
        }
        if (ctr.getFileType() == FILE_MKI) {
          ctr.save(ctr.save_file.getPath(), nowLine, showFPS, showImage, sheetMusicDisplay, measureLine, measureNumber,
                   colorMode, displayMode, songTimeType, tonicOffset, zoomLevel);
          break;
        }
        [[fallthrough]];
      case ACTION::SAVE_AS:
        if (!ctr.getPlayState() && ctr.getFileType() != FILE_NONE) {
          ctr.save_file.dialog();
          if (ctr.save_file.pending()) {
            string save_path = ctr.save_file.getPath();
            if (getExtension(save_path, true) != ".mki") {
              save_path += ".mki";
            }

            ctr.save(save_path, nowLine, showFPS, showImage, sheetMusicDisplay, measureLine, measureNumber, colorMode,
                     displayMode, songTimeType, tonicOffset, zoomLevel);
            ctr.save_file.resetPending();
          }
        }
        break;
      case ACTION::RELOAD:
        if (!ctr.getLiveState()) {
          ctr.open_file.setPending(ctr.getFileFullPath());
        }
        break;
      case ACTION::EXIT:
        ctr.setCloseFlag();
        break;
      case ACTION::SHEET:
        editMenu.swapLabel("EDIT_MENU_DISABLE_SHEET_MUSIC", "EDIT_MENU_ENABLE_SHEET_MUSIC", EDIT_MENU_SHEET_MUSIC);
        sheetMusicDisplay = !sheetMusicDisplay;
        ctr.barHeight = sheetMusicDisplay ? ctr.menuHeight + ctr.sheetHeight : ctr.menuHeight;
        break;
      case ACTION::PREFERENCES:
        ctr.dialog.clear_invert_status(DIALOG::PREFERENCES);
        break;
      case ACTION::INFO:
        ctr.dialog.clear_invert_status(DIALOG::INFO);
        break;
      case ACTION::FILE_INFO:
        ctr.dialog.clear_invert_status(DIALOG::FILE);
        break;
      case ACTION::LIVEPLAY:
        if (midiMenu.isContentLabel("MIDI_MENU_ENABLE_LIVE_PLAY", MIDI_MENU_LIVE_PLAY)) {
          zoomLevel *= 3;
        }
        else if (midiMenu.isContentLabel("MIDI_MENU_DISABLE_LIVE_PLAY", MIDI_MENU_LIVE_PLAY)) {
          zoomLevel *= 1.0 / 3.0;
        }
        midiMenu.swapLabel("MIDI_MENU_ENABLE_LIVE_PLAY", "MIDI_MENU_DISABLE_LIVE_PLAY", MIDI_MENU_LIVE_PLAY);
        ctr.toggleLivePlay();
        if (!ctr.getLiveState()) {
          timeOffset = 0;
        }
        break;
      case ACTION::CHANGE_MODE:
        for (unsigned int mode = 0; mode < min(static_cast<unsigned int>(DISPLAY_NONE), 9u); ++mode) {
          if (isKeyDown(KEY_ONE + mode)) {
            displayMode = DISPLAY_BAR + mode;
          }
          else if (ctr.pendingActionValue != -1) {
            displayMode = DISPLAY_BAR + ctr.pendingActionValue;
          }
        }
        break;
      case ACTION::NAV_ZOOM_IMAGE:
        if (ctr.image.exists() && showImage) {
          // defaults to ±1, adjusted depending on default image scale value
          float scaleModifier = 0.02f;
          if (isKeyDown(KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT)) {
            scaleModifier = 0.002f;
          }
          ctr.image.changeScale(scaleModifier * GetMouseWheelMove());
        }
        break;
      case ACTION::NAV_ZOOM_IN:
        if (zoomLevel > 0.00001) {
          zoomLevel *= (ctr.pendingActionValue ? 0.9 : 0.75);
        }
        break;
      case ACTION::NAV_ZOOM_OUT:
        if (zoomLevel < 1.2) {
          zoomLevel *= 1.0 / (ctr.pendingActionValue ? 0.9 : 0.75);
        }
        break;
      case ACTION::NAV_HOME:
        timeOffset = 0;
        break;
      case ACTION::NAV_SET_MEASURE:
        if (ctr.pendingActionValue < ctr.getMeasureCount()) {
          if (!ctr.getLiveState()) {
            double measureLineX = convertSSX(stream.measureMap[ctr.pendingActionValue - 1].getLocation());
            timeOffset = unconvertSSX(measureLineX);
          }
          break;
        }
        [[fallthrough]];
      case ACTION::NAV_END:
        timeOffset = ctr.getLastTime();
        pauseOffset = timeOffset;
        break;
      case ACTION::NAV_PREV:
        if (timeOffset > shiftC * 6) {
          timeOffset -= shiftC * 6;
        }
        else if (timeOffset > 0) {
          timeOffset = 0;
        }
        break;
      case ACTION::NAV_PREV_FAST:
        if (timeOffset > shiftC * 60) {
          timeOffset -= shiftC * 60;
        }
        else if (timeOffset > 0) {
          timeOffset = 0;
        }
        break;
      case ACTION::NAV_PREV_MEASURE:
        if (!ctr.getLiveState()) {
          int foundMeasure = ctr.findCurrentMeasure(timeOffset);
          foundMeasure -= ctr.pendingActionValue <= 0 ? 1 : ctr.pendingActionValue;
          if (foundMeasure > 0) {
            timeOffset = stream.measureMap[foundMeasure].getLocation() - 1;
          }
          else {
            timeOffset = 0;
          }
        }
        break;
      case ACTION::NAV_NEXT:
        if (timeOffset + shiftC * 6 < ctr.getLastTime()) {
          timeOffset += shiftC * 6;
        }
        else {
          timeOffset = ctr.getLastTime();
        }
        break;
      case ACTION::NAV_NEXT_FAST:

        if (timeOffset + shiftC * 60 < ctr.getLastTime()) {
          timeOffset += shiftC * 60;
        }
        else if (timeOffset < ctr.getLastTime()) {
          timeOffset = ctr.getLastTime();
        }
        break;
      case ACTION::NAV_NEXT_MEASURE:
        if (!ctr.getLiveState()) {
          int foundMeasure = ctr.findCurrentMeasure(timeOffset);
          foundMeasure += ctr.pendingActionValue <= 0 ? 1 : ctr.pendingActionValue;
          if (foundMeasure < ctr.getMeasureCount()) {
            timeOffset = stream.measureMap[foundMeasure].getLocation();
          }
          else {
            timeOffset = ctr.getLastTime();
          }
        }
        break;
      case ACTION::NAV_SPACE:
        if (timeOffset != ctr.getLastTime()) {
          ctr.run = !ctr.run;
          pauseOffset = timeOffset;
        }
        break;
      default:
        break;
    }
    action = ACTION::NONE;

    // mouse button handling
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      switch (fileMenu.getActiveElement()) {
        case MENU_INACTIVE:
          fileMenu.render = false;
          break;
        case FILE_MENU_FILE:
          fileMenu.render = !fileMenu.render;
          break;
        default:
          if (!fileMenu.render) {
            break;
          }
          switch (fileMenu.getActiveElement()) {
            case FILE_MENU_OPEN_FILE:
              action = ACTION::OPEN;
              break;
            case FILE_MENU_OPEN_IMAGE:
              action = ACTION::OPEN_IMAGE;
              break;
            case FILE_MENU_SAVE:
              action = ACTION::SAVE;
              break;
            case FILE_MENU_SAVE_AS:
              action = ACTION::SAVE_AS;
              break;
            case FILE_MENU_CLOSE_FILE:
              action = ACTION::CLOSE;
              break;
            case FILE_MENU_CLOSE_IMAGE:
              action = ACTION::CLOSE_IMAGE;
              break;
            case FILE_MENU_RELOAD:
              action = ACTION::RELOAD;
              break;
            case FILE_MENU_EXIT:
              action = ACTION::EXIT;
              break;
          }
          break;
      }
      switch (editMenu.getActiveElement()) {
        case MENU_INACTIVE:
          editMenu.render = false;
          break;
        case EDIT_MENU_EDIT:
          editMenu.render = !editMenu.render;
          break;
        default:
          if (!editMenu.render) {
            break;
          }
          switch (editMenu.getActiveElement()) {
            case EDIT_MENU_SHEET_MUSIC:
              action = ACTION::SHEET;
              break;
            case EDIT_MENU_PREFERENCES:
              action = ACTION::PREFERENCES;
              break;
          }
          break;
      }
      switch (displayMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!displayMenu.parentOpen() || displayMenu.parent->isActive()) {
            displayMenu.render = false;
          }
          break;
        default:
          if (!displayMenu.render) {
            break;
          }
          switch (displayMenu.getActiveElement()) {
            case DISPLAY_MENU_DEFAULT:
              displayMode = DISPLAY_BAR;
              break;
            case DISPLAY_MENU_LINE:
              displayMode = DISPLAY_LINE;
              break;
            case DISPLAY_MENU_PULSE:
              displayMode = DISPLAY_PULSE;
              break;
            case DISPLAY_MENU_BALL:
              displayMode = DISPLAY_BALL;
              break;
            case DISPLAY_MENU_FFT:
              displayMode = DISPLAY_FFT;
              break;
            case DISPLAY_MENU_VORONOI:
              displayMode = DISPLAY_VORONOI;
              break;
            case DISPLAY_MENU_LOOP:
              displayMode = DISPLAY_LOOP;
              break;
          }
          break;
      }
      switch (songMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!songMenu.parentOpen() || songMenu.parent->isActive()) {
            songMenu.render = false;
          }
          break;
        default:
          if (!songMenu.render) {
            break;
          }
          switch (songMenu.getActiveElement()) {
            case SONG_MENU_RELATIVE:
              songTimeType = SONGTIME_RELATIVE;
              viewMenu.setContentLabel("VIEW_MENU_HIDE_SONG_TIME", VIEW_MENU_SONG_TIME);
              break;
            case SONG_MENU_ABSOLUTE:
              songTimeType = SONGTIME_ABSOLUTE;
              viewMenu.setContentLabel("VIEW_MENU_HIDE_SONG_TIME", VIEW_MENU_SONG_TIME);
              break;
          }
          break;
      }
      switch (viewMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!viewMenu.childOpen()) {
            viewMenu.render = false;
          }
          break;
        case VIEW_MENU_VIEW:
          viewMenu.render = !viewMenu.render;
          break;
        default:
          if (!viewMenu.render) {
            break;
          }
          switch (viewMenu.getActiveElement()) {
            case VIEW_MENU_DISPLAY_MODE:
              if (viewMenu.childOpen() && displayMenu.render == false) {
                viewMenu.hideChildMenu();
                displayMenu.render = true;
              }
              else {
                displayMenu.render = !displayMenu.render;
              }
              break;
            case VIEW_MENU_SONG_TIME:
              if (viewMenu.childOpen() && songMenu.render == false) {
                viewMenu.hideChildMenu();
                songMenu.render = true;
              }
              else {
                if (viewMenu.isContentLabel("VIEW_MENU_HIDE_SONG_TIME", VIEW_MENU_SONG_TIME)) {
                  viewMenu.setContentLabel("VIEW_MENU_DISPLAY_SONG_TIME", VIEW_MENU_SONG_TIME);
                  songTimeType = SONGTIME_NONE;
                }
                else {
                  songMenu.render = !songMenu.render;
                }
              }
              break;
            case VIEW_MENU_KEY_SIGNATURE:
              viewMenu.swapLabel("VIEW_MENU_SHOW_KEY_SIGNATURE", "VIEW_MENU_HIDE_KEY_SIGNATURE",
                                 VIEW_MENU_KEY_SIGNATURE);
              showKey = !showKey;
              break;
            case VIEW_MENU_TEMPO:
              viewMenu.swapLabel("VIEW_MENU_SHOW_TEMPO", "VIEW_MENU_HIDE_TEMPO", VIEW_MENU_TEMPO);
              showTempo = !showTempo;
              break;
            case VIEW_MENU_NOW_LINE:
              viewMenu.swapLabel("VIEW_MENU_SHOW_NOW_LINE", "VIEW_MENU_HIDE_NOW_LINE", VIEW_MENU_NOW_LINE);
              nowLine = !nowLine;
              break;
            case VIEW_MENU_MEASURE_LINE:
              viewMenu.swapLabel("VIEW_MENU_SHOW_MEASURE_LINE", "VIEW_MENU_HIDE_MEASURE_LINE", VIEW_MENU_MEASURE_LINE);
              measureLine = !measureLine;
              break;
            case VIEW_MENU_MEASURE_NUMBER:
              viewMenu.swapLabel("VIEW_MENU_SHOW_MEASURE_NUMBER", "VIEW_MENU_HIDE_MEASURE_NUMBER",
                                 VIEW_MENU_MEASURE_NUMBER);
              measureNumber = !measureNumber;
              break;
            case VIEW_MENU_BACKGROUND:
              viewMenu.swapLabel("VIEW_MENU_SHOW_BACKGROUND", "VIEW_MENU_HIDE_BACKGROUND", VIEW_MENU_BACKGROUND);
              showImage = !showImage;
              break;
            case VIEW_MENU_FPS:
              viewMenu.swapLabel("VIEW_MENU_SHOW_FPS", "VIEW_MENU_HIDE_FPS", VIEW_MENU_FPS);
              showFPS = !showFPS;
              FPSText = to_string(GetFPS());
              break;
          }
          break;
      }
      switch (inputMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!inputMenu.parentOpen() || inputMenu.parent->isActive()) {
            inputMenu.render = false;
          }
          break;
        default:
          if (!inputMenu.render) {
            break;
          }
          ctr.input.openPort(inputMenu.getActiveElement());
          break;
      }
      switch (outputMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!outputMenu.parentOpen() || outputMenu.parent->isActive()) {
            outputMenu.render = false;
          }
          break;
        default:
          if (!outputMenu.render) {
            break;
          }
          ctr.output.openPort(outputMenu.getActiveElement());
          break;
      }
      switch (midiMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!midiMenu.childOpen()) {
            midiMenu.render = false;
          }
          break;
        case MIDI_MENU_MIDI:
          midiMenu.render = !midiMenu.render;
          break;
        default:
          if (!midiMenu.render) {
            break;
          }
          switch (midiMenu.getActiveElement()) {
            case MIDI_MENU_INPUT:
              inputMenu.update(ctr.input.getPorts());
              if (midiMenu.childOpen() && !inputMenu.render) {
                midiMenu.hideChildMenu();
                inputMenu.render = true;
              }
              else {
                inputMenu.render = !inputMenu.render;
              }
              break;
            case MIDI_MENU_OUTPUT:
              outputMenu.update(ctr.output.getPorts());
              if (midiMenu.childOpen() && !outputMenu.render) {
                midiMenu.hideChildMenu();
                outputMenu.render = true;
              }
              else {
                outputMenu.render = !outputMenu.render;
              }
              break;
            case MIDI_MENU_LIVE_PLAY:
              action = ACTION::LIVEPLAY;
              break;
          }
          break;
      }
      switch (schemeMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!schemeMenu.parentOpen() || schemeMenu.parent->isActive()) {
            schemeMenu.render = false;
          }
          break;
        default:
          if (!schemeMenu.render) {
            break;
          }
          switch (schemeMenu.getActiveElement()) {
            case SCHEME_MENU_PART:
              colorMode = COLOR_PART;
              colorSetOn = ctr.setTrackOn;
              colorSetOff = ctr.setTrackOff;
              break;
            case SCHEME_MENU_VELOCITY:
              colorMode = COLOR_VELOCITY;
              colorSetOn = ctr.setVelocityOn;
              colorSetOff = ctr.setVelocityOff;
              break;
            case SCHEME_MENU_TONIC:
              colorMode = COLOR_TONIC;
              colorSetOn = ctr.setTonicOn;
              colorSetOff = ctr.setTonicOff;
              break;
          }
          break;
      }
      switch (paletteMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!paletteMenu.parentOpen() || paletteMenu.parent->isActive()) {
            paletteMenu.render = false;
          }
          break;
        default:
          if (!paletteMenu.render) {
            break;
          }
          switch (paletteMenu.getActiveElement()) {
            case PALETTE_MENU_DEFAULT:
              getColorScheme(KEY_COUNT, ctr.setVelocityOn, ctr.setVelocityOff);
              getColorScheme(TONIC_COUNT, ctr.setTonicOn, ctr.setTonicOff);
              getColorScheme(ctr.getTrackCount(), ctr.setTrackOn, ctr.setTrackOff, ctr.file.trackHeightMap);
              break;
            case PALETTE_MENU_FROM_BACKGROUND:
              if (ctr.image.exists()) {
                // prevent overflow on RT audio thread
                ctr.criticalSection(true);
                getColorSchemeImage(SCHEME_KEY, ctr.setVelocityOn, ctr.setVelocityOff);
                getColorSchemeImage(SCHEME_TONIC, ctr.setTonicOn, ctr.setTonicOff);
                getColorSchemeImage(SCHEME_TRACK, ctr.setTrackOn, ctr.setTrackOff, ctr.file.trackHeightMap);
                ctr.criticalSection(false);
              }
              else {
                logW(LL_WARN, "attempt to get color scheme from nonexistent image");
              }
              break;
          }
          break;
      }
      switch (colorMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!colorMenu.childOpen()) {
            colorMenu.render = false;
          }
          break;
        case COLOR_MENU_COLOR:
          colorMenu.render = !colorMenu.render;
          break;
        default:
          if (!colorMenu.render) {
            break;
          }
          switch (colorMenu.getActiveElement()) {
            case COLOR_MENU_COLOR_BY:
              if (colorMenu.childOpen() && schemeMenu.render == false) {
                colorMenu.hideChildMenu();
                schemeMenu.render = true;
              }
              else {
                schemeMenu.render = !schemeMenu.render;
              }
              break;
            case COLOR_MENU_COLOR_SCHEME:
              if (colorMenu.childOpen() && paletteMenu.render == false) {
                colorMenu.hideChildMenu();
                paletteMenu.render = true;
              }
              else {
                paletteMenu.render = !paletteMenu.render;
              }
              break;
            case COLOR_MENU_SWAP_COLORS:
              swap(colorSetOn, colorSetOff);
              break;
            case COLOR_MENU_INVERT_COLOR_SCHEME:
              invertColorScheme(ctr.bgColor, ctr.bgNow, colorSetOn, colorSetOff);
              break;
          }
          break;
      }
      switch (infoMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!infoMenu.childOpen()) {
            infoMenu.render = false;
          }
          break;
        case INFO_MENU_INFO:
          infoMenu.render = !infoMenu.render;
          break;
        default:
          if (!infoMenu.render) {
            break;
          }
          switch (infoMenu.getActiveElement()) {
            case INFO_MENU_PROGRAM_INFO:
              action = ACTION::INFO;
              break;
            case INFO_MENU_FILE_INFO:
              action = ACTION::FILE_INFO;
              break;
            case INFO_MENU_HELP:
              infoMenu.render = false;
              OpenURL(SITE_LINK);
              break;
          }
          break;
      }
      switch (colorSelect.getActiveElement()) {
        case MENU_INACTIVE:
          if (!colorSelect.parentOpen() || colorSelect.parent->isActive()) {
            colorSelect.render = false;
          }
          break;
        default:
          if (!colorSelect.render) {
            break;
          }
          switch (colorSelect.getActiveElement()) {
            case 0:
              colorMove = true;
              if (pointInBox(getMousePosition(), colorSelect.getSquare()) || colorSelect.clickCircle(1)) {
                colorSquare = true;
              }
              else if (colorSelect.clickCircle(0)) {
                colorCircle = true;
              }
              break;
          }
          break;
      }
      switch (rightMenu.getActiveElement()) {
        case MENU_INACTIVE:
          if (!rightMenu.childOpen()) {
            rightMenu.render = false;
          }
          else {
          }
          break;
        default:
          if (!rightMenu.render) {
            break;
          }
          switch (rightMenu.getActiveElement()) {
            case 0:
              if (rightMenu.getSize() == 1) {
                if (rightMenu.childOpen() && colorSelect.render == false) {
                  rightMenu.hideChildMenu();
                }
                else {
                  if (rightMenu.isContentLabel("RIGHT_MENU_REMOVE_IMAGE", 0)) {
                    // logQ("attempted to remove image: size:");
                    ctr.image.unloadData();
                    rightMenu.render = false;
                  }
                  else {
                    colorSelect.render = !colorSelect.render;
                  }
                }
              }
              break;
            case 1:
              if (rightMenu.getSize() == 3) {
                if (rightMenu.childOpen() && colorSelect.render == false) {
                  rightMenu.hideChildMenu();
                }
                else {
                  colorSelect.render = !colorSelect.render;
                }
              }
              break;
            case 2:
              tonicOffset = (notes[clickNote].y - MIN_NOTE_IDX + tonicOffset) % 12;
              break;
          }
          break;
      }
      if (!hoverType.contains(HOVER_DIALOG) && !any_of(action, ACTION::PREFERENCES, ACTION::FILE_INFO, ACTION::INFO)) {
        ctr.dialog.clear_status();
      }

      if (!hoverType.contains(HOVER_NOW, HOVER_NOTE, HOVER_MEASURE, HOVER_MENU, HOVER_SHEET, HOVER_DIALOG) &&
          !hoverType.containsLastFrame(HOVER_MENU, HOVER_MEASURE)) {
        if (hoverType.contains(HOVER_IMAGE)) {
          ctr.image.updateBasePosition();
        }
      }

      if (ctr.dialog.get_status(DIALOG::PREFERENCES)) {
        ctr.dialog.process();
      }
    }
    // logQ("can image move?:", !hoverType.contains(HOVER_NOW, HOVER_NOTE,
    // HOVER_MEASURE, HOVER_MENU, HOVER_SHEET) &&
    //! hoverType.containsLastFrame(HOVER_MENU, HOVER_MEASURE));
    // logQ("label", rightMenuContents[1], "v.", rightMenu.getContent(0));
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
      colorMove = false;
      colorSquare = false;
      colorCircle = false;

      nowMove = false;
      ctr.image.finalizePosition();

      if (ctr.dialog.get_status(DIALOG::PREFERENCES)) {
        ctr.dialog.end_process();
      }
    }
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
      ctr.image.updatePosition();

      if (pointInBox(getMousePosition(),
                     {static_cast<int>(nowLineX - 3), ctr.barHeight, 6, ctr.getHeight() - ctr.barHeight}) &&
          !hoverType.contains(HOVER_DIALOG) && !colorSelect.render && !ctr.menu.mouseOnMenu() && !ctr.image.movable()) {
        nowMove = true;
      }
      if (nowMove && nowLine) {
        // provide reset
        if (isKeyDown(KEY_LEFT_SHIFT)) {
          nowLineX = ctr.getWidth() / 2.0;
          nowMove = false;  // disable after rest
        }
        else {
          nowLineX = clampValue(getMousePosition().x, 1, ctr.getWidth() - 1);
        }
      }
    }
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
      clickNote = clickTmp;
      clickOn = clickOnTmp;
      ctr.menu.hide();

      if (!ctr.menu.mouseOnMenu()) {
        if (!pointInBox(getMousePosition(), (rect){0, 0, ctr.getWidth(), 20})) {
          int rightX = 0, rightY = 0, colorX = 0, colorY = 0;

          if (clickNote != -1) {
            rightX = round(nowLineX + (notes[clickNote].x - timeOffset) * zoomLevel);
            rightY = (ctr.getHeight() -
                      round((ctr.getHeight() - ctr.menuHeight) *
                            static_cast<double>(notes[clickNote].y - MIN_NOTE_IDX + 3) / (NOTE_RANGE + 3)));
          }

          // find coordinate to draw right click menu
          rightMenu.findMenuLocation(rightX, rightY);
          rightMenu.findColorSelectLocation(colorX, colorY, rightX, rightY);

          colorSelect.setXY(colorX, colorY);

          if (clickNote != -1) {
            selectType = SELECT_NOTE;
            rightMenuContents[1] = ctr.text.getString("RIGHT_MENU_CHANGE_PART_COLOR");
            rightMenuContents[2] = ctr.text.getString("RIGHT_MENU_SET_TONIC");
            rightMenu.update(rightMenuContents);
            rightMenu.setContent(ctr.getNoteLabel(clickNote), 0);

            // set note color for color wheel
            if (clickOn) {
              colorSelect.setColor(ctr.setTrackOn[notes[clickNote].track]);
            }
            else {
              colorSelect.setColor(ctr.setTrackOff[notes[clickNote].track]);
            }
          }
          else {
            if (sheetMusicDisplay &&
                pointInBox(getMousePosition(), (rect){0, ctr.menuHeight, ctr.getWidth(), ctr.barHeight}) &&
                !hoverType.contains(HOVER_DIALOG)) {
              hoverType.add(HOVER_SHEET);
              selectType = SELECT_SHEET;
              colorSelect.setColor(ctr.bgSheet);
            }
            else if (pointInBox(getMousePosition(),
                                {static_cast<int>(nowLineX - 3), ctr.barHeight, 6, ctr.getHeight() - ctr.barHeight}) &&
                     !hoverType.contains(HOVER_DIALOG)) {
              hoverType.add(HOVER_LINE);
              selectType = SELECT_LINE;
              rightMenuContents[1] = ctr.text.getString("RIGHT_MENU_CHANGE_LINE_COLOR");
              colorSelect.setColor(ctr.bgNow);
            }
            else {
              bool measureSelected = false;
              for (unsigned int i = 0; i < stream.measureMap.size(); i++) {
                double measureLineX = convertSSX(stream.measureMap[i].getLocation());
                if (pointInBox(getMousePosition(), {static_cast<int>(measureLineX - 3), ctr.barHeight, 6,
                                                    ctr.getHeight() - ctr.barHeight}) &&
                    !ctr.menu.mouseOnMenu() && !hoverType.contains(HOVER_DIALOG)) {
                  measureSelected = true;
                  break;
                }
              }

              if (measureSelected) {
                selectType = SELECT_MEASURE;
                rightMenuContents[1] = ctr.text.getString("RIGHT_MENU_CHANGE_LINE_COLOR");
                colorSelect.setColor(ctr.bgMeasure);
              }
              else if (ctr.image.exists() &&
                       (!hoverType.contains(HOVER_NOW, HOVER_NOTE, HOVER_MEASURE, HOVER_MENU, HOVER_SHEET,
                                            HOVER_DIALOG) &&
                        !hoverType.containsLastFrame(HOVER_MENU) && hoverType.contains(HOVER_IMAGE))) {
                selectType = SELECT_NONE;  // no color change on image
                rightMenuContents[1] = ctr.text.getString("RIGHT_MENU_REMOVE_IMAGE");
                // logQ("rightclicked on image");
              }
              else if (!hoverType.contains(HOVER_DIALOG)) {
                selectType = SELECT_BG;
                rightMenuContents[1] = ctr.text.getString("RIGHT_MENU_CHANGE_COLOR");
                colorSelect.setColor(ctr.bgColor);
              }
              else {
                selectType = SELECT_NONE;
                rightMenuContents[1] = ctr.text.getString("RIGHT_MENU_EMPTY");
              }
            }
            rightMenu.setContent("", 0);

            // logQ(formatVector(rightMenuContents));
            vector<string> newRight(rightMenuContents.begin() + 1, rightMenuContents.end() - 1);
            rightMenu.update(newRight);
          }
          rightMenu.setXY(rightX, rightY);

          if (!hoverType.contains(HOVER_DIALOG)) {
            ctr.dialog.clear_status();
            rightMenu.render = true;
          }
        }
      }
    }
    if (ctr.menu.mouseOnMenu() || pointInBox(getMousePosition(), {0, 0, ctr.getWidth(), ctr.menuHeight})) {
      hoverType.add(HOVER_MENU);
    }

    ctr.update(timeOffset, zoomLevel, nowLineX);
  }

  ctr.unloadData();
  return 0;
}
