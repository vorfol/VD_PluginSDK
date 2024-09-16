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

struct RoutePane {
public:
    PaneType    Type;
    std::string Name;
    int         X;
    int         Y;
    int         W;
    int         H;
    int         Opaque;     // 0-100
    time_t      Start;      // GMT
    time_t      End;        // GMT
};


struct TextPane : public RoutePane {
public:
    Gdiplus::Color  FillColor;
    Gdiplus::Color  FontColor;
    int             FontSize;
    std::string     FontName;
    TextAlignment   Align;
    TextType        TextType;
    std::string     Value;  // Text for comment type, XML path for other types
};

struct ImagePane : public RoutePane {
public:
    std::string     Image;      // name
};

struct LegPane : public ImagePane {
public:
    std::string     Path;       // name
    int             TailWidth;
    int             Margins;
    Gdiplus::Color  TailColor;
};

struct PosPane : public LegPane {
public:
    std::string     Pointer;        // name
    int             PointerOpaque;  // 0-100
    time_t          Tail;
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

    std::map<std::string, RoutePane*>   m_Panes;        // [name] = pane

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
        switch (sscanf(s.c_str(), "%d:%dZ", &T.tm_hour, &T.tm_min, &T.tm_sec)) {
            default:
                ret = 0;
                break;
            case 3:
                ret -= T.tm_hour * 3600;
                // fall
            case 2:
                ret -= T.tm_min * 60;
                // fall
            case 1: 
                ret -= T.tm_sec;
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

RoutePane* FillPane(RoutePane *pPane, pugi::xml_node &node) {
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
        }
    }
    return pPane;
}

RoutePane* FillText(TextPane *pPane, pugi::xml_node &node) {
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

    return FillPane(pPane, node);
}

RoutePane* FillImage(ImagePane *pPane, pugi::xml_node &node) {
    if (pPane == nullptr) {
        pPane = new ImagePane();
        pPane->Type = PaneType::Image;
    }

    pPane->Image = "";
    pugi::xml_node image_node = node.child("Image");
    if (!image_node.empty()) {
        pPane->Image = image_node.child_value();
    }

    return FillPane(pPane, node);
}

RoutePane* FillLeg(LegPane *pPane, pugi::xml_node &node) {
    if (pPane == nullptr) {
        pPane = new LegPane();
        pPane->Type = PaneType::Leg;
    }

    pPane->Path = "";
    pugi::xml_node path_node = node.child("Path");
    if (!path_node.empty()) {
        pPane->Path = path_node.child_value();
    }

    pPane->TailWidth = 8;
    pugi::xml_node width_node = node.child("TailWidth");
    if (!width_node.empty()) {
        pPane->TailWidth = fromString<int>(width_node.child_value());
    }

    pPane->Margins = 30;
    pugi::xml_node margins_node = node.child("Margins");
    if (!margins_node.empty()) {
        pPane->Margins = fromString<int>(margins_node.child_value());
    }

    pPane->TailColor = Gdiplus::Color(0x60,0xff,0,0);
    pugi::xml_node color_node = node.child("TailColor");
    if (!color_node.empty()) {
        pPane->TailColor = fromString<Gdiplus::Color>(color_node.child_value());
    }

    return FillImage(pPane, node);
}

RoutePane* FillPos(PosPane *pPane, pugi::xml_node &node) {
    if (pPane == nullptr) {
        pPane = new PosPane();
        pPane->Type = PaneType::Pos;
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

    pPane->Tail = 60;
    pugi::xml_node tail_node = node.child("Tail");
    if (!tail_node.empty()) {
        pPane->Tail = fromString<time_t>(tail_node.child_value());
    }

    return FillLeg(pPane, node);
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
        RoutePane *pPane = nullptr;
        std::string type_name = std::string(pane_node.attribute("type").as_string());
        if (type_name == "leg") {
            pPane = FillLeg(nullptr, pane_node);
        } else if (type_name == "image") {
            pPane = FillImage(nullptr, pane_node);
        } else if (type_name == "pos") {
            pPane = FillPos(nullptr, pane_node);
        } else if (type_name == "text") {
            pPane = FillText(nullptr, pane_node);
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
    pugi::xml_node                  lastSample;
    int                             legPosition;
    Gdiplus::Matrix                *pLegMatrix;
    std::vector<Gdiplus::PointF>    legPoints;
    std::vector<double>             legTimes;
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

    virtual void ZeroVars();    //pointers only
    virtual void DeleteVars();
    virtual void CreateVars();  //from m_Config!

    Gdiplus::Bitmap *PrepareRGB32(void* data, uint32 pitch, uint32 w, uint32 h);
	void DrawRoute(Gdiplus::Bitmap *bmp, uint32 ms);
    void ApplyRGB32(Gdiplus::Bitmap *bmp, void* data, uint32 pitch, uint32 w, uint32 h);

	void ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc);

	RouteFilterConfig	m_Config;

    std::map<std::string, Gdiplus::Bitmap*>     m_Images;
	std::map<std::string, PathState>            m_PathDocs;

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

    for (auto it = m_PathDocs.begin(); it != m_PathDocs.end(); ++it) {
        delete it->second.pLegMatrix;
        delete it->second.pPath;
    }
    m_PathDocs.clear();

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
            P.pPath->load_file(it->second.c_str());
            m_PathDocs[it->first] = P;
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

    for (auto it = m_Config.m_Panes.begin(); it != m_Config.m_Panes.end(); ++it) {
        // m_Images[it->first] = Gdiplus::Bitmap::FromFile(pugi::as_wide(it->second).c_str());
    }
    // //calculate current run time
    // int32 time_run = ms - m_Config.m_PathOffset*1000;
    // if (time_run < 0)
    // {
    //     time_run = 0;
    // }

    // //find current sample by run time
    // pugi::xml_node current_sample = m_LastSample;
    // if (current_sample.empty())
    // {
    //     //get first sample
    //     current_sample = m_pPathDoc->child("Route").child("Segment").first_child();
    // }
    // //go forward
    // double elapsed_time = current_sample.attribute("elapsedTime").as_double();
    // while(time_run > elapsed_time*1000)
    // {
    //     current_sample = current_sample.next_sibling();
    //     if (current_sample.empty())
    //     {
    //         //end of path reached
    //         current_sample = m_pPathDoc->child("Route").child("Segment").last_child();
    //         break;
    //     }
    //     elapsed_time = current_sample.attribute("elapsedTime").as_double();
    // }
    // //go to previous sample
    // if (!current_sample.previous_sibling().empty())
    // {
    //     current_sample = current_sample.previous_sibling();
    //     elapsed_time = current_sample.attribute("elapsedTime").as_double();
    // }
    // //go backward
    // while(time_run < elapsed_time*1000)
    // {
    //     current_sample = current_sample.previous_sibling();
    //     if (current_sample.empty())
    //     {
    //         //begin of path reaches
    //         current_sample = m_pPathDoc->child("Route").child("Segment").first_child();;
    //         break;
    //     }
    //     elapsed_time = current_sample.attribute("elapsedTime").as_double();
    // }

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
