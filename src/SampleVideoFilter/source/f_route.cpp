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

template<typename... Args> void Log(const char * f, Args... args) {
    FILE* pFile;
    fopen_s(&pFile, "log.txt", "a");
    fprintf(pFile, f, args...);
    fclose(pFile);
}

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
    PaneType        Type;
    std::string     Name;
    Gdiplus::REAL   X;
    Gdiplus::REAL   Y;
    Gdiplus::REAL   W;
    Gdiplus::REAL   H;
    int             Opaque;     // 0-100
    time_t          Start;      // from video
    time_t          End;        // from video, must be > Start, else infinite
};


struct TextPane : public BasePane {
public:
    Gdiplus::Color  FillColor;
    Gdiplus::Color  FontColor;
    Gdiplus::REAL   FontSize;
    std::string     FontName;
    TextAlignment   Align;
    TextType        TextType;
    std::string     Value;  // Text for comment type, XML path for other types
    time_t          Avg;
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
    Gdiplus::REAL   TailWidth;
    Gdiplus::Color  TailColor;
    std::string     Pointer;        // name
    int             PointerOpaque;  // 0-100
    float           MaxScale;
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

    int                                 m_PaneWidth;
    int                                 m_PaneHeight;
    std::vector<BasePane*>              m_Panes;

