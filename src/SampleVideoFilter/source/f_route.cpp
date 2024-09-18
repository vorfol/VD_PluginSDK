// /s c:\rgmapvideo\debug.jobs
#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <stdio.h>
#include <time.h>

#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <regex>

#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>
#include "../resource.h"
#include "gdiplusheaders.h"

#include "pugixml.hpp"

#define _USE_MATH_DEFINES
#include "math.h"

using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")

extern int g_VFVAPIVersion;

class GDIToken
{
protected:
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR           gdiplusToken;
public:
    GDIToken()
    {
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    }
    ~GDIToken()
    {
        //GdiplusShutdown(gdiplusToken);
    }
};

///////////////////////////////////////////////////////////////////////////////
#pragma region RouteFilterConfig

enum PaneType {
    Image,
    Leg,
    Pos,
    Text
};

enum TextAlignment {
    Center,
    Left,
    Right
};

enum TextType {
    Comment,
    Pace,
    Time,
    HR
};

struct BasePane {
public:
    PaneType    Type;
    std::string Name;
    int         X;
    int         Y;
    int         W;
    int         H;
    int         Opaque;     // 0-100
    time_t      Start;      // from video
    time_t      End;        // from video, must be > Start, else infinite
};


struct TextPane : public BasePane {
public:
    Gdiplus::Color  FillColor;
    Gdiplus::Color  FontColor;
    int             FontSize;
    std::string     FontName;
    TextAlignment   Align;
    TextType        TextType;
    std::string     Value;  // Text for comment type, XML path for other types
};

struct ImagePane : public BasePane {
public:
    std::string     ImageName;      // name
};

struct RoutePane : public ImagePane {
public:
    std::string     PathName;       // name
    int             Margins;
    time_t          Tail;
    int             TailWidth;
    Gdiplus::Color  TailColor;
    std::string     Pointer;        // name
    int             PointerOpaque;  // 0-100
};

class RouteFilterConfig 
{
public:
	RouteFilterConfig()	{
	}

    bool            FromXml(pugi::xml_document &doc);

public:

	std::string		                    m_File;         //save/load config - do not stored in config string

	std::map<std::string, std::string>  m_Images;       // [name] = image path
	
    std::map<std::string, std::string>  m_Paths;        // [name] = xml path

    std::map<std::string, BasePane*>   m_Panes;        // [name] = pane

    time_t                              m_VideoStart;   // start of the video in GMT

};

std::istream& operator >> (std::istream& i, Gdiplus::Color& c) {
	unsigned long v;
	i >> std::hex >> v;
	c.SetValue(v);
	return i;
}

template<class T>
T fromString(const std::string& s) {
	T t;
	std::istringstream i(s);
	i >> t;
	return t;
}

template<>
std::string fromString(const std::string& s) {
	return s;
}

template<>
time_t fromString(const std::string& s) {
    time_t ret = 0;
    struct tm T;
    memset(&T, 0, sizeof(T));
    // GMT:
    //      "YYYY-MM-DDThh:mm:ssZ"
    if (6 == sscanf(s.c_str(), "%d-%d-%dT%d:%d:%dZ", 
            &T.tm_year,
            &T.tm_mon,
            &T.tm_mday,
            &T.tm_hour,
            &T.tm_min,
            &T.tm_sec))
    {
        T.tm_year -= 1900;
        T.tm_mon -= 1;
        ret = mktime(&T);
        if (ret == -1) {
            ret = 0;
        }
    } else {
    // Duration:
    //      "mm:ss" 
    //      "hh:mm:ss"
        switch (sscanf(s.c_str(), "%d:%d:%d", &T.tm_hour, &T.tm_min, &T.tm_sec)) {
            default:
                ret = 0;
                break;
            case 3:
                ret = T.tm_hour * 3600 + T.tm_min * 60 + T.tm_sec;
                break;
            case 2:
                ret = T.tm_hour * 60 + T.tm_min;
                break;
            case 1: 
                ret = T.tm_hour;
                break;
        }
    }
    return ret;
}

#define CapsCharToVal(x) (*(x)>'9'?*(x)-'A'+10:*(x)-'0')
#define CapsCharToVal2(x) (CapsCharToVal(x+1)|(CapsCharToVal(x)<<4))

Gdiplus::Color ColorFromStr(const char* str) {
    if (str == NULL || *str == '0') return Gdiplus::Color(0,0,0,0);
    BYTE a = CapsCharToVal2(str);
    BYTE r = CapsCharToVal2(str+2);
    BYTE g = CapsCharToVal2(str+4);
    BYTE b = CapsCharToVal2(str+6);
    return Gdiplus::Color(a,r,g,b);
}

template<>
Gdiplus::Color fromString(const std::string& s) {
    return ColorFromStr(s.c_str());
}

BasePane* FillBasePane(BasePane *pPane, pugi::xml_node &node) {
    if (pPane != nullptr) {
        pPane->Name = node.attribute("name").as_string();
        pPane->X = node.attribute("x").as_int();
        pPane->Y = node.attribute("y").as_int();
        pPane->W = node.attribute("w").as_int();
        pPane->H = node.attribute("h").as_int();
        pPane->Opaque = 100;
        if (!node.attribute("opaq").empty()) {
            pPane->Opaque = node.attribute("opaq").as_int();
        }
        pPane->Start = 0;
        pugi::xml_node start_node = node.child("Start");
        if (!start_node.empty()) {
            pPane->Start = fromString<time_t>(start_node.child_value());
        }
        pPane->End = 0;
        pugi::xml_node end_node = node.child("End");
        if (!end_node.empty()) {
            pPane->End = fromString<time_t>(end_node.child_value());
        } else {
            pugi::xml_node duration_node = node.child("Duration");
            if (!duration_node.empty()) {
                pPane->End = pPane->Start + fromString<time_t>(duration_node.child_value());
            }
        }
    }
    return pPane;
}

BasePane* FillTextPane(TextPane *pPane, pugi::xml_node &node) {
    if (pPane == nullptr) {
        pPane = new TextPane();
        pPane->Type = PaneType::Text;
    }

    pPane->FillColor = Gdiplus::Color(0x80,0x50,0x50,0x50);
    pugi::xml_node fill_color_node = node.child("FillColor");
    if (!fill_color_node.empty()) {
        pPane->FillColor = fromString<Gdiplus::Color>(fill_color_node.child_value());
    }

    pPane->FontColor = Gdiplus::Color(0xff,0xff,0xff,0xff);
    pugi::xml_node font_color_node = node.child("FontColor");
    if (!font_color_node.empty()) {
        pPane->FontColor = fromString<Gdiplus::Color>(font_color_node.child_value());
    }

    pPane->FontSize = 16;
    pugi::xml_node font_size_node = node.child("FontSize");
    if (!font_size_node.empty()) {
        pPane->FontSize = fromString<int>(font_size_node.child_value());
    }

    pPane->FontName = "Arial";
    pugi::xml_node font_name_node = node.child("FontName");
    if (!font_name_node.empty()) {
        pPane->FontName = font_name_node.child_value();
    }

    pPane->Align = TextAlignment::Left;
    pugi::xml_node align_node = node.child("Align");
    if (!align_node.empty()) {
        std::string align_str = align_node.child_value();
        if (align_str == "Center") {
            pPane->Align = TextAlignment::Center;
        } else if (align_str == "Right") {
            pPane->Align = TextAlignment::Right;
        }
    }

    pPane->TextType = TextType::Comment;
    pugi::xml_node text_type_node = node.child("TextType");
    if (!text_type_node.empty()) {
        std::string text_type_str = text_type_node.child_value();
        if (text_type_str == "Time") {
            pPane->TextType = TextType::Time;
        } else if (text_type_str == "Pace") {
            pPane->TextType = TextType::Pace;
        } else if (text_type_str == "HR") {
            pPane->TextType = TextType::HR;
        }
    }
    
    // Text for comment type, XML path for other types
    pPane->Value = "";
    pugi::xml_node value_node = node.child("Value");
    if (!value_node.empty()) {
        pPane->Value = value_node.child_value();
    }

    return FillBasePane(pPane, node);
}

