#include <numeric>
#include "sheetctr.h"
#include "define.h"
#include "enum.h"
#include "wrap.h"

using std::accumulate;

int sheetController::getGlyphWidth(int codepoint, int size) {
  return GetGlyphInfo(*ctr.getFont("LELAND", size), codepoint).image.width;
}

void sheetController::drawTimeSignature(const timeSig& time, int x, colorRGB col) {
  // y-coordinate is constant in respect to sheet controller parameters, not a user-callable parameter
  const int y = ctr.barMargin+ctr.barWidth*2;


  // for centering
  drawRing({static_cast<float>(x), static_cast<float>(y)}, 0, 2, {255,0,0});

  if (time.getTop() < 0 || time.getTop() > 99 || time.getBottom() < 0 || time.getBottom() > 99) {
    logW(LL_WARN, "complex time signature detected with meter", time.getTop(), "/", time.getBottom());
    return;
  }

  x = x + getTimeWidth(timeSig(time.getTop(),time.getBottom(),0))/2;

  // the given x-coordinate is the spatial center of the time signature
  // the given y-coordinate is the top line of the upper staff
  

  auto drawSingleSig = [&](int val, int yOffset) {
    const int point = SYM_TIME_0+val;
    const int halfDist = (getGlyphWidth(point) + 1)/2;

    drawSymbol(point, fSize, x-halfDist, y+yOffset, col);

  };
  auto drawDoubleSig = [&](int val, int yOffset) {
    int padding = 0;
    
    int numL = val/10;
    int numR = val%10;

    if (numL == 1) {
      padding = -2;
    }

    const int pointL = SYM_TIME_0+numL;
    const int widthL= getGlyphWidth(pointL);
    const int pointR = SYM_TIME_0+numR;
    const int widthR= getGlyphWidth(pointR);


    const int leftPos = x - (padding + widthL + widthR)/2;
    const int rightPos = leftPos + widthL + padding;

    drawSymbol(pointL, fSize, leftPos, y+yOffset, col);
    

    drawSymbol(pointR, fSize, rightPos, y+yOffset, col);

  };

  if (time.getTop() > 9) { // two-digit top
    drawDoubleSig(time.getTop(), (ctr.sheetHeight-200)/2-ctr.barMargin-ctr.barWidth*2 + 1);
    drawDoubleSig(time.getTop(), (ctr.sheetHeight-200)/2-ctr.barMargin+ctr.barSpacing-ctr.barWidth*2 + 1);
  }
  else {
    drawSingleSig(time.getTop(), (ctr.sheetHeight-200)/2-ctr.barMargin-ctr.barWidth*2 + 1);
    drawSingleSig(time.getTop(), (ctr.sheetHeight-200)/2-ctr.barMargin+ctr.barSpacing-ctr.barWidth*2 + 1);
  }

  if (time.getBottom() > 9) { // two-digit bottom
    drawDoubleSig(time.getBottom(), (ctr.sheetHeight-200)/2-ctr.barMargin + 1);
    drawDoubleSig(time.getBottom(), (ctr.sheetHeight-200)/2-ctr.barMargin+ctr.barSpacing + 1);
  }
  else {
    drawSingleSig(time.getBottom(), (ctr.sheetHeight-200)/2-ctr.barMargin + 1);
    drawSingleSig(time.getBottom(), (ctr.sheetHeight-200)/2-ctr.barMargin+ctr.barSpacing + 1);
  }
}