    std::map<time_t, time_t>            m_Videos;       // [video time] = real time

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
    if (6 == sscanf_s(s.c_str(), "%d-%d-%dT%d:%d:%dZ", 
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
        switch (sscanf_s(s.c_str(), "%d:%d:%d", &T.tm_hour, &T.tm_min, &T.tm_sec)) {
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
        if (node.attribute("name").empty()) {
            pPane->Name = "";
        } else {
            pPane->Name = node.attribute("name").as_string();
        }
        pPane->X = 0;
        if (!node.attribute("x").empty()) {
            if (std::string("right") == node.attribute("x").as_string()) {
                pPane->X = FLT_MAX;
            } else {
                pPane->X = node.attribute("x").as_float();
            }
        }
        pPane->Y = 0;
        if (!node.attribute("y").empty()) {
            if (std::string("bottom") == node.attribute("y").as_string()) {
                pPane->Y = FLT_MAX;
            } else {
                pPane->Y = node.attribute("y").as_float();
            }
        }
        if (node.attribute("w").empty()) {
            pPane->W = 0;    
        } else {
            pPane->W = node.attribute("w").as_float();
        }
        if (node.attribute("h").empty()) {
            pPane->H = 0;
        } else {
            pPane->H = node.attribute("h").as_float();
        }
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
        pPane->FontSize = fromString<Gdiplus::REAL>(font_size_node.child_value());
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

    pPane->Avg = 0;
    pugi::xml_node avg_node = node.child("Avg");
    if (!avg_node.empty()) {
        pPane->Avg = fromString<time_t>(avg_node.child_value());
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
        pPane->TailWidth = fromString<Gdiplus::REAL>(width_node.child_value());
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
    
    pPane->MaxScale = 1.0;
    pugi::xml_node scale_node = node.child("MaxScale");
    if (!scale_node.empty()) {
        pPane->MaxScale = fromString<float>(scale_node.child_value());
    }

    return FillImagePane(pPane, node);
}

bool RouteFilterConfig::FromXml(pugi::xml_document &doc)
{
    pugi::xml_node settings = doc.child("RouteAddSettings");

	m_Videos.clear();
    pugi::xml_node videos_node = settings.child("Videos");
    if (!videos_node.empty()) {
        pugi::xml_node video_node = videos_node.child("Video");
        while (!video_node.empty())	{
            m_Videos[fromString<time_t>(video_node.attribute("start").as_string())] = fromString<time_t>(video_node.attribute("time").as_string());
            video_node = video_node.next_sibling();
        }
    }

	m_Images.clear();
    pugi::xml_node images_node = settings.child("Images");
    if (!images_node.empty()) {
        pugi::xml_node image_node = images_node.child("Image");
        while (!image_node.empty())	{
            m_Images[image_node.attribute("name").as_string()] = image_node.child_value();
            image_node = image_node.next_sibling();
        }
    }

	m_Paths.clear();
    pugi::xml_node paths_node = settings.child("Paths");
    if (!paths_node.empty()) {
        pugi::xml_node path_node = paths_node.child("Path");
        while (!path_node.empty())	{
            m_Paths[path_node.attribute("name").as_string()] = path_node.child_value();
            path_node = path_node.next_sibling();
        }
    }

    m_PaneWidth = 0;
    m_PaneHeight = 0;
    m_Panes.clear();
    pugi::xml_node panes_node = settings.child("Panes");
    if (!panes_node.empty()) {
        pugi::xml_attribute pane_width = panes_node.attribute("width");
        if (!pane_width.empty()) {
            m_PaneWidth = pane_width.as_int();
        }
        pugi::xml_attribute pane_height = panes_node.attribute("height");
        if (!pane_height.empty()) {
            m_PaneHeight = pane_height.as_int();
        }
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
                m_Panes.push_back(pPane);
            }
            pane_node = pane_node.next_sibling();
        }
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
    PathState() {
        pPath = nullptr;
        startTime = 0;
        pugi::xml_node e;
        lastSample = e;
        currentSample = e;
        currentLap = -1;
        legPosition = -1;
        pLegMatrix = nullptr;
        vectorSize = 1.0;
        left = top = right = bottom = 0;
        legCenter.X = legCenter.Y = 0;
    };
    pugi::xml_document             *pPath;
    time_t                          startTime;
    pugi::xml_node                  lastSample;
    pugi::xml_node                  currentSample;
    int                             currentLap;
    int                             legPosition;
    Gdiplus::Matrix                *pLegMatrix; // without scale
    std::vector<Gdiplus::PointF>    legPoints;
    std::vector<int>                legTimes;
    Gdiplus::REAL                   vectorSize;
    Gdiplus::REAL                   left;
    Gdiplus::REAL                   top;
    Gdiplus::REAL                   right;
    Gdiplus::REAL                   bottom;
    Gdiplus::PointF                 legCenter;
};

struct RoutePaneState {
public:
    RoutePaneState() {
        visible = false;
        currentLap = -1;
        pLegMatrix = nullptr;
    };
    bool                            visible;
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
	std::vector<RoutePaneState>                 m_RoutePaneStates; // by position in m_Config.m_Panes

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

    for (size_t i = 0; i < m_RoutePaneStates.size(); ++i) {
        delete m_RoutePaneStates[i].pLegMatrix;
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
                pugi::xml_node route = P.pPath->child("Route");
                if (route.empty()) {
                    Log("No Route in %s\n", it->second.c_str());
                } else {
                    pugi::xml_node segment = route.child("Segment");
                    if (segment.empty()) {
                        Log("No Segment in %s\n", it->second.c_str());
                    } else {
                        pugi::xml_node e;
                        P.lastSample = e;
                        P.currentSample = segment.first_child();
                        if (P.currentSample.empty()) {
                            Log("No samples in %s\n", it->second.c_str());
                        } else {
                            bool good_file = true;
                            const char *requiredAttributes[] = {
                                "time",
                                "elapsedTimeFromStart",
                                "imageX",
                                "imageY",
                                "lapNumber"
                            };
                            for(size_t i = 0; i < sizeof(requiredAttributes)/sizeof(requiredAttributes[0]); ++i) {
                                if (P.currentSample.attribute(requiredAttributes[i]).empty()) {
                                    Log("No \"%s\" attribute in %s\n", requiredAttributes[i], it->second.c_str());
                                    good_file = false;
                                }                                
                            }
                            if (good_file) {
                                P.startTime = fromString<time_t>(P.currentSample.attribute("time").as_string());
                                m_PathStates[it->first] = P;
                            }
                        }
                    }
                }
            }
        }
        //init pane states
        for (size_t i = 0; i < m_Config.m_Panes.size(); ++i) {
            RoutePaneState P;
            P.pLegMatrix = nullptr;
            P.visible = false;
            P.currentLap = 0;
            m_RoutePaneStates.push_back(P);
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
                Gdiplus::Bitmap *pbmp = PrepareRGB32(pxdst.data, (uint32)pxdst.pitch, pxdst.w, pxdst.h);
                DrawRoute(pbmp, fa->pfsi->lSourceFrameMS);
                ApplyRGB32(pbmp, pxdst.data, (uint32)pxdst.pitch, pxdst.w, pxdst.h);
                delete pbmp;
				break;
		}
	} else {
        Gdiplus::Bitmap *pbmp = PrepareRGB32(fa->dst.data, (uint32)fa->dst.pitch, fa->dst.w, fa->dst.h);
        DrawRoute(pbmp, fa->pfsi->lSourceFrameMS);
        ApplyRGB32(pbmp, fa->dst.data, (uint32)fa->dst.pitch, fa->dst.w, fa->dst.h);
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
        Log("No image to draw at %i\n", ms / 1000);
        return;
    }
    // Test if bitmap size changed
    if (m_pLastBmp && (m_pLastBmp->GetWidth() != pbmp->GetWidth() || m_pLastBmp->GetHeight() != pbmp->GetHeight())) {
        delete m_pLastBmp;
        m_pLastBmp = nullptr;
    }

    // Recalculate paths
    for (auto it = m_PathStates.begin(); it != m_PathStates.end(); ++it) {
        PathState &PathState = it->second;
        time_t realTime = ms;
        if (m_Config.m_Videos.size()) {
            auto videoIt = m_Config.m_Videos.lower_bound(ms/1000);
            if (videoIt != m_Config.m_Videos.begin()) {
                --videoIt;
            }
            realTime += videoIt->second * 1000 - videoIt->first * 1000;
        } else {
            realTime += PathState.startTime * 1000;
        }
        int64 time_run = realTime - PathState.startTime * 1000;
        if (time_run < 0) {
            time_run = 0;
        }
        PathState.lastSample = PathState.currentSample;
        if (PathState.currentSample.empty()) {
            PathState.currentSample = PathState.pPath->child("Route").child("Segment").first_child();
        }
        //go forward
        int elapsed_time = PathState.currentSample.attribute("elapsedTimeFromStart").as_int();
        while(time_run > elapsed_time * 1000) {
            PathState.currentSample = PathState.currentSample.next_sibling();
            if (PathState.currentSample.empty()) {
                //end of path reached
                PathState.currentSample = PathState.pPath->child("Route").child("Segment").last_child();
                break;
            }
            elapsed_time = PathState.currentSample.attribute("elapsedTimeFromStart").as_int();
        }
        //go to previous sample
        if (!PathState.currentSample.previous_sibling().empty()) {
            PathState.currentSample = PathState.currentSample.previous_sibling();
            elapsed_time = PathState.currentSample.attribute("elapsedTimeFromStart").as_int();
        }
        //go backward
        while(time_run < elapsed_time * 1000) {
            PathState.currentSample = PathState.currentSample.previous_sibling();
            if (PathState.currentSample.empty()) {
                //begin of path reaches
                PathState.currentSample = PathState.pPath->child("Route").child("Segment").first_child();;
                break;
            }
            elapsed_time = PathState.currentSample.attribute("elapsedTimeFromStart").as_int();
        }
    
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
                PathState.legPoints.push_back(Gdiplus::PointF(sample.attribute("imageX").as_float(), sample.attribute("imageY").as_float()));
                PathState.legTimes.push_back(sample.attribute("elapsedTimeFromStart").as_int());
                sample = sample.next_sibling();
            }

            //2. build matrix
            size_t samples = PathState.legPoints.size();
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

					Gdiplus::REAL angle = atan2(vector.Y, vector.X);
                    Gdiplus::REAL leg_direction = 90.0f+180.0f*(angle)/(Gdiplus::REAL)M_PI;

                    delete PathState.pLegMatrix;
                    PathState.pLegMatrix = new Gdiplus::Matrix();
                    PathState.pLegMatrix->RotateAt(-leg_direction, PathState.legCenter);

                    //rotate test points
                    std::vector<Gdiplus::PointF> testArr = PathState.legPoints;
                    PathState.pLegMatrix->TransformPoints(testArr.data(), (int)samples);

                    //find bounds
                    PathState.left = testArr[0].X - PathState.legCenter.X;
                    PathState.top = testArr[0].Y - PathState.legCenter.Y;
                    PathState.right = PathState.left;
                    PathState.bottom = PathState.top;
                    for(size_t i = 0; i < samples; i++) {
                        PathState.left = min(PathState.left, testArr[i].X - PathState.legCenter.X);
                        PathState.top = min(PathState.top, testArr[i].Y - PathState.legCenter.Y);
                        PathState.right = max(PathState.right, testArr[i].X - PathState.legCenter.X);
                        PathState.bottom = max(PathState.bottom, testArr[i].Y - PathState.legCenter.Y);
                    }
                }
            }
        } else {
            // Update position in leg points
            if (!PathState.lastSample.empty() && PathState.lastSample.next_sibling() == PathState.currentSample) {
                ++PathState.legPosition;
            } else {
                //find leg position
                PathState.legPosition = 0;
                for(size_t i = 0; i < PathState.legPoints.size() && PathState.legTimes[i] < elapsed_time; ++i, PathState.legPosition++){
                    //TODO: binary search;
                }
            }
        }
    }

    bool do_refresh = false;
    // Walk over all panes to set/reset visibility
    for (size_t i = 0; i < m_Config.m_Panes.size(); ++i) {
        BasePane *pPane = m_Config.m_Panes[i];
        RoutePaneState &RoutePaneState = m_RoutePaneStates[i];
        if (pPane->Opaque > 0 &&
            1000 * pPane->Start <= ms && 
            (pPane->End <= pPane->Start || 1000 * pPane->End >= ms))
        {
            if (!RoutePaneState.visible) {
                // one of the panes become visible
                do_refresh = true;
            }
            RoutePaneState.visible = true;
            if (!do_refresh) {
                // Also, test if sample changes
                switch (pPane->Type) {
                    case PaneType::Pos:
                    case PaneType::Leg:
                        {
                            RoutePane* pRoutePane = (RoutePane*)pPane;
                            PathState &PathState = m_PathStates[pRoutePane->PathName];
                            do_refresh |= (PathState.lastSample != PathState.currentSample);
                        }
                        break;
                    case PaneType::Text:
                        {
                            TextPane* pTextPane = (TextPane*)pPane;
                            if (pTextPane->TextType != TextType::Comment) {
                                PathState &PathState = m_PathStates[pTextPane->Value];
                                do_refresh |= (PathState.lastSample != PathState.currentSample);
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        } else {
            if (RoutePaneState.visible) {
                // one of the panes become invisible
                do_refresh = true;
            }
            RoutePaneState.visible = false;
        }
    }

    Gdiplus::Status status;

    if (do_refresh) {

        // Create new if needed
        if (!m_pLastBmp) {
            m_pLastBmp = new Gdiplus::Bitmap(pbmp->GetWidth(), pbmp->GetHeight(), PixelFormat32bppARGB);
        }

        // Craw into bitmap
        Gdiplus::Graphics graphics(m_pLastBmp);

        // Clear all
        Color clear_color(0,0,0,0);
        graphics.Clear(clear_color);

        for (size_t i = 0; i < m_Config.m_Panes.size(); ++i) {
            BasePane *pPane = m_Config.m_Panes[i];
            RoutePaneState &RoutePaneState = m_RoutePaneStates[i];
            if (RoutePaneState.visible) {
                Gdiplus::REAL X = pPane->X;
                Gdiplus::REAL Y = pPane->Y;
                Gdiplus::REAL W = pPane->W;
                Gdiplus::REAL H = pPane->H;
                // Scale if pane desired size does not equal video size
                if (m_Config.m_PaneWidth && m_Config.m_PaneWidth != m_pLastBmp->GetWidth()) {
                    W = W * m_pLastBmp->GetWidth() / m_Config.m_PaneWidth;
                    if (X != FLT_MAX) {
                        X = X * m_pLastBmp->GetWidth() / m_Config.m_PaneWidth;
                    }
                }
                if (m_Config.m_PaneHeight && m_Config.m_PaneHeight != m_pLastBmp->GetHeight()) {
                    H = H * m_pLastBmp->GetHeight() / m_Config.m_PaneHeight;
                    if (Y != FLT_MAX) {
                        Y = Y * m_pLastBmp->GetHeight() / m_Config.m_PaneHeight;
                    }
                }
                if (X == FLT_MAX) {
                    X = m_pLastBmp->GetWidth() - W;
                }
                if (Y == FLT_MAX) {
                    Y = m_pLastBmp->GetHeight() - H;
                }
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
                        if (W == 0) {
                            W = (Gdiplus::REAL)pImage->GetWidth();
                            if (m_Config.m_PaneWidth && m_Config.m_PaneWidth != m_pLastBmp->GetWidth()) {
                                W = W * m_pLastBmp->GetWidth() / m_Config.m_PaneWidth;
                            }
                            if (pPane->X == FLT_MAX) {
                                X = m_pLastBmp->GetWidth() - W;
                            }
                        }
                        if (H == 0) {
                            H = (Gdiplus::REAL)pImage->GetHeight();
                            if (m_Config.m_PaneHeight && m_Config.m_PaneHeight != m_pLastBmp->GetHeight()) {
                                H = H * m_pLastBmp->GetHeight() / m_Config.m_PaneHeight;
                            }                        
                            if (pPane->Y == FLT_MAX) {
                                Y = m_pLastBmp->GetHeight() - H;
                            }
                        }
                        Gdiplus::RectF destination(X, Y, W, H);
                        status = graphics.DrawImage(pImage, destination, 0, 0, (Gdiplus::REAL)pImage->GetWidth(), (Gdiplus::REAL)pImage->GetHeight(), Gdiplus::UnitPixel, &ImgAttr);
                        if (status != Status::Ok) {
                            Log("Status draw Image \"%s\" image => %i at %d\n", pPane->Name.c_str(), status, ms / 1000);
                        }
                    }
                } else if (pPane->Type == PaneType::Pos) {
                    // Draw current position
                    RoutePane* pRoutePane = (RoutePane*)pPane;
                    PathState &PathState = m_PathStates[pRoutePane->PathName];
                    if (&PathState == nullptr) {
                        continue;   // Go to the next pane
                    }

                    Gdiplus::REAL image_x = PathState.currentSample.attribute("imageX").as_float();
                    Gdiplus::REAL image_y = PathState.currentSample.attribute("imageY").as_float();

                    Gdiplus::REAL head_direction = 0;
                    if (!PathState.currentSample.attribute("direction").empty()) {
                        head_direction = PathState.currentSample.attribute("direction").as_float();
                    }

                    // Next transformations are both for the image and the tail
                    Gdiplus::GraphicsState state = graphics.Save();

                    //TODO: clip by region
                    Gdiplus::RectF clip_rect(X, Y, W, H);
                    graphics.SetClip(clip_rect);

                    Gdiplus::Matrix rotate_at_map;
                    Gdiplus::PointF center(X + W/2.0f, Y + H/2.0f);
                    Gdiplus::PointF image_pos(image_x, image_y);
                    rotate_at_map.RotateAt(-head_direction, image_pos);
                    rotate_at_map.Scale(pRoutePane->MaxScale, pRoutePane->MaxScale, Gdiplus::MatrixOrderAppend);
                    rotate_at_map.Translate(center.X - image_x * pRoutePane->MaxScale, center.Y - image_y * pRoutePane->MaxScale, Gdiplus::MatrixOrderAppend);
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
                            Log("Status draw Pos \"%s\" image => %i at %d\n", pRoutePane->Name.c_str(), status, ms / 1000);
                        }
                    }

                    //draw tail
                    if (pRoutePane->TailColor.GetAlpha() != 0) {
                        int curElapsedTime = PathState.currentSample.attribute("elapsedTimeFromStart").as_int();
                        std::vector<Gdiplus::PointF> points;
                        points.emplace_back(image_x, image_y);
                        pugi::xml_node sample = PathState.currentSample.previous_sibling();
                        while(!sample.empty() && curElapsedTime - sample.attribute("elapsedTimeFromStart").as_int() < pRoutePane->Tail) {
                            points.emplace_back(sample.attribute("imageX").as_float(), sample.attribute("imageY").as_float());
                            sample = sample.previous_sibling();
                        }
                        if (points.size() > 1) {
                            Gdiplus::Pen *pPenTail = new Gdiplus::Pen(pRoutePane->TailColor, pRoutePane->TailWidth);
                            graphics.DrawLines(pPenTail, points.data(), (int)points.size());
                            delete pPenTail;
                        }
                    }

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
                        Gdiplus::RectF   destination(X + W/2 - pPointer->GetWidth()/2, 
                                                     Y + H/2 - pPointer->GetHeight()/2,
                                                    (Gdiplus::REAL)pPointer->GetWidth(), (Gdiplus::REAL)pPointer->GetHeight());
                        status = graphics.DrawImage(pPointer, destination, 0, 0, (Gdiplus::REAL)pPointer->GetWidth(), (Gdiplus::REAL)pPointer->GetHeight(), UnitPixel, &ImgAttr);
                        if (status != Status::Ok) {
                            Log("Status draw Pos \"%s\" pointer => %i at %d\n", pRoutePane->Name.c_str(), status, ms / 1000);
                        }
                    }
                } else if (pPane->Type == PaneType::Leg) {
                    //draw leg
                    RoutePane* pRoutePane = (RoutePane*)pPane;
                    PathState &PathState = m_PathStates[pRoutePane->PathName];
                    if (&PathState == nullptr || &RoutePaneState == nullptr) {
                        continue;   // to the next pane
                    }
                    if (RoutePaneState.currentLap != PathState.currentLap) {
                        RoutePaneState.currentLap = PathState.currentLap;
                        Gdiplus::REAL scale = (H - pRoutePane->Margins * 2.0f)/PathState.vectorSize;
                        if (fabs(PathState.left) > 0.01) {
                            scale = min(scale, (W/2.0f - pRoutePane->Margins)/fabs(PathState.left));
                        }
                        if (fabs(PathState.right) > 0.01f) {
                            scale = min(scale, (W/2.0f - pRoutePane->Margins)/fabs(PathState.right));
                        }
                        if (fabs(PathState.top) > 0.01f) {
                            scale = min(scale, (H/2.0f - pRoutePane->Margins)/fabs(PathState.top));
                        }
                        if (fabs(PathState.bottom) > 0.01f) {
                            scale = min(scale, (H/2.0f - pRoutePane->Margins)/fabs(PathState.bottom));
                        }
                        if (scale > pRoutePane->MaxScale) {
                            scale = pRoutePane->MaxScale;
                        } else if (scale < 0.01f) {
                            scale = 0.01f;
                        }
                        RoutePaneState.legPoints = PathState.legPoints;
                        Gdiplus::PointF view_center(X + W/2.0f, Y + H/2.0f);
                        RoutePaneState.pLegMatrix = PathState.pLegMatrix->Clone();
                        RoutePaneState.pLegMatrix->Scale(scale, scale, Gdiplus::MatrixOrderAppend);
                        RoutePaneState.pLegMatrix->Translate(view_center.X - PathState.legCenter.X * scale, view_center.Y - PathState.legCenter.Y * scale, Gdiplus::MatrixOrderAppend);
                        RoutePaneState.pLegMatrix->TransformPoints(RoutePaneState.legPoints.data(), (int)RoutePaneState.legPoints.size());
                    }
                    //draw leg
                    Gdiplus::Bitmap *pImage = m_Images[pRoutePane->ImageName];
                    if (pImage != nullptr) {
                        GraphicsState state = graphics.Save();
                        
                        //TODO: set clip by region
                        Gdiplus::RectF clip_rect(X, Y, W, H);
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
                        ImgAttr.SetColorMatrix(&ClrMatrix, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);

                        Rect    destination(0, 0, pImage->GetWidth(), pImage->GetHeight());
                        status = graphics.DrawImage(pImage, destination, 0, 0, pImage->GetWidth(), pImage->GetHeight(), UnitPixel, &ImgAttr);
                        if (status != Gdiplus::Status::Ok) {
                            Log("Status draw Leg \"%s\" image => %i at %d\n", pRoutePane->Name.c_str(), status, ms / 1000);
                        }

                        graphics.Restore(state);
                    }
                    // LegPoints are already transformed
                    if (RoutePaneState.legPoints.size() > 1) {
                        if (PathState.legPosition > 1) {
                            if (pRoutePane->TailColor.GetAlpha() != 0) {
                                Gdiplus::Pen *pPenTail = new Gdiplus::Pen(pRoutePane->TailColor, pRoutePane->TailWidth);
                                status = graphics.DrawLines(pPenTail, RoutePaneState.legPoints.data(), PathState.legPosition);
                                if (status != Gdiplus::Status::Ok) {
                                    Log("Status draw Leg \"%s\" points => %i, size %i, pos %i, at %d\n", 
                                        pRoutePane->Name.c_str(), status, RoutePaneState.legPoints.size(), PathState.legPosition, ms / 1000);
                                }
                                delete pPenTail;
                            }
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
                            WCHAR wstr[256] = {0};
                            if (pTextPane->TextType == TextType::Time) {
                                int elapsedTimeFromStart = PathState.currentSample.attribute("elapsedTimeFromStart").as_int();
                                uint32 hour = elapsedTimeFromStart / 3600;
                                uint32 min = (elapsedTimeFromStart % 3600) / 60;
                                uint32 sec = elapsedTimeFromStart % 60;
                                wsprintfW(wstr, L"%02i:%02i'%02i", hour, min, sec);
                            } else if (pTextPane->TextType == TextType::HR) {
                                pugi::xml_attribute hr_attr = PathState.currentSample.attribute("heartRate");
                                if (!hr_attr.empty()) {
                                    wsprintfW(wstr, L"%i", hr_attr.as_int());
                                }
                            } else if (pTextPane->TextType == TextType::Pace) {
                                int pace = 0;
                                pugi::xml_attribute pace_attr = PathState.currentSample.attribute("pace");
                                if (!pace_attr.empty()) {
                                    pace = pace_attr.as_int();
                                }
                                if (pTextPane->Avg) {
                                    int avgCount = 1;
                                    int curElapsedTime = PathState.currentSample.attribute("elapsedTimeFromStart").as_int();
                                    pugi::xml_node sample = PathState.currentSample.previous_sibling();
                                    while(!sample.empty() && curElapsedTime - sample.attribute("elapsedTimeFromStart").as_int() < pTextPane->Avg) {
                                        if (!sample.attribute("pace").empty()) {
                                            pace += sample.attribute("pace").as_int();
                                            ++avgCount;
                                        }
                                        sample = sample.previous_sibling();
                                    }
                                    pace /= avgCount;
                                } 
                                wsprintfW(wstr, L"%i'%02i", pace / 60, pace % 60);
                            }
                            out_string = wstr;
                        }
                    }
                    if (!out_string.empty()) {
                        Gdiplus::Brush *pFillBrush = new Gdiplus::SolidBrush(pTextPane->FillColor);
                        graphics.FillRectangle(pFillBrush, X, Y, W, H);
                        delete pFillBrush;
                        Gdiplus::Brush *pTextBrush = new Gdiplus::SolidBrush(pTextPane->FontColor);
                        Gdiplus::REAL fontSize = pTextPane->FontSize;
                        if (m_Config.m_PaneHeight && m_Config.m_PaneHeight != m_pLastBmp->GetHeight()) {
                            fontSize = fontSize * m_pLastBmp->GetHeight() / m_Config.m_PaneHeight;
                        }
                        Gdiplus::Font  *pTextFont = new Gdiplus::Font(pugi::as_wide(pTextPane->FontName).c_str(), fontSize);
                        Gdiplus::RectF rc(X, Y, W, H);
                        Gdiplus::StringFormat *pFormat = Gdiplus::StringFormat::GenericDefault()->Clone();
                        switch(pTextPane->Align) {
                            case TextAlignment::Center:
                                pFormat->SetAlignment(Gdiplus::StringAlignment::StringAlignmentCenter);
                                break;
                            case TextAlignment::Left:
                                pFormat->SetAlignment(Gdiplus::StringAlignment::StringAlignmentNear);
                                break;
                            case TextAlignment::Right:
                                pFormat->SetAlignment(Gdiplus::StringAlignment::StringAlignmentFar);
                                break;
                            default:
                                break;
                        }
                        graphics.DrawString(
                            out_string.c_str(), (int)out_string.length(),
                            pTextFont,
                            rc,
                            pFormat,
                            pTextBrush);
                        delete pFormat;
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
        status = gr_to.DrawImage(m_pLastBmp, 0, 0);
        if (status != Status::Ok) {
            Log("Status draw layer => %i at %d\n", status, ms / 1000);
        }
    }

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

extern VDXFilterDefinition filterDef_RouteAdd = VDXVideoFilterDefinition<RouteFilter>("Vorfol", "Add Route", "Add route.");