BasePane* FillImagePane(ImagePane *pPane, pugi::xml_node &node) {
    if (pPane == nullptr) {
        pPane = new ImagePane();
        pPane->Type = PaneType::Image;
    }

    pPane->ImageName = "";
    pugi::xml_node image_node = node.child("ImageName");
    if (!image_node.empty()) {
        pPane->ImageName = image_node.child_value();
    }

    return FillBasePane(pPane, node);
}

BasePane* FillRoutePane(RoutePane *pPane, pugi::xml_node &node) {
    if (pPane == nullptr) {
        pPane = new RoutePane();
        pPane->Type = PaneType::Leg;
    }

    pPane->PathName = "";
    pugi::xml_node path_node = node.child("PathName");
    if (!path_node.empty()) {
        pPane->PathName = path_node.child_value();
    }

    pPane->Margins = 30;
    pugi::xml_node margins_node = node.child("Margins");
    if (!margins_node.empty()) {
        pPane->Margins = fromString<int>(margins_node.child_value());
    }

    pPane->Tail = 30;
    pugi::xml_node tail_node = node.child("Tail");
    if (!tail_node.empty()) {
        pPane->Tail = fromString<time_t>(tail_node.child_value());
    }

    pPane->TailWidth = 8;
    pugi::xml_node width_node = node.child("TailWidth");
    if (!width_node.empty()) {
        pPane->TailWidth = fromString<int>(width_node.child_value());
    }

    pPane->TailColor = Gdiplus::Color(0x60,0xff,0,0);
    pugi::xml_node color_node = node.child("TailColor");
    if (!color_node.empty()) {
        pPane->TailColor = fromString<Gdiplus::Color>(color_node.child_value());
    }

    pPane->Pointer = "";
    pugi::xml_node pointer_node = node.child("Pointer");
    if (!pointer_node.empty()) {
        pPane->Pointer = pointer_node.child_value();
    }

    pPane->PointerOpaque = 50;
    pugi::xml_node opaque_node = node.child("PointerOpaque");
    if (!opaque_node.empty()) {
        pPane->PointerOpaque = fromString<int>(opaque_node.child_value());
    }

    return FillImagePane(pPane, node);
}

bool RouteFilterConfig::FromXml(pugi::xml_document &doc)
{
    pugi::xml_node settings = doc.child("RouteAddSettings");

    pugi::xml_node video_node = settings.child("Video");
    m_VideoStart = fromString<time_t>(video_node.attribute("start").as_string());

    pugi::xml_node images_node = settings.child("Images");
	m_Images.clear();
	pugi::xml_node image_node = images_node.child("Image");
	while (!image_node.empty())	{
        m_Images[image_node.attribute("name").as_string()] = image_node.child_value();
		image_node = image_node.next_sibling();
	}

    pugi::xml_node paths_node = settings.child("Paths");
	m_Paths.clear();
	pugi::xml_node path_node = paths_node.child("Path");
	while (!path_node.empty())	{
        m_Paths[path_node.attribute("name").as_string()] = path_node.child_value();
		path_node = path_node.next_sibling();
	}

    pugi::xml_node panes_node = settings.child("Panes");
	m_Panes.clear();
	pugi::xml_node pane_node = panes_node.child("Pane");
	while (!pane_node.empty())	{
        BasePane *pPane = nullptr;
        std::string type_name = std::string(pane_node.attribute("type").as_string());
        if (type_name == "leg") {
            pPane = FillRoutePane(nullptr, pane_node);
            pPane->Type = PaneType::Leg;
        } else if (type_name == "pos") {
            pPane = FillRoutePane(nullptr, pane_node);
            pPane->Type = PaneType::Pos;
        } else if (type_name == "image") {
            pPane = FillImagePane(nullptr, pane_node);
        } else if (type_name == "text") {
            pPane = FillTextPane(nullptr, pane_node);
        }
        if (pPane != nullptr) {
            m_Panes[pPane->Name] = pPane;
        }
		pane_node = pane_node.next_sibling();
	}

	return true;
}

#pragma endregion RouteFilterConfig


#pragma region ConfigDialog
///////////////////////////////////////////////////////////////////////////////


class RouteFilterDialog : public VDXVideoFilterDialog {
public:
	RouteFilterDialog(RouteFilterConfig& config, IVDXFilterPreview *ifp) : m_Config(config)  {
        m_OldConfig = m_Config;
    }

	bool Show(HWND parent) {
		return 0 != VDXVideoFilterDialog::Show(NULL, MAKEINTRESOURCE(IDD_FILTER_ROUTE_DLG), parent);
	}

	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

protected:

	bool OnInit();
	bool OnCommand(int cmd);
	void OnDestroy();

    bool DoBrowse(const char* filter, std::string& file);

protected:

	RouteFilterConfig& m_Config;
	RouteFilterConfig m_OldConfig;
};

INT_PTR RouteFilterDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			return OnInit();

		case WM_DESTROY:
			OnDestroy();
			break;

		case WM_COMMAND:
			if (OnCommand(LOWORD(wParam))) {
				return TRUE;
            }
			break;
	}

	return FALSE;
}

bool RouteFilterDialog::OnInit() {
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_file(m_Config.m_File.c_str());
    if (res) {
        m_Config.FromXml(doc);
        SetDlgItemText(mhdlg, IDC_FILE, m_Config.m_File.c_str());
        EnableWindow(GetDlgItem(mhdlg, IDOK), TRUE);
    } else {
        m_Config = m_OldConfig;
        SetDlgItemText(mhdlg, IDC_FILE, m_Config.m_File.c_str());
        EnableWindow(GetDlgItem(mhdlg, IDOK), FALSE);
    }

	return true;
}

bool RouteFilterDialog::DoBrowse(const char* filter, std::string &file) {
	OPENFILENAME ofn; 
	char file_buf[512];
	strcpy_s(file_buf, file.c_str());
    // Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = mhdlg;
	ofn.lpstrFile = file_buf;
	ofn.nMaxFile = _countof(file_buf);
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	// Display the Open dialog box. 

	if (GetOpenFileName(&ofn) != FALSE) {
		file = file_buf;
		return true;
	}
	return false;
}


void RouteFilterDialog::OnDestroy() {
}

