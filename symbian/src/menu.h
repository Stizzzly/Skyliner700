#ifndef SKY_MENU_H
#define SKY_MENU_H
#include "soft3d.h"
struct MenuState {
    enum Mode { Main,Flight,Paused };
    enum Action { None,Start,Resume,Return,Exit };
    Mode mode;int selection;
    MenuState():mode(Main),selection(0){}
    int Count()const{return mode==Main?2:3;}
    bool Visible()const{return mode!=Flight;}
    void Move(int direction){if(Visible())selection=(selection+direction+Count())%Count();}
    void Pause(){if(mode==Flight){mode=Paused;selection=0;}}
    Action Select(){
        if(mode==Main){if(selection==1)return Exit;mode=Flight;return Start;}
        if(mode==Paused){if(selection==2)return Exit;if(selection==1){mode=Main;selection=0;return Return;}mode=Flight;return Resume;}
        return None;
    }
};
void SkyDrawMenu(Pixel* pixels,int stride,const MenuState& state,const Pixel* background,int backgroundStride);
void SkyDrawIcon(Pixel* pixels,int stride,int width,int height);
#endif