void sheetController::drawKeySignature(const keySig& key, int x, colorRGB col) {
  const int y = ctr.barMargin+ctr.barWidth*2;

  // for centering
  drawRing({static_cast<float>(x), static_cast<float>(y)}, 0, 2, {255,0,0});

  int symbol = SYM_NONE;
  int prevAcc = 0;
  int prevType = SYM_ACC_NATURAL;
  findKeyData(key, symbol, prevAcc, prevType);
  
 
  auto drawKeySigPart = [&] (const int symbol, const int index, const int prevType) {
    if (index > accMax || index < 0) { return; }
    
    int* accHash = const_cast<int*>(flatHash);
    int* accSpacing = const_cast<int*>(flatSpacing);
    int* accY = const_cast<int*>(flatY);

    double posMod = 0;
    
    switch(symbol) {
      case SYM_ACC_SHARP:
        accHash = const_cast<int*>(sharpHash);
        accSpacing = const_cast<int*>(sharpSpacing);
        accY = const_cast<int*>(sharpY);
        posMod = accHash[index]-1;
        break;
      case SYM_ACC_FLAT:
        accHash = const_cast<int*>(flatHash);
        accSpacing = const_cast<int*>(flatSpacing);
        accY = const_cast<int*>(flatY);
        posMod = accHash[index]-1;
        break;
      case SYM_ACC_NATURAL:
        switch(prevType) {
          case SYM_ACC_SHARP:
            accHash = const_cast<int*>(sharpHash);
            accSpacing = const_cast<int*>(sharpSpacing);
            accY = const_cast<int*>(sharpY);
            posMod = accHash[index]-1;
            break;
          case SYM_ACC_FLAT:
            accHash = const_cast<int*>(flatHash);
            accSpacing = const_cast<int*>(flatSpacing);
            accY = const_cast<int*>(flatY);
            posMod = accHash[index]-1;
            break;
        }
        break;
    }
    //logQ(getGlyphWidth(symbol) + index*accConstSpacing+accSpacing[index]);

    drawSymbol(symbol, fSize, x+index*accConstSpacing+accSpacing[index], 
                              y-ctr.barSpacing+posMod*(ctr.barWidth/2.0f+0.4)-accY[index], col);
    drawSymbol(symbol, fSize, x+index*accConstSpacing+accSpacing[index], 
                              y+posMod*(ctr.barWidth/2.0f+0.4)-accY[index]+1+ctr.barWidth, col);
  };

  int drawLimit = symbol == SYM_ACC_NATURAL ? prevAcc : key.getSize();

  for (auto i = 0; i < drawLimit; i++) {
    drawKeySigPart(symbol, i, prevType);
  }
}


void sheetController::drawNote(const sheetNote& noteData, int x, colorRGB col) {
  const int y = ctr.barMargin+ctr.barWidth*2;

  // for centering
  drawRing({static_cast<float>(x), static_cast<float>(y)}, 0, 2, {255,0,0});

  drawSymbol(SYM_HEAD_STD, fSize, x, y+1, col);
}

int sheetController::getKeyWidth(const keySig& key) {
  int symbol = SYM_NONE;
  int prevAcc = 0;
  int prevType = SYM_ACC_NATURAL;
  findKeyData(key, symbol, prevAcc, prevType);
  
  //drawLine(x,0,x,ctr.getHeight(),ctr.bgNow);
 
  int xBound = 0;
  switch(symbol) {
    case SYM_ACC_SHARP:
      xBound += sharpWidths[key.getSize()];
      break;
    case SYM_ACC_FLAT:
      xBound += flatWidths[key.getSize()];
      break;
    case SYM_ACC_NATURAL:
        // adjustments are due to difference in width between the accidental symbols
        switch(prevType) {
          case SYM_ACC_SHARP:
            xBound += sharpWidths[prevAcc]-3;
            break;
          case SYM_ACC_FLAT:
            xBound += flatWidths[prevAcc]-1;
            break;
        }
      break;
  }

  //drawLine(x+xBound,0,x+xBound,ctr.getHeight(),ctr.bgSheetNote);

  return xBound;
}

int sheetController::getTimeWidth(const timeSig& key) {
  return max(findTimePartWidth(key.getTop()), findTimePartWidth(key.getBottom()));
}

int sheetController::findTimePartWidth(const int part) {
  // error checking already done earlier
  if (part < 10) {
    return timeWidths[part]; 
  }

  // account for shortening of "1" glyph in range 10-19
  if (part/10 == 1) {
    return timeWidths[part/10]-2+timeWidths[part%10];
  }
  
  return timeWidths[part/10]+timeWidths[part%10];
}

void sheetController::findKeyData(const keySig& key, int& symbol, int& prevAcc, int& prevType) {
  symbol = SYM_NONE;
  prevAcc = 0;
  prevType = SYM_NONE;

  if (key.getAcc() == 0) {
    // C major / A minor
    // find number of courtesy accidentals
    symbol = SYM_ACC_NATURAL;
    if (key.getPrev() != nullptr) {
      prevAcc = key.getPrev()->getSize();
    }
    if (prevAcc != 0) {
      prevType = key.getPrev()->isSharp() ? SYM_ACC_SHARP : SYM_ACC_FLAT;
    }
  }
  else if (key.isSharp()) {
    symbol = SYM_ACC_SHARP;
  }
  else {
    symbol = SYM_ACC_FLAT;
  }
}