bool RouteFilterDialog::OnCommand(int cmd) {
	switch(cmd) {
		case IDOK:
			EndDialog(mhdlg, TRUE);
			return true;

		case IDCANCEL:
			m_Config = m_OldConfig;
			EndDialog(mhdlg, FALSE);
			return true;

		case IDC_BROWSE:
            if (DoBrowse("All\0*.*\0XML\0*.xml\0", m_Config.m_File)) {
                OnInit();
			}
			return true;
    }

	return false;
}

#pragma endregion ConfigDialog

///////////////////////////////////////////////////////////////////////////////

struct PathState {
public:
    pugi::xml_document             *pPath;
    time_t                          startTime;
    pugi::xml_node                  currentSample;
    int                             currentLap;
    int                             legPosition;
    Gdiplus::Matrix                *pLegMatrix; // without scale
    std::vector<Gdiplus::PointF>    legPoints;
    std::vector<int>                legTimes;
    double                          vectorSize;
    Gdiplus::REAL                   left;
    Gdiplus::REAL                   top;
    Gdiplus::REAL                   right;
    Gdiplus::REAL                   bottom;
    Gdiplus::PointF                 legCenter;
};

struct RoutePaneState {
public:
    int                             currentLap;
    std::vector<Gdiplus::PointF>    legPoints;
    Gdiplus::Matrix                *pLegMatrix; // scaled by pane view
};

class RouteFilter : public VDXVideoFilter {
public:
    RouteFilter() {
        ZeroVars();
    }

	RouteFilter(const RouteFilter &f) {
		ZeroVars();
		m_Config = f.m_Config;
	}

	RouteFilter& operator = (const RouteFilter &f) {
		ZeroVars();
		m_Config = f.m_Config;
		return *this;
	}

	virtual ~RouteFilter();

	virtual uint32 GetParams();
    virtual bool Init();
	virtual void Start();
	virtual void Run();
	virtual void End();
	virtual bool Configure(VDXHWND hwnd);
	virtual void GetSettingString(char *buf, int maxlen);
	virtual void GetScriptString(char *buf, int maxlen);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:

    GDIToken    m_GDIToken;

    virtual void ZeroVars();    //pointers only
    virtual void DeleteVars();
    virtual void CreateVars();  //from m_Config!

    Gdiplus::Bitmap *PrepareRGB32(void* data, uint32 pitch, uint32 w, uint32 h);
	void DrawRoute(Gdiplus::Bitmap *bmp, uint32 ms);
    void ApplyRGB32(Gdiplus::Bitmap *bmp, void* data, uint32 pitch, uint32 w, uint32 h);

	void ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc);

	RouteFilterConfig	                        m_Config;

    std::map<std::string, Gdiplus::Bitmap*>     m_Images;
	std::map<std::string, PathState>            m_PathStates;
	std::map<std::string, RoutePaneState>       m_RoutePaneStates;

    Gdiplus::Bitmap*                            m_pLastBmp;

    bool                                        m_Initialized;
};

VDXVF_BEGIN_SCRIPT_METHODS(RouteFilter)
VDXVF_DEFINE_SCRIPT_METHOD(RouteFilter, ScriptConfig, "s")
VDXVF_END_SCRIPT_METHODS()

void RouteFilter::ZeroVars()
{
    m_pLastBmp = NULL;

    m_Initialized = false;
}

void RouteFilter::DeleteVars()
{
    delete m_pLastBmp;

    for (auto it = m_Images.begin(); it != m_Images.end(); ++it) {
        delete it->second;
    }
    m_Images.clear();

    for (auto it = m_PathStates.begin(); it != m_PathStates.end(); ++it) {
        delete it->second.pLegMatrix;
        delete it->second.pPath;
    }
    m_PathStates.clear();

    for (auto it = m_RoutePaneStates.begin(); it != m_RoutePaneStates.end(); ++it) {
        delete it->second.pLegMatrix;
    }
    m_RoutePaneStates.clear();

    ZeroVars();
}

uint32 RouteFilter::GetParams() {
	if (g_VFVAPIVersion >= 12) {
		switch(fa->src.mpPixmapLayout->format) {
			case nsVDXPixmap::kPixFormat_XRGB8888:
				break;

			default:
				return FILTERPARAM_NOT_SUPPORTED;
		}
	}

	fa->dst.offset = fa->src.offset;
	return FILTERPARAM_SUPPORTS_ALTFORMATS;
}



RouteFilter::~RouteFilter() {
    DeleteVars();
}

bool RouteFilter::Init() {
    bool ret = VDXVideoFilter::Init();

    DeleteVars();

    return ret;
}

void RouteFilter::CreateVars() {
    if (!m_Config.m_File.empty()) {
        //load images
        for (auto it = m_Config.m_Images.begin(); it != m_Config.m_Images.end(); ++it) {
            m_Images[it->first] = Gdiplus::Bitmap::FromFile(pugi::as_wide(it->second).c_str());
        }
        //load paths
        for (auto it = m_Config.m_Paths.begin(); it != m_Config.m_Paths.end(); ++it) {
            PathState P;
            P.pLegMatrix = nullptr;
            P.pPath = new pugi::xml_document();
            if (P.pPath->load_file(it->second.c_str())) {
                P.currentSample = P.pPath->child("Route").child("Segment").first_child();
                P.startTime = fromString<time_t>(P.currentSample.attribute("time").as_string());
                m_PathStates[it->first] = P;
            }
        }
        //init pane states
        for (auto it = m_Config.m_Panes.begin(); it != m_Config.m_Panes.end(); ++it) {
            RoutePaneState P;
            P.pLegMatrix = nullptr;
            m_RoutePaneStates[it->first] = P;
        }
    }
    m_Initialized = TRUE;
}

void RouteFilter::Start() {
    if (!m_Initialized) {
        CreateVars();
    }
}

void RouteFilter::End() {
    DeleteVars();
}

void RouteFilter::Run() {
    if (!m_Initialized) 
        return;

    if (g_VFVAPIVersion >= 12) {
		const VDXPixmap& pxdst = *fa->dst.mpPixmap;
		const VDXPixmap& pxsrc = *fa->src.mpPixmap;

		switch(pxdst.format) {
			case nsVDXPixmap::kPixFormat_XRGB8888:
                Gdiplus::Bitmap *pbmp = PrepareRGB32(pxdst.data, pxdst.pitch, pxdst.w, pxdst.h);
                DrawRoute(pbmp, fa->pfsi->lSourceFrameMS);
                ApplyRGB32(pbmp, pxdst.data, pxdst.pitch, pxdst.w, pxdst.h);
                delete pbmp;
				break;
		}
	} else {
        Gdiplus::Bitmap *pbmp = PrepareRGB32(fa->dst.data, fa->dst.pitch, fa->dst.w, fa->dst.h);
        DrawRoute(pbmp, fa->pfsi->lSourceFrameMS);
        ApplyRGB32(pbmp, fa->dst.data, fa->dst.pitch, fa->dst.w, fa->dst.h);
        delete pbmp;
	}
}

Gdiplus::Bitmap* RouteFilter::PrepareRGB32(void *data, uint32 pitch, uint32 w, uint32 h) {
    //create bitmap on frame!
    Gdiplus::Bitmap *pbmp = new Gdiplus::Bitmap(w, h, pitch, PixelFormat32bppRGB, (BYTE*)data);
    return pbmp;

}

