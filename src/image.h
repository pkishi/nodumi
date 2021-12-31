 #pragma once

#include <raylib.h>
#include <string>
#include "box.h"

using std::string;

class imageController {
  public:
    imageController() : img(),  imgTex(), isLoaded(false), canMove(false),
                        position({0,0}), base({0,0}), offset({0,0}), rotation(0), scale(1) {};

    void load(string path);
    void unload();

    void draw();

    bool exists() { return isLoaded; }

    int getX() { return position.x; }
    int getY() { return position.y; }
    int getWidth() { return isLoaded ? img.width : -1; }
    int getHeight() { return isLoaded ? img.height : -1; }

    
    void updatePosition();
    void updateBasePosition();
    void finalizePosition();

  private:
    
    Image img;
    Texture2D imgTex;
    
    bool isLoaded;
    bool canMove;

    point position;
    point base;
    point offset;
    float rotation;
    float scale;
    

};
