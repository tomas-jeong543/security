#pragma once
#include "Hero.h"
#include <windows.h>
struct PlayerCommand{Vec2 move,mouseWorldPosition;bool basic=false,ultimate=false,togglePause=false,start=false,restart=false;UpgradeChoice upgrade=UpgradeChoice::None;};
class InputManager{public:PlayerCommand Read(HWND window);void ResetEdges();static Vec2 MoveVector(bool left,bool right,bool up,bool down){return Normalize({static_cast<float>(right-left),static_cast<float>(down-up)});}static bool BasicCommand(bool leftMouse,bool keyK){return leftMouse||keyK;}private:bool previousBasic_=false,previousUltimate_=false,previousPause_=false,previousEnter_=false,previousRestart_=false,previous1_=false,previous2_=false,previous3_=false;};