void sheetController::disectMeasure(measureController& measure) {
  //logQ("measure", measure.getNumber(), "has timesig", measure.currentTime.getTop(), measure.currentTime.getBottom());
  // preprocessing
  sheetMeasure dm;
  dm.buildChordMap(measure.displayNotes);


  // run a DFA for each valid midi key, advancing state based on new-note-to-midi-position values
  // each DFA set is unique to its measure
  // add "1" to size for inclusive bounding
  // TODO: make initial state depend on keysig accidental at that position
  vector<int> presentDFAState(MAX_STAVE_IDX - MIN_STAVE_IDX + 1);
  findDFAStartVector(presentDFAState, measure.currentKey); 


  logQ(formatVector(presentDFAState));

  //logQ(measure.getNumber(), "has", dm.chords.size(), "chords");
  for (const auto& c : dm.chords) {
    for (const auto& n : c.second) {

      //int& keyPos = presentDFAState[n->oriNote->sheetY];
      n->displayAcc = getDisplayAccType(presentDFAState[mapSheetY(n->oriNote->sheetY)], n->oriNote->accType);
      logQ(presentDFAState[mapSheetY(n->oriNote->sheetY)], n->displayAcc, n->oriNote->accType);
    }

    //logQ(formatVector(presentDFAState));
    //logQ("chord", ch++, "@", c.first, "has", c.second.size(), "quantized notes");
  }


  // left measure spacing
  dm.addSpace(borderSpacing);


  //logQ("MEASURE", measure.getNumber(), "with length", measure.getTickLen());



  if (measure.notes.size() == 0) {
    // fill space with rests

  }



  // right measure spacing
  // last item before ending this item
  dm.addSpace(borderSpacing);
  
  displayMeasure.push_back(dm);
}

void sheetController::findDFAStartVector(vector<int>& DFAState, const keySig& ks) {
  //DFAState.resize(MAX_STAVE_IDX - MIN_STAVE_IDX + 1);


  const int* keyStateTemplate = staveKeySigMap[ks.getKey()];

  for (unsigned int idx = 0; idx < DFAState.size(); ++idx) {
    int templateIdx = idx + abs(MIN_STAVE_IDX) + abs(MIN_STAVE_IDX) % 7;
    DFAState[idx] = keyStateTemplate[templateIdx % 7]; 
  }
}

int sheetController::mapSheetY(int sheetY) {
  // input [0, MIN_STAVE_IDX + MAX_STAVE_IDX]
  return sheetY + abs(MIN_STAVE_IDX);
}

int sheetController::getDisplayAccType(int& DFAState, int noteAccType) {
  switch(DFAState) {
    case DA_STATE_CLEAR:
      switch(noteAccType) {
        case ACC_NONE:
          DFAState = DA_STATE_CLEAR;
          return ACC_NONE;
        case ACC_SHARP:
          DFAState = DA_STATE_SHARP;
          return ACC_SHARP;
        case ACC_FLAT:
          DFAState = DA_STATE_FLAT;
          return ACC_FLAT;
      }
      break;
    case DA_STATE_SHARP:
      switch(noteAccType) {
        case ACC_NONE:
          DFAState = DA_STATE_NATURAL;
          return ACC_NATURAL;
        case ACC_SHARP:
          DFAState = DA_STATE_SHARP_MULT;
          return ACC_NONE;
        case ACC_FLAT:
          DFAState = DA_STATE_FLAT;
          return ACC_FLAT;
      }
      break;
    case DA_STATE_FLAT:
      switch(noteAccType) {
        case ACC_NONE:
          DFAState = DA_STATE_NATURAL;
          return ACC_NATURAL;
        case ACC_SHARP:
          DFAState = DA_STATE_SHARP;
          return ACC_SHARP;
        case ACC_FLAT:
          DFAState = DA_STATE_FLAT_MULT;
          return ACC_NONE;
      }
      break;
    case DA_STATE_NATURAL:
      switch(noteAccType) {
        case ACC_NONE:
          DFAState = DA_STATE_CLEAR;
          return ACC_NONE;
        case ACC_SHARP:
          DFAState = DA_STATE_SHARP;
          return ACC_SHARP;
        case ACC_FLAT:
          DFAState = DA_STATE_FLAT;
          return ACC_FLAT;
      }
      break;
    case DA_STATE_SHARP_MULT:
      switch(noteAccType) {
        case ACC_NONE:
          DFAState = DA_STATE_NATURAL;
          return ACC_NATURAL;
        case ACC_SHARP:
          DFAState = DA_STATE_SHARP_MULT;
          return ACC_NONE;
        case ACC_FLAT:
          DFAState = DA_STATE_FLAT;
          return ACC_FLAT;
      }
      break;
    case DA_STATE_FLAT_MULT:
      switch(noteAccType) {
        case ACC_NONE:
          DFAState = DA_STATE_NATURAL;
          return ACC_NATURAL;
        case ACC_SHARP:
          DFAState = DA_STATE_SHARP;
          return ACC_SHARP;
        case ACC_FLAT:
          DFAState = DA_STATE_FLAT_MULT;
          return ACC_NONE;
      }
      break;
    default:
      logW(LL_WARN, "invalid DFA state or accidental type for \
                     display accidental calculation (state, type):", DFAState, noteAccType);
      break;
  }

  return -1;
}
