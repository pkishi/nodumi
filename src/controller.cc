#include "controller.h"

void controller::updateKeyState() {
  if (WindowShouldClose()) {
    programState = false;
  }
}

void controller::toggleLivePlay() {
  if (setTrackOn.size() < 1) {
    getColorScheme(2, setTrackOn, setTrackOff);
  }
  livePlayState = !livePlayState;
  if (livePlayState) {
    notes = &liveInput.noteStream.notes;
  }
  else {
    notes = &file.notes;
  }
}

int controller::getTrackCount() {
  if (livePlayState) {
    return 1;
  }
  return file.getTrackCount();
}

int controller::getNoteCount() {
  if (livePlayState) {
    return liveInput.getNoteCount();
  }
  return file.getNoteCount();
}

int controller::getLastTick() {
  if (livePlayState) {
    return 0;
  }
  return file.getLastTick();
}

int controller::getTempo(int idx) {
  if (livePlayState) {
    return 120;
  }
  else {
    return file.getTempo(idx);
  }
}

void controller::load(string filename) {
  file.load(filename);
  getColorScheme(file.getTrackCount(), setTrackOn, setTrackOff, file.trackHeightMap);
}

void controller::setCloseFlag() {
  programState = false;
}