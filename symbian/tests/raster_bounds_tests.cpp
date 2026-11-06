// Compare span bounds with an independent per-pixel edge/plane oracle.
#define SKY_TEST_RASTER_COUNTS
#define private public
#include "../src/soft3d.cpp"
#undef private
#include <stdio.h>
#include <stdlib.h>
static Soft3D renderer;
static Pixel actual[(SKY_W+3)*SKY_H+32],expected[(SKY_W+3)*SKY_H+32],expectedZ[SKY_W*SKY_H];
static unsigned oldFloors=0,oldInverses=0;
static void Check(bool ok,const char* message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
static void Reference(Screen a,Screen b,Screen c,Pixel color){
    Screen s[3]={a,b,c};int area=Edge(a,b,c.x,c.y);
    if(!area)return;
    if(area<0){Screen t=s[1];s[1]=s[2];s[2]=t;area=-area;}
    int minx=s[0].x,maxx=minx,miny=s[0].y,maxy=miny;
    for(int i=1;i<3;++i){if(s[i].x<minx)minx=s[i].x;if(s[i].x>maxx)maxx=s[i].x;if(s[i].y<miny)miny=s[i].y;if(s[i].y>maxy)maxy=s[i].y;}
    minx=minx/16;maxx=(maxx+15)/16;miny=miny/16;maxy=(maxy+15)/16;
    if(minx<0)minx=0;if(maxx>=renderer.width)maxx=renderer.width-1;
    if(miny<0)miny=0;if(maxy>=renderer.height)maxy=renderer.height-1;
    if(minx>maxx||miny>maxy)return;
    int row[3],dx[3],dy[3];
    for(int e=0;e<3;++e){Screen u=s[(e+1)%3],v=s[(e+2)%3];
        row[e]=Edge(u,v,minx*16+8,miny*16+8);dx[e]=-(v.y-u.y)*16;dy[e]=(v.x-u.x)*16;
        if(dx[e])++oldInverses;
    }
    // Direct integer division is an independent reference for plane setup.
    int dzx=(int)((((Wide)dx[0]*s[0].z+(Wide)dx[1]*s[1].z+(Wide)dx[2]*s[2].z)*256)/area);
    int dzy=(int)((((Wide)dy[0]*s[0].z+(Wide)dy[1]*s[1].z+(Wide)dy[2]*s[2].z)*256)/area);
    int rowz=(int)((((Wide)row[0]*s[0].z+(Wide)row[1]*s[1].z+(Wide)row[2]*s[2].z)*256)/area);
    for(int y=miny;y<=maxy;++y){
        // Count precisely the old edge-bound divisions, independently of pixels.
        for(int e=0;e<3;++e){
            if(row[e]<0){if(dx[e]<=0)break;++oldFloors;}
            else if(dx[e]<0)++oldFloors;
        }
        int w0=row[0],w1=row[1],w2=row[2],z=rowz;
        for(int x=minx;x<=maxx;++x){
            bool hud=renderer.screenHud&&y>=SKY_HUD_TOP&&y<SKY_HUD_BOTTOM&&x>=SKY_HUD_LEFT&&x<SKY_HUD_RIGHT;
            if(!hud&&w0>=0&&w1>=0&&w2>=0){
                int d=(z>>8)+renderer.depthBias;
                if(d>expectedZ[y*renderer.width+x]){expectedZ[y*renderer.width+x]=(Pixel)d;expected[16+y*renderer.pitch+x]=color;}
            }
            w0+=dx[0];w1+=dx[1];w2+=dx[2];z+=dzx;
        }
        for(int e=0;e<3;++e)row[e]+=dy[e];rowz+=dzy;
    }
}
int main(){
    unsigned seed=660044;
    for(int mode=0;mode<2;++mode)for(int hud=0;hud<2;++hud)for(int sample=0;sample<2500;++sample){
        renderer.SetHalfResolution(mode!=0);renderer.screenHud=hud&&!mode;
        renderer.target=actual+16;renderer.pitch=renderer.width+sample%3;renderer.depthBias=(sample%3)*2;
        Screen s[3];
        for(int i=0;i<3;++i){
            seed=seed*1664525u+1013904223u;s[i].x=(int)(seed%(renderer.width*16+129))-64;
            seed=seed*1664525u+1013904223u;s[i].y=(int)(seed%(renderer.height*16+129))-64;
            s[i].z=5000;
        }
        if(sample%5==0){ // Single clipped row at the bottom, with nonzero area.
            s[0].y=renderer.height*16-16;s[1].y=renderer.height*16+16;s[2].y=renderer.height*16+32;
        }
        if(sample%7==0){ // Exact edge/sample boundaries and opposite winding.
            s[0].x=8;s[0].y=8;s[1].x=renderer.width*16-8;s[1].y=8;
            s[2].x=(sample&1)?8:renderer.width*8;s[2].y=renderer.height*16-8;
        }
        if(sample%11==0){ // Degenerate / zero-area triangle.
            s[1]=s[0];
        }
        if(sample%13==0){ // Defensive oversized-divisor fallback, still no edge overflow.
            s[0].x=0;s[0].y=-1600000;s[1].x=16;s[1].y=1600000;s[2].x=32;s[2].y=0;
        }
        int area=Edge(s[0],s[1],s[2].x,s[2].y);
        if(area>100000||area< -100000){s[1].z=4900;s[2].z=5100;}
        for(unsigned i=0;i<sizeof(actual)/sizeof(actual[0]);++i)actual[i]=expected[i]=0xA55A;
        for(int i=0;i<SKY_W*SKY_H;++i){seed=seed*1664525u+1013904223u;renderer.depth[i]=expectedZ[i]=(Pixel)(seed%6000);}
        Reference(s[0],s[1],s[2],12345);
        renderer.RasterScreen(s[0],s[1],s[2],12345);
        if(memcmp(actual,expected,sizeof(actual))||memcmp(renderer.depth,expectedZ,sizeof(expectedZ))){
            fprintf(stderr,"FAIL: raster oracle mismatch mode=%d hud=%d sample=%d\n",mode,hud,sample);return 1;
        }
    }
    Check(rasterSingleRows>0,"single-row optimization was exercised");
    Check(rasterEdgeFloors<oldFloors,"bound guards did not remove EdgeFloor calls");
    Check(rasterEdgeInverses<oldInverses,"lazy edge reciprocals did not remove work");
    printf("PASS: 10000 triangles match per-pixel color/Z oracle, both resolutions/HUD modes, clipped single rows, gradients, winding, degenerate/oversized edges and all guards.\n");
    printf("EdgeFloor %u -> %u; edge inverses %u -> %u; single-row triangles %u (operation counts, not CPU timing).\n",oldFloors,rasterEdgeFloors,oldInverses,rasterEdgeInverses,rasterSingleRows);
}
