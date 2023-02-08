#include "build_target.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <thread>
#include <algorithm>
#include <bit>
#include "gl_compat.h"
#include "aghfile.h"
#include "enum.h"
#include "box.h"
#include "log.h"
#include "misc.h"
#include "menu.h"
#include "data.h"
#include "wrap.h"
#include "lerp.h"
#include "image.h"
#include "color.h"
#include "define.h"
#include "menuctr.h"
#include "controller.h"

using std::string;
using std::vector;
using std::min;
using std::max;
using std::to_string;
using std::__countl_zero;

controller ctr;

int main (int argc, char* argv[]) {

  #if defined(TARGET_REL)
    SetTraceLogLevel(LOG_ERROR);
  #else
    // debug
    SetTraceLogLevel(LOG_ERROR);
    //SetTraceLogLevel(LOG_DEBUG);
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
  SetTargetFPS(60);
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

  // play settings
  bool run = false;

  // view settings
  bool nowLine = true;
  bool nowMove = false;
  bool showFPS = false;
  bool showImage = true;
  bool colorMove = false;
  bool colorSquare = false;
  bool colorCircle = false;
  bool sheetMusicDisplay = false;
 
  bool measureLine = true;
  bool measureNumber = true;

  bool showKey = false;
  int songTimeType = SONGTIME_NONE;
  int tonicOffset = 0;
  int displayMode = DISPLAY_PULSE;
  
  double nowLineX = ctr.getWidth()/2.0f;

  string FPSText = "";

  enumChecker<hoverType> hoverType;
  actionType action = actionType::ACTION_NONE;

  // color variables
  int selectType = SELECT_NONE;
  int colorMode = COLOR_PART;
  vector<colorRGB>* colorSetOn = &ctr.setTrackOn;
  vector<colorRGB>* colorSetOff = &ctr.setTrackOff;

  fileType curFileType = FILE_NONE; 
  bool clearFile = false;

  // right click variables
  int clickNote = -1;
  int clickTmp = -1;
  bool clickOn = false;
  bool clickOnTmp = false;

  // screen space conversion functions
  const auto convertSSX = [&] (int value) {
    return nowLineX + (value - timeOffset) * zoomLevel;
  };

  const auto convertSSY = [&] (int value) {
    return (ctr.getHeight() - (ctr.getHeight() - (ctr.topHeight)) *
            static_cast<double>(value - MIN_NOTE_IDX + 3) / (NOTE_RANGE + 4));
  };

  // inverse screen space conversion functions
  const auto unconvertSSX = [&] (int value) {
    return timeOffset + (value-nowLineX)/zoomLevel;
  };

  const auto inverseSSX = [&] () {
    //double spaceR = ctr.getWidth() - nowLineX;
    double minVal = max(0.0, timeOffset - nowLineX/zoomLevel);

    // extra "1" prevents roundoff error
    double maxVal = min(ctr.getLastTime(), static_cast<int>(timeOffset+(ctr.getWidth()+1-nowLineX)/zoomLevel));

    return make_pair(minVal, maxVal);
  };

  // menu variables
  constexpr int songInfoSpacing = 4;
  Vector2 songTimePosition = {6.0f, 26.0f};

  // menu objects
  vector<string> fileMenuContents = {"File", "Open File", "Open Image", "Save", "Save As", "Close File", "Close Image", "Exit"};
  menu fileMenu(ctr.getSize(), fileMenuContents, TYPE_MAIN, ctr.menu->getOffset(), 0);
  ctr.menu->registerMenu(fileMenu);
   
  vector<string> editMenuContents = {"Edit", "Enable Sheet Music", "Preferences"};
  menu editMenu(ctr.getSize(), editMenuContents, TYPE_MAIN, ctr.menu->getOffset(), 0);
  ctr.menu->registerMenu(editMenu);
  
  vector<string> viewMenuContents = {"View", "Display Mode:", "Display Song Time:", "Display Key Signature", "Hide Now Line", 
                                     "Hide Measure Line", "Hide Measure Number", "Hide Background", "Show FPS"};
  menu viewMenu(ctr.getSize(), viewMenuContents, TYPE_MAIN, ctr.menu->getOffset(), 0);
  ctr.menu->registerMenu(viewMenu);
  
  vector<string> displayMenuContents = {"Default", "Line", "Pulse", "Ball", "FFT", "Voronoi", "Loop"};
  menu displayMenu(ctr.getSize(), displayMenuContents, TYPE_SUB, 
                   viewMenu.getX() + viewMenu.getWidth(), viewMenu.getItemY(1), &viewMenu, 1);
  ctr.menu->registerMenu(displayMenu);

  vector<string> songMenuContents = {"Relative", "Absolute"};
  menu songMenu(ctr.getSize(), songMenuContents, TYPE_SUB, 
                viewMenu.getX() + viewMenu.getWidth(), viewMenu.getItemY(2), &viewMenu, 2);
  ctr.menu->registerMenu(songMenu);

  vector<string> midiMenuContents = {"Midi", "Input", "Output", "Enable Live Play"};
  menu midiMenu(ctr.getSize(), midiMenuContents, TYPE_MAIN, ctr.menu->getOffset(), 0);
  ctr.menu->registerMenu(midiMenu);

  vector<string> inputMenuContents = {""};
  menu inputMenu(ctr.getSize(), inputMenuContents, TYPE_SUB, 
                 midiMenu.getX() + midiMenu.getWidth(), midiMenu.getItemY(1), &midiMenu, 1);
  ctr.menu->registerMenu(inputMenu);

  vector<string> outputMenuContents = {""};
  menu outputMenu(ctr.getSize(), outputMenuContents, TYPE_SUB, 
                 midiMenu.getX() + midiMenu.getWidth(), midiMenu.getItemY(2), &midiMenu, 2);
  ctr.menu->registerMenu(outputMenu);
  
  vector<string> colorMenuContents = {"Color", "Color By:", "Color Scheme:", "Swap Colors", "Invert Color Scheme"};
  menu colorMenu(ctr.getSize(), colorMenuContents, TYPE_MAIN, ctr.menu->getOffset(), 0);
  ctr.menu->registerMenu(colorMenu);
   
  vector<string> schemeMenuContents = {"Part", "Velocity", "Tonic"};
  menu schemeMenu(ctr.getSize(), schemeMenuContents, TYPE_SUB, 
                  colorMenu.getX() + colorMenu.getWidth(), colorMenu.getItemY(1), &colorMenu, 1);
  ctr.menu->registerMenu(schemeMenu);
  
  vector<string> infoMenuContents= {"Info", "Program Info", "Help"};
  menu infoMenu(ctr.getSize(), infoMenuContents, TYPE_MAIN, ctr.menu->getOffset(), 0);
  ctr.menu->registerMenu(infoMenu);

  vector<string> paletteMenuContents = {"Default", "From Background"};
  menu paletteMenu(ctr.getSize(), paletteMenuContents, TYPE_SUB, 
                   colorMenu.getX() + colorMenu.getWidth(), colorMenu.getItemY(2), &colorMenu, 2);
  ctr.menu->registerMenu(paletteMenu);
  
  vector<string> rightMenuContents = {"Info", "Change Part Color", "Set Tonic"};
  menu rightMenu(ctr.getSize(), rightMenuContents, TYPE_RIGHT, -100,-100); 
  ctr.menu->registerMenu(rightMenu);
  
  vector<string> colorSelectContents = {"Color Select"};
  menu colorSelect(ctr.getSize(), colorSelectContents, TYPE_COLOR, -100,-100, &rightMenu, 1); 
  ctr.menu->registerMenu(colorSelect);

  if (argc >= 2) {
    // input parameter order is arbitrary
    // avoid argv[0]
    ctr.updateFiles(&argv[1], argc-1);
  }

  //DEBUG ONLY
  //ctr.toggleLivePlay();
  //ctr.input.openPort(1);
  //ctr.output.openPort(3);

  // main program logic
  while (ctr.getProgramState()) {

    if (ctr.open_file.pending() || clearFile) {
      run = false;
      timeOffset = 0;
      pauseOffset = 0;

      if (ctr.open_file.pending()) {
        ctr.load(ctr.open_file.getPath(), curFileType,
                 nowLine,  showFPS,  showImage,  sheetMusicDisplay,
                 measureLine,  measureNumber, 
                 colorMode,  displayMode,
                 songTimeType,  tonicOffset, 
                 zoomLevel);

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
      ctr.input.update();
      run = false;
    }

    // fix FPS count bug
    GetFPS();

    // preprocess variables
    clickTmp = -1;
    hoverType.clear();

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
        ctr.image.draw();
        if (!hoverType.contains(HOVER_DIALOG)) {
          if (getMousePosition().x > ctr.image.getX() && 
              getMousePosition().x < ctr.image.getX() + ctr.image.getWidth()) {
            if (getMousePosition().y > ctr.image.getY() && 
                getMousePosition().y < ctr.image.getY() + ctr.image.getHeight()) {

              hoverType.add(HOVER_IMAGE); 
            }
          }
        }
      }
      
      switch(displayMode) {
        case DISPLAY_VORONOI:
          if (ctr.voronoi.vertex.size() != 0) {
            int voro_y = (sheetMusicDisplay ? ctr.menuHeight + ctr.sheetHeight : 0);
            ctr.voronoi.resample(voro_y);

            ctr.voronoi.render();
           
          }
          break;
      }

      int lastMeasureNum = 0;

      double measureSpacing = measureTextEx(to_string(ctr.getStream().measureMap.size() - 1).c_str()).x; 

      if (measureLine || measureNumber) {
        for (unsigned int i = 0; i < ctr.getStream().measureMap.size(); i++) {
          float measureLineWidth = 0.5;
          int measureLineY = ctr.menuHeight + (sheetMusicDisplay ? ctr.menuHeight + ctr.sheetHeight : 0);
          double measureLineX = convertSSX(ctr.getStream().measureMap[i].getLocation());
          bool hideLine = false;
          if (i && i + 1 < ctr.getStream().measureMap.size()) {
            if (convertSSX(ctr.getStream().measureMap[i + 1].getLocation()) - measureLineX < 10) {
              hideLine = true;
            }
          }
                
          if (measureLine && !hideLine) {
            if (pointInBox(getMousePosition(), {int(measureLineX - 3), measureLineY, 6, ctr.getHeight() - measureLineY}) &&
                !hoverType.containsLastFrame(HOVER_MENU) && !hoverType.contains(HOVER_DIALOG)) {
              measureLineWidth = 1;
              hoverType.add(HOVER_MEASURE);
            }
            if (!nowLine || fabs(nowLineX - measureLineX) > 3) {  
              drawLineEx(static_cast<int>(measureLineX), measureLineY,
                         static_cast<int>(measureLineX), ctr.getHeight(), measureLineWidth, ctr.bgMeasure);
            }
          }

          switch(displayMode) {
            case DISPLAY_VORONOI:
              ctr.beginBlendMode(CGL_ONE_MINUS_DST_COLOR, CGL_ZERO, CGL_ADD);
              ctr.beginShaderMode("SH_INVERT");
              break;
          }

          if (measureNumber) {
            double lastMeasureLocation = convertSSX(ctr.getStream().measureMap[lastMeasureNum].getLocation()); 
            if (!i || lastMeasureLocation + measureSpacing + 10 < measureLineX) {
              // measure number / song time + key sig collision detection
              Vector2 songInfoSize = {0,0}; 
              switch (songTimeType) {
                case SONGTIME_ABSOLUTE:
                 songInfoSize = measureTextEx(getSongTime(timeOffset, ctr.getLastTime()));
                 break;
                case SONGTIME_RELATIVE: 
                 songInfoSize = measureTextEx(getSongPercent(timeOffset, ctr.getLastTime()));
                 break;
              }
              if (showKey) {
                Vector2 keySigSize = measureTextEx(ctr.getKeySigLabel(timeOffset));
                songInfoSize.x += keySigSize.x;
                songInfoSize.y += keySigSize.y;

                if (songTimeType == SONGTIME_ABSOLUTE || songTimeType == SONGTIME_RELATIVE) {
                  songInfoSize.x += songInfoSpacing;
                }
              }
             
              double fadeWidth = 2.0*measureSpacing;
              int measureLineTextAlpha = 255*(min(fadeWidth, ctr.getWidth()-measureLineX))/fadeWidth;

              if (songTimeType != SONGTIME_NONE && measureLineX + 4 < songTimePosition.x*2 + songInfoSize.x) {
                measureLineTextAlpha = max(0.0,min(255.0, 
                                                   255.0 * (1-(songTimePosition.x*2 + songInfoSize.x - measureLineX - 4)/10)));
              }
              else if (measureLineX < fadeWidth) {
                measureLineTextAlpha = 255*max(0.0, (min(fadeWidth, measureLineX+measureSpacing))/fadeWidth);

              }

              int measureTextY = ctr.menuHeight + 4 + (sheetMusicDisplay ? ctr.sheetHeight + ctr.menuHeight : 0);
              drawTextEx(to_string(i + 1).c_str(), measureLineX + 4, measureTextY, ctr.bgLight, measureLineTextAlpha);
              lastMeasureNum = i;
            }
          }
        }
      }

      switch(displayMode) {
        case DISPLAY_VORONOI:
          ctr.endShaderMode();
          ctr.endBlendMode();
          break;
      }

      if (nowLine) {
        float nowLineWidth = 0.5;
        int nowLineY = ctr.menuHeight + (sheetMusicDisplay ? ctr.menuHeight + ctr.sheetHeight : 0);
        if (pointInBox(getMousePosition(), {int(nowLineX - 3), nowLineY, 6, ctr.getHeight() - ctr.barHeight}) && 
            !ctr.menu->mouseOnMenu() && !hoverType.contains(HOVER_DIALOG)) {
          nowLineWidth = 1;
          hoverType.add(HOVER_NOW);
        }
        drawLineEx(nowLineX, nowLineY, nowLineX, ctr.getHeight(), nowLineWidth, ctr.bgNow);
      }

      auto getColorSet = [&](int idx, const vector<int>& lp = {}) {
        switch(displayMode) {
          case DISPLAY_FFT:
            [[fallthrough]];
          case DISPLAY_VORONOI:
            [[fallthrough]];
          case DISPLAY_BAR:
            [[fallthrough]];
          case DISPLAY_BALL:
            switch (colorMode) {
              case COLOR_PART:
                return ctr.getNotes()[idx].track;
              case COLOR_VELOCITY:
                return ctr.getNotes()[idx].velocity;
              case COLOR_TONIC:
                return (ctr.getNotes()[idx].y - MIN_NOTE_IDX + tonicOffset) % 12 ;
            }
            break;
          case DISPLAY_LOOP:
            [[fallthrough]];
          case DISPLAY_PULSE:
            [[fallthrough]];
          case DISPLAY_LINE:
            switch (colorMode) {
              case COLOR_PART:
                if (ctr.getLiveState()) return 0;
                return ctr.getNotes()[lp[idx]].track;
              case COLOR_VELOCITY:
                return ctr.getNotes()[lp[idx]].velocity;
              case COLOR_TONIC:
                return (ctr.getNotes()[lp[idx]].y - MIN_NOTE_IDX + tonicOffset) % 12 ;
            }
        }
        return 0;
      };

      

      pair<double, double> currentBoundaries = inverseSSX();
      vector<int> curNote;

      // note rendering
      for (int i = 0; i < ctr.getNoteCount(); i++) {
        
        if (ctr.getNotes()[i].x < currentBoundaries.first*0.9 && ctr.getNotes()[i].x > currentBoundaries.second*1.1) {
          continue;
        }
        
        bool noteOn = false;
        
        const auto updateClickIndex = [&](int clickIndex = -1){
          if (!hoverType.contains(HOVER_DIALOG)) {
            noteOn ? clickOnTmp = true : clickOnTmp = false;
            noteOn = !noteOn;
            clickTmp = clickIndex == -1 ? i : clickIndex;
            hoverType.add(HOVER_NOTE);
          }
        };
        
        float cX = convertSSX(ctr.getNotes()[i].x);
        float cY = convertSSY(ctr.getNotes()[i].y);
        float cW = ctr.getNotes()[i].duration * zoomLevel < 1 ? 1 : ctr.getNotes()[i].duration * zoomLevel;
        float cH = (ctr.getHeight() - ctr.menuHeight) / 88.0f;

        switch (displayMode) {
          case DISPLAY_BAR:
            {
              int colorID = getColorSet(i);
              if (ctr.getNotes()[i].isOn ||
                 (timeOffset >= ctr.getNotes()[i].x && 
                  timeOffset < ctr.getNotes()[i].x + ctr.getNotes()[i].duration)) {
                noteOn = true;
              }
              if (pointInBox(getMousePosition(), (rect){int(cX), int(cY), int(cW), int(cH)}) && !ctr.menu->mouseOnMenu()) {
                updateClickIndex();
              }

              auto cSet = noteOn ? colorSetOn : colorSetOff;
              drawRectangle(cX, cY, cW, cH, (*cSet)[colorID]);
            }
            break;
          case DISPLAY_VORONOI:
            if (cX > -0.2*ctr.getWidth() && cX + cW < 1.2*ctr.getWidth()){
              int colorID = getColorSet(i);
              if (ctr.getNotes()[i].isOn ||
                 (timeOffset >= ctr.getNotes()[i].x && 
                  timeOffset < ctr.getNotes()[i].x + ctr.getNotes()[i].duration)) {
                noteOn = true;
              }

              //float radius = -1 + 2 * (32 - __countl_zero(int(cW)));
              float radius = 1 + log2(cW+1);
              if (getDistance(ctr.getMouseX(), ctr.getMouseY(), cX, cY + cH) < radius + 2) {
                updateClickIndex();
              }


              auto cSet = noteOn ? colorSetOn : colorSetOff;
              

              ctr.voronoi.vertex.push_back({cX/ctr.getWidth(), 1-cY/ctr.getHeight()});
              ctr.voronoi.color.push_back((*cSet)[colorID]);

              drawRing({cX, cY+cH}, radius-1, radius+2, ctr.bgDark);
              drawRing({cX, cY+cH}, 0, radius, (*cSet)[colorID]);
            }
            break;
          case DISPLAY_BALL:
            {
              int colorID = getColorSet(i);
              float radius = -1 + 2 * (32 - __countl_zero(int(cW)));
              float maxRad = radius;
              float ballY = cY + 2;
              if (cX + cW + radius > 0 && cX - radius < ctr.getWidth()) {
                if (cX < nowLineX - cW) {
                  radius *= 0.3;
                }
                if (ctr.getNotes()[i].isOn ||
                   (timeOffset >= ctr.getNotes()[i].x && 
                    timeOffset < ctr.getNotes()[i].x + ctr.getNotes()[i].duration)) {
                  noteOn = true;
                  radius *= (0.3f + 0.7f * (1.0f - float(timeOffset - ctr.getNotes()[i].x) / ctr.getNotes()[i].duration));
                }
                if (!ctr.menu->mouseOnMenu()) {
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
                           pointInBox(getMousePosition(), 
                                      (rect) {int(cX), int(ballY) - 2, max(int(nowLineX - cX), 0), 4}))) {
                    updateClickIndex();
                  }
                }
                
                if (noteOn) {
                  if (cX >= nowLineX) {
                    drawRing({cX, ballY}, radius - 2, radius, (*colorSetOn)[colorID]);
                  }
  
                  else if (cX + cW < nowLineX) {
                    drawRing({cX + cW, ballY}, radius - 2, radius, (*colorSetOn)[colorID]);
                  }
                  else if (cX < nowLineX) {
                    drawRing({cX, ballY}, radius - 2, radius, (*colorSetOn)[colorID], 255*radius/maxRad);
                    drawRing({static_cast<float>(nowLineX), ballY}, radius - 2, radius, (*colorSetOn)[colorID]);
                    if (nowLineX - cX > 2 * radius) {
                      drawGradientLineH({cX + radius, ballY + 1}, {static_cast<float>(nowLineX) - radius + 1, ballY + 1}, 
                                        2, (*colorSetOn)[colorID], 255, 255*radius/maxRad);
                    }
                  }
                }
                else {
                  if (cX < nowLineX && cX + cW > nowLineX) {
                    drawRing({cX, ballY}, radius - 2, radius, (*colorSetOff)[colorID], 255*radius/maxRad);
                    drawRing({static_cast<float>(nowLineX), ballY}, radius - 2, radius, (*colorSetOff)[colorID]);
                    if (nowLineX - cX > 2 * radius) {
                      drawLineEx(cX + radius, ballY + 1, nowLineX - radius, ballY + 1, 2, (*colorSetOff)[colorID]);
                    }
                  }
                  else if (cX < nowLineX) {
                    drawRing({cX + cW, ballY}, radius - 2, radius, (*colorSetOff)[colorID]);
                  }
                  else {
                    drawRing({cX, ballY}, radius - 2, radius, (*colorSetOff)[colorID]);
                  }
                }
                //drawSymbol(SYM_TREBLE, 75, cX,cY, (*colorSetOff)[colorID]);
              }
            }
            break;
          case DISPLAY_LINE:
            {
              if (i != 0){
                break;
              }
              vector<int> linePositions = ctr.getStream().getLineVerts();
              if (!ctr.getLiveState() || (convertSSX(ctr.getNotes()[i].getNextChordRoot()->x) > 0 && cX < ctr.getWidth())) {
                if (ctr.getNotes()[i].isChordRoot()) {
                  for (unsigned int j = 0; j < linePositions.size(); j += 5) {
                    int colorID = getColorSet(j,linePositions);
                    float convSS[4] = {
                                        static_cast<float>(convertSSX(linePositions[j+1])),
                                        static_cast<float>(convertSSY(linePositions[j+2])),
                                        static_cast<float>(convertSSX(linePositions[j+3])),
                                        static_cast<float>(convertSSY(linePositions[j+4]))
                                      };

                    if (convSS[2] < 0 || convSS[0] > ctr.getWidth()) {
                       continue;
                    }

                    noteOn = convSS[0] <= nowLineX && convSS[2] > nowLineX;
                    if (pointInBox(getMousePosition(), 
                                   pointToRect(
                                                {static_cast<int>(convSS[0]), static_cast<int>(convSS[1])},
                                                {static_cast<int>(convSS[2]), static_cast<int>(convSS[3])}
                                              ))) {
                      updateClickIndex(linePositions[j]);
                    }
                    auto cSet = noteOn ? colorSetOn : colorSetOff;
                    if (convSS[2] - convSS[0] > 3) {
                      drawLineBezier(convSS[0], convSS[1], convSS[2], convSS[3],
                                 2, (*cSet)[colorID]);
                    }
                    else {
                      drawLineEx(convSS[0], convSS[1], convSS[2], convSS[3],
                                 2, (*cSet)[colorID]);
                    }
                  }
                }
              }
            }
            break;

          case DISPLAY_PULSE:
            {
              if (i != 0){
                break;
              }
              vector<int> linePositions = ctr.getStream().getLineVerts();
              if (!ctr.getLiveState() || (convertSSX(ctr.getNotes()[i].getNextChordRoot()->x) > 0 && cX < ctr.getWidth())) {
                if (ctr.getNotes()[i].isChordRoot()) {
                  for (unsigned int j = 0; j < linePositions.size(); j += 5) {
                    int colorID = getColorSet(j,linePositions);
                    float convSS[4] = {
                                        static_cast<float>(convertSSX(linePositions[j+1])),
                                        static_cast<float>(convertSSY(linePositions[j+2])),
                                        static_cast<float>(convertSSX(linePositions[j+3])),
                                        static_cast<float>(convertSSY(linePositions[j+4]))
                                      };

                    if (convSS[2] < 0 || convSS[0] > ctr.getWidth()) {
                       continue;
                    }

                    noteOn = convSS[0] <= nowLineX && convSS[2] > nowLineX;

                    if (pointInBox(getMousePosition(), 
                                   pointToRect(
                                                {static_cast<int>(convSS[0]), static_cast<int>(convSS[1])},
                                                {static_cast<int>(convSS[2]), static_cast<int>(convSS[3])}
                                              ))) {
                      updateClickIndex(linePositions[j]);
                    }
                    auto cSet = noteOn ? colorSetOn : colorSetOff;
                    double nowRatio = (nowLineX-convSS[0])/(convSS[2]-convSS[0]);
                    if (noteOn || clickTmp == linePositions[j]) {
                      double newY = (convSS[3]-convSS[1])*nowRatio + convSS[1];
                      bool nowNote = clickTmp == linePositions[j] ? false : noteOn;
                      drawLineEx(nowNote ? nowLineX - floatLERP(0, (nowLineX-convSS[0])/2.0, nowRatio, INT_SINE) : convSS[0],
                                 nowNote ? newY     - floatLERP(0, (newY-convSS[1])/2.0, nowRatio, INT_SINE) : convSS[1], 
                                 nowNote ? nowLineX - floatLERP(0, (nowLineX-convSS[2])/2.0, nowRatio, INT_ISINE) : convSS[2], 
                                 nowNote ? newY     - floatLERP(0, (newY-convSS[3])/2.0, nowRatio, INT_ISINE) : convSS[3], 
                                 3, (*cSet)[colorID]);
                      drawRing({float(nowNote ? nowLineX - floatLERP(0, (nowLineX-convSS[0])/2.0, nowRatio, INT_SINE) 
                                              : convSS[0]),
                                float(nowNote ? newY - floatLERP(0, (newY-convSS[1])/2.0, nowRatio, INT_SINE) 
                                              : convSS[1])}, 
                               0, 1.5, (*cSet)[colorID]);
                      drawRing({float(nowNote ? nowLineX - floatLERP(0, (nowLineX-convSS[2])/2.0, nowRatio, INT_ISINE) 
                                              : convSS[0]),
                                float(nowNote ? newY - floatLERP(0, (newY-convSS[3])/2.0, nowRatio, INT_ISINE) 
                                              : convSS[1])}, 
                               0, 1.5, (*cSet)[colorID]);
                    }
                    
                    if (convSS[2] >= nowLineX) {
                      double ringFadeAlpha = noteOn ? 255*(1-nowRatio) : 255;
                      drawRing({convSS[0], convSS[1]},
                               0, 3, (*cSet)[colorID], ringFadeAlpha);
                    }
                    if (convSS[2] <= nowLineX) {
                      drawRing({convSS[2], convSS[3]},
                               0, 3, (*cSet)[colorID]);
                    }

                    int ringLimit = 400;
                    int ringDist = timeOffset - linePositions[j+1];

                    double ringRatio = ringDist/static_cast<double>(ringLimit); 
                    if (run && linePositions[j+1] < pauseOffset && timeOffset >= pauseOffset) {
                      ringRatio = 0;
                    }
                    else if (ctr.getPauseTime()<1 && timeOffset == pauseOffset){// || linePositions[j+1] >= pauseOffset) {
                      // this effect has a run-down time of 1 second
                      ringRatio += min(1-ringRatio, ctr.getPauseTime());
                    }
                    else if (linePositions[j+1] < pauseOffset && timeOffset == pauseOffset) {
                      ringRatio = 0;
                      //ringRatio *= max(ctr.getRunTime(), 1.0);
                    }
                    //logQ(timeOffset, (linePositions[j+1], linePositions[j+2]));
                    if (ringDist <= ringLimit && ringDist > 4) {
                      int noteLen = ctr.getNotes()[linePositions[j]].duration * zoomLevel < 1 ? 
                                  1 : ctr.getNotes()[linePositions[j]].duration * zoomLevel;
                      noteLen = noteLen ? 32 - __countl_zero(noteLen) : 0;
                      double ringRad = floatLERP(6, 5*noteLen, ringRatio, INT_ILINEAR);

                      if (ringRatio > 0) {
                        drawRing({convSS[0], convSS[1]},
                                 ringRad-3, ringRad, 
                                 colorLERP((*colorSetOn)[colorID], (*colorSetOff)[colorID], ringRatio, INT_ICIRCULAR),
                                 floatLERP(0,255, ringRatio, INT_ICIRCULAR));
                      }
                    }
                  }
                }
              }
            }
            break;
          case DISPLAY_LOOP:
            {
              if (i != 0){
                break;
              }
              vector<int> linePositions = ctr.getStream().getLineVerts();
              if (!ctr.getLiveState() || (convertSSX(ctr.getNotes()[i].getNextChordRoot()->x) > 0 && cX < ctr.getWidth())) {
                if (ctr.getNotes()[i].isChordRoot()) {
                  for (unsigned int j = 0; j < linePositions.size(); j += 5) {
                    int colorID = getColorSet(j,linePositions);
                    float convSS[4] = {
                                        static_cast<float>(convertSSX(linePositions[j+1])),
                                        static_cast<float>(convertSSY(linePositions[j+2])),
                                        static_cast<float>(convertSSX(linePositions[j+3])),
                                        static_cast<float>(convertSSY(linePositions[j+4]))
                                      };

                    if (convSS[2] < 0 || convSS[0] > ctr.getWidth()) {
                       continue;
                    }

                    noteOn = convSS[0] <= nowLineX && convSS[2] > nowLineX;
                    if (pointInBox(getMousePosition(), 
                                   pointToRect(
                                                {static_cast<int>(convSS[0]), static_cast<int>(convSS[1])},
                                                {static_cast<int>(convSS[2]), static_cast<int>(convSS[3])}
                                              ))) {
                      updateClickIndex(linePositions[j]);
                    }
                    auto cSet = noteOn ? colorSetOn : colorSetOff;
                    double nowRatio = (nowLineX-convSS[0])/(convSS[2]-convSS[0]);
                    if (convSS[2] < nowLineX) {
                    drawLineEx(convSS[0], convSS[1], convSS[2], convSS[3],
                               2, (*cSet)[colorID]);
                    }
                    else if (convSS[0] < nowLineX) {

                      double newY = (convSS[3]-convSS[1])*nowRatio + convSS[1];
                      bool nowNote = clickTmp == linePositions[j] ? false : noteOn;
                      drawLineEx(convSS[0],
                                 convSS[1],
                                 nowNote ? nowLineX - floatLERP(0, (nowLineX-convSS[2])/2.0, nowRatio, INT_ISINE) : convSS[2], 
                                 nowNote ? newY     - floatLERP(0, (newY-convSS[3])/2.0, nowRatio, INT_ISINE) : convSS[3], 
                                 3, (*cSet)[colorID]);
                    }
                    drawRing({convSS[0], convSS[1]},
                             0, 3, (*cSet)[colorID]);
                    drawRing({convSS[2], convSS[3]},
                             0, 3, (*cSet)[colorID]);
                    if (convSS[2] < nowLineX) {
                      drawRing({convSS[2], convSS[3]},
                               4, 8, (*cSet)[colorID]);
                    }
                    
                    if (nowRatio > 0 && nowRatio < 1) {
                      drawRing({convSS[2], convSS[3]},
                               4, 8, (*cSet)[colorID], 255, (nowRatio-0.5f)*360.0f, 180.0f);
                    }
                  }
                }
              }
            }
            break;
          case DISPLAY_FFT:
            if (cX + cW > 0 && cX < ctr.getWidth()) {
              bool drawFFT = false;
              double fftStretchRatio = 1.78;          //TODO: make fft-selection semi-duration-invariant
              int colorID = getColorSet(i); 
              if (ctr.getNotes()[i].isOn ||
                 (timeOffset >= ctr.getNotes()[i].x && 
                  timeOffset < ctr.getNotes()[i].x + ctr.getNotes()[i].duration)) {
                noteOn = true;
                drawFFT = true;
              }
              else if ((timeOffset >= ctr.getNotes()[i].x + ctr.getNotes()[i].duration && 
                        timeOffset < ctr.getNotes()[i].x + fftStretchRatio*ctr.getNotes()[i].duration) ||
                       (timeOffset < ctr.getNotes()[i].x && 
                        timeOffset >= ctr.getNotes()[i].x - ctr.getMinTickLen())) {
                
                drawFFT = true;
              }
              if (pointInBox(getMousePosition(), (rect){int(cX), int(cY), int(cW), int(cH)}) && !ctr.menu->mouseOnMenu()) {
                updateClickIndex();
              }
              if(drawFFT) {
                curNote.push_back(i);
              }

              auto cSet = noteOn ? colorSetOn : colorSetOff;
              drawRectangle(cX, cY, cW, cH, (*cSet)[colorID]);
            }
            break;
        }
      }

      // render FFT lines after notes
      if (displayMode == DISPLAY_FFT) {
        for (const auto& idx : curNote) {
          double freq = ctr.fft.getFundamental(ctr.getNotes()[idx].y);
          int colorID = getColorSet(idx);
          double nowRatio = (timeOffset-ctr.getNotes()[idx].x)/(ctr.getNotes()[idx].duration);
          double pitchRatio = 0;
          if (nowRatio > -0.25 && nowRatio < 0) {
            pitchRatio = 16*pow(nowRatio+0.25,2);
          }
          else if (nowRatio < 1) {
            pitchRatio = 1-1.8*pow(nowRatio,2)/4.5;
          }
          else if (nowRatio < 1.78) {                   // TODO: smoothen interpolation functions
            pitchRatio = 1.0*pow(nowRatio-1.78,2);      // TODO: make function duration-invariant
          }
          int binScale = 2+5*(1+log(1+ctr.getNotes()[idx].duration)) +
                         (15.0/128)*((ctr.getNotes()[idx].velocity)+1);

          //logQ(ctr.getNotes()[idx].y, freq, ctr.fft.fftbins.size());
          // simulate harmonics

          for (unsigned int bin = 0; bin < ctr.fft.fftbins.size(); ++bin) {
            ctr.fft.fftbins[bin].second = 0;
          }

          bool foundNote = false;
          for (unsigned int harmonicScale = 0; harmonicScale < ctr.fft.harmonicsSize; ++harmonicScale) {
            for (unsigned int bin = 0; bin < ctr.fft.fftbins.size(); ++bin) {
              if (freq*ctr.fft.harmonics[harmonicScale] < FFT_MIN_FREQ ||
                  freq*ctr.fft.harmonics[harmonicScale] > FFT_MAX_FREQ) {
                continue;
              }
              double fftBinLen = ctr.fft.harmonicsCoefficient[harmonicScale]*binScale*pitchRatio * 
                                 ctr.fft.fftAC(freq*ctr.fft.harmonics[harmonicScale], ctr.fft.fftbins[bin].first);
              
              // pseudo-random numerically stable offset
              
              // TODO: implement spectral rolloff in decay (based on BIN FREQ v. spectral rolloff curve)
              
              fftBinLen *= 1+0.7*pow(((ctr.getPSR() ^ static_cast<int>(ctr.fft.fftbins[bin].first)) % 
                                      static_cast<int>(ctr.fft.fftbins[bin].first)) / ctr.fft.fftbins[bin].first - 0.5, 2);
              
              int startX = FFT_BIN_WIDTH*(bin + 1);
             
              auto cSet = colorSetOn;

              //collision
              if (!foundNote && !hoverType.contains(HOVER_DIALOG) && pointInBox(getMousePosition(), 
                             {startX-3, static_cast<int>(ctr.getHeight()-ctr.fft.fftbins[bin].second-fftBinLen), 
                              7,        static_cast<int>(fftBinLen)})) {
                foundNote = true;
                cSet = colorSetOff;
                hoverType.add(HOVER_NOTE);
                clickOnTmp = true;
                clickTmp = idx;

              }
              
              drawLineEx(startX,ctr.getHeight()-ctr.fft.fftbins[bin].second,
                         startX,ctr.getHeight()-ctr.fft.fftbins[bin].second-fftBinLen,1, (*cSet)[colorID]);
              ctr.fft.fftbins[bin].second += fftBinLen;
            }
          }
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
            drawLineEx(ctr.sheetSideMargin, 
                       ctr.menuHeight + ctr.barMargin + i * ctr.barSpacing + j * ctr.barWidth,
                       ctr.getWidth() - ctr.sheetSideMargin, 
                       ctr.menuHeight + ctr.barMargin + i * ctr.barSpacing + j * ctr.barWidth, 
                       1, ctr.bgSheetNote);
          }
        }
        
        //// end lines
        drawLineEx(ctr.sheetSideMargin, ctr.menuHeight + ctr.barMargin, ctr.sheetSideMargin,
                   ctr.menuHeight + ctr.barMargin + 4 * ctr.barWidth + ctr.barSpacing, 2, ctr.bgSheetNote);
        drawLineEx(ctr.getWidth() - ctr.sheetSideMargin, ctr.menuHeight + ctr.barMargin, ctr.getWidth() - ctr.sheetSideMargin,
                   ctr.menuHeight + ctr.barMargin + 4 * ctr.barWidth + ctr.barSpacing, 2, ctr.bgSheetNote);

        
        drawSymbol(SYM_STAFF_BRACE, 480, 17.0f, float(ctr.menuHeight + ctr.barMargin) - 120, ctr.bgSheetNote);
        drawSymbol(SYM_CLEF_TREBLE, 155, 40.0f, ctr.menuHeight + ctr.barMargin - 47, ctr.bgSheetNote);
        drawSymbol(SYM_CLEF_BASS, 155, 40.0f, float(ctr.menuHeight + ctr.barSpacing + ctr.barMargin - 67), ctr.bgSheetNote);
      

        if (ctr.getStream().measureMap.size() != 0) {
          ctr.getStream().sheetData.drawSheetPage();
        }

       
        if (IsKeyPressed(KEY_F)) {
          

        }
        //ctr.getStream().sheetData.findSheetPages();
        //logQ("cloc", ctr.getCurrentMeasure(timeOffset));
        //logQ("cloc", formatPair(ctr.getStream().sheetData.findSheetPageLimit(ctr.getCurrentMeasure(timeOffset))));
      }
      
      // option actions
      //

      Vector2 songTimeSizeV = {0.0, 0.0};
      string songTimeContent = "";

      switch (songTimeType) {
        case SONGTIME_RELATIVE:
          songTimeContent = getSongPercent(timeOffset, ctr.getLastTime());
          break;
        case SONGTIME_ABSOLUTE:
          songTimeContent = getSongTime(timeOffset, ctr.getLastTime());
          break;
        case SONGTIME_NONE:
          break;
        default:
          logQ("invalid song time option:", songTimeType);
          break;
      }
      
      drawTextEx(songTimeContent, songTimePosition, ctr.bgLight);
      songTimeSizeV = measureTextEx(songTimeContent);


      // draw key signature label
      // TODO: render accidental glyph using MUSIC_FONT
      if (showKey) {
        if (songTimeType == SONGTIME_ABSOLUTE || songTimeType == SONGTIME_RELATIVE) {
          songTimeSizeV.x += songInfoSpacing;
        }
        //logQ("got label:", ctr.getKeySigLabel(timeOffset));
        drawTextEx(ctr.getKeySigLabel(timeOffset), songTimePosition.x+songTimeSizeV.x, songTimePosition.y, ctr.bgLight);
      }


      if (showFPS) {
        if (GetTime() - (int)GetTime() < GetFrameTime()) {
          FPSText = to_string(GetFPS());
        }
        drawTextEx(FPSText.c_str(),
                   ctr.getWidth() - 
                  measureTextEx(FPSText.c_str()).x - 4, 4, ctr.bgDark);
      }

      ctr.menu->render();

      ctr.dialog.render();

      // warning about windows stability
      ctr.warning.render();
    
    EndDrawing();

    // key actions
    ctr.processAction(action);

    if (run) {
      if (timeOffset + GetFrameTime() * 500 < ctr.getLastTime()) {
        timeOffset += GetFrameTime() * 500;
      }
      else {
        timeOffset = ctr.getLastTime();
        run = false;
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
            ctr.setTrackOn[ctr.getNotes()[clickNote].track] = colorSelect.getColor();
          }
          else {
            ctr.setTrackOff[ctr.getNotes()[clickNote].track] = colorSelect.getColor();
          }
          break;
        case SELECT_BG:
          ctr.bgColor = colorSelect.getColor();
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
      case actionType::ACTION_OPEN:
        ctr.open_file.dialog();
        ctr.menu->hide();
        break;
      case actionType::ACTION_OPEN_IMAGE:
        ctr.open_image.dialog();
        showImage = true; 
        viewMenu.setContent("Hide Background", 4);
        ctr.menu->hide();
        break;
      case actionType::ACTION_CLOSE:
        clearFile = true;
        break;
      case actionType::ACTION_CLOSE_IMAGE:
        ctr.image.unloadData();
        break;
      case actionType::ACTION_SAVE:
        if (ctr.getPlayState()) {
          break; 
        }
        if (curFileType == FILE_MKI) {
          ctr.save(ctr.save_file.getPath(), nowLine, showFPS, showImage, sheetMusicDisplay, measureLine, measureNumber,
                   colorMode, displayMode, songTimeType, tonicOffset, zoomLevel);
          break;
        }
        [[fallthrough]];
      case actionType::ACTION_SAVE_AS:
        if (!ctr.getPlayState() && curFileType != FILE_NONE) {

           ctr.save_file.dialog();

           if (ctr.save_file.pending()) {

            string save_path = ctr.save_file.getPath();

            if (getExtension(save_path, true) != ".mki") {
              save_path += ".mki";
            }

            ctr.save(save_path, nowLine, showFPS, showImage, sheetMusicDisplay, measureLine, measureNumber,
                     colorMode, displayMode, songTimeType, tonicOffset, zoomLevel);
            curFileType = FILE_MKI;

            ctr.save_file.resetPending();
          }
        }
        break;
      case actionType::ACTION_SHEET:
        if (editMenu.getContent(1) == "Disable Sheet Music") {
          editMenu.setContent("Enable Sheet Music", 1);
        }
        else if (editMenu.getContent(1) == "Enable Sheet Music") {
          editMenu.setContent("Disable Sheet Music", 1);
        }
        sheetMusicDisplay = !sheetMusicDisplay;
        ctr.barHeight = sheetMusicDisplay ? ctr.menuHeight + ctr.sheetHeight : ctr.menuHeight;
        break;
      case actionType::ACTION_PREFERENCES:
        ctr.dialog.preferenceDisplay = !ctr.dialog.preferenceDisplay;
        break;
      case actionType::ACTION_INFO:
        ctr.dialog.infoDisplay = !ctr.dialog.infoDisplay;
        break;
      case actionType::ACTION_LIVEPLAY:
        if (midiMenu.getContent(3) == "Enable Live Play") {
          midiMenu.setContent("Disable Live Play", 3);
          zoomLevel *= 3;
        }
        else if (midiMenu.getContent(3) == "Disable Live Play") {
          midiMenu.setContent("Enable Live Play", 3);
          zoomLevel *= 1.0/3.0;
        }
        ctr.toggleLivePlay();
        if (!ctr.getLiveState()) {
          timeOffset = 0;
        }
        break;
      default:
        break;
    }
    action = actionType::ACTION_NONE;


    // key logic
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SHIFT)) && GetMouseWheelMove() != 0) {
      if (ctr.image.exists() && showImage) {

        // defaults to ±1, adjusted depending on default image scale value
        float scaleModifier = 0.02f;
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
          scaleModifier = 0.002f;
        }
        ctr.image.changeScale(scaleModifier*GetMouseWheelMove());
      }
    }
    
    if ((!IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_LEFT_SHIFT)) && 
        (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_UP) || GetMouseWheelMove() != 0)) {
      if (IsKeyPressed(KEY_DOWN) || (GetMouseWheelMove() < 0)) {
        if (zoomLevel > 0.00001) {
          zoomLevel *= 0.75;
        }
      }
      else {
        if (zoomLevel < 1.2) {
          zoomLevel *= 1.0/0.75;
        }
      }
    }
    if (IsKeyPressed(KEY_SPACE)) {
      if (timeOffset != ctr.getLastTime()) {
        run = !run; 
        pauseOffset = timeOffset;
      }
    }
    if (IsKeyDown(KEY_LEFT)) {
      if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        if (timeOffset > shiftC * 60) {
          timeOffset -= shiftC * 60;
        }
        else if (timeOffset > 0) {
          timeOffset = 0;
        }
      }
      else if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        // not supported in live mode
        if (!ctr.getLiveState()) {
          bool measureFirst = true;
          for (unsigned int i = ctr.getStream().measureMap.size()-1; i > 0; --i) {
            double measureLineX = convertSSX(ctr.getStream().measureMap[i].getLocation());
            if (measureLineX < nowLineX-1) {
              timeOffset = unconvertSSX(measureLineX);
              measureFirst = false;
              break;
            }
          }
          if (measureFirst) {
            timeOffset = 0;
          }
        }
      }
      else {
        if (timeOffset > shiftC * 6) {
          timeOffset -= shiftC * 6;
        }
        else if (timeOffset > 0) {
          timeOffset = 0;
        }
      }
    }
    if (IsKeyDown(KEY_RIGHT)) {
      if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        if (timeOffset + shiftC * 60 < ctr.getLastTime()) {
          timeOffset += shiftC * 60;
        }
        else if (timeOffset < ctr.getLastTime()) {
          timeOffset = ctr.getLastTime();
        }
      }
      else if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        // immediately move to next measure
        bool measureLast = true;
        for (auto& measure : ctr.getStream().measureMap) {
          double measureLineX = convertSSX(measure.getLocation());
          if (measureLineX > nowLineX+1) {
            timeOffset = unconvertSSX(measureLineX);
            measureLast = false;
            break;
          }
        }
        if (measureLast) {
          timeOffset = ctr.getLastTime();
        }
      }
      else {
        if (timeOffset + shiftC * 6 < ctr.getLastTime()) {
          timeOffset += shiftC * 6;
        }
        else {
          timeOffset = ctr.getLastTime();
        }
      }
    }
    if (IsKeyDown(KEY_HOME)) {
      timeOffset = 0;
    }
    if (IsKeyDown(KEY_END)) {
      timeOffset = ctr.getLastTime();
      pauseOffset = timeOffset;
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {

      if (!hoverType.contains(HOVER_DIALOG)) {
        ctr.dialog.preferenceDisplay = ctr.dialog.infoDisplay = false;
      }

      switch(fileMenu.getActiveElement()) {
        case -1:
          fileMenu.render = false;
          break;
        case 0:
          fileMenu.render = !fileMenu.render;
          break;
        default:
          if (!fileMenu.render) {
            break;
          }
          switch(fileMenu.getActiveElement()) {
            case 1:
              action = actionType::ACTION_OPEN;
              break;
            case 2:
              action = actionType::ACTION_OPEN_IMAGE;
              break;
            case 3:
              action = actionType::ACTION_SAVE;
              break;
            case 4:
              action = actionType::ACTION_SAVE_AS;
              break;
            case 5:
              action = actionType::ACTION_CLOSE;
              break;
            case 6:
              action = actionType::ACTION_CLOSE_IMAGE;
              break;
            case 7:
                ctr.setCloseFlag(); 
              break;
          }
          break;
      }
      switch(editMenu.getActiveElement()) {
        case -1:
          editMenu.render = false;
          break;
        case 0:
          editMenu.render = !editMenu.render;
          break;
        default:
          if (!editMenu.render) {
            break;
          }
          switch(editMenu.getActiveElement()) {
            case 1:
              action = actionType::ACTION_SHEET;
              break;
            case 2:
              action = actionType::ACTION_PREFERENCES;
              break;
            case 3:
              break;
            case 4:
              break;
            case 5:
              break;
          }
          break;
      }
      switch(displayMenu.getActiveElement()) {
        case -1:
          if (!displayMenu.parentOpen() || displayMenu.parent->getActiveElement() == -1) {
            displayMenu.render = false;
          }
          break;
        default:
          if (!displayMenu.render) {
            break;
          }
          switch(displayMenu.getActiveElement()) {
            case 0:
              displayMode = DISPLAY_BAR;
              break;
            case 1:
              displayMode = DISPLAY_LINE;
              break;
            case 2:
              displayMode = DISPLAY_PULSE;
              break;
            case 3:
              displayMode = DISPLAY_BALL;
              break;
            case 4:
              displayMode = DISPLAY_FFT;
              break;
            case 5:
              displayMode = DISPLAY_VORONOI;
              break;
            case 6:
              displayMode = DISPLAY_LOOP;
              break;
            case 7:
              break;
            case 8:
              break;
            case 9:
              break;
          }
          break;
      }
      switch(songMenu.getActiveElement()) {
        case -1:
          if (!songMenu.parentOpen() || songMenu.parent->getActiveElement() == -1) {
            songMenu.render = false;
          }
          break;
        default:
          if (!songMenu.render) {
            break;
          }
          switch(songMenu.getActiveElement()) {
            case 0:
              songTimeType = SONGTIME_RELATIVE;
              viewMenu.setContent("Hide Song Time", 2);
              break;
            case 1:
              songTimeType = SONGTIME_ABSOLUTE;
              viewMenu.setContent("Hide Song Time", 2);
              break;
            case 2:
              break;
            case 3:
              break;
            case 4:
              break;
            case 5:
              break;
          }
          break;
      }
      switch(viewMenu.getActiveElement()) {
        case -1:
          if (!viewMenu.childOpen()) {
            viewMenu.render = false;
          }
          break;
        case 0:
          viewMenu.render = !viewMenu.render;
          break;
        default:
          if (!viewMenu.render) {
            break;
          }
          switch(viewMenu.getActiveElement()) {
            case 1:
              if (viewMenu.childOpen() && displayMenu.render == false) {
                viewMenu.hideChildMenu();
                displayMenu.render = true;
              }
              else {
                displayMenu.render = !displayMenu.render;
              }
              break;
            case 2:
              if (viewMenu.childOpen() && songMenu.render == false) {
                viewMenu.hideChildMenu();
                songMenu.render = true;
              }
              else {
                if (viewMenu.getContent(2) == "Hide Song Time") {
                  viewMenu.setContent("Display Song Time:", 2);  
                  songTimeType = SONGTIME_NONE;
                }
                else {
                  songMenu.render = !songMenu.render;
                }
              }
              break;
            case 3:
              if (viewMenu.getContent(3) == "Display Key Signature") {
                viewMenu.setContent("Hide Key Signature", 3);
              }
              else if (viewMenu.getContent(3) == "Hide Key Signature") {
                viewMenu.setContent("Display Key Signature", 3);
              }
              showKey = !showKey;
              break;
            case 4:
              if (viewMenu.getContent(4) == "Show Now Line") {
                viewMenu.setContent("Hide Now Line", 4);
              }
              else if (viewMenu.getContent(4) == "Hide Now Line") {
                viewMenu.setContent("Show Now Line", 4);
              }
              nowLine = !nowLine;
              break;
            case 5:
              if (viewMenu.getContent(5) == "Show Measure Line") {
                viewMenu.setContent("Hide Measure Line", 5);
              }
              else if (viewMenu.getContent(5) == "Hide Measure Line") {
                viewMenu.setContent("Show Measure Line", 5);
              }
              measureLine = !measureLine;
              break;
            case 6:
              if (viewMenu.getContent(6) == "Show Measure Number") {
                viewMenu.setContent("Hide Measure Number", 6);
              }
              else if (viewMenu.getContent(6) == "Hide Measure Number") {
                viewMenu.setContent("Show Measure Number", 6);
              }
              measureNumber = !measureNumber;
              break;
            case 7:
              if (viewMenu.getContent(7) == "Show Background") {
                viewMenu.setContent("Hide Background", 7);
              }
              else if (viewMenu.getContent(7) == "Hide Background") {
                viewMenu.setContent("Show Background", 7);
              }
              showImage = !showImage;
              break;
            case 8:
              if (viewMenu.getContent(8) == "Show FPS") {
                viewMenu.setContent("Hide FPS", 8);
              }
              else if (viewMenu.getContent(8) == "Hide FPS") {
                viewMenu.setContent("Show FPS", 8);
              }
              showFPS = !showFPS;
              FPSText = to_string(GetFPS());
              break;
          }
          break;
      }
      switch(inputMenu.getActiveElement()) {
        case -1:
          if (!inputMenu.parentOpen() || inputMenu.parent->getActiveElement() == -1) {
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
      switch(outputMenu.getActiveElement()) {
        case -1:
          if (!outputMenu.parentOpen() || outputMenu.parent->getActiveElement() == -1) {
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
      switch(midiMenu.getActiveElement()) {
        case -1:
          if (!midiMenu.childOpen()) {
            midiMenu.render = false;
          }
          break;
        case 0:
          midiMenu.render = !midiMenu.render;
          break;
        default:
          if (!midiMenu.render) {
            break;
          }
          switch(midiMenu.getActiveElement()) {
            case 1:
              inputMenu.update(ctr.input.getPorts());
              if (midiMenu.childOpen() && !inputMenu.render) {
                midiMenu.hideChildMenu();
                inputMenu.render = true;
              }
              else {
                inputMenu.render = !inputMenu.render;
              }
              break;
            case 2:
              outputMenu.update(ctr.output.getPorts());
              if (midiMenu.childOpen() && !outputMenu.render) {
                midiMenu.hideChildMenu();
                outputMenu.render = true;
              }
              else {
                outputMenu.render = !outputMenu.render;
              }
              break;
            case 3:
              action = actionType::ACTION_LIVEPLAY;
              break;
            case 4:
              break;
            case 5:
              break;
          }
          break;
      }
      switch(schemeMenu.getActiveElement()) {
        case -1:
          if (!schemeMenu.parentOpen() || schemeMenu.parent->getActiveElement() == -1) {
            schemeMenu.render = false;
          }
          break;
        default:
          if (!schemeMenu.render) {
            break;
          }
          switch(schemeMenu.getActiveElement()) {
            case 0:
              colorMode = COLOR_PART;
              colorSetOn = &ctr.setTrackOn;
              colorSetOff = &ctr.setTrackOff;
              break;
            case 1:
              colorMode = COLOR_VELOCITY;
              colorSetOn = &ctr.setVelocityOn;
              colorSetOff = &ctr.setVelocityOff;
              break;
            case 2:
              colorMode = COLOR_TONIC;
              colorSetOn = &ctr.setTonicOn;
              colorSetOff = &ctr.setTonicOff;
              break;
            case 3:
              break;
            case 4:
              break;
            case 5:
              break;
          }
          break;
      }
      switch(paletteMenu.getActiveElement()) {
        case -1:
          if (!paletteMenu.parentOpen() || paletteMenu.parent->getActiveElement() == -1) {
            paletteMenu.render = false;
          }
          break;
        default:
          if (!paletteMenu.render) {
            break;
          }
          switch(paletteMenu.getActiveElement()) {
            case 0:
              getColorScheme(KEY_COUNT, ctr.setVelocityOn, ctr.setVelocityOff);
              getColorScheme(TONIC_COUNT, ctr.setTonicOn, ctr.setTonicOff);
              getColorScheme(ctr.getTrackCount(), ctr.setTrackOn, ctr.setTrackOff, ctr.file.trackHeightMap);
              break;
            case 1:
              if (ctr.image.exists()) {
                getColorSchemeImage(SCHEME_KEY, ctr.setVelocityOn, ctr.setVelocityOff);
                getColorSchemeImage(SCHEME_TONIC, ctr.setTonicOn, ctr.setTonicOff);
                getColorSchemeImage(SCHEME_TRACK, ctr.setTrackOn, ctr.setTrackOff, ctr.file.trackHeightMap);
              }
              else{
                logW(LL_WARN, "attempt to get color scheme from nonexistent image");
              }
              break;

          }
          break;
      }
      switch(colorMenu.getActiveElement()) {
        case -1:
          if (!colorMenu.childOpen()) {
            colorMenu.render = false;
          }
          break;
        case 0:
          colorMenu.render = !colorMenu.render;
          break;
        default:
          if (!colorMenu.render) {
            break;
          }
          switch(colorMenu.getActiveElement()) {
            case 1:
              if (colorMenu.childOpen() && schemeMenu.render == false) {
                colorMenu.hideChildMenu();
                schemeMenu.render = true;
              }
              else {
                schemeMenu.render = !schemeMenu.render;
              }
             break;
             case 2:
               if (colorMenu.childOpen() && paletteMenu.render == false) {
                 colorMenu.hideChildMenu();
                 paletteMenu.render = true;
               }
               else {
                 paletteMenu.render = !paletteMenu.render;
               }
               break;
             case 3:
               swap(colorSetOn, colorSetOff);
               break;
             case 4:
               invertColorScheme(ctr.bgColor, ctr.bgNow, colorSetOn, colorSetOff);
               break;
             case 5:
               break;
          }
          break;
      }
      switch(infoMenu.getActiveElement()) {
        case -1:
          if (!infoMenu.childOpen()) {
            infoMenu.render = false;
          }
          break;
        case 0:
          infoMenu.render = !infoMenu.render;
          break;
        default:
          if (!infoMenu.render) {
            break;
          }
          switch(infoMenu.getActiveElement()) {
            case 1:
              action = actionType::ACTION_INFO;
              break;
            case 2:
              infoMenu.render = false;
              OpenURL(SITE_LINK);
              break;
          }
          break;
      }
      switch(colorSelect.getActiveElement()) {
        case -1:
          if (!colorSelect.parentOpen() || colorSelect.parent->getActiveElement() == -1) {
            colorSelect.render = false;
          }
          break;
        default:
          if (!colorSelect.render) {
            break;
          }
          switch(colorSelect.getActiveElement()) {
            case 0:
              colorMove = true;
              if (pointInBox(getMousePosition(), colorSelect.getSquare()) || colorSelect.clickCircle(1)) {
                colorSquare = true;
              }
              else if (colorSelect.clickCircle(0)) {
                colorCircle = true;
              }
              break;
            case 1:
              break;
            case 2:
              break;
            case 3:
              break;
            case 4:
              break;
            case 5:
              break;
          }
          break;
      }
      switch(rightMenu.getActiveElement()) {
        case -1:
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
          switch(rightMenu.getActiveElement()) {
            case 0:
              if (rightMenu.getSize() == 1) {
                if (rightMenu.childOpen() && colorSelect.render == false) {
                  rightMenu.hideChildMenu();
                }
                else {
                  if (rightMenu.getContent(0) == "Remove Image") {
                    //logQ("attempted to remove image: size:");
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
              tonicOffset = (ctr.getNotes()[clickNote].y - MIN_NOTE_IDX + tonicOffset) % 12;
              break;
          }
          break;
      }

      if (!hoverType.contains(HOVER_NOW, HOVER_NOTE, HOVER_MEASURE, HOVER_MENU, HOVER_SHEET, HOVER_DIALOG) && 
          !hoverType.containsLastFrame(HOVER_MENU, HOVER_MEASURE)) {
        if (hoverType.contains(HOVER_IMAGE)) {
          ctr.image.updateBasePosition();
          //logQ("UPDATE BASE");
        }
      }

      if (ctr.dialog.preferenceDisplay) {
        ctr.dialog.process();
      }

    }
    //logQ("can image move?:", !hoverType.contains(HOVER_NOW, HOVER_NOTE, HOVER_MEASURE, HOVER_MENU, HOVER_SHEET) && 
          //!hoverType.containsLastFrame(HOVER_MENU, HOVER_MEASURE));
    //logQ("label", rightMenuContents[1], "v.", rightMenu.getContent(0));
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        colorMove = false;
        colorSquare = false;
        colorCircle = false;

        nowMove = false;

        ctr.image.finalizePosition();
    }
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {    
      ctr.image.updatePosition();

      if (pointInBox(getMousePosition(), 
                     {static_cast<int>(nowLineX - 3), ctr.barHeight, 6, ctr.getHeight() - ctr.barHeight}) &&
          !hoverType.contains(HOVER_DIALOG) && 
          !colorSelect.render && 
          !ctr.menu->mouseOnMenu() &&
          !ctr.image.movable()) {
        nowMove = true;
      }
      if (nowMove) {
        // provide reset
        if (IsKeyDown(KEY_LEFT_SHIFT)) {
          nowLineX = ctr.getWidth()/2.0;
          nowMove = false;  // disable after rest
        }
        else {
          nowLineX = clampValue(getMousePosition().x, 1, ctr.getWidth()-1);
        }
      }
    }
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
      clickNote = clickTmp;
      clickOn = clickOnTmp;
      ctr.menu->hide();

      if (!ctr.menu->mouseOnMenu()) {
        if (!pointInBox(getMousePosition(), (rect){0, 0, ctr.getWidth(), 20})) {
          
          int rightX = 0, rightY = 0, colorX = 0, colorY = 0;

          if (clickNote != -1) {
            rightX = round(nowLineX + (ctr.getNotes()[clickNote].x - timeOffset) * zoomLevel);
            rightY = (ctr.getHeight() - round((ctr.getHeight() - ctr.menuHeight) * 
                      static_cast<double>(ctr.getNotes()[clickNote].y - MIN_NOTE_IDX + 3)/(NOTE_RANGE + 3)));
          }
          
          // find coordinate to draw right click menu
          rightMenu.findMenuLocation(rightX, rightY);
          rightMenu.findColorSelectLocation(colorX, colorY, rightX, rightY);
          
          colorSelect.setXY(colorX, colorY);

          if (clickNote != -1) {
            selectType = SELECT_NOTE;
            rightMenuContents[1] = "Change Part Color";
            rightMenuContents[2] = "Set Tonic";
            rightMenu.update(rightMenuContents);
            rightMenu.setContent(getNoteInfo(ctr.getNotes()[clickNote].track, 
                                             ctr.getNotes()[clickNote].y - MIN_NOTE_IDX), 0);
            
            // set note color for color wheel
            if (clickOn) {
              colorSelect.setColor(ctr.setTrackOn[ctr.getNotes()[clickNote].track]);
            }
            else{
              colorSelect.setColor(ctr.setTrackOff[ctr.getNotes()[clickNote].track]);
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
              rightMenuContents[1] = "Change Line Color";
              colorSelect.setColor(ctr.bgNow);
            }
            else {
              bool measureSelected = false;
              for (unsigned int i = 0; i < ctr.getStream().measureMap.size(); i++) {
                double measureLineX = convertSSX(ctr.getStream().measureMap[i].getLocation());
                if (pointInBox(getMousePosition(), 
                               {static_cast<int>(measureLineX - 3), ctr.barHeight, 6, ctr.getHeight() - ctr.barHeight}) && 
                    !ctr.menu->mouseOnMenu() && !hoverType.contains(HOVER_DIALOG)) {
                  measureSelected = true;
                  break; 
                }
              }
              
              
              if (measureSelected) {
                selectType = SELECT_MEASURE;
                rightMenuContents[1] = "Change Line Color";
                colorSelect.setColor(ctr.bgMeasure);
              }
              else if (ctr.image.exists() &&
                      (!hoverType.contains(HOVER_NOW, HOVER_NOTE, HOVER_MEASURE, HOVER_MENU, HOVER_SHEET, HOVER_DIALOG) && 
                       !hoverType.containsLastFrame(HOVER_MENU) && hoverType.contains(HOVER_IMAGE))) { 
                    selectType = SELECT_NONE; // no color change on image
                    rightMenuContents[1] = "Remove Image";
                    //logQ("rightclicked on image");
              }
              else if (!hoverType.contains(HOVER_DIALOG)) {
                selectType = SELECT_BG;
                rightMenuContents[1] = "Change Color";
                colorSelect.setColor(ctr.bgColor);
              }
              else {
                selectType = SELECT_NONE;
                rightMenuContents[1] = "Empty";
              }
            }
            rightMenu.setContent("", 0);

            //logQ(formatVector(rightMenuContents));
            vector<string> newRight(rightMenuContents.begin() + 1, rightMenuContents.end() - 1);
            rightMenu.update(newRight);
          }
          rightMenu.setXY(rightX, rightY);
          
          if (!hoverType.contains(HOVER_DIALOG)) {
            ctr.dialog.preferenceDisplay = ctr.dialog.infoDisplay = false;
            rightMenu.render = true;
          }
        }
      }
    }
    if (ctr.menu->mouseOnMenu() || pointInBox(getMousePosition(), {0, 0, ctr.getWidth(), ctr.menuHeight})) {
      hoverType.add(HOVER_MENU);
    }

    ctr.update(timeOffset, nowLineX, run);
  }

  ctr.unloadData();
  return 0;
}
