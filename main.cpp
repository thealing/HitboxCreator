#include<windows.h>
#include<windowsx.h>
#include<wincodec.h>
#include<stdint.h>
#include<math.h>
#include<vector>
#include<string>

#define ID_OPEN 1
#define ID_COPY 2
#define ID_CLEAR 3
#define ID_UNDO 4

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define CENTER_RADIUS 7
#define CLICK_RADIUS 10
#define LINE_WIDTH 4

const HPEN redPen=CreatePen(PS_SOLID,LINE_WIDTH,RGB(255,0,0));
const HPEN greenPen=CreatePen(PS_SOLID,LINE_WIDTH,RGB(0,255,0));
const HPEN bluePen=CreatePen(PS_SOLID,LINE_WIDTH,RGB(0,0,255));
const HPEN cyanPen=CreatePen(PS_SOLID,LINE_WIDTH,RGB(0,255,255));
const HBRUSH blueBrush=CreateSolidBrush(RGB(0,0,255));

HWND window;
HDC windowDC;
HDC renderDC;
HBITMAP renderBitmap;
int imageWidth;
int imageHeight;
uint32_t*imagePixels;
BITMAPINFO bmi;
int hasImage;
int windowWidth;
int windowHeight;
int renderWidth;
int renderHeight;
POINT mouse;
POINT center;
std::vector<std::vector<POINT>>polygons;
std::vector<POINT>currentPoints;

int square(int x){
	return x*x;
}

int distanceSquared(POINT point1,POINT point2){
	return square(point1.x-point2.x)+square(point1.y-point2.y);
}

void openImage(){
	if(hasImage){
		DeleteDC(renderDC);
		DeleteObject(renderBitmap);
		delete[]imagePixels;
		hasImage=0;
	}
	OPENFILENAME ofn{};
	ofn.lStructSize=sizeof(OPENFILENAME);
	ofn.hwndOwner=window;
	ofn.lpstrFilter="Image Files\0*.png\0\0";
	char path[MAX_PATH]="\0";
	ofn.lpstrFile=path;
	ofn.nMaxFile=MAX_PATH;
	ofn.Flags=OFN_FILEMUSTEXIST|OFN_EXPLORER;
	if(!GetOpenFileName(&ofn)){
		return;
	}
	wchar_t widePath[MAX_PATH];
	mbstowcs(widePath,path,MAX_PATH);
	CoInitialize(NULL);
	IWICImagingFactory*factory;
	CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_IWICImagingFactory,(void**)&factory);
	IWICBitmapDecoder*decoder;
	factory->CreateDecoderFromFilename(widePath,nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
	IWICBitmapFrameDecode*frame;
	decoder->GetFrame(0,&frame);
	IWICBitmapSource*bitmap;
	WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA,(IWICBitmapSource*)frame,&bitmap);
	bitmap->GetSize((UINT*)&imageWidth,(UINT*)&imageHeight);
	imagePixels=new uint32_t[imageWidth*imageHeight];
	bitmap->CopyPixels(nullptr,imageWidth*sizeof(uint32_t),imageWidth*imageHeight*sizeof(uint32_t),(BYTE*)imagePixels);
	for(int i=0;i<imageWidth*imageHeight;i++){
		char*p=(char*)(imagePixels+i);
		if (p[3]==0){
			p[0]=255;
			p[1]=0;
			p[2]=255;
		}
	}
	bitmap->Release();
	frame->Release();
	decoder->Release();
	factory->Release();
	CoUninitialize();
	if((double)imageWidth/imageHeight>(double)renderWidth/renderHeight){
		renderWidth=WINDOW_WIDTH;
		renderHeight=WINDOW_WIDTH*imageHeight/imageWidth;
	}
	else{
		renderWidth=WINDOW_HEIGHT*imageWidth/imageHeight;
		renderHeight=WINDOW_HEIGHT;
	}
	SetWindowPos(window,nullptr,0,0,renderWidth,renderHeight,SWP_NOMOVE);
	renderDC=CreateCompatibleDC(windowDC);
	renderBitmap=CreateCompatibleBitmap(windowDC,renderWidth,renderHeight);
	SelectObject(renderDC,renderBitmap);
	bmi.bmiHeader.biSize=sizeof(BITMAPINFO);
	bmi.bmiHeader.biWidth=imageWidth;
	bmi.bmiHeader.biHeight=imageHeight;
	bmi.bmiHeader.biPlanes=1;
	bmi.bmiHeader.biBitCount=32;
	bmi.bmiHeader.biCompression=BI_RGB;
	hasImage=1;
}

LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
	switch(message){
		case WM_SIZE:{
			windowWidth=LOWORD(lParam);
			windowHeight=HIWORD(lParam);
			break;
		}
		case WM_MOUSEMOVE:{
			mouse.x=GET_X_LPARAM(lParam)*renderWidth/windowWidth;
			mouse.y=renderHeight-GET_Y_LPARAM(lParam)*renderHeight/windowHeight;
			break;
		}
		case WM_LBUTTONUP:{
			if(!currentPoints.empty()&&distanceSquared(currentPoints.front(),mouse)<square(CLICK_RADIUS)){
//				currentPoints.push_back(currentPoints.front());
				polygons.emplace_back(std::move(currentPoints));
			}
			else{
				currentPoints.push_back(mouse);
			}
			break;
		}
		case WM_RBUTTONUP:{
			if(wParam&MK_SHIFT){
				center.x=lround(mouse.x/(double)(renderWidth/2))*renderWidth/2;
				center.y=lround(mouse.y/(double)(renderHeight/2))*renderHeight/2;
			}
			else{
				center.x=mouse.x;
				center.y=mouse.y;
			}
			break;
		}
		case WM_COMMAND:{
			switch(LOWORD(wParam)){
				case ID_OPEN:{
					openImage();
					break;
				}
				case ID_COPY:{
					std::string text;
					text+='{';
					for(const auto&points:polygons){
						text+='{';
						for(const auto&point:points){
							int x=(point.x-center.x)*imageWidth/renderWidth;
							int y=(point.y-center.y)*imageHeight/renderHeight;
							text+='{'+std::to_string(x)+','+std::to_string(y)+(&point==&points.back()?"}":"},");
						}
						text+=&points==&polygons.back()?"}":"},";
					}
					text+='}';
					OpenClipboard(window);
					EmptyClipboard();
					HGLOBAL globalMemory=GlobalAlloc(GMEM_MOVEABLE,text.size()+1);
					LPVOID globalString=GlobalLock(globalMemory);
					memcpy(globalString,text.c_str(),text.size()+1);
					SetClipboardData(CF_TEXT,globalMemory);
					CloseClipboard();
					break;
				}
				case ID_CLEAR:{
					polygons.clear();
					currentPoints.clear();
					break;
				}
				case ID_UNDO:{
					if(!currentPoints.empty()){
						currentPoints.pop_back();
					}
					else if(!polygons.empty()){
						polygons.pop_back();
					}
					break;
				}
			}
			break;
		}
	}
	return DefWindowProc(window,message,wParam,lParam);
}

int main(){
	window=CreateWindow("STATIC","Hitbox Creator",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,nullptr,nullptr,nullptr,nullptr);
	SetWindowLongPtr(window,GWLP_WNDPROC,(LONG_PTR)windowProc);
	HMENU menu=CreateMenu();
	AppendMenu(menu,MF_STRING,ID_OPEN,"Open");
	AppendMenu(menu,MF_STRING,ID_COPY,"Copy");
	AppendMenu(menu,MF_STRING,ID_CLEAR,"Clear");
	AppendMenu(menu,MF_STRING,ID_UNDO,"Undo");
	SetMenu(window,menu);
	windowDC=GetDC(window);
	openImage();
	while(IsWindow(window)){
		MSG msg;
		while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)>0){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if(hasImage){
			StretchDIBits(renderDC,0,0,renderWidth,renderHeight,0,0,imageWidth,imageHeight,imagePixels,&bmi,DIB_RGB_COLORS,SRCCOPY);
			SelectObject(renderDC,bluePen);
			SelectObject(renderDC,blueBrush);
			Ellipse(renderDC,center.x-CENTER_RADIUS,center.y-CENTER_RADIUS,center.x+CENTER_RADIUS,center.y+CENTER_RADIUS);
			SelectObject(renderDC,greenPen);
			for(const auto&points:polygons){
				for(size_t i1=points.size()-1,i2=0;i2<points.size();i1=i2,i2++){
					MoveToEx(renderDC,points[i1].x,points[i1].y,nullptr);
					LineTo(renderDC,points[i2].x,points[i2].y);
				}
			}
			SelectObject(renderDC,redPen);
			for(size_t i=0;i+1<currentPoints.size();i++){
				MoveToEx(renderDC,currentPoints[i].x,currentPoints[i].y,nullptr);
				LineTo(renderDC,currentPoints[i+1].x,currentPoints[i+1].y);
			}
			if(!currentPoints.empty()&&GetKeyState(VK_LBUTTON)<0){
				SelectObject(renderDC,distanceSquared(currentPoints.front(),mouse)<square(CLICK_RADIUS)?redPen:cyanPen);
				MoveToEx(renderDC,currentPoints.back().x,currentPoints.back().y,nullptr);
				LineTo(renderDC,mouse.x,mouse.y);
			}
			StretchBlt(windowDC,0,windowHeight,windowWidth,-windowHeight,renderDC,0,0,renderWidth,renderHeight,SRCCOPY);
		}
		else{
			RECT renderRect{0,0,windowWidth,windowHeight};
			FillRect(windowDC,&renderRect,(HBRUSH)GetStockObject(BLACK_BRUSH));
		}
		Sleep(1);
	}
	return 0;
}