void RouteFilter::ApplyRGB32(Gdiplus::Bitmap *pbmp, void *data, uint32 pitch, uint32 w, uint32 h) {
    //do nothing
}

bool RouteFilter::Configure(VDXHWND hwnd) {
    RouteFilterDialog dlg(m_Config, fa->ifp);

	bool ret = dlg.Show((HWND)hwnd);

    if (ret) {
        DeleteVars();
        CreateVars();
    }
    return ret;
}

void RouteFilter::DrawRoute(Gdiplus::Bitmap *pbmp, uint32 ms) {
    if (pbmp == nullptr) {
        FILE* pFile = fopen("log.txt", "a");
        fprintf(pFile, "No image to draw at %i ms\n", ms);
        fclose(pFile);
        return;
    }
    // Test if bitmap size changed
    if (m_pLastBmp && (m_pLastBmp->GetWidth() != pbmp->GetWidth() || m_pLastBmp->GetHeight() != pbmp->GetHeight())) {
        delete m_pLastBmp;
        m_pLastBmp = nullptr;
    }

    bool refreshImages = (m_pLastBmp == nullptr);

    // Recalculate paths
    for (auto it = m_PathStates.begin(); it != m_PathStates.end(); ++it) {
        PathState &PathState = it->second;
        int64 time_run = ms + 1000 * (m_Config.m_VideoStart - PathState.startTime);
        if (time_run < 0) {
            time_run = 0;
        }
        pugi::xml_node last_sample = PathState.currentSample;
        if (PathState.currentSample.empty()) {
            PathState.currentSample = PathState.pPath->child("Route").child("Segment").first_child();
        }
        //go forward
        int elapsed_time = PathState.currentSample.attribute("elapsedTime").as_int();
        while(time_run > elapsed_time*1000) {
            PathState.currentSample = PathState.currentSample.next_sibling();
            if (PathState.currentSample.empty()) {
                //end of path reached
                PathState.currentSample = PathState.pPath->child("Route").child("Segment").last_child();
                break;
            }
            elapsed_time = PathState.currentSample.attribute("elapsedTime").as_int();
        }
        //go to previous sample
        if (!PathState.currentSample.previous_sibling().empty()) {
            PathState.currentSample = PathState.currentSample.previous_sibling();
            elapsed_time = PathState.currentSample.attribute("elapsedTime").as_int();
        }
        //go backward
        while(time_run < elapsed_time*1000) {
            PathState.currentSample = PathState.currentSample.previous_sibling();
            if (PathState.currentSample.empty()) {
                //begin of path reaches
                PathState.currentSample = PathState.pPath->child("Route").child("Segment").first_child();;
                break;
            }
            elapsed_time = PathState.currentSample.attribute("elapsedTime").as_int();
        }
    
        refreshImages |= (PathState.currentSample != last_sample);

        if (PathState.currentLap != PathState.currentSample.attribute("lapNumber").as_int()) {
            //rebuild leg data
            PathState.currentLap = PathState.currentSample.attribute("lapNumber").as_int();

            //1. collect all image positions
            PathState.legPoints.clear();
            PathState.legTimes.clear();

            pugi::xml_node sample = PathState.currentSample;
            //find start of leg, remember leg position
            PathState.legPosition = 0;
            while(!sample.previous_sibling().empty() && sample.previous_sibling().attribute("lapNumber").as_int() == PathState.currentLap) {
                sample = sample.previous_sibling();
                ++PathState.legPosition;
            }
            //add all leg points to array
            while(!sample.empty() && sample.attribute("lapNumber").as_int() == PathState.currentLap) {
                PathState.legPoints.push_back(Gdiplus::PointF(sample.attribute("imageX").as_int(), sample.attribute("imageY").as_int()));
                PathState.legTimes.push_back(sample.attribute("elapsedTime").as_int());
                sample = sample.next_sibling();
            }

            //2. build matrix
            int samples = PathState.legPoints.size();
            if (samples > 1)  {
                Gdiplus::PointF start(PathState.legPoints[0].X, PathState.legPoints[0].Y);
                Gdiplus::PointF end(PathState.legPoints[samples-1].X, PathState.legPoints[samples-1].Y);
                Gdiplus::PointF vector(end.X - start.X, end.Y - start.Y);
            
                if (fabs(vector.X) > 1 || fabs(vector.Y) > 1) {
					PathState.legCenter = Gdiplus::PointF((end.X + start.X) / 2, (end.Y + start.Y) / 2);

					PathState.vectorSize = sqrt(vector.X*vector.X + vector.Y*vector.Y);
                    if (PathState.vectorSize < 1) {
                        PathState.vectorSize = 1;
                    }

					double angle = atan2((double)(vector.Y), (double)(vector.X));
                    double leg_direction = 90.0+180.0*(angle)/M_PI;

                    delete PathState.pLegMatrix;
                    PathState.pLegMatrix = new Gdiplus::Matrix();
                    PathState.pLegMatrix->RotateAt(-leg_direction, PathState.legCenter);

                    //rotate test points
                    std::vector<Gdiplus::PointF> testArr = PathState.legPoints;
                    PathState.pLegMatrix->TransformPoints(testArr.data(), samples);

                    //find bounds
                    PathState.left = testArr[0].X - PathState.legCenter.X;
                    PathState.top = testArr[0].Y - PathState.legCenter.Y;
                    PathState.right = PathState.left;
                    PathState.bottom = PathState.top;
                    for(int i = 0; i < samples; i++) {
                        PathState.left = min(PathState.left, testArr[i].X - PathState.legCenter.X);
                        PathState.top = min(PathState.top, testArr[i].Y - PathState.legCenter.Y);
                        PathState.right = max(PathState.right, testArr[i].X - PathState.legCenter.X);
                        PathState.bottom = max(PathState.bottom, testArr[i].Y - PathState.legCenter.Y);
                    }
                }
            }
        } else {
            // Update position in leg points
            if (!last_sample.empty() && last_sample.next_sibling() == PathState.currentSample) {
                ++PathState.legPosition;
            } else {
                //find leg position
                PathState.legPosition = 0;
                for(int i = 0; i < PathState.legPoints.size() && PathState.legTimes[i] < elapsed_time; ++i, PathState.legPosition++){
                    //TODO: binary search;
                }
            }
        }
    }

    // Refresh image
    if (refreshImages) {
        // Create new if needed
        if (!m_pLastBmp) {
            m_pLastBmp = new Gdiplus::Bitmap(pbmp->GetWidth(), pbmp->GetHeight(), PixelFormat32bppARGB);
        }

        Gdiplus::Status status;

        //draw into bitmap
        Gdiplus::Graphics graphics(m_pLastBmp);
        Color clear_color(0,0,0,0);
        graphics.Clear(clear_color);
        for (auto it = m_Config.m_Panes.begin(); it != m_Config.m_Panes.end(); ++it) {
            BasePane *pPane = it->second;
            if (pPane->Opaque > 0 &&
                1000 * pPane->Start <= ms && 
                (pPane->End <= pPane->Start || 1000 * pPane->End >= ms))
            {
                if (pPane->Type == PaneType::Image) {
                    Gdiplus::Bitmap *pImage = m_Images[((ImagePane*)pPane)->ImageName];
                    if (pImage != nullptr) {
                        Gdiplus::ImageAttributes ImgAttr;
                        Gdiplus::ColorMatrix ClrMatrix = { 
                                1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, pPane->Opaque/(100.0f), 0.0f,
                                0.0f, 0.0f, 0.0f, 0.0f, 1.0f
                        };
                        ImgAttr.SetColorMatrix(&ClrMatrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);
                        Gdiplus::Rect    destination(pPane->X, pPane->Y, pPane->W, pPane->H);
                        status = graphics.DrawImage(pImage, destination, 0, 0, pImage->GetWidth(), pImage->GetHeight(), Gdiplus::UnitPixel, &ImgAttr);
                    }
                } else if (pPane->Type == PaneType::Pos) {
                    //draw current position
                    RoutePane* pRoutePane = (RoutePane*)pPane;
                    PathState &PathState = m_PathStates[pRoutePane->PathName];
                    if (&PathState == nullptr) {
                        continue;   // to the next pane
                    }

                    int32 image_x = PathState.currentSample.attribute("imageX").as_int();
                    int32 image_y = PathState.currentSample.attribute("imageY").as_int();

                    double head_direction = PathState.currentSample.attribute("direction").as_double();

                    // Next transformations are both for the image and the tail
                    Gdiplus::GraphicsState state = graphics.Save();

                    //TODO: clip by region
                    Gdiplus::Rect clip_rect(pRoutePane->X, pRoutePane->Y, pRoutePane->W, pRoutePane->H);
                    graphics.SetClip(clip_rect);

                    Gdiplus::Matrix rotate_at_map;
                    Gdiplus::PointF center(pRoutePane->X + pRoutePane->W/2, pRoutePane->Y + pRoutePane->H/2);
                    Gdiplus::PointF image_pos(image_x, image_y);
                    rotate_at_map.RotateAt(-head_direction, image_pos);
                    rotate_at_map.Translate(center.X-image_x, center.Y-image_y, Gdiplus::MatrixOrderAppend);
                    graphics.SetTransform(&rotate_at_map);

                    Gdiplus::ColorMatrix ClrMatrix = { 
                            1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, pRoutePane->Opaque/(100.0f), 0.0f,
                            0.0f, 0.0f, 0.0f, 0.0f, 1.0f
                    };
                    Gdiplus::ImageAttributes ImgAttr;
                    ImgAttr.SetColorMatrix(&ClrMatrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

                    Gdiplus::Bitmap *pImage = m_Images[pRoutePane->ImageName];
                    if (pImage != nullptr) {
                        Gdiplus::Rect    destination(0, 0, pImage->GetWidth(), pImage->GetHeight());
                        status = graphics.DrawImage(pImage, destination, 0, 0, pImage->GetWidth(), pImage->GetHeight(), UnitPixel, &ImgAttr);
                        if (status != Status::Ok) {
                            FILE* pFile = fopen("log.txt", "a");
                            fprintf(pFile, "Status draw \"%s\" image => %i\n", pRoutePane->Name.c_str(), status);
                            fclose(pFile);
                        }
                    }

                    //draw tail
                    Gdiplus::Pen *pPenTail = new Gdiplus::Pen(pRoutePane->TailColor, pRoutePane->TailWidth);
                    int tail = pRoutePane->Tail;
                    int last_x = image_x;
                    int last_y = image_y;
                    pugi::xml_node sample = PathState.currentSample.previous_sibling();
                    while(tail-- > 0) {
                        if (sample.empty()) {
                            break;
                        }
                        int new_img_x = sample.attribute("imageX").as_int();
                        int new_img_y = sample.attribute("imageY").as_int();
                        graphics.DrawLine(pPenTail, last_x, last_y, new_img_x, new_img_y);
                        last_x = new_img_x;
                        last_y = new_img_y;
                        sample = sample.previous_sibling();
                    }
                    delete pPenTail;

                    graphics.Restore(state);

                    // Draw pointer, by the center and to the north only
                    Gdiplus::Bitmap *pPointer = m_Images[pRoutePane->Pointer];
                    if (pPointer != nullptr) {
                        Gdiplus::ImageAttributes ImgAttr;
                        ColorMatrix ClrMatrix = { 
                                1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, pRoutePane->Opaque/(100.0f), 0.0f,
                                0.0f, 0.0f, 0.0f, 0.0f, 1.0f
                        };
                        ImgAttr.SetColorMatrix(&ClrMatrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);
                        Gdiplus::Rect    destination(pRoutePane->X + pRoutePane->W/2 - pPointer->GetWidth()/2, 
                                                    pRoutePane->Y + pRoutePane->H/2 - pPointer->GetHeight()/2,
                                                    pPointer->GetWidth(), pPointer->GetHeight());
                        status = graphics.DrawImage(pPointer, destination, 0, 0, pPointer->GetWidth(), pPointer->GetHeight(), UnitPixel, &ImgAttr);
                        if (status != Status::Ok) {
                            FILE* pFile = fopen("log.txt", "a");
                            fprintf(pFile, "Status draw \"%s\" pointer => %i\n", pRoutePane->Name.c_str(), status);
                            fclose(pFile);
                        }
                    }
                } else if (pPane->Type == PaneType::Leg) {
                    //draw leg
                    RoutePane* pRoutePane = (RoutePane*)pPane;
                    PathState &PathState = m_PathStates[pRoutePane->PathName];
                    RoutePaneState &RoutePaneState = m_RoutePaneStates[pRoutePane->PathName];
                    if (&PathState == nullptr || &RoutePaneState == nullptr) {
                        continue;   // to the next pane
                    }
                    if (RoutePaneState.currentLap != PathState.currentLap) {
                        RoutePaneState.currentLap = PathState.currentLap;
                        double scale = (pRoutePane->H - pRoutePane->Margins * 2)/PathState.vectorSize;
                        if (fabs(PathState.left) > 0.01) {
                            scale = min(scale, (pRoutePane->W/2.0 - pRoutePane->Margins)/fabs(PathState.left));
                        }
                        if (fabs(PathState.right) > 0.01) {
                            scale = min(scale, (pRoutePane->W/2.0 - pRoutePane->Margins)/fabs(PathState.right));
                        }
                        if (fabs(PathState.top) > 0.01) {
                            scale = min(scale, (pRoutePane->H/2.0 - pRoutePane->Margins)/fabs(PathState.top));
                        }
                        if (fabs(PathState.bottom) > 0.01) {
                            scale = min(scale, (pRoutePane->H/2.0 - pRoutePane->Margins)/fabs(PathState.bottom));
                        }
                        if (scale > 1) {
                            scale = 1;
                        }
                        RoutePaneState.legPoints = PathState.legPoints;
                        Gdiplus::PointF view_center(pRoutePane->X + pRoutePane->W/2, pRoutePane->Y + pRoutePane->H/2);
                        RoutePaneState.pLegMatrix = PathState.pLegMatrix->Clone();
                        RoutePaneState.pLegMatrix->Scale(scale, scale, Gdiplus::MatrixOrderAppend);
                        RoutePaneState.pLegMatrix->Translate(view_center.X - PathState.legCenter.X * scale, view_center.Y - PathState.legCenter.Y * scale, Gdiplus::MatrixOrderAppend);
                        RoutePaneState.pLegMatrix->TransformPoints(RoutePaneState.legPoints.data(), RoutePaneState.legPoints.size());
                    }
                    //draw leg
                    Gdiplus::Bitmap *pImage = m_Images[pRoutePane->ImageName];
                    if (pImage != nullptr) {
                        GraphicsState state = graphics.Save();
                        
                        //TODO: set clip by region
                        Rect clip_rect(pRoutePane->X, pRoutePane->Y, pRoutePane->W, pRoutePane->H);
                        graphics.SetClip(clip_rect);

                        REAL t1[6];
                        RoutePaneState.pLegMatrix->GetElements(t1);
                        graphics.SetTransform(RoutePaneState.pLegMatrix);

                        ColorMatrix ClrMatrix = { 
                                1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, pRoutePane->Opaque/(100.0f), 0.0f,
                                0.0f, 0.0f, 0.0f, 0.0f, 1.0f
                        };
                        ImageAttributes ImgAttr;
                        ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

                        Rect    destination(0, 0, pImage->GetWidth(), pImage->GetHeight());
                        status = graphics.DrawImage(pImage, destination, 0, 0, pImage->GetWidth(), pImage->GetHeight(), UnitPixel, &ImgAttr);
                        if (status != Gdiplus::Status::Ok) {
                            FILE* pFile = fopen("log.txt", "a");
                            fprintf(pFile, "Status draw leg \"%s\" image => %i\n", pRoutePane->Name.c_str(), status);
                            fclose(pFile);
                        }

                        graphics.Restore(state);
                    }
                    // LegPoints are already transformed
                    if (RoutePaneState.legPoints.size() > 1) {
                        if (PathState.legPosition > 1) {
                            Gdiplus::Pen *pPenTail = new Gdiplus::Pen(pRoutePane->TailColor, pRoutePane->TailWidth);
                            status = graphics.DrawLines(pPenTail, RoutePaneState.legPoints.data(), PathState.legPosition);
                            if (status != Gdiplus::Status::Ok) {
                                FILE* pFile = fopen("log.txt", "a");
                                fprintf(pFile, "Status draw \"%s\" points => %i, size %i, pos %i\n", 
                                    pRoutePane->Name.c_str(), status, RoutePaneState.legPoints.size(), PathState.legPosition);
                                fclose(pFile);
                            }
                            delete pPenTail;
                        }
                    }
                } else if (pPane->Type == PaneType::Text) {
                    //draw leg
                    TextPane* pTextPane = (TextPane*)pPane;
                    std::wstring out_string;
                    if (pTextPane->TextType == TextType::Comment) {
                        out_string = pugi::as_wide(pTextPane->Value);
                    } else {
                        PathState &PathState = m_PathStates[pTextPane->Value];
                        if (&PathState != nullptr) {
                            WCHAR wstr[256];
                            if (pTextPane->TextType == TextType::Time) {
                                int elapsedTime = PathState.currentSample.attribute("elapsedTime").as_int();
                                uint32 hour = elapsedTime / 3600;
                                uint32 min = (elapsedTime % 3600) / 60;
                                uint32 sec = elapsedTime % 60;
                                wsprintfW(wstr, L"%02i:%02i'%02i", hour, min, sec);
                                out_string = wstr;
                            }
                        }
                    }
                    if (!out_string.empty()) {
                        Gdiplus::Brush *pFillBrush = new Gdiplus::SolidBrush(pTextPane->FillColor);
                        graphics.FillRectangle(pFillBrush, pTextPane->X, pTextPane->Y, pTextPane->W, pTextPane->H);
                        delete pFillBrush;
                        Gdiplus::Brush *pTextBrush = new Gdiplus::SolidBrush(pTextPane->FontColor);
                        Gdiplus::Font  *pTextFont = new Gdiplus::Font(pugi::as_wide(pTextPane->FontName).c_str(), pTextPane->FontSize);
                        graphics.DrawString(out_string.c_str(), out_string.length(),
                            pTextFont,
                            Gdiplus::PointF(pTextPane->X, pTextPane->Y),
                            pTextBrush);
                        delete pTextFont;
                        delete pTextBrush;
                    }
                }
            }
        }
    }

    // Draw last bitmap for each frame
    if (m_pLastBmp) {
        Gdiplus::Graphics gr_to(pbmp);
        gr_to.DrawImage(m_pLastBmp, 0, 0);
    }

    // //test if sample change
    // if (m_LastSample.empty() || m_LastSample != current_sample)
    // {
    //     //create bitmap
    //     if (m_pLastBmp && (m_pLastBmp->GetWidth() != pbmp->GetWidth() || m_pLastBmp->GetHeight() != pbmp->GetHeight())) {
    //         delete m_pLastBmp;
    //         m_pLastBmp = nullptr;
    //     }
    //     if (!m_pLastBmp)
    //         m_pLastBmp = new Bitmap(pbmp->GetWidth(), pbmp->GetHeight(), PixelFormat32bppARGB);

    //     Gdiplus::Status status;

    //     //draw into bitmap
    //     Gdiplus::Graphics graphics(m_pLastBmp);
    //     Color clear_color(0,0,0,0);
    //     graphics.Clear(clear_color);

    //     graphics.FillRectangle(m_pTextBrush, m_Config.m_TextX, m_Config.m_TextY, m_Config.m_TextWidth, m_Config.m_TextHeight);

    //     //draw time text NOTE: samples must be each second!!! in other case time will be displayed with gaps
    //     WCHAR wstr[256];
    //     uint32 hour = (time_run/1000)/3600;
    //     uint32 min = ((time_run/1000)%3600)/60;
    //     uint32 sec = (time_run/1000)%60;
    //     wsprintfW(wstr, L"%02i:%02i'%02i", hour, min, sec);
    //     graphics.DrawString(wstr, wcslen(wstr), m_pTimeFont, PointF(m_Config.m_TextX + m_Config.m_TimeX, m_Config.m_TextY + m_Config.m_TimeY), m_pTimeBrush); 

    //     wsprintfW(wstr, L"%i", current_sample.attribute("heartRate").as_int());
    //     graphics.DrawString(wstr, wcslen(wstr), m_pPulseFont, PointF(m_Config.m_TextX + m_Config.m_PulseX, m_Config.m_TextY + m_Config.m_PulseY), m_pPulseBrush); 

    //     //draw pace
    //     int pace_m = current_sample.attribute("pace").as_int();
    //     if (m_Config.m_PaceAvg > 1)  //NOTE: tail by samples, not by seconds
    //     {
    //         pugi::xml_node sample = current_sample.previous_sibling();
    //         for(int i = 1; i < m_Config.m_PaceAvg; i++)
    //         {
    //             if (!sample.empty())
    //             {
    //                 pace_m += sample.attribute("pace").as_int();
    //                 sample = sample.previous_sibling();
    //             }
    //             else
    //             {
    //                 pace_m += 30*60;
    //             }
    //         }
    //         pace_m /= m_Config.m_PaceAvg;
    //     }
    //     if (pace_m > 30*60)
    //     {
    //         pace_m = 0;
    //     }
    //     int pace_s = pace_m%60;
    //     pace_m /=60;
    //     wsprintfW(wstr, L"%i'%02i", pace_m, pace_s); 
    //     graphics.DrawString(wstr, wcslen(wstr), m_pPaceFont, PointF(m_Config.m_TextX + m_Config.m_PaceX, m_Config.m_TextY + m_Config.m_PaceY), m_pPaceBrush);

	// 	//draw comments
	// 	if (m_pCommBrush != NULL && m_pCommFont != NULL)
	// 	{
	// 		for (auto& comment : m_Config.m_Comments)
	// 		{
	// 			if (ms / 1000 > comment.m_Time &&
	// 				ms / 1000 < comment.m_Time + comment.m_Dur)
	// 			{
	// 				RectF rc(m_Config.m_CommX, m_Config.m_CommY, m_Config.m_CommW, m_Config.m_CommH);
	// 				std::wstring draw_me = pugi::as_wide(comment.m_Text.c_str());
	// 				graphics.DrawString
	// 					(draw_me.c_str(),
	// 					draw_me.length(),
	// 					m_pCommFont, 
	// 					rc, 
	// 					StringFormat::GenericDefault(),
	// 					m_pCommBrush);
	// 			}
	// 		}

	// 	}

    //     //draw logo
    //     if (m_pLogo)
    //     {
    //         ImageAttributes ImgAttr;
    //         ColorMatrix ClrMatrix = { 
    //                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    //                 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
    //                 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
    //                 0.0f, 0.0f, 0.0f, m_Config.m_LogoOpaque/(100.0f), 0.0f,
    //                 0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    //         };
    //         ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
    //         Rect    destination(m_Config.m_LogoX, m_Config.m_LogoY, m_pLogo->GetWidth(), m_pLogo->GetHeight());
    //         status = graphics.DrawImage(m_pLogo, destination, 0, 0, m_pLogo->GetWidth(), m_pLogo->GetHeight(), UnitPixel, &ImgAttr);
    //         if (status != Gdiplus::Status::Ok) {
    //             // TODO: show message
    //             FILE* pFile = fopen("log.txt", "a");
    //             fprintf(pFile, "Status 1 %i\n", status);
    //             fclose(pFile);
    //             return;
    //         }
    //     }

    //     int32 last_lap = m_LastSample.attribute("lapNumber").as_int();
    //     int32 cur_lap = current_sample.attribute("lapNumber").as_int();

    //     bool leg_position_found = false;

    //     if (last_lap != cur_lap)
    //     {
    //         //rebuild leg data

    //         //1. collect all image positions
    //         m_LegPoints.clear();
    //         m_LegTimes.clear();

    //         pugi::xml_node sample = current_sample;
    //         //find start of leg, remember leg position
    //         m_LegPosition = 0;
    //         leg_position_found = true;
    //         while(!sample.previous_sibling().empty() && sample.previous_sibling().attribute("lapNumber").as_int() == cur_lap)
    //         {
    //             sample = sample.previous_sibling();
    //             m_LegPosition++;
    //         }
    //         //add all leg points to array
    //         while(!sample.empty() && sample.attribute("lapNumber").as_int() == cur_lap)
    //         {
    //             m_LegPoints.push_back(PointF(sample.attribute("imageX").as_int(), sample.attribute("imageY").as_int()));
    //             m_LegTimes.push_back(sample.attribute("elapsedTime").as_double());
    //             sample = sample.next_sibling();
    //         }

    //         //2. build matrix
    //         int samples = m_LegPoints.size();
    //         if (samples > 1)
    //         {
    //             PointF start(m_LegPoints[0].X, m_LegPoints[0].Y);
    //             PointF end(m_LegPoints[samples-1].X, m_LegPoints[samples-1].Y);
    //             PointF vector(end.X - start.X, end.Y - start.Y);
            
    //             if (fabs(vector.X) > 1 || fabs(vector.Y) > 1)
    //             {
	// 				PointF leg_center((end.X + start.X) / 2, (end.Y + start.Y) / 2);

	// 				double vector_size = sqrt(vector.X*vector.X + vector.Y*vector.Y);

	// 				double angle = atan2((double)(vector.Y), (double)(vector.X));
    //                 double leg_direction = 90.0+180.0*(angle)/M_PI;

    //                 delete m_pLegMatrix;
    //                 m_pLegMatrix = new Matrix();
    //                 m_pLegMatrix->RotateAt(-leg_direction, leg_center);

    //                 //rotate test points
    //                 std::vector<PointF> testArr = m_LegPoints;
    //                 m_pLegMatrix->TransformPoints(testArr.data(), samples);

    //                 //find bounds
    //                 REAL left = testArr[0].X - leg_center.X;
    //                 REAL top = testArr[0].Y - leg_center.Y;
    //                 REAL right = left;
    //                 REAL bottom = top;
    //                 for(int i = 0; i < samples; i++)
    //                 {
    //                     left = min(left, testArr[i].X - leg_center.X);
    //                     top = min(top, testArr[i].Y - leg_center.Y);
    //                     right = max(right, testArr[i].X - leg_center.X);
    //                     bottom = max(bottom, testArr[i].Y - leg_center.Y);
    //                 }
                
    //                 double scale = (m_Config.m_LegHeight - m_Config.m_LegMargins*2)/vector_size;
    //                 if (fabs(left) > 0.01)
    //                 {
    //                     scale = min(scale, (m_Config.m_LegWidth/2.0 - m_Config.m_LegMargins)/fabs(left));
    //                 }
    //                 if (fabs(right) > 0.01)
    //                 {
    //                     scale = min(scale, (m_Config.m_LegWidth/2.0 - m_Config.m_LegMargins)/fabs(right));
    //                 }
    //                 if (fabs(top) > 0.01)
    //                 {
    //                     scale = min(scale, (m_Config.m_LegHeight/2.0 - m_Config.m_LegMargins)/fabs(top));
    //                 }
    //                 if (fabs(bottom) > 0.01)
    //                 {
    //                     scale = min(scale, (m_Config.m_LegHeight/2.0 - m_Config.m_LegMargins)/fabs(bottom));
    //                 }
                    
    //                 if (scale > 1)
    //                 {
    //                     scale = 1;
    //                 }

    //                 PointF view_center(m_Config.m_LegX + m_Config.m_LegWidth/2, m_Config.m_LegY + m_Config.m_LegHeight/2);

    //                 m_pLegMatrix->Scale(scale, scale, MatrixOrderAppend);

    //                 m_pLegMatrix->Translate(view_center.X-leg_center.X*scale, view_center.Y-leg_center.Y*scale, MatrixOrderAppend);

    //                 m_pLegMatrix->TransformPoints(m_LegPoints.data(), samples);

    //             }
    //         }
    //     }

    //     //draw leg
    //     if (m_pMap && m_LegPoints.size() > 1 && m_Config.m_LegOpaque > 0)
    //     {
    //         GraphicsState state = graphics.Save();
            
    //         //TODO: set clip by region
    //         Rect clip_rect(m_Config.m_LegX, m_Config.m_LegY, m_Config.m_LegWidth, m_Config.m_LegHeight);
    //         graphics.SetClip(clip_rect);

    //         REAL t1[6];
    //         m_pLegMatrix->GetElements(t1);
    //         graphics.SetTransform(m_pLegMatrix);

    //         ColorMatrix ClrMatrix = { 
    //                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    //                 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
    //                 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
    //                 0.0f, 0.0f, 0.0f, m_Config.m_LegOpaque/(100.0f), 0.0f,
    //                 0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    //         };
    //         ImageAttributes ImgAttr;
    //         ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

    //         Rect    destination(0, 0, m_pMap->GetWidth(), m_pMap->GetHeight());
    //         status = graphics.DrawImage(m_pMap, destination, 0, 0, m_pMap->GetWidth(), m_pMap->GetHeight(), UnitPixel, &ImgAttr);
    //         if (status != Gdiplus::Status::Ok) {
    //             // TODO: show message
    //             FILE* pFile = fopen("log.txt", "a");
    //             fprintf(pFile, "Status 2 %i\n", status);
    //             fclose(pFile);
    //             return;
    //         }

    //         graphics.Restore(state);

    //         if (!leg_position_found)
    //         {
    //             if (!m_LastSample.empty() && m_LastSample.next_sibling() == current_sample)
    //             {
    //                     m_LegPosition++;
    //             }
    //             else
    //             {
    //                 //find leg position
    //                 m_LegPosition = 0;
    //                 for(int i = 0; i < m_LegPoints.size() && m_LegTimes[i] < elapsed_time; i++, m_LegPosition++)
    //                 {
    //                     //TODO: binary search;
    //                 }
    //             }
    //         }
    //         if (m_LegPosition > 1) {
    //             status = graphics.DrawLines(m_pPenLeg, m_LegPoints.data(), m_LegPosition);
    //             if (status != Gdiplus::Status::Ok) {
    //                 // TODO: show message
    //                 FILE* pFile = fopen("log.txt", "a");
    //                 fprintf(pFile, "Status 3 %i, size %i, pos %i\n", status, m_LegPoints.size(), m_LegPosition);
    //                 fclose(pFile);
    //                 return;
    //             }
    //         }
    //     }

    //     //draw current position
    //     if (m_pMap && m_Config.m_PosWidth > 0 && m_Config.m_PosHeight > 0 && m_Config.m_PosOpaque > 0)
    //     {
    //         GraphicsState state = graphics.Save();

    //         int32 image_x = current_sample.attribute("imageX").as_int();
    //         int32 image_y = current_sample.attribute("imageY").as_int();
    //         double head_direction = current_sample.attribute("direction").as_double();

    //         //TODO: clip by region
    //         Rect clip_rect(m_Config.m_PosX, m_Config.m_PosY, m_Config.m_PosWidth, m_Config.m_PosHeight);
    //         graphics.SetClip(clip_rect);

    //         Matrix rotate_at_map;
    //         PointF center(m_Config.m_PosX + m_Config.m_PosWidth/2, m_Config.m_PosY + m_Config.m_PosHeight/2);
    //         PointF image_pos(image_x, image_y);
    //         rotate_at_map.RotateAt(-head_direction, image_pos);
    //         rotate_at_map.Translate(center.X-image_x, center.Y-image_y, MatrixOrderAppend);
    //         graphics.SetTransform(&rotate_at_map);

    //         ColorMatrix ClrMatrix = { 
    //                 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    //                 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
    //                 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
    //                 0.0f, 0.0f, 0.0f, m_Config.m_PosOpaque/(100.0f), 0.0f,
    //                 0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    //         };
    //         ImageAttributes ImgAttr;
    //         ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

    //         Rect    destination(0, 0, m_pMap->GetWidth(), m_pMap->GetHeight());
    //         status = graphics.DrawImage(m_pMap, destination, 0, 0, m_pMap->GetWidth(), m_pMap->GetHeight(), UnitPixel, &ImgAttr);
    //         if (status != Status::Ok) {
    //             // TODO: show message
    //             FILE* pFile = fopen("log.txt", "a");
    //             fprintf(pFile, "Status 4 %i\n", status);
    //             fclose(pFile);
    //             return;
    //         }

    //         //draw tail
    //         {
    //             int tail = m_Config.m_PosTail;
    //             int last_x = image_x;
    //             int last_y = image_y;
    //             pugi::xml_node sample = current_sample.previous_sibling();
    //             while(tail-- > 0)
    //             {
    //                 if (sample.empty())
    //                 {
    //                     break;
    //                 }
    //                 int new_img_x = sample.attribute("imageX").as_int();
    //                 int new_img_y = sample.attribute("imageY").as_int();
    //                 graphics.DrawLine(m_pPenTail, last_x, last_y, new_img_x, new_img_y);
    //                 last_x = new_img_x;
    //                 last_y = new_img_y;
    //                 sample = sample.previous_sibling();
    //             }
    //         }

    //         graphics.Restore(state);

    //         //draw pointer
    //         {
    //             ImageAttributes ImgAttr;
    //             ColorMatrix ClrMatrix = { 
    //                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    //                     0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
    //                     0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
    //                     0.0f, 0.0f, 0.0f, m_Config.m_PointerOpaque/(100.0f), 0.0f,
    //                     0.0f, 0.0f, 0.0f, 0.0f, 1.0f
    //             };
    //             ImgAttr.SetColorMatrix(&ClrMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
    //             Rect    destination(m_Config.m_PosX + m_Config.m_PosWidth/2 - m_pPointer->GetWidth()/2, m_Config.m_PosY + m_Config.m_PosHeight/2 - m_pPointer->GetHeight()/2, m_pPointer->GetWidth(), m_pPointer->GetHeight());
    //             status = graphics.DrawImage(m_pPointer, destination, 0, 0, m_pPointer->GetWidth(), m_pPointer->GetHeight(), UnitPixel, &ImgAttr);
    //             if (status != Status::Ok) {
    //                 // TODO: show message
    //                 FILE* pFile = fopen("log.txt", "a");
    //                 fprintf(pFile, "Status 5 %i\n", status);
    //                 fclose(pFile);
    //                 return;
    //             }
    //         }
    //     }
    //     m_LastSample = current_sample;
    // }

    // //draw last bitmap
    // if (m_pLastBmp)
    // {
    //     Gdiplus::Graphics gr_to(pbmp);
    //     gr_to.DrawImage(m_pLastBmp, 0, 0);
    // }

    return;
}

void RouteFilter::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) 
{
    RouteFilterConfig cfg_new;
    cfg_new.m_File = *argv[0].asString();
    pugi::xml_document doc;
    if (doc.load_file(cfg_new.m_File.c_str()) && cfg_new.FromXml(doc)) {
        m_Config = cfg_new;
    }
}

void RouteFilter::GetSettingString(char *buf, int maxlen) 
{
	SafePrintf(buf, maxlen, " (\"%s\")", m_Config.m_File.c_str());
}

void RouteFilter::GetScriptString(char *buf, int maxlen) 
{
    std::regex slash_regex("\\\\");
    std::string doubled_slashes = std::regex_replace(m_Config.m_File, slash_regex, "\\\\");
    SafePrintf(buf, maxlen, "Config(\"%s\")", doubled_slashes.c_str());
}

///////////////////////////////////////////////////////////////////////////////

extern VDXFilterDefinition filterDef_RouteAdd = VDXVideoFilterDefinition<RouteFilter>("Vorfol", "Route add", "Add route.");
